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
import contextlib
import itertools
import os
import pytest
import random
import shutil
import sys
import tempfile

from pytest import raises

pytestmark = pytest.mark.skipif(sys.platform.lower() in ("win32", "cygwin"),
                                reason="For linux only")

import _miccheck
from _miccheck.common import exceptions as ex
from _miccheck.linux import tests

from utils import (generic_tag, devices, FakePassTest,
                   FakeFailTest, tut)

from _miccheck.common import knx, knc, knl
from _miccheck.common import target_host, target_device
from _miccheck.common.testtypes import PASS, FAIL, NOT_RUN
import _miccheck.common.testtypes as ttypes

import utils


def create_device(path, devnr, did, vendor):
    d = os.path.join(path, str(devnr))
    os.mkdir(d)
    with open(os.path.join(d, "device"), "w") as f:
        f.write(did)

    with open(os.path.join(d, "vendor"), "w") as f:
        f.write(vendor)

@pytest.fixture
def fake_sysfs_reader(request):
    """Fixture to replace tests.sysfs_device_read_attr.

    Required kwargs:
    sysfs
    """
    sysfs = getattr(request.function, "sysfs")
    def f(_, attr):
        return sysfs

    return f

@pytest.fixture
def pci_devices(request):
    #create mock pci devices as requested
    devices_path = tempfile.mkdtemp()
    ndevs = getattr(request.function, "ndevs", 2)
    devtype = getattr(request.function, "devtype", "knl")
    did = "0x2261"

    #create the requested devices
    for dev in range(ndevs):
        create_device(devices_path, dev, did, "0x8086")

    #create more devices
    for dev in range(ndevs+1, ndevs+10):
        sampl = "0123456789abcdef"
        vid = "0x" + "".join(random.sample(sampl, 4))
        ddid = "0x" + "".join(random.sample(sampl, 4))
        #avoid randomly creating KNX devices
        if ddid == did:
            continue
        create_device(devices_path, dev, did, vid)


    def fin():
        shutil.rmtree(devices_path)
    request.addfinalizer(fin)
    return devices_path


def test_read_file_success():
    s = "something\n\n"
    temp = tempfile.NamedTemporaryFile()
    try:
        temp.write(s)
        temp.seek(0)
        assert tests.read_file(temp.name) == s.strip()
    finally:
        temp.close()


def test_read_file_fail():
    with pytest.raises(IOError):
        tests.read_file("/thisbetternotexist")

@pytest.mark.parametrize("ndevs,dirs",
    (
        (1, ("mic0", "somedir")),
        (2, ("mic0", "mic1", "somedir")),
        (3, ("mic0", "mic1", "mic2", "somedir"))
    )
)
def test_num_mics_driver(monkeypatch, ndevs, dirs):
    monkeypatch.setattr(os, "listdir", lambda *args: dirs)
    assert tests._num_mics_driver() == ndevs

def test_get_modules(monkeypatch):
    class FakeFile(object):
        def read(self):
            return "mod1 stuff\nmod2 stuff\nmod3 stuff"
    @contextlib.contextmanager
    def fake_open_file(*args):
        yield FakeFile()
    monkeypatch.setattr(tests, "open_file", fake_open_file)
    assert tests._get_modules() == "mod1 mod2 mod3".split()

@generic_tag(devtype="knl", ndevs=4)
def test_num_mics_pci_knl_success(pci_devices):
    tests.PCI_DEVS_PATH = pci_devices
    fam, ncards = tests._num_mics_pci()
    assert fam == "knl"
    assert ncards == 4

@generic_tag(ndevs=0)
def test_num_mics_pci_no_devs(pci_devices):
    tests.PCI_DEVS_PATH = pci_devices
    fam, ncards = tests._num_mics_pci()
    assert not fam
    assert not ncards

@generic_tag(test=tests.PciDevicesTest, status=PASS)
def test_PciDevicesTest_success(tut):
    tests.TOTAL_MICS = 2
    tut.run()

@generic_tag(test=tests.PciDevicesTest, status=FAIL)
def test_PciDevicesTest_fail(tut):
    tests.TOTAL_MICS = 0
    with raises(ex.FailedTestException):
        tut.run()

@generic_tag(test=tests.ScifDevicesTest, status=PASS)
def test_ScifDevicesTest_success(tut, monkeypatch):
    tests.TOTAL_MICS = 3
    monkeypatch.setattr(tests, "_num_mics_driver", lambda: 3)
    tut.run()

