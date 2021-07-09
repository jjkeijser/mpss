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

ALL_CPPFLAGS = -I./include -I./src -I$(TOPDIR)/common-framework/include -I$(TOPDIR)/apps/systoolsd/shared -I$(TOPDIR)/3rd-party/linux/drivers/mpss3/ -fPIC $(CPPFLAGS)
UT_ALL_CPPFLAGS = $(ALL_CPPFLAGS) -DUNIT_TESTS -DMICMGMT_MOCK_ENABLE -DGTEST_LANG_CXX11=0 -Wno-conversion-null -Wno-sign-compare

ALL_LDFLAGS = $(LDFLAGS) -L./lib -L$(TOPDIR)/common-framework/lib -lpthread
UT_ALL_LDFLAGS = $(ALL_LDFLAGS) -ldl -lgtest -lgmock -lsystools-cf-ut

ifdef DEBUG
	ALL_CPPFLAGS += -g
endif

LIBNAME = systools-sdk

LIBSOURCES = \
	src/FATFileReaderBase.cpp \
	src/FATFileReader.cpp \
	src/FlashDeviceInfo.cpp \
	src/FlashHeaderBase.cpp \
	src/FlashImage.cpp \
	src/FlashImageFile.cpp \
	src/FlashStatus.cpp \
	src/KnlDeviceBase.cpp \
	src/KnlFlashHeader.cpp \
	src/KnlMockDevice.cpp \
	src/KnlMockDevice2.cpp \
	src/FATMockFileReader.cpp \
	src/MicBootConfigInfo.cpp \
	src/MicCoreInfo.cpp \
	src/MicCoreUsageInfo.cpp \
	src/MicDevice.cpp \
	src/MicDeviceDetails.cpp \
	src/MicDeviceError.cpp \
	src/MicDeviceFactory.cpp \
	src/MicDeviceImpl.cpp \
	src/MicDeviceInfo.cpp \
	src/MicDeviceManager.cpp \
	src/MicFrequency.cpp \
	src/MicMemory.cpp \
	src/MicMemoryInfo.cpp \
	src/MicMemoryUsageInfo.cpp \
	src/MicPciConfigInfo.cpp \
	src/MicPlatformInfo.cpp \
	src/MicPower.cpp \
	src/MicPowerThresholdInfo.cpp \
	src/MicPowerState.cpp \
	src/MicPowerUsageInfo.cpp \
	src/MicProcessorInfo.cpp \
	src/MicSampleBase.cpp \
	src/MicSpeed.cpp \
	src/MicTemperature.cpp \
	src/MicThermalInfo.cpp \
	src/MicVersionInfo.cpp \
	src/MicVoltage.cpp \
	src/MicVoltageInfo.cpp \
	src/Mpss3StackBase.cpp \
	src/Mpss3StackMock.cpp \
	src/MpssStackBase.cpp \
	src/Mpss3StackLinux.cpp \
	src/Mpss4StackBase.cpp \
	src/Mpss4StackMock.cpp \
	src/Mpss4StackLinux.cpp \
	src/PciAddress.cpp \
	src/ScifConnectionBase.cpp \
	src/ScifRequest.cpp \
	src/ThrottleInfo.cpp \
	src/SharedLibraryAdapter.cpp

UT_SOURCES = \
	ut/FlashDeviceInfoUt.cpp \
	ut/FlashImageFileUt.cpp \
	ut/FlashImageUt.cpp \
	ut/FlashStatusUt.cpp \
	ut/KnlMockDeviceUt.cpp \
	ut/KnlCustomBootManagerUt.cpp \
	ut/MicBootConfigInfoUt.cpp \
	ut/MicCoreInfoUt.cpp \
	ut/MicCoreUsageInfoUt.cpp \
	ut/MicDeviceDetailsUt.cpp \
	ut/MicDeviceErrorUt.cpp \
	ut/MicDeviceInfoUt.cpp \
	ut/MicDeviceStaticUt.cpp \
	ut/MicFrequencyUt.cpp \
	ut/MicMemoryInfoUt.cpp \
	ut/MicMemoryUsageInfoUt.cpp \
	ut/MicMemoryUt.cpp \
	ut/MicPciConfigInfoUt.cpp \
	ut/MicPlatformInfoUt.cpp \
	ut/MicPowerStateUt.cpp \
	ut/MicPowerThresholdInfoUt.cpp \
	ut/MicPowerUsageInfoUt.cpp \
	ut/MicPowerUt.cpp \
	ut/MicProcessorInfoUt.cpp \
	ut/MicSpeedUt.cpp \
	ut/MicTemperatureUt.cpp \
	ut/MicThermalInfoUt.cpp \
	ut/MicVersionInfoUt.cpp \
	ut/MicVoltageInfoUt.cpp \
	ut/MicVoltageUt.cpp \
	ut/MpssStackBaseUt.cpp \
	ut/Mpss4StackLinuxUt.cpp \
	ut/PciAddressUt.cpp \
	ut/ThrottleInfoUt.cpp \
	ut/ScifRequestUt.cpp \
	ut/SystoolsdConnectionUt.cpp \
	ut/RasConnectionUt.cpp \
	ut/ScifConnectionBaseUt.cpp \
	ut/KnlDeviceUt.cpp

-include $(TOPDIR)/common.mk

local_test : test
	-
