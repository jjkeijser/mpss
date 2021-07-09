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
import random
import pytest
import pywintypes

from pytest import raises

import _miccheck
from _miccheck.windows import tests


from _miccheck.common import testtypes as ttypes
from _miccheck.common import exceptions as ex
from _miccheck.common.testtypes import get_test_name
from _miccheck.common.testtypes import PASS, FAIL, NOT_RUN

from utils import (Settings, FakeEntity, tut, generic_tag,
                   devices, FakeMicWmi, raiser)

#valid_knX_dids are the valid PCI device IDs for KNC and KNL respectively
valid_knc_dids = ["VEN_8086&DEV_225{0}".format(i) for i in
                 [1, 2, 3, 4, 5, 6, 7, 8, 9, 0, "a", "b", "c",
                   "d", "e", "f"]]
valid_knl_dids = ["VEN_8086&DEV_226{0}".format(i) for i in
                 [0, 1, 2, 3, 5]]

all_valid_dids = [(d) for d in valid_knc_dids + valid_knl_dids]
invalid_dids = [did for did in ["VEN_8086&DEV_3C00"] +
               ["VEN_8086&DEV_3C00"] +
               ["VEN_8086&DEV_226{0}".format(i) for i in [4, 6, 7, 8, 9,
                "a", "b", "c", "d", "e", "f"]]]

@pytest.mark.parametrize(("did"), all_valid_dids)
def test_num_mics_pci(monkeypatch, did):
    ndevices = random.randint(1, 255)
    entities = [FakeEntity(did) for i in range(ndevices)]
    print "Faking {0} devices".format(ndevices)
    monkeypatch.setattr(tests, "_get_pnpentities", lambda: entities)
    family, total_mics = tests._num_mics_pci()
    assert total_mics == ndevices

@pytest.mark.parametrize(("did"), invalid_dids)
def test_num_mics_pci_no_mics(monkeypatch, did):
    ndevices = random.randint(1, 255)
    entities = [FakeEntity(did) for i in range(ndevices)]
    print "Faking {0} devices".format(ndevices)
    monkeypatch.setattr(tests, "_get_pnpentities", lambda: entities)
    family, total_mics = tests._num_mics_pci()
    assert total_mics == 0

optests = [(test) for test in tests.OPTIONAL_TESTS]
invalid_test = ["this_is_not_a_miccheck_test"]
@pytest.mark.parametrize(("optest"), optests)
def test_optionals_enabled(optest):
    settings = Settings()
    setattr(settings, optest, True)
    assert tests._optionals_enabled(settings)

def test_optionals_enabled_no_optionals():
    settings = Settings()
    assert not tests._optionals_enabled(settings)

retcodes = [0, 1, 255, -1]
@pytest.mark.parametrize("retcode", retcodes)
def test_execute_program(monkeypatch, retcode):
    class FakePopen(object):
        _returncode = -1
        def __init__(self, command_list, **kwargs):
            self.returncode = FakePopen._returncode
            pass

        def communicate(self):
            return "out", "err"


    setattr(FakePopen, "_returncode", retcode)
    monkeypatch.setattr(tests, "Popen", FakePopen)
    #ensure ExecProgramException is raised when retcode != 0
    if retcode:
        with raises(ex.ExecProgramException):
            tests._execute_program("")

    else:
        tests._execute_program("")

def test_num_mics_wmi(monkeypatch):
    expected = 4
    #return any list, only length is used internally
    mics = [0] * expected
    monkeypatch.setattr(tests, "_get_wmi_mic_instances", lambda: mics)
    assert expected == tests._num_mics_wmi()

def test_num_mics_wmi_com_error(monkeypatch):
    monkeypatch.setattr(tests, "_get_wmi_mic_instances", lambda: raiser(pywintypes.com_error()))
    monkeypatch.setattr(tests, "pywintypes", pywintypes)
    assert 0 == tests._num_mics_wmi()

def test_wmi_get_driver_version(monkeypatch):
    class Fake(object):
        def __init__(self, driverver):
            self.driver_version = driverver
    ver = "somedriverversion"
    monkeypatch.setattr(tests, "_get_wmi_mic_instances", lambda: [Fake(ver)]);
    assert ver == tests._wmi_get_driver_version()


@generic_tag(test=tests.PciDevicesTest, status=PASS)
def test_PciDevicesTest_success(tut, monkeypatch):
    monkeypatch.setattr(tests, "TOTAL_MICS", 4)
    tut.run()

@generic_tag(test=tests.PciDevicesTest, status=FAIL)
def test_PciDevicesTest_fail(tut, monkeypatch):
    monkeypatch.setattr(tests, "TOTAL_MICS", 0)
    with raises(ex.FailedTestException):
        tut.run()

@generic_tag(test=tests.WmiDevicesTest, status=PASS)
def test_WmiDevicesTest_success(tut, monkeypatch):
    total_mics = 4
    monkeypatch.setattr(tests, "TOTAL_MICS", total_mics)
    monkeypatch.setattr(tests, "_num_mics_wmi", lambda: total_mics)
    tut.run()

@generic_tag(test=tests.WmiDevicesTest, status=FAIL)
def test_WmiDevicesTest_fail(tut, monkeypatch):
    total_mics = 4
    monkeypatch.setattr(tests, "TOTAL_MICS", total_mics)
    monkeypatch.setattr(tests, "_num_mics_wmi", lambda: total_mics - 1)
    with raises(ex.FailedTestException):
        tut.run()

