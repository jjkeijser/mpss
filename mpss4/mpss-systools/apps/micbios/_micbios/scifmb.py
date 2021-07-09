#Copyright (c) 2017, Intel Corporation.
#
#This program is free software; you can redistribute it and/or modify it
#under the terms and conditions of the GNU Lesser General Public License,
#version 2.1, as published by the Free Software Foundation.
#
#This program is distributed in the hope it will be useful, but WITHOUT ANY
#WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
#FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
#more details.
import ctypes
import sys
import appmb
import systoolsd_api

from errormb import MicBiosError
from systoolsd_api import Shifts, SyscfgCommands

def _get_libscif():
    if "linux" in sys.platform.lower():
        scif = ctypes.CDLL("libscif.so.0", use_errno=True)
    elif "win" in sys.platform.lower():
        scif = ctypes.CDLL("uSCIF.dll", use_errno=True)
    return scif


class _ScifPortID(ctypes.Structure):
    _fields_ = [("node", ctypes.c_uint16),
                ("port", ctypes.c_uint16)]

#request header for systoolsd
class _SystoolsdReq(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("req_type", ctypes.c_uint16),
                ("length", ctypes.c_uint16),
                ("card_errno", ctypes.c_uint16),
                ("extra", ctypes.c_uint32),
                ("data", ctypes.c_char * 16)]


class _MicBiosRequest(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("cmd", ctypes.c_uint8),
                ("prop", ctypes.c_uint8),
                ("value", ctypes.c_uint64)]

class _ScifClient(object):
    def __init__(self, device, port):
        self.device = device
        self.port = port
        self._scif = _get_libscif()
        self._ep = -1
        self._open()
        self._bind()
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
            self._raise("Unable to receive information from systoolsd: scif_open")

    def _bind(self):
        for p in range(1, 1024):
            port_bound = self._scif.scif_bind(self._ep, p)
            if port_bound == -1:
                continue
            else:
                return
        if ctypes.get_errno() == 13:
            self._raise("Permission denied.")
        else:
            self._raise("failed to bind: {0}".format(ctypes.get_errno()))

    def _connect(self):
        port_id = _ScifPortID(self.device + 1, self.port)
        ret = self._scif.scif_connect(self._ep, ctypes.byref(port_id))
        if ret == -1:
            self._raise("Unable to receive information from systoolsd: scif_connect")

    def _raise(self, msg):
        errno = ctypes.get_errno()
        raise MicBiosError(msg)

    def send(self, buffer_obj):
        ret = self._scif.scif_send(self._ep, buffer(buffer_obj)[:],
                                    len(buffer(buffer_obj)), 1)
        if ret == -1:
            self._raise("Unable to receive information from systoolsd: scif_send")

    def recv(self, nbytes, to_struct=None):
        #optional param to_struct should by a ctypes.Structure type
        #If set, the buffer read from scif will be memmoved into an
        #instance of that type and that object will be returned
        buf = ctypes.create_string_buffer(nbytes)
        bytes_recv = self._scif.scif_recv(self._ep, buf, ctypes.sizeof(buf), 1)
        if bytes_recv == -1:
            # The first C-String buffer might have been created incorrectly on
            # Windows due to a ctypes issue, presumably. Lets try again...
            buf = ctypes.create_string_buffer(nbytes)
            bytes_recv = self._scif.scif_recv(self._ep, buf, ctypes.sizeof(buf), 1)
            if bytes_recv == -1:
                self._raise("Unable to receive information from systoolsd: scif_recv")

        if to_struct:
            obj = to_struct()
            ctypes.memmove(ctypes.addressof(obj), buf, ctypes.sizeof(obj))
            return obj

        return buf

