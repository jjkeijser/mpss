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
#include <ios>
#include <sstream>
#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "FileReader.hpp"
#include "smbios/EntryPointMemoryScan.hpp"
#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

using std::ios_base;

namespace
{
    const uint64_t smbios_start_offset = 0xf0000;
    const size_t devmem_buffer_len = 0x10000;
}

EntryPointMemoryScan::EntryPointMemoryScan() :
    m_got_the_addr(false), m_addr(0),
    m_devmem(new FileReaderStream<std::ifstream>), m_devmem_path("/dev/mem")
{
    //nothing to do (yet)
}

EntryPointMemoryScan::EntryPointMemoryScan(const std::string devmem_path) :
    m_got_the_addr(false), m_addr(0),
    m_devmem(new FileReaderStream<std::ifstream>), m_devmem_path(devmem_path)
{
    //nothing to do (yet)
}

EntryPointMemoryScan::~EntryPointMemoryScan()
{
    close();
}

uint64_t EntryPointMemoryScan::get_addr()
{
    if(m_got_the_addr)
        return m_addr;

    if(!m_devmem->is_open())
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "file is not open");
    }

    //first, move the file pointer
    m_devmem->seekg(static_cast<std::streampos>(smbios_start_offset));
    if(!m_devmem->good())
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "seek smbios_start_offset");
    }

    //paragraph-aligned
    const std::streampos last_offs = (devmem_buffer_len - 16 - 1) + smbios_start_offset;
    //buffer size
    const size_t len = 16;
    char buf[len];
    uint64_t offset = 0;
    std::streampos pos;
    while((pos = m_devmem->tellg()) != -1 && pos <= last_offs && m_devmem->good())
    {
        m_devmem->read(buf, len); // This already moves the stream pointer by 16 bytes
        if(std::string(buf, 4) == "_SM_")
        {
            m_got_the_addr = true;
            m_addr = offset;
            return offset;
        }

        offset += len;
    }
    throw SystoolsdException(SYSTOOLSD_IO_ERROR, "no SMBIOS entry point found");
}

bool EntryPointMemoryScan::is_efi() const
{
    return false;
}

void EntryPointMemoryScan::set_file_reader(FileReader *devmem_)
{
    if(!devmem_)
    {
        throw std::invalid_argument("NULL FileInterface");
    }
    m_devmem.reset(devmem_);
}

void EntryPointMemoryScan::open()
{
    m_devmem->open(m_devmem_path, ios_base::in|ios_base::binary);
    if(!m_devmem->good())
    {
        std::stringstream ss;
        ss << "unable to open " << m_devmem_path;
        ss << " : " << errno << " : " << strerror(errno);
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, ss.str().c_str());
    }
}

void EntryPointMemoryScan::close()
{
    m_devmem->close();
}
