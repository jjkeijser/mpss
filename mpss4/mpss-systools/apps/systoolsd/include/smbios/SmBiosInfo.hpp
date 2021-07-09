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

#ifndef _SYSTOOLS_SYSTOOLSD_SMBIOSINFO_HPP_
#define _SYSTOOLS_SYSTOOLSD_SMBIOSINFO_HPP_

#include <cstdint>
#include <map>
#include <vector>

#include "SmBiosStructureBase.hpp"
#include "SmBiosTypes.hpp"
#include "SystoolsdException.hpp"

#include "daemonlog.h"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

class EntryPointFinderInterface;

class SmBiosInterface
{
public:
    virtual ~SmBiosInterface() { }
    virtual void parse() = 0;
    virtual void find_entry_point(EntryPointFinderInterface *finder) = 0;
    virtual const std::vector<SmBiosStructureBase::ptr> &get_structures_of_type(SmBiosStructureType type) const = 0;
};

class SmBiosInfo : public SmBiosInterface
{
public:
    SmBiosInfo();
    ~SmBiosInfo();
    void parse();
    void find_entry_point(EntryPointFinderInterface *finder);
    const std::vector<SmBiosStructureBase::ptr> &get_structures_of_type(SmBiosStructureType type) const;
    template <typename T>
    static std::vector<const T*> cast_to(const std::vector<SmBiosStructureBase::ptr> &structs);
    typedef std::vector<SmBiosStructureBase::ptr> structures_vector;

private:
    SmBiosInfo(const SmBiosInfo&);
    SmBiosInfo &operator=(SmBiosInfo &);

PRIVATE:
    void load_eps(SmBiosEntryPointStructure *eps, const char *const buf, size_t len);
    bool checksum(uint8_t *buf, size_t len);
    bool checksum(const SmBiosEntryPointStructure &eps);
    bool inter_checksum(const SmBiosEntryPointStructure &eps);
    void validate_eps(const SmBiosEntryPointStructure &eps);
    void load_header(SmBiosStructHeader *hdr, const char *const buf, size_t len);
    int64_t get_next_struct_offset(const SmBiosStructHeader &hdr, const char *const buf, size_t len);
    void parse_and_add_structs(const char *const buf, size_t len);
    void add_struct(SmBiosStructHeader &hdr, const char *const buf, size_t len);
    void load_devmem_at_offset(int64_t offset, size_t len);

//used directly by unit tests
PRIVATE:
    std::map<SmBiosStructureType, structures_vector> structures;

private:
    static std::vector<SmBiosStructureType> supported_types;
    char *smbios_buf;
    int64_t entry_point_offset;
    bool is_efi;
};

template <typename T>
std::vector<const T*> SmBiosInfo::cast_to(const std::vector<SmBiosStructureBase::ptr> &structs)
{
    std::vector<const T*> retv;
    for(auto i = structs.begin(); i != structs.end(); ++i)
    {
        const T *p = NULL;
        if( ! (p = dynamic_cast<const T*>(i->get())))
        {
            throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, "invalid SmBiosStructureBase cast");
        }
        retv.push_back(p);
    }
    return retv;
}

#endif //_SYSTOOLS_SYSTOOLSD_SMBIOSINFO_HPP_
