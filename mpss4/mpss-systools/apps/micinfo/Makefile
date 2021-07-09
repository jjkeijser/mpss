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

TOPDIR ?= ../..

ALL_CPPFLAGS = -I$(TOPDIR)/common-framework/include -I$(TOPDIR)/sdk/include -I./include $(CPPFLAGS)
UT_ALL_CPPFLAGS = $(ALL_CPPFLAGS) -I$(TOPDIR)/sdk/src -I$(TOPDIR)/common-framework/src -I./ut -DUNIT_TESTS -DMICMGMT_MOCK_ENABLE

COMMON_LDFLAGS = $(LDFLAGS) -L$(TOPDIR)/common-framework/lib -L$(TOPDIR)/sdk/lib -lpthread
ALL_LDFLAGS = $(COMMON_LDFLAGS) -ldl -lsystools-sdk -lsystools-cf
UT_ALL_LDFLAGS = $(COMMON_LDFLAGS) -lgtest -lsystools-sdk-ut -lsystools-cf-ut

ifdef DEBUG
	ALL_CPPFLAGS += -g
endif

EXENAME = micinfo

COMMON_SOURCES = \
	src/utilities.cpp

SOURCES = \
	$(COMMON_SOURCES) \
	src/main.cpp

UT_SOURCES = \
	src/utilities.cpp \
	ut/ut-utilities.cpp \
	ut/micinfo-ut.cpp

-include $(TOPDIR)/common.mk
