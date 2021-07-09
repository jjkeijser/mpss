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

#ifndef _SYSTOOLS_SYSTOOLSD_CACHEDDATAGROUPBASE_HPP_
#define _SYSTOOLS_SYSTOOLSD_CACHEDDATAGROUPBASE_HPP_

#include <cstdint>
#include <cstring>
#include <mutex>

//for gettimeofday
#include <sys/time.h>

#include "DataGroupInterface.hpp"

#ifdef UNIT_TESTS
#define PROTECTED public
#else
#define PROTECTED protected
#endif

/* CachedDataGroupBase
 * This class template adds the logic for data caching based on
 * the expiration time passed to the constructor
 */

template <typename T>
class CachedDataGroupBase : public DataGroupInterface
{
public:
    //CachedDataGroupBase();
    virtual ~CachedDataGroupBase();
    const T &get_data(bool force_refresh=false);
    virtual size_t get_size();
    virtual void copy_data_to(char *buf, size_t *size);
    virtual void *get_raw_data(bool force_refresh=false);
    virtual void force_refresh();

protected:
    CachedDataGroupBase(uint64_t expiration_time=0);
    uint64_t expiration_time;
    T data;

PROTECTED:
    uint64_t last_refresh;

private:
    void lock_and_refresh();
    void update_last_refresh();
    T &p_get_data();
    std::mutex mutex_data;
    std::mutex mutex_refresh_time;
    typedef std::lock_guard<std::mutex> lock;
};

template<typename T>
CachedDataGroupBase<T>::CachedDataGroupBase(uint64_t expiration_time) :
    expiration_time(expiration_time), data(), last_refresh(0)
{
}

template <typename T>
CachedDataGroupBase<T>::~CachedDataGroupBase()
{

}

template <typename T>
size_t CachedDataGroupBase<T>::get_size()
{
    //If this is the first call
    if(!last_refresh)
        (void)get_data();

    return sizeof(data);
}

template <typename T>
void CachedDataGroupBase<T>::copy_data_to(char *buf, size_t *size)
{
    if(!size)
        return;

    (void)get_data();

    if(buf)
    {
        memcpy(buf, &data, *size);
    }

    *size = get_size();
    return;
}

template <typename T>
void *CachedDataGroupBase<T>::get_raw_data(bool force_refresh)
{
    return (void*)&get_data(force_refresh);
}

template <typename T>
void CachedDataGroupBase<T>::force_refresh()
{
    lock_and_refresh();
}

template <typename T>
const T &CachedDataGroupBase<T>::get_data(bool force_refresh)
{
    struct timeval tval;
    gettimeofday(&tval, 0);

    uint64_t currtime = tval.tv_usec / 1000; //turn into milliseconds
    currtime += tval.tv_sec * 1000;

    if(force_refresh)
    {
        lock_and_refresh();
        return p_get_data();
    }

    //if data group is static
    if(expiration_time == 0)
    {
        if(!last_refresh)
            lock_and_refresh();
        return p_get_data();
    }

    //If this is the first call
    if(!last_refresh)
    {
        lock_and_refresh();
        return p_get_data();
    }

    //If data has gone stale
    if((currtime - last_refresh) > expiration_time)
    {
        lock_and_refresh();
    }

    return p_get_data();
}

template <typename T>
void CachedDataGroupBase<T>::lock_and_refresh()
{
    lock l(mutex_data);
    refresh_data();
    update_last_refresh();
}

template <typename T>
void CachedDataGroupBase<T>::update_last_refresh()
{
    lock l(mutex_refresh_time);
    struct timeval tval;
    gettimeofday(&tval, 0);
    last_refresh = (tval.tv_usec / 1000) + (tval.tv_sec * 1000);
}

template <typename T>
T &CachedDataGroupBase<T>::p_get_data()
{
    lock l(mutex_data);
    return data;
}

#endif
