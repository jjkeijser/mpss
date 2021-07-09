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

// PROJECT INCLUDES
//
#include    "FlashStatus.hpp"
#include    "KnlDeviceBase.hpp"

// PRIVATE DATA
//
namespace  micmgmt {
struct  FlashStatus::PrivData
{
    FlashStatus::State  mState;
    int                 mProgress;
    int                 mStage;
    std::string         mStageText;
    std::string         mErrorText;
    uint32_t            mErrorCode;

    PrivData() : mState(FlashStatus::eIdle), mProgress(0), mStage(0), mErrorCode(0) {}
};
}

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::FlashStatus   FlashStatus.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates flash status information
 *
 *  The \b %FlashStatus class encapsulates flash status information and
 *  provides accessors to the relevant information.
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::FlashStatus::State
 *
 *  Enumerations of flash update operation states.
 */

/** @var    FlashStatus::State  FlashStatus::eIdle
 *
 *  Idle state. No flash update operation was initiated.
 */

/** @var    FlashStatus::State  FlashStatus::eBusy
 *
 *  Busy state. A flash update operation was initiated and still in progress.
 */

/** @var    FlashStatus::State  FlashStatus::eDone
 *
 *  Done state. A flash update operation has successfully completed.
 */

/** @var    FlashStatus::State  FlashStatus::eFailed
 *
 *  Failed state. A flash update operation has failed.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     FlashStatus::FlashStatus( State state, int progress, int stage, uint32_t error )
 *  @param  state     State (optional)
 *  @param  progress  Progress in percent [0..100] (optional)
 *  @param  stage     Stage (optional)
 *  @param  error     Error code (optional)
 *
 *  Construct a flash status object based on given \a state, \a progress,
 *  \a stage, and \a error code.
 */

FlashStatus::FlashStatus( State state, int progress, int stage, uint32_t error ) :
    m_pData( new PrivData )
{
    set( state, progress, stage, error );
}


//----------------------------------------------------------------------------
/** @fn     FlashStatus::FlashStatus( const FlashStatus& that )
 *  @param  that    Other flash device info object
 *
 *  Construct a flash device info object as deep copy of \a that info object.
 */

FlashStatus::FlashStatus( const FlashStatus& that ) :
    m_pData( new PrivData )
{
    m_pData->mState     = that.m_pData->mState;
    m_pData->mProgress  = that.m_pData->mProgress;
    m_pData->mStage     = that.m_pData->mStage;
    m_pData->mStageText = that.m_pData->mStageText;
    m_pData->mErrorText = that.m_pData->mErrorText;
    m_pData->mErrorCode = that.m_pData->mErrorCode;
}


//----------------------------------------------------------------------------
/** @fn     FlashStatus::~FlashStatus()
 *
 *  Cleanup.
 */

FlashStatus::~FlashStatus()
{
    // Nothing to do - unique_ptr
}


//----------------------------------------------------------------------------
/** @fn     FlashStatus&  FlashStatus::operator = ( const FlashStatus& that )
 *  @param  that    Other info object
 *  @return reference to this object
 *
 *  Assign \a that object to this and return a reference to this object.
 */

FlashStatus&  FlashStatus::operator = ( const FlashStatus& that )
{
    if (&that != this)
    {
        m_pData->mState     = that.m_pData->mState;
        m_pData->mProgress  = that.m_pData->mProgress;
        m_pData->mStage     = that.m_pData->mStage;
        m_pData->mStageText = that.m_pData->mStageText;
        m_pData->mErrorText = that.m_pData->mErrorText;
        m_pData->mErrorCode = that.m_pData->mErrorCode;
    }

    return  *this;
}

bool  FlashStatus::operator == ( const FlashStatus& that )
{
    return !(isNewStage(that) || isNewState(that ) || isNewProgress(that));
}

/*
    We have only three valid transitions. One for each firmware capsule.

    eIdleStage -> eBiosStage -> eNextStage
    eIdleStage -> eSmcStage  -> eNextStage
    eIdleStage -> eMeStage   -> eNextStage

    Any other path will cause a failed state.
    */

bool  FlashStatus::isValidTransition( const FlashStatus& that ) const
{
    bool sIdle = m_pData->mStage == KnlDeviceBase::FlashStage::eIdleStage;
    bool sBios = m_pData->mStage == KnlDeviceBase::FlashStage::eBiosStage;
    bool sSMC  = m_pData->mStage == KnlDeviceBase::FlashStage::eSmcStage;
    bool sME   = m_pData->mStage == KnlDeviceBase::FlashStage::eMeStage;
    bool sNext = m_pData->mStage == KnlDeviceBase::FlashStage::eNextStage;
    bool lIdle = that.m_pData->mStage == KnlDeviceBase::FlashStage::eIdleStage;
    bool lBios = that.m_pData->mStage == KnlDeviceBase::FlashStage::eBiosStage;
    bool lSMC  = that.m_pData->mStage == KnlDeviceBase::FlashStage::eSmcStage;
    bool lME   = that.m_pData->mStage == KnlDeviceBase::FlashStage::eMeStage;
    bool lNext = that.m_pData->mStage == KnlDeviceBase::FlashStage::eNextStage;
    bool Complete = that.m_pData->mState == eStageComplete;

    if (that.m_pData->mState == eFailed)
        return true;
    if (lIdle & (sBios | sSMC | sME))
        return true;
    if ((lBios | lSMC | lME) & Complete & sNext)
        return true;
    if (lNext & sIdle)
        return true;
    if (m_pData->mStage == that.m_pData->mStage)
        return true;

    return false;
}

