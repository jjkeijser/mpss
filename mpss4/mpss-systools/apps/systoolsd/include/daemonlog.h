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

#ifndef SYSTOOLS_SYSTOOLSD_DAEMONLOG_H
#define SYSTOOLS_SYSTOOLSD_DAEMONLOG_H

#include <syslog.h>

#define ERROR LOG_ERR
#define WARNING LOG_WARNING
#define INFO LOG_INFO
#define DEBUG LOG_DEBUG

class SystoolsdException;

void initlog();
void log(int priority, const char *s, ...);
void log(int priority, const SystoolsdException &excp);
void log(int priority, const SystoolsdException &excp, const char *s, ...);
void shutdownlog();

#endif //SYSTOOLS_SYSTOOLSD_DAEMONLOG_H
