/*
 * Copyright (c) 2018 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>

#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <chrono>
#include <utility>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/platform_impl/EventLoopCommon.h>
#include <aipstack/platform_specific/EventProviderLinux.h>

namespace AIpStack {

namespace {

static std::uint32_t events_to_epoll (EventLoopFdEvents req_ev)
{
    std::uint32_t epoll_ev = 0;
    if ((req_ev & EventLoopFdEvents::Read) != EnumZero) {
        epoll_ev |= EPOLLIN;
    }
    if ((req_ev & EventLoopFdEvents::Write) != EnumZero) {
        epoll_ev |= EPOLLOUT;
    }
    return epoll_ev;
}

static EventLoopFdEvents get_events_to_report (
    std::uint32_t epoll_ev, EventLoopFdEvents req_ev)
{
    EventLoopFdEvents events = EventLoopFdEvents();
    if ((req_ev & EventLoopFdEvents::Read) != EnumZero && (epoll_ev & EPOLLIN) != 0) {
        events |= EventLoopFdEvents::Read;
    }
    if ((req_ev & EventLoopFdEvents::Write) != EnumZero && (epoll_ev & EPOLLOUT) != 0) {
        events |= EventLoopFdEvents::Write;
    }
    if ((epoll_ev & EPOLLERR) != 0) {
        events |= EventLoopFdEvents::Error;
    }
    if ((epoll_ev & EPOLLHUP) != 0) {
        events |= EventLoopFdEvents::Hup;
    }
    return events;
}

}

EventProviderLinux::EventProviderLinux () :
    m_cur_epoll_event(0),
    m_num_epoll_events(0)
{
    m_epoll_fd = FileDescriptorWrapper(::epoll_create1(EPOLL_CLOEXEC));
    if (!m_epoll_fd) {
        throw std::runtime_error("epoll_create1 failed");
    }

    m_timer_fd = FileDescriptorWrapper(
        ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC));
    if (!m_timer_fd) {
        throw std::runtime_error("timerfd_create failed");
    }

    control_epoll(EPOLL_CTL_ADD, *m_timer_fd, EPOLLIN, &m_timer_fd);
}

EventProviderLinux::~EventProviderLinux ()
{}

void EventProviderLinux::waitForEvents (EventLoopWaitTimeoutInfo timeout_info)
{
    namespace chrono = std::chrono;
    using Period = EventLoopTime::period;
    using Rep = EventLoopTime::rep;
    using SecType = decltype((itimerspec()).it_value.tv_sec);
    using NsecType = decltype((itimerspec()).it_value.tv_nsec);
    using NsecDuration = chrono::duration<NsecType, std::nano>;

    static_assert(Period::num == 1, "");
    static_assert(Period::den <= std::nano::den, "");
    static_assert(std::is_signed<Rep>::value, "");
    static_assert(std::is_signed<SecType>::value, "");
    static_assert(TypeMax<Rep>() / Period::den <= TypeMax<SecType>(), "");
    static_assert(TypeMin<Rep>() / Period::den >= TypeMin<SecType>() + 1, "");

    if (timeout_info.time_changed) {
        EventLoopTime::duration time_dur = timeout_info.time.time_since_epoch();

        SecType sec = time_dur.count() / Period::den;
        Rep subsec = time_dur.count() % Period::den;
        if (subsec < 0) {
            sec--;
            subsec += Period::den;
        }

        struct itimerspec itspec = {};
        itspec.it_value.tv_sec = sec;
        itspec.it_value.tv_nsec =
            chrono::duration_cast<NsecDuration>(EventLoopTime::duration(subsec)).count();

        int res = ::timerfd_settime(m_timer_fd.get(), TFD_TIMER_ABSTIME, &itspec, nullptr);
        AIPSTACK_ASSERT_FORCE(res == 0)
    }

    int wait_res;
    while (true) {
        wait_res = ::epoll_wait(m_epoll_fd.get(), m_epoll_events, MaxEpollEvents, -1);
        if (wait_res >= 0) {
            break;
        }
        int err = errno;
        AIPSTACK_ASSERT_FORCE(err == EINTR)
    }

    AIPSTACK_ASSERT_FORCE(wait_res <= MaxEpollEvents)

    m_cur_epoll_event = 0;
    m_num_epoll_events = wait_res;
}

bool EventProviderLinux::dispatchEvents ()
{
    while (m_cur_epoll_event < m_num_epoll_events) {
        struct epoll_event *ev = &m_epoll_events[m_cur_epoll_event++];
        void *data_ptr = ev->data.ptr;

        if (data_ptr == nullptr) {
            continue;
        }

        if (data_ptr == &m_timer_fd) {
            // TODO
            continue;
        }

        auto &fd = *static_cast<EventProviderLinuxFd *>(data_ptr);
        fd.EventProviderFdBase::sanityCheck();

        EventLoopFdEvents events =
            get_events_to_report(ev->events, fd.EventProviderFdBase::getFdEvents());

        if (events != EnumZero) {
            fd.EventProviderFdBase::callFdEventHandler(events);

            if (AIPSTACK_UNLIKELY(EventProviderBase::getStop())) {
                return false;
            }
        }
    }

    return true;
}

void EventProviderLinux::control_epoll (
    int op, int fd, std::uint32_t events, void *data_ptr)
{
    epoll_event ev = {};
    ev.events = events;
    ev.data.ptr = data_ptr;
    
    int res = ::epoll_ctl(m_epoll_fd.get(), op, fd, &ev);
    AIPSTACK_ASSERT_FORCE(res == 0)    
}

void EventProviderLinuxFd::initFdImpl (int fd, EventLoopFdEvents events)
{
    EventProviderLinux &prov = getProvider();

    prov.control_epoll(EPOLL_CTL_ADD, fd, events_to_epoll(events), this);
}

void EventProviderLinuxFd::updateEventsImpl (int fd, EventLoopFdEvents events)
{
    EventProviderLinux &prov = getProvider();

    prov.control_epoll(EPOLL_CTL_MOD, fd, events_to_epoll(events), this);
}

void EventProviderLinuxFd::resetImpl (int fd)
{
    EventProviderLinux &prov = getProvider();

    prov.control_epoll(EPOLL_CTL_DEL, fd, 0, nullptr);

    for (int i = prov.m_cur_epoll_event; i < prov.m_num_epoll_events; i++) {
        struct epoll_event *ev = &prov.m_epoll_events[i];
        if (ev->data.ptr == this) {
            ev->data.ptr = nullptr;
        }
    }
}

EventProviderLinux & EventProviderLinuxFd::getProvider () const
{
    return static_cast<EventProviderLinux &>(EventProviderFdBase::getProvider());
}

}
