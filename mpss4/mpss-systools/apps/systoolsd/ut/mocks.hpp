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

#ifndef _SYSTOOLS_SYSTOOLSD_MOCKS_HPP_
#define _SYSTOOLS_SYSTOOLSD_MOCKS_HPP_

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "FileInterface.hpp"
#include "FileReader.hpp"
#include "I2cAccess.hpp"
#include "I2cIo.hpp"
#include "PopenInterface.hpp"
#include "PThresh.hpp"
#include "PWindow.hpp"
#include "ScifEp.hpp"
#include "ScifSocket.hpp"
#include "Syscfg.hpp"
#include "SystoolsdServices.hpp"
#include "TurboSettings.hpp"

#include "info/KernelInfo.hpp"
#include "smbios/SmBiosInfo.hpp"

//fake SCIF implementation
class FakeScifImpl : public ScifSocket
{
public:
    MOCK_METHOD5(accept, int(int, uint16_t*, uint16_t*, int*, bool));
    MOCK_METHOD2(bind, int(int, uint16_t));
    MOCK_METHOD1(close, int(int));
    MOCK_METHOD3(connect, int(int, uint16_t*, uint16_t*));
    MOCK_METHOD1(get_fd, int(int));
    MOCK_METHOD3(get_node_ids, int(uint16_t*, int, uint16_t*));
    MOCK_METHOD2(listen, int(int, int));
    MOCK_METHOD0(open, int());
    MOCK_METHOD4(recv, int(int, void*, int, bool));
    MOCK_METHOD4(send, int(int, void *, int, bool));
    MOCK_METHOD3(poll_read, std::vector<int>(const std::vector<int>&, long, int*));
    MOCK_METHOD3(poll, int(scif_pollepd*, unsigned int, long));
};

//mock i2c implementation
class MockI2cIo : public I2cIo
{
public:
    MOCK_METHOD1(open_adapter, int(uint8_t));
    MOCK_METHOD2(set_slave_addr, int(int, long));
    MOCK_METHOD4(read, int(int, uint8_t, char*, size_t));
    MOCK_METHOD4(write, int(int, uint8_t, char*, size_t));
    MOCK_METHOD1(close_adapter, int(int));
};

class MockI2cAccess : public I2cBase
{
public:
    MOCK_METHOD3(read_bytes, void(uint8_t, char*, size_t));
    MOCK_METHOD3(write_bytes, void(uint8_t, char*, size_t));
    MOCK_METHOD1(read_u32, uint32_t(uint8_t));
    MOCK_METHOD2(write_u32, void(uint8_t, uint32_t));
    MOCK_METHOD1(restart_device, void(uint8_t));
    MOCK_METHOD0(is_device_busy, BusyInfo());
    MOCK_METHOD0(time_busy, uint32_t());
    MOCK_METHOD3(read_bytes_, void(uint8_t, char*, size_t));
    MOCK_METHOD3(write_bytes_, void(uint8_t, char*, size_t));
    MOCK_METHOD1(read_u32_, uint32_t(uint8_t));
    MOCK_METHOD2(write_u32_, void(uint8_t, uint32_t));
};

class MockFile : public FileInterface
{
public:
    MOCK_METHOD2(open, int(const std::string&, int));
    MOCK_METHOD1(close, void(int));
    MOCK_METHOD3(read, ssize_t(int, char *const, size_t));
    MOCK_METHOD3(write, ssize_t(int, const char *, size_t));
    MOCK_METHOD2(write, ssize_t(int, int));
    MOCK_METHOD3(lseek, off_t(int, off_t, int));
    MOCK_METHOD1(rewind, void(int));
};

class MockPopen : public PopenInterface
{
public:
    MOCK_METHOD1(run, void(const char*));
    MOCK_METHOD0(get_output, std::string());
    MOCK_METHOD0(get_retcode, int());
};

class MockSmBios : public SmBiosInterface
{
public:
    MOCK_CONST_METHOD1(get_structures_of_type,
            const std::vector<SmBiosStructureBase::ptr>&(SmBiosStructureType));

    // Empty implementations
    virtual void find_entry_point(EntryPointFinderInterface*)
    {

    }

    virtual void parse()
    {

    }
};

class MockPThresh : public PThreshInterface
{
public:
    MOCK_CONST_METHOD0(get_max_phys_pwr, uint32_t());
    MOCK_CONST_METHOD0(get_low_pthresh, uint32_t());
    MOCK_CONST_METHOD0(get_high_pthresh, uint32_t());
    MOCK_CONST_METHOD1(window_factory, std::shared_ptr<PWindowInterface>(uint8_t));
};

class MockPWindow : public PWindowInterface
{
public:
    MOCK_CONST_METHOD0(get_window_info, PowerWindowInfo());
    MOCK_CONST_METHOD0(get_pthresh, uint32_t());
    MOCK_CONST_METHOD0(get_time_window, uint32_t());
    MOCK_METHOD1(set_pthresh, void(uint32_t));
    MOCK_METHOD1(set_time_window, void(uint32_t));
};

