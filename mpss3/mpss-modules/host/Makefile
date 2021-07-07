#
#  Manycore Throughput Linux Driver
#  Copyright (c) 2010, Intel Corporation.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms and conditions of the GNU General Public License,
#  version 2, as published by the Free Software Foundation.
#
#  This program is distributed in the hope it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
#  more details.
#
#  You should have received a copy of the GNU General Public License along with
#  this program; if not, write to the Free Software Foundation, Inc.,
#  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#
#

KERNELDIR = /lib/modules/$(shell uname -r)/build
KBUILD := $(MAKE) -C $(KERNELDIR) M=$(CURDIR)
EXTRADIR = $(shell readlink -f $(KERNELDIR))

ifneq ($(DESTDIR),)
INSTALL_MOD_PATH = $(DESTDIR)
endif

.PHONY: default modules install modules_install clean

default: modules
install: modules_install udev

modules:
	+$(KBUILD) $@

modules_install:
	+$(KBUILD) INSTALL_MOD_PATH=$(DESTDIR) modules_install
	mkdir -p $(DESTDIR)$(EXTRADIR)/include
	install -m644 include/scif.h $(DESTDIR)$(EXTRADIR)/include
	install -m644 Module.symvers $(DESTDIR)$(EXTRADIR)/Module.symvers.mic

udev: udev-scif.rules
	mkdir -p $(DESTDIR)/etc/udev/rules.d
	cp $< $(DESTDIR)/etc/udev/rules.d/50-$<

clean:
	+$(KBUILD) clean
