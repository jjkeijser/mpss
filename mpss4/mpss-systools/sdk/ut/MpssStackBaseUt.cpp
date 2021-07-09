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
#include <gmock/gmock.h>

#include    "MicDeviceError.hpp"
#include    "MpssStackBase.hpp"

using ::testing::_;
using ::testing::An;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace
{

    template <int devtype, uint32_t ret>
    uint32_t fakeSystemDeviceType( int* type, size_t number )
    {
        assert(type);
        *type = devtype;
        (void)number;
        return ret;
    }
} // anon namespace

namespace micmgmt
{

// In oder to test MpssStackBase, we need to inherit from it
class MpssStackForTesting : public micmgmt::MpssStackBase
{
public:
    explicit MpssStackForTesting( int deviceNo ) :
        micmgmt::MpssStackBase( deviceNo )
    {
    }

    virtual ~MpssStackForTesting()
    {
    }

    // Mock this overload of getDeviceProperty() - pure virtual in MpssStackBase
    MOCK_CONST_METHOD2( getDeviceProperty, uint32_t(std::string*, const std::string&) );

    MOCK_CONST_METHOD1( getDeviceType, uint32_t(int*) );
    MOCK_CONST_METHOD2( getSystemDeviceType, uint32_t(int*, size_t) );

    // Empty implementations for the pure virtual functions defined in MpssStackBase
    virtual std::string mpssHomePath() const
    {
        return "mpssHomePath";
    }

    virtual std::string mpssBootPath() const
    {
        return "mpssBootPath";
    }

    virtual std::string mpssFlashPath() const
    {
        return "mpssFlashPath";
    }

    virtual uint32_t getSystemDriverVersion( std::string* version ) const
    {
        (void)version;
        return 0;
    }

    virtual uint32_t getSystemDeviceNumbers( std::vector<size_t>* list ) const
    {
        (void)list;
        return 0;
    }

    virtual uint32_t getSystemProperty( std::string* data, const std::string& name ) const
    {
        (void)data;
        (void)name;
        return 0;
    }


    virtual uint32_t getDeviceState( int* state ) const
    {
        (void)state;
        return 0;
    }

    virtual uint32_t getDevicePciConfigData( micmgmt::PciConfigData* data ) const
    {
        (void)data;
        return 0;
    }

    // Beautiful hack to be able to call MpssStackBase::getDeviceType()
    // MpssStackForTesting implements its own getDeviceType() with gmock
    uint32_t getDeviceTypeProxy( int* type ) const
    {
        return MpssStackBase::getDeviceType( type );
    }
};

class MpssStackBaseTest : public ::testing::Test
{
protected:
    MpssStackForTesting mpss;
    MpssStackBase *pmpss; // for polymorphism

