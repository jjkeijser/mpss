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
import os
import re
import shlex
import subprocess

from os.path import join, isdir, exists

import _miccheck
from _miccheck.common import exceptions as ex
from _miccheck.common import tests as ctests
from _miccheck.common import testtypes as ttypes
from _miccheck.common import target_host, target_device
from _miccheck.common import knx, knl
from _miccheck.common import filter_tests
from _miccheck.common import MiccheckTest, OptionalTest
from _miccheck.common import testattrs
from _miccheck.common import FABS

from _miccheck.common.testtypes import get_test_name


#device ID for the x200 family (KNL)
# 0x2264 reserved for DMA channels
knl_did_re = re.compile("^0x226[01235]$", re.I)

SYSFS_VERSION = '/sys/class/mic/ctrl/version'

#available Linux optional tests
OPTIONAL_TESTS = map(get_test_name, [ttypes.PING, ttypes.SSH, ttypes.COI,
                    ttypes.BIOS_VERSION, ttypes.SMC_VERSION])

PCI_DEVS_PATH = "/sys/bus/pci/devices"

def open_file(*args):
    return open(*args)

def read_file(path):
    data = None
    with open(path) as attrib:
        data = attrib.read()
    return data.rstrip(' \t\n\r')

def _num_mics_pci():
    """Return a tuple (<family>, <coprocessors_present>)"""
    base = PCI_DEVS_PATH
    all_devs = filter(isdir, [join(base, d) for d in os.listdir(base)])
    #get a list of tuples (<vendor>, <device>)
    all_devs = [(read_file(join(d, "vendor")), read_file(join(d, "device")))
                for d in all_devs if exists(join(d, "vendor"))
                                 and exists(join(d, "device"))]
    #d[0] -> vendor
    #d[1] -> device
    all_mics = [d[1] for d in all_devs
                if d[0] == "0x8086" and knl_did_re.match(d[1])]

    if not all_mics:
        return "", 0

    return (knl, len(all_mics))

def _num_mics_driver():
    return len([True for entry in os.listdir("/sys/class/mic")
                if re.match("mic\d+", entry)])

def _get_modules():
    """Helper function to get output of /proc/modules"""
    with open_file("/proc/modules", "r") as f:
        out = f.read().split("\n")

    return [line.split()[0] for line in out if line]

try:
    FAMILY, TOTAL_MICS = _num_mics_pci()
except:
    FAMILY, TOTAL_MICS = "unknown", 0


def doptionals_enabled(settings):
    """Return True if any optional test is enabled; False otherwise."""
    return any([getattr(settings, dopt, None) for dopt in OPTIONAL_TESTS])


def get_proc_attrs(path, attr_list=["Name", "Pid", "PPid"]):
    """Opens file 'status' located in path
    (which must be a /proc/<pid> directory)
    and returns a dict with the process'.

    This function will allow IOError exceptions
    to propagate (caller must handle them).
    """
    #Lines in status file are in the form:
    #Name:  progname
    #State: S (sleeping)
    #...

    #allow IOError to propagate
    f = open(join(path, "status"))
    stripper = lambda (x, y) : (x.strip(), y.strip())
    try:
        return dict((k, v) for k, v in #3: create the dict
               map(stripper,           #2: strip() elements
               [line.split(":")[:2] for line in f]) #1: create list of tuples from "status" file
               if k in attr_list) #4: ... if key is in attr_list
    except:
        raise
    finally:
        f.close()


def sysfs_device_read_attr(dev_num, attr):
    return read_file('/sys/class/mic/mic%d/%s' % (dev_num, attr))


def execute_program(command):
    command_list = shlex.split(command)
    popen = subprocess.Popen(command_list, shell=False, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)

    outputs = popen.communicate()

    if popen.returncode != 0:
        raise ex.ExecProgramException('Failed to execute \'%s\': \'%s\'' %
                                   (command, outputs[1].rstrip(' \t\n\r')))
    return outputs[0].rstrip(' \t\n\r')


def is_micdriver_loaded():
    return "mic" in _get_modules()


def is_mpssd_running():
    base = "/proc"
    all_procs = filter(isdir, [join(base, d) for d in os.listdir(base)])
    all_procs = [d for d in all_procs if exists(join(d, "status"))]
    for proc in all_procs:
        try:
            #get process' name and parent process ID
            procinfo = get_proc_attrs(proc, ["Name", "PPid"])
            name, ppid = procinfo["Name"], procinfo["PPid"]
            #avoid false positives: process' name must be "mpssd"
            # and its parent must be PID 1 (init)
            if name == "mpssd" and ppid == "1":
                return True
        except IOError:
            continue
    else:
        return False

def _get_card_fab(dev_num):
    return FABS[sysfs_device_read_attr(dev_num, "info/fab_version")]

