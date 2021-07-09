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
import optparse as op
import os
import platform
import re
import sys
import textwrap

from _miccheck.common import testrunner
if "linux" in sys.platform.lower():
    from _miccheck.linux import tests as pltfm
elif sys.platform.lower() in ("cygwin", "win32"):
    from _miccheck.windows import tests as pltfm
from _miccheck.common import printing as prnt
from _miccheck.common import testtypes as ttypes
from _miccheck.common import DeviceInfo
from _miccheck.common.testtypes import get_test_name
import _miccheck


class MiccheckOptionParser(op.OptionParser):
    def error(self, msg):
        self.print_help(sys.stdout)
        self.exit(2, "\n%s: error: %s\n" % (self.get_prog_name(), msg))


def parse_command_line(argv):
    desc = textwrap.dedent("""\
        Utility which performs software sanity checks on a host machine with
        Intel(R) Xeon Phi(TM) coprocessors installed, by running a suite of
        diagnostic tests. By default, a subset of all available tests are run;
        additional tests can be enabled individually. The default behavior is
        to run all enabled tests applicable to the host system first, and then
        those applicable to the Intel(R) Xeon Phi(TM) coprocessors in turn.""")

    parser = MiccheckOptionParser(formatter=op.TitledHelpFormatter(width=79),
                                  add_help_option=None, description=desc)
    # options
    options_group = op.OptionGroup(parser, 'General')
    options_group.add_option("--version", action="store_true", dest="version",
                             default=False, help="Display miccheck tool version")
    options_group.add_option('-h', '--help', action='help',
                             help='Show this help message.')
    options_group.add_option('-v', '--verbose', dest="verbose",
                             action='store_true', default=False,
                             help='Enables verbosity.')
    options_group.add_option('-d', '--device', action="store",
                             dest='device', default='all',
                             help='Select which devices to test. Example: '
                                  '--device=mic0;--device=mic0,mic2;--device=mic0-mic3')
    # tests
    tests_group = op.OptionGroup(parser, 'Optional tests available')
    tests_group.add_option('-f', '--firmware', dest="firmware",
                           action='store_true', default=False,
                           help='Check whether firmware versions of device are correct.')

    tests_group.add_option('-p', '--ping', dest=get_test_name(ttypes.PING),
                           action='store_true', default=False,
                           help='Check whether network interface of device can '
                                'be pinged.')

    if platform.system() == "Linux":
        tests_group.add_option('', '--ssh', dest=get_test_name(ttypes.SSH),
                               action='store_true', default=False,
                               help='Check whether network interface of device can '
                                    'be accessed through ssh.')
        tests_group.add_option("-c", "--coi", dest=get_test_name(ttypes.COI),
                               action="store_true", default=False,
                               help="Check whether COI daemon is running in device.")
        options_group.add_option('--devicehost', action='store', dest='hostnames',
                                 default='',
                                 help='Specify custom hostnames for coprocessors in the '
                                      'form: micM:hostname1,micN:hostname2,...')

    parser.add_option_group(options_group)
    parser.add_option_group(tests_group)

    settings, args = parser.parse_args(argv)

    #custom hostnames not supported on Windows. Set default value.
    if platform.system() == "Windows":
        setattr(settings, "hostnames", "")

    # validate parsed args
    if args:
        parser.error('options not supported: "%s"' % (args,))

    if settings.verbose:
        prnt.set_debug()

    return settings, parser


def select_devices(devices):
    # depending on platform, get the number of devices detected over pci
    num_devices = pltfm.TOTAL_MICS

    if devices == 'all':
        prnt.p_out_debug("Discovered devices = {0}".format(str(devices)))
        return map(DeviceInfo, list(range(num_devices)))

    str_devs = devices.split(",")
    #check mutually exclusive "all" argument
    if "all" in str_devs:
        raise ValueError("cannot specify 'all' in device list/range")

    #validate all items are in the form micX or micM-micN
    mic_re = re.compile("^(mic\d+|mic\d+-mic\d+)$")
    is_invalid = lambda d: not bool(mic_re.match(d))
    invalid_args = filter(is_invalid, str_devs)
    if invalid_args:
        raise ValueError("invalid list/range: {0}".format(str(invalid_args)))

    #expand ranges
    is_range = lambda x: "-" in x
    expanded_list = []
    found_out_of_range = False
    for dev in str_devs:
        if is_range(dev):
            start, end = dev.split("-")
            start, end = start.replace("mic", ""), end.replace("mic", "")
            start, end = int(start), int(end)
            if end > num_devices-1 or start > num_devices-1:
                found_out_of_range = True
                break
            if start > end:
                raise ValueError("invalid range: {0}".format(str(dev)))
            expanded_list = expanded_list + list(range(start, end+1))
        else:
            devnr = int(dev.replace("mic", ""))
            if devnr > num_devices-1:
                found_out_of_range = True
                break
            expanded_list.append(int(dev.replace("mic", "")))

    if found_out_of_range:
        raise ValueError("requested devices cannot be greater than "
                        "available devices")


    #remove dups and order the list
    return [DeviceInfo(d) for d in set(expanded_list)]

def set_hostnames(settings):
    #we expect settings.hostnames to be in the form
    # micM:hn1,micN:hn2,...
    if not settings.hostnames:
        return
    mappings = settings.hostnames.split(",")

    #from RFC1034, "the total number of octets that represent a
    # domain name [...] is limited to 255"
    valid_re = re.compile("^mic\d+:[A-Za-z0-9\-\._]{1,255}$")
    is_invalid = lambda x : not valid_re.match(x)
    invalid_mappings = filter(is_invalid, mappings)
    if invalid_mappings:
        raise ValueError("invalid hostname mapping: {0}".format(
            str(invalid_mappings)))

    #create dictionary with the mappings
    mappings = [m.split(":") for m in mappings]
    hostnames = dict()
    total_devices = pltfm.TOTAL_MICS

    for micname, hostname in mappings:
        if micname in hostnames:
            raise ValueError("duplicate hostname for {0}".format(micname))
        if int(micname.replace("mic", "")) > total_devices - 1:
            raise ValueError("cannot specify hostname for "
                             "inexistent device {0}".format(micname))

        hostnames[micname] = hostname

    for device in settings.device:
        device.hostname = hostnames.get(device.name, device.hostname)


def enable_firmware_tests(settings):
    setattr(settings, get_test_name(ttypes.BIOS_VERSION), True)
    setattr(settings, get_test_name(ttypes.SMC_VERSION), True)
    setattr(settings, get_test_name(ttypes.ME_VERSION), True)
    setattr(settings, get_test_name(ttypes.NTBEEPROM_VERSION), True)

def main():
    try:
        setup_completed = False
        settings, args = parse_command_line(sys.argv[1:])  # parse command line

        if settings.version:
            banner = 'miccheck version {0}\nCopyright (C) 2017, Intel Corporation.\n'
            prnt.p_out(banner.format(_miccheck.__version__))
            return 0

        if settings.firmware:
            enable_firmware_tests(settings)

        settings.device = select_devices(settings.device)
        set_hostnames(settings)
        all_tests = pltfm.get_all_tests(settings)
        test_runner = testrunner.TestRunner(*all_tests)

        setup_completed = True
        test_runner.run(settings.device)

        print
        prnt.p_out('Status: OK')
        status = 0
    except Exception, excp:
        print
        prnt.p_out('Status: FAIL')
        prnt.p_err('Failure: ' + str(excp))
        status = 1

    return status
