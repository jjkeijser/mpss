# Copyright 2010-2017 Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2,
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# Disclaimer: The codes contained in these modules may be specific to
# the Intel Software Development Platform codenamed Knights Ferry,
# and the Intel product codenamed Knights Corner, and are not backward
# compatible with other Intel products. Additionally, Intel will NOT
# support the codes or instruction set in future products.
#
# Intel offers no warranty of any kind regarding the code. This code is
# licensed on an "AS IS" basis and Intel is not obligated to provide
# any support, assistance, installation, training, or other services
# of any kind. Intel is also not obligated to provide any updates,
# enhancements or extensions. Intel specifically disclaims any warranty
# of merchantability, non-infringement, fitness for any particular
# purpose, and any other warranty.
#
# Further, Intel disclaims all liability of any kind, including but
# not limited to liability for infringement of any proprietary rights,
# relating to the use of the code, even if Intel is notified of the
# possibility of such liability. Except as expressly stated in an Intel
# license agreement provided with this code and agreed upon with Intel,
# no license, express or implied, by estoppel or otherwise, to any
# intellectual property rights is granted herein.

MPSS_COMMIT ?= $(or $(shell sed -ne '2 p' .mpss-metadata 2>/dev/null), \
	$(error .mpss-metadata file is missing or incorrect))
MPSS_VERSION ?= $(or $(shell sed -ne '1 p' .mpss-metadata 2>/dev/null), \
	$(error .mpss-metadata file is missing or incorrect))
MPSS_BUILDNO ?= 0
export MPSS_COMMIT := $(MPSS_COMMIT)
export MPSS_VERSION := $(MPSS_VERSION)
export MPSS_BUILDNO := $(MPSS_BUILDNO)
export MPSS_BUILTBY := $(shell echo "`whoami`@`uname -n`")
export MPSS_BUILTON := $(shell date +'%F %T %z')

KERNEL_VERSION := $(shell uname -r)
KERNEL_SRC = /lib/modules/$(KERNEL_VERSION)/build

INSTALL = install
INSTALL_d = $(INSTALL) -d
INSTALL_x = $(INSTALL)
INSTALL_f = $(INSTALL) -m644

prefix = /usr/local
sysconfdir = $(prefix)/etc
includedir = $(prefix)/include

kmodinstalldir = /lib/modules/$(KERNEL_VERSION)
kmodincludedir = $(realpath $(KERNEL_SRC))/include/modules

# If building the host's driver for a MIC co-processor card, which card
# $(ARCH) it should support
export MIC_CARD_ARCH

.PHONY: all install modules
.PHONY: modules_install conf_install dev_install kdev_install

all: modules

install: modules_install conf_install kdev_install

modules modules_install: %:
	$(MAKE) -C $(KERNEL_SRC) M=$(CURDIR) $* \
		INSTALL_MOD_PATH=$(DESTDIR)

conf_install:
ifneq ($(MIC_CARD_ARCH),)
	$(INSTALL_d) $(DESTDIR)$(sysconfdir)/sysconfig/modules
	$(INSTALL_x) mic.modules $(DESTDIR)$(sysconfdir)/sysconfig/modules
	$(INSTALL_d) $(DESTDIR)$(sysconfdir)/modprobe.d
	$(INSTALL_f) mic.conf $(DESTDIR)$(sysconfdir)/modprobe.d
endif
	$(INSTALL_d) $(DESTDIR)$(sysconfdir)/udev/rules.d
	$(INSTALL_f) udev-mic.rules $(DESTDIR)$(sysconfdir)/udev/rules.d/50-udev-mic.rules

dev_install:
	$(INSTALL_d) $(DESTDIR)$(includedir)/mic
	$(INSTALL_f) include/scif_ioctl.h $(DESTDIR)$(includedir)
	$(INSTALL_f) include/mic/io_interface.h $(DESTDIR)$(includedir)/mic
	$(INSTALL_f) include/mic/mic_pm.h $(DESTDIR)$(includedir)/mic
	$(INSTALL_f) ras/micras_api.h $(DESTDIR)$(includedir)/mic
	$(INSTALL_f) ras/micmca_api.h $(DESTDIR)$(includedir)/mic
ifeq ($(MIC_CARD_ARCH),) # Card side
	$(INSTALL_f) ras/micpm_api.h $(DESTDIR)$(includedir)/mic
	$(INSTALL_f) ras/micras.h $(DESTDIR)$(includedir)/mic
else # Host side
	$(INSTALL_f) include/mic/micbaseaddressdefine.h $(DESTDIR)$(includedir)/mic
	$(INSTALL_f) include/mic/micsboxdefine.h $(DESTDIR)$(includedir)/mic
	$(INSTALL_f) include/mic/micdboxdefine.h $(DESTDIR)$(includedir)/mic
	$(INSTALL_f) ras/micpm_api.h $(DESTDIR)$(includedir)/mic
endif

kdev_install:
	$(INSTALL_d) $(DESTDIR)$(kmodinstalldir)
	$(INSTALL_f) Module.symvers $(DESTDIR)$(kmodinstalldir)/scif.symvers
	$(INSTALL_d) $(DESTDIR)$(kmodincludedir)
	$(INSTALL_f) include/scif.h $(DESTDIR)$(kmodincludedir)
