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
#include "MicCoreUsageInfo.hpp"
#include "CoreUsageData_p.hpp"  // Private

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_MicCoreUsageInfo_001)
    {
        const size_t COUNTER_TYPES = 5; //eUser, eSystem, eNice, eIdle, eTotal

        const size_t CORE_COUNT = 62;
        const size_t CORE_THREAD_COUNT = 2;
        const size_t FREQUENCY = 1000;
        const uint64_t TICK_COUNT = 4;
        const uint64_t TICKS_PER_SECOND = 100;
        const uint64_t COUNTER_TOTAL[5] = { 1, 2, 3, 4, 5 };

        MicCoreUsageInfo::Counters COUNTERS[ COUNTER_TYPES ];
        for( size_t type = 0 ; type < COUNTER_TYPES ; type++ )
            for( size_t thread = 0 ; thread < CORE_COUNT * CORE_THREAD_COUNT ; thread++ )
                COUNTERS[type].push_back( type + (thread * COUNTER_TYPES) );

        {   // Empty tests
            MicCoreUsageInfo mcui;
            EXPECT_FALSE( mcui.isValid() );
            EXPECT_EQ( size_t(0), mcui.coreCount() );
            EXPECT_EQ( size_t(0), mcui.coreThreadCount() );
            EXPECT_FALSE( mcui.frequency().isValid() );
            EXPECT_EQ( uint64_t(0), mcui.tickCount() );
            EXPECT_EQ( uint64_t(0), mcui.ticksPerSecond() );

            for( size_t type = 0 ; type < COUNTER_TYPES ; type++ ){
                EXPECT_EQ( uint64_t(0), mcui.counterTotal( static_cast<MicCoreUsageInfo::Counter>(type) ) );
                EXPECT_EQ( uint64_t(0), mcui.counterValue( static_cast<MicCoreUsageInfo::Counter>(type), 0 ) );
                EXPECT_TRUE( mcui.usageCounters( static_cast<MicCoreUsageInfo::Counter>(type) ).empty() );
            }
        }

        {   // Counters constructor tests
            MicCoreUsageInfo mcui( &COUNTERS[0], &COUNTERS[1], &COUNTERS[2], &COUNTERS[3], &COUNTERS[4] );
            EXPECT_FALSE( mcui.isValid() );
            EXPECT_EQ( size_t(0), mcui.coreCount() );
            EXPECT_EQ( size_t(0), mcui.coreThreadCount() );
            EXPECT_FALSE( mcui.frequency().isValid() );
            EXPECT_EQ( uint64_t(0), mcui.tickCount() );
            EXPECT_EQ( uint64_t(0), mcui.ticksPerSecond() );

            mcui.setValid( true );
            EXPECT_TRUE( mcui.isValid() );

            for( size_t type = 0 ; type < COUNTER_TYPES ; type++ ){
                EXPECT_EQ( uint64_t(0), mcui.counterTotal( static_cast<MicCoreUsageInfo::Counter>(type) ) );
                for( size_t thread = 0 ; thread < CORE_COUNT * CORE_THREAD_COUNT ; thread++ ){
                    EXPECT_EQ( COUNTERS[type].at(thread), mcui.counterValue( static_cast<MicCoreUsageInfo::Counter>(type), thread ) );
                }
                EXPECT_EQ( COUNTERS[type], mcui.usageCounters( static_cast<MicCoreUsageInfo::Counter>(type) ) );
            }
        }

        {   // Set * tests
            MicCoreUsageInfo mcui;

            mcui.setValid( true );
            EXPECT_TRUE( mcui.isValid() );

            mcui.setCoreCount( CORE_COUNT );
            EXPECT_EQ( CORE_COUNT, mcui.coreCount() );

            mcui.setCoreThreadCount( CORE_THREAD_COUNT );
            EXPECT_EQ( CORE_THREAD_COUNT, mcui.coreThreadCount() );
            for( size_t type = 0 ; type < COUNTER_TYPES ; type++ )
                EXPECT_EQ( CORE_COUNT * CORE_THREAD_COUNT, mcui.usageCounters( static_cast<MicCoreUsageInfo::Counter>(type) ).size() );

            mcui.setFrequency( FREQUENCY );
            EXPECT_EQ( FREQUENCY, mcui.frequency().value() );

            mcui.setTickCount( TICK_COUNT );
            EXPECT_EQ( TICK_COUNT, mcui.tickCount() );

            mcui.setTicksPerSecond( TICKS_PER_SECOND );
            EXPECT_EQ( TICKS_PER_SECOND, mcui.ticksPerSecond() );

            for( size_t type = 0 ; type < COUNTER_TYPES ; type++ ){
                mcui.setCounterTotal( static_cast<MicCoreUsageInfo::Counter>(type), COUNTER_TOTAL[type] );
                for( size_t thread = 0 ; thread < CORE_COUNT * CORE_THREAD_COUNT ; thread++ ){
                    mcui.setUsageCount( static_cast<MicCoreUsageInfo::Counter>(type), thread, COUNTERS[type].at(thread) );
                }
                EXPECT_EQ( COUNTERS[type], mcui.usageCounters( static_cast<MicCoreUsageInfo::Counter>(type) ) );
            }
        }

        {   // Negative/coverage tests
            MicCoreUsageInfo mcui( &COUNTERS[0] );
            mcui.setValid( true );
            EXPECT_TRUE( mcui.isValid() );

            EXPECT_EQ( uint64_t(0), mcui.counterValue( MicCoreUsageInfo::eUser, CORE_COUNT * CORE_THREAD_COUNT ) );
            EXPECT_EQ( uint64_t(0), mcui.counterValue( MicCoreUsageInfo::eSystem, 0 ) );

            const uint64_t newUsageCount = 999;
            mcui.setUsageCount( MicCoreUsageInfo::eUser, CORE_COUNT * CORE_THREAD_COUNT, newUsageCount );
            EXPECT_NE( newUsageCount, mcui.counterValue( MicCoreUsageInfo::eUser, CORE_COUNT * CORE_THREAD_COUNT ) );

            mcui.setCoreCount( 0 );
            EXPECT_EQ( size_t(0), mcui.coreCount() );
            EXPECT_FALSE( mcui.usageCounters( MicCoreUsageInfo::eUser ).empty() );
        }

        {   // Clear tests
            MicCoreUsageInfo mcui( &COUNTERS[0], &COUNTERS[1], &COUNTERS[2], &COUNTERS[3], &COUNTERS[4] );
            mcui.setValid( true );
            mcui.setCoreCount( CORE_COUNT );
            mcui.setCoreThreadCount( CORE_THREAD_COUNT );
            mcui.setFrequency( FREQUENCY );
            mcui.setTickCount( TICK_COUNT );
            mcui.setTicksPerSecond( TICKS_PER_SECOND );

            mcui.clear();
            EXPECT_FALSE( mcui.isValid() );
            EXPECT_EQ( size_t(0), mcui.coreCount() );
            EXPECT_EQ( size_t(0), mcui.coreThreadCount() );
            EXPECT_FALSE( mcui.frequency().isValid() );
            EXPECT_EQ( uint64_t(0), mcui.tickCount() );
            EXPECT_EQ( uint64_t(0), mcui.ticksPerSecond() );

            for( size_t type = 0 ; type < COUNTER_TYPES ; type++ ){
                EXPECT_EQ( uint64_t(0), mcui.counterTotal( static_cast<MicCoreUsageInfo::Counter>(type) ) );
                EXPECT_EQ( uint64_t(0), mcui.counterValue( static_cast<MicCoreUsageInfo::Counter>(type), 0 ) );
                EXPECT_TRUE( mcui.usageCounters( static_cast<MicCoreUsageInfo::Counter>(type) ).empty() );
            }
        }

    } // sdk.TC_KNL_mpsstools_MicCoreUsageInfo_001

}   // namespace micmgmt
