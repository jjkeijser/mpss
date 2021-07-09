/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
*/

#ifndef MICFW_COUNTDOWNMUTEX_HPP
#define MICFW_COUNTDOWNMUTEX_HPP

#include <mutex>

namespace micfw
{
    class CountdownMutex
    {
    public: // construction/destruction
        explicit CountdownMutex(size_t initialCount);
        ~CountdownMutex();

    public: // API
        void set(size_t count);

        CountdownMutex& operator--(int);

        void waitForZero();
        bool isZero();

    private: // implementation
        void decrement();

    private: // disabled
        CountdownMutex();
        CountdownMutex(const CountdownMutex&);
        CountdownMutex& operator=(const CountdownMutex&);

    private: // Fields
        size_t counter_;
        std::mutex lock_;
    };
} // namespace micfw

#endif // MICFW_COUNTDOWNMUTEX_HPP