# test pci device detection
@testattrs(ttypes.PCI_CARD_COUNT, target_host)
class PciDevicesTest(ctests.PciDevicesTestBase):
    def __init__(self):
        ctests.PciDevicesTestBase.__init__(self)

    @staticmethod
    def _run(**kwargs):
        if TOTAL_MICS < 1:
            raise ex.FailedTestException(ctests.PciDevicesTestBase.err_msg)


# test mic driver number of devices
@testattrs(ttypes.DRIVER_CARD_COUNT, target_host)
class ScifDevicesTest(ctests.ScifDevicesTestBase):
    def __init__(self):
        ctests.ScifDevicesTestBase.__init__(self)

    @staticmethod
    def _run(**kwargs):
        try:
            num_dev_pci = TOTAL_MICS
            num_dev_scif = _num_mics_driver()

            if num_dev_scif != num_dev_pci:
                raise ex.FailedTestException(ctests.ScifDevicesTestBase.err_msg)
        except ValueError as excp:
            raise ex.FailedTestException('incorrect value of SCIF nodes: %s' %
                                         str(excp))

# test KNL drivers are loaded
@testattrs(ttypes.DRIVER_LOADED, target_host)
class KnlDriversTest(MiccheckTest):
    @staticmethod
    def _run(**kwargs):
        all_mods = set(_get_modules())
        expected_mods = set(("mic_x200_dma scif_bus vop_bus cosm_bus "
                             "scif vop mic_cosm mic_x200").split())
        missing = list(expected_mods - all_mods)
        if missing:
            missing_list = ", ".join(missing)
            msg = "missing following drivers: {0}".format(missing_list)
            raise ex.FailedTestException(msg)

    @staticmethod
    def msg_executing():
        return "Check required drivers are loaded"

@testattrs(ttypes.DRIVER_LOADED, target_host)
class MicDriverTest(MiccheckTest):
    def __new__(cls):
        return KnlDriversTest()

# test mpssd daemon running
@testattrs(ttypes.MPSS_DAEMON, target_host)
class MpssRunTest(MiccheckTest):
    @staticmethod
    def _run(**kwargs):
        if not is_mpssd_running():
            raise ex.FailedTestException('mpssd daemon not running')

    @staticmethod
    def msg_executing():
        return "Check mpssd daemon is running"

# test KNL device is in online mode
@testattrs(ttypes.STATE_POST, target_device, knl)
class KnlStateTest(MiccheckTest):
    def _run(self, **kwargs):
        dev_num = kwargs["device"].dev
        state = sysfs_device_read_attr(dev_num, "state")
        postcode = sysfs_device_read_attr(dev_num, "spad/post_code")

        if state.lower() != "online":
            raise ex.FailedTestException("device is not online: {0}".format(state))

        if postcode.lower() != "0xff":
            raise ex.FailedTestException("POST code: {0}".format(postcode))

    @staticmethod
    def msg_executing():
        return "Check device state and POST code"

class DeviceStateTest(MiccheckTest):
    def __new__(cls):
        return KnlStateTest()

@testattrs(ttypes.SYSTOOLSD, target_device, knl)
class SystoolsdTest(ctests.SystoolsdTest):
    pass


class DaemonTest(MiccheckTest):
    def __new__(cls):
        return SystoolsdTest()

# check the bios version of the device
@testattrs(ttypes.BIOS_VERSION, target_device, knx, order=0)
class BiosVersionTest(OptionalTest):
    def _run(self, **kwargs): # not static because it is a device test
        dev_num = kwargs["device"].dev
        expected_version = _miccheck.__bios_version__
        try:
            running_version = sysfs_device_read_attr(dev_num, "info/bios_version")
        except IOError:
            raise ex.FailedTestException("could not retrieve BIOS firmware version")

        if expected_version != running_version:
            raise ex.FailedTestException("device BIOS version does not match, "
                                         "should be '{0}', it is '{1}'".format(
                                             expected_version, running_version))

    @staticmethod
    def msg_executing():
        return "Check BIOS version is correct"


@testattrs(ttypes.SMC_VERSION, target_device, knx, 1)
class SmcVersionTest(OptionalTest):
    def _run(self, **kwargs):
        dev_num = kwargs["device"].dev
        expected_versions = _miccheck.__smc_version__
        try:
            card_fab = _get_card_fab(dev_num)
        except IOError:
            raise ex.FailedTestException("could not retrieve fab version")
        try:
            running_version = sysfs_device_read_attr(dev_num, "info/bios_smc_version")
        except IOError:
            raise ex.FailedTestException("could not retrieve SMC firmware version")

        #perform equality check for now
        if expected_versions[card_fab] != running_version:
            raise ex.FailedTestException("device SMC version does not match, "
                                         "should be '{0}', it is '{1}'".format(
                                             expected_versions[card_fab], running_version))

    @staticmethod
    def msg_executing():
        return "Check SMC firmware version is correct"

