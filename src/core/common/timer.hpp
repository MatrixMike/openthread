/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions for the multiplexed timer service.
 */

#ifndef TIMER_HPP_
#define TIMER_HPP_

#include "openthread-core-config.h"

#include <stddef.h>
#include <stdint.h>

#include <openthread/platform/alarm-micro.h>
#include <openthread/platform/alarm-milli.h>

#include "common/debug.hpp"
#include "common/linked_list.hpp"
#include "common/locator.hpp"
#include "common/non_copyable.hpp"
#include "common/tasklet.hpp"
#include "common/time.hpp"

namespace ot {

/**
 * @addtogroup core-timer
 *
 * @brief
 *   This module includes definitions for the multiplexed timer service.
 *
 * @{
 */

/**
 * Represents an object tracking the next fire time along with the current time (now).
 */
class NextFireTime
{
public:
    /**
     * Initializes the `NextFireTime` with a given current time.
     *
     * @pram[in] aNow    The current time.
     */
    explicit NextFireTime(Time aNow);

    /**
     * Initializes the `NextFireTime` using `TimerMilli::GetNow()` for current time.
     */
    NextFireTime(void);

    /**
     * Gets the current time (now) tracked by the `NextFireTime` object.
     *
     * @returns The current time.
     */
    Time GetNow(void) const { return mNow; }

    /**
     * Updates the tracked next fire time with a new given time only if it is earlier.
     *
     * If the given @p aTime is in the past relative to the tracked `GetNow()`, the `GetNow()` time is used instead.
     * This ensures that the next fire time is never scheduled before the current time.
     *
     * @param[in] aTime   The new time.
     */
    void UpdateIfEarlier(Time aTime);

    /**
     * Updates the tracked next fire time with a new given time, but only if it is earlier than the current
     * fire time and in the future relative to `GetNow()`.
     *
     * If the given @p aTime is not in the future relative to `GetNow()`, it is ignored. This is unlike
     * `UpdateIfEarlier()`, which allows all `aTime` values, including ones that are in the past (where it uses
     * `GetNow()`).
     *
     * This method can be used to track the next fire time among non-expired times, ensuring the tracked next fire time
     * will be in the future relative to `GetNow()`.
     *
     * @param[in] aTime     The new time.
     */
    void UpdateIfEarlierAndInFuture(Time aTime);

    /**
     * Indicates whether or not next fire time is set.
     *
     * @retval TRUE   The next fire time is set.
     * @retval FALSE  The next fire time is not set.
     */
    bool IsSet(void) const { return (mNextTime != mNow.GetDistantFuture()); }

    /**
     * Gets the next fire time.
     *
     * If the next fire time is not, `GetNow().GetDistantFuture()` will be returned.
     *
     * @returns The next fire time.
     */
    Time GetNextTime(void) const { return mNextTime; }

private:
    Time mNow;
    Time mNextTime;
};

/**
 * Implements a timer.
 */
class Timer : public InstanceLocator, public LinkedListEntry<Timer>
{
    friend class LinkedListEntry<Timer>;

public:
    /**
     * This constant defines maximum delay allowed when starting a timer.
     */
    static const uint32_t kMaxDelay = (Time::kMaxDuration >> 1);

    /**
     * Defines a function reference which is invoked when the timer expires.
     *
     * @param[in]  aTimer    A reference to the expired timer instance.
     */
    typedef void (&Handler)(Timer &aTimer);

    /**
     * Returns the fire time of the timer.
     *
     * @returns The fire time.
     */
    Time GetFireTime(void) const { return mFireTime; }

    /**
     * Indicates whether or not the timer instance is running.
     *
     * @retval TRUE   If the timer is running.
     * @retval FALSE  If the timer is not running.
     */
    bool IsRunning(void) const { return (mNext != this); }

protected:
    class Scheduler : public InstanceLocator, private NonCopyable
    {
        friend class Timer;

    protected:
        struct AlarmApi
        {
            void (*AlarmStartAt)(otInstance *aInstance, uint32_t aT0, uint32_t aDt);
            void (*AlarmStop)(otInstance *aInstance);
            uint32_t (*AlarmGetNow)(void);
        };

        explicit Scheduler(Instance &aInstance)
            : InstanceLocator(aInstance)
        {
        }

        void Add(Timer &aTimer, const AlarmApi &aAlarmApi);
        void Remove(Timer &aTimer, const AlarmApi &aAlarmApi);
        void RemoveAll(const AlarmApi &aAlarmApi);
        void ProcessTimers(const AlarmApi &aAlarmApi);
        void SetAlarm(const AlarmApi &aAlarmApi);

        LinkedList<Timer> mTimerList;
    };

