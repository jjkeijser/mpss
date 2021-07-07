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
ALL_CFLAGS += -Wno-write-strings $(USERWARNFLAGS)

FILES = mpssconfig.c sysfs.c genfs.c passwd.c micenv.c utilfuncs.c config_pm.c verify_bzimage.c netutils.c bootapi.c
HEADERS = mpssconfig.h libmpsscommon.h

.PHONY: all install

all: $(libmpssconfig_dev)

libmpssconfig.map: config_pm.c libmpssconfig.cfg
	gen-symver-map $< $@ -- $(filter-out $<, $^) -- $(CXX) $(ALL_CFLAGS)

$(libmpssconfig_dev): $(FILES) $(HEADERS)
	$(CXX) $(ALL_CFLAGS) -fpic -shared $(FILES) -o $@ -Wl,-soname,$(libmpssconfig_all)

install:
	$(INSTALL_d) $(DESTDIR)$(libdir)
	$(INSTALL_x) $(libmpssconfig_dev) $(DESTDIR)$(libdir)/$(libmpssconfig_all)
	ln -fs $(libmpssconfig_all) $(DESTDIR)$(libdir)/$(libmpssconfig_abi)
	ln -fs $(libmpssconfig_all) $(DESTDIR)$(libdir)/$(libmpssconfig_dev)
	$(INSTALL_d) $(DESTDIR)$(includedir)
	$(INSTALL_f) mpssconfig.h $(DESTDIR)$(includedir)/mpssconfig.h

clean:
	rm -f $(libmpssconfig_dev) $(libmpssconfig_abi) $(libmpssconfig_all) libmpssconfig.map
