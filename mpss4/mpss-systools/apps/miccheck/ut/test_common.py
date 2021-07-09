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
from StringIO import StringIO
import ctypes
import itertools
import logging
import pytest
import random

import _miccheck
from _miccheck import common
from _miccheck.common import DeviceInfo
from _miccheck.common import exceptions as ex
from _miccheck.common import printing as prnt
from _miccheck.common import testrunner
from _miccheck.common import testtypes as ttypes
from _miccheck.common.tests import (MiccheckTest, OptionalTest,
                                   SystoolsdTest)
from utils import generic_tag, devices, FakePassTest, FakeFailTest

import _miccheck.common.tests as ctests
import _miccheck.common.version as version

import _miccheck.common.base as base

import utils

NTESTS = 4
NCARDS = 2

_miccheck.__smc_fw_version__ = utils.SMCVER


def count_tests(tests, target, name):
    return len([test for test in tests if test.target == target
                                       and test.name == name])


def getvalue(streams):
    """Get contents from StringIO objects from capture fixture"""
    return (streams[0].getvalue().strip(),
            streams[1].getvalue().strip())


@pytest.fixture
def libscif_mock(request):
    """Returns a tuple (scif_instance, function).

    Useful to replace _get_libscif() function from common.tests. By
    default, the scif object it returns succeeds on every call to
    its exposed methods.

    Note that test functions needing other than the default behavior
    must be decorated with the generic_tag decorator (kwargs scif_open,
    scif_connect).
    """
    #the following must be set by tests using this fixture
    scif_open = getattr(request.function, "scif_open", 7)
    scif_connect = getattr(request.function, "scif_connect", 7777)
    scif_recv = getattr(request.function, "scif_recv", 6)
    scif_send = getattr(request.function, "scif_send", 6)

    class scif(object):
        def __init__(self):
            self.was_closed = False
            self._open = None
            self._connect = None

        def scif_open(self):
            return self._open

        def scif_close(self, nr):
            self.was_closed = True
            return 0

        def scif_connect(self, fd, port_id):
            return self._connect

        def scif_recv(self, *args):
            return self._recv

        def scif_send(self, *args):
            return self._send

    scif_instance = scif()
    attrs = [("_open", scif_open), ("_connect", scif_connect),
             ("_recv", scif_recv), ("_send", scif_send)]
    [setattr(scif_instance, attr, value) for attr, value in attrs]

    def f():
        return scif_instance

    return scif_instance, f

executed_tests = lambda x: x.get_status() != ttypes.NOT_RUN

@pytest.fixture
def capture():
    """Fixture to capture output from miccheck loggers"""
    #get the loggers used by miccheck
    out = logging.getLogger("out")
    err = logging.getLogger("err")
    #reset the handlers and replace with StringIO objects
    out.handlers = []
    err.handlers = []
    stdout = StringIO()
    stderr = StringIO()
    out_handler = logging.StreamHandler(stdout)
    err_handler = logging.StreamHandler(stderr)
    out.addHandler(out_handler)
    err.addHandler(err_handler)
    return stdout, stderr

@pytest.fixture
def host_tests():
    return [FakePassTest() for i in range(NTESTS)]

@pytest.fixture
def device_tests():
    return [[FakePassTest() for i in range(NTESTS)]
             for j in range(NCARDS)]


def test_TestRunner_no_tests(capture):
    testrunner.TestRunner([], [], [], []).run([])
    out, err = getvalue(capture)
    assert not out
    assert not err

def test_TestRunner_default_host(capture, host_tests):
    runner = testrunner.TestRunner(host_tests, [], [], [])
    runner.run([])
    out, err = getvalue(capture)
    assert out
    assert out.count(FakePassTest.__name__) == NTESTS
    assert out.count("pass") == NTESTS
    assert not "optional" in out
    assert not "device" in out
    assert "host" in out
    assert not err
    #ensure all tests passed in were executed
    assert all(map(executed_tests, host_tests))

def test_TestRunner_optional_host(capture, host_tests):
    runner = testrunner.TestRunner([], host_tests, [], [])
    runner.run([])
    out, err = getvalue(capture)
    assert out
    assert out.count(FakePassTest.__name__) == NTESTS
    assert out.count("pass") == NTESTS
    assert not "default" in out
    assert not "device" in out
    assert "host" in out
    assert not err
    assert all(map(executed_tests, host_tests))

def test_TestRunner_default_device(capture, device_tests, devices):
    runner = testrunner.TestRunner([], [], device_tests, [])
    runner.run(devices)
    out, err = getvalue(capture)
    assert out
    assert out.count(FakePassTest.__name__) == NTESTS * NCARDS
    assert out.count("pass") == NTESTS * NCARDS
    assert "default" in out
    assert not "optional" in out
    assert not "host" in out
    assert "device" in out
    assert "mic0" in out
    assert "mic1" in out
    assert not err
    assert all(map(executed_tests, device_tests[0]))
    assert all(map(executed_tests, device_tests[1]))

