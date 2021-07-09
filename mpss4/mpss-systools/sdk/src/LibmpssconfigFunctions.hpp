/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
*/
#ifndef MICMGMT_LIBMPSSCONFIGFUNCTIONS_HPP_
#define MICMGMT_LIBMPSSCONFIGFUNCTIONS_HPP_

namespace micmgmt
{

struct LibmpssconfigFunctions
{
    typedef int   (*libmpss_boot_linux)( int node );
    typedef int   (*libmpss_boot_media)( int node, const char* path );
    typedef char*  (*libmpss_get_state)( int node );
    typedef int   (*libmpss_reset_node)( int node );

    libmpss_boot_linux     boot_linux;
    libmpss_boot_media     boot_media;
    libmpss_get_state      get_state;
    libmpss_reset_node     reset_node;

    LibmpssconfigFunctions() : boot_linux(0),
    boot_media(0), get_state(0), reset_node(0)
    {

    }

    LibmpssconfigFunctions(libmpss_boot_linux bootl, libmpss_boot_media bootm,
            libmpss_get_state gets, libmpss_reset_node res) :
        boot_linux(bootl), boot_media(bootm), get_state(gets), reset_node(res)
    {

    }

};

} // namespace micmgmt


#endif // MICMGMT_LIBMPSSCONFIGFUNCTIONS_HPP_
