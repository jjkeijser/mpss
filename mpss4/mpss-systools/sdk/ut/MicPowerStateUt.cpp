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
#include "MicPowerState.hpp"

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicPowerState_001)
    {
        const std::string  CPUSTATE_NAME = "cpufreq";
        const std::string  CO6STATE_NAME = "corec6";
        const std::string  PC3STATE_NAME = "pc3";
        const std::string  PC6STATE_NAME = "pc6";

        const MicPowerState::State CPUSTATE = MicPowerState::eCpuFreq;
        const MicPowerState::State CO6STATE = MicPowerState::eCoreC6;
        const MicPowerState::State PC3STATE = MicPowerState::ePc3;
        const MicPowerState::State PC6STATE = MicPowerState::ePc6;

        {   // Value constructor tests
            MicPowerState mps( CPUSTATE, true );
            EXPECT_TRUE( mps.isAvailable() );
            EXPECT_EQ( CPUSTATE, mps.state() );
            EXPECT_TRUE( mps.isEnabled() );
            EXPECT_EQ( CPUSTATE_NAME, mps.name() );

            MicPowerState mpsdefaultone( CPUSTATE );
            EXPECT_TRUE( mpsdefaultone.isAvailable() );
            EXPECT_EQ( CPUSTATE, mpsdefaultone.state() );
            EXPECT_FALSE( mpsdefaultone.isEnabled() );
        }

        {   // Copy constructor tests
            MicPowerState mpsthat( CPUSTATE, true );
            MicPowerState mpsthis( mpsthat );
            EXPECT_TRUE( mpsthis.isAvailable() );
            EXPECT_EQ( CPUSTATE, mpsthis.state() );
            EXPECT_TRUE( mpsthis.isEnabled() );
        }

        {   // Copy assignment tests
            MicPowerState mpsthat( CPUSTATE, true );
            MicPowerState mpsthis( CO6STATE );
            EXPECT_EQ( CO6STATE_NAME, mpsthis.name() );
            mpsthis = mpsthat;
            EXPECT_TRUE( mpsthis.isAvailable() );
            EXPECT_EQ( CPUSTATE, mpsthis.state() );
            EXPECT_TRUE( mpsthis.isEnabled() );

            mpsthis = mpsthis; //code coverage..
            EXPECT_TRUE( mpsthis.isAvailable() );
            EXPECT_EQ( CPUSTATE, mpsthis.state() );
            EXPECT_TRUE( mpsthis.isEnabled() );
        }

        {   // setAvailable tests
            MicPowerState mps( CPUSTATE );
            EXPECT_TRUE( mps.isAvailable() );
            mps.setAvailable( false );
            EXPECT_FALSE( mps.isAvailable() );
            mps.setAvailable( true );
            EXPECT_TRUE( mps.isAvailable() );
        }

        {   // stateAsString tests
            EXPECT_EQ( CPUSTATE_NAME, MicPowerState::stateAsString( CPUSTATE ) );
            EXPECT_EQ( CO6STATE_NAME, MicPowerState::stateAsString( CO6STATE ) );
            EXPECT_EQ( PC3STATE_NAME, MicPowerState::stateAsString( PC3STATE ) );
            EXPECT_EQ( PC6STATE_NAME, MicPowerState::stateAsString( PC6STATE ) );
        }

    } // sdk.TC_KNL_mpsstools_MicPowerState_001

}   // namespace micmgmt