class MockKernel : public KernelInterface
{
public:
    MOCK_CONST_METHOD0(get_logical_core_count, uint32_t());
    MOCK_CONST_METHOD0(get_physical_core_count, uint32_t());
    MOCK_CONST_METHOD0(get_threads_per_core, uint16_t());
    MOCK_CONST_METHOD0(get_cpu_frequency, uint32_t());
    MOCK_CONST_METHOD2(get_physical_core_usage, const std::vector<CoreCounters>&(CoreCounters*, uint64_t *));
    MOCK_CONST_METHOD2(get_logical_core_usage, const std::vector<CoreCounters>&(CoreCounters*, uint64_t *));
};

class MockScifEp : public ScifEpInterface
{
    typedef std::shared_ptr<ScifEpInterface> ptr;
public:
    MOCK_METHOD1(bind, void(uint16_t));
    MOCK_METHOD1(listen, void(int));
    MOCK_METHOD2(connect, void(uint16_t, uint16_t));
    MOCK_METHOD1(accept, std::shared_ptr<ScifEpInterface>(bool));
    MOCK_METHOD3(recv, int(char *const, int , bool));
    MOCK_METHOD2(send, int(const char *const, int));
    MOCK_METHOD0(get_port_id, std::pair<uint16_t, uint16_t>());
    MOCK_METHOD0(get_epd, int());
    MOCK_METHOD0(close, void());
    MOCK_METHOD0(reset, void());
    MOCK_METHOD2(select_read, std::vector<ptr>(const std::vector<ptr>&, timeval *tv));
    MOCK_METHOD3(poll, int(scif_pollepd*, unsigned int, long));
};

class MockTurbo : public TurboSettingsInterface
{
public:
    MOCK_CONST_METHOD0(is_turbo_enabled, bool());
    MOCK_CONST_METHOD0(get_turbo_pct, uint8_t());
    MOCK_METHOD1(set_turbo_enabled, void(bool));
};

class MockSyscfg : public SyscfgInterface
{
public:
    MOCK_CONST_METHOD0(ecc_enabled, bool());
    MOCK_CONST_METHOD0(errinject_enabled, bool());
    MOCK_CONST_METHOD0(eist_enabled, bool());
    MOCK_CONST_METHOD0(micfw_update_flag_enabled, bool());
    MOCK_CONST_METHOD0(apei_support_enabled, bool());
    MOCK_CONST_METHOD0(apei_ffm_logging_enabled, bool());
    MOCK_CONST_METHOD0(apei_pcie_errinject_enabled, bool());
    MOCK_CONST_METHOD0(apei_pcie_errinject_table_enabled, bool());
    MOCK_CONST_METHOD0(get_ecc, Ecc());
    MOCK_CONST_METHOD0(get_mem_type, MemType());
    MOCK_CONST_METHOD0(get_cluster_mode, Cluster());
    MOCK_METHOD2(set_errinject, void(bool, const char *));
    MOCK_METHOD2(set_eist, void(bool, const char *));
    MOCK_METHOD2(set_micfw_update_flag, void(bool, const char *));
    MOCK_METHOD2(set_apei_support, void(bool, const char *));
    MOCK_METHOD2(set_apei_ffm_logging, void(bool, const char *));
    MOCK_METHOD2(set_apei_pcie_errinject, void(bool, const char *));
    MOCK_METHOD2(set_apei_pcie_errinject_table, void(bool, const char *));
    MOCK_METHOD2(set_ecc, void(Ecc, const char *));
    MOCK_METHOD2(set_cluster_mode, void(Cluster, const char *));
    MOCK_METHOD2(set_passwd, void(const char *, const char *));
};

class MockFileStream : public FileReader
{
public:
    MOCK_METHOD2(open, void(const char *, std::ios_base::openmode));
    MOCK_METHOD2(open, void(const std::string&, std::ios_base::openmode));
    MOCK_METHOD0(clear, void());
    MOCK_METHOD0(close, void());

    MOCK_METHOD2(getline, FileReader&(char *, size_t));
    MOCK_METHOD1(seekg, FileReader&(std::streampos));
    MOCK_METHOD2(seekg, FileReader&(std::streamoff, std::ios_base::seekdir));
    MOCK_METHOD2(read, FileReader&(char *, std::streamsize));
    MOCK_METHOD0(tellg, std::streampos());
    MOCK_CONST_METHOD0(good, bool());
    MOCK_CONST_METHOD0(is_open, bool());
};

class MockServices : public SystoolsdServices
{
public:
    MockServices() :
        SystoolsdServices(
            std::unique_ptr<I2cBase>(new MockI2cAccess),
            std::unique_ptr<SmBiosInterface>(new MockSmBios),
            std::unique_ptr<PThreshInterface>(new MockPThresh),
            std::unique_ptr<TurboSettingsInterface>(new MockTurbo),
            std::unique_ptr<SyscfgInterface>(new MockSyscfg),
            std::unique_ptr<KernelInterface>(new ::testing::NiceMock<MockKernel>))
    {
    }

    static SystoolsdServices::ptr get_services()
    {
        return std::unique_ptr<SystoolsdServices>(new MockServices);
    }

    static SystoolsdServices::ptr get_empty_services()
    {
        // Emtpy ctor contains null pointers to services
        return std::unique_ptr<Services>(new Services);
    }
};



#endif
