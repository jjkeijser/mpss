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
#include <stdexcept>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "FileInterface.hpp"
#include "smbios/EntryPointFinderInterface.hpp"
#include "smbios/SmBiosInfo.hpp"
#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

namespace
{
    const char devmem[] = "/dev/mem";
    const uint8_t end_of_table = 127;
    const uint64_t smbios_start_offset = 0xf0000;
}

std::vector<SmBiosStructureType> SmBiosInfo::supported_types =
{
    SmBiosStructureType::BIOS,
    SmBiosStructureType::SYSTEM_INFO,
    SmBiosStructureType::PROCESSOR_INFO,
    SmBiosStructureType::MEMORY_DEVICE
};

SmBiosInfo::SmBiosInfo() : structures(), smbios_buf(0),
    entry_point_offset(-1), is_efi(false)
{
    //initialize structures map
    for(auto i = supported_types.begin(); i != supported_types.end(); ++i)
    {
        structures[*i] = structures_vector();
    }
}

void SmBiosInfo::parse()
{
    //user of this class must have called find_entry_point() at this point
    if(entry_point_offset == -1)
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "entry_point_offset = -1");
    }

    SmBiosEntryPointStructure eps;
    bzero(&eps, sizeof(SmBiosEntryPointStructure));
    smbios_buf = NULL;

    if(is_efi)
    {
        load_devmem_at_offset(entry_point_offset, 0x20);
        load_eps(&eps, smbios_buf, sizeof(SmBiosEntryPointStructure));
    }
    else
    {
        const size_t len = 0x10000;
        //load /dev/mem
        //contents will be loaded in class member smbios_buf
        load_devmem_at_offset(smbios_start_offset, len);
        load_eps(&eps, smbios_buf + entry_point_offset, sizeof(SmBiosEntryPointStructure));
    }

    validate_eps(eps);

    //use entry point structure to find start of structures table
    //table is now loaded in smbios_buf
    load_devmem_at_offset(eps.struct_table_address, eps.struct_table_length);
    parse_and_add_structs(smbios_buf, eps.struct_table_length);
}

SmBiosInfo::~SmBiosInfo()
{
    if(smbios_buf)
    {
        delete [] smbios_buf;
        smbios_buf = NULL;
    }
}

const std::vector<SmBiosStructureBase::ptr> &SmBiosInfo::get_structures_of_type(SmBiosStructureType type) const
{
    return structures.at(type);
}

/*
 * get_entry_point_offset()
 * Returns the paragraph-aligned offset of the SMBIOS table entry point.
 * Returns -1 if entry point is not found.
 */
void SmBiosInfo::find_entry_point(EntryPointFinderInterface *finder)
{
    if(!finder)
    {
        throw std::invalid_argument("NULL EntryPointFinderInterface");
    }
    entry_point_offset = finder->get_addr();
    is_efi = finder->is_efi();
}

/*
 * load_eps()
 * Stores an SmBiosEntryPointStructure in parameter eps, read from parameter buf.
 * Note: parameter buf must point directly to the start of the entry point structure,
 *       and len must be >= sizeof(SmBiosEntryPointStructure)
 */
void SmBiosInfo::load_eps(SmBiosEntryPointStructure *eps, const char *const buf, size_t len)
{
    if(!eps)
    {
        throw std::invalid_argument("NULL SmBiosEntryPointStructure");
    }
    if(!buf || len == 0)
    {
        throw std::invalid_argument("NULL buffer/0-size buffer");
    }

    if(len < sizeof(SmBiosEntryPointStructure))
    {
        throw std::invalid_argument("buffer length must be >= sizeof(SmBiosEntryPointStructure)");
    }

    memcpy(eps, buf, sizeof(SmBiosEntryPointStructure));
}

/*
 * checksum()
 * Helper function to calculate checksum. Returns true if sum==0, false otherwise.
 */
bool SmBiosInfo::checksum(uint8_t *buf, size_t len)
{
    if(!buf || len == 0)
    {
        throw std::invalid_argument("NULL buffer/0-size buffer");
    }

    uint8_t sum = 0;
    for(size_t i = 0; i < len; ++i)
    {
        sum += buf[i];
    }

    return (sum == 0);
}

bool SmBiosInfo::checksum(const SmBiosEntryPointStructure &eps)
{
    //use the length specified by the entry point structure length field.
    return checksum((uint8_t*)&eps, eps.eps_length);
}

/*
 * inter_checksum()
 * Calculates the entry point structure intermediate checksum.
 * Return true if sum==0, false otherwise.
 */
bool SmBiosInfo::inter_checksum(const SmBiosEntryPointStructure &eps)
{
    const size_t length = 0x0f; //from DMTF SMBIOS spec
    return checksum((uint8_t*)&eps.inter_anchor, length);
}

/*
 * validate_eps()
 * This function throws an exception if the entry point structure
 * is invalid. The validity is determined by the structure's
 * checksum, intermediate checksum, and whether or not the
 * intermediate anchor string (_DMI_) is found.
 */
