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

#ifndef MICMGMT_MSTIMER_HPP
#define MICMGMT_MSTIMER_HPP

#include <chrono>

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

namespace micmgmt
{
    class MsTimer
    {
    public: // API
        MsTimer();
        MsTimer(unsigned int milliseconds);

        bool expired() const;

        std::chrono::milliseconds elapsed() const;

        void timeout() const;

        void reset(unsigned int milliseconds);

        unsigned int timerValue() const;

    public: // STATIC
        static void sleep(unsigned int milliseconds);

    PRIVATE: // DATA
        std::chrono::milliseconds                      ms_;
        std::chrono::high_resolution_clock::time_point now_;
    };

}; // namespace micmgmt

#endif // MICMGMT_MSTIMER_HPP
