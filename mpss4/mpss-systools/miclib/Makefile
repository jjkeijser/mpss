# Copyright (c) 2017, Intel Corporation.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
# 
# This program is distributed in the hope it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details.

TOPDIR ?= ..

ALL_CPPFLAGS = -I$(TOPDIR)/common-framework/include -I./include -I$(TOPDIR)/sdk/include -I./src -I$(TOPDIR)/apps/systoolsd/shared $(CPPFLAGS) -fPIC
UT_ALL_CPPFLAGS = $(ALL_CPPFLAGS) -I$(TOPDIR)/sdk/src -I$(TOPDIR)/common-framework/src -DUNIT_TESTS -DMICMGMT_MOCK_ENABLE

COMMON_LDFLAGS = -L$(TOPDIR)/common-framework/lib -L$(TOPDIR)/sdk/lib -lpthread $(LDFLAGS)
ALL_LDFLAGS = $(COMMON_LDFLAGS) -ldl -lsystools-sdk -lsystools-cf
UT_ALL_LDFLAGS = $(COMMON_LDFLAGS) -lgtest -lsystools-sdk-ut -lsystools-cf-ut

ifdef DEBUG
   ALL_CPPFLAGS += -g
endif

LIBNAME = systools
LIBMAJOR = 0
LIBMINOR = 1

LIBSOURCES = \
	src/libsystools.cpp \
	src/libsystools_p.cpp

UT_SOURCES = \
	ut/LibsystoolsUt.cpp

-include $(TOPDIR)/common.mk

EXAMPLE_DIR := $(DESTDIR)$(DOCDIR)/systools/examples

examples_install:
	$(INSTALL_d) $(EXAMPLE_DIR)
	$(INSTALL_r) examples/*.c $(EXAMPLE_DIR)
	$(INSTALL_r) examples/Makefile_example $(EXAMPLE_DIR)/Makefile

docs_install:
	$(INSTALL_d) $(DESTDIR)$(MANDIR)/man3
	SYSTOOLS_DOCROOT=$(TOPDIR)/dox DATAROOTDIR=$(DATAROOTDIR) $(DOXYGEN) dox/libsystools_man.dox

install: docs_install examples_install

.PHONY: all docs_install examples_install install
