# Intel MIC Platform Software Stack (MPSS)
# Copyright (c) 2016, Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.

# Makefile - Intel MIC Linux driver.

BUILD_ROOT :=

ifneq ($(SDKTARGETSYSROOT),)
BUILD_CARD := true
KERNEL_SRC := $(SDKTARGETSYSROOT)/usr/src/kernel
else
BUILD_CARD := false
KERNEL_SRC := /lib/modules/$(shell uname -r)/build
endif

RELEASE_FILE := include/config/kernel.release

ifneq ($(wildcard $(O)/$(RELEASE_FILE)),)
KERNEL_BUILD := $(O)
else ifneq ($(wildcard $(KERNEL_SRC)/$(RELEASE_FILE)),)
KERNEL_BUILD := $(KERNEL_SRC)
else
$(error Unable to detect kernel build tree. Please specify it as KERNEL_SRC param)
endif

KERNEL_RELEASE := $(shell cat $(KERNEL_BUILD)/include/config/kernel.release)
KERNEL_MODULES := /lib/modules/$(KERNEL_RELEASE)/extra/mic

MODULE_SYMVERS := /lib/modules/$(KERNEL_RELEASE)
MODULE_KERNEL_HEADERS := $(KERNEL_BUILD)/include/modules
MODULE_USER_HEADERS := /usr/include
MODULE_SYSCONFIG := /etc

BUILD_X100 := false
BUILD_MODULES_SIGN := false
BUILD_COVERAGE := false

$(info == Building MIC kernel modules with following configuration:)
$(info BUILD_ROOT:              $(BUILD_ROOT))
$(info KERNEL_BUILD:            $(KERNEL_BUILD))
$(info KERNEL_RELEASE:          $(KERNEL_RELEASE))
$(info KERNEL_MODULES:          $(KERNEL_MODULES))
$(info MODULE_SYMVERS:          $(MODULE_SYMVERS))
$(info MODULE_KERNEL_HEADERS:   $(MODULE_KERNEL_HEADERS))
$(info MODULE_USER_HEADERS:     $(MODULE_USER_HEADERS))
$(info MODULE_SYSCONFIG:        $(MODULE_SYSCONFIG))
$(info BUILD_CARD:              $(BUILD_CARD))
$(info BUILD_X100:              $(BUILD_X100))
$(info BUILD_MODULES_SIGN:      $(BUILD_MODULES_SIGN))
$(info BUILD_COVERAGE:          $(BUILD_COVERAGE))

# define kernel config for the MIC
KCONFIG += "CONFIG_INTEL_MIC=m"
KCONFIG += "CONFIG_INTEL_MIC_X200_DMA=m"
KCONFIG += "CONFIG_INTEL_MIC_X200=m"
KCONFIG += "CONFIG_SCIF_BUS=m"
KCONFIG += "CONFIG_SCIF=m"

ifeq ($(BUILD_CARD),true)
KCONFIG += "CONFIG_INTEL_MIC_CARD=m"
KCONFIG += "CONFIG_INTEL_MIC_HOST=n"
KCONFIG += "CONFIG_MIC_COSM=n"
KCONFIG += "CONFIG_MIC_COSM_CLIENT=m"
else
KCONFIG += "CONFIG_INTEL_MIC_HOST=m"
KCONFIG += "CONFIG_INTEL_MIC_CARD=n"
KCONFIG += "CONFIG_MIC_COSM=m"
KCONFIG += "CONFIG_MIC_COSM_CLIENT=n"
endif

ifeq ($(BUILD_X100),true)
KCONFIG += "CONFIG_INTEL_MIC_X100_DMA=m"
KCONFIG += "CONFIG_INTEL_MIC_X100=m"
else
KCONFIG += "CONFIG_INTEL_MIC_X100_DMA=n"
KCONFIG += "CONFIG_INTEL_MIC_X100=n"
endif

INSTALL = install
INSTALL_d = $(INSTALL) -d
INSTALL_x = $(INSTALL)
INSTALL_f = $(INSTALL) -m644

.PHONY: all install modules clean
.PHONY: modules_install conf_install dev_install kdev_install
.PHONY: modules_sign

all: modules

install: modules_install conf_install dev_install kdev_install

modules_sign:
	mpss-modsign.sh --modules-dir $(CURDIR)

ifeq ($(BUILD_MODULES_SIGN),true)
modules_install: modules_sign
endif

modules clean: %:
	$(MAKE) -j $(PARALLEL) -C $(KERNEL_BUILD) M=$(CURDIR) $(KCONFIG) $*

modules_install: modules
	$(INSTALL_d) $(BUILD_ROOT)$(KERNEL_MODULES)
	$(INSTALL_f) $(shell find $(CURDIR) -name '*.ko') $(BUILD_ROOT)$(KERNEL_MODULES)

conf_install:
	$(INSTALL_d) $(BUILD_ROOT)$(MODULE_SYSCONFIG)/modprobe.d
	$(INSTALL_f) modprobe.conf $(BUILD_ROOT)$(MODULE_SYSCONFIG)/modprobe.d/mic_x200.conf
	$(INSTALL_d) $(BUILD_ROOT)$(MODULE_SYSCONFIG)/udev/rules.d
	$(INSTALL_f) udev-mic.rules $(BUILD_ROOT)$(MODULE_SYSCONFIG)/udev/rules.d/50-udev-mic_x200.rules
ifneq ($(BUILD_CARD),true)
	$(INSTALL_d) $(BUILD_ROOT)$(MODULE_SYSCONFIG)/modules-load.d
	$(INSTALL_f) modules-load.conf $(BUILD_ROOT)$(MODULE_SYSCONFIG)/modules-load.d/mic_x200.conf
endif

dev_install:
	$(INSTALL_d) $(BUILD_ROOT)$(MODULE_USER_HEADERS)
	$(INSTALL_f) mic/scif/scif_ioctl.h $(BUILD_ROOT)$(MODULE_USER_HEADERS)
	$(INSTALL_f) mic/vop/mic_ioctl.h $(BUILD_ROOT)$(MODULE_USER_HEADERS)
	$(INSTALL_f) mic/common/mic_common.h $(BUILD_ROOT)$(MODULE_USER_HEADERS)

kdev_install:
	$(INSTALL_d) $(BUILD_ROOT)$(MODULE_SYMVERS)
	$(INSTALL_f) Module.symvers $(BUILD_ROOT)$(MODULE_SYMVERS)/scif.symvers
	$(INSTALL_d) $(BUILD_ROOT)$(MODULE_KERNEL_HEADERS)
	$(INSTALL_f) mic/scif/scif.h $(BUILD_ROOT)$(MODULE_KERNEL_HEADERS)
	$(INSTALL_f) mic/scif/scif_ioctl.h $(BUILD_ROOT)$(MODULE_KERNEL_HEADERS)
