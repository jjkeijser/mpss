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
LDLIBS = -pthread -L../libmpssconfig -lmpssconfig -lscif -ldl

ALL_CFLAGS = $(CFLAGS) -fpie -rdynamic -I../libmpssconfig -pthread
ALL_LDFLAGS = $(LDFLAGS) -pie -rdynamic

# Makes it easy to inject "-Wall -Werror" from the environment
ALL_CFLAGS += $(USERWARNFLAGS)

FILES = virtio.cpp monitor.cpp mpssd.cpp utils.cpp sync_utils.cpp

HEADERS = ../libmpssconfig/libmpsscommon.h ../libmpssconfig/mpssconfig.h
PROGRAMS = mpssd

.PHONY: all install clean $(PROGRAMS)

all: $(PROGRAMS)

install: $(PROGRAMS)
	$(INSTALL_d) $(DESTDIR)/var/mpss
	$(INSTALL_d) $(DESTDIR)/lib/firmware/mic
	$(INSTALL_d) $(DESTDIR)$(bindir)
	$(INSTALL_x) mpssd $(DESTDIR)$(bindir)
	$(INSTALL_d) $(DESTDIR)$(mpssconf)/
	$(INSTALL_d) $(DESTDIR)/usr/lib/systemd/system
	$(INSTALL_f) mpss.service $(DESTDIR)/usr/lib/systemd/system
	$(INSTALL_d) $(DESTDIR)$(logrotate)/
	$(INSTALL_f) mpssd.logrotate $(DESTDIR)$(logrotate)/mpssd

clean:
	rm -f $(PROGRAMS) $(FILES:%.cpp=%.o)

$(PROGRAMS): virtio.o monitor.o mpssd.o utils.o sync_utils.o
	$(CXX) -std=c++11 $(ALL_LDFLAGS) $^ $(LDLIBS) -o $@

$(FILES:%.cpp=%.o): %.o: %.cpp $(HEADERS)
	$(CXX) -std=c++11 $(ALL_CFLAGS) $< -c -o $@