void SmBiosInfo::validate_eps(const SmBiosEntryPointStructure &eps)
{
    int found_dmi = strncmp("_DMI_", eps.inter_anchor, 5);
    if(!checksum(eps) || !inter_checksum(eps) || found_dmi != 0)
    {
        throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, "invalid SMBIOS entry point structure");
    }
}

/*
 * load_header()
 * Loads an SMBIOS struct header in parameter hdr from paramer buf, which
 * must be pointing directly at the beginning of the header.
 */
void SmBiosInfo::load_header(SmBiosStructHeader *hdr, const char *const buf, size_t len)
{
    if(!hdr)
    {
        throw std::invalid_argument("NULL SmBiosStructHeader");
    }

    if(!buf || len == 0)
    {
        throw std::invalid_argument("NULL buffer/0-size buffer");
    }

    if(len < sizeof(SmBiosStructHeader))
    {
        throw std::invalid_argument("buffer length must be >= sizeof(SmBiosStructHeader)");
    }

    memcpy(hdr, buf, sizeof(SmBiosStructHeader));
}

/*
 * get_next_struct_offset()
 * Returns the offset of the next struct header.
 * hdr : the _current_ SmBiosStructHeader. length field will be read in order to find
 *       next struct
 * buf : a pointer to a buffer containing SMBIOS structures. Note that the pointer
 *       must point directly to where parameter hdr resides.
 * len : the length of parameter buf.
 */
int64_t SmBiosInfo::get_next_struct_offset(const SmBiosStructHeader &hdr, const char *const buf, size_t len)
{
    if(len < hdr.length)
    {
        throw std::invalid_argument("buffer length must be >= hdr.length");
    }
    size_t offs = hdr.length; //offs starts at hdr.length
    const char *p = NULL;
    //Look for the structure terminator, \0\0
    //p starts pointing at the end of the formatted section
    for(p = (char*)buf + hdr.length ; offs < len -1; ++p, ++offs)
    {
        if(p[0] == '\0' && p[1] == '\0')
        {
            //Reached struct terminator
            return (int64_t)offs + 2;
        }
    }
    return -1;
}

/*
 * parse_and_add_structs()
 * This function traverses the SMBIOS structures table and adds supported structures to map
 * buf : a pointer to the Structure Table - offs 0x18 in EPS
 * len : the length of the SMBIOS Structure Table - offs 0x16 in EPS
 * total_structs : the number of structures found in the SMBIOS Structure Table
 */
void SmBiosInfo::parse_and_add_structs(const char *const buf, size_t len)
{
    if(!buf || len == 0)
    {
        throw std::invalid_argument("NULL buffer/0-size buffer");
    }
    char *p = (char*)buf;
    size_t offset = 0;
    int64_t next_offset = 0;
    while(offset <= len)
    {
        SmBiosStructHeader hdr = {0, 0, 0};
        load_header(&hdr, p, sizeof(SmBiosStructHeader));

        if(hdr.type == end_of_table)
        {
            break;
        }

        //allow get_next_struct_offset() to search for next struct in len-offset range
        next_offset = get_next_struct_offset(hdr, p, len - offset);
        if(next_offset == -1)
        {
            return;
        }
        add_struct(hdr, p, next_offset - 1);
        offset += next_offset;
        p += next_offset;
    }
}

/*
 * add_struct()
 * Adds a new structure to structures map, if supported.
 * buf : a buffer holding the SMBIOS structure, from the SMBIOS header to
 *       the end of the structure.
 * len : the size of the buffer.
 */
void SmBiosInfo::add_struct(SmBiosStructHeader &hdr, const char *const buf, size_t len)
{
    try
    {
        structures_vector &v = structures.at((SmBiosStructureType)hdr.type);
        SmBiosStructureBase::ptr structure = SmBiosStructureBase::create_structure((SmBiosStructureType)hdr.type);
        structure->validate_and_build(hdr, (uint8_t*)buf, len);
        v.push_back(structure);
    }
    catch(std::out_of_range &ofr)
    {
        //unsupported SMBIOS structure type, don't add anything
        return;
    }
    catch(std::runtime_error)
    {
        //unsupported SMBIOS structure type, don't add anything
        return;
    }
}

void SmBiosInfo::load_devmem_at_offset(int64_t offset, const size_t length)
{
    if(offset < 0)
        throw std::logic_error("offset must be > 0");

    if(smbios_buf)
    {
        //reload
        delete [] smbios_buf;
        smbios_buf = NULL;
    }

    smbios_buf = new char[length];
    File file;
    int fd;
    if((fd = file.open(devmem, O_RDONLY)) == -1)
    {
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "unable to open /dev/mem");
    }

    if(file.lseek(fd, offset, SEEK_SET) == -1)
    {
        file.close(fd);
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "lseek");
    }

    if(file.read(fd, smbios_buf, length) == -1)
    {
        file.close(fd);
        throw SystoolsdException(SYSTOOLSD_IO_ERROR, "error loading file");
    }
    file.close(fd);
}

