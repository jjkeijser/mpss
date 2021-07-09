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

#ifndef MICMGMT_THREADPOOL_HPP
#define MICMGMT_THREADPOOL_HPP

#include "WorkItemInterface.hpp"
#include "micmgmtCommon.hpp"

#include <memory>

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

namespace micmgmt
{
    class ThreadPoolImpl;

    class ThreadPool
    {
    public: // Static
        static unsigned int hardwareThreads();

    public: // API
        explicit ThreadPool(unsigned int maxThreads = 0, micmgmt::trapSignalHandler handler = NULL);
        ~ThreadPool();

        void addWorkItem(const std::shared_ptr<WorkItemInterface>& item);
        bool wait(int timeoutSeconds = -1);

        unsigned int jobCompleted();
        void stopPool();

    PRIVATE: // PIMPL DATA
        ThreadPoolImpl* impl_;

    private: // DISABLED
        ThreadPool(const ThreadPool&);
        ThreadPool operator=(const ThreadPool&);
    };

}; // namespace micmgmt

#endif // AMONSON_THREADPOOL_HPP
