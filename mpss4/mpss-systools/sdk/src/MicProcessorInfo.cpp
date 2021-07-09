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
#include    "MicProcessorInfo.hpp"
#include    "ProcessorInfoData_p.hpp"   // Private

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicProcessorInfo   MicProcessorInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates coprocessor information
 *
 *  The \b %MicProcessorInfo class encapsulates coprocessor information of a
 *  MIC device and provides accessors to the relevant information.
 *
 *  %MicProcessorInfo is available in any device state, as long as the
 *  device driver is loaded.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicProcessorInfo::MicProcessorInfo()
 *
 *  Construct an empty processor info object.
 */

MicProcessorInfo::MicProcessorInfo() :
    m_pData( new ProcessorInfoData )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicProcessorInfo::MicProcessorInfo( const ProcessorInfoData& data )
 *  @param  data    Mic processor info data
 *
 *  Construct a mic processor info object based on specified prcoessor \a data.
 */

MicProcessorInfo::MicProcessorInfo( const ProcessorInfoData& data ) :
    m_pData( new ProcessorInfoData )
{
    m_pData->set( data );
}


//----------------------------------------------------------------------------
/** @fn     MicProcessorInfo::MicProcessorInfo( const MicProcessorInfo& that )
 *  @param  that    Other mic processor info object
 *
 *  Construct a mic processor object as deep copy of \a that object.
 */

MicProcessorInfo::MicProcessorInfo( const MicProcessorInfo& that ) :
    m_pData( new ProcessorInfoData )
{
    m_pData->set( *(that.m_pData) );
}


//----------------------------------------------------------------------------
/** @fn     MicProcessorInfo::~MicProcessorInfo()
 *
 *  Cleanup.
 */

MicProcessorInfo::~MicProcessorInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     MicProcessorInfo&  MicProcessorInfo::operator = ( const MicProcessorInfo& that )
 *  @param  that    Other mic processor object
 *  @return reference to this object
 *
 *  Assign \a that object to this object and return the updated object.
 */

MicProcessorInfo&  MicProcessorInfo::operator = ( const MicProcessorInfo& that )
{
    if (&that != this)
        m_pData->set( *(that.m_pData) );

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  MicProcessorInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the processor info is valid.
 *  \todo Remove this method.
 */

bool  MicProcessorInfo::isValid() const
{
    return m_pData->mType.isValid() && m_pData->mModel.isValid() &&
        m_pData->mFamily.isValid() && m_pData->mExtendedModel.isValid() &&
        m_pData->mExtendedFamily.isValid() && m_pData->mSteppingData.isValid() &&
        m_pData->mSubsteppingData.isValid() && m_pData->mStepping.isValid() &&
        m_pData->mSku.isValid();
}


//----------------------------------------------------------------------------
/** @fn     const MicValueUInt16&  MicProcessorInfo::type() const
 *  @return coprocessor type
 *
 *  Returns the coprocessor type.
 */

const MicValueUInt16&  MicProcessorInfo::type() const
{
    return m_pData->mType;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueUInt16&  MicProcessorInfo::model() const
 *  @return coprocessor model
 *
 *  Returns the coprocessor model.
 */

const MicValueUInt16&  MicProcessorInfo::model() const
{
    return m_pData->mModel;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueUInt16&  MicProcessorInfo::family() const
 *  @return coprocessor family
 *
 *  Returns the coprocessor family.
 */

const MicValueUInt16&  MicProcessorInfo::family() const
{
    return m_pData->mFamily;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueUInt16&  MicProcessorInfo::extendedModel() const
 *  @return coprocessor extended model
 *
 *  Returns the coprocessor extended model.
 */

const MicValueUInt16&  MicProcessorInfo::extendedModel() const
{
    return m_pData->mExtendedModel;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueUInt16&  MicProcessorInfo::extendedFamily() const
 *  @return coprocessor extended family
 *
 *  Returns the coprocessor extended family.
 */

const MicValueUInt16&  MicProcessorInfo::extendedFamily() const
{
    return m_pData->mExtendedFamily;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueUInt32&  MicProcessorInfo::steppingId() const
 *  @return stepping ID
 *
 *  Returns the coprocessor stepping ID.
 */

const MicValueUInt32&  MicProcessorInfo::steppingId() const
{
    return m_pData->mSteppingData;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueUInt32&  MicProcessorInfo::substeppingId() const
 *  @return stepping ID
 *
 *  Returns the coprocessor substepping ID.
 */

const MicValueUInt32&  MicProcessorInfo::substeppingId() const
{
    return m_pData->mSubsteppingData;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicProcessorInfo::stepping() const
 *  @return stepping
 *
 *  Returns coprocessor stepping info. (e.g. "B0")
 */

const MicValueString&  MicProcessorInfo::stepping() const
{
    return m_pData->mStepping;
}


//----------------------------------------------------------------------------
/** @fn     const MicValueString&  MicProcessorInfo::sku() const
 *  @return SKU
 *
 *  Returns the coprocessor device SKU. (e.g. "ES2-P/A/A 1750")
 */

const MicValueString&  MicProcessorInfo::sku() const
{
    return m_pData->mSku;
}


//----------------------------------------------------------------------------
/** @fn     void  MicProcessorInfo::clear()
 *
 *  Clear (invalidate) processor info data.
 */

void  MicProcessorInfo::clear()
{
    m_pData->clear();
}

