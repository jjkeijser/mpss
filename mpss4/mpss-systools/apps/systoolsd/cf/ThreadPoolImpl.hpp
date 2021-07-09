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

#ifndef MICMGMT_THREADPOOLIMPL_HPP
#define MICMGMT_THREADPOOLIMPL_HPP

#include <condition_variable>
#include <memory>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "WorkItemInterface.hpp"
#include "micmgmtCommon.hpp"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

namespace micmgmt
{
    class ThreadPoolImpl
    {
    public: // API
        explicit ThreadPoolImpl(unsigned int maxThreads, trapSignalHandler handler = NULL);
        ~ThreadPoolImpl();

        void stopPool(long long msTimeout = 0);

        void addWorkItem(const std::shared_ptr<WorkItemInterface>& item);
        bool wait(int timeoutSeconds = -1);

		unsigned int jobCompleted();

    PRIVATE: // DATA
        unsigned int                              maxThreads_;
        unsigned int                              jobsCompleted_;
        unsigned int                              jobsAdded_;

        std::vector<std::unique_ptr<std::thread>> threads_;

        std::queue<std::shared_ptr<WorkItemInterface>> workItems_;
        std::mutex                                workItemsLock_;
        std::mutex                                mutexJobsCompleted_;
        std::condition_variable                   queueCv_;
        std::map<std::thread::id, bool>           threadsReady_;

        SafeBool                                  stopPool_;

        trapSignalHandler                         handler_;

    private: // DISABLE
        ThreadPoolImpl();
        ThreadPoolImpl(const ThreadPoolImpl&);
        ThreadPoolImpl operator=(const ThreadPoolImpl&);

    private: // IMPLEMENTATION
        void threadFunction();
        bool jobsFinished();
        void platformStopThread(std::unique_ptr<std::thread>& thrd, long long timeout);

    private: // STATIC
        static void staticThreadFunc(ThreadPoolImpl* thisPtr);
    };

}; // namespace micmgmt

#endif // MICMGMT_THREADPOOLIMPL_HPP
