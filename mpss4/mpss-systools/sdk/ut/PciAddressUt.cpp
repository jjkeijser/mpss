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
#include "PciAddress.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_PciAddress_001)
    {
#ifdef __linux__
        const std::string  invalids( "????:??:??.?" );
        const std::string  pciadrs( "0001:02:03.4" );
#else
        const std::string  invalids( "????:?:?:?" );
        const std::string  pciadrs( "0001:2:3:4" );
#endif
        const uint32_t     pciadrv = 0x0001021c;

        {   // Empty tests
            PciAddress  pci;
            EXPECT_FALSE( pci.isValid() );
            EXPECT_EQ( -1, pci.function() );
            EXPECT_EQ( -1, pci.device() );
            EXPECT_EQ( -1, pci.bus() );
            EXPECT_EQ( -1, pci.domain() );
            EXPECT_EQ( invalids, pci.asString() );
        }

        {   // Hexadecimal constructor tests
            PciAddress  pcifunct( 0x00000001 );
            EXPECT_TRUE( pcifunct.isValid() );
            EXPECT_EQ( 1, pcifunct.function() );
            EXPECT_EQ( 0, pcifunct.device() );
            EXPECT_EQ( 0, pcifunct.bus() );
            EXPECT_EQ( 0, pcifunct.domain() );

            PciAddress  pcidevice( 0x00000008 );
            EXPECT_TRUE( pcidevice.isValid() );
            EXPECT_EQ( 0, pcidevice.function() );
            EXPECT_EQ( 1, pcidevice.device() );
            EXPECT_EQ( 0, pcidevice.bus() );
            EXPECT_EQ( 0, pcidevice.domain() );

            PciAddress  pcibus( 0x00000100 );
            EXPECT_TRUE( pcibus.isValid() );
            EXPECT_EQ( 0, pcibus.function() );
            EXPECT_EQ( 0, pcibus.device() );
            EXPECT_EQ( 1, pcibus.bus() );
            EXPECT_EQ( 0, pcibus.domain() );

            PciAddress  pcidomain( 0x00010000 );
            EXPECT_TRUE( pcidomain.isValid() );
            EXPECT_EQ( 0, pcidomain.function() );
            EXPECT_EQ( 0, pcidomain.device() );
            EXPECT_EQ( 0, pcidomain.bus() );
            EXPECT_EQ( 1, pcidomain.domain() );
        }

        {   // Field-separated integer constructor tests
            PciAddress  pcifunct( 1, 0, 0, 0 );
            EXPECT_TRUE( pcifunct.isValid() );
            EXPECT_EQ( 1, pcifunct.function() );
            EXPECT_EQ( 0, pcifunct.device() );
            EXPECT_EQ( 0, pcifunct.bus() );
            EXPECT_EQ( 0, pcifunct.domain() );

            PciAddress  pcidevice( 0, 1, 0, 0 );
            EXPECT_TRUE( pcidevice.isValid() );
            EXPECT_EQ( 0, pcidevice.function() );
            EXPECT_EQ( 1, pcidevice.device() );
            EXPECT_EQ( 0, pcidevice.bus() );
            EXPECT_EQ( 0, pcidevice.domain() );

            PciAddress  pcibus( 0, 0, 1, 0 );
            EXPECT_TRUE( pcibus.isValid() );
            EXPECT_EQ( 0, pcibus.function() );
            EXPECT_EQ( 0, pcibus.device() );
            EXPECT_EQ( 1, pcibus.bus() );
            EXPECT_EQ( 0, pcibus.domain() );

            PciAddress  pcidomain( 0, 0, 0, 1 );
            EXPECT_TRUE( pcidomain.isValid() );
            EXPECT_EQ( 0, pcidomain.function() );
            EXPECT_EQ( 0, pcidomain.device() );
            EXPECT_EQ( 0, pcidomain.bus() );
            EXPECT_EQ( 1, pcidomain.domain() );

            PciAddress pcidefaultone( 1, 1, 1 );
            EXPECT_TRUE( pcidevice.isValid() );
            EXPECT_EQ( 1, pcidefaultone.function() );
            EXPECT_EQ( 1, pcidefaultone.device() );
            EXPECT_EQ( 1, pcidefaultone.bus() );
            EXPECT_EQ( 0, pcidefaultone.domain() );

            PciAddress pcidefaulttwo( 1, 1 );
            EXPECT_TRUE( pcidevice.isValid() );
            EXPECT_EQ( 1, pcidefaulttwo.function() );
            EXPECT_EQ( 1, pcidefaulttwo.device() );
            EXPECT_EQ( 0, pcidefaulttwo.bus() );
            EXPECT_EQ( 0, pcidefaulttwo.domain() );
        }

        {   // String constructor tests
            PciAddress  pci( pciadrs );
            EXPECT_TRUE( pci.isValid() );
            EXPECT_EQ( 4, pci.function() );
            EXPECT_EQ( 3, pci.device() );
            EXPECT_EQ( 2, pci.bus() );
            EXPECT_EQ( 1, pci.domain() );
            EXPECT_EQ( pciadrs, pci.asString() );
        }

        {   // Copy constructor tests
            PciAddress  pcithat( pciadrs );
            PciAddress  pcithis( pcithat );
            EXPECT_TRUE( pcithis.isValid() );
            EXPECT_EQ( 4, pcithis.function() );
            EXPECT_EQ( 3, pcithis.device() );
            EXPECT_EQ( 2, pcithis.bus() );
            EXPECT_EQ( 1, pcithis.domain() );
            EXPECT_EQ( pciadrs, pcithis.asString() );
        }

        {   // Integer assignment tests
            PciAddress  pci;
            pci = pciadrv;
            EXPECT_TRUE( pci.isValid() );
            EXPECT_EQ( 4, pci.function() );
            EXPECT_EQ( 3, pci.device() );
            EXPECT_EQ( 2, pci.bus() );
            EXPECT_EQ( 1, pci.domain() );
            EXPECT_EQ( pciadrs, pci.asString() );
        }

        {   // String assignment tests
            PciAddress  pci;
            pci = pciadrs;
            EXPECT_TRUE( pci.isValid() );
            EXPECT_EQ( 4, pci.function() );
            EXPECT_EQ( 3, pci.device() );
            EXPECT_EQ( 2, pci.bus() );
            EXPECT_EQ( 1, pci.domain() );
            EXPECT_EQ( pciadrs, pci.asString() );

            PciAddress  pcifail;
            pcifail = "fail";
            EXPECT_FALSE( pcifail.isValid() );
        }

        {   // Copy assignment tests
            PciAddress  pcithat( pciadrs );
            PciAddress  pcithis;
            pcithis = pcithat;
            EXPECT_TRUE( pcithis.isValid() );
            EXPECT_EQ( 4, pcithis.function() );
            EXPECT_EQ( 3, pcithis.device() );
            EXPECT_EQ( 2, pcithis.bus() );
            EXPECT_EQ( 1, pcithis.domain() );
            EXPECT_EQ( pciadrs, pcithis.asString() );

            pcithis = pcithis; //code coverage..
            EXPECT_TRUE( pcithis.isValid() );
            EXPECT_EQ( 4, pcithis.function() );
            EXPECT_EQ( 3, pcithis.device() );
            EXPECT_EQ( 2, pcithis.bus() );
            EXPECT_EQ( 1, pcithis.domain() );
            EXPECT_EQ( pciadrs, pcithis.asString() );
        }

        {   // Clear tests
            PciAddress pci( pciadrs );
            pci.clear();
            EXPECT_FALSE( pci.isValid() );
        }

    } // sdk.TC_KNL_mpsstools_PciAddress_001

}   // namespace micmgmt
