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
import shlex
import re

from distutils.version import LooseVersion
from subprocess import Popen, PIPE

import win32com.client
import pywintypes

import _miccheck
from _miccheck.common import exceptions as ex
from _miccheck.common import printing as prnt
from _miccheck.common import tests as ctests
from _miccheck.common import MiccheckTest, OptionalTest
from _miccheck.common import knx, knc, knl
from _miccheck.common import target_host, target_device
from _miccheck.common import testtypes as ttypes
from _miccheck.common import FABS

from _miccheck.common import testattrs, filter_tests
from _miccheck.common.testtypes import get_test_name

OPTIONAL_TESTS = map(get_test_name, [ttypes.PING, ttypes.BIOS_VERSION,
                                     ttypes.SMC_VERSION, ttypes.ME_VERSION,
                                     ttypes.NTBEEPROM_VERSION])

knc_did_re = re.compile(r"VEN_8086&DEV_225[0-9A-F]", re.I)
knl_did_re = re.compile(r"VEN_8086&DEV_226[01235]", re.I)

def _get_pnpentities():
    return win32com.client.GetObject(r"winmgmts:\root\cimv2").InstancesOf("win32_pnpentity")


def _optionals_enabled(settings):
    return any([getattr(settings, opt, None) for opt in OPTIONAL_TESTS])