def test_TestRunner_optional_device(capture, device_tests, devices):
    runner = testrunner.TestRunner([], [], [], device_tests)
    runner.run(devices)
    out, err = getvalue(capture)
    assert out
    assert out.count(FakePassTest.__name__) == NTESTS * NCARDS
    assert out.count("pass") == NTESTS * NCARDS
    assert "optional" in out
    assert not "default" in out
    assert not "host" in out
    assert "device" in out
    assert "mic0" in out
    assert "mic1" in out
    assert not err
    assert all(map(executed_tests, device_tests[0]))
    assert all(map(executed_tests, device_tests[1]))

def test_TestRunner_default_host_failure(capture, host_tests):
    #have the third test fail
    #ensure exception is raised and that no more tests were
    #executed after the failure
    host_tests[2] = FakeFailTest()
    runner = testrunner.TestRunner(host_tests, [], [], [])
    with pytest.raises(ex.FailedTestException):
        runner.run([])
    out, err = getvalue(capture)
    assert out
    assert err
    #two tests must pass and reflect that in stdout
    assert out.count(FakePassTest.__name__) == NTESTS - 2
    #one test must fail and reflect that in stderr
    assert err.count(FakeFailTest.__name__) == 1
    assert err.count("fail") == 1
    assert out.count("pass") == NTESTS - 2
    assert "default" in out
    assert not "optional" in out
    assert all(map(executed_tests, host_tests[:3]))
    assert not executed_tests(host_tests[3])

def test_TestRunner_optional_host_failure(capture, host_tests):
    #all optional tests must execute regardless of failures
    host_tests[2] = FakeFailTest()
    runner = testrunner.TestRunner([], host_tests, [], [])
    with pytest.raises(ex.FailedTestException):
        runner.run([])
    out, err = getvalue(capture)
    assert out
    assert err
    #three tests must pass and reflect that in stdout
    assert out.count(FakePassTest.__name__) == 3
    assert err.count(FakeFailTest.__name__) == 1
    assert out.count("pass") == 3
    assert err.count("fail") == 1
    assert not "default" in out
    assert "optional" in out
    assert all(map(executed_tests, host_tests))

def test_TestRunner_default_device_failure(capture, device_tests, devices):
    #have the third test for the first device fail
    #after the failure, no more tests should be executed for that device
    device_tests[0][2] = FakeFailTest()
    runner = testrunner.TestRunner([], [], device_tests, [])
    with pytest.raises(ex.FailedTestException):
        runner.run(devices)
    out, err = getvalue(capture)
    assert out
    assert err
    #the fourth test for the first device should not run
    # that means that NCARDS*NTESTS - 2 tests must pass
    assert out.count(FakePassTest.__name__) == NTESTS * NCARDS - 2
    assert err.count(FakeFailTest.__name__) == 1
    assert out.count("pass") == NTESTS * NCARDS - 2
    assert err.count("fail") == 1
    assert "default" in out
    assert not "optional" in out
    assert all(map(executed_tests, device_tests[0][:3]))
    assert not executed_tests(device_tests[0][3])
    assert all(map(executed_tests, device_tests[1]))

def test_TestRunner_optional_device_failure(capture, device_tests, devices):
    device_tests[0][2] = FakeFailTest()
    runner = testrunner.TestRunner([], [], [], device_tests)
    with pytest.raises(ex.FailedTestException):
        runner.run(devices)
    out, err = getvalue(capture)
    assert out
    assert err
    #only one test should have failed
    assert out.count(FakePassTest.__name__) == NTESTS * NCARDS - 1
    assert err.count(FakeFailTest.__name__) == 1
    assert out.count("pass") == NTESTS * NCARDS - 1
    assert err.count("fail") == 1
    assert not "default" in out
    assert "optional" in out
    assert all(map(executed_tests, device_tests[0]))
    assert all(map(executed_tests, device_tests[1]))

def test_TestClass_common():
    test = FakePassTest()
    test.run()
    assert not test.get_errmsg()
    test = FakeFailTest()
    try:
        test.run()
    except:
        pass
    assert test.get_errmsg()

def test_TestClass_order():
    order = 5
    @generic_tag(_order=order)
    class FakeOptionalTest(OptionalTest):
        pass

    optest = FakeOptionalTest()
    assert order == optest.get_order()

