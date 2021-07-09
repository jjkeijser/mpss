# Copyright (c) 2016, Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details.

# COSAI BEGIN BLOCK
USERWARNFLAGS ?= -Wall -Werror
# COSAI END BLOCK -

CFLAGS ?= -g
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
CC = $(CROSS_COMPILE)gcc
CPP = $(CC) -E
AR = $(CROSS_COMPILE)ar
NM = $(CROSS_COMPILE)nm
STRIP = $(CROSS_COMPILE)strip
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
INSTALL = install
INSTALL_d = $(INSTALL) -d
INSTALL_x = $(INSTALL)
INSTALL_f = $(INSTALL) -m 644

prefix = /usr/local
exec_prefix = $(prefix)
datarootdir = $(prefix)/share
includedir = $(prefix)/include
docdir = $(datarootdir)/doc/libscif-$(libscif_major).$(libscif_minor)
pdfdir = $(docdir)
libdir = $(exec_prefix)/lib
mandir = $(datarootdir)/man
man3dir = $(mandir)/man3

TARGET_ARCH := $(shell $(CC) -dumpmachine | sed -n 's/.*\b\([lk]1om\)\b.*/\1/p')

ALL_CFLAGS_k1om = -D_MIC_SCIF_
ALL_CFLAGS_l1om = -D_MIC_SCIF_
ALL_CFLAGS += -O2 $(CFLAGS) $(ALL_CFLAGS_$(TARGET_ARCH)) -I$(CURDIR) $(CPPFLAGS)

# Makes it easy to inject "-Wall -Werror" from the environment
ALL_CFLAGS += $(USERWARNFLAGS)

libscif_major := 0
libscif_minor := 0.1
libscif_dev := libscif.so
libscif_abi := libscif.so.$(libscif_major)
libscif_all := libscif.so.$(libscif_major).$(libscif_minor)

.PHONY: all lib pdf install install-pdf docs

all: $(libscif_dev) docs

libscif.map: libscif.cfg scif_api.c
	./gen-symver-map $< $@ -- $(filter-out $<, $^) -- $(CC) $(ALL_CFLAGS)

$(libscif_dev): scif_api.c libscif.map
	$(CC) $(ALL_CFLAGS) -fpic -shared $< $(MPSS_METADATA_BRAND) -o $@ \
		-Wl,-soname,$(libscif_abi) \
		-Wl,--version-script=libscif.map

pdf:
	@echo create PDF...

A2X = a2x
XSLTPROC_ARGS_FOR_A2X=--xsltproc-opts='--catalogs'

SCIFMANDIR = man
UMANDIR = $(SCIFMANDIR)/man_3
UMANSRCS = $(wildcard $(UMANDIR)/*.txt)
UMANPAGES = $(UMANSRCS:.txt=.3)

docs: $(UMANPAGES)

%.3:%.txt
	$(A2X) --doctype manpage -D $(UMANDIR) --format  manpage --attribute=scif_user $(XSLTPROC_ARGS_FOR_A2X) $<

install:
	$(INSTALL_d) $(DESTDIR)$(libdir)
	$(INSTALL_x) $(libscif_dev) $(DESTDIR)$(libdir)/$(libscif_all)
	ln -s $(libscif_all) $(DESTDIR)$(libdir)/$(libscif_abi)
	ln -s $(libscif_all) $(DESTDIR)$(libdir)/$(libscif_dev)
	$(INSTALL_d) $(DESTDIR)$(includedir)
	$(INSTALL_f) scif.h $(DESTDIR)$(includedir)/scif.h
	$(INSTALL_d) $(DESTDIR)$(man3dir)
	$(INSTALL_f) $(UMANDIR)/*.3 $(DESTDIR)$(man3dir)

install-pdf:
	@echo install PDF...
clean:
	rm -f $(UMANDIR)/*.3
	rm -f $(UMANDIR)/*.xml
