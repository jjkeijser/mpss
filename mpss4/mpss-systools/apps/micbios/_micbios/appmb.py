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
"""Application functions module"""

from constantsmb import Subcommandsmb
from errormb import MicBiosError
from devicemb import MicDeviceInfo
from scifmb import MicBiosReq, SyscfgSettings
from systoolsd_api import Properties


class Micbios(object):
    """Base class"""
    actions = dict()
    def __init__(self, settings):
        self.settings = settings

    @staticmethod
    def register(name, cls):
        """Register the application action with its corresponding class"""
        Micbios.actions[name] = cls

    def run(self):
        """Run method for each Micbios subclass"""
        action = Micbios.actions[self.settings.action](self.settings)
        return action.run()

class Actionmb(object):
    """Implementation class"""
    def run(self):
        raise NotImplementedError()

class SyscfgAction(Actionmb):
    def __init__(self, settings):
        self.prop = ""
        self.value = 0
        self.settings = settings
        """Call the ssh class to execute the command if all devices are online."""
        self.devices = [MicDeviceInfo(devname) for devname in self.settings.device_list]
        try:
            devs_offline = [dev.name for dev in self.devices if not dev.is_card_online()]
        except:
            raise MicBiosError("Requested devices cannot be greater than available devices.")
        if devs_offline:
            raise MicBiosError("The following coprocessors are not "
                    "in online state: {}".format(devs_offline))

    def run(self):
        raise NotImplementedError()

class SetAction(SyscfgAction):
    def __init__(self, settings):
        SyscfgAction.__init__(self, settings)

    def run(self):
        """Print the reboot message if the action was executed successfully."""
        self._run()
        print "Successfully Completed"
        print "Please reboot the coprocessor(s) for changes to take effect."

    def _run(self):
        raise NotImplementedError()

    def _do_write_req(self, prop, syscfg_settings, passwd):
        for dev in self.devices:
            MicBiosReq(dev).write_req(prop,
                syscfg_settings.value, passwd)


class Getinfo(SyscfgAction):
    """Specialized class for getinfo that will map the command with the BIOS setting
    given"""
    mapper = {"cluster": (0x01, "Cluster Mode"),
              "ecc": (0x02, "ECC Support"),
              "apei-supp": (0x04, "APEI Support"),
              "apei-ffm": (0x08, "APEI FFM Logging"),
              "apei-einj": (0x10, "APEI PCIe Error Injection"),
              "apei-table": (0x20, "APEI PCIe EInj Action Table"),
              "fwlock": (0x40, "MICFW Update Flag")}

    def __init__(self, settings):
        SyscfgAction.__init__(self, settings)

    def run(self):
        self.prop = self.mapper[self.settings.biossetting][0]
        mb_settings = [MicBiosReq(device).read_req(self.prop)for device in self.devices]
        for dev, settings in zip(self.devices, mb_settings):
            print ("mic{}:\n\n   {:<35} : {}\n".format(dev.num,
                    self.mapper[self.settings.biossetting][1],
                    settings.setting_by_name(self.settings.biossetting)))

Micbios.register(Subcommandsmb.getinfo, Getinfo)

class SetCluster(SetAction):
    """Specialized class for set-cluster-mode that will map the command with the
    configuration of BIOS setting given"""
    mapper = {"all2all": 0,
              "snc2": 1,
              "snc4": 2,
              "hemisphere": 3,
              "quadrant": 4,
              "auto": 5}
    def __init__(self, settings):
        SyscfgAction.__init__(self, settings)

    def _run(self):
        prop = Properties.cluster
        value = self.mapper[self.settings.mode]
        syscfg_settings = SyscfgSettings().set_cluster_mode(value)
        self._do_write_req(prop, syscfg_settings, self.settings.password)

Micbios.register(Subcommandsmb.cluster, SetCluster)