def test_filtertests():
    #filter_tests expects an object on which it can call
    # a values() method (like a dict)
    class FakeDict(object):
        def __init__(self, a_list):
            self._list = a_list

        def values(self):
            return self._list

    #the fake test classes will be tagged with this target
    targets = ["target 1", "target 2", "target 3"]
    #fake test classes also need a name attribute, which must
    # be enabled in the Settings object passed into the function
    test_names = ["name 1", "name 2", "name 3", "name 4"]
    #nclasses will be created
    nclasses = random.randint(1, 100)

    all_tests = []
    for i in range(nclasses):
        class SomeClass(MiccheckTest):
            pass
        setattr(SomeClass, "target", random.choice(targets))
        setattr(SomeClass, "name", random.choice(test_names))
        all_tests.append(SomeClass)

    #the target by which we'll filter tests
    target_to_find = random.choice(targets)
    #the name by which we'll filter tests. It'll have to be enabled
    # in a Settings object
    name_to_find = random.choice(test_names)
    settings = utils.Settings()
    setattr(settings, name_to_find, True)
    fake_dict = FakeDict(all_tests)

    real_count = count_tests(all_tests, target_to_find, name_to_find)
    count_from_test = len(base.filter_tests(target_to_find, settings, fake_dict))
    assert real_count == count_from_test

@pytest.mark.parametrize("daemon_test, cstruct, field",
        [(ctests.RasTest, ctests._MrHdr, "cmd"),
         (ctests.SystoolsdTest, ctests._SystoolsdReq, "length")])
def test_DaemonTest_success(devices, monkeypatch, daemon_test, cstruct, field):
    hdr = cstruct()
    setattr(hdr, field, 7)
    #special case for ras, set the MR_RESP bit
    if daemon_test.__name__ == ctests.RasTest.__name__:
        setattr(hdr, field, 1 << 14)

    monkeypatch.setattr(utils.FakeScifClient, "recv", lambda *args, **kwargs: hdr)
    monkeypatch.setattr(ctests, "_ScifClient", utils.FakeScifClient)
    t = daemon_test()
    assert t.get_status() == ttypes.NOT_RUN
    t.run(device=devices[0])
    assert t.get_status() == ttypes.PASS
    assert t.msg_executing()

def test_SystoolsdTest_fail(devices, monkeypatch):
    #fail by raising a ScifError
    monkeypatch.setattr(utils.FakeScifClient, "send", lambda *args: utils.raiser(ex.ScifError()))
    monkeypatch.setattr(ctests, "_ScifClient", utils.FakeScifClient)
    t = SystoolsdTest()
    assert t.get_status() == ttypes.NOT_RUN
    with pytest.raises(ex.FailedTestException):
        t.run(device=devices[0])
    assert t.get_status() == ttypes.FAIL
    #reset env
    monkeypatch.undo()
    #fail from _SystoolsdReq() with length field set to 0
    monkeypatch.setattr(utils.FakeScifClient, "recv",
            lambda *args, **kwargs: ctests._SystoolsdReq())
    monkeypatch.setattr(ctests, "_ScifClient", utils.FakeScifClient)
    t= SystoolsdTest()
    with pytest.raises(ex.FailedTestException):
        t.run(device=devices[0])

#def test_RasTest_success(devices, monkeypatch):

def test_isenabled():
    settings = utils.Settings()
    attr = "attr"
    setattr(settings, attr, 1)
    assert base.is_enabled(attr, settings)
    assert not base.is_enabled("no_attr", settings)

#two tests
default_host_tests = [FakePassTest()] * 2
#two tests
optional_host_tests = [FakePassTest()] * 2
#two tests for two devices
default_device_tests = [[FakePassTest()] * 2] * 2
#two tests for two devices
optional_device_tests = [[FakePassTest()] * 2] * 2

def test_ScifClient_success(libscif_mock, monkeypatch):
    scif, get_scif = libscif_mock
    monkeypatch.setattr(ctests, "_get_libscif", get_scif)
    client = ctests._ScifClient(0, 100)
    client.send("our fake scif won't care what we send")
    nbytes = 10
    resp = client.recv(nbytes)
    assert nbytes == len(resp)
    hdr = ctests._MrHdr()
    resp = client.recv(ctypes.sizeof(hdr), to_struct=ctests._MrHdr)
    assert type(resp) == ctests._MrHdr

@pytest.mark.parametrize("scif_function",
        ["scif_open", "scif_connect",
         "scif_recv", "scif_send"])
def test_ScifClient_fail(libscif_mock, monkeypatch, scif_function):
    scif, get_scif = libscif_mock
    #have every parametrized function return -1
    setattr(scif, scif_function, lambda *args: -1)
    monkeypatch.setattr(ctests, "_get_libscif", get_scif)
    with pytest.raises(ex.ScifError):
        client = ctests._ScifClient(0, 100)
        client.send("whatever")
        client.recv(10)