@generic_tag(test=tests.ScifDevicesTest, status=FAIL)
def test_ScifDevicesTest_fail(tut, monkeypatch):
    tests.TOTAL_MICS = 1
    monkeypatch.setattr(tests, "_num_mics_driver", lambda: 3)
    with raises(ex.FailedTestException):
        tut.run()

@generic_tag(test=tests.MpssRunTest, status=PASS)
def test_MpssRunTest_success(tut, monkeypatch):
    monkeypatch.setattr(tests, "is_mpssd_running", lambda: True)
    tut.run()

@generic_tag(test=tests.MpssRunTest, status=FAIL)
def test_MpssRunTest_fail(tut, monkeypatch):
    monkeypatch.setattr(tests, "is_mpssd_running", lambda: False)
    with raises(ex.FailedTestException):
        tut.run()

@generic_tag(test=tests.BiosVersionTest, status=PASS, sysfs="1")
def test_BiosVersionTest_success(tut, devices, monkeypatch, fake_sysfs_reader):
    monkeypatch.setattr(_miccheck, "__bios_version__", "1")
    monkeypatch.setattr(tests, "sysfs_device_read_attr", fake_sysfs_reader)
    tut.run(device=devices[0])

@generic_tag(test=tests.BiosVersionTest, status=FAIL, sysfs="0")
def test_BiosVersionTest_fail(tut, devices, monkeypatch, fake_sysfs_reader):
    monkeypatch.setattr(_miccheck, "__bios_version__", "1")
    monkeypatch.setattr(tests, "sysfs_device_read_attr", fake_sysfs_reader)
    with raises(ex.FailedTestException):
        tut.run(device=devices[0])

@generic_tag(test=tests.BiosVersionTest, status=FAIL, sysfs="0")
def test_BiosVersionTest_fail_ioerror(tut, devices, monkeypatch):
    monkeypatch.setattr(_miccheck, "__bios_version__", "1")
    monkeypatch.setattr(tests, "sysfs_device_read_attr",
            lambda *args: utils.raiser(IOError()))
    with raises(ex.FailedTestException):
        tut.run(device=devices[0])

@generic_tag(test=tests.SmcFirmwareTest, status=PASS, sysfs="1.2.3")
def test_SmcFirmwareTest_success(tut, devices, monkeypatch, fake_sysfs_reader):
    monkeypatch.setattr(_miccheck, "__smc_fw_version__", "1.2.3")
    monkeypatch.setattr(tests, "sysfs_device_read_attr", fake_sysfs_reader)
    tut.run(device=devices[0])

@generic_tag(test=tests.SmcFirmwareTest, status=FAIL)
def test_SmcFirmwareTest_fail_ioerror(tut, devices, monkeypatch):
    monkeypatch.setattr(tests, "sysfs_device_read_attr",
            lambda *args: utils.raiser(IOError()))
    with raises(ex.FailedTestException):
        tut.run(device=devices[0])

@generic_tag(test=tests.SmcFirmwareTest, status=FAIL, sysfs="1.2.3")
def test_SmcFirmwareTest_fail_version(tut, devices, monkeypatch, fake_sysfs_reader):
    monkeypatch.setattr(_miccheck, "__smc_fw_version__", "3.2.1")
    monkeypatch.setattr(tests, "sysfs_device_read_attr", fake_sysfs_reader)
    with raises(ex.FailedTestException):
        tut.run(device=devices[0])

@generic_tag(test=tests.PingTest, status=PASS)
def test_PingTest_success(tut, devices, monkeypatch):
    monkeypatch.setattr(tests, "execute_program", lambda x: "")
    tut.run(device=devices[0])

@generic_tag(test=tests.PingTest, status=FAIL)
def test_PingTest_failbynoping(tut, devices, monkeypatch):
    monkeypatch.setattr(tests, "execute_program", lambda x: utils.raiser(OSError()))
    with raises(ex.ExecProgramException) as err:
        tut.run(device=devices[0])
    assert "ping could not be found" in str(err)

@generic_tag(test=tests.PingTest, status=FAIL)
def test_PingTest_failbynotreached(tut, devices, monkeypatch):
    monkeypatch.setattr(tests, "execute_program",
            lambda x: utils.raiser(ex.ExecProgramException))
    with raises(ex.FailedTestException) as err:
        tut.run(device=devices[0])
    assert "did not respond to ping" in str(err)

@generic_tag(test=tests.SshTest, status=PASS)
def test_SshTest_success(tut, devices, monkeypatch):
    monkeypatch.setattr(tests, "execute_program", lambda x: "")
    tut.run(device=devices[0])