    Timer(Instance &aInstance, Handler aHandler)
        : InstanceLocator(aInstance)
        , mHandler(aHandler)
        , mNext(this)
    {
    }

    bool DoesFireBefore(const Timer &aSecondTimer, Time aNow) const;
    void Fired(void) { mHandler(*this); }

    Handler mHandler;
    Time    mFireTime;
    Timer  *mNext;
};

extern "C" void otPlatAlarmMilliFired(otInstance *aInstance);

/**
 * Implements the millisecond timer.
 */
class TimerMilli : public Timer
{
public:
    /**
     * Implements the millisecond timer scheduler.
     */
    class Scheduler : private Timer::Scheduler
    {
        friend class TimerMilli;
        friend void otPlatAlarmMilliFired(otInstance *aInstance);

    public:
        /**
         * Initializes the object.
         *
         * @param[in]  aInstance  A reference to the instance object.
         */
        explicit Scheduler(Instance &aInstance)
            : Timer::Scheduler(aInstance)
        {
        }

    private:
        void Add(TimerMilli &aTimer) { Timer::Scheduler::Add(aTimer, sAlarmMilliApi); }
        void Remove(TimerMilli &aTimer) { Timer::Scheduler::Remove(aTimer, sAlarmMilliApi); }
        void RemoveAll(void) { Timer::Scheduler::RemoveAll(sAlarmMilliApi); }
        void ProcessTimers(void) { Timer::Scheduler::ProcessTimers(sAlarmMilliApi); }

        static const AlarmApi sAlarmMilliApi;
    };

    /**
     * Creates a millisecond timer instance.
     *
     * @param[in]  aInstance   A reference to the OpenThread instance.
     * @param[in]  aHandler    A pointer to a function that is called when the timer expires.
     */
    TimerMilli(Instance &aInstance, Handler aHandler)
        : Timer(aInstance, aHandler)
    {
    }

    /**
     * Schedules the timer to fire after a given delay (in milliseconds) from now.
     *
     * @param[in]  aDelay   The delay in milliseconds. It must not be longer than `kMaxDelay`.
     */
    void Start(uint32_t aDelay);

    /**
     * Schedules the timer to fire after a given delay (in milliseconds) from a given start time.
     *
     * @param[in]  aStartTime  The start time.
     * @param[in]  aDelay      The delay in milliseconds. It must not be longer than `kMaxDelay`.
     */
    void StartAt(TimeMilli aStartTime, uint32_t aDelay);

    /**
     * Schedules the timer to fire at a given fire time.
     *
     * @param[in]  aFireTime  The fire time.
     */
    void FireAt(TimeMilli aFireTime);

    /**
     * Schedules the timer to fire at a given fire time.
     *
     * Is @p aNextFireTime is not set, the timer is stopped.
     *
     * @param[in]  aNextFireTime  The fire time.
     */
    void FireAt(const NextFireTime &aNextFireTime);

    /**
     * Re-schedules the timer with a given a fire time only if the timer is not running or the new given
     * fire time is earlier than the current fire time.
     *
     * @param[in]  aFireTime  The fire time.
     */
    void FireAtIfEarlier(TimeMilli aFireTime);

    /**
     * Re-schedules the timer with a given a fire time only if the timer is not running or the new given
     * fire time is earlier than the current fire time.
     *
     * @param[in]  aNextFireTime  The fire time.
     */
    void FireAtIfEarlier(const NextFireTime &aNextFireTime);

    /**
     * Stops the timer.
     */
    void Stop(void);

    /**
     * Returns the current time in milliseconds.
     *
     * @returns The current time in milliseconds.
     */
    static TimeMilli GetNow(void) { return TimeMilli(otPlatAlarmMilliGetNow()); }

protected:
    static void RemoveAll(Instance &aInstance);
};

/**
 * Defines a timer owned by a specific type and using a method on owner type as the callback.
 *
 * @tparam Owner              The type of the owner of this timer.
 * @tparam HandleTimerPtr     A pointer to a non-static member method of `Owner` to use as timer handler.
 *
 * The `Owner` MUST be a type that is accessible using `InstanceLocator::Get<Owner>()`.
 */
template <typename Owner, void (Owner::*HandleTimerPtr)(void)> class TimerMilliIn : public TimerMilli
{
public:
    /**
     * Initializes the timer.
     *
     * @param[in]  aInstance   The OpenThread instance.
     */
    explicit TimerMilliIn(Instance &aInstance)
        : TimerMilli(aInstance, HandleTimer)
    {
    }

private:
    static void HandleTimer(Timer &aTimer); // Implemented in `instance.hpp`
};

/**
 * Implements a millisecond timer that also maintains a user context pointer.
 *
 * In typical `TimerMilli`/`TimerMicro` use, in the timer callback handler, the owner of the timer is determined using
 * `GetOwner<Type>` method. This method works if there is a single instance of `Type` within OpenThread instance
 * hierarchy. The `TimerMilliContext` is intended for cases where there may be multiple instances of the same class/type
 * using a timer object. `TimerMilliContext` will store a context `void *` information.
 */
class TimerMilliContext : public TimerMilli
{
public:
    /**
     * Creates a millisecond timer that also maintains a user context pointer.
     *
     * @param[in]  aInstance   A reference to the OpenThread instance.
     * @param[in]  aHandler    A pointer to a function that is called when the timer expires.
     * @param[in]  aContext    A pointer to an arbitrary context information.
     */
    TimerMilliContext(Instance &aInstance, Handler aHandler, void *aContext)
        : TimerMilli(aInstance, aHandler)
        , mContext(aContext)
    {
    }

