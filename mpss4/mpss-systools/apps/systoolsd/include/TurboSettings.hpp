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

#ifndef SYSTOOLS_SYSTOOLSD_TURBOSETTINGS_HPP_
#define SYSTOOLS_SYSTOOLSD_TURBOSETTINGS_HPP_

#include <cstdint>
#include <memory>

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

class FileInterface;

class TurboSettingsInterface
{
public:
    virtual ~TurboSettingsInterface() { }
    virtual bool is_turbo_enabled() const = 0;
    virtual uint8_t get_turbo_pct() const = 0;
    virtual void set_turbo_enabled(bool enabled) = 0;
};

class TurboSettings : public TurboSettingsInterface
{
public:
    TurboSettings();
    TurboSettings(FileInterface *sysfs);
    ~TurboSettings();
    bool is_turbo_enabled() const;
    uint8_t get_turbo_pct() const;
    void set_turbo_enabled(bool enabled);
    void set_file_interface(FileInterface *file);

protected:
    uint8_t read(const std::string &path) const;

private:
    std::unique_ptr<FileInterface> sysfs;

};

#endif
