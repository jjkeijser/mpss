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

VERSION = $(Version)
ifeq ($(VERSION), )
	VERSION = 0.0.0
endif

ALL_CPPFLAGS = -I./include -I./src -fPIC $(CPPFLAGS) -DBUILD_VERSION=\"$(VERSION)\"
UT_ALL_CPPFLAGS = $(ALL_CPPFLAGS) -DUNIT_TESTS \
-Wno-sign-compare # Prevent a known issue in gtest

ALL_LDFLAGS = $(LDFLAGS) -lpthread -ldl
UT_ALL_LDFLAGS = $(ALL_LDFLAGS) -L./lib -lgtest -lgmock

ifdef DEBUG
	ALL_CPPFLAGS += -g
endif

LIBNAME = systools-cf

LIBSOURCES = \
	src/MicOutputFormatterBase.cpp \
	src/ConsoleOutputFormatter.cpp \
	src/MicException.cpp \
	src/micmgmtCommon.cpp \
	src/ToolSettings.cpp \
	src/MicOutput.cpp \
	src/MicOutputImpl.cpp \
	src/CliParser.cpp \
	src/CliParserImpl.cpp \
	src/NameHelp_p.cpp \
	src/ArgumentDef_p.cpp \
	src/OptionDef_p.cpp \
	src/SubcommandDef_p.cpp \
	src/CliParserExitException_p.cpp \
	src/XmlOutputFormatter.cpp \
	src/MicErrorBase.cpp \
	src/CliParserError_p.cpp \
	src/MicOutputError_p.cpp \
	src/InternalLogger.cpp \
	src/MicLogger.cpp \
	src/MsTimer.cpp \
	src/SafeBool.cpp \
	src/ThreadPoolImpl.cpp \
	src/ThreadPool.cpp \
	src/SharedLibrary.cpp

UT_SOURCES = \
	ut/micmgmtCommonUt.cpp \
	ut/MicExceptionUt.cpp \
	ut/ConsoleOutputFormatterUt.cpp \
	ut/ToolSettingsUt.cpp \
	ut/MicOutputUt.cpp \
	ut/NameHelpUt.cpp \
	ut/ArgumentDefUt.cpp \
	ut/OptionDefUt.cpp \
	ut/SubcommandDefUt.cpp \
	ut/CliParserUt.cpp \
	ut/XmlOutputFormatterUt.cpp \
	ut/MicErrorBaseUt.cpp \
	ut/MicOutputErrorUt.cpp \
	ut/CliParserErrorUt.cpp \
	ut/MicLoggerUt.cpp \
	ut/SafeBoolUt.cpp \
	ut/MsTimerUt.cpp \
	ut/ThreadPoolUt.cpp

-include $(TOPDIR)/common.mk

clean: cleandocs

cleandocs:
	$(RM) -f docs

docs: src/*.hpp src/*.cpp src/*.txt include/*.hpp
	$(RM) -rf docs
	make -C ./dox -f Makefile docs
