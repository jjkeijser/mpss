# Copyright (c) 2017, Intel Corporation.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
# 
# This program is distributed in the hope it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details.
from constantsmb import Enum
"""Constants Module"""

SyscfgCommands = Enum(read = 0,
                 write = 1,
                 password = 2)

Shifts = Enum(cluster_shift=0,
            ecc_shift=4,
            apei_supp_shift=7,
            apei_einj_shift=9,
            apei_ffm_shift=11,
            apei_table_shift=13,
            fwlock_shift=15)

Properties = Enum(cluster=0x01,
                  ecc=0x02,
                  apeisupp=0x04,
                  apeiffm=0x08,
                  apeieinj=0x10,
                  apeitable=0x20,
                  fwlock=0x40)

def systoolsd_error(err):
    mapper = {
            0 : "SYSTOOLSD_SUCCESS",
            1 : "SYSTOOLSD_UNKNOWN_ERROR",
            2 : "SYSTOOLSD_UNSUPPORTED_REQ",
            3 : "SYSTOOLSD_INVAL_STRUCT",
            4 : "Invalid password.", #SYSTOOLSD_INVAL_ARGUMENT
            5 : "SYSTOOLSD_TOO_BUSY",
            6 : "SYSTOOLSD_INSUFFICIENT_PRIVILEGES",
            7 : "SYSTOOLSD_DEVICE_BUSY",
            8 : "SYSTOOLSD_RESTART_IN_PROGRESS",
            9 : "SYSTOOLSD_SMC_ERROR",
            10: "SYSTOOLSD_IO_ERROR",
            11: "This setting could not be modified, make sure apei-supp is enabled.",#SYSTOOLSD_INTERNAL_ERROR
            12: "SYSTOOLSD_SCIF_ERROR"
        }
    return mapper.get(err, "SYSTOOLSD_UNKNOWN_ERROR")

def cluster_config_name(mode):#TODO bump up the values so we can use the 0 for unknown options.
    mapper = {0 : "All2All",
              1 : "SNC-2",
              2 : "SNC-4",
              3 : "Hemisphere",
              4 : "Quadrant",
              5 : "Auto"}
    return mapper[mode]

def ecc_config_name(mode):
    mapper = {0 : "Disable",
              1 : "Enable",
              2 : "Auto"}
    return mapper[mode]

def apei_supp_config_name(mode):
    mapper = {0 : "Disable",
              1 : "Enable"}
    return mapper[mode]

def apei_einj_config_name(mode):
    mapper = {0 : "Disable",
              1 : "Enable"}
    return mapper[mode]

def apei_ffm_config_name(mode):
    mapper = {0 : "Disable",
              1 : "Enable"}
    return mapper[mode]

def apei_table_config_name(mode):
    mapper = {0 : "Disable",
              1 : "Enable"}
    return mapper[mode]

def fwlock_config_name(mode):
    mapper = {0 : "Disable",
              1 : "Enable"}
    return mapper[mode]
