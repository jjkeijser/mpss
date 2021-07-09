/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
*/
#include <gtest/gtest.h>

#include "FlashStatus.hpp"
#include "KnlDevice.hpp"
#include "MicDeviceError.hpp"

#include "mocks.hpp"

using ::testing::_;
using ::testing::An;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace
{
const uint32_t     MIC_SUCCESS = micmgmt::MicDeviceError::errorCode( MICSDKERR_SUCCESS );
const uint32_t     INVALID_ARG = micmgmt::MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );
const uint32_t     INTERNAL_ERROR = micmgmt::MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
const char* const  PROPVAL_DEVICE_MODE_UNKNOWN   = "N/A";
const char* const  PROPVAL_DEVICE_MODE_NORMAL    = "linux";
const char* const  PROPVAL_DEVICE_MODE_FLASH     = "flash";
}

namespace micmgmt
{

struct MpssCreator
{
    static MockMpssStack* createMpssStack( int device )
    {
        return new MockMpssStack( device );
    }
};


class KnlMockBase : public KnlDeviceBase
{
public:
    KnlMockBase( int number ) : m_devnum(number)
    {

    }

    MOCK_CONST_METHOD2( smcRegisterSize, int(uint8_t, KnlDeviceBase::AccessMode*) );

private:
    int m_devnum;
};

typedef KnlDeviceAbstract<KnlMockBase, MockMpssStack, MpssCreator, MockScifConnection>
        KnlDeviceUnderTest;


class KnlDeviceTest : public ::testing::Test
{
protected:
    MockScifConnection* scif;
    MockMpssStack* mpss;

    KnlDeviceUnderTest dev;

protected:
    KnlDeviceTest() : scif(0), mpss(0) { }
    virtual void SetUp()
    {
        scif = dev.getScifDev();
        mpss = dev.getMpss();
    }

};


// New type for our KnlDeviceStateTest:
//      first = MpssStackBase::DeviceState to be set by internal call
//              to MpssStackBase::getDeviceState
//      second = Expected MicDevice::DeviceState
typedef std::pair<uint32_t, uint32_t> StateTestData;
// Parameterized test class for available device states
class KnlDeviceStateTest : public KnlDeviceTest,
    public ::testing::WithParamInterface<StateTestData>
{
};

TEST_F(KnlDeviceTest, TC_ctor_001)
{
    ASSERT_NO_THROW( KnlDeviceUnderTest() );
}

TEST_F(KnlDeviceTest, TC_isdeviceopen_001)
{
    EXPECT_CALL( *scif, isOpen() )
        .WillOnce( Return(true) )
        .WillOnce( Return(false) );
    ASSERT_TRUE( dev.isDeviceOpen() );
    ASSERT_FALSE( dev.isDeviceOpen() );
}


TEST_P(KnlDeviceStateTest, TC_getdevicestate_001)
{
    auto data = GetParam();
    auto retState = data.first;
    auto expected = data.second;
    MicDevice::DeviceState state;
    EXPECT_CALL( *mpss, getDeviceState(_) )
        .WillOnce(
            DoAll(
                SetArgPointee<0>(retState),
                Return(MIC_SUCCESS)
        ));
    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceState(&state) );
    ASSERT_EQ( expected, state );
}

INSTANTIATE_TEST_CASE_P(ValidStates,
    KnlDeviceStateTest,
    ::testing::Values(
        StateTestData(MpssStackBase::eStateReady, MicDevice::eReady),
        StateTestData(MpssStackBase::eStateBooting, MicDevice::eReboot),
        StateTestData(MpssStackBase::eStateNoResponse, MicDevice::eOffline),
        StateTestData(MpssStackBase::eStateShutdown, MicDevice::eOffline),
        StateTestData(MpssStackBase::eStateResetFailed, MicDevice::eOffline),
        StateTestData(MpssStackBase::eStateBootFailed, MicDevice::eOffline),
        StateTestData(MpssStackBase::eStateOnline, MicDevice::eOnline),
        StateTestData(MpssStackBase::eStateNodeLost, MicDevice::eLost),
        StateTestData(MpssStackBase::eStateResetting, MicDevice::eReset),
        StateTestData(MpssStackBase::eStateInvalid, MicDevice::eError),
        // The following should work as long as eError is always the enum's last element
        StateTestData(MpssStackBase::eStateInvalid + 1, MicDevice::eError)
));

TEST_F(KnlDeviceStateTest, TC_getdevicestate_fail_001)
{
    MicDevice::DeviceState state;
    EXPECT_CALL( *mpss, getDeviceState(_) )
        .WillOnce( Return(1) );
    ASSERT_NE( 0, dev.getDeviceState(&state) );
}

TEST_F(KnlDeviceStateTest, TC_getdevicestate_null_fail_001)
{
    ASSERT_EQ( INVALID_ARG, dev.getDeviceState(NULL) );
}

TEST_F(KnlDeviceTest, TC_getpciconfig_001)
{
    EXPECT_CALL( *mpss, getDevicePciConfigData(_) )
        .WillOnce( Return(MIC_SUCCESS) );

    MicPciConfigInfo pci;
    ASSERT_EQ( MIC_SUCCESS, dev.getDevicePciConfigInfo(&pci) );
}

