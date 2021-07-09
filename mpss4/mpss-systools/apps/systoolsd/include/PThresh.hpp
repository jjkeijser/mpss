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

#ifndef SYSTOOLS_SYSTOOLSD_PTHRESH_HPP_
#define SYSTOOLS_SYSTOOLSD_PTHRESH_HPP_

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif //UNIT_TESTS

#include <memory>
#include <string>

#include "PWindow.hpp"

class FileInterface;

class PThreshInterface
{
public:
    virtual ~PThreshInterface() { };
    virtual uint32_t get_max_phys_pwr() const = 0;
    virtual uint32_t get_low_pthresh() const = 0;
    virtual uint32_t get_high_pthresh() const = 0;
    virtual std::shared_ptr<PWindowInterface> window_factory(uint8_t window) const = 0;
};

class PThresh : public PThreshInterface
{
public:
    PThresh();
    virtual ~PThresh();
    virtual uint32_t get_max_phys_pwr() const;
    virtual uint32_t get_low_pthresh() const;
    virtual uint32_t get_high_pthresh() const;
    void set_file_interface(FileInterface *file);
    std::shared_ptr<PWindowInterface> window_factory(uint8_t window) const;

private: //disable
    PThresh(const PThresh &);
    PThresh &operator=(PThresh &);

private:
    std::unique_ptr<FileInterface> sysfs;
    const std::string path;
};

#endif //SYSTOOLS_SYSTOOLSD_PTHRESH_HPP_
