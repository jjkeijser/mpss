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

#ifndef SYSTOOLS_SYSTOOLSD_ENTRYPOINTMEMORYSCAN_HPP_
#define SYSTOOLS_SYSTOOLSD_ENTRYPOINTMEMORYSCAN_HPP_

#include <memory>
#include <string>

#include "smbios/EntryPointFinderInterface.hpp"

#ifdef UNIT_TESTS
#define PROTECTED public
#else
#define PROTECTED protected
#endif // UNIT_TESTS

class FileReader;

class EntryPointMemoryScan : public EntryPointFinderInterface
{
public:
    EntryPointMemoryScan();
    EntryPointMemoryScan(const std::string devmem_path);
    ~EntryPointMemoryScan();

    //methods defined in EntryPointFinderInterface
    uint64_t get_addr();
    bool is_efi() const;

    //methods defined by the class
    void set_file_reader(FileReader *devmem_);
    void open();
    //not necessary to call close(). Dtor will close it.
    void close();

private:
    EntryPointMemoryScan (const EntryPointMemoryScan&);
    EntryPointMemoryScan &operator=(const EntryPointMemoryScan&);

    bool m_got_the_addr;
    uint64_t m_addr;

private:
    std::unique_ptr<FileReader> m_devmem;
    std::string m_devmem_path;
};

#endif //SYSTOOLS_SYSTOOLSD_ENTRYPOINTMEMORYSCAN_HPP_