TEST_F(KnlDeviceTest, TC_getpciconfig_null_fail_001)
{
    ASSERT_EQ( INVALID_ARG, dev.getDevicePciConfigInfo(NULL) );
}

TEST_F(KnlDeviceTest, TC_getpciconfig_mpssstack_fail_001)
{
    EXPECT_CALL( *mpss, getDevicePciConfigData(_) )
        .WillOnce( Return(INTERNAL_ERROR) );
    MicPciConfigInfo pci;
    ASSERT_EQ( INTERNAL_ERROR, dev.getDevicePciConfigInfo(&pci) );
}

TEST_F(KnlDeviceTest, TC_getprocinfo_001)
{
    EXPECT_CALL( *mpss, getDeviceProperty( An<uint32_t*>(), _, _ ) )
        .WillRepeatedly( Return(MIC_SUCCESS) );
    EXPECT_CALL( *mpss, getDeviceProperty( An<std::string*>(), _ ) )
        .WillRepeatedly( Return(MIC_SUCCESS) );
    MicProcessorInfo procinfo;
    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceProcessorInfo(&procinfo) );
    ASSERT_TRUE( procinfo.sku().isValid() );
}

TEST_F(KnlDeviceTest, TC_getprocinfo_getdevprop_fail_001)
{
    // Even though calls to getDeviceProperty() fail, expect success.
    // The .valid() of each MicValue in the MicProcessorInfo should be false
    EXPECT_CALL( *mpss, getDeviceProperty( An<uint32_t*>(), _, _ ) )
        .WillRepeatedly( Return(INTERNAL_ERROR) );
    EXPECT_CALL( *mpss, getDeviceProperty( An<std::string*>(), _ ) )
        .WillRepeatedly( Return(INTERNAL_ERROR) );
    MicProcessorInfo procinfo;
    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceProcessorInfo(&procinfo) );
    ASSERT_FALSE( procinfo.sku().isValid() );
}

TEST_F(KnlDeviceTest, TC_getprocinfo_null_fail_001)
{
    ASSERT_EQ( INVALID_ARG, dev.getDevicePciConfigInfo(NULL) );
}

TEST_F(KnlDeviceTest, TC_getversioninfo_001)
{
    EXPECT_CALL( *mpss, getDeviceProperty(_, _) )
        .WillRepeatedly( Return(MIC_SUCCESS) );
    EXPECT_CALL( *mpss, getSystemMpssVersion(_) )
        .WillRepeatedly( Return(MIC_SUCCESS) );
    EXPECT_CALL( *mpss, getSystemDriverVersion(_) )
        .WillRepeatedly( Return(MIC_SUCCESS) );
    EXPECT_CALL( *scif, isOpen() )
        .WillRepeatedly( Return(true) );
    EXPECT_CALL( *scif, request(_) )
        .WillRepeatedly( Return(MIC_SUCCESS) );
    MicVersionInfo info;
    ASSERT_EQ(MIC_SUCCESS, dev.getDeviceVersionInfo(&info) );
    ASSERT_TRUE( info.flashVersion().isValid() );
}

TEST_F(KnlDeviceTest, TC_getversioninfo_scifdevice_fail_001)
{
    EXPECT_CALL( *mpss, getDeviceProperty(_, _) )
        .WillRepeatedly( Return(MIC_SUCCESS) );
    EXPECT_CALL( *mpss, getSystemMpssVersion(_) )
        .WillRepeatedly( Return(MIC_SUCCESS) );
    EXPECT_CALL( *mpss, getSystemDriverVersion(_) )
        .WillRepeatedly( Return(MIC_SUCCESS) );
    EXPECT_CALL( *scif, isOpen() )
        .WillRepeatedly( Return(true) );
    EXPECT_CALL( *scif, request(_) )
        .WillRepeatedly( Return(INTERNAL_ERROR) );
    MicVersionInfo info;
    ASSERT_EQ(MIC_SUCCESS, dev.getDeviceVersionInfo(&info) );
    ASSERT_TRUE( info.flashVersion().isValid() );
    ASSERT_FALSE( info.smcBootLoaderVersion().isValid() );
}

TEST_F(KnlDeviceTest, TC_getversioninfo_mpssstack_fail_001)
{
    EXPECT_CALL( *mpss, getDeviceProperty(_, _) )
        .WillRepeatedly( Return(INTERNAL_ERROR) );
    EXPECT_CALL( *mpss, getSystemMpssVersion(_) )
        .WillRepeatedly( Return(INTERNAL_ERROR) );
    EXPECT_CALL( *mpss, getSystemDriverVersion(_) )
        .WillRepeatedly( Return(INTERNAL_ERROR) );
    EXPECT_CALL( *scif, isOpen() )
        .WillRepeatedly( Return(false) );
    MicVersionInfo info;
    ASSERT_EQ(MIC_SUCCESS, dev.getDeviceVersionInfo(&info) );
    ASSERT_FALSE( info.flashVersion().isValid() );
}

TEST_F(KnlDeviceTest, TC_getversioninfo_null_fail_001)
{
    ASSERT_EQ( INVALID_ARG, dev.getDeviceVersionInfo(NULL) );
}

