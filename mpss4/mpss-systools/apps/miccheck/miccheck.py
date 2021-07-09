#!/usr/bin/env python
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

import sys
sys.dont_write_bytecode = True

try:
    from _miccheck.common import main
except ImportError as error:
    print error
    print('Could not import _miccheck, please make sure it is installed'
        ' in a reachable location and dependencies are installed.')
    sys.exit(1)

if __name__ == "__main__":
    STATUS = main.main()
    sys.exit(STATUS)
