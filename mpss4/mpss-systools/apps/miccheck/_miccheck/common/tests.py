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
import ctypes
import inspect
import os
import sys

import _miccheck
import _miccheck.common.testtypes as ttypes

from _miccheck.common import exceptions as ex
from _miccheck.common import printing as prnt
from _miccheck.common import MiccheckTest, OptionalTest

def _get_libscif():
    if sys.platform.startswith("linux"):
        scif = ctypes.CDLL("libscif.so.0", use_errno=True)
    elif sys.platform.startswith("win"):
        scif = ctypes.CDLL("uSCIF.dll", use_errno=True)
    return scif


class _ScifPortID(ctypes.Structure):
    _fields_ = [("node", ctypes.c_uint16),
                ("port", ctypes.c_uint16)]

#request header for RAS daemon
class _MrHdr(ctypes.Structure):
    _pack_ = 4
    _fields_ = [("cmd", ctypes.c_uint16),
                ("len", ctypes.c_uint16),
                ("parm", ctypes.c_uint32),
                ("stamp", ctypes.c_uint64),
                ("spent", ctypes.c_uint64)]


#request header for systoolsd
class _SystoolsdReq(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("req_type", ctypes.c_uint16),
                ("length", ctypes.c_uint16),
                ("card_errno", ctypes.c_uint16),
                ("extra", ctypes.c_uint32),
                ("data", ctypes.c_char * 16)]

class _ScifClient(object):
    def __init__(self, device, port):
        self.device = device
        self.port = port
        self._scif = _get_libscif()
        self._ep = -1
        self._open()
        self._connect()

    def __del__(self):
        try:
            if self._ep != -1:
                self._scif.scif_close(self._ep)
        except AttributeError:
            pass

    def _open(self):
        self._ep = self._scif.scif_open()
        if self._ep == -1:
            self._raise("scif_open failed")

    def _connect(self):
        port_id = _ScifPortID(self.device + 1, self.port)
        ret = self._scif.scif_connect(self._ep, ctypes.byref(port_id))
        if ret == -1:
            self._raise("scif_connect failed")

    def _raise(self, msg):
        errno = ctypes.get_errno()
        raise ex.ScifError(msg)

    def send(self, buffer_obj):
        ret = self._scif.scif_send(self._ep, buffer(buffer_obj)[:],
                                   len(buffer(buffer_obj)), 1)
        if ret == -1:
            self._raise("scif_send failed")

    def recv(self, nbytes, to_struct=None):
        #optional param to_struct should by a ctypes.Structure type
        #If set, the buffer read from scif will be memmoved into an
        #instance of that type and that object will be returned
        buf = ctypes.create_string_buffer(nbytes)
        bytes_recv = self._scif.scif_recv(self._ep, buf, ctypes.sizeof(buf), 1)
        if bytes_recv == -1:
            self._raise("scif_recv failed")

        if to_struct:
            obj = to_struct()
            ctypes.memmove(ctypes.addressof(obj), buf, ctypes.sizeof(obj))
            return obj

        return buf


class SystoolsdTest(MiccheckTest):
    def _run(self, **kwargs):
        device = kwargs["device"].dev
        #constants defined in systoolsd_api.h
        GET_SYSTOOLSD_INFO = 1
        hdr = _SystoolsdReq()
        hdr.req_type = GET_SYSTOOLSD_INFO
        try:
            client = _ScifClient(device, 130)
            client.send(hdr)
            resp = client.recv(ctypes.sizeof(hdr), to_struct=_SystoolsdReq)
        except ex.ScifError as excp:
            raise ex.FailedTestException(str(excp))

        #perform checks on header
        if not resp.length:
            raise ex.FailedTestException("systoolsd failed to respond")

    @staticmethod
    def msg_executing():
        return "Check systoolsd is running in device"

class RasTest(MiccheckTest):
    def _run(self, **kwargs): # not static because it is a device test
        device = kwargs["device"].dev
        #some constants defined in micras_api.h
        MR_REQ_HWINF = 1
        MR_RESP = 1 << 14

        hdr = _MrHdr()
        hdr.cmd = MR_REQ_HWINF
        try:
            #RAS listens on port 100
            client = _ScifClient(device, 100)
            client.send(hdr)
            resp = client.recv(ctypes.sizeof(hdr), to_struct=_MrHdr)
        except ex.ScifError as excp:
            raise ex.FailedTestException(str(excp))

        #ensure RAS daemon responded
        if not resp.cmd & MR_RESP:
            raise ex.FailedTestException("RAS daemon failed to respond")

    @staticmethod
    def msg_executing():
        return "Check RAS daemon is available in device"

class PciDevicesTestBase(MiccheckTest):
    err_msg = "no Intel(R) Xeon Phi(TM) coprocessor devices detected"

    @staticmethod
    def msg_executing():
        return "Check number of devices the OS sees in the system"


class ScifDevicesTestBase(MiccheckTest):
    err_msg = "SCIF nodes do not match the number of PCI-detected devices"

    @staticmethod
    def msg_executing():
        return "Check number of devices driver sees in the system"


class PingTestBase(OptionalTest):
    @staticmethod
    def msg_error(addr):
        return "{0} did not respond to ping request".format(addr)

    @staticmethod
    def msg_executing():
        return "Check device can be pinged over its network interface"

