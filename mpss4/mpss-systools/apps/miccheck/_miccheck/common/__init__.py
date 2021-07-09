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
import re

from base import MiccheckTest, OptionalTest, DeviceInfo, filter_tests, testattrs

target_host = "host"
target_device = "device"
knx = "knx"
knc = "knc"
knl = "knl"

FABS = {'0': "A", '1': "B"}

