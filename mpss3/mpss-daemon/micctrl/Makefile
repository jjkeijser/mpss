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
INSTALL_x = $(INSTALL) -m 755
INSTALL_s = $(INSTALL) -m 4755
INSTALL_f = $(INSTALL) -m 644

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
sbindir = $(exec_prefix)/sbin
sysconfdir = $(prefix)/etc

mpssconf = $(sysconfdir)/mpss

CFLAGS ?= -g
LDFLAGS ?= -g
LDLIBS = -lcrypt -L../libmpssconfig -lmpssconfig -lscif #-lncurses

ALL_CFLAGS = $(CFLAGS) -Wno-write-strings -rdynamic -I../libmpssconfig -pthread
ALL_LDFLAGS = $(LDFLAGS) -rdynamic

# Makes it easy to inject "-Wall -Werror" from the environment
ALL_CFLAGS += $(USERWARNFLAGS)

FILES = micctrl.c init.c network.c rootdev.c state.c user.c power.c dirs.c misc.c \
	config.c utilfuncs.c help.c mpsscookie.c
OBJS = micctrl.o init.o network.o rootdev.o state.o user.o power.o dirs.o misc.o \
	config.o utilfuncs.o help.o mpsscookie.o

HEADERS = ../libmpssconfig/libmpsscommon.h ../libmpssconfig/mpssconfig.h micctrl.h help.h
PROGRAMS = micctrl micctrl_passwd

.PHONY: all install clean $(PROGRAMS)

all: $(PROGRAMS)

install: $(PROGRAMS)
	$(INSTALL_d) $(DESTDIR)$(mpssconf)/conf.d
	$(INSTALL_d) $(DESTDIR)$(sbindir)
	$(INSTALL_x) micctrl $(DESTDIR)$(sbindir)
	$(INSTALL_s) micctrl_passwd $(DESTDIR)$(sbindir)

clean:
	rm -f $(PROGRAMS) $(FILES:%.c=%.o) micctrl_passwd.o

micctrl: $(OBJS)
micctrl_passwd: micctrl_passwd.o utilfuncs.o mpsscookie.o

$(PROGRAMS): 
	$(CXX) $(ALL_LDFLAGS) $^ $(LDLIBS) -o $@

$(FILES:%.c=%.o) micctrl_passwd.o: %.o: %.c $(HEADERS)
	$(CXX) $(ALL_CFLAGS) $< -c -o $@
