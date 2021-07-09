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
LDLIBS = -L../libmpssconfig -lmpssconfig -lscif -lboost_iostreams -lboost_program_options

ALL_CFLAGS = $(CFLAGS) -fpie -rdynamic -I../libmpssconfig -pthread
ALL_LDFLAGS = $(LDFLAGS) -pie -rdynamic

# Makes it easy to inject "-Wall -Werror" from the environment
ALL_CFLAGS += $(USERWARNFLAGS)

FILES = micctrl.c init.c rootdev.c state.c misc.c config.c utilfuncs.c help.c
OBJS = micctrl.o init.o rootdev.o state.o misc.o config.o utilfuncs.o help.o

HEADERS = ../libmpssconfig/libmpsscommon.h ../libmpssconfig/mpssconfig.h micctrl.h help.h
PROGRAMS = micctrl 

.PHONY: all install clean $(PROGRAMS)

all: $(PROGRAMS)

install: $(PROGRAMS)
	$(INSTALL_d) $(DESTDIR)$(bindir)
	$(INSTALL_x) micctrl $(DESTDIR)$(bindir)

clean:
	rm -f $(PROGRAMS) $(FILES:%.c=%.o) 

micctrl: $(OBJS)

$(PROGRAMS): 
	$(CXX) -std=c++11 $(ALL_LDFLAGS) $^ $(LDLIBS) -o $@

$(FILES:%.c=%.o) micctrl_passwd.o: %.o: %.c $(HEADERS)
	$(CXX) -std=c++11 $(ALL_CFLAGS) $< -c -o $@