@generic_tag(test=tests.SshTest, status=FAIL)
def test_SshTest_failbynossh(tut, devices, monkeypatch):
    monkeypatch.setattr(tests, "execute_program", lambda x: utils.raiser(OSError()))
    with raises(ex.ExecProgramException) as err:
        tut.run(device=devices[0])
    assert "ssh could not be found" in str(err)

@generic_tag(test=tests.SshTest, status=FAIL)
def test_SshTest_failbynotreached(tut, devices, monkeypatch):
    monkeypatch.setattr(tests, "execute_program",
            lambda x: utils.raiser(ex.ExecProgramException))
    with raises(ex.FailedTestException) as err:
        tut.run(device=devices[0])
    assert "could not be accessed through ssh" in str(err)

@generic_tag(test=tests.CoiTest, status=PASS)
def test_CoiTest_success(tut, devices, monkeypatch):
    monkeypatch.setattr(tests, "SshTest", FakePassTest)
    monkeypatch.setattr(tests, "execute_program", lambda x: "")
    tut.run(device=devices[0])

@generic_tag(test=tests.CoiTest, status=FAIL)
def test_CoiTest_fail(tut, devices, monkeypatch):
    # We only need the command that checks for COI to fail, not the rest, so:
    class Raise(object):
        def __call__(self, command):
            if "coi" in command:
                raise ex.ExecProgramException()
            pass
    monkeypatch.setattr(tests, "execute_program", Raise())
    with raises(ex.FailedTestException):
        tut.run(device=devices[0])

@generic_tag(test=tests.KnlDriversTest, status=PASS)
def test_KnlDriversTest_success(tut, devices, monkeypatch):
    expected_mods = set(("mic_x200_dma scif_bus vop_bus cosm_bus "
                         "scif vop mic_cosm mic_x200").split())
    monkeypatch.setattr(tests, "_get_modules", lambda: expected_mods)
    tut.run()

@generic_tag(test=tests.KnlDriversTest, status=FAIL)
def test_KnlDriversTest_fail(tut, monkeypatch):
    some_mods = ["fake_module"]
    monkeypatch.setattr(tests, "_get_modules", lambda: some_mods)
    with raises(ex.FailedTestException):
        tut.run()

@generic_tag(test=tests.KnlStateTest, status=PASS)
def test_KnlStateTest_success(tut, devices, monkeypatch):
    class FakeSysfs(object):
        def __call__(self, _, attr):
            if attr == "state":
                return "online"
            if "post_code" in attr:
                return "0xff"
    monkeypatch.setattr(tests, "sysfs_device_read_attr", FakeSysfs())
    tut.run(device=devices[0])

@generic_tag(test=tests.KnlStateTest, status=FAIL)
def test_KnlStateTest_fail_state(tut, devices, monkeypatch):
    monkeypatch.setattr(tests, "sysfs_device_read_attr", lambda *args: "ready")
    with raises(ex.FailedTestException):
        tut.run(device=devices[0])

@generic_tag(test=tests.KnlStateTest, status=FAIL)
def test_KnlStateTest_fail_post(tut, devices, monkeypatch):
    class FakeSysfs(object):
        def __call__(self, _, attr):
            if attr == "state":
                return "online"
            if "post_code" in attr:
                return "0xad" # not 0xff, fail
    monkeypatch.setattr(tests, "sysfs_device_read_attr", FakeSysfs())
    with raises(ex.FailedTestException):
        tut.run(device=devices[0])

def test_defaulthosttests():
    default_tests = tests.get_default_host_tests()
    #check returned tests have a 'target' attr and that it is target_host,
    # they are instances of MiccheckTest and they are not instances
    # of OptionalTest
    #check returned objects are MiccheckTest but not OptionalTest
    is_default = lambda t: getattr(t, "target") == target_host \
                 and isinstance(t, tests.MiccheckTest) \
                 and not isinstance(t, tests.OptionalTest)
    assert all(map(is_default, default_tests))

@pytest.mark.parametrize("optest", tests.OPTIONAL_TESTS)
def test_optionalhosttests(optest):
    #get available optional tests
    settings = utils.Settings()
    setattr(settings, optest, True)
    optional_tests = tests.get_optional_host_tests(settings)
    is_optional = lambda t: getattr(t, "target") == target_host \
                  and isinstance(t, tests.OptionalTest)
    assert all(map(is_optional, optional_tests))

@pytest.mark.parametrize("devtype", knl)
def test_defaultdevicetests(monkeypatch, devtype):
    monkeypatch.setattr(tests, "FAMILY", devtype)
    ndevices = 4
    default_tests = tests.get_default_device_tests(ndevices)
    #default_tests should be a list of lists of tests
    assert len(default_tests) == ndevices
    #flatten the list
    default_tests = [t for t in itertools.chain(*default_tests)]
    is_default = lambda t: getattr(t, "target") == target_device \
                 and isinstance(t, tests.MiccheckTest) \
                 and not isinstance(t, tests.OptionalTest)
    assert all(map(is_default, default_tests))