    /**
     * Returns the pointer to the arbitrary context information.
     *
     * @returns Pointer to the arbitrary context information.
     */
    void *GetContext(void) { return mContext; }

private:
    void *mContext;
};

#if OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE

extern "C" void otPlatAlarmMicroFired(otInstance *aInstance);

/**
 * Implements the microsecond timer.
 */
class TimerMicro : public Timer
{
public:
    /**
     * Implements the microsecond timer scheduler.
     */
    class Scheduler : private Timer::Scheduler
    {
        friend class TimerMicro;
        friend void otPlatAlarmMicroFired(otInstance *aInstance);

    public:
        /**
         * Initializes the object.
         *
         * @param[in]  aInstance  A reference to the instance object.
         */
        explicit Scheduler(Instance &aInstance)
            : Timer::Scheduler(aInstance)
        {
        }

    private:
        void Add(TimerMicro &aTimer) { Timer::Scheduler::Add(aTimer, sAlarmMicroApi); }
        void Remove(TimerMicro &aTimer) { Timer::Scheduler::Remove(aTimer, sAlarmMicroApi); }
        void RemoveAll(void) { Timer::Scheduler::RemoveAll(sAlarmMicroApi); }
        void ProcessTimers(void) { Timer::Scheduler::ProcessTimers(sAlarmMicroApi); }

        static const AlarmApi sAlarmMicroApi;
    };

    /**
     * Creates a timer instance.
     *
     * @param[in]  aInstance   A reference to the OpenThread instance.
     * @param[in]  aHandler    A pointer to a function that is called when the timer expires.
     */
    TimerMicro(Instance &aInstance, Handler aHandler)
        : Timer(aInstance, aHandler)
    {
    }

    /**
     * Schedules the timer to fire after a given delay (in microseconds) from now.
     *
     * @param[in]  aDelay   The delay in microseconds. It must not be be longer than `kMaxDelay`.
     */
    void Start(uint32_t aDelay);

    /**
     * Schedules the timer to fire after a given delay (in microseconds) from a given start time.
     *
     * @param[in]  aStartTime  The start time.
     * @param[in]  aDelay      The delay in microseconds. It must not be longer than `kMaxDelay`.
     */
    void StartAt(TimeMicro aStartTime, uint32_t aDelay);

    /**
     * Schedules the timer to fire at a given fire time.
     *
     * @param[in]  aFireTime  The fire time.
     */
    void FireAt(TimeMicro aFireTime);

    /**
     * Stops the timer.
     */
    void Stop(void);

    /**
     * Returns the current time in microseconds.
     *
     * @returns The current time in microseconds.
     */
    static TimeMicro GetNow(void) { return Time(otPlatAlarmMicroGetNow()); }

protected:
    static void RemoveAll(Instance &aInstance);
};

/**
 * Defines a timer owned by a specific type and using a method on owner type as the callback.
 *
 * @tparam Owner              The type of the owner of this timer.
 * @tparam HandleTimerPtr     A pointer to a non-static member method of `Owner` to use as timer handler.
 *
 * The `Owner` MUST be a type that is accessible using `InstanceLocator::Get<Owner>()`.
 */
template <typename Owner, void (Owner::*HandleTimerPtr)(void)> class TimerMicroIn : public TimerMicro
{
public:
    /**
     * Initializes the timer.
     *
     * @param[in]  aInstance   The OpenThread instance.
     */
    explicit TimerMicroIn(Instance &aInstance)
        : TimerMicro(aInstance, HandleTimer)
    {
    }

private:
    static void HandleTimer(Timer &aTimer); // Implemented in `instance.hpp`
};

#endif // OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE

/**
 * @}
 */

} // namespace ot

#endif // TIMER_HPP_