@generic_tag(test=tests.MicDriverTest, status=PASS)
def test_MicDriverTest_success(tut, monkeypatch):
    monkeypatch.setattr(tests, "_num_mics_wmi", lambda: 1)
    tut.run()

@generic_tag(test=tests.MicDriverTest, status=FAIL)
def test_MicDriverTest_fail(tut, monkeypatch):
    monkeypatch.setattr(tests, "_num_mics_wmi", lambda: 0)
    with raises(ex.FailedTestException):
        tut.run()

@generic_tag(test=tests.BiosVersionTest, status=PASS)
def test_BiosVersionTest_success(tut, monkeypatch, devices):
    mic = FakeMicWmi(bios_version="1.2.3")
    monkeypatch.setattr(_miccheck, "__bios_version__", "1.2.3")
    monkeypatch.setattr(tests, "_get_wmi_mic_instances", lambda: [mic])
    tut.run(device=devices[0])

@generic_tag(test=tests.BiosVersionTest, status=FAIL)
def test_BiosVersionTest_fail_version(tut, monkeypatch, devices):
    mic = FakeMicWmi(bios_version="1.2.3")
    monkeypatch.setattr(_miccheck, "__bios_version__", "3.2.1")
    monkeypatch.setattr(tests, "_get_wmi_mic_instances", lambda: [mic])
    with raises(ex.FailedTestException):
        tut.run(device=devices[0])

@generic_tag(test=tests.SmcFirmwareTest, status=PASS)
def test_SmcFirmwareTest_success(tut, monkeypatch, devices):
    mic = FakeMicWmi(bios_smc_version="1.2.3")
    monkeypatch.setattr(_miccheck, "__smc_fw_version__", "1.2.3")
    monkeypatch.setattr(tests, "_get_wmi_mic_instances", lambda: [mic])
    tut.run(device=devices[0])

@generic_tag(test=tests.SmcFirmwareTest, status=FAIL)
def test_SmcFirmwareTest_fail_version(tut, monkeypatch, devices):
    mic = FakeMicWmi(bios_smc_version="1.2.3")
    monkeypatch.setattr(_miccheck, "__smc_fw_version__", "3.2.1")
    monkeypatch.setattr(tests, "_get_wmi_mic_instances", lambda: [mic])
    with raises(ex.FailedTestException):
        tut.run(device=devices[0])

#parametrize valid combinations of driver versions
#see note in windows/tests.py about DriverVersionTest
@pytest.mark.parametrize("build_ver,running_ver",
        [("1.2.3", "1.2.3"), #equal versions
         ("1.2.4", "1.2.5"), #running driver higher revision
         ("1.2.4", "1.2.3")  #running driver lower revision
         ])
@generic_tag(test=tests.DriverVersionTest, status=PASS)
def test_DriverVersionTest_success(tut, monkeypatch, build_ver, running_ver):
    monkeypatch.setattr(_miccheck, "__version__", build_ver)
    monkeypatch.setattr(tests, "_wmi_get_driver_version", lambda: running_ver)
    tut.run()

@pytest.mark.parametrize("build_ver,running_ver",
        [("", "1.2.3"), #build_ver empty
         ("1.2.3", ""), #running_ver empty
         ("1.3.1", "1.2.1"), #running driver minor version lower
         ("1.2.1", "1.3.1") #running driver minor version higher
         ])
@generic_tag(test=tests.DriverVersionTest, status=FAIL)
def test_DriverVersionTest_fail(tut, monkeypatch, build_ver, running_ver):
    monkeypatch.setattr(_miccheck, "__version__", build_ver)
    monkeypatch.setattr(tests, "_wmi_get_driver_version", lambda: running_ver)
    with raises(ex.FailedTestException):
        tut.run()

@generic_tag(test=tests.StateTest, status=PASS)
def test_StateTest_success(tut, monkeypatch, devices):
    mic = FakeMicWmi(state="ONLINE", post_code=0xFF)
    monkeypatch.setattr(tests, "_get_wmi_mic_instances", lambda: [mic])
    tut.run(device=devices[0])

@pytest.mark.parametrize("state,post",
        [("READY", 0xFF), #invalid state
         ("ONLINE", 0xFB) #invalid post
         ])
@generic_tag(test=tests.StateTest, status=FAIL)
def test_StateTest_fail(tut, monkeypatch, devices, state, post):
    mic = FakeMicWmi(state=state, post_code=post)
    monkeypatch.setattr(tests, "_get_wmi_mic_instances", lambda: [mic])
    with raises(ex.FailedTestException):
        tut.run(device=devices[0])

@generic_tag(test=tests.PingTest, status=PASS)
def test_PingTest_success(tut, monkeypatch, devices):
    monkeypatch.setattr(tests, "_execute_program", lambda _: "success")
    tut.run(device=devices[0])

@pytest.mark.parametrize("excp",
        [OSError, ex.ExecProgramException])
@generic_tag(test=tests.PingTest, status=FAIL)
def test_PingTest_fail(tut, monkeypatch, devices, excp):
    monkeypatch.setattr(tests, "_execute_program", lambda _: raiser(excp))
    if excp is OSError:
        with raises(ex.ExecProgramException):
            tut.run(device=devices[0])
    else:
        with raises(ex.FailedTestException):
            tut.run(device=devices[0])