@pytest.mark.parametrize("optest", tests.OPTIONAL_TESTS)
def test_optionaldevicetests(optest):
    #assert empty list when Settings object has no optional
    # tests enabled
    assert not tests.get_optional_device_tests(utils.Settings())
    #get available optional tests
    ndevices = 4
    settings = utils.Settings()
    setattr(settings, optest, True)
    setattr(settings, "device", [True] * ndevices)
    optional_tests = tests.get_optional_device_tests(settings)
    assert len(optional_tests) == ndevices
    optional_tests = itertools.chain(*optional_tests)
    is_optional = lambda t: getattr(t, "target") == target_device \
                  and isinstance(t, tests.OptionalTest)
    assert all(map(is_optional, optional_tests))

def test_getalltests():
    settings = utils.Settings()
    ndevices = 4
    setattr(settings, "device", [True] * ndevices)
    assert tests.get_all_tests(settings)

modules_no_mic = """
i2c_algo_bit 5935 2 igb,nouveau, Live 0x0000000000000000
i2c_core 31084 7 i2c_dev,igb,i2c_i801,nouveau,drm_kms_helper,drm,i2c_algo_bit, Live 0x0000000000000000
mxm_wmi 1967 1 nouveau, Live 0x0000000000000000
video 20674 1 nouveau, Live 0x0000000000000000
output 2409 1 video, Live 0x0000000000000000
wmi 6287 2 nouveau,mxm_wmi, Live 0x0000000000000000
dm_mirror 14384 0 - Live 0x0000000000000000
dm_region_hash 12085 1 dm_mirror, Live 0x0000000000000000
dm_log 9930 2 dm_mirror,dm_region_hash, Live 0x0000000000000000
dm_mod 84209 2 dm_mirror,dm_log, Live 0x0000000000000000
"""

modules_like_mic = """
i2c_algo_bit 5935 2 igb,nouveau, Live 0x0000000000000000
i2c_core 31084 7 i2c_dev,igb,i2c_i801,nouveau,drm_kms_helper,drm,i2c_algo_bit, Live 0x0000000000000000
mxm_wmi 1967 1 nouveau, Live 0x0000000000000000
video 20674 1 nouveau, Live 0x0000000000000000
output 2409 1 video, Live 0x0000000000000000
micmod 2409 1 video, Live 0x0000000000000000
wmi 6287 2 nouveau,mxm_wmi, Live 0x0000000000000000
dm_mirror 14384 0 - Live 0x0000000000000000
dm_region_hash 12085 1 dm_mirror, Live 0x0000000000000000
dm_log 9930 2 dm_mirror,dm_region_hash, Live 0x0000000000000000
dm_mod 84209 2 dm_mirror,dm_log, Live 0x0000000000000000
"""

modules_with_mic = """
i2c_algo_bit 5935 2 igb,nouveau, Live 0x0000000000000000
i2c_core 31084 7 i2c_dev,igb,i2c_i801,nouveau,drm_kms_helper,drm,i2c_algo_bit, Live 0x0000000000000000
mxm_wmi 1967 1 nouveau, Live 0x0000000000000000
video 20674 1 nouveau, Live 0x0000000000000000
output 2409 1 video, Live 0x0000000000000000
wmi 6287 2 nouveau,mxm_wmi, Live 0x0000000000000000
dm_mirror 14384 0 - Live 0x0000000000000000
mic 593169 1 ib_qib, Live 0x0000000000000000
dm_region_hash 12085 1 dm_mirror, Live 0x0000000000000000
dm_log 9930 2 dm_mirror,dm_region_hash, Live 0x0000000000000000
dm_mod 84209 2 dm_mirror,dm_log, Live 0x0000000000000000
"""

def test_ismicdriverloaded_success(monkeypatch):
    modlist = [line.split()[0] for line in modules_with_mic.splitlines() if line]
    monkeypatch.setattr(tests, "_get_modules", lambda: modlist)
    assert tests.is_micdriver_loaded()

@pytest.mark.parametrize("modules", [modules_no_mic, modules_like_mic])
def test_ismicdriverloaded_fail(monkeypatch, modules):
    modlist = [line.split()[0] for line in modules.splitlines() if line]
    monkeypatch.setattr(tests, "_get_modules", lambda: modlist)
    assert not tests.is_micdriver_loaded()