@testattrs(ttypes.ME_VERSION, target_device, knx, 2)
class MEVersionTest(OptionalTest):
    def _run(self, **kwargs):
        dev_num = kwargs["device"].dev
        expected_version = _miccheck.__me_version__
        try:
            running_version = sysfs_device_read_attr(dev_num, "info/bios_me_version")
        except IOError:
            raise ex.FailedTestException("could not retrieve ME firmware version")

        #perform equality check for now
        if expected_version != running_version:
            raise ex.FailedTestException("device ME version does not match, "
                                         "should be '{0}', it is '{1}'".format(
                                             expected_version, running_version))

    @staticmethod
    def msg_executing():
        return "Check ME firmware version is correct"

@testattrs(ttypes.NTBEEPROM_VERSION, target_device, knx, 3)
class NTBEEPROMVersionTest(OptionalTest):
    def _run(self, **kwargs):
        dev_num = kwargs["device"].dev
        expected_versions = _miccheck.__ntbeeprom_version__
        try:
            card_fab = _get_card_fab(dev_num)
        except IOError:
            raise ex.FailedTestException("could not retrieve fab version")
        try:
            running_version = sysfs_device_read_attr(dev_num, "info/ntb_eeprom_version")
        except IOError:
            raise ex.FailedTestException("could not retrieve NTB EEPROM firmware version")

        #perform equality check for now
        if expected_versions[card_fab] != running_version:
            raise ex.FailedTestException("device NTB EEPROM version does not match, "
                                         "should be '{0}', it is '{1}'".format(
                                             expected_versions[card_fab], running_version))

    @staticmethod
    def msg_executing():
        return "Check NTB EEPROM firmware version is correct"



# test device can be pinged
@testattrs(ttypes.PING, target_device, knx, order=4)
class PingTest(ctests.PingTestBase):
    def _run(self, **kwargs): # not static because it is a device test
        hostname = kwargs["device"].hostname
        try:
            timeout = 3
            output = execute_program('/bin/ping -c1 -w%d %s' % (timeout, hostname))
        except OSError:
            raise ex.ExecProgramException('/bin/ping could not be found in '
                                          'the system')
        except ex.ExecProgramException, excp:
            raise ex.FailedTestException(self.msg_error(hostname))


# test device can be accessed through ssh
@testattrs(ttypes.SSH, target_device, knx, order=5)
class SshTest(OptionalTest):
    def _run(self, **kwargs): # not static because it is a device test
        dev_name = kwargs["device"].hostname
        try:
            timeout = 3
            exec_line = ('/usr/bin/ssh -oConnectTimeout=%d -oBatchMode=yes '
                        '-oStrictHostKeyChecking=no %s echo hello' %
                        (timeout, dev_name))
            output = execute_program(exec_line)
        except OSError:
            raise ex.ExecProgramException('/usr/bin/ssh could not be found in '
                                          'the system')
        except ex.ExecProgramException, excp:
            raise ex.FailedTestException('hostname %s could not be accessed '
                                         'through ssh' % dev_name)

    @staticmethod
    def msg_executing():
        return "Check device can be accessed through ssh"


@testattrs(ttypes.COI, target_device, "knx", 6)
class CoiTest(OptionalTest):
    def _run(self, **kwargs):
        device = kwargs["device"]
        dev_num = device.dev
        hostname = device.hostname
        timeout = 3
        ssh_line = ("/usr/bin/ssh -oConnectTimeout={0} -oBatchMode=yes "
                    "-oStrictHostKeyChecking=no {1} '{2}'")
        #Look in stat procfiles for coi_daemon with parent 1,
        #and discard states:
        # Z:zombie,T:stopped,t:tracing stop, X:dead, x:dead
        find_coi = ("grep -o \"(coi_daemon) . 1\" /proc/*/stat |"
                    "grep -v \"\s[ZTtXx]\s\"")
        coi_command = ssh_line.format(timeout, hostname, find_coi)

        #first, test that ssh is available and works
        SshTest().run(device=device)

        #and now test if COI is running
        try:
            execute_program(coi_command)
        except ex.ExecProgramException as excp:
            raise ex.FailedTestException("COI daemon is not running in {0}".format(hostname))

    @staticmethod
    def msg_executing():
        return "Check COI daemon is available in device"


def get_default_host_tests():
    return [PciDevicesTest(), MicDriverTest(),
            ScifDevicesTest(), MpssRunTest()]


def get_optional_host_tests(settings):
    return sorted([t() for t in filter_tests(target_host, settings, globals())],
                  key=OptionalTest.get_order)


def get_default_device_tests(ndevices):
    #return a list of lists (so that each device
    # gets a separate instance
    classes = DeviceStateTest, DaemonTest
    return [[t() for t in classes] for i in range(ndevices)]


def get_optional_device_tests(settings):
    if not doptionals_enabled(settings):
        return []

    classes = filter_tests(target_device, settings, globals())
    return [sorted([t() for t in classes], key=OptionalTest.get_order)
            for i in range(len(settings.device))]


def get_all_tests(settings):
    return (get_default_host_tests(),
            get_optional_host_tests(settings),
            get_default_device_tests(len(settings.device)),
            get_optional_device_tests(settings))

