/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

#include "MsTimer.hpp"

// Macro for the correct system sleep() call.
#ifdef _WIN32
#include <Windows.h>
#define msSleep(x) Sleep((DWORD)x)
#else
#include <unistd.h>
#define msSleep(x) usleep((useconds_t)x * (useconds_t)1000)
#endif

namespace micmgmt
{
    using namespace std;

    /// @class MsTimer
    /// @ingroup common
    /// @brief Class for basic time duration and sleep features of the platform.
    ///
    /// Class for basic time duration and sleep features of the platform.

    /// @brief Constructs a object to perform timeout operations with setting the initial timeout value of 0.
    ///
    /// Constructs a object to perform timeout operations with setting the initial timeout value of 0.
    /// This object can be reused by using the MsTimer::reset method.
    MsTimer::MsTimer()
    {
        reset((unsigned int)0);
        now_ = std::chrono::high_resolution_clock::now();
    }

    /// @class MsTimer
    /// @ingroup common
    /// @brief Class for basic time duration and sleep features of the platform.
    ///
    /// Class for basic time duration and sleep features of the platform.

    /// @brief Constructs a object to perform timeout operations with setting the initial timeout value.
    /// @param [in] milliseconds [Required] This is the delay to wait for before MsTimer::expired becomes \c true.
    /// Setting to \c 0 will pre-expire the timer.
    ///
    /// Constructs a object to perform timeout operations with setting the initial timeout value.
    /// This object can be reused by using the MsTimer::reset method.
    MsTimer::MsTimer(unsigned int milliseconds)
    {
        reset(milliseconds);
        now_ = std::chrono::high_resolution_clock::now();
    }

    /// @brief This method returns \c true if the time specified in the constructor or the MsTimer::reset
    /// method has expired.
    /// @return This method returns \c true if the time has expired or \c false if it has not expired.
    ///
    /// This method returns \c true if the time specified in the constructor or the MsTimer::reset method
    /// has expired.
    bool MsTimer::expired() const
    {
        return (elapsed() > ms_);
    }

    /// @brief This method returns the milliseconds elapsed since the timer was started at construction or
    /// with the MsTimer::reset method.
    /// @return The milliseconds since the timer was started.
    ///
    /// This method returns the milliseconds elapsed since the timer was started at construction or
    /// with the MsTimer::reset method.
    std::chrono::milliseconds MsTimer::elapsed() const
    {
        auto diff = (std::chrono::high_resolution_clock::now() - now_);
        return std::chrono::duration_cast<std::chrono::milliseconds>(diff);
    }

    /// @brief Using the MsTimer::expired method, thie method will block until the timeout occurs.
    ///
    /// Using the MsTimer::expired method, thie method will block until the timeout occurs.
    void MsTimer::timeout() const
    {
        while (expired() == false)
        {
            MsTimer::sleep(1);
        }
    }

    /// @brief This method will reset the timer with a new millisecond value as an unsigned int.
    /// @param [in] milliseconds [Required] The miliseconds to set the timer value for future expiration.
    /// Setting to \c 0 will pre-expire the timer.
    ///
    /// This method will reset the timer with a new millisecond value as an unsigned int.
    void MsTimer::reset(unsigned int milliseconds)
    {
        ms_ = std::chrono::milliseconds(milliseconds);
        now_ = std::chrono::high_resolution_clock::now();
    }

    /// @brief This method will return the original timeout set in the MsTimer::reset() method.
    /// @return The milliseconds timer was started with.
    ///
    /// This method will return the original timeout set in the MsTimer::reset() method.
    unsigned int MsTimer::timerValue() const
    {
        return (unsigned int)ms_.count();
    }

    // Static
    /// @brief This static method exposes a platform independent millisecond sleep function.
    /// @param [in] milliseconds The number of milliseconds to halt the thread for.
    ///
    /// This static method exposes a platform independent millisecond sleep function.
    /// @note This is NOT using a hi-resolution clock!
    void MsTimer::sleep(unsigned int milliseconds)
    {
        msSleep(milliseconds);
    }

}; // namespace micmgmt