    virtual void SetUp()
    {
    }

public:
    MpssStackBaseTest() : mpss(0), pmpss(&mpss)
    {
    }

};

TEST_F(MpssStackBaseTest, TC_getdeviceprop_001)
{
    const std::string prop = "someprop";
    std::string retStr = "255";
    const uint32_t expected = 255;
    // test values, 255 represented in different bases
    std::vector<std::pair<std::string, int>> values = {
        {"ff",  16},
        {"255", 10},
        {"377", 8},
        {"255", 17} // if base not in {8, 10, 16}, it should default to 10
    };
    // Call the uint32_t overload
    for (auto value : values)
    {
        EXPECT_CALL( mpss, getDeviceProperty(_, _) )
            .WillOnce(
                DoAll(
                    SetArgPointee<0>(value.first),
                    Return( MICSDKERR_SUCCESS )
                )
            );

        uint32_t data;
        ASSERT_EQ( MICSDKERR_SUCCESS,
                   pmpss->getDeviceProperty( (uint32_t*)&data, prop, value.second ) );
        ASSERT_EQ( expected, data );
    }

    // Call the uint16_t overload
    for (auto value : values)
    {
        EXPECT_CALL( mpss, getDeviceProperty(_, _) )
            .WillOnce(
                DoAll(
                    SetArgPointee<0>(value.first),
                    Return( MICSDKERR_SUCCESS )
                )
            );

        uint16_t data;
        ASSERT_EQ( MICSDKERR_SUCCESS,
                   pmpss->getDeviceProperty( (uint16_t*)&data, prop, value.second ) );
        ASSERT_EQ( expected, data );
    }

    // Call the uint8_t overload
    for (auto value : values)
    {
        EXPECT_CALL( mpss, getDeviceProperty(_, _) )
            .WillOnce(
                DoAll(
                    SetArgPointee<0>(value.first),
                    Return( MICSDKERR_SUCCESS )
                )
            );

        uint16_t data;
        ASSERT_EQ( MICSDKERR_SUCCESS,
                   pmpss->getDeviceProperty( (uint8_t*)&data, prop, value.second ) );
        ASSERT_EQ( expected, data );
    }
}

TEST_F(MpssStackBaseTest, TC_getdeviceprop_negative_001)
{
    const std::string retStr = "255";
    const std::string prop = "someprop";
    {
        uint32_t data;
        EXPECT_CALL( mpss, getDeviceProperty(_, _) )
            .WillRepeatedly(
                DoAll(
                    SetArgPointee<0>(retStr),
                    Return( MICSDKERR_SUCCESS )
                )
            );
        ASSERT_EQ( MICSDKERR_INVALID_ARG,
                   pmpss->getDeviceProperty( (uint32_t*)nullptr, prop, 10 ) );
        ASSERT_EQ( MICSDKERR_INVALID_ARG,
                   pmpss->getDeviceProperty( &data, "", 10 ) );
    }

    {
        uint16_t data;
        EXPECT_CALL( mpss, getDeviceProperty(_, _) )
            .WillRepeatedly( Return( MICSDKERR_INTERNAL_ERROR ) );
        ASSERT_EQ( MICSDKERR_INTERNAL_ERROR,
                   pmpss->getDeviceProperty( &data, prop, 10 ) );
    }

    {
        uint8_t data;
        EXPECT_CALL( mpss, getDeviceProperty(_, _) )
            .WillRepeatedly( Return( MICSDKERR_INTERNAL_ERROR ) );
        ASSERT_EQ( MICSDKERR_INTERNAL_ERROR,
                   pmpss->getDeviceProperty( &data, prop, 10 ) );
    }
}

TEST_F(MpssStackBaseTest, TC_whichKnxDevice_001)
{
    EXPECT_CALL( mpss, getDeviceType(_) )
        .WillRepeatedly(
            DoAll(
                SetArgPointee<0>(MpssStackBase::eTypeKnl),
                Return(MICSDKERR_SUCCESS)
            )
        );
    ASSERT_TRUE( pmpss->isKnlDevice() );
    ASSERT_FALSE( pmpss->isKncDevice() );

    EXPECT_CALL( mpss, getDeviceType(_) )
        .WillRepeatedly(
            DoAll(
                SetArgPointee<0>(MpssStackBase::eTypeKnc),
                Return(MICSDKERR_SUCCESS)
            )
        );
    ASSERT_FALSE( pmpss->isKnlDevice() );
    ASSERT_TRUE( pmpss->isKncDevice() );
}

TEST_F(MpssStackBaseTest, TC_getDeviceType_001)
{
    const int expectedType = 0;
    int type;
    EXPECT_CALL( mpss, getSystemDeviceType(_, _) )
        .WillOnce(
            Invoke(
                fakeSystemDeviceType<expectedType, MICSDKERR_SUCCESS>
            )
        );

    ASSERT_EQ( MICSDKERR_SUCCESS, mpss.getDeviceTypeProxy(&type) );
    ASSERT_EQ( expectedType, type );
}

TEST_F(MpssStackBaseTest, TC_getDeviceType_negative_001)
{
    ASSERT_EQ( MICSDKERR_INVALID_ARG, mpss.getDeviceTypeProxy(nullptr) );

    MpssStackForTesting otherMpss(-1);
    int type;
    ASSERT_EQ( MICSDKERR_INTERNAL_ERROR, otherMpss.getDeviceTypeProxy(&type) );
}

TEST_F(MpssStackBaseTest, TC_isMockStack_001)
{
    // Just for coverage...
    MpssStackBase::isMockStack();
}

} // micmgmt

