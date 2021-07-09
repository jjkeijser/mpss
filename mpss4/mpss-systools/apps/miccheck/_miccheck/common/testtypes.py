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
PCI_CARD_COUNT = 0
DRIVER_CARD_COUNT = PCI_CARD_COUNT + 1
DRIVER_LOADED = DRIVER_CARD_COUNT + 1
MPSS_DAEMON = DRIVER_LOADED + 1
DRIVER_VERSION = MPSS_DAEMON + 1
STATE_POST = DRIVER_VERSION + 1
RAS = STATE_POST + 1
BIOS_VERSION = RAS + 1
SMC_VERSION = BIOS_VERSION + 1
ME_VERSION = SMC_VERSION + 1
NTBEEPROM_VERSION = ME_VERSION + 1
PING = NTBEEPROM_VERSION + 1
SSH = PING + 1
COI = SSH + 1
SYSTOOLSD = COI + 1

TEST_NAMES = {
        PCI_CARD_COUNT : "pci_card_count",
        DRIVER_CARD_COUNT : "driver_card_count",
        DRIVER_LOADED : "driver_loaded",
        MPSS_DAEMON : "mpss_daemon",
        DRIVER_VERSION : "driver_version",
        STATE_POST : "state_post",
        RAS : "ras",
        SYSTOOLSD : "systoolsd",
        BIOS_VERSION : "bios_version",
        SMC_VERSION : "smc_version",
        ME_VERSION : "me_version",
        NTBEEPROM_VERSION : "ntbeeprom_version",
        PING : "ping",
        SSH : "ssh",
        COI : "coi"
}

NOT_RUN = "not_run"
PASS = "pass"
FAIL = "fail"

def get_test_name(ttype):
    return TEST_NAMES[ttype]