TEST_F(KnlDeviceTest, TC_getmeminfo_001)
{
    EXPECT_CALL( *scif, request(_) )
        .WillOnce( Return(MIC_SUCCESS) );
    MicMemoryInfo info;
    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceMemoryInfo(&info) );
    ASSERT_NE( "", info.memoryType() );
}

TEST_F(KnlDeviceTest, TC_getmeminfo_scifdevice_fail_001)
{
    EXPECT_CALL( *scif, request(_) )
        .WillOnce( Return(INTERNAL_ERROR) );
    MicMemoryInfo info;
    ASSERT_EQ( MICSDKERR_DEVICE_IO_ERROR, dev.getDeviceMemoryInfo(&info) );
    ASSERT_EQ( "", info.memoryType() );
}

TEST_F(KnlDeviceTest, TC_getmeminfo_null_fail_001)
{
    ASSERT_EQ( INVALID_ARG, dev.getDeviceMemoryInfo(NULL) );
}

/////////////////////////////////////////////////////////////////////////////
//
//          Tests for getDeviceFlashUpdateStatus()
//
/////////////////////////////////////////////////////////////////////////////

// Local struct for representing expected values set in
// getDeviceFlashUpdateStatus
struct FlashStatusTestData : public KnlDeviceEnums
{
    // The value that is going to be returned by our mock MpssStack,
    // representing a SPAD reading
    int                         progress; // Percentage of completion
    int                         state;    // State returned in spad (idle, busy,
                                          // complete, failed.
    KnlDeviceBase::FlashStage   stage;    // Flashing stage (BIOS, ME, SMC)
    std::string                 stageText; // Stage text
    bool                        completed; // Signal stage finished

    FlashStatusTestData(int prog, int state_,
                        KnlDeviceBase::FlashStage stage_, std::string stageText_,
                        bool comp):
        progress(prog), state(state_), stage(stage_),
        stageText(stageText_), completed(comp)
    {
    }

    FlashStatusTestData (const FlashStatusTestData&) = default;
};

class FlashUpdateStatusTest : public KnlDeviceTest,
    public ::testing::WithParamInterface<FlashStatusTestData>
{
};

TEST_P(FlashUpdateStatusTest, TC_busy_001)
{
    FlashStatusTestData data = GetParam();
    uint32_t spadReading = 0;
    // Byte 0   : 4 lsb : status
    // Byte 1   : 7 lsb : progress
    // Byte 2   : 3 lsb : stage number

    spadReading |= data.state & 0x000f;
    spadReading |= (data.progress & 0x007f) << 8;
    spadReading |= (data.stage & 0x0007) << 16;

    if (data.completed)
        spadReading |= 0x0002; // somehow, a '2' in the LSB means done...

    EXPECT_CALL( *mpss, getDeviceProperty( An<uint32_t*>(), _, _ ) )
        .WillRepeatedly(
            DoAll(
                SetArgPointee<0>(spadReading),
                Return(MIC_SUCCESS)
            )
        );

    FlashStatus fstatus;

    dev.setBinariesToUpdate(false, false, false, 0);
    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceFlashUpdateStatus( &fstatus ) );
    //ASSERT_EQ( FlashStatus::eDone, fstatus.state() ); //No update took place, so Done
    ASSERT_EQ( data.stage, fstatus.stage() );
    ASSERT_EQ( data.progress, fstatus.progress() );
    ASSERT_EQ( data.stageText, fstatus.stageText() );

    dev.setBinariesToUpdate(true, true, true, 0); // mock we are updating all binaries...

    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceFlashUpdateStatus( &fstatus ) );
    ASSERT_EQ( data.state, fstatus.state() );
    ASSERT_EQ( data.stage, fstatus.stage() );
    ASSERT_EQ( data.progress, fstatus.progress() );
    ASSERT_EQ( data.stageText, fstatus.stageText() );
}

INSTANTIATE_TEST_CASE_P(FlashStatusValues,
    FlashUpdateStatusTest,
    ::testing::Values(
        FlashStatusTestData(50, 0, KnlDeviceBase::eIdleStage,
                            "Idle", false),
        FlashStatusTestData(50, 1, KnlDeviceBase::eBiosStage,
                            "BIOS", false),
        FlashStatusTestData(50, 1, KnlDeviceBase::eMeStage,
                            "ME", false),
        FlashStatusTestData(50, 1, KnlDeviceBase::eSmcStage,
                            "SMC", false)
));

TEST_F(FlashUpdateStatusTest, TC_stagecompleted_001)
{
    FlashStatusTestData data(100, 2, KnlDevice::eBiosStage,
                             "BIOS", true);
    uint32_t spadReading = 0;
    // Byte 0   : 4 lsb : status
    // Byte 1   : 7 lsb : progress
    // Byte 2   : 3 lsb : stage number

    spadReading |= data.state & 0x000f;
    spadReading |= (data.progress & 0x007f) << 8;
    spadReading |= (data.stage & 0x0007) << 16;

    if (data.completed)
        spadReading |= 0x0002; // somehow, a '2' in the LSB means done...

    EXPECT_CALL( *mpss, getDeviceProperty( An<uint32_t*>(), _, _ ) )
        .WillRepeatedly(
            DoAll(
                SetArgPointee<0>(spadReading),
                Return(MIC_SUCCESS)
            )
        );

    FlashStatus fstatus;

    dev.setBinariesToUpdate(true, true, true, 0); // mock we are updating all binaries...

    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceFlashUpdateStatus( &fstatus ) );
    ASSERT_EQ( FlashStatus::eStageComplete, fstatus.state() );
}

