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

CFLAGS = -g
INSTALL = install
INSTALL_d = $(INSTALL) -d
INSTALL_x = $(INSTALL)
INSTALL_f = $(INSTALL) -m 644

prefix = /usr
exec_prefix = $(prefix)
includedir = $(prefix)/include/mic
libdir = $(prefix)/lib64

mpssconf = $(sysconfdir)/mpss

libmpssconfig_major := 0
libmpssconfig_minor := 0.1
libmpssconfig_dev := libmpssconfig.so
libmpssconfig_abi := libmpssconfig.so.$(libmpssconfig_major)
libmpssconfig_all := libmpssconfig.so.$(libmpssconfig_major).$(libmpssconfig_minor)

ALL_CFLAGS += $(CFLAGS) -rdynamic -I$(CURDIR)

# Makes it easy to inject "-Wall -Werror" from the environment
ALL_CFLAGS += $(USERWARNFLAGS)

FILES = mpssconfig.c sysfs.c micenv.c utilfuncs.c bootapi.c
HEADERS = mpssconfig.h libmpsscommon.h mic.h

.PHONY: all install

all: $(libmpssconfig_dev)

$(libmpssconfig_dev): $(FILES) $(HEADERS)
	$(CXX) -std=c++11 -lboost_regex $(ALL_CFLAGS) -fpic -shared $(FILES) -o $@ -Wl,-soname,$(libmpssconfig_all)

install:
	$(INSTALL_d) $(DESTDIR)$(libdir)
	$(INSTALL_x) $(libmpssconfig_dev) $(DESTDIR)$(libdir)/$(libmpssconfig_all)
	ln -fs $(libmpssconfig_all) $(DESTDIR)$(libdir)/$(libmpssconfig_abi)
	ln -fs $(libmpssconfig_all) $(DESTDIR)$(libdir)/$(libmpssconfig_dev)

clean:
	rm -f $(libmpssconfig_dev) $(libmpssconfig_abi) $(libmpssconfig_all)
