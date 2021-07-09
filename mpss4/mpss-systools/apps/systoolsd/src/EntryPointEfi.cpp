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

#include <cstring>
#include <fstream>
#include <sstream>

#include "FileReader.hpp"
#include "smbios/EntryPointEfi.hpp"
#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

EntryPointEfi::EntryPointEfi() :
    m_got_the_addr(false), m_addr(0), m_efi(new FileReaderStream<std::ifstream>())
{
    //nothing to do
}

EntryPointEfi::~EntryPointEfi()
{
    close();
}

uint64_t EntryPointEfi::get_addr()
{
    if(m_got_the_addr)
        return m_addr;

    if(!m_efi->is_open())
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "file is not open");
    }

    const size_t len = 64;
    char buf[len];
    uint64_t addr;
    while(m_efi->good())
    {
        bzero(buf, len);
        m_efi->getline(buf, len);
        std::string temp(buf);

        auto eq_pos = temp.find("=");
        if (eq_pos == std::string::npos)
                continue;
        if (temp.compare(0, eq_pos, "SMBIOS"))
                continue;

        std::stringstream ss(temp.substr(eq_pos + 1));
        ss >> std::hex >> addr;
        if (ss.fail())
            break;
        m_got_the_addr = true;
        m_addr = addr;
        return addr;
    }
    throw SystoolsdException(SYSTOOLSD_IO_ERROR, "no SMBIOS");
}

bool EntryPointEfi::is_efi() const
{
    return true;
}

void EntryPointEfi::open()
{
    //First option...
    m_efi->open("/sys/firmware/efi/systab");
    if(!m_efi->good())
        m_efi->clear();
    else
        return;

    // Last option
    m_efi->open("/proc/efi/systab");
    if(m_efi->good())
        return;

    throw SystoolsdException(SYSTOOLSD_IO_ERROR, "EFI not found");
}

void EntryPointEfi::close()
{
    m_efi->close();
}

void EntryPointEfi::set_file_reader(FileReader *stream)
{
    if(!stream)
        throw std::invalid_argument("NULL FileReader");

    m_efi.reset(stream);
}