//----------------------------------------------------------------------------
/** @fn     bool  FlashStatus::isBusy() const
 *  @return busy state
 *
 *  Returns \c true if a flash stage is in progress. The actual progress
 *  information is available using the progress() function.
 */

bool  FlashStatus::isBusy() const
{
    return  (state() == eBusy);
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashStatus::isError() const
 *  @return error state
 *
 *  Returns \c true if a flash stage failed.
 */

bool  FlashStatus::isError() const
{
    return  ((state() == eFailed) || (state() == eAuthFailedVM));
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashStatus::isCompleted() const
 *  @return completed state
 *
 *  Returns \c true if all flash stages have successfully completed.
 */

bool  FlashStatus::isCompleted() const
{
    return  ((state() == eDone) && (stage() == 0));
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashStatus::isNewStage( const FlashStatus& that ) const
 *  @param  that    Other flash status object
 *  @return stage changed state
 *
 *  Returns \c true if the current status stage is different from \a that
 *  status stage.
 */

bool  FlashStatus::isNewStage( const FlashStatus& that ) const
{
    return  (stage() != that.stage());
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashStatus::isNewState( const FlashStatus& that ) const
 *  @param  that    Other flash status object
 *  @return state changed state
 *
 *  Returns \c true if the current status state is different from \a that
 *  status state.
 */

bool  FlashStatus::isNewState( const FlashStatus& that ) const
{
    return  (state() != that.state());
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashStatus::isNewProgress( const FlashStatus& that ) const
 *  @param  that    Other flash status object
 *  @return progress changed state
 *
 *  Returns \c true if the current status progress is different from \a that
 *  status progress.
 */

bool  FlashStatus::isNewProgress( const FlashStatus& that ) const
{
    return  (progress() != that.progress());
}


//----------------------------------------------------------------------------
/** @fn     int  FlashStatus::progress() const
 *  @return progress
 *
 *  Returns the progress of the flash stage in percent.
 */

int  FlashStatus::progress() const
{
    return  m_pData->mProgress;
}


//----------------------------------------------------------------------------
/** @fn     FlashStatus::State  FlashStatus::state() const
 *  @return state
 *
 *  Returns the current flash update operation state.
 */

FlashStatus::State  FlashStatus::state() const
{
    return  m_pData->mState;
}


//----------------------------------------------------------------------------
/** @fn     int  FlashStatus::stage() const
 *  @return flash stage
 *
 *  Returns the flash stage.
 *
 *  A flash operation may involve more than one stage. Certain subsystems may
 *  be loaded or updated as part of the flash operation. The stage() function
 *  returns the current stage. Stage 0 is defined as final stage.
 */

int  FlashStatus::stage() const
{
    return  m_pData->mStage;
}


//----------------------------------------------------------------------------
/** @fn     std::string  FlashStatus::stageText() const
 *  @return stage text
 *
 *  Returns the stage text.
 */

std::string  FlashStatus::stageText() const
{
    return  m_pData->mStageText;
}


//----------------------------------------------------------------------------
/** @fn     std::string  FlashStatus::errorText() const
 *  @return error text
 *
 *  Returns the error text.
 */

std::string  FlashStatus::errorText() const
{
    return  m_pData->mErrorText;
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  FlashStatus::errorCode() const
 *  @return error code
 *
 *  Returns the current error code.
 */

uint32_t  FlashStatus::errorCode() const
{
    return  m_pData->mErrorCode;
}


//----------------------------------------------------------------------------
/** @fn     void  FlashStatus::set( State state, int progress, int stage, uint32_t error )
 *  @param  state     State
 *  @param  progress  Progress in percent [0..100] (optional)
 *  @param  stage     Flash stage [...0] (optional)
 *  @param  error     Error code (optional)
 *
 *  Set flash status based on specified parameters.
 */

void  FlashStatus::set( State state, int progress, int stage, uint32_t error )
{
    m_pData->mState     = state;
    m_pData->mProgress  = progress;
    m_pData->mStage     = stage;
    m_pData->mErrorCode = error;
}


//----------------------------------------------------------------------------
/** @fn     void  FlashStatus::setStageText( const string& text )
 *  @param  text    Stage text
 *
 *  Set the stage text.
 */

void  FlashStatus::setStageText( const string& text )
{
    m_pData->mStageText = text;
}


//----------------------------------------------------------------------------
/** @fn     void  FlashStatus::setErrorText( const string& text )
 *  @param  text    Error text
 *
 *  Set the error text.
 */

void  FlashStatus::setErrorText( const string& text )
{
    m_pData->mErrorText = text;
}

