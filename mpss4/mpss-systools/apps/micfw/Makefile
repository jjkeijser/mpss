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
ALL_LDFLAGS = $(COMMON_LDFLAGS) -ldl -lsystools-sdk -lsystools-cf -lcrypto
UT_ALL_LDFLAGS = $(COMMON_LDFLAGS) -lgtest -lsystools-sdk-ut -lsystools-cf-ut -lcrypto

ifdef DEBUG
        ALL_CPPFLAGS += -g
endif

EXENAME = micfw

COMMON_SOURCES = \
	src/Application.cpp \
	src/CountdownMutex.cpp \
	src/FlashFileVersion.cpp \
	src/FlashWorkItem.cpp \
	src/KnxFlashing.cpp \
	src/MicFwError.cpp

SOURCES = \
	$(COMMON_SOURCES) \
	src/main.cpp

UT_SOURCES = \
	$(COMMON_SOURCES) \
	ut/CommonUt.cpp \
	ut/KnlDeviceVersion.cpp \
	ut/KnlFileVersion.cpp \
	ut/KnlHelp.cpp \
	ut/KnlUpdate.cpp \
	ut/Version.cpp \
	ut/KnlUnitTestDevice.cpp

-include $(TOPDIR)/common.mk

install: installscripts

installscripts: $(EXECUTABLE)
	$(INSTALL_d) $(DESTDIR)/$(BINDIR)
	$(INSTALL_x) scripts/micflash.py $(DESTDIR)/$(BINDIR)
	ln -f -s micflash.py $(DESTDIR)/$(BINDIR)/micflash