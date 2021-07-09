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
"""Device functions module"""

import os
import re
import sys
from errormb import MicBiosError


def MicDeviceInfo(device):
    """Factory method for specialized platform"""
    return impl(device)

class MicDevice(object):
    """Base class"""
    def __init__(self, mic_name):
        self.name = mic_name
        self.num = int(mic_name.replace("mic", ""))

    @classmethod
    def device_count(cls):
        return impl.device_count()

    def get_state(self):
        raise NotImplementedError()

    def is_card_online(self):
        """Check if card is in online state"""
        return self.get_state().lower() == "online"

class Linux(MicDevice):
    """Linux platform device methods"""
    SYSFS_BASE = "/sys/class/mic/"

    @classmethod
    def _check_driver_loaded(cls):
        """Check if sysfs exists"""
        if not os.path.exists(cls.SYSFS_BASE):
            raise MicBiosError("missing required drivers")

    @classmethod
    def device_count(cls):
        """Return the number of devices"""
        #expand ranges
        cls._check_driver_loaded()
        dirs = os.listdir(cls.SYSFS_BASE)
        mics = filter(lambda d: re.match("^mic\d{1,3}$", d), dirs)
        return len(mics)

    def _read_sysfs(self, path):
        """Return the content of the file in the specified path"""
        with open(os.path.join(self.SYSFS_BASE, self.name, path)) as sysfs:
            return sysfs.read().strip()

    def get_state(self):
        """Return the card state"""
        return self._read_sysfs("state")

class Windows(MicDevice):
    """Windows platform device methods"""
    WMI_OBJECT = r"winmgmts:\root\wmi"

    @classmethod
    def device_count(cls):
        """Return the number of devices"""
        import win32com.client
        return len(win32com.client.GetObject(cls.WMI_OBJECT).InstancesOf("MIC"))

    @classmethod
    def _query_wmi(cls,table, param, num):
        """Execute a WMI query"""
        import win32com.client
        query = "SELECT {} FROM {} WHERE node_id = {}".format(
                param, table, num)
        values = win32com.client.GetObject(cls.WMI_OBJECT).ExecQuery(query)
        return next(iter(values)).Properties_(param).Value

    def get_state(self):
        """Return the card state"""
        return self._query_wmi("MIC", "state", self.num)


if "linux" in sys.platform.lower():
    impl = Linux
elif "win" in sys.platform.lower():
    impl = Windows
else:
    raise MicBiosError("Unsupported platform")