TEST_F(FlashUpdateStatusTest, TC_failed_001)
{
    FlashStatusTestData data(100, 3, KnlDevice::eBiosStage,
                             "BIOS", true);
    uint32_t spadReading = 0;
    // Byte 0   : 4 lsb : status
    // Byte 1   : 7 lsb : progress
    // Byte 2   : 3 lsb : stage number

    spadReading |= data.state & 0x000f;
    spadReading |= (data.progress & 0x007f) << 8;
    spadReading |= (data.stage & 0x0007) << 16;

    if (data.completed)
        spadReading |= 0x0002; // somehow, a '2' in the LSB means done...

    EXPECT_CALL( *mpss, getDeviceProperty( An<uint32_t*>(), _, _ ) )
        .WillRepeatedly(
            DoAll(
                SetArgPointee<0>(spadReading),
                Return(MIC_SUCCESS)
            )
        );

    FlashStatus fstatus;

    dev.setBinariesToUpdate(true, true, true, 0); // mock we are updating all binaries...

    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceFlashUpdateStatus( &fstatus ) );
    ASSERT_EQ( FlashStatus::eFailed, fstatus.state() );
}

TEST_F(FlashUpdateStatusTest, TC_authfailed_001)
{
    FlashStatusTestData data(100, 4, KnlDevice::eBiosStage, // Auth failed
                             "BIOS", true);
    uint32_t spadReading = 0;
    // Byte 0   : 4 lsb : status
    // Byte 1   : 7 lsb : progress
    // Byte 2   : 3 lsb : stage number

    spadReading |= data.state & 0x000f;
    spadReading |= (data.progress & 0x007f) << 8;
    spadReading |= (data.stage & 0x0007) << 16;

    EXPECT_CALL( *mpss, getDeviceProperty( An<uint32_t*>(), _, _ ) )
        .WillOnce(
            DoAll(
                SetArgPointee<0>(spadReading),
                Return(MIC_SUCCESS)
            )
        );

    FlashStatus fstatus;

    dev.setBinariesToUpdate(true, true, true, 0); // mock we are updating all binaries...

    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceFlashUpdateStatus( &fstatus ) );
    ASSERT_EQ( FlashStatus::eAuthFailedVM, fstatus.state() );
}

/////////////////////////////////////////////////////////////////////////////
//
//          Tests for getDeviceSmcRegisterData()
//
/////////////////////////////////////////////////////////////////////////////
TEST_F(KnlDeviceTest, TC_smcregisterdata_invalid_smc_reg_001)
{
    EXPECT_CALL( dev, smcRegisterSize(_, _) )
        .WillOnce( Return(-1) );
    int n;
    size_t size = sizeof(n);
    ASSERT_EQ( MICSDKERR_INVALID_SMC_REG_OFFSET,
        dev.getDeviceSmcRegisterData( 0x60, (uint8_t*) &n, &size )
    );
}

TEST_F(KnlDeviceTest, TC_smcregisterdata_opnotpermitted_001)
{
    int n;
    size_t size = sizeof(n);

    // smcRegisterSize returns eNoAccess
    EXPECT_CALL( dev, smcRegisterSize(_, _) )
        .WillOnce(
            DoAll(
                SetArgPointee<1>( KnlDeviceBase::eNoAccess ),
                Return(size)
            )
        );
    ASSERT_EQ( MICSDKERR_SMC_OP_NOT_PERMITTED,
        dev.getDeviceSmcRegisterData( 0x60, (uint8_t*) &n, &size )
    );

    // smcRegisterSize returns eWriteOnly
    EXPECT_CALL( dev, smcRegisterSize(_, _) )
        .WillOnce(
            DoAll(
                SetArgPointee<1>( KnlDeviceBase::eWriteOnly ),
                Return(size)
            )
        );
    ASSERT_EQ( MICSDKERR_SMC_OP_NOT_PERMITTED,
        dev.getDeviceSmcRegisterData( 0x60, (uint8_t*) &n, &size )
    );
}

TEST_F(KnlDeviceTest, TC_smcregisterdata_buffer_001)
{
    size_t size = 0;
    size_t expectedSize = 4;
    EXPECT_CALL( dev, smcRegisterSize(_, _) )
        .WillRepeatedly(
            DoAll(
                SetArgPointee<1>( KnlDeviceBase::eReadOnly ),
                Return(expectedSize)
            )
        );
    ASSERT_EQ( MIC_SUCCESS,
        dev.getDeviceSmcRegisterData( 0x60, nullptr, &size )
    );

    ASSERT_EQ( expectedSize, size );

    // Pass in null buffer and size != 0
    ASSERT_EQ( INVALID_ARG,
        dev.getDeviceSmcRegisterData( 0x60, nullptr, &size )
    );

    // Pass in null buffer and null size ptr
    ASSERT_EQ( INVALID_ARG,
        dev.getDeviceSmcRegisterData( 0x60, nullptr, nullptr )
    );

    // Pass in valid buffer and null size ptr
    uint8_t c;
    ASSERT_EQ( INVALID_ARG,
        dev.getDeviceSmcRegisterData( 0x60, &c, nullptr )
    );
}