def _execute_program(command):
    command_list = shlex.split(command)
    pipe = Popen(command_list, shell=False, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    if pipe.returncode:
        raise ex.ExecProgramException("Failed to execute '{0}' : '{1}'".format(
                 command, err.rstrip(" \t\n\r")))

    return out.rstrip(" \t\n\r")


def _num_mics_pci():
    wmi = _get_pnpentities()
    knc_filter = lambda ent: knc_did_re.search(ent.Properties_("DeviceID").Value)
    knl_filter = lambda ent: knl_did_re.search(ent.Properties_("DeviceID").Value)
    kncs = filter(knc_filter, wmi)
    knls = filter(knl_filter, wmi)
    #having KNC and KNL devices in a single host system is unsupported
    if kncs and knls:
        raise ex.FailedTestException("unsupported configuration")

    all_mics = kncs or knls

    if knls:
        return knl, len(all_mics)
    elif kncs:
        return knc, len(all_mics)
    else:
        return "", 0


FAMILY, TOTAL_MICS = _num_mics_pci()


def _get_wmi_mic_instances():
    mics = win32com.client.GetObject(r"winmgmts:\root\wmi").InstancesOf("MIC")
    return {mic.node_id: mic for mic in mics}


def _num_mics_wmi():
    try:
        wmi = _get_wmi_mic_instances()
        return len(wmi)
    except pywintypes.com_error:
        return 0


def _wmi_get_driver_version():
    wmi = _get_wmi_mic_instances()
    return wmi[0].driver_version

# test pci device detection
@testattrs(ttypes.PCI_CARD_COUNT, target_host)
class PciDevicesTest(ctests.PciDevicesTestBase):
    def __init__(self):
        ctests.PciDevicesTestBase.__init__(self)

    @staticmethod
    def _run(**kwargs):
        if TOTAL_MICS < 1:
            raise ex.FailedTestException(ctests.PciDevicesTestBase.err_msg)


# compare pci num devices with wmi num devices
@testattrs(ttypes.DRIVER_CARD_COUNT, target_host)
class WmiDevicesTest(ctests.ScifDevicesTestBase):
    def __init__(self):
        ctests.ScifDevicesTestBase.__init__(self)

    @staticmethod
    def _run(**kwargs):
        try:
            num_dev_wmi = _num_mics_wmi()

            if num_dev_wmi != TOTAL_MICS:
                raise ex.FailedTestException(ctests.ScifDevicesTestBase.err_msg)
        except ValueError, excp:
            raise ex.FailedTestException('incorrect value of wmi devices: %s' %
                                         str(excp))


# test mic driver detection
@testattrs(ttypes.DRIVER_LOADED, target_host)
class MicDriverTest(MiccheckTest):
    @staticmethod
    def _run(**kwargs):
        num_devices = _num_mics_wmi()

        if num_devices < 1:
            raise ex.FailedTestException('required drivers are not loaded')

    @staticmethod
    def msg_executing():
        return "Check required drivers are loaded"

def ltrim_version(string, num_dots):
    return ".".join(string.split(".")[:num_dots])

# tests the driver version is correct
# if a is the current driver version and b is the miccheck version,
# then "a == b" is an acceptable scenario; "b < a" and "a < b"
# will be acceptable only if the first two sub-versions are equal;
# e.g. a=3.2.40, b=3.2.50 is acceptable, a=3.1.40, b=3.2.50
# is not.
@testattrs(ttypes.DRIVER_VERSION, target_host, order=0)
class DriverVersionTest(OptionalTest):
    @staticmethod
    def _run(**kwargs):
        build_version = _miccheck.__version__ # b
        wmi_version = _wmi_get_driver_version() # a
        err_msg = 'loaded driver version incorrect: \'{0}\', ' \
                  'reference is \'{1}\'.'.format(wmi_version, build_version)

        # if either is empty, LooseVersion() will fail
        if not build_version or not wmi_version:
            raise ex.FailedTestException(err_msg)

        if LooseVersion(build_version) != LooseVersion(wmi_version):
            trim_build = ltrim_version(build_version, 2)
            trim_wmi = ltrim_version(wmi_version, 2)

	    if LooseVersion(trim_build) != LooseVersion(trim_wmi):
                raise ex.FailedTestException(err_msg)

        prnt.p_out_debug('    loaded driver version \'{0}\','
                         ' reference is \'{1}\'.'
                         .format(wmi_version, build_version))

    @staticmethod
    def msg_executing():
        return "Check loaded driver version is correct"


# test device in online mode and postcode FF
@testattrs(ttypes.STATE_POST, target_device, knx)
class StateTest(MiccheckTest):
    def _run(self, **kwargs): # not static, because it is a device test
        dev_num = kwargs["device"].dev
        mics = _get_wmi_mic_instances()
        mic = mics[dev_num]

        if mic.state != "ONLINE":
            raise ex.FailedTestException('device is not online: '
                                         '{0}'.format(mic.state))

        if mic.post_code != 0xFF:
            raise ex.FailedTestException('POST code: '
                                         '{0}'.format(mic.post_code))

    @staticmethod
    def msg_executing():
        return "Check device state and POST code"


# test device has RAS daemon available
@testattrs(ttypes.RAS, target_device, knc)
class RasTest(ctests.RasTest):
    pass


@testattrs(ttypes.SYSTOOLSD, target_device, knx)
class SystoolsdTest(ctests.SystoolsdTest):
    pass


@testattrs(ttypes.BIOS_VERSION, target_device, knx, order=0)
class BiosVersionTest(OptionalTest):
    def _run(self, **kwargs):
        dev_num = kwargs["device"].dev
        mic = _get_wmi_mic_instances()[dev_num]
        expected_version = _miccheck.__bios_version__
        running_version = mic.bios_version

        if running_version != expected_version:
            raise ex.FailedTestException("device BIOS version does not match, "
                                         "should be '{0}', it is '{1}'".format(
                                             expected_version, running_version))

    @staticmethod
    def msg_executing():
        return "Check BIOS version is correct"


@testattrs(ttypes.SMC_VERSION, target_device, knx, order=1)
class SmcVersionTest(OptionalTest):
    def _run(self, **kwargs):
        dev_num = kwargs["device"].dev
        mic = _get_wmi_mic_instances()[dev_num]
        fab_version = FABS[mic.fab_version]
        expected_version = _miccheck.__smc_version__[fab_version]
        running_version = mic.bios_smc_version

        if running_version != expected_version:
            raise ex.FailedTestException("device SMC version does not match, "
                                         "should be '{0}', it is '{1}'".format(
                                             expected_version, running_version))
    @staticmethod
    def msg_executing():
        return "Check SMC firmware version is correct"

@testattrs(ttypes.ME_VERSION, target_device, knx, order=2)
class MEVersionTest(OptionalTest):
    def _run(self, **kwargs):
        dev_num = kwargs["device"].dev
        mic = _get_wmi_mic_instances()[dev_num]
        expected_version = _miccheck.__me_version__
        running_version = mic.bios_me_version

        if running_version != expected_version:
            raise ex.FailedTestException("device ME version does not match, "
                                         "should be '{0}', it is '{1}'".format(
                                             expected_version, running_version))

    @staticmethod
    def msg_executing():
        return "Check ME firmware version is correct"

@testattrs(ttypes.NTBEEPROM_VERSION, target_device, knx, order=3)
class NTBEEPROMVersionTest(OptionalTest):
    def _run(self, **kwargs):
        dev_num = kwargs["device"].dev
        mic = _get_wmi_mic_instances()[dev_num]
        fab_version = FABS[mic.fab_version]
        expected_version = _miccheck.__ntbeeprom_version__[fab_version]
        running_version = mic.bios_eeprom_version

        if running_version != expected_version:
            raise ex.FailedTestException("device NTBEEPROM version does not match, "
                                         "should be '{0}', it is '{1}'".format(
                                             expected_version, running_version))

    @staticmethod
    def msg_executing():
        return "Check NTB EEPROM firmware version is correct"
@testattrs(ttypes.PING, target_device, knx, order=4)
class PingTest(ctests.PingTestBase):
    def _run(self, **kwargs):
        dev = kwargs["device"].dev
        ipaddr = "172.31.{0}.1".format(dev + 1)
        pingcmd = "ping -n 1 -w 3 {0}".format(ipaddr)
        try:
            output = _execute_program(pingcmd)
        except OSError:
            raise ex.ExecProgramException("ping could not be found in the system")
        except ex.ExecProgramException as excp:
            raise ex.FailedTestException(self.msg_error(ipaddr))


def get_default_host_tests():
    return [PciDevicesTest(), MicDriverTest(),
            WmiDevicesTest()]


def get_optional_host_tests(settings):
    return sorted([t() for t in filter_tests(target_host, settings, globals())],
                  key=OptionalTest.get_order)


def get_default_device_tests(ndevices):
    classes = ()
    if FAMILY == knc:
        classes = StateTest, RasTest
    elif FAMILY == knl:
        classes = StateTest, SystoolsdTest

    return [[t() for t in classes] for i in range(ndevices)]


def get_optional_device_tests(settings):
    if not _optionals_enabled(settings):
        return []

    classes = filter_tests(target_device, settings, globals())
    return [sorted([t() for t in classes], key=OptionalTest.get_order)
            for i in range(len(settings.device))]


def get_all_tests(settings):
    return (get_default_host_tests(),
            get_optional_host_tests(settings),
            get_default_device_tests(len(settings.device)),
            get_optional_device_tests(settings))
