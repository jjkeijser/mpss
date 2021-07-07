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

INSTALL = install
INSTALL_d = $(INSTALL) -d
INSTALL_x = $(INSTALL)
INSTALL_f = $(INSTALL) -m 644

prefix = /usr
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
sbindir = $(exec_prefix)/sbin
datarootdir = $(prefix)/share
sysconfdir = $(prefix)/etc

mpssconf = $(sysconfdir)/mpss
logrotate = $(sysconfdir)/logrotate.d
CFLAGS ?= -g
LDFLAGS ?= -g
LDLIBS = -pthread -L../libmpssconfig -lmpssconfig -lscif

ALL_CFLAGS = $(CFLAGS) -rdynamic -I../libmpssconfig -pthread
ALL_LDFLAGS = $(LDFLAGS) -rdynamic

# Makes it easy to inject "-Wall -Werror" from the environment
ALL_CFLAGS += -Wno-write-strings $(USERWARNFLAGS)

FILES = mpssd.c

HEADERS = ../libmpssconfig/libmpsscommon.h ../libmpssconfig/mpssconfig.h
PROGRAMS = mpssd

.PHONY: all install clean $(PROGRAMS)

all: $(PROGRAMS)

install: $(PROGRAMS)
	$(INSTALL_d) $(DESTDIR)$(sbindir)
	$(INSTALL_x) mpssd $(DESTDIR)$(sbindir)
	$(INSTALL_d) $(DESTDIR)$(mpssconf)/
	$(INSTALL_x) mpss.common $(DESTDIR)$(mpssconf)
	$(INSTALL_x) mpss.redhat $(DESTDIR)$(mpssconf)
	$(INSTALL_x) mpss.service $(DESTDIR)$(mpssconf)
	$(INSTALL_x) mpss.suse $(DESTDIR)$(mpssconf)
	$(INSTALL_x) mpss.ubuntu $(DESTDIR)$(mpssconf)
	$(INSTALL_x) mpss.suspend.service $(DESTDIR)$(mpssconf)
	$(INSTALL_x) mpss.suspend $(DESTDIR)$(mpssconf)
	$(INSTALL_x) mpss.resume $(DESTDIR)$(mpssconf)
	$(INSTALL_d) $(DESTDIR)$(logrotate)/
	$(INSTALL_f) mpssd.logrotate $(DESTDIR)$(logrotate)/mpssd

clean:
	rm -f $(PROGRAMS) $(FILES:%.c=%.o)

$(PROGRAMS): mpssd.o
	$(CXX) $(ALL_LDFLAGS) $^ $(LDLIBS) -o $@

$(FILES:%.c=%.o): %.o: %.c $(HEADERS)
	$(CXX) $(ALL_CFLAGS) $< -c -o $@