TEST_F(KnlDeviceTest, TC_smcregisterdata_smcreq_fails_001)
{
    int n;
    size_t expectedSize = sizeof(n);
    size_t size = expectedSize;
    EXPECT_CALL( dev, smcRegisterSize(_, _) )
        .WillRepeatedly(
            DoAll(
                SetArgPointee<1>( KnlDeviceBase::eReadOnly ),
                Return(expectedSize)
            )
        );

    // Scif device returns an error
    EXPECT_CALL( *scif, smcRequest(_) )
        .WillOnce( Return(MICSDKERR_INTERNAL_ERROR) );

    ASSERT_EQ( MICSDKERR_DEVICE_IO_ERROR,
        dev.getDeviceSmcRegisterData( 0x60, (uint8_t*) &n, &size )
    );

}

TEST_F(KnlDeviceTest, TC_success_001)
{
    int n;
    size_t expectedSize = sizeof(n);
    size_t size = expectedSize;
    EXPECT_CALL( dev, smcRegisterSize(_, _) )
        .WillRepeatedly(
            DoAll(
                SetArgPointee<1>( KnlDeviceBase::eReadOnly ),
                Return(expectedSize)
            )
        );

    EXPECT_CALL( *scif, smcRequest(_) )
        .WillRepeatedly( Return(MIC_SUCCESS) );

    ASSERT_EQ( MICSDKERR_SUCCESS,
        dev.getDeviceSmcRegisterData( 0x60, (uint8_t*) &n, &size )
    );


    size = 8; // more than required (4, as per the EXPECT_CALL above)
    ASSERT_EQ( MICSDKERR_SUCCESS,
        dev.getDeviceSmcRegisterData( 0x60, (uint8_t*) &n, &size )
    );
}

/////////////////////////////////////////////////////////////////////////////
//
//          Tests for getDevicePlatformInfo()
//
/////////////////////////////////////////////////////////////////////////////
struct HwRevisionTestData
{
    uint32_t        hwRev;
    std::string     boardStr;
    std::string     fabStr;
    std::string     tdpStr;
    std::string     coolingStr;

    HwRevisionTestData( uint32_t hwr, const std::string& bs,
                        const std::string& fs, const std::string& ts,
                        const std::string cs) :
        hwRev(hwr), boardStr(bs), fabStr(fs), tdpStr(ts), coolingStr(cs)
    {
    }


    HwRevisionTestData( const HwRevisionTestData& ) = default;
};

class PlatformInfoTest : public KnlDeviceTest,
    public ::testing::WithParamInterface<HwRevisionTestData>
{
protected:
    DeviceInfo devinfo;
    const std::string dummyStr = "dummy";

    virtual void SetUp()
    {
        KnlDeviceTest::SetUp();
        std::memset( &devinfo, 0, sizeof(DeviceInfo) );
        std::strncpy( devinfo.os_version, dummyStr.c_str(), sizeof(devinfo.os_version) );
        std::strncpy( devinfo.bios_version, dummyStr.c_str(),
                sizeof(devinfo.bios_version) );
        std::strncpy( devinfo.bios_release_date, dummyStr.c_str(),
                sizeof(devinfo.bios_release_date) );
        std::strncpy( devinfo.uuid, dummyStr.c_str(), sizeof(devinfo.uuid) );
        std::strncpy( devinfo.part_number, dummyStr.c_str(),
                sizeof(devinfo.part_number) );
        std::strncpy( devinfo.manufacture_date, dummyStr.c_str(),
                sizeof(devinfo.manufacture_date) );
        std::strncpy( devinfo.serialno, dummyStr.c_str(), sizeof(devinfo.serialno) );
    }

public:
    template <uint32_t ret>
    uint32_t fakeRequest( ScifRequestInterface *req )
    {
        std::memcpy(req->buffer(), &devinfo, req->byteCount());
        req->setError(ret);
        return ret;
    }
};

TEST_P(PlatformInfoTest, TC_success_001)
{
    HwRevisionTestData data = GetParam();

    devinfo.hw_revision = data.hwRev;
    EXPECT_CALL( *scif, request(_) )
        .WillOnce(
                Invoke(this, &PlatformInfoTest::fakeRequest<0>)
        );

    EXPECT_CALL( *mpss, getDeviceProperty(_, _) )
        .WillRepeatedly(
            DoAll(
                SetArgPointee<0>(dummyStr),
                Return(MIC_SUCCESS)
            )
        );

    MicPlatformInfo platinfo;
    ASSERT_EQ( MIC_SUCCESS, dev.getDevicePlatformInfo( &platinfo ) );

    std::string features = platinfo.featureSet().value();

    // Check for the HW features as expected from our HwRevisionTestData
    ASSERT_NE( std::string::npos, features.find(data.boardStr) );
    ASSERT_NE( std::string::npos, features.find(data.fabStr) );
    ASSERT_NE( std::string::npos, features.find(data.tdpStr) );
    ASSERT_NE( std::string::npos, features.find(data.coolingStr) );

    EXPECT_EQ( data.coolingStr == "Active", platinfo.isCoolingActive().value() );
}

