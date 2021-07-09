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

ALL_CPPFLAGS = -I$(TOPDIR)/common-framework/include -I$(TOPDIR)/sdk/include -I./src -I$(TOPDIR)/apps/systoolsd/shared $(CPPFLAGS)
UT_ALL_CPPFLAGS = $(ALL_CPPFLAGS) -I$(TOPDIR)/sdk/src -I$(TOPDIR)/common-framework/src -DUNIT_TESTS -DMICMGMT_MOCK_ENABLE

COMMON_LDFLAGS = -L$(TOPDIR)/common-framework/lib -L$(TOPDIR)/sdk/lib -lpthread $(LDFLAGS)
ALL_LDFLAGS = $(COMMON_LDFLAGS) -ldl -lsystools-sdk -lsystools-cf
UT_ALL_LDFLAGS = $(COMMON_LDFLAGS) -lgtest -lsystools-sdk-ut -lsystools-cf-ut

EXENAME = micsmc-cli

ifdef DEBUG
	ALL_CPPFLAGS += -g
endif

COMMON_SOURCES = \
	src/CliApplication.cpp \
	src/MicSmcCliError.cpp \
	src/ChangeSettingsWorkItem.cpp

SOURCES = \
	$(COMMON_SOURCES) \
	src/main.cpp

UT_SOURCES = \
	$(COMMON_SOURCES) \
	ut/CommonUt.cpp \
	ut/KnxMockUtDevice.cpp \
	ut/GeneralUt.cpp \
	ut/DisplayDataUt.cpp \
	ut/StatusUt.cpp \
	ut/ChangeSettingsUt.cpp

-include $(TOPDIR)/common.mk
