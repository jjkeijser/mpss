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
"""Device utils module"""

import re

from itertools import ifilterfalse

from errormb import MicBiosError
from devicemb import MicDevice

def get_device_count():
    """Get the number of cards"""
    return MicDevice.device_count()

def _expand_device_ranges(device_ranges):
    # Assumes device_ranges are correctly formed
    all_devs = set()
    for rang in device_ranges:
        start, end = rang.split("-")
        start = int(start.replace("mic", ""))
        end = int(end.replace("mic", ""))

        if start > end:
            start, end = end, start

        all_devs = all_devs.union(set(range(start, end+1)))

    return ["mic{0}".format(d) for d in all_devs]

def expand_devices(devs_str):
    """Receives a string of comma-separated device
    list/range form, returns a sorted list of devices."""
    dev_list = _expand_devices(devs_str)
    return dev_list

def _expand_devices(devs_str):
    # e.g. "mic0,mic1-mic3"

    if devs_str == "all":
        return ["mic{0}".format(d) for d in range(get_device_count())]

    ranges_and_devs = devs_str.split(",")
    is_range = lambda s: "-" in s
    ranges = filter(is_range, ranges_and_devs)
    devs = list(ifilterfalse(is_range, ranges_and_devs))
    matcher = re.compile("^mic\d{1,3}$")
    is_dev_invalid = lambda x: matcher.match(x) is None

    # check individual devs
    invalid_devs = filter(is_dev_invalid, devs)
    if invalid_devs:
        raise MicBiosError("invalid devices: {0}".format(invalid_devs))

    # now check ranges
    matcher = re.compile("^mic\d{1,3}-mic\d{1,3}$")
    is_range_invalid = lambda x: matcher.match(x) is None
    invalid_ranges = filter(is_range_invalid, ranges)
    if invalid_ranges:
        raise MicBiosError("invalid devices ranges: {0}".format(invalid_ranges))

    all_devs = _expand_device_ranges(ranges)
    all_devs += devs
    all_devs = list(set(all_devs))
    all_devs = sorted(all_devs, key=lambda x: int(x.replace("mic", "")))
    return all_devs
