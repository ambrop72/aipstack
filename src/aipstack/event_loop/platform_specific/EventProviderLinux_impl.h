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

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <type_traits>
#include <chrono>
#include <utility>

#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/event_loop/FormatString.h>
#include <aipstack/event_loop/EventLoopCommon.h>
#include <aipstack/event_loop/platform_specific/EventProviderLinux.h>

namespace AIpStack {

namespace EventProviderLinuxPriv {

inline std::uint32_t get_events_to_request (EventLoopFdEvents req_ev)
{
    std::uint32_t epoll_ev = 0;
    if ((req_ev & EventLoopFdEvents::Read) != Enum0) {
        epoll_ev |= EPOLLIN;
    }
    if ((req_ev & EventLoopFdEvents::Write) != Enum0) {
        epoll_ev |= EPOLLOUT;
    }
    return epoll_ev;
}

inline EventLoopFdEvents get_events_to_report (
    std::uint32_t epoll_ev, EventLoopFdEvents req_ev)
{
    EventLoopFdEvents events = EventLoopFdEvents();
    if ((req_ev & EventLoopFdEvents::Read) != Enum0 && (epoll_ev & EPOLLIN) != 0) {
        events |= EventLoopFdEvents::Read;
    }
    if ((req_ev & EventLoopFdEvents::Write) != Enum0 && (epoll_ev & EPOLLOUT) != 0) {
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
    m_timerfd_time(EventLoopTime::max()),
    m_force_timerfd_update(true),
    m_cur_epoll_event(0),
    m_num_epoll_events(0)
{
    m_epoll_fd = FileDescriptorWrapper(::epoll_create1(EPOLL_CLOEXEC));
    if (!m_epoll_fd) {
        throw std::runtime_error(formatString(
            "EventProviderLinux: epoll_create1 failed, err=%d", errno));
    }

    m_timer_fd = FileDescriptorWrapper(
        ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC));
    if (!m_timer_fd) {
        throw std::runtime_error(formatString(
            "EventProviderLinux: timerfd_create failed, err=%d", errno));
    }

    m_event_fd = FileDescriptorWrapper(::eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC));
    if (!m_event_fd) {
        throw std::runtime_error(formatString(
            "EventProviderLinux: eventfd failed, err=%d", errno));
    }

    control_epoll(EPOLL_CTL_ADD, *m_timer_fd, EPOLLIN, &m_timer_fd);
    control_epoll(EPOLL_CTL_ADD, *m_event_fd, EPOLLIN, &m_event_fd);
}

EventProviderLinux::~EventProviderLinux ()
{}

void EventProviderLinux::waitForEvents (EventLoopTime wait_time)
{
    AIPSTACK_ASSERT(m_cur_epoll_event == m_num_epoll_events)
    
    namespace chrono = std::chrono;
    using Period = EventLoopTime::period;
    using Rep = EventLoopTime::rep;
    using SecType = decltype(itimerspec().it_value.tv_sec);
    using NsecType = decltype(itimerspec().it_value.tv_nsec);
    using NsecDuration = chrono::duration<NsecType, std::nano>;

    static_assert(Period::num == 1);
    static_assert(Period::den <= std::nano::den);
    static_assert(std::is_signed<Rep>::value);
    static_assert(std::is_signed<SecType>::value);
    static_assert(TypeMax<Rep>() / Period::den <= TypeMax<SecType>());
    static_assert(TypeMin<Rep>() / Period::den >= TypeMin<SecType>() + 1);

    if (wait_time != m_timerfd_time || m_force_timerfd_update) {
        m_force_timerfd_update = true;

        EventLoopTime::duration time_dur = wait_time.time_since_epoch();

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

        // Prevent accidentally disarming the timerfd.
        if (itspec.it_value.tv_sec == 0 && itspec.it_value.tv_nsec == 0) {
            itspec.it_value.tv_nsec = 1;
        }

        if (::timerfd_settime(*m_timer_fd, TFD_TIMER_ABSTIME, &itspec, nullptr) < 0) {
            throw std::runtime_error(formatString(
                "EventProviderLinux: timerfd_settime failed, err=%d", errno));
        }

        m_timerfd_time = wait_time;
        m_force_timerfd_update = false;
    }

    int wait_res;
    while (true) {
        wait_res = ::epoll_wait(*m_epoll_fd, m_epoll_events, MaxEpollEvents, -1);
        if (AIPSTACK_LIKELY(wait_res >= 0)) {
            break;
        }
        
        int err = errno;
        if (err != EINTR) {
            throw std::runtime_error(formatString(
                "EventProviderLinux: epoll_wait failed, err=%d", err));
        }
    }

    AIPSTACK_ASSERT(wait_res <= MaxEpollEvents)

    m_cur_epoll_event = 0;
    m_num_epoll_events = wait_res;
}

bool EventProviderLinux::dispatchEvents ()
{
    using namespace EventProviderLinuxPriv;

    while (m_cur_epoll_event < m_num_epoll_events) {
        struct epoll_event *ev = &m_epoll_events[m_cur_epoll_event++];
        void *data_ptr = ev->data.ptr;

        if (data_ptr == nullptr) {
            continue;
        }

        if (data_ptr == &m_timer_fd) {
            // Don't read from timerfd since this is relatively expensive, but make sure
            // that we call timerfd_settime before the next epoll_wait, which clears events
            // from the timer.
            m_force_timerfd_update = true;
        }
        else if (data_ptr == &m_event_fd) {
            std::uint64_t value;
            auto res = ::read(*m_event_fd, &value, sizeof(value));
            
            if (AIPSTACK_UNLIKELY(res < 0)) {
                int err = errno;
                if (err != EAGAIN) {
                    std::fprintf(stderr,
                        "EventProviderLinux: read from eventfd failed, err=%d\n", err);
                }
            }

            if (!EventProviderBase::dispatchAsyncSignals()) {
                return false;
            }
        }
        else {
            auto &fd = *static_cast<EventProviderLinuxFd *>(data_ptr);
            fd.EventProviderFdBase::sanityCheck();

            EventLoopFdEvents events =
                get_events_to_report(ev->events, fd.EventProviderFdBase::getFdEvents());

            if (events != Enum0) {
                if (!fd.EventProviderFdBase::callFdEventHandler(events)) {
                    return false;
                }
            }
        }
    }

    return true;
}

void EventProviderLinux::signalToCheckAsyncSignals ()
{
    std::uint64_t value = 1;
    auto res = ::write(*m_event_fd, &value, sizeof(value));

    if (AIPSTACK_UNLIKELY(res < 0)) {
        int err = errno;
        if (err != EAGAIN) {
            std::fprintf(stderr,
                "EventProviderLinux: write to eventfd failed, err=%d\n", err);
        }
    }
}

void EventProviderLinux::control_epoll (
    int op, int fd, std::uint32_t events, void *data_ptr)
{
    epoll_event ev = {};
    ev.events = events;
    ev.data.ptr = data_ptr;
    
    if (AIPSTACK_UNLIKELY(::epoll_ctl(*m_epoll_fd, op, fd, &ev) < 0)) {
        throw std::runtime_error(formatString(
            "EventProviderLinux: epoll_ctl failed, err=%d", errno));
    }
}

void EventProviderLinuxFd::initFdImpl (int fd, EventLoopFdEvents events)
{
    using namespace EventProviderLinuxPriv;

    EventProviderLinux &prov = getProvider();

    prov.control_epoll(EPOLL_CTL_ADD, fd, get_events_to_request(events), this);
}

void EventProviderLinuxFd::updateEventsImpl (EventLoopFdEvents events)
{
    using namespace EventProviderLinuxPriv;

    EventProviderLinux &prov = getProvider();

    EventLoopFdEvents cur_events = EventProviderFdBase::getFdEvents();

    EventLoopFdEvents mask = EventLoopFdEvents::Read|EventLoopFdEvents::Write;

    if ((events & mask) != (cur_events & mask)) {
        int fd = EventProviderFdBase::getFd();
        prov.control_epoll(EPOLL_CTL_MOD, fd, get_events_to_request(events), this);
    }
}

void EventProviderLinuxFd::resetImpl ()
{
    EventProviderLinux &prov = getProvider();

    int fd = EventProviderFdBase::getFd();

    try {
        prov.control_epoll(EPOLL_CTL_DEL, fd, 0, nullptr);
    } catch (std::runtime_error const &ex) {
        // This is called from EventLoopFdWatcher destructor and reset() and therefore
        // must not throw. Let's hope the error is something benign and the file descriptor
        // is no longer associated, because if it is and it generates an event, undefined
        // behavior would occur as dispatchEvents() tries to process it.
        std::fprintf(stderr, "%s\n", ex.what());
    }

    // Set the data.ptr pointer in any unprocessed events for this file descriptor to
    // inhibit their processing.
    for (int i = prov.m_cur_epoll_event; i < prov.m_num_epoll_events; i++) {
        struct epoll_event &ev = prov.m_epoll_events[i];
        if (ev.data.ptr == this) {
            ev.data.ptr = nullptr;
        }
    }
}

EventProviderLinux & EventProviderLinuxFd::getProvider () const
{
    return static_cast<EventProviderLinux &>(EventProviderFdBase::getProvider());
}

}