class Setpass(SetAction):
    """Specialized class for set-pass that will map the command with the passwords
    given"""
    def __init__(self, settings):
        SyscfgAction.__init__(self, settings)

    def _run(self):
        for dev in self.devices:
            MicBiosReq(dev).passwd_req(self.settings.old_pass, self.settings.new_pass)

Micbios.register(Subcommandsmb.setpass, Setpass)

class SetEcc(SetAction):
    """Specialized class for set-ecc that will map the command with the configuration
    of BIOS setting given"""
    mapper = {"disable": 0,
              "enable": 1,
              "auto": 2}
    def __init__(self, settings):
        SyscfgAction.__init__(self, settings)

    def _run(self):
        prop = Properties.ecc
        value = self.mapper[self.settings.mode]
        syscfg_settings = SyscfgSettings().set_ecc_mode(value)
        self._do_write_req(prop, syscfg_settings, self.settings.password)

Micbios.register(Subcommandsmb.ecc, SetEcc)

class SetApeiSupp(SetAction):
    """Specialized class for set-apei-supp that will map the command with the
    configuration of BIOS setting given"""
    mapper = {"disable": 0,
              "enable": 1}
    def __init__(self, settings):
        SyscfgAction.__init__(self, settings)

    def _run(self):
        prop = Properties.apeisupp
        value = self.mapper[self.settings.mode]
        syscfg_settings = SyscfgSettings().set_apei_supp_mode(value)
        self._do_write_req(prop, syscfg_settings, self.settings.password)

Micbios.register(Subcommandsmb.apeisupp, SetApeiSupp)

class SetApeiFfm(SetAction):
    """Specialized class for set-apei-ffm that will map the command with the
    configuration of BIOS setting given"""
    mapper = {"disable": 0,
              "enable": 1}
    def __init__(self, settings):
        SyscfgAction.__init__(self, settings)

    def _run(self):
        prop = Properties.apeiffm
        value = self.mapper[self.settings.mode]
        syscfg_settings = SyscfgSettings().set_apei_ffm_mode(value)
        self._do_write_req(prop, syscfg_settings, self.settings.password)

Micbios.register(Subcommandsmb.apeiffm, SetApeiFfm)

class SetApeiEinj(SetAction):
    """Specialized class for set-apei-einj that will map the command with the
    configuration of BIOS setting given"""
    mapper = {"disable": 0,
              "enable": 1}
    def __init__(self, settings):
        SyscfgAction.__init__(self, settings)

    def _run(self):
        prop = Properties.apeieinj
        value = self.mapper[self.settings.mode]
        syscfg_settings = SyscfgSettings().set_apei_einj_mode(value)
        self._do_write_req(prop, syscfg_settings, self.settings.password)

Micbios.register(Subcommandsmb.apeieinj, SetApeiEinj)

class SetApeiTable(SetAction):
    """Specialized class for set-apei-einjtable that will map the command with
    the configuration of BIOS setting given"""
    mapper = {"disable": 0,
              "enable": 1}
    def __init__(self, settings):
        SyscfgAction.__init__(self, settings)

    def _run(self):
        prop = Properties.apeitable
        value = self.mapper[self.settings.mode]
        syscfg_settings = SyscfgSettings().set_apei_table_mode(value)
        self._do_write_req(prop, syscfg_settings, self.settings.password)

Micbios.register(Subcommandsmb.apeitable, SetApeiTable)

class SetFwlock(SetAction):
    """Specialized class for set-fwlock that will map the command with the
    configuration of BIOS setting given"""
    mapper = {"disable": 0,
              "enable": 1}
    def __init__(self, settings):
        SyscfgAction.__init__(self, settings)

    def _run(self):
        prop = Properties.fwlock
        value = self.mapper[self.settings.mode]
        syscfg_settings = SyscfgSettings().set_fwlock_mode(value)
        self._do_write_req(prop, syscfg_settings, self.settings.password)

Micbios.register(Subcommandsmb.fwlock, SetFwlock)