INSTANTIATE_TEST_CASE_P(PlatformInfoHwRevision,
    PlatformInfoTest,
    ::testing::Values(
        // Board type
        HwRevisionTestData(0x00000000, "RP", "Fab A", "225W", "Passive"),
        HwRevisionTestData(0x00010000, "RPRD", "Fab A", "225W", "Passive"),
        HwRevisionTestData(0x00020000, "STHI", "Fab A", "225W", "Passive"),
        // Fab
        HwRevisionTestData(0x00000100, "RP", "Fab B", "225W", "Passive"),
        HwRevisionTestData(0x00000200, "RP", "Fab C", "225W", "Passive"),
        HwRevisionTestData(0x00000300, "RP", "Fab D", "225W", "Passive"),
        // TDP
        HwRevisionTestData(0x00000002, "RP", "Fab A", "300W", "Passive"),
        // Cooling
        HwRevisionTestData(0x00000004, "RP", "Fab A", "225W", "Active")
));

TEST_F(PlatformInfoTest, TC_invalidarg_001)
{
    ASSERT_EQ( MICSDKERR_INVALID_ARG, dev.getDevicePlatformInfo(nullptr) );
}

TEST_F(PlatformInfoTest, TC_scif_and_deviceprop_fail_001)
{
    EXPECT_CALL( *scif, request(_) )
        .WillOnce(
                Invoke(this, &PlatformInfoTest::fakeRequest<-1>)
        );

    EXPECT_CALL( *mpss, getDeviceProperty(_, _ ) )
        .WillRepeatedly( Return(MICSDKERR_INTERNAL_ERROR) );

    MicPlatformInfo info;
    ASSERT_EQ( MIC_SUCCESS, dev.getDevicePlatformInfo(&info) );
    ASSERT_FALSE( info.featureSet().isValid() );
    ASSERT_FALSE( info.isCoolingActive().isValid() );
    ASSERT_FALSE( info.manufactureDate().isValid() );
    ASSERT_FALSE( info.manufactureTime().isValid() );
    ASSERT_FALSE( info.serialNo().isValid() );
    ASSERT_FALSE( info.uuid().isValid() );
    ASSERT_FALSE( info.coprocessorOs().isValid() );
    ASSERT_FALSE( info.coprocessorOsVersion().isValid() );
    ASSERT_FALSE( info.coprocessorBrand().isValid() );
    ASSERT_FALSE( info.boardSku().isValid() );
    ASSERT_FALSE( info.boardType().isValid() );
    ASSERT_FALSE( info.strapInfo().isValid() );
    ASSERT_FALSE( info.strapInfoRaw().isValid() );
    ASSERT_FALSE( info.smBusBaseAddress().isValid() );
    ASSERT_FALSE( info.maxPower().isValid() );
}

TEST_F(PlatformInfoTest, TC_driver_unloaded_001)
{
    EXPECT_CALL( *scif, request(_) )
        .WillOnce(
                Invoke(this, &PlatformInfoTest::fakeRequest<-1>)
        );

    EXPECT_CALL( *mpss, getDeviceProperty(_, _ ) )
        .WillRepeatedly( Return(MICSDKERR_DRIVER_NOT_LOADED) );

    MicPlatformInfo info;
    ASSERT_EQ( MICSDKERR_DRIVER_NOT_LOADED, dev.getDevicePlatformInfo(&info) );
    ASSERT_FALSE( info.featureSet().isValid() );
    ASSERT_FALSE( info.isCoolingActive().isValid() );
    ASSERT_FALSE( info.manufactureDate().isValid() );
    ASSERT_FALSE( info.manufactureTime().isValid() );
    ASSERT_FALSE( info.serialNo().isValid() );
    ASSERT_FALSE( info.uuid().isValid() );
    ASSERT_FALSE( info.coprocessorOs().isValid() );
    ASSERT_FALSE( info.coprocessorOsVersion().isValid() );
    ASSERT_FALSE( info.coprocessorBrand().isValid() );
    ASSERT_FALSE( info.boardSku().isValid() );
    ASSERT_FALSE( info.boardType().isValid() );
    ASSERT_FALSE( info.strapInfo().isValid() );
    ASSERT_FALSE( info.strapInfoRaw().isValid() );
    ASSERT_FALSE( info.smBusBaseAddress().isValid() );
    ASSERT_FALSE( info.maxPower().isValid() );
}

/////////////////////////////////////////////////////////////////////////////
//
//          Tests for deviceOpen()
//
/////////////////////////////////////////////////////////////////////////////
class KnlDeviceTest_deviceOpen : public KnlDeviceTest
{
protected:
    SystoolsdInfo protInfo;
    const uint8_t major = SYSTOOLSD_MAJOR_VER;
    const uint8_t minor = SYSTOOLSD_MINOR_VER;

