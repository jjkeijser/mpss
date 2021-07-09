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

#ifndef SYSTOOLS_SYSTOOLSD_ENTRYPOINTEFI_HPP_
#define SYSTOOLS_SYSTOOLSD_ENTRYPOINTEFI_HPP_

#include <cstdio>
#include <memory>

#include "FileReader.hpp"
#include "smbios/EntryPointFinderInterface.hpp"

#ifdef UNIT_TESTS
#define PROTECTED public
#else
#define PROTECTED protected
#endif //UNIT_TESTS

class EntryPointEfi : public EntryPointFinderInterface
{
public:
    EntryPointEfi();
    ~EntryPointEfi();

    //methods defined in EntryPointFinderInterface
    uint64_t get_addr();
    bool is_efi() const;

    //methods defined by this class
    void open();
    void close();

PROTECTED:
    void set_file_reader(FileReader *stream);

private:
    EntryPointEfi (const EntryPointEfi &);
    EntryPointEfi &operator=(const EntryPointEfi&);

private:
    bool m_got_the_addr;
    uint64_t m_addr;
    std::unique_ptr<FileReader> m_efi;
};

#endif //SYSTOOLS_SYSTOOLSD_ENTRYPOINTEFI_HPP_
