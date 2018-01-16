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

#ifndef AIPSTACK_HOSTED_PLATFORM_IMPL_H
#define AIPSTACK_HOSTED_PLATFORM_IMPL_H

#include <type_traits>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/event_loop/EventLoop.h>

namespace AIpStack {

class HostedPlatformImpl
{
public:
    using ThePlatformRef = PlatformRef<HostedPlatformImpl>;

    inline HostedPlatformImpl (EventLoop &loop);

    inline EventLoop & getEventLoop () const { return m_loop; }

    static bool const ImplIsStatic = false;

    using TimeType = std::make_unsigned_t<EventLoopTime::rep>;

    static constexpr double TimeFreq =
        double(EventLoopTime::period::den) / EventLoopTime::period::num;
    
    static constexpr TimeType RelativeTimeLimit =
        EventLoopTime::max().time_since_epoch().count() / 64;
    
    inline static TimeType getTime ();

    inline TimeType getEventTime ();

    class Timer :
        private NonCopyable<Timer>,
        private ThePlatformRef,
        private EventLoopTimer
    {
    public:
        inline Timer (ThePlatformRef ref);

        inline ~Timer ();

        using ThePlatformRef::ref;

        inline bool isSet () const;

        inline TimeType getSetTime () const;

        inline void unset ();

        inline void setAt (TimeType abs_time);
    };

private:
    inline static TimeType eventLoopTimeToTimeType (EventLoopTime time);

    inline static EventLoopTime timeTypeToEventLoopTime (TimeType time);

private:
    EventLoop &m_loop;
};

HostedPlatformImpl::HostedPlatformImpl (EventLoop &loop) :
    m_loop(loop)
{}

auto HostedPlatformImpl::getTime () -> TimeType
{
    return eventLoopTimeToTimeType(EventLoop::getTime());
}

auto HostedPlatformImpl::getEventTime () -> TimeType
{
    return eventLoopTimeToTimeType(m_loop.getEventTime());
}

HostedPlatformImpl::Timer::Timer (ThePlatformRef ref) :
    ThePlatformRef(ref),
    EventLoopTimer(ref.platformImpl()->m_loop)
{}

HostedPlatformImpl::Timer::~Timer ()
{}

bool HostedPlatformImpl::Timer::isSet () const
{
    return EventLoopTimer::isSet();
}

auto HostedPlatformImpl::Timer::getSetTime () const -> TimeType
{
    return eventLoopTimeToTimeType(EventLoopTimer::getSetTime());
}

void HostedPlatformImpl::Timer::unset ()
{
    return EventLoopTimer::unset();
}

void HostedPlatformImpl::Timer::setAt (TimeType abs_time)
{
    return EventLoopTimer::setAt(timeTypeToEventLoopTime(abs_time));
}

auto HostedPlatformImpl::eventLoopTimeToTimeType (EventLoopTime time) -> TimeType
{
    // Converting signed to unsigned (modulo reduction).
    return TimeType(time.time_since_epoch().count());
}

auto HostedPlatformImpl::timeTypeToEventLoopTime (TimeType time) -> EventLoopTime
{
    // Converting unsigned to signed (implementation defined).
    // No simple way to avoid it.
    return EventLoopTime(EventLoopTime::duration(EventLoopTime::rep(time)));
}

}

#endif
