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
#include "MicPciConfigInfo.hpp"
#include "PciConfigData_p.hpp"  // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicPciConfigInfo_001)
    {
#ifdef __linux__
        const std::string PCI_ADDRESS( "0001:02:03.4" );
#else
        const std::string PCI_ADDRESS( "0001:2:3:4" );
#endif
        const std::string LINK_SPEED_NAME( "speed" );
        const uint32_t LINK_WIDTH = 8;
        const uint32_t PAYLOAD_SIZE = 7;
        const uint32_t REQUEST_SIZE = 6;
        const uint16_t VENDOR_ID = 5;
        const uint16_t DEVICE_ID = 4;
        const uint16_t SUBSYSTEM_ID = 3;
        const uint32_t CLASS_CODE = 2;
        const uint8_t REVISION_ID = 1;
        const uint8_t ASPM_STATE = MicPciConfigInfo::eL0sL1Enabled;

        PciConfigData data;
        data.mClassCode = CLASS_CODE;
        data.mPciAddress = PciAddress( PCI_ADDRESS );
        data.mLinkSpeed = MicSpeed( LINK_SPEED_NAME, 800 );
        data.mLinkWidth = LINK_WIDTH;
        data.mPayloadSize = PAYLOAD_SIZE;
        data.mRequestSize = REQUEST_SIZE;
        data.mVendorId = VENDOR_ID;
        data.mDeviceId = DEVICE_ID;
        data.mSubsystemId = SUBSYSTEM_ID;
        data.mRevisionId = REVISION_ID;
        data.mAspmControl = 0x3;
        data.mNoSnoop = true;
        data.mExtTagEnable = true;
        data.mRelaxedOrder = true;
        data.mHasAccess = true;
        data.mValid = true;

        {   // Empty tests
            MicPciConfigInfo mpci;
            EXPECT_FALSE( mpci.isValid() );
            EXPECT_FALSE( mpci.hasFullAccess() );
            EXPECT_FALSE( mpci.isNoSnoopEnabled() );
            EXPECT_FALSE( mpci.isExtendedTagEnabled() );
            EXPECT_FALSE( mpci.isRelaxedOrderEnabled() );
            EXPECT_EQ( MicPciConfigInfo::eUnknown, mpci.aspmControlState() );
            EXPECT_FALSE( mpci.address().isValid() );
            EXPECT_EQ( 0x0, mpci.vendorId() );
            EXPECT_EQ( 0x0, mpci.deviceId() );
            EXPECT_EQ( ((short)0), mpci.revisionId() );
            EXPECT_EQ( 0x0, mpci.subsystemId() );
            EXPECT_FALSE( mpci.linkSpeed().isValid() );
            EXPECT_EQ( 0U, mpci.linkWidth() );
            EXPECT_EQ( 0U, mpci.payloadSize() );
            EXPECT_EQ( 0U, mpci.requestSize() );
            EXPECT_EQ( 0U, mpci.classCode() );
        }

        {   // Data constructor tests
            MicPciConfigInfo mpci( data );
            EXPECT_TRUE( mpci.isValid() );
            EXPECT_TRUE( mpci.hasFullAccess() );
            EXPECT_TRUE( mpci.isNoSnoopEnabled() );
            EXPECT_TRUE( mpci.isExtendedTagEnabled() );
            EXPECT_TRUE( mpci.isRelaxedOrderEnabled() );
            EXPECT_EQ( ASPM_STATE, mpci.aspmControlState() );
            EXPECT_EQ( PCI_ADDRESS, mpci.address().asString() );
            EXPECT_EQ( VENDOR_ID, mpci.vendorId() );
            EXPECT_EQ( DEVICE_ID, mpci.deviceId() );
            EXPECT_EQ( REVISION_ID, mpci.revisionId() );
            EXPECT_EQ( SUBSYSTEM_ID, mpci.subsystemId() );
            EXPECT_EQ( LINK_SPEED_NAME, mpci.linkSpeed().name() );
            EXPECT_EQ( LINK_WIDTH, mpci.linkWidth() );
            EXPECT_EQ( PAYLOAD_SIZE, mpci.payloadSize() );
            EXPECT_EQ( REQUEST_SIZE, mpci.requestSize() );
            EXPECT_EQ( CLASS_CODE, mpci.classCode() );
        }

        {   // Copy constructor tests
            MicPciConfigInfo mpcithat( data );
            MicPciConfigInfo mpcithis( mpcithat );
            EXPECT_TRUE( mpcithis.isValid() );
            EXPECT_TRUE( mpcithis.hasFullAccess() );
            EXPECT_TRUE( mpcithis.isNoSnoopEnabled() );
            EXPECT_TRUE( mpcithis.isExtendedTagEnabled() );
            EXPECT_TRUE( mpcithis.isRelaxedOrderEnabled() );
            EXPECT_EQ( ASPM_STATE, mpcithis.aspmControlState() );
            EXPECT_EQ( PCI_ADDRESS, mpcithis.address().asString() );
            EXPECT_EQ( VENDOR_ID, mpcithis.vendorId() );
            EXPECT_EQ( DEVICE_ID, mpcithis.deviceId() );
            EXPECT_EQ( REVISION_ID, mpcithis.revisionId() );
            EXPECT_EQ( SUBSYSTEM_ID, mpcithis.subsystemId() );
            EXPECT_EQ( LINK_SPEED_NAME, mpcithis.linkSpeed().name() );
            EXPECT_EQ( LINK_WIDTH, mpcithis.linkWidth() );
            EXPECT_EQ( PAYLOAD_SIZE, mpcithis.payloadSize() );
            EXPECT_EQ( REQUEST_SIZE, mpcithis.requestSize() );
            EXPECT_EQ( CLASS_CODE, mpcithis.classCode() );
        }

        {   // Copy assignment tests
            MicPciConfigInfo mpcithat( data );
            MicPciConfigInfo mpcithis;
            mpcithis = mpcithat;
            EXPECT_TRUE( mpcithis.isValid() );
            EXPECT_TRUE( mpcithis.hasFullAccess() );
            EXPECT_TRUE( mpcithis.isNoSnoopEnabled() );
            EXPECT_TRUE( mpcithis.isExtendedTagEnabled() );
            EXPECT_TRUE( mpcithis.isRelaxedOrderEnabled() );
            EXPECT_EQ( ASPM_STATE, mpcithis.aspmControlState() );
            EXPECT_EQ( PCI_ADDRESS, mpcithis.address().asString() );
            EXPECT_EQ( VENDOR_ID, mpcithis.vendorId() );
            EXPECT_EQ( DEVICE_ID, mpcithis.deviceId() );
            EXPECT_EQ( REVISION_ID, mpcithis.revisionId() );
            EXPECT_EQ( SUBSYSTEM_ID, mpcithis.subsystemId() );
            EXPECT_EQ( LINK_SPEED_NAME, mpcithis.linkSpeed().name() );
            EXPECT_EQ( LINK_WIDTH, mpcithis.linkWidth() );
            EXPECT_EQ( PAYLOAD_SIZE, mpcithis.payloadSize() );
            EXPECT_EQ( REQUEST_SIZE, mpcithis.requestSize() );
            EXPECT_EQ( CLASS_CODE, mpcithis.classCode() );

            mpcithis = mpcithis; //code coverage..
            EXPECT_TRUE( mpcithis.isValid() );
            EXPECT_TRUE( mpcithis.hasFullAccess() );
            EXPECT_TRUE( mpcithis.isNoSnoopEnabled() );
            EXPECT_TRUE( mpcithis.isExtendedTagEnabled() );
            EXPECT_TRUE( mpcithis.isRelaxedOrderEnabled() );
            EXPECT_EQ( ASPM_STATE, mpcithis.aspmControlState() );
            EXPECT_EQ( PCI_ADDRESS, mpcithis.address().asString() );
            EXPECT_EQ( VENDOR_ID, mpcithis.vendorId() );
            EXPECT_EQ( DEVICE_ID, mpcithis.deviceId() );
            EXPECT_EQ( REVISION_ID, mpcithis.revisionId() );
            EXPECT_EQ( SUBSYSTEM_ID, mpcithis.subsystemId() );
            EXPECT_EQ( LINK_SPEED_NAME, mpcithis.linkSpeed().name() );
            EXPECT_EQ( LINK_WIDTH, mpcithis.linkWidth() );
            EXPECT_EQ( PAYLOAD_SIZE, mpcithis.payloadSize() );
            EXPECT_EQ( REQUEST_SIZE, mpcithis.requestSize() );
            EXPECT_EQ( CLASS_CODE, mpcithis.classCode() );
        }

        {   // Clear tests
            MicPciConfigInfo mpci( data );
            mpci.clear();
            EXPECT_FALSE( mpci.isValid() );
        }

        {   // Aspm control state tests
            PciConfigData dataAspmDisabled;
            dataAspmDisabled.mAspmControl = 0x0;
            dataAspmDisabled.mValid = true;
            MicPciConfigInfo mpciDisabled( dataAspmDisabled );
            EXPECT_EQ( MicPciConfigInfo::eL0sL1Disabled, mpciDisabled.aspmControlState() );

            PciConfigData dataAspmL0sEnabled;
            dataAspmL0sEnabled.mAspmControl = 0x1;
            dataAspmL0sEnabled.mValid = true;
            MicPciConfigInfo mpciL0sEnabled( dataAspmL0sEnabled );
            EXPECT_EQ( MicPciConfigInfo::eL0sEnabled, mpciL0sEnabled.aspmControlState() );

            PciConfigData dataAspmL1Enabled;
            dataAspmL1Enabled.mAspmControl = 0x2;
            dataAspmL1Enabled.mValid = true;
            MicPciConfigInfo mpciL1Enabled( dataAspmL1Enabled );
            EXPECT_EQ( MicPciConfigInfo::eL1Enabled, mpciL1Enabled.aspmControlState() );

            PciConfigData dataAspmL0sL1Enabled;
            dataAspmL0sL1Enabled.mAspmControl = 0x3;
            dataAspmL0sL1Enabled.mValid = true;
            MicPciConfigInfo mpciL0sL1Enabled( dataAspmL0sL1Enabled );
            EXPECT_EQ( MicPciConfigInfo::eL0sL1Enabled, mpciL0sL1Enabled.aspmControlState() );
        }

        {   // Aspm state as string tests
            MicPciConfigInfo mpci;
            EXPECT_EQ( "unknown", mpci.aspmStateAsString( MicPciConfigInfo::eUnknown ) );
            EXPECT_EQ( "Disabled", mpci.aspmStateAsString( MicPciConfigInfo::eL0sL1Disabled) );
            EXPECT_EQ( "L0s", mpci.aspmStateAsString( MicPciConfigInfo::eL0sEnabled ) );
            EXPECT_EQ( "L1", mpci.aspmStateAsString( MicPciConfigInfo::eL1Enabled ) );
            EXPECT_EQ( "L0s & L1", mpci.aspmStateAsString( MicPciConfigInfo::eL0sL1Enabled ) );
        }

    } // sdk.TC_KNL_mpsstools_MicPciConfigInfo_001

}   // namespace micmgmt
