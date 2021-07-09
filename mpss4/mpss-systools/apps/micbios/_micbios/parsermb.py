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
"""Parser module"""

from constantsmb import Subcommandsmb
import sys
import argparse
import version

_version = ("%(prog)s version {0}".format(version.__version__))

def micbios_parser():
    """Parse the arguments given and works with a subparser so each BIOS setting
    and options of micbios can have its own help"""

    desc = ("Utility to read and write BIOS configurations of the Intel(R) Xeon "
            "Phi(TM) x200 Coprocessors")

    #password parser
    password_parser = argparse.ArgumentParser(add_help=False)
    password_parser.add_argument("--password", help="BIOS password. Defaults to "
                               "an empty string if not specified.", dest="password",
                               default="")
    #principal parser
    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument("--version", action="version", version=_version)
    #subparser
    subparser = parser.add_subparsers(dest="subcommands")
    #setpass
    setpass_parser=subparser.add_parser("set-pass", help="Change the BIOS password")
    setpass_parser.add_argument("old_pass", help="Old BIOS password")
    setpass_parser.add_argument("new_pass", help="New BIOS password should be "
            "a printable ASCII string containing, lower case letters, upper case letters, "
            "digits or any of the following characters: ! @ # $ %% ^ & * ( ) - _ + = ? "
            "with a maximum length of 14.")
    setpass_parser.add_argument("device_list",help="Comma-separated list of "
                                "devices/range of devices, e.g. mic0,mic1-mic3,mic5")
    #amazon-feature
    setfwlock_parser=subparser.add_parser("set-fwlock", help="Prevents firmware "
                                          "updates, regardless of the source or "
                                          "user privileges.", parents=[password_parser])
    setfwlock_parser.add_argument("mode", help="Mode Options",
                                  choices=["enable", "disable"])
    setfwlock_parser.add_argument("device_list",help="Comma-separated list of "
                                  "devices/range of devices, e.g. mic0,mic1-mic3,mic5")
    #cluster
    cluster_parser = subparser.add_parser("set-cluster-mode", help="Change "
                                          "cluster mode", parents=[password_parser])
    cluster_parser.add_argument("mode", help="Mode Options", choices=["all2all",
                                "snc2", "snc4", "hemisphere", "quadrant", "auto"])
    cluster_parser.add_argument("device_list",help="Comma-separated list of "
                                "devices/range of devices, e.g. mic0,mic1-mic3,mic5")
    #ecc
    ecc_parser = subparser.add_parser("set-ecc", help="Change ECC Support",
                                      parents=[password_parser])
    ecc_parser.add_argument("mode", help="Mode Options", choices=["enable",
                            "disable", "auto"])
    ecc_parser.add_argument("device_list",help="Comma-separated list of "
                            "devices/range of devices, e.g. mic0,mic1-mic3,mic5")
    #apei
    apei_parser = subparser.add_parser("set-apei-supp", help="Change APEI "
                                       "Support biossetting. This setting must "
                                       "be enabled to show and configure "
                                       "others APEI settings such as: APEI FFM "
                                       "Logging, APEI PCIe Error Injection,"
                                       " APEI PCIe EInj Action Table",
                                       parents=[password_parser])
    apei_parser.add_argument("mode", help="Mode Options", choices=["enable",
                             "disable"])
    apei_parser.add_argument("device_list",help="Comma-separated list of "
                             "devices/range of devices, e.g. mic0,mic1-mic3,mic5")
    #apeiffm
    apeiffm_parser = subparser.add_parser("set-apei-ffm", help = "Change APEI "
                                          "FFM Logging setting",
                                          parents=[password_parser])
    apeiffm_parser.add_argument("mode", help="Mode Options", choices=["enable",
                                "disable"])
    apeiffm_parser.add_argument("device_list",help="Comma-separated list of "
                                "devices/range of devices, e.g. mic0,mic1-mic3,mic5")
    #apeitable
    apeitable_parser = subparser.add_parser("set-apei-einjtable", help ="Change "
                                            "APEI PCIe EInj Action Table setting",
                                            parents=[password_parser])
    apeitable_parser.add_argument("mode", help="Mode Options", choices=["enable",
                                  "disable"])
    apeitable_parser.add_argument("device_list",help="Comma-separated list of "
                                  "devices/range of devices, e.g. mic0,mic1-mic3,mic5")
    #apeieinj
    apeieinj_parser = subparser.add_parser("set-apei-einj", help = "Change "
                                           "APEI PCIe Error Injection setting",
                                           parents=[password_parser])
    apeieinj_parser.add_argument("mode", help="Mode Options", choices=["enable",
                                 "disable"])
    apeieinj_parser.add_argument("device_list",help="Comma-separated list of "
                                 "devices/range of devices, e.g. mic0,mic1-mic3,mic5")
    #getinfo
    getinfo_parser=subparser.add_parser("getinfo", help="Display BIOS settings.")
    # --device meant for "read" operations
    getinfo_parser.add_argument("--device", "-d", help="Comma-separated list of "
                                "devices/range of devices, e.g. mic0,mic1-mic3,mic5",
                                dest="device_list", default="all")
    getinfo_parser.add_argument("biossetting", help="Settings to display",
                                choices=["cluster", "ecc", "apei-supp", "apei-ffm",
                                    "apei-einj", "apei-table", "fwlock"])
    return parser

def validate_getinfo(args):
        pass

def validate_setpass(args):
        pass

def validate_setcluster(args):
        pass

def validate_setecc(args):
        pass

def validate_setapeisupp(args):
        pass

def validate_setapeiffm(args):
        pass

def validate_setapeieinj(args):
        pass

def validate_setapeitable(args):
        pass

def validate_setfwlock(args):
        pass

validators={
        Subcommandsmb.setpass : validate_setpass,
        Subcommandsmb.cluster : validate_setcluster,
        Subcommandsmb.ecc : validate_setecc,
        Subcommandsmb.apeisupp : validate_setapeisupp,
        Subcommandsmb.apeieinj : validate_setapeieinj,
        Subcommandsmb.apeitable : validate_setapeitable,
        Subcommandsmb.apeiffm : validate_setapeiffm,
        Subcommandsmb.fwlock : validate_setfwlock,
        Subcommandsmb.getinfo : validate_getinfo
        }

def validate_args(args):
    """Validate the args given with the constants module"""
    validators[args.subcommands](args)
