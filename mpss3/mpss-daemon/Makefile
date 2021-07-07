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

prefix = /usr
exec_prefix = $(prefix)
includedir = $(prefix)/include/mic
libdir = $(prefix)/lib64
sysconfdir = /etc

all:
	cd libmpssconfig; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" DESTDIR=$(DESTDIR)
	cd mpssd; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" DESTDIR=$(DESTDIR)
	cd micctrl; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" DESTDIR=$(DESTDIR)

install:
	cd libmpssconfig; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" DESTDIR=$(DESTDIR) install
	cd mpssd; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" DESTDIR=$(DESTDIR) install
	cd micctrl; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" DESTDIR=$(DESTDIR) install

clean:
	cd libmpssconfig; make clean
	cd mpssd; make clean
	cd micctrl; make clean

