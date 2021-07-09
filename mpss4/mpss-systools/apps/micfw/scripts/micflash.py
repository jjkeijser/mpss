#!/usr/bin/env python2
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

import subprocess
import sys
import argparse

sys.tracebacklimit = 0
MICFW_GET_CMD = "micfw device-version"
MICFW_DEV_CMD = "micfw device-version -d"
MICFW_FILE = "micfw file-version"
PARSER = argparse.ArgumentParser()

def execute(command):
    """execute function executes the given command line.

    Arguments :
       command : type string, receives the command string

    """
    popen = subprocess.Popen(command, shell=True)
    popen.communicate()
    if popen.returncode == 2:
    #2 if is not recognized by argparse
        print PARSER.print_help()


def getversion(args):
    """getversion function will check which execute function should be executed.

    Arguments :
        args : type argparse.Namespace, used to access to the given arguments.
    """
    if not args.getversion and args.device:
        #device-version
        execute('{0} {1}'.format(MICFW_DEV_CMD, args.device))
    elif args.getversion != 'empty' and args.getversion:
        #fileversion
        execute('{0} \"{1}\"'.format(MICFW_FILE, args.getversion))
    elif not args.getversion:
        execute('{0}'.format(MICFW_GET_CMD))
    else:
        PARSER.print_help()


def update(args):
    """update function will check which execute function should be executed.

    Arguments :
        args : type argparse.Namespace, used to access to the given arguments.
    """
    if not args.update and args.device:
        #update only with device argument
        execute('micfw update {0}'.format(args.device))
    elif args.update != 'empty' and args.update:
        #update with file_path and/or device arguments
        if args.device:
            execute('micfw update {0} --file \"{1}\"'.format(args.device, args.update))
        else:
            execute('micfw update all --file \"{0}\"'.format(args.update))
    elif not args.update:
        execute('micfw update all')
    else:
        PARSER.print_help()


def verbose(args):
    """Executes commands with verbose option"""
    if not args.update and args.device:
        execute('micfw update {0} -v'.format(args.device))
    elif not args.getversion and args.device:
        execute('{0} {1} -v'.format(MICFW_DEV_CMD, args.device))
    elif args.update != "empty" and args.update:
        if args.device:
            execute('micfw update {0} --file {1} -v'.format(args.device,
                    args.update))
        else:
            execute('micfw update all --file {0} -v'.format(args.update))
    elif args.getversion != "empty" and args.getversion:
        execute('{0} {1} -v'.format(MICFW_FILE, args.getversion))
    elif not args.update:
        execute('micfw update all -v')
    elif not args.getversion:
        execute('{0} -v'.format(MICFW_GET_CMD))
    else:
        PARSER.print_help()


def validate_multiple_options(getveropt, updateopt, function, *args):
    """Checks if getversion and update options are not invoked at same time """
    if getveropt == "empty" or updateopt == "empty":
        function(*args)
    else:
        raise SystemExit("micflash: Multiple commands -getversion -update")


def main():
    """main function

    Variables :
        PARSER : type argparse.ArgumentParser, used to have acces to the
        argparse class and its methods.
        args : type argparse.Namespace, used to access to the given arguments.

    Checks the given arguments and executes the corresponding action.

    Raises an ArgumentTypeError if input has no arguments

    A Warning is printed everytime -log option is called saying that this option
    is deprecated

    """
    PARSER.add_argument("-getversion", nargs='?', action="store",
            default="empty", help="""Get version of flash image in the specified
            file, or from the specified device. If no device is provided
            'all' will be assumed""", metavar='File|Device')
    PARSER.add_argument("-update", nargs='?', action="store",
            default="empty", help="""Updates the flash on specified Intel(R)
            Xeon Phi(TM) Coprocessors using given image. If no device is provided
            'all' will be assumed""", metavar='File|Device')
    PARSER.add_argument("-device", "-d", help="""Subcommand of micflash used to
            specify the devices ids in the form micN where N is the
            device number""")
    PARSER.add_argument("-help", help="Usage: micflash", dest="helpmsg",
            action="store_true")
    PARSER.add_argument("-v", help="Show verbose status messages.",
            action="store_true")
    PARSER.add_argument("-log", help="This option is deprecated")
    PARSER.add_argument("-noreboot", help="This option is deprecated",
            action="store_true")
    PARSER.add_argument("-nodowngrade", help="This option is deprecated",
            action="store_true")
    PARSER.add_argument("-smcbootloader", help="This option is deprecated",
            action="store_true")
    args = PARSER.parse_args()
    if args.log:
        print "\nNote: -log option is deprecated\n"
    if args.noreboot:
        print "\nNote: -noreboot option is deprecated\n"
    if args.nodowngrade:
        print "\nNote: -nodowngrade option is deprecated\n"
    if args.smcbootloader:
        print "\nNote: -smcbootloader option is deprecated\n"
    if args.helpmsg:
        PARSER.print_help()
    elif args.v:
        validate_multiple_options(args.getversion, args.update, verbose, args)
    elif args.getversion != "empty":
        validate_multiple_options(args.getversion, args.update, getversion, args)
    elif args.update != "empty":
        validate_multiple_options(args.getversion, args.update, update, args)
    else:
        PARSER.print_help()
        raise argparse.ArgumentTypeError("micflash: No command specified")

if __name__ == "__main__":
    main()
