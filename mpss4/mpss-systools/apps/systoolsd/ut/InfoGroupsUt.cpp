/*
 * Copyright (c) 2017, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <unistd.h>

#include "Daemon.hpp"
#include "mocks.hpp"

#include "info/CoreUsageInfoGroup.hpp"
#include "info/DeviceInfoGroup.hpp"
#include "info/DiagnosticsInfoGroup.hpp"
#include "info/FwUpdateInfoGroup.hpp"
#include "info/MemoryInfoGroup.hpp"
#include "info/PowerThresholdsInfoGroup.hpp"
#include "info/PowerUsageInfoGroup.hpp"
#include "info/ProcessorInfoGroup.hpp"
#include "info/SmbaInfoGroup.hpp"
#include "info/ThermalInfoGroup.hpp"
#include "info/TurboInfoGroup.hpp"
#include "info/VoltageInfoGroup.hpp"

#include "smbios/BiosInfoStructure.hpp"
#include "smbios/MemoryDeviceStructure.hpp"
#include "smbios/ProcessorInfoStructure.hpp"
#include "smbios/SmBiosTypes.hpp"
#include "smbios/SystemInfoStructure.hpp"

using ::testing::AtLeast;
using ::testing::Gt;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;

class InfoGroupTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        services = MockServices::get_services();
        i2c = static_cast<MockI2cAccess*>(services->get_i2c_srv());
        smbios = static_cast<MockSmBios*>(services->get_smbios_srv());
        pthresh = static_cast<MockPThresh*>(services->get_pthresh_srv());
        kernel = static_cast<MockKernel*>(services->get_kernel_srv());
        turbo = static_cast<MockTurbo*>(services->get_turbo_srv());
        syscfg = static_cast<MockSyscfg*>(services->get_syscfg_srv());
    }

    //for each field in vector fields, an expectation will be set on member i2c
    //and value will be set as the return value for that specific call to the mock
    void set_i2c_read_u32_value(const std::vector<uint8_t> &fields, uint32_t value)
    {
        for(auto field = fields.begin(); field != fields.end(); ++field)
            EXPECT_CALL(*i2c, read_u32(*field))
                .WillRepeatedly(Return(value));
    }

    MockI2cAccess *i2c;
    MockSmBios *smbios;
    MockPThresh *pthresh;
    MockKernel *kernel;
    MockTurbo *turbo;
    MockSyscfg *syscfg;
    SystoolsdServices::ptr services;
};

typedef InfoGroupTest DiagnosticInfoGroupTest;
typedef InfoGroupTest SmbaInfoGroupTest;
typedef InfoGroupTest PowerUsageInfoGroupTest;
typedef InfoGroupTest ThermalInfoGroupTest;
typedef InfoGroupTest VoltageInfoGroupTest;
typedef InfoGroupTest FwUpdateInfoGroupTest;
typedef InfoGroupTest DeviceInfoGroupTest;
typedef InfoGroupTest ProcessorInfoGroupTest;
typedef InfoGroupTest MemoryInfoGroupTest;
typedef InfoGroupTest PowerThresholdsInfoGroupTest;
typedef InfoGroupTest CoreUsageInfoGroupTest;
typedef InfoGroupTest TurboInfoGroupTest;

//the following constants are defined in the source file of each
// InfoGroup class. Keep the definitions separate so that unit tests
// don't depend on the same values as the implementations.
namespace
{
    //constants for power telemetry SMC registers
    const uint8_t pwr_pcie = 0x28;
    const uint8_t pwr_2x3 = 0x29;
    const uint8_t pwr_2x4 = 0x2A;
    const uint8_t force_throttle = 0x2B;
    const uint8_t avg_power_0 = 0x35;
    const uint8_t avg_power_1 = 0x36;
    const uint8_t inst_power = 0x3A;
    const uint8_t inst_power_max = 0x3B;
    const uint8_t power_vccp = 0x70;
    const uint8_t power_vccu = 0x71;
    const uint8_t power_vccclr = 0x72;
    const uint8_t power_vccmlb = 0x73;
    const uint8_t power_vccmp = 0x76;
    const uint8_t power_ntb1 = 0x77;

    //constants for thermal telemetry SMC registers
    const uint8_t temp_cpu = 0x40;
    const uint8_t temp_exhaust = 0x41;
    const uint8_t temp_vccp = 0x43;
    const uint8_t temp_vccclr = 0x44;
    const uint8_t temp_vccmp = 0x45;
    const uint8_t temp_mic = 0x46;
    const uint8_t temp_west = 0x47;
    const uint8_t temp_east = 0x48;
    const uint8_t fan_tach = 0x49;
    const uint8_t fan_pwm = 0x4A;
    const uint8_t fan_pwm_adder = 0x4B;
    const uint8_t tcritical = 0x4C;
    const uint8_t tcontrol = 0x4D;

    //constants for voltage telemetry SMC registers
    const uint8_t voltage_vccp = 0x50;
    const uint8_t voltage_vccu = 0x51;
    const uint8_t voltage_vccclr = 0x52;
    const uint8_t voltage_vccmlb = 0x53;
    const uint8_t voltage_vccmp = 0x56;
    const uint8_t voltage_ntb1 = 0x57;
    const uint8_t voltage_vccpio = 0x58;
    const uint8_t voltage_vccsfr = 0x59;
    const uint8_t voltage_pch = 0x5A;
    const uint8_t voltage_vccmfuse = 0x5B;
    const uint8_t voltage_ntb2 = 0x5C;
    const uint8_t voltage_vpp = 0x5D;

    //constants for FW update information SMC registers
    const uint8_t fwu_sts = 0xE1;
    const uint8_t fwu_cmd = 0xE2;

    //constants for miscelaneous device information SMC registers
    const uint8_t part_number = 0x18;
    const uint8_t manufacture_date = 0x19;
    const uint8_t serialno = 0x15;
    const uint8_t card_tdp = 0x1E;
    const uint8_t fwu_cap = 0xE0;
    const uint8_t cpu_id = 0x1C;
    const uint8_t pci_smba = 0x07;
    const uint8_t fw_version = 0x11;
    const uint8_t exe_domain = 0x12;
    const uint8_t sts_selftest = 0x13;
    const uint8_t boot_fw_version = 0x16;
    const uint8_t hw_revision = 0x14;
}

TEST_F(DiagnosticInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(DiagnosticsInfoGroup d(services));
}

TEST_F(DiagnosticInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(DiagnosticsInfoGroup d(s), std::invalid_argument);
}

TEST_F(DiagnosticInfoGroupTest, TC_refreshdata_001)
{
    EXPECT_CALL(*i2c, read_u32(0x60))
        .Times(AtLeast(2))
        .WillOnce(Return(0))
        .WillOnce(Return(2));
    DiagnosticsInfoGroup d(services);
    DiagnosticsInfo info;
    ASSERT_NO_THROW(info = d.get_data());
    ASSERT_EQ(0, info.led_blink);
    ASSERT_NO_THROW(info = d.get_data(true));
    ASSERT_EQ(1, info.led_blink);
}

TEST_F(SmbaInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(SmbaInfoGroup s(services));
}

TEST_F(SmbaInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(SmbaInfoGroup d(s), std::invalid_argument);
}

TEST_F(SmbaInfoGroupTest, TC_refreshdata_001)
{
    BusyInfo busy_expected = {1, 1000};
    BusyInfo avail_expected = {0, 0};
    EXPECT_CALL(*i2c, is_device_busy())
        .Times(AtLeast(2))
        .WillOnce(Return(busy_expected))
        .WillOnce(Return(avail_expected));
    SmbaInfoGroup smba_info(services);
    SmbaInfo b;
    ASSERT_NO_THROW(b = smba_info.get_data());
    ASSERT_EQ(busy_expected.is_busy, b.is_busy);
    ASSERT_EQ(SMBA_RESTART_WAIT_MS - busy_expected.elapsed_busy, b.ms_remaining);

    b = smba_info.get_data(true);
    ASSERT_EQ(avail_expected.is_busy, b.is_busy);
    ASSERT_EQ(0, b.ms_remaining);
}

TEST_F(PowerUsageInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(PowerUsageInfoGroup p(services));
}

TEST_F(PowerUsageInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(PowerUsageInfoGroup d(s), std::invalid_argument);
}

TEST_F(PowerUsageInfoGroupTest, TC_refreshdata_001)
{
    std::vector<uint8_t> fields = {pwr_pcie, pwr_2x3, pwr_2x4, force_throttle,
                                   avg_power_0, inst_power,
                                   inst_power_max, power_vccp, power_vccu,
                                   power_vccclr, power_vccmlb, power_vccmp,
                                   power_ntb1};
    uint32_t b = 0xbaf0;
    set_i2c_read_u32_value(fields, b);

    PowerUsageInfoGroup info(services);
    PowerUsageInfo data;
    ASSERT_NO_THROW(data = info.get_data());

    //make some assertions
    ASSERT_EQ(b, data.pwr_pcie);
    ASSERT_EQ(b, data.pwr_2x3);
    ASSERT_EQ(b, data.pwr_2x4);
    ASSERT_EQ(b, data.force_throttle);
    ASSERT_EQ(b, data.avg_power_0);
    ASSERT_EQ(b, data.inst_power);
    ASSERT_EQ(b, data.inst_power_max);
    ASSERT_EQ(b, data.power_vccp);
    ASSERT_EQ(b, data.power_vccu);
    ASSERT_EQ(b, data.power_vccclr);
    ASSERT_EQ(b, data.power_vccmlb);
    ASSERT_EQ(b, data.power_ntb1);
    ASSERT_EQ(b, data.power_vccmp);

    //reset expectations and set new ones
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(i2c));
    b = 0xbad;
    set_i2c_read_u32_value(fields, b);

    ASSERT_NO_THROW(data = info.get_data(true));

    ASSERT_EQ(b, data.pwr_pcie);
    ASSERT_EQ(b, data.pwr_2x3);
    ASSERT_EQ(b, data.pwr_2x4);
    ASSERT_EQ(b, data.force_throttle);
    ASSERT_EQ(b, data.avg_power_0);
    ASSERT_EQ(b, data.inst_power);
    ASSERT_EQ(b, data.inst_power_max);
    ASSERT_EQ(b, data.power_vccp);
    ASSERT_EQ(b, data.power_vccu);
    ASSERT_EQ(b, data.power_vccclr);
    ASSERT_EQ(b, data.power_vccmlb);
    ASSERT_EQ(b, data.power_ntb1);
    ASSERT_EQ(b, data.power_vccmp);
}

TEST_F(ThermalInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(ThermalInfoGroup p(services));
}

TEST_F(ThermalInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(ThermalInfoGroup d(s), std::invalid_argument);
}

TEST_F(ThermalInfoGroupTest, TC_refreshdata_001)
{
    std::vector<uint8_t> fields = {temp_cpu, temp_exhaust, temp_vccp,
                                   temp_vccclr, temp_vccmp, temp_mic, temp_west,
                                   temp_east, fan_tach, fan_pwm, fan_pwm_adder,
                                   tcritical, tcontrol};
    uint32_t b = 0xabad1dea;
    set_i2c_read_u32_value(fields, b);

    ThermalInfoGroup info(services);
    ThermalInfo data;
    ASSERT_NO_THROW(data = info.get_data());

    ASSERT_EQ(b, data.temp_cpu);
    ASSERT_EQ(b, data.temp_exhaust);
    ASSERT_EQ(b, data.temp_vccp);
    ASSERT_EQ(b, data.temp_vccclr);
    ASSERT_EQ(b, data.temp_vccmp);
    ASSERT_EQ(b, data.temp_west);
    ASSERT_EQ(b, data.temp_east);
    ASSERT_EQ(b, data.fan_tach);
    ASSERT_EQ(b, data.fan_pwm);
    ASSERT_EQ(b, data.fan_pwm_adder);
    ASSERT_EQ(b, data.tcritical);
    ASSERT_EQ(b, data.tcontrol);

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(i2c));
    b = 0xa1ada;
    set_i2c_read_u32_value(fields, b);

    ASSERT_NO_THROW(data = info.get_data(true));

    ASSERT_EQ(b, data.temp_cpu);
    ASSERT_EQ(b, data.temp_exhaust);
    ASSERT_EQ(b, data.temp_vccp);
    ASSERT_EQ(b, data.temp_vccclr);
    ASSERT_EQ(b, data.temp_vccmp);
    ASSERT_EQ(b, data.temp_west);
    ASSERT_EQ(b, data.temp_east);
    ASSERT_EQ(b, data.fan_tach);
    ASSERT_EQ(b, data.fan_pwm);
    ASSERT_EQ(b, data.fan_pwm_adder);
    ASSERT_EQ(b, data.tcritical);
    ASSERT_EQ(b, data.tcontrol);
}

TEST_F(VoltageInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(VoltageInfoGroup p(services));
}

TEST_F(VoltageInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(VoltageInfoGroup d(s), std::invalid_argument);
}

TEST_F(VoltageInfoGroupTest, TC_refreshdata_001)
{
    std::vector<uint8_t> fields = {voltage_vccp, voltage_vccu, voltage_vccclr, voltage_vccmlb,
                                   voltage_ntb1, voltage_vccmp, voltage_vccpio, voltage_vccsfr,
                                   voltage_pch, voltage_vccmfuse, voltage_ntb2, voltage_vpp};

    uint32_t b = 0xabad1dea;

    set_i2c_read_u32_value(fields, b);

    VoltageInfoGroup info(services);
    VoltageInfo data;
    ASSERT_NO_THROW(data = info.get_data());

    ASSERT_EQ(b, data.voltage_vccp);
    ASSERT_EQ(b, data.voltage_vccu);
    ASSERT_EQ(b, data.voltage_vccclr);
    ASSERT_EQ(b, data.voltage_vccmlb);
    ASSERT_EQ(b, data.voltage_ntb1);
    ASSERT_EQ(b, data.voltage_vccmp);
    ASSERT_EQ(b, data.voltage_vccpio);
    ASSERT_EQ(b, data.voltage_vccsfr);
    ASSERT_EQ(b, data.voltage_pch);
    ASSERT_EQ(b, data.voltage_vccmfuse);
    ASSERT_EQ(b, data.voltage_ntb2);
    ASSERT_EQ(b, data.voltage_vpp);

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(i2c));
    b = 0x11d0;
    set_i2c_read_u32_value(fields, b);

    ASSERT_NO_THROW(data = info.get_data(true));

    ASSERT_EQ(b, data.voltage_vccp);
    ASSERT_EQ(b, data.voltage_vccu);
    ASSERT_EQ(b, data.voltage_vccclr);
    ASSERT_EQ(b, data.voltage_vccmlb);
    ASSERT_EQ(b, data.voltage_ntb1);
    ASSERT_EQ(b, data.voltage_vccmp);
    ASSERT_EQ(b, data.voltage_vccpio);
    ASSERT_EQ(b, data.voltage_vccsfr);
    ASSERT_EQ(b, data.voltage_pch);
    ASSERT_EQ(b, data.voltage_vccmfuse);
    ASSERT_EQ(b, data.voltage_ntb2);
    ASSERT_EQ(b, data.voltage_vpp);
}

TEST_F(FwUpdateInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(FwUpdateInfoGroup p(services));
}

TEST_F(FwUpdateInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(FwUpdateInfoGroup d(s), std::invalid_argument);
}

TEST_F(FwUpdateInfoGroupTest, TC_refreshdata_001)
{
    uint32_t b = 0xabad1dea;
    EXPECT_CALL(*i2c, read_u32(fwu_sts))
        .WillOnce(Return(b));
    EXPECT_CALL(*i2c, read_u32(fwu_cmd))
        .WillOnce(Return(b));

    FwUpdateInfoGroup info(services);
    FwUpdateInfo data;
    ASSERT_NO_THROW(data = info.get_data());

    ASSERT_EQ(b, data.fwu_sts);
    ASSERT_EQ(b, data.fwu_cmd);

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(i2c));
    b = 0xbaaaad;

    EXPECT_CALL(*i2c, read_u32(fwu_sts))
        .WillOnce(Return(b));
    EXPECT_CALL(*i2c, read_u32(fwu_cmd))
        .WillOnce(Return(b));

    ASSERT_NO_THROW(data = info.get_data(true));

    ASSERT_EQ(b, data.fwu_sts);
    ASSERT_EQ(b, data.fwu_cmd);
}

TEST_F(DeviceInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(DeviceInfoGroup p(services));
}

TEST_F(DeviceInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(DeviceInfoGroup d(s), std::invalid_argument);
}

TEST_F(DeviceInfoGroupTest, TC_ctor_refreshdata_001)
{
    std::vector<uint8_t> fields = {card_tdp, fwu_cap, cpu_id, pci_smba,
                                   fw_version, exe_domain, sts_selftest,
                                   boot_fw_version, hw_revision};
    uint32_t b = 0xabad1dea;
    set_i2c_read_u32_value(fields, b);

    const char serial[] = "9386823789";
    const char part_number_array[] = "part number";
    const char date_array[] = "032615";

    //set expectations on i2c::read_bytes()
    EXPECT_CALL(*i2c, read_bytes(part_number, NotNull(), Gt(0)))
        .WillRepeatedly(SetArrayArgument<1>(part_number_array, part_number_array + sizeof(part_number_array)));

    EXPECT_CALL(*i2c, read_bytes(manufacture_date, NotNull(), Gt(0)))
        .WillRepeatedly(SetArrayArgument<1>(date_array, date_array + sizeof(date_array)));

    EXPECT_CALL(*i2c, read_bytes(serialno, NotNull(), Gt(0)))
        .WillRepeatedly(SetArrayArgument<1>(serial, serial + sizeof(serial)));

    //BiosInfoStructure used in DeviceInfoGroup
    s_BiosInfoStructure bios_struct;
    bzero(&bios_struct, sizeof(bios_struct));
    const std::string vendor = "some_vendor";
    const std::string version = "some_version";
    const std::string date = "today!";
    SmBiosInfo::structures_vector bios_structures;
    bios_structures.push_back(
            std::make_shared<BiosInfoStructure>(bios_struct, vendor, version, date));

    s_SystemInfoStructure system_struct;
    bzero(&system_struct, sizeof(system_struct));
    const std::string manufacturer = "some_manuf";
    const std::string product = "some_product";
    const std::string system_version = "sysversion";
    const std::string sku_number = "thisIsASku";
    const std::string family = "family";
    SmBiosInfo::structures_vector system_structures;
    system_structures.push_back(
            std::make_shared<SystemInfoStructure>(
                system_struct, manufacturer, product, system_version, serial,
                sku_number, family));

    EXPECT_CALL(*smbios, get_structures_of_type(BIOS))
        .WillRepeatedly(ReturnRef(bios_structures));
    EXPECT_CALL(*smbios, get_structures_of_type(SYSTEM_INFO))
        .WillRepeatedly(ReturnRef(system_structures));

    {
        MockPopen *popen = new MockPopen; //will live in a unique_ptr inside DeviceInfoGroup instance
        const std::string os_version = "GNU/Linux\n"; //notice newline char
        const std::string expected_os_version = "GNU/Linux";
        EXPECT_CALL(*popen, run(NotNull()))
            .Times(1);
        EXPECT_CALL(*popen, get_output())
            .WillOnce(Return(os_version));
        EXPECT_CALL(*popen, get_retcode())
        .WillOnce(Return(0));
        DeviceInfoGroup info(services);
        info.set_popen(popen);
        DeviceInfo data;

        ASSERT_NO_THROW(data = info.get_data());

        ASSERT_EQ(b & 0xFFFF, data.card_tdp);
        ASSERT_EQ(b, data.fwu_cap);
        ASSERT_EQ(b, data.cpu_id);
        ASSERT_EQ(b, data.pci_smba);
        ASSERT_EQ(b, data.fw_version);
        ASSERT_EQ(b, data.exe_domain);
        ASSERT_EQ(b, data.sts_selftest);
        ASSERT_EQ(b, data.boot_fw_version);
        ASSERT_EQ(b, data.hw_revision);

        ASSERT_EQ(version, std::string(data.bios_version));
        ASSERT_EQ(date, std::string(data.bios_release_date));
        ASSERT_EQ(expected_os_version, std::string(data.os_version));

        ASSERT_STREQ(part_number_array, data.part_number);
        ASSERT_EQ(std::string(date_array),
                std::string(data.manufacture_date,
                    sizeof(data.manufacture_date)));
    }

    {   //negative case where command run in Popen fails
        MockPopen *popen = new MockPopen; //will live in a unique_ptr inside DeviceInfoGroup instance
        EXPECT_CALL(*popen, run(NotNull()))
            .Times(1);
        EXPECT_CALL(*popen, get_retcode())
            .WillOnce(Return(1));
        DeviceInfoGroup info(services);
        info.set_popen(popen);
        DeviceInfo data;
        ASSERT_THROW(data = info.get_data(), SystoolsdException);
    }

    {   //negative case where string returned by popen is empty
        MockPopen *popen = new MockPopen; //will live in a unique_ptr inside DeviceInfoGroup instance
        EXPECT_CALL(*popen, run(NotNull()))
            .Times(1);
        EXPECT_CALL(*popen, get_retcode())
            .WillOnce(Return(0));
        EXPECT_CALL(*popen, get_output())
            .WillOnce(Return(std::string("")));
        DeviceInfoGroup info(services);
        info.set_popen(popen);
        DeviceInfo data;
        ASSERT_THROW(data = info.get_data(), SystoolsdException);
    }
}

TEST_F(ProcessorInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(ProcessorInfoGroup p(services));
}

TEST_F(ProcessorInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(ProcessorInfoGroup d(s), std::invalid_argument);
}

TEST_F(ProcessorInfoGroupTest, TC_refreshdata_001)
{
    //Create a few of ProcessorInfoStructure objects
    const uint16_t family = 0xAB;
    const uint16_t type = 0xFB;
    const uint16_t enabled_cores = 4;
    const uint16_t thread_count = 8;
    const uint16_t threads_per_core = 2;
    const uint8_t ncores = 4;
    const std::string empty = "";

    SmBiosInfo::structures_vector proc_structs;
    for(uint8_t i = 0; i < ncores; ++i)
    {
        s_ProcessorInfoStructure p;
        bzero(&p, sizeof(p));
        p.processor_type = type;
        p.processor_family = family;
        p.core_enabled = enabled_cores;
        p.thread_count = thread_count;
        auto proc = std::make_shared<ProcessorInfoStructure>(p, empty, empty, empty,
                empty, empty, empty);
        proc_structs.push_back(proc);
    }

    EXPECT_CALL(*smbios, get_structures_of_type(PROCESSOR_INFO))
        .WillOnce(ReturnRef(proc_structs));

    ProcessorInfoGroup info(services);
    ProcessorInfo data;
    ASSERT_NO_THROW(data = info.get_data());

    ASSERT_EQ(family, data.family);
    ASSERT_EQ(type, data.type);
    ASSERT_EQ(threads_per_core, data.threads_per_core);
}

TEST_F(MemoryInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(MemoryInfoGroup p(services));
}

TEST_F(MemoryInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(MemoryInfoGroup d(s), std::invalid_argument);
}

TEST_F(MemoryInfoGroupTest, TC_refreshdata_001)
{
    const uint16_t size = 64;
    const uint16_t speed = 120;
    const uint8_t type = 1;
    const uint8_t ndevices = 8;
    const std::string manuf = "hynix";
    const std::string empty = "";
    SmBiosInfo::structures_vector devices;
    for(uint8_t i = 0; i < ndevices; ++i)
    {
        s_MemoryDeviceStructure m;
        bzero(&m, sizeof(m));
        m.size = size;
        m.speed = speed;
        m.memory_type = type;

        auto device = std::make_shared<MemoryDeviceStructure>(m, empty, empty, manuf, empty, empty, empty);
        devices.push_back(device);
    }
    EXPECT_CALL(*smbios, get_structures_of_type(MEMORY_DEVICE))
        .WillOnce(ReturnRef(devices));
    EXPECT_CALL(*syscfg, ecc_enabled())
        .WillOnce(Return(true));
    MemoryInfoGroup info(services);
    MemoryInfo data;
    ASSERT_NO_THROW(data = info.get_data());

    ASSERT_EQ(size * ndevices, data.total_size);
    ASSERT_EQ(speed, data.speed);
    ASSERT_EQ(type, data.type);
    ASSERT_EQ(manuf, std::string(data.manufacturer));
    ASSERT_TRUE(data.ecc_enabled);
}

TEST_F(PowerThresholdsInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(PowerThresholdsInfoGroup p(services));
}

TEST_F(PowerThresholdsInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(PowerThresholdsInfoGroup d(s), std::invalid_argument);
}

TEST_F(PowerThresholdsInfoGroupTest, TC_refreshdata_001)
{
    const uint32_t max = 100;
    const uint32_t low = 50;
    const uint32_t hi = 200;
    const uint32_t w0_thresh = 10;
    const uint32_t w0_time = 20;
    const uint32_t w1_thresh = 30;
    const uint32_t w1_time = 40;
    PowerWindowInfo w0_info = {w0_thresh, w0_time};
    PowerWindowInfo w1_info = {w1_thresh, w1_time};
    auto w0 = std::shared_ptr<MockPWindow>(new MockPWindow);
    auto w1 = std::shared_ptr<MockPWindow>(new MockPWindow);
    EXPECT_CALL(*w0.get(), get_window_info())
        .WillOnce(Return(w0_info));
    EXPECT_CALL(*w1.get(), get_window_info())
        .WillOnce(Return(w1_info));
    EXPECT_CALL(*pthresh, get_max_phys_pwr())
        .WillOnce(Return(max));
    EXPECT_CALL(*pthresh, get_low_pthresh())
        .WillOnce(Return(low));
    EXPECT_CALL(*pthresh, get_high_pthresh())
        .WillOnce(Return(hi));
    EXPECT_CALL(*pthresh, window_factory(0))
        .WillOnce(Return(w0));
    EXPECT_CALL(*pthresh, window_factory(1))
        .WillOnce(Return(w1));
    PowerThresholdsInfoGroup info(services);
    PowerThresholdsInfo data;
    ASSERT_NO_THROW(data = info.get_data());

    ASSERT_EQ(max, data.max_phys_power);
    ASSERT_EQ(low, data.low_threshold);
    ASSERT_EQ(hi, data.hi_threshold);
    ASSERT_EQ(w0_thresh, data.w0.threshold);
    ASSERT_EQ(w0_time, data.w0.time_window);
    ASSERT_EQ(w1_thresh, data.w1.threshold);
    ASSERT_EQ(w1_time, data.w1.time_window);
}

TEST_F(CoreUsageInfoGroupTest, TC_ctor_001)
{
    EXPECT_CALL(*kernel, get_physical_core_count())
        .WillOnce(Return(16));
    EXPECT_CALL(*kernel, get_threads_per_core())
        .WillOnce(Return(2));
    ASSERT_NO_THROW(CoreUsageInfoGroup p(services));
}

TEST_F(CoreUsageInfoGroupTest, TC_ctor_throw_002)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(CoreUsageInfoGroup d(s), std::invalid_argument);
}

TEST_F(CoreUsageInfoGroupTest, TC_refreshdata_001)
{
    //we're gonna have 8 physical cores, 2 threads per core
    //the current CPU frequency will be 1000 MHz
    //for each type of counter (user, nice, system, idle) set 2 jiffies
    const uint64_t clk_tck = sysconf(_SC_CLK_TCK);
    const size_t phys_ncores = 8;
    const uint64_t jiffies = 2;
    const uint16_t threads_per_core = 2;
    const uint32_t frequency = 1000;
    std::vector<CoreCounters> logical_counters(phys_ncores * threads_per_core);
    CoreCounters sum;

    for(auto thread = logical_counters.begin(); thread != logical_counters.end(); ++thread)
    {
        thread->user = jiffies;
        thread->nice = jiffies;
        thread->system = jiffies;
        thread->idle = jiffies;
    }

    sum.user = jiffies * phys_ncores;
    sum.nice = jiffies * phys_ncores;
    sum.system = jiffies * phys_ncores;
    sum.idle = jiffies * phys_ncores;

    EXPECT_CALL(*kernel, get_physical_core_count())
        .WillRepeatedly(Return(static_cast<uint64_t>(phys_ncores)));
    EXPECT_CALL(*kernel, get_threads_per_core())
        .WillRepeatedly(Return(threads_per_core));
    EXPECT_CALL(*kernel, get_cpu_frequency())
        .WillRepeatedly(Return(frequency));
    EXPECT_CALL(*kernel, get_logical_core_usage(NotNull(), NotNull()))
        .WillRepeatedly(DoAll(SetArgPointee<0>(sum), SetArgPointee<1>(100),
                              ReturnRef(logical_counters)));

    //use copy_data_to() instead of get_data()
    CoreUsageInfoGroup info(services);
    //expected size if sizeof(CoreUsageInfo) + sizeof(CoreCounters) * number of threads
    size_t size = info.get_size();
    ASSERT_EQ(sizeof(CoreUsageInfo) + sizeof(CoreCounters) * phys_ncores * threads_per_core, size);

    //use EXPECT from here on, not ASSERT so we can free raw_buf
    char *raw_buf = new char[size];

    //The data contained (and returned by/copied) by this GroupInfo has the following format:
    // [  [CoreUsageInfo]  [CoreCounters[N]]  ]
    EXPECT_NO_THROW(info.copy_data_to(raw_buf, &size));
    CoreUsageInfo *cusage = reinterpret_cast<CoreUsageInfo*>(raw_buf);

    EXPECT_EQ(phys_ncores, cusage->num_cores);
    EXPECT_EQ(threads_per_core, cusage->threads_per_core);
    EXPECT_EQ(frequency, cusage->frequency);
    EXPECT_EQ(clk_tck, cusage->clocks_per_sec);
    EXPECT_EQ(sum.user, cusage->sum.user);
    EXPECT_EQ(sum.nice, cusage->sum.nice);
    EXPECT_EQ(sum.system, cusage->sum.system);
    EXPECT_EQ(sum.idle, cusage->sum.idle);

    //Check each CoreCounters struct for each thread
    for(size_t thread = 0; thread != phys_ncores * threads_per_core; ++thread)
    {
        //remember to offset the pointer
        CoreCounters *p = reinterpret_cast<CoreCounters*>(
                raw_buf + sizeof(CoreUsageInfo) + (sizeof(CoreCounters) * thread));
        EXPECT_EQ(jiffies, p->user);
        EXPECT_EQ(jiffies, p->nice);
        EXPECT_EQ(jiffies, p->system);
        EXPECT_EQ(jiffies, p->idle);
    }
    delete [] raw_buf;
}

TEST_F(CoreUsageInfoGroupTest, TC_copydatato_001)
{
    CoreUsageInfoGroup info(services);
    char b = '\0';
    size_t size = 1;
    ASSERT_THROW(info.copy_data_to(&b, NULL), std::invalid_argument);
    ASSERT_THROW(info.copy_data_to(NULL, &size), std::invalid_argument);
    ASSERT_TRUE(size == 1);
    size = 0;
    ASSERT_THROW(info.copy_data_to(&b, &size), std::invalid_argument);
}

TEST_F(TurboInfoGroupTest, TC_ctor_001)
{
    ASSERT_NO_THROW(TurboInfoGroup info(services));
}

TEST_F(TurboInfoGroupTest, TC_ctor_throw_001)
{
    Services::ptr s = MockServices::get_empty_services();
    ASSERT_THROW(TurboInfoGroup info1(s), std::invalid_argument);
}

TEST_F(TurboInfoGroupTest, TC_refreshdata_001)
{
    EXPECT_CALL(*turbo, is_turbo_enabled())
        .Times(1);
    EXPECT_CALL(*turbo, get_turbo_pct())
        .Times(1);
    TurboInfoGroup info(services);
    ASSERT_NO_THROW(info.get_data());
}
