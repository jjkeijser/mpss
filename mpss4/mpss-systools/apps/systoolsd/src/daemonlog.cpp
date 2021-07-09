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

#include <cstdarg>
#include <cstdio>
#include <sstream>

#include "SystoolsdException.hpp"

#include "daemonlog.h"

#ifdef SYSTOOLSD_DEBUG

#include <mutex>

namespace
{
    std::mutex m;
}

#endif //SYSTOOLSD_DEBUG

void initlog()
{
#if defined(NO_SYSLOG) || defined(UNIT_TESTS)
    return;
#else
    static bool inited = false;
    if(inited)
        return;

    openlog("systoolsd", LOG_PID|LOG_ODELAY, LOG_USER);
    inited = true;
#endif //NO_SYSLOG
}

void log(int priority, const char *s, ...)
{
#ifdef UNIT_TESTS
    (void)priority;
    (void)s;
    return;
#endif

#ifdef SYSTOOLSD_DEBUG
    {
        std::lock_guard<std::mutex> l(m);
        va_list args;
        va_start(args, s);
        printf("pr=%d : ", priority);
        vprintf(s, args);
        va_end(args);
        printf("\n");
    }
#endif

#ifndef NO_SYSLOG
    va_list args;
    va_start(args, s);
    vsyslog(priority, s, args);
    va_end(args);
#endif //NO_SYSLOG
}

void log(int priority, const SystoolsdException &excp, const char *s, ...)
{
#ifdef UNIT_TESTS
    (void)priority;
    (void)excp;
    (void)s;
    return;
#endif

#ifdef SYSTOOLSD_DEBUG
    {
        std::lock_guard<std::mutex> l(m);
        va_list args;
        va_start(args, s);
        printf("pr=%d : ", priority);
        printf("systoolsd error 0x%x (%s) : ", excp.error_nr(), excp.error_type());
        vprintf(s, args);
        va_end(args);
        printf("\n");
    }
#endif

#ifndef NO_SYSLOG
    va_list args;
    std::stringstream ss;
    ss << "systoolsd error 0x" << std::hex << excp.error_nr() << " (" << excp.error_type()
        << ") : " << s;
    va_start(args, s);
    vsyslog(priority, ss.str().c_str(), args);
    va_end(args);
#endif //NO_SYSLOG
}

void log(int priority, const SystoolsdException &excp)
{
#ifdef UNIT_TESTS
    (void)priority;
    (void)excp;
    return;
#endif

#ifdef SYSTOOLSD_DEBUG
    {
        std::lock_guard<std::mutex> l(m);
        printf("pr=%d : ", priority);
        printf("systoolsd error 0x%x (%s)\n", excp.error_nr(), excp.error_type());
    }
#endif

#ifndef NO_SYSLOG
    std::stringstream ss;
    ss << "systoolsd error 0x" << std::hex << excp.error_nr() << " (" << excp.error_type()
        << ")";
    syslog(priority, ss.str().c_str());
#endif //NO_SYSLOG
}

void shutdownlog()
{
    closelog();
}
