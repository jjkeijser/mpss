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

import _micbios.appmb as appmb
import _micbios.parsermb as parsermb

from _micbios.commandsmb import MicBiosCommands
from _micbios.errormb import MicBiosError

def main():
    aparser = parsermb.micbios_parser()
    args = aparser.parse_args()
    try:
        parsermb.validate_args(args)
        commands = MicBiosCommands.factory(args)
        app = appmb.Micbios(commands)
        return app.run()
    except (MicBiosError, OSError, ImportError) as e:
        sys.stderr.write(str(e) + "\n" * 2)
        return 1

if  __name__ == "__main__":
    sys.exit(main())
