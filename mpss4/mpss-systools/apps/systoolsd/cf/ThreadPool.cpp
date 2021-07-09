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

#include "ThreadPool.hpp"
#include "ThreadPoolImpl.hpp"
#include <thread>

namespace micmgmt
{
    using namespace std;

    /// @class ThreadPool
    /// @ingroup common
    /// @brief This class creates a thread pool that will execute added work items (derived from WorkItemInterface).
    ///
    /// This class creates a thread pool that will execute added work items (derived from WorkItemInterface).

    /// @brief Returns the platforms maximum logical threads or 0 if undetermined.
    /// @return Returns the platforms maximum logical threads or 0 if undetermined.
    ///
    /// Returns the platforms maximum logical threads or 0 if undetermined.
    unsigned int ThreadPool::hardwareThreads()
    {
        return (unsigned int)std::thread::hardware_concurrency();
    }

    /// @brief Creates a thread pool with the specified number of threads.
    /// @param [in] maxThreads [Optional; default=16] The number threads to create in the thread pool.
    /// @param [in] handler [Optional; default=<tt>NULL</tt>] The signal trap handler that will be registered
    /// for all threads in the pool or \c NULL for not special handling.
    /// @exception std::out_of_range This exception is thrown when \a maxThreads < 2.
    ///
    /// Creates a thread pool with the specified number of threads.
    ThreadPool::ThreadPool(unsigned int maxThreads, micmgmt::trapSignalHandler handler) : impl_(new ThreadPoolImpl(maxThreads, handler))
    {
    }

    /// @brief Destroys the thread pool by calling ThreadPool::stopPool then deleteing the object data.
    ///
    /// Destroys the thread pool by calling ThreadPool::stopPool then deleteing the object data.
    ThreadPool::~ThreadPool()
    {
        delete impl_;
    }

    /// @brief Stops the thread pool by setting \c false the atomic_bool passed to the WorkItemInterface::Run
    /// method of your derived class.
    /// @exception std::logic_error This exception is thrown when ThreadPool::stopPool has already been called once.
    ///
    /// Stops the thread pool by setting \c false the atomic_bool passed to the WorkItemInterface::Run
    /// method of your derived class.
    /// @note You can only stop the pool once and cannot restart it. ThreadPool is intended to be used during the
    /// lifetime of the binary execution of the tool.
    void ThreadPool::stopPool()
    {
        impl_->stopPool();
    }

    /// @brief Adds a work item (object instance derived from WorkItemInterface) to the queue to be executed
    /// on the next free thread in the thread pool.
    /// @param [in] item This is an existing object wrapped in a shared pointer created and passed to the thread
    /// pool for execution by the caller.
    /// @exception std::logic_error This exception is thrown when ThreadPool::stopPool has already been called.
    ///
    /// Adds a work item (object instance derived from WorkItemInterface) to the queue to be executed
    /// on the next free thread in the thread pool.
    void ThreadPool::addWorkItem(const std::shared_ptr<WorkItemInterface>& item)
    {
        impl_->addWorkItem(item);
    }

    /// @brief Waits for all threads in the pool to become idle.
    /// @param [in] timeoutMsSeconds [Optional; default=\c -1] Time in ms to wait for the threads to become idle.
    /// If set to less than zero, the method will wait forever.
    /// @return The return value will be \c false if the timeout occured or \c true if the threads all completed normally.
    /// @exception std::logic_error This exception is thrown when ThreadPool::stopPool has already been called.
    ///
    /// Waits for all threads in the pool to become idle.
    bool ThreadPool::wait(int timeoutMsSeconds)
    {
        return impl_->wait(timeoutMsSeconds);
    }

    /// @brief Returns the number of jobs that the threadpool has started on threads at this instance of the call
    /// to this method.
    /// @return Returns the job count completed (this will include prematurely stopped threads as well).
    ///
    /// Returns the number of jobs that the threadpool has started on threads at this instance of the call
    /// to this method.
    unsigned int ThreadPool::jobCompleted()
    {
        return impl_->jobCompleted();
    }

}; // namespace micmgmt
