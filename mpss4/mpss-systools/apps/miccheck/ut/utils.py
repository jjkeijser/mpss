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
import pytest

from _miccheck.common import exceptions as ex
from _miccheck.common.tests import MiccheckTest, _SystoolsdReq
from _miccheck.common import DeviceInfo

from _miccheck.common.testtypes import PASS, FAIL, NOT_RUN

SMCVER = "1.7"

class FakePassTest(MiccheckTest):
    """Testing class"""
    name = "FakePassTest"
    def _run(self, **kwargs):
        pass

    def msg_executing(self):
        return self.__class__.__name__


class FakeFailTest(MiccheckTest):
    """Testing class for negative cases"""
    name = "FakeFailTest"
    def _run(self, **kwargs):
        raise ex.FailedTestException("something nasty has happened")

    def msg_executing(self):
        return self.__class__.__name__


class FakeProperties(object):
    def __init__(self, did):
        self.Value = did

class FakeEntity(object):
    def __init__(self, did):
        self.properties = {"DeviceID": FakeProperties(did)}

    def Properties_(self, key):
        return self.properties[key]

class FakeMicWmi(object):
    """Fake class to be used in Windows tests"""
    def __init__(self, **kwargs):
        for k, v in kwargs.iteritems():
            setattr(self, k, v)


class FakeScifClient(object):
    def __init__(self, *args, **kwargs):
        pass

    def send(self, *args, **kwargs):
        pass

    def recv(self, *args, **kwargs):
        resp = _SystoolsdReq()
        resp.length = 16
        return resp


class Settings(object):
    pass


class generic_tag(object):
    """Decorator class used to add generic attributes to test functions.

    It accepts arbitrary kwargs in its constructor. For every key and
    value in kwargs, the following will be called:
    setattr(test_function, k, v)

    This is useful in conjunction with pytest.fixture and
    pytest's request.function.
    """
    def __init__(self, **kwargs):
        self._attrs = kwargs

    def __call__(self, f):
        for k, v in self._attrs.items():
            setattr(f, k, v)
        return f


@pytest.fixture
def devices():
    #return NTESTS devices
    return [DeviceInfo(0), DeviceInfo(1)]


@pytest.fixture
def tut(request):
    """Test under test fixture function"""
    #tut = test under test
    #will raise if not set by test function
    test = getattr(request.function, "test")()
    status = getattr(request.function, "status")
    assert test.get_status() == NOT_RUN
    assert test.msg_executing()
    def fin():
        assert test.get_status() == status
    request.addfinalizer(fin)
    return test

def raiser(ex):
    raise ex


