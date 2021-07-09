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

#include "ThreadPoolImpl.hpp"
#include "MsTimer.hpp"
#include "MicLogger.hpp"

#include <iostream>
#include <stdexcept>
#include <csignal>

#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif

namespace micmgmt
{
    using namespace std;

    ThreadPoolImpl::ThreadPoolImpl(unsigned int maxThreads, trapSignalHandler handler)
        : maxThreads_(maxThreads), jobsCompleted_(0), jobsAdded_(0),
        stopPool_(false), handler_(handler)
    {
        unsigned int hwThreads = std::thread::hardware_concurrency();

        if (hwThreads == 0)
        { // Default value for gcc 4.4 because std::thread::hardware_concurrency() failed!
            hwThreads = 24; // Value is for a single processor Xeon Board (conservative choice).
        }
        if (maxThreads_ == 0)
        {
            maxThreads_ = hwThreads;
        }
        if (maxThreads_ < 1)
        {
            throw out_of_range("The thread pool cannot have less than 1 threads");
        }
        if (maxThreads_ > (2 * hwThreads))
        {
            maxThreads_ = 2 * hwThreads;
        }

        for (unsigned int i = 0; i < maxThreads_; ++i)
        {
            auto t = new std::thread(&ThreadPoolImpl::staticThreadFunc, this);
            threads_.push_back(unique_ptr<std::thread>(t));
            // Wait until the created thread is ready and waiting for work
            std::map<std::thread::id, bool>::const_iterator cit;
            for (;;)
            {
                auto notFound = threadsReady_.end();
                if ((cit = threadsReady_.find(t->get_id())) == notFound)
                {
                    MsTimer::sleep(5);
                    continue;
                }
                if (cit->second)
                    break;
            }
        }
    }


    ThreadPoolImpl::~ThreadPoolImpl()
    {
        if (stopPool_ == false)
        {
            stopPool();
        }
    }

    void ThreadPoolImpl::stopPool(long long msTimeout)
    {
        if (stopPool_ == true)
            throw logic_error("Thread pool was already stopped");

        // Wake threads up so that they exit.
        stopPool_ = true;
        queueCv_.notify_all();

        long long count = (long long)threads_.size();
        long long individualTimeout = msTimeout / count;
        for (unsigned int i = 0; i < maxThreads_; ++i)
        {
            if (threads_[i]->joinable() == true)
            {
                if (msTimeout <= 0)
                {
                    // WARNING: If WorkItem::Run() implementations misbehaves this can hang forever!
                    threads_[i]->join();
                }
                else
                {
                    platformStopThread(threads_[i], individualTimeout);
                }
            }
        }
        threads_.clear();
    }

    void ThreadPoolImpl::addWorkItem(const shared_ptr<WorkItemInterface>& item)
    {
        if (stopPool_ == true)
            throw logic_error("Thread pool not running");

        std::unique_lock<std::mutex> ul(workItemsLock_);
        workItems_.push(item);
        ++jobsAdded_;
        queueCv_.notify_one();
    }

    bool ThreadPoolImpl::wait(int timeoutMsSeconds)
    {
        if (stopPool_ == true)
            throw logic_error("Thread pool was already stopped");

        if (timeoutMsSeconds > 0)
        {
            MsTimer timer(timeoutMsSeconds);
            while (jobsFinished() == false && timer.expired() == false)
            {
                MsTimer::sleep(10);
            }
            return !timer.expired();
        }
        else
        {
            while (jobsFinished() == false)
            {
                MsTimer::sleep(10);
            }
            return true;
        }
    }

    unsigned int ThreadPoolImpl::jobCompleted()
    {
        std::lock_guard<std::mutex> l(mutexJobsCompleted_);
        return jobsCompleted_;
    }

    // Implementation
    void ThreadPoolImpl::staticThreadFunc(ThreadPoolImpl* thisPtr)
    {
        thisPtr->threadFunction();
    }

    void ThreadPoolImpl::threadFunction()
    {
        registerTrapSignalHandler(handler_);
        while (stopPool_ == false)
        {
            std::shared_ptr<WorkItemInterface> wi;
            {
                //Let's wait until there are items to process
                std::unique_lock<std::mutex> ul(workItemsLock_);
                //Let the main thread know we are ready
                threadsReady_[std::this_thread::get_id()] = true;
                queueCv_.wait(ul,
                    [this]
                    {
                        return this->workItems_.size() != 0 || stopPool_ == true;
                    });
                //If we woke up but a stopPool() has been called...
                if (stopPool_ == true)
                    return;

                wi = workItems_.front();
                workItems_.pop();
            }

            wi->Run(stopPool_); // Do Work
            {
                std::lock_guard<std::mutex> l(mutexJobsCompleted_);
                ++jobsCompleted_;
            }
        }
        unregisterTrapSignalHandler(handler_);
    }

    bool ThreadPoolImpl::jobsFinished()
    {
        bool finished = false;
        {
            lock_guard<mutex> guard(mutexJobsCompleted_);
            finished = (jobsAdded_ == jobsCompleted_);
        }
        return finished;
    }
}; // namespace micmgmt

// NOTE: There are no UT tests for "hard terminate" feature of this method.  It is currently
// inaccessible from the public interface.  It is a place holder only for a possible feature
// and is experimental only!
void micmgmt::ThreadPoolImpl::platformStopThread(std::unique_ptr<std::thread>& thrd, long long timeout)
{
    MsTimer timer((int)timeout);
    while(timer.expired() == false && thrd->joinable() == true)
    {
        MsTimer::sleep(1);
    }
    if (thrd->joinable() == false)
    {
        return; // thread terminated within timeout normally.
    }

#ifdef _WIN32
    thread::native_handle_type native = thrd->native_handle(); // Get OS handle...
    thrd->detach(); // Remove handle from std::thread object tracking...
    TerminateThread((HANDLE)native, 1); // Terminate on Windows
#else
    // pthread_kill() appears undefined in RH 7 or in RH 6.5!!!
    // thread::native_handle_type native = thrd->native_handle(); // Get OS handle...
    // thrd->detach(); // Remove handle from std::thread object tracking...
    // pthread_kill((pthread_t)native, 15); // (SIGTERM) Terminate on Linux
#endif
}