    virtual void SetUp()
    {
        KnlDeviceTest::SetUp();
        std::memset( &protInfo, 0, sizeof(protInfo) );
        protInfo.major_ver = major;
        protInfo.minor_ver = minor;

        // Default expectations for scif's request() and open()
        ON_CALL( *scif, open() )
            .WillByDefault( Return(MIC_SUCCESS) );
        ON_CALL( *scif, request(_) )
            .WillByDefault( Invoke(this, &KnlDeviceTest_deviceOpen::fakeRequest<0>) );
    }

public:
    template <uint32_t ret>
    uint32_t fakeRequest( ScifRequestInterface *req )
    {
        std::memcpy(req->buffer(), &protInfo, req->byteCount());
        req->setError(ret);
        return ret;
    }
};


TEST_F(KnlDeviceTest_deviceOpen, TC_success_001)
{
    ASSERT_EQ( MIC_SUCCESS, dev.deviceOpen() );
}

TEST_F(KnlDeviceTest_deviceOpen, TC_open_fail_001)
{
    EXPECT_CALL( *scif, open() )
        .WillRepeatedly( Return(MICSDKERR_INTERNAL_ERROR) );
    ASSERT_EQ( MICSDKERR_DEVICE_OPEN_FAILED, dev.deviceOpen() );
}

TEST_F(KnlDeviceTest_deviceOpen, TC_systoolsd_fail_001)
{
    EXPECT_CALL( *scif, request(_) )
        .WillOnce(
            Invoke(this,
                   &KnlDeviceTest_deviceOpen::fakeRequest<1> // Return an error
            )
        );

    ASSERT_EQ( MICSDKERR_DEVICE_IO_ERROR, dev.deviceOpen() );

}

TEST_F(KnlDeviceTest_deviceOpen, TC_systoolsd_proto_mismatch_001)
{
    protInfo.major_ver++; // incorrect major version
    ASSERT_EQ( MICSDKERR_VERSION_MISMATCH, dev.deviceOpen() );

    protInfo.major_ver--; // reset
    protInfo.minor_ver++; // incorrect minor version
    ASSERT_EQ( MICSDKERR_VERSION_MISMATCH, dev.deviceOpen() );
}

/////////////////////////////////////////////////////////////////////////////
//
//          Tests for getDeviceTurboMode()
//
/////////////////////////////////////////////////////////////////////////////
class KnlDeviceTest_TurboMode : public KnlDeviceTest
{
protected:
    TurboInfo turbo;

    virtual void SetUp()
    {
        KnlDeviceTest::SetUp();
        std::memset( &turbo, 0, sizeof(turbo) );
        turbo.enabled = 1;
        turbo.turbo_pct = 1;

        // Default expectations for scif's request() and open()
        ON_CALL( *scif, request(_) )
            .WillByDefault( Invoke(this, &KnlDeviceTest_TurboMode::fakeRequest<0>) );
    }

public:
    template <uint32_t ret>
    uint32_t fakeRequest( ScifRequestInterface *req )
    {
        std::memcpy(req->buffer(), &turbo, req->byteCount());
        req->setError(ret);
        return ret;
    }
};

TEST_F(KnlDeviceTest_TurboMode, TC_success_001)
{
    bool enabled, available, active;
    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceTurboMode(&enabled, &available, &active) );
    ASSERT_TRUE( enabled );
    ASSERT_TRUE( available );
    ASSERT_TRUE( active );
}

TEST_F(KnlDeviceTest_TurboMode, TC_nullptr_001)
{
    ASSERT_EQ( MICSDKERR_INVALID_ARG,
               dev.getDeviceTurboMode( nullptr, nullptr, nullptr) );
}

TEST_F(KnlDeviceTest_TurboMode, TC_systoolsd_fail_001)
{
    bool a, e, act;
    EXPECT_CALL( *scif, request(_) )
        .WillOnce(
            Invoke( this, &KnlDeviceTest_TurboMode::fakeRequest<1> ) );

    ASSERT_EQ( MICSDKERR_DEVICE_IO_ERROR,
               dev.getDeviceTurboMode( &a, &e, &act ) );
}

TEST_F(KnlDeviceTest_TurboMode, TC_turbo_values_001)
{
    bool enabled;
    turbo.enabled = 0;
    turbo.turbo_pct = 0;
    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceTurboMode( &enabled, nullptr, nullptr ) );
    ASSERT_FALSE( enabled );

    turbo.enabled = 1;
    turbo.turbo_pct = 0;
    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceTurboMode( &enabled, nullptr, nullptr ) );
    ASSERT_TRUE( enabled );

    turbo.enabled = 0;
    turbo.turbo_pct = 1;
    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceTurboMode( &enabled, nullptr, nullptr ) );
    ASSERT_FALSE( enabled );

    turbo.enabled = 1;
    turbo.turbo_pct = 1;
    ASSERT_EQ( MIC_SUCCESS, dev.getDeviceTurboMode( &enabled, nullptr, nullptr ) );
    ASSERT_TRUE( enabled );
}

