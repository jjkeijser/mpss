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

#include "CountdownMutex.hpp"
#include "MsTimer.hpp"

using namespace std;

namespace micfw
{
    CountdownMutex::CountdownMutex(size_t initialCount) : counter_(0)
    {
        set(initialCount);
    }

    CountdownMutex::~CountdownMutex()
    {
        set(0);
    }

    void CountdownMutex::set(size_t count)
    {
        lock_guard<mutex> guard(lock_);
        if (counter_ > 0)
        {
            if (count == 0)
            {
                counter_ = 0;
            }
            else
            {
                counter_ = count;
            }
        }
        else
        {
            if (count > 0)
            {
                counter_ = count;
            }
        }
    }

    CountdownMutex& CountdownMutex::operator--(int)
    {
        decrement();
        return *this;
    }

    void CountdownMutex::waitForZero()
    {
        while (isZero() == false)
        {
            micmgmt::MsTimer::sleep(10); // 10 ms
        }
    }

    bool CountdownMutex::isZero()
    {
        lock_guard<mutex> guard(lock_);
        return (counter_ == 0);
    }

    // private implementation
    void CountdownMutex::decrement()
    {
        size_t localCounter;
        {
            lock_guard<mutex> guard(lock_);
            localCounter = counter_;
        }
        if (localCounter > 0)
        {
            set(localCounter - 1);
        }
    }

} // namespace micfw
