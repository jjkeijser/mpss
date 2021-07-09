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
#ifndef MICMGMT_SCIFFUNCTIONS_HPP_
#define MICMGMT_SCIFFUNCTIONS_HPP_

// SCIF INCLUDES
//
#include <scif.h>

namespace micmgmt
{

struct ScifFunctions
{
    typedef scif_epd_t (*scif_open) (void);
    typedef int        (*scif_close) (scif_epd_t epd);
    typedef int        (*scif_bind) (scif_epd_t epd, uint16_t pn);
    typedef int        (*scif_connect) (scif_epd_t epd, struct scif_portID *dst);
    typedef int        (*scif_send) (scif_epd_t epd, void *msg, int len, int flags);
    typedef int        (*scif_recv) (scif_epd_t epd, void *msg, int len, int flags);

    scif_open open;
    scif_close close;
    scif_bind bind;
    scif_connect connect;
    scif_send send;
    scif_recv recv;

    ScifFunctions() :
        open(0), close(0), bind(0),
        connect(0), send(0), recv(0)
    {
    }
};

} // namespace micmgmt
#endif // MICMGMT_SCIFFUNCTIONS_HPP_
