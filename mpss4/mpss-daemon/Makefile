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

prefix = /usr
exec_prefix = $(prefix)
includedir = $(prefix)/include/mic
libdir = $(prefix)/lib64
sysconfdir = /etc

CFLAGS += -fstack-protector -Wall -fvisibility=hidden -g -O2 $(CPPFLAGS)
LDFLAGS += -Wl,-z,relro,-z,now

all:
	cd libmpssconfig; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" DESTDIR=$(DESTDIR)
	cd mpssd; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" DESTDIR=$(DESTDIR)
	cd micctrl; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" DESTDIR=$(DESTDIR)

install:
	cd libmpssconfig; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" DESTDIR=$(DESTDIR) install
	cd mpssd; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" DESTDIR=$(DESTDIR) install
	cd micctrl; make prefix=$(prefix) exec_prefix=$(exec_prefix) sysconfdir=$(sysconfdir) \
		includedir=$(includedir) libdir=$(libdir) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" DESTDIR=$(DESTDIR) install

clean:
	cd libmpssconfig; make clean
	cd mpssd; make clean
	cd micctrl; make clean