/////////////////////////////////////////////////////////////////////////////
//
//          Tests for setDeviceSmcRegisterData()
//
/////////////////////////////////////////////////////////////////////////////
typedef KnlDeviceTest KnlDeviceTest_setDeviceSmcRegisterData;
TEST_F(KnlDeviceTest_setDeviceSmcRegisterData, TC_success_001)
{
    uint8_t c;
    uint8_t offs = 0x60; // whatever...
    size_t size = sizeof(c);
    EXPECT_CALL( dev, smcRegisterSize(_, _) )
        .WillOnce(
            DoAll(
                SetArgPointee<1>( KnlDeviceBase::eWriteOnly ),
                Return(size)
            )

        );
    EXPECT_CALL( *scif, smcRequest(_) )
        .WillOnce( Return(MIC_SUCCESS) );

    ASSERT_EQ( MIC_SUCCESS, dev.setDeviceSmcRegisterData( offs, &c, size ) );
}

TEST_F(KnlDeviceTest_setDeviceSmcRegisterData, TC_nullptr)
{
    uint8_t c;
    ASSERT_EQ( MICSDKERR_INVALID_ARG, dev.setDeviceSmcRegisterData(0x60, nullptr, 4) );
    ASSERT_EQ( MICSDKERR_INVALID_ARG, dev.setDeviceSmcRegisterData(0x60, &c, 0) );
}

TEST_F(KnlDeviceTest_setDeviceSmcRegisterData, TC_invalid_smcreg_001)
{
    uint8_t c;
    size_t size = sizeof(c);
    EXPECT_CALL( dev, smcRegisterSize(_, _) )
        .WillOnce( Return(-1) );
    ASSERT_EQ(MICSDKERR_INVALID_SMC_REG_OFFSET,
        dev.setDeviceSmcRegisterData(0x60, &c, size) );
}

TEST_F(KnlDeviceTest_setDeviceSmcRegisterData, TC_bad_access_001)
{
    uint8_t c;
    size_t size = sizeof(c);
    // eNoAccess
    EXPECT_CALL( dev, smcRegisterSize(_, _) )
        .WillOnce(
            DoAll(
                SetArgPointee<1>(KnlDeviceBase::eNoAccess),
                Return(0)
            )
        );
    ASSERT_EQ(MICSDKERR_SMC_OP_NOT_PERMITTED,
        dev.setDeviceSmcRegisterData(0x60, &c, size) );

    // eReadOnly
    EXPECT_CALL( dev, smcRegisterSize(_, _) )
        .WillOnce(
            DoAll(
                SetArgPointee<1>(KnlDeviceBase::eReadOnly),
                Return(0)
            )
        );
    ASSERT_EQ(MICSDKERR_SMC_OP_NOT_PERMITTED,
        dev.setDeviceSmcRegisterData(0x60, &c, size) );
}

TEST_F(KnlDeviceTest, TC_powerSensorAsText_001)
{
    using PowerSensor = KnlDevice::PowerSensor;
    auto sensorAsText = &KnlDevice::powerSensorAsText; // alias for short, thanks C++11
    std::vector<std::pair<PowerSensor, std::string>> values = {
        {KnlDeviceEnums::ePciePower,    "PCIe"},
        {KnlDeviceEnums::e2x3Power,     "2x3"},
        {KnlDeviceEnums::e2x4Power,     "2x4"},
        {KnlDeviceEnums::eAvg0Power,    "Average 0"},
        {KnlDeviceEnums::eCurrentPower, "Current"},
        {KnlDeviceEnums::eMaximumPower, "Maximum"},
        {KnlDeviceEnums::eVccpPower,    "VCCP"},
        {KnlDeviceEnums::eVccuPower,    "VCCU"},
        {KnlDeviceEnums::eVccclrPower,  "VCCCLR"},
        {KnlDeviceEnums::eVccmlbPower,  "VCCMLB"},
        {KnlDeviceEnums::eVccmpPower,   "VCCMP"},
        {KnlDeviceEnums::eNtb1Power,    "NTB1"}
    };

    for (const auto& value : values)
    {
        ASSERT_EQ( value.second, sensorAsText(value.first) );
    }

    ASSERT_EQ( std::string("Unknown"),
        sensorAsText((PowerSensor)-1) );

}

TEST_F(KnlDeviceTest, TC_voltSensorAsText_001)
{
    using VoltSensor = KnlDevice::VoltSensor;
    auto sensorAsText = &KnlDevice::voltSensorAsText;
    std::vector<std::pair<VoltSensor, std::string>> values = {
        {KnlDevice::eVccpVolt,      "VCCP"},
        {KnlDevice::eVccuVolt,      "VCCU"},
        {KnlDevice::eVccclrVolt,    "VCCCLR"},
        {KnlDevice::eVccmlbVolt,    "VCCMLB"},
        {KnlDevice::eVccmpVolt,     "VCCMP"},
        {KnlDevice::eNtb1Volt,      "NTB1"},
        {KnlDevice::eVccpioVolt,    "VCCPIO"},
        {KnlDevice::eVccsfrVolt,    "VCCSFR"},
        {KnlDevice::ePchVolt,       "PCH"},
        {KnlDevice::eVccmfuseVolt,  "VCCMFUSE"},
        {KnlDevice::eNtb2Volt,      "NTB2"},
        {KnlDevice::eVppVolt,       "VPP"}
    };

    for (const auto& value : values)
    {
        ASSERT_EQ( value.second, sensorAsText(value.first) );
    }

    ASSERT_EQ( std::string("Unknown"),
        sensorAsText((VoltSensor)-1) );
}


} // micmgmt
