# Copyright (c) 2017, Intel Corporation.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
# 
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.

TOPDIR ?= ../..

ALL_CPPFLAGS = -I./include -I./shared -I./cf -DGTEST_LANG_CXX11=0 $(CPPFLAGS)

#no-sign-compare required by gtest, no-unused-parameter required because of NO_SYSLOG flags for UTs.
UT_ALL_CPPFLAGS = $(ALL_CPPFLAGS) -Wno-sign-compare -Wno-unused-parameter \
				  -I./ut -DUNIT_TESTS -DNO_SYSLOG

COMMON_LDFLAGS = $(LDFLAGS) -L$(TOPDIR)/common-framework/lib -lpthread
ALL_LDFLAGS =  $(COMMON_LDFLAGS) -lscif
UT_ALL_LDFLAGS = $(COMMON_LDFLAGS) -lgtest -lgmock

EXENAME = systoolsd

COMMON_SOURCES = \
	src/PWindow.cpp \
	src/SystoolsdException.cpp \
	src/SystoolsdServices.cpp \
	src/PThresh.cpp \
	src/ScifEp.cpp \
	src/I2cAccess.cpp \
	src/I2cBase.cpp \
	src/I2cToolsImpl.cpp \
	src/FileInterface.cpp \
	src/Daemon.cpp \
	src/utils.cpp \
	src/daemonlog.cpp \
	src/Popen.cpp \
	src/Syscfg.cpp \
	src/TurboSettings.cpp \
	src/DaemonSession.cpp \
	src/FwUpdateInfoGroup.cpp \
	src/PowerUsageInfoGroup.cpp \
	src/KernelInfo.cpp \
	src/MemoryUsageInfoGroup.cpp \
	src/DiagnosticsInfoGroup.cpp \
	src/SmbaInfoGroup.cpp \
	src/ProcessorInfoGroup.cpp \
	src/CoreUsageInfoGroup.cpp \
	src/PowerThresholdsInfoGroup.cpp \
	src/ProcStatCore.cpp \
	src/ThermalInfoGroup.cpp \
	src/VoltageInfoGroup.cpp \
	src/DeviceInfoGroup.cpp \
	src/CoresInfoGroup.cpp \
	src/MemoryInfoGroup.cpp \
	src/TurboInfoGroup.cpp \
	src/SystoolsdInfoGroup.cpp \
	src/RequestHandlerBase.cpp \
	src/RestartSmba.cpp \
	src/MicBiosRequestHandler.cpp \
	src/PThreshRequestHandler.cpp \
	src/SetRequestHandler.cpp \
	src/RequestHandler.cpp \
	src/TurboRequestHandler.cpp \
	src/SmcRwHandler.cpp \
	src/BiosInfoStructure.cpp \
	src/MemoryDeviceStructure.cpp \
	src/ProcessorInfoStructure.cpp \
	src/SmBiosInfo.cpp \
	src/SmBiosStructureBase.cpp \
	src/SystemInfoStructure.cpp \
	src/EntryPointMemoryScan.cpp \
	src/EntryPointEfi.cpp \
	cf/SafeBool.cpp \
	cf/ThreadPool.cpp \
	cf/ThreadPoolImpl.cpp \
	cf/MsTimer.cpp \
	cf/micmgmtCommon.cpp \
	cf/ToolSettings.cpp \
	cf/MicLogger.cpp \
	cf/InternalLogger.cpp

ifdef HOST_TEST
	COMMON_SOURCES += src/FakePWindow.cpp \
		src/FakePThresh.cpp \
		src/FakeI2c.cpp \
		src/FakeTurbo.cpp \
		src/FakeSyscfg.cpp \
		src/FakeSmBiosInfo.cpp
	ALL_CPPFLAGS += -DSYSTOOLSD_HOST_TEST $(shell python config_fakes.py fakes.config)
endif

ifdef DEBUG
	ALL_CPPFLAGS += -DNO_SYSLOG -DSYSTOOLSD_DEBUG -g
endif

SOURCES = \
	$(COMMON_SOURCES) \
	src/ScifImpl.cpp \
	main.cpp

UT_SOURCES = \
	$(COMMON_SOURCES) \
	ut/ut_utils.cpp \
	ut/ScifEpUt.cpp \
	ut/TestDataGroup.cpp \
	ut/CachedDataGroupUt.cpp \
	ut/MemoryUsageInfoGroupUt.cpp \
	ut/I2cAccessUt.cpp \
	ut/DaemonUt.cpp \
	ut/PThreshUt.cpp \
	ut/SyscfgUt.cpp \
	ut/InfoGroupsUt.cpp \
	ut/RequestHandlersUt.cpp \
	ut/TurboSettingsUt.cpp \
	ut/PopenUt.cpp \
	ut/ProcStatCoreUt.cpp \
	ut/FileUt.cpp \
	ut/SmBiosStructuresUt.cpp \
	ut/SmBiosInfoTest.cpp \
	ut/EntryPointFindersUt.cpp \
	cf/ut/MicLoggerUt.cpp \
	cf/ut/MicLoggerUt.cpp \
	cf/ut/micmgmtCommonUt.cpp \
	cf/ut/MsTimerUt.cpp \
	cf/ut/SafeBoolUt.cpp \
	cf/ut/ThreadPoolUt.cpp \
	cf/ut/ToolSettingsUt.cpp

-include $(TOPDIR)/common.mk

sample_client_objs = src/ScifEp.o src/ScifImpl.o

sample_client : sample_client.cpp $(OBJECTS)
	$(CXX) $(STDCPP) $(ALL_CPPFLAGS) $< -lscif $(sample_client_objs) -o $@

clean_sample_client :
	$(RM) sample_client

clean_objs :
	find . -name "*.o" -o -name "*.d" -type f -delete

clean : clean_sample_client clean_objs

local_test : $(UT_EXECUTABLE)
	$(UT_EXECUTABLE) --gtest_filter=-"common_framework*" --gtest_color=no --gtest_shuffle

install : $(EXECUTABLE)
	$(INSTALL_d) $(DESTDIR)/$(INITDDIR)
	$(INSTALL_x) init/$(EXENAME) $(DESTDIR)/$(INITDDIR)
	$(INSTALL_d) $(DESTDIR)/$(SBINDIR)
	$(INSTALL_x) $(EXECUTABLE) $(DESTDIR)/$(SBINDIR)
