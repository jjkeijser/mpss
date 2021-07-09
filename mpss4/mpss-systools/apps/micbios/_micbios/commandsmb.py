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
"""Application commands module"""

import utils

from constantsmb import Subcommandsmb

# Base class
class MicBiosCommands(object):
    def __init__(self, args):
        """Represents base settings"""
        self.action = args.subcommands
        self.device_list = utils.expand_devices(args.device_list)

    def setsetting(self, args):
        """Common settings used on set-actions"""
        MicBiosCommands.__init__(self, args)
        self.mode = args.mode
        self.password = args.password

    @staticmethod
    def factory(args):
        """Factory method for specialized subcommand settings"""
        classes = MicBiosCommands.__subclasses__()
        class_names = map(lambda x: x.subcommand, classes)
        classes_dict = dict(zip(class_names, classes))
        return classes_dict[args.subcommands](args)

class SetPass(MicBiosCommands):
    subcommand = Subcommandsmb.setpass
    """Represents settings for the 'set-pass' subcommand"""
    def __init__(self, args):
        MicBiosCommands.__init__(self, args)
        self.old_pass = args.old_pass
        self.new_pass = args.new_pass

class SetCluster(MicBiosCommands):
    subcommand = Subcommandsmb.cluster
    """Represents settings for the 'set-cluster' subcommand"""
    def __init__(self, args):
        MicBiosCommands.setsetting(self, args)

class SetEcc(MicBiosCommands):
    subcommand = Subcommandsmb.ecc
    """Represents settings for the 'set-ecc' subcommand"""
    def __init__(self, args):
        MicBiosCommands.setsetting(self, args)

class SetApeiSupp(MicBiosCommands):
    subcommand = Subcommandsmb.apeisupp
    """Represents settings for the 'set-apei-supp' subcommand"""
    def __init__(self, args):
        MicBiosCommands.setsetting(self, args)

class SetApeiFfm(MicBiosCommands):
    subcommand = Subcommandsmb.apeiffm
    """Represents settings for the 'set-apei-ffm' subcommand"""
    def __init__(self, args):
        MicBiosCommands.setsetting(self, args)

class SetApeiEinj(MicBiosCommands):
    subcommand = Subcommandsmb.apeieinj
    """Represents settings for the 'set-apei-einj' subcommand"""
    def __init__(self, args):
        MicBiosCommands.setsetting(self, args)

class SetApeiTable(MicBiosCommands):
    subcommand = Subcommandsmb.apeitable
    """Represents settings for the 'set-apei-table' subcommand"""
    def __init__(self, args):
        MicBiosCommands.setsetting(self, args)

class SetFwlock(MicBiosCommands):
    subcommand = Subcommandsmb.fwlock
    """Represents settings for the 'set-fwlock' subcommand"""
    def __init__(self, args):
        MicBiosCommands.setsetting(self, args)

class Getinfo(MicBiosCommands):
    subcommand = Subcommandsmb.getinfo
    """Represents settings for the 'getinfo' subcommand"""
    def __init__(self, args):
        MicBiosCommands.__init__(self, args)
        self.biossetting = args.biossetting
