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

namespace micmgmt
{
    using namespace std;

    TEST(sdk, TC_KNL_mpsstools_FlashStatus_001)
    {
        const int PROGRESS = 39;
        const FlashStatus::State STATE = FlashStatus::eFailed;
        const int STAGE = 1;
        const uint32_t ERROR_CODE = 10;
        const std::string STAGE_TEXT( "stage" );
        const std::string ERROR_TEXT( "error" );

        {   // Empty tests
            FlashStatus fs;
            EXPECT_FALSE( fs.isBusy() );
            EXPECT_FALSE( fs.isError() );
            EXPECT_FALSE( fs.isCompleted() );
            EXPECT_EQ( 0, fs.progress() );
            EXPECT_EQ( FlashStatus::eIdle, fs.state() );
            EXPECT_EQ( 0, fs.stage() );
            EXPECT_EQ( "", fs.stageText() );
            EXPECT_EQ( "", fs.errorText() );
            EXPECT_EQ( 0U, fs.errorCode() );
        }

        {   // Value constructor tests
            FlashStatus fs( STATE, PROGRESS, STAGE, ERROR_CODE );
            EXPECT_FALSE( fs.isBusy() );
            EXPECT_TRUE( fs.isError() );
            EXPECT_FALSE( fs.isCompleted() );
            EXPECT_EQ( PROGRESS, fs.progress() );
            EXPECT_EQ( STATE, fs.state() );
            EXPECT_EQ( STAGE, fs.stage() );
            EXPECT_EQ( "", fs.stageText() );
            EXPECT_EQ( "", fs.errorText() );
            EXPECT_EQ( ERROR_CODE, fs.errorCode() );

            FlashStatus fsd1( STATE, PROGRESS, STAGE );
            EXPECT_FALSE( fsd1.isBusy() );
            EXPECT_TRUE( fsd1.isError() );
            EXPECT_FALSE( fsd1.isCompleted() );
            EXPECT_EQ( PROGRESS, fsd1.progress() );
            EXPECT_EQ( STATE, fsd1.state() );
            EXPECT_EQ( STAGE, fsd1.stage() );
            EXPECT_EQ( "", fsd1.stageText() );
            EXPECT_EQ( "", fsd1.errorText() );
            EXPECT_EQ( 0U, fsd1.errorCode() );

            FlashStatus fsd2( STATE, PROGRESS );
            EXPECT_FALSE( fsd2.isBusy() );
            EXPECT_TRUE( fsd2.isError() );
            EXPECT_FALSE( fsd2.isCompleted() );
            EXPECT_EQ( PROGRESS, fsd2.progress() );
            EXPECT_EQ( STATE, fsd2.state() );
            EXPECT_EQ( 0, fsd2.stage() );
            EXPECT_EQ( "", fsd2.stageText() );
            EXPECT_EQ( "", fsd2.errorText() );
            EXPECT_EQ( 0U, fsd2.errorCode() );

            FlashStatus fsd3( STATE );
            EXPECT_FALSE( fsd3.isBusy() );
            EXPECT_TRUE( fsd3.isError() );
            EXPECT_FALSE( fsd3.isCompleted() );
            EXPECT_EQ( 0, fsd3.progress() );
            EXPECT_EQ( STATE, fsd3.state() );
            EXPECT_EQ( 0, fsd2.stage() );
            EXPECT_EQ( "", fsd2.stageText() );
            EXPECT_EQ( "", fsd2.errorText() );
            EXPECT_EQ( 0U, fsd2.errorCode() );
        }

        {   // Copy constructor tests
            FlashStatus fsthat( STATE, PROGRESS, STAGE, ERROR_CODE );
            FlashStatus fsthis( fsthat );
            EXPECT_FALSE( fsthis.isBusy() );
            EXPECT_TRUE( fsthis.isError() );
            EXPECT_FALSE( fsthis.isCompleted() );
            EXPECT_EQ( PROGRESS, fsthis.progress() );
            EXPECT_EQ( STATE, fsthis.state() );
            EXPECT_EQ( STAGE, fsthis.stage() );
            EXPECT_EQ( "", fsthis.stageText() );
            EXPECT_EQ( "", fsthis.errorText() );
            EXPECT_EQ( ERROR_CODE, fsthis.errorCode() );
        }

        {   // Copy assignment tests
            FlashStatus fsthat( STATE, PROGRESS, STAGE, ERROR_CODE );
            FlashStatus fsthis;
            fsthis = fsthat;
            EXPECT_FALSE( fsthis.isBusy() );
            EXPECT_TRUE( fsthis.isError() );
            EXPECT_FALSE( fsthis.isCompleted() );
            EXPECT_EQ( PROGRESS, fsthis.progress() );
            EXPECT_EQ( STATE, fsthis.state() );
            EXPECT_EQ( STAGE, fsthis.stage() );
            EXPECT_EQ( "", fsthis.stageText() );
            EXPECT_EQ( "", fsthis.errorText() );
            EXPECT_EQ( ERROR_CODE, fsthis.errorCode() );

            fsthis = fsthis;
            EXPECT_FALSE( fsthis.isBusy() );
            EXPECT_TRUE( fsthis.isError() );
            EXPECT_FALSE( fsthis.isCompleted() );
            EXPECT_EQ( PROGRESS, fsthis.progress() );
            EXPECT_EQ( STATE, fsthis.state() );
            EXPECT_EQ( STAGE, fsthis.stage() );
            EXPECT_EQ( "", fsthis.stageText() );
            EXPECT_EQ( "", fsthis.errorText() );
            EXPECT_EQ( ERROR_CODE, fsthis.errorCode() );
        }

        {   // New Stage tests
            FlashStatus fs( STATE, PROGRESS, STAGE, ERROR_CODE );
            FlashStatus fssame( STATE, PROGRESS, STAGE, ERROR_CODE );
            FlashStatus fsdiff( STATE, PROGRESS, (STAGE+1), ERROR_CODE );
            EXPECT_FALSE( fs.isNewStage( fssame ) );
            EXPECT_TRUE( fs.isNewStage( fsdiff ) );
        }

        {   // New State tests
            FlashStatus fs( STATE, PROGRESS, STAGE, ERROR_CODE );
            FlashStatus fssame( STATE, PROGRESS, STAGE, ERROR_CODE );
            FlashStatus fsdiff( FlashStatus::eDone, PROGRESS, STAGE, ERROR_CODE );
            EXPECT_FALSE( fs.isNewState( fssame ) );
            EXPECT_TRUE( fs.isNewState( fsdiff ) );
        }

        {   // New Progress tests
            FlashStatus fs( STATE, PROGRESS, STAGE, ERROR_CODE );
            FlashStatus fssame( STATE, PROGRESS, STAGE, ERROR_CODE );
            FlashStatus fsdiff( STATE, (PROGRESS-10), STAGE, ERROR_CODE );
            EXPECT_FALSE( fs.isNewProgress( fssame ) );
            EXPECT_TRUE( fs.isNewProgress( fsdiff ) );
        }

        {   // Set stage tests
            FlashStatus fs( STATE, PROGRESS, STAGE, ERROR_CODE );
            fs.setStageText( STAGE_TEXT );
            EXPECT_EQ( STAGE_TEXT, fs.stageText() );
        }

        {   // Set error tests
            FlashStatus fs( STATE, PROGRESS, STAGE, ERROR_CODE );
            fs.setErrorText( ERROR_TEXT );
            EXPECT_EQ( ERROR_TEXT, fs.errorText() );
        }

        {   // State coverage tests
            FlashStatus fsbusy( FlashStatus::eBusy, 81, 1 );
            EXPECT_TRUE( fsbusy.isBusy() );
            EXPECT_FALSE( fsbusy.isError() );
            EXPECT_FALSE( fsbusy.isCompleted() );

            FlashStatus fscompleted( FlashStatus::eDone, 100 );
            EXPECT_FALSE( fscompleted.isBusy() );
            EXPECT_FALSE( fscompleted.isError() );
            EXPECT_TRUE( fscompleted.isCompleted() );

            FlashStatus fscompstage( FlashStatus::eDone, 100, 2 );
            EXPECT_FALSE( fscompstage.isBusy() );
            EXPECT_FALSE( fscompstage.isError() );
            EXPECT_FALSE( fscompstage.isCompleted() );
        }

    } // sdk.TC_KNL_mpsstools_FlashStatus_001

}   // namespace micmgmt
