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

#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

const char *SystoolsdException::error_type() const
{
    switch(error)
    {
        case SYSTOOLSD_UNSUPPORTED_REQ:
            return "SYSTOOLSD_UNSUPPORTED_REQ";
        case SYSTOOLSD_INVAL_STRUCT:
            return "SYSTOOLSD_INVAL_STRUCT";
        case SYSTOOLSD_INVAL_ARGUMENT:
            return "SYSTOOLSD_INVAL_ARGUMENT";
        case SYSTOOLSD_TOO_BUSY:
            return "SYSTOOLSD_TOO_BUSY";
        case SYSTOOLSD_INSUFFICIENT_PRIVILEGES:
            return "SYSTOOLSD_INSUFFICIENT_PRIVILEGES";
        case SYSTOOLSD_DEVICE_BUSY:
            return "SYSTOOLSD_DEVICE_BUSY";
        case SYSTOOLSD_RESTART_IN_PROGRESS:
            return "SYSTOOLSD_RESTART_IN_PROGRESS";
        case SYSTOOLSD_SMC_ERROR:
            return "SYSTOOLSD_SMC_ERROR";
        case SYSTOOLSD_IO_ERROR:
            return "SYSTOOLSD_IO_ERROR";
        case SYSTOOLSD_INTERNAL_ERROR :
            return "SYSTOOLSD_INTERNAL_ERROR";
        case SYSTOOLSD_SCIF_ERROR:
            return "SYSTOOLSD_SCIF_ERROR";
        default: //SYSTOOLSD_UNKOWN_ERROR:
            return "SYSTOOLSD_UNKOWN_ERROR";

    }
}

uint16_t SystoolsdException::error_nr() const
{
    return static_cast<uint16_t>(error);
}

