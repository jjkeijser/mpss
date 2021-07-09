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

#ifndef MICSDKERRNO_H
#define MICSDKERRNO_H

/* Error Codes for MIC-SDK */

#ifdef  __cplusplus
#include    <cstdint>
#else
#include    <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/************************/
/*  micsdk error codes  */
/************************/

typedef enum
{
    MICSDKERR_SUCCESS = 0,              /* 0x00: No error - Success */
    MICSDKERR_INVALID_ARG,              /* 0x01: Invalid argument */
    MICSDKERR_NO_MPSS_STACK,            /* 0x02: No MPSS stack found*/
    MICSDKERR_DRIVER_NOT_LOADED,        /* 0x03: Driver not loaded */
    MICSDKERR_DRIVER_NOT_INITIALIZED,   /* 0x04: Driver not initialized */
    MICSDKERR_SYSTEM_NOT_INITIALIZED,   /* 0x05: System not initialized */
    MICSDKERR_NO_DEVICES,               /* 0x06: No devices available */
    MICSDKERR_UNKNOWN_DEVICE_TYPE,      /* 0x07: Unknown device type */
    MICSDKERR_DEVICE_OPEN_FAILED,       /* 0x08: Failed to open device */
    MICSDKERR_INVALID_DEVICE_NUMBER,    /* 0x09: Invalid device number */
    MICSDKERR_DEVICE_NOT_OPEN,          /* 0x0a: Device not open */
    MICSDKERR_DEVICE_NOT_ONLINE,        /* 0x0b: Device not online */
    MICSDKERR_DEVICE_IO_ERROR,          /* 0x0c: Device I/O error */
    MICSDKERR_PROPERTY_NOT_FOUND,       /* 0x0d: Property not found */
    MICSDKERR_INTERNAL_ERROR,           /* 0x0e: Internal error */
    MICSDKERR_NOT_SUPPORTED,            /* 0x0f: Operation not supported */
    MICSDKERR_NO_ACCESS,                /* 0x10: No access rights */
    MICSDKERR_FILE_IO_ERROR,            /* 0x11: File I/O error */
    MICSDKERR_DEVICE_BUSY,              /* 0x12: Device busy */
    MICSDKERR_NO_SUCH_DEVICE,           /* 0x13: No such device */
    MICSDKERR_DEVICE_NOT_READY,         /* 0x14: Device not ready */
    MICSDKERR_INVALID_BOOT_IMAGE,       /* 0x15: Invalid boot image */
    MICSDKERR_INVALID_FLASH_IMAGE,      /* 0x16: Invalid flash image */
    MICSDKERR_SHARED_LIBRARY_ERROR,     /* 0x17: Shared library error */
    MICSDKERR_BUFFER_TOO_SMALL,         /* 0x18: Buffer too small */
    MICSDKERR_INVALID_SMC_REG_OFFSET,   /* 0x19: Invalid SMC register offset */
    MICSDKERR_SMC_OP_NOT_PERMITTED,     /* 0x1a: SMC operation not permitted */
    MICSDKERR_TIMEOUT,                  /* 0x1b: Operation timed out */
    MICSDKERR_INVALID_CONFIGURATION,    /* 0x1c: Invalid configuration detected */
    MICSDKERR_NO_MEMORY,                /* 0x1d: Out of Memory */
    MICSDKERR_DEVICE_ALREADY_OPEN,      /* 0x1e: Device already open */
    MICSDKERR_VERSION_MISMATCH          /* 0x1f: Version mismatch */
} micsdkerrno_t;


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MICSDKERRNO_H */