class SyscfgSettings(object):
    mapper = {"cluster"   : "cluster_mode",
              "ecc"       : "ecc_mode",
              "apei-supp" : "apei_supp_mode",
              "apei-einj" : "apei_einj_mode",
              "apei-ffm"  : "apei_ffm_mode",
              "apei-table": "apei_table_mode",
              "fwlock"    : "fwlock_mode"}

    def __init__(self, value=0):
        self.value = value
        self.cluster = (value >> Shifts.cluster_shift) & 0b1111
        self.ecc = (value >> Shifts.ecc_shift) & 0b111
        self.apei_supp = (value >> Shifts.apei_supp_shift) & 0b11
        self.apei_einj = (value >> Shifts.apei_einj_shift) & 0b11
        self.apei_ffm = (value >> Shifts.apei_ffm_shift) & 0b11
        self.apei_table = (value >> Shifts.apei_table_shift) & 0b11
        self.fwlock = (value >> Shifts.fwlock_shift) & 0b11

    def cluster_mode(self):
        return systoolsd_api.cluster_config_name(self.cluster)

    def ecc_mode(self):
        return systoolsd_api.ecc_config_name(self.ecc)

    def apei_supp_mode(self):
        return systoolsd_api.apei_supp_config_name(self.apei_supp)

    def apei_einj_mode(self):
        return systoolsd_api.apei_einj_config_name(self.apei_einj)

    def apei_ffm_mode(self):
        return systoolsd_api.apei_ffm_config_name(self.apei_ffm)

    def apei_table_mode(self):
        return systoolsd_api.apei_ffm_config_name(self.apei_table)

    def fwlock_mode(self):
        return systoolsd_api.fwlock_config_name(self.fwlock)

    def set_cluster_mode(self, cl_mode):
        self.value |= (cl_mode << Shifts.cluster_shift)
        return self

    def set_ecc_mode(self, ecc_mode):
        self.value |= (ecc_mode << Shifts.ecc_shift)
        return self

    def set_apei_supp_mode(self, ap_sup_mode):
        self.value |= (ap_sup_mode << Shifts.apei_supp_shift)
        return self

    def set_apei_einj_mode(self, ap_ei_mode):
        self.value |= (ap_ei_mode << Shifts.apei_einj_shift)
        return self

    def set_apei_ffm_mode(self, ap_ff_mode):
        self.value |= (ap_ff_mode << Shifts.apei_ffm_shift)
        return self

    def set_apei_table_mode(self, ap_ta_mode):
        self.value |= (ap_ta_mode << Shifts.apei_table_shift)
        return self

    def set_fwlock_mode(self, fw_mode):
        self.value |= (fw_mode << Shifts.fwlock_shift)
        return self

    def setting_by_name(self, setting):
        return getattr(self, self.mapper[setting])()

class MicBiosReq(object):
    def __init__(self, device):
        self.device = device
        self.sys_req = _SystoolsdReq()
        self.client = _ScifClient(self.device.num, 130)

    def _init(self, command, prop, value=0, passwd=""):
        MICBIOS_REQUEST = 17
        self.sys_req.req_type = MICBIOS_REQUEST
        self.sys_req.data = passwd
        self.client.send(self.sys_req)
        self.resp = self.client.recv(ctypes.sizeof(self.sys_req), to_struct=_SystoolsdReq)
        if self.resp.card_errno:
            raise MicBiosError("Daemon error")
        self.mb_req = _MicBiosRequest()
        self.mb_req.cmd = command
        self.mb_req.prop = prop
        self.mb_req.value = value
        self.client.send(self.mb_req)

    def _systoolsd_error(self, err):
        raise MicBiosError("Error: {}".format(
            systoolsd_api.systoolsd_error(err)))

    def write_req(self, prop, value, passwd):
        self._init(SyscfgCommands.write, prop, value, passwd)
        self.resp = self.client.recv(ctypes.sizeof(self.sys_req), to_struct=_SystoolsdReq)
        if self.resp.card_errno:
            self._systoolsd_error(self.resp.card_errno)

    def read_req(self, prop):
        self._init(SyscfgCommands.read, prop)
        resp = self.client.recv(ctypes.sizeof(_SystoolsdReq), to_struct=_SystoolsdReq)
        if resp.card_errno:
            self._systoolsd_error(resp.card_errno)
        resp = self.client.recv(ctypes.sizeof(self.mb_req), to_struct=_MicBiosRequest)
        return SyscfgSettings(resp.value)

    def passwd_req(self, old_passwd, new_passwd):
        self.sys_req.length = len(new_passwd)
        self._init(SyscfgCommands.password, 0, 0, old_passwd)
        self.resp = self.client.recv(ctypes.sizeof(_SystoolsdReq), to_struct=_SystoolsdReq)
        if self.resp.card_errno:
            self._systoolsd_error(self.resp.card_errno)
        pwd = ctypes.create_string_buffer(new_passwd)
        self.client.send(pwd)
        self.resp = self.client.recv(ctypes.sizeof(self.sys_req), to_struct=_SystoolsdReq)
        if self.resp.card_errno:
            self._systoolsd_error(self.resp.card_errno)
