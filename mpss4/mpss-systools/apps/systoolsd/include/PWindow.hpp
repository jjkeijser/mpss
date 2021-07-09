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

#ifndef SYSTOOLS_SYSTOOLSD_PWINDOW_HPP_
#define SYSTOOLS_SYSTOOLSD_PWINDOW_HPP_

#include <string>

#include "systoolsd_api.h"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif //UNIT_TESTS

class FileInterface;

class PWindowInterface
{
public:
    virtual ~PWindowInterface() { };
    virtual PowerWindowInfo get_window_info() const = 0;
    virtual uint32_t get_pthresh() const = 0;
    virtual uint32_t get_time_window() const = 0;
    virtual void set_pthresh(uint32_t pthresh_uw) = 0;
    virtual void set_time_window(uint32_t window_us) = 0;
};

class PWindow : public PWindowInterface
{
public:
    PWindow(std::string rapl_zone, uint8_t window_nr);
    virtual ~PWindow();
    PowerWindowInfo get_window_info() const;
    uint32_t get_pthresh() const;
    uint32_t get_time_window() const;
    void set_pthresh(uint32_t pthresh_uw);
    void set_time_window(uint32_t window_us);

PRIVATE:
    //ctor for unit tests
    PWindow(std::string rapl_zone, uint8_t window_nr, FileInterface *impl);

private:
    PWindow();
    PWindow(const PWindow &);
    PWindow &operator=(PWindow &);
    virtual void init(FileInterface *impl);
    FileInterface *sysfs;
    int pthresh_fd;
    int window_fd;
    uint8_t window_nr;
    const std::string sysfs_path;
    const std::string pthresh_path;
    const std::string time_window_path;

};

#endif //SYSTOOLS_SYSTOOLSD_PWINDOW_HPP_
