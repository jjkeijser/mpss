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
import inspect

import testtypes as ttypes

class DeviceInfo(object):
    def __init__(self, devnr, hostname=""):
        self.dev = devnr
        self.name = "mic{0}".format(devnr)
        self.hostname = self.name if not hostname else hostname
    def __eq__(self, other):
        if isinstance(other, DeviceInfo):
            return other.name == self.name and other.hostname == self.hostname and other.dev == self.dev
        return NotImplemented


class MiccheckTest(object):
    def __init__(self):
        self._status = ttypes.NOT_RUN
        self._errmsg = ""

    def run(self, **kwargs):
        try:
            self._run(**kwargs)
            self._status = ttypes.PASS
        except Exception as excp:
            self._status = ttypes.FAIL
            self._errmsg = "{0}".format(str(excp))
            raise

    def get_status(self):
        return self._status

    def get_errmsg(self):
        return self._errmsg


class OptionalTest(MiccheckTest):
    def get_order(self):
        return self._order


class testattrs(object):
    #decorator class to add properties to classes
    def __init__(self, ttype, target, knx="", order=-1):
        self.name = ttypes.get_test_name(ttype)
        self.target = target
        self.knx = knx
        self._order = order

    def __call__(self, cls):
        setattr(cls, "name", self.name)
        setattr(cls, "target", self.target)
        setattr(cls, "knx", self.knx)
        setattr(cls, "_order", self._order)
        return cls


def is_enabled(test_name, settings):
    return bool(getattr(settings, test_name, ""))


def filter_tests(target, settings, from_dict):
    return [t for t in from_dict.values() if inspect.isclass(t)
            and issubclass(t, MiccheckTest)
            and getattr(t, "target", "") == target
            and is_enabled(t.name, settings)]


