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
#include    "MicCoreUsageInfo.hpp"
#include    "MicValue.hpp"
//
#include    "CoreUsageData_p.hpp"  // Private

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::MicCoreUsageInfo   MicCoreUsageInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates core utilization data
 *
 *  The \b %MicCoreUsageInfo class encapsulates core utilization of a Mic
 *  device and provides accessors to the relevant information.
 *
 *  Please \b note that all accessors only return valid data if isValid()
 *  returns \c true.
 *
 *  Usage data is available in form of data vectors of 64-bit unsigned
 *  counters. The size of the data vector is usually equal to the number
 *  of active cores on the device. To access the usage data vectors, the
 *  %MicCoreUsageInfo class supports two methods:
 *  - User provided usage data vectors
 *  - Internal usage data vectors
 *
 *  If usage data is retrieved rarely, the use of the internal data vectors
 *  is a convenient way. The user does not have to worry about allocation
 *  and cleanup of the data vectors. The data vectors are available throughout
 *  the lifetime of the %MicCoreUsageInfo object.
 *
 *  In cases where core usage data is collected frequenctly, it is recommended
 *  to provide user preallocated data vectors to the %MicCoreUsageInfo class.
 *  A dedicated constructor is available for this purpose. The collected usage
 *  data is copied directly into these user buffers without any additional
 *  data buffer allocation, copying, and freeing. This reduces the use of
 *  system resources significantly.
 */


//============================================================================
//  P U B L I C   C O N S T A N T S
//============================================================================

//----------------------------------------------------------------------------
/** @enum   micmgmt::MicCoreUsageInfo::Counter
 *
 *  Enumerations of available usage counters.
 */

/** @var    MicCoreUsageInfo::Counter  MicCoreUsageInfo::eUser
 *
 *  User mode usage counter.
 */

/** @var    MicCoreUsageInfo::Counter  MicCoreUsageInfo::eSystem
 *
 *  System mode usage counter.
 */

/** @var    MicCoreUsageInfo::Counter  MicCoreUsageInfo::eNice
 *
 *  Nice mode usage counter.
 */

/** @var    MicCoreUsageInfo::Counter  MicCoreUsageInfo::eIdle
 *
 *  Idle mode usage counter.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     MicCoreUsageInfo::MicCoreUsageInfo()
 *
 *  Construct an empty core usage object.
 */

MicCoreUsageInfo::MicCoreUsageInfo() :
    m_pData( new CoreUsageData )
{
    m_pData->mCoreCount       = 0;
    m_pData->mCoreThreadCount = 0;
    m_pData->mFrequency       = 0;
    m_pData->mTickCount       = 0;
    m_pData->mTicksPerSecond  = 0;

    // The wonderful life of C++ enums ;-)
    for (unsigned int i=eUser; i<=eTotal; i++)
    {
        m_pData->mCounterTotal[i] = 0;
        m_pData->mpCounters[i]    = new Counters;
        m_pData->mCleanup[i]      = true;
    }

    m_pData->mValid = false;
}


//----------------------------------------------------------------------------
/** @fn     MicCoreUsageInfo::MicCoreUsageInfo( Counters* user, Counters* system, Counters* nice, Counters* idle )
 *  @param  user    Pointer to user counters data vector
 *  @param  system  Pointer to system counters data vector (optional)
 *  @param  nice    Pointer to nice counters data vector (optional)
 *  @param  idle    Pointer to idle counters data vector (optional)
 *
 *  Construct a core usage object using specified custom data vectors to
 *  return counters information. The % MicCoreUsageInfo object does \b not
 *  take ownership of the specified counters data vectors, it only uses the
 *  counters to return core usage information. It is the caller's
 *  responsibility to deallocate the data vectors.
 */

MicCoreUsageInfo::MicCoreUsageInfo( Counters* user, Counters* system, Counters* nice,
        Counters* idle, Counters* total ) :
    m_pData( new CoreUsageData )
{
    m_pData->mCoreCount       = 0;
    m_pData->mCoreThreadCount = 0;
    m_pData->mFrequency       = 0;
    m_pData->mTickCount       = 0;
    m_pData->mTicksPerSecond  = 0;

    m_pData->mCounterTotal[eUser]   = 0;
    m_pData->mCounterTotal[eSystem] = 0;
    m_pData->mCounterTotal[eNice]   = 0;
    m_pData->mCounterTotal[eIdle]   = 0;
    m_pData->mCounterTotal[eTotal]  = 0;

    m_pData->mpCounters[eUser]   = user   ? user   : new Counters;
    m_pData->mpCounters[eSystem] = system ? system : new Counters;
    m_pData->mpCounters[eNice]   = nice   ? nice   : new Counters;
    m_pData->mpCounters[eIdle]   = idle   ? idle   : new Counters;
    m_pData->mpCounters[eTotal]  = idle   ? total  : new Counters;

    m_pData->mCleanup[eUser]   = user   ? false : true;
    m_pData->mCleanup[eSystem] = system ? false : true;
    m_pData->mCleanup[eNice]   = nice   ? false : true;
    m_pData->mCleanup[eIdle]   = idle   ? false : true;
    m_pData->mCleanup[eTotal]  = total  ? false : true;

    m_pData->mValid = false;
}


//----------------------------------------------------------------------------
/** @fn     MicCoreUsageInfo::~MicCoreUsageInfo()
 *
 *  Cleanup.
 */

MicCoreUsageInfo::~MicCoreUsageInfo()
{
    // The wonderful life of C++ enums ;-)
    for (unsigned int i=eUser; i<=eTotal; i++)
    {
        if (m_pData->mCleanup[i] && m_pData->mpCounters[i])
            delete  m_pData->mpCounters[i];
    }
}


//----------------------------------------------------------------------------
/** @fn     bool  MicCoreUsageInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the core usage info is valid.
 */

bool  MicCoreUsageInfo::isValid() const
{
    return  m_pData->mValid;
}


//----------------------------------------------------------------------------
/** @fn     size_t  MicCoreUsageInfo::coreCount() const
 *  @return core count
 *
 *  Returns the number of available cores.
 */

size_t  MicCoreUsageInfo::coreCount() const
{
    return  (isValid() ? m_pData->mCoreCount : 0);
}


//----------------------------------------------------------------------------
/** @fn     size_t  MicCoreUsageInfo::coreThreadCount() const
 *  @return core thread count
 *
 *  Returns the number of threads per core.
 */

size_t  MicCoreUsageInfo::coreThreadCount() const
{
    return  (isValid() ? m_pData->mCoreThreadCount : 0);
}


//----------------------------------------------------------------------------
/** @fn     MicFrequency  MicCoreUsageInfo::frequency() const
 *  @return current CPU frequency
 *
 *  Returns the current CPU frequency.
 */

MicFrequency  MicCoreUsageInfo::frequency() const
{
    return  (isValid() ? m_pData->mFrequency : MicFrequency());
}


//----------------------------------------------------------------------------
/** @fn     uint64_t  MicCoreUsageInfo::tickCount() const
 *  @return tick count
 *
 *  Returns the tick count.
 */

uint64_t  MicCoreUsageInfo::tickCount() const
{
    return  (isValid() ? m_pData->mTickCount : 0);
}


//----------------------------------------------------------------------------
/** @fn     uint64_t  MicCoreUsageInfo::ticksPerSecond() const
 *  @return tick count
 *
 *  Returns the number of ticks per second. This information gives some
 *  inside on the possible resolution of the usage tick counts.
 */

uint64_t  MicCoreUsageInfo::ticksPerSecond() const
{
    return  (isValid() ? m_pData->mTicksPerSecond : 0);
}


//----------------------------------------------------------------------------
/** @fn     uint64_t  MicCoreUsageInfo::counterTotal( Counter type ) const
 *  @param  type    Counter type
 *  @return total count
 *
 *  Returns the total tick count of specified counter \a type for all cores.
 */

uint64_t  MicCoreUsageInfo::counterTotal( Counter type ) const
{
    return  (isValid() ? m_pData->mCounterTotal[type] : 0);
}


//----------------------------------------------------------------------------
/** @fn     uint64_t  MicCoreUsageInfo::counterValue( Counter type, size_t thread ) const
 *  @param  type    Counter type
 *  @param  thread  Thread number [0...]
 *  @return counter value
 *
 *  Returns the usage count of specified counter \a type and \a thread number.
 */

uint64_t  MicCoreUsageInfo::counterValue( Counter type, size_t thread ) const
{
    if (isValid() && (thread < m_pData->mpCounters[type]->size()))
        return  m_pData->mpCounters[type]->at( thread );

    return  0;
}


//----------------------------------------------------------------------------
/** @fn     Counters  MicCoreUsageInfo::usageCounters( Counter type ) const
 *  @param  type      Counter type
 *  @return usage counters data vector
 *
 *  Returns a usage counters data vector.
 */

MicCoreUsageInfo::Counters  MicCoreUsageInfo::usageCounters( Counter type ) const
{
    return  (isValid() ? Counters( *m_pData->mpCounters[type] ) : Counters());
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreUsageInfo::clear()
 *
 *  Clear core usage info.
 */

void  MicCoreUsageInfo::clear()
{
    m_pData->mCoreCount       = 0;
    m_pData->mCoreThreadCount = 0;
    m_pData->mTickCount       = 0;
    m_pData->mTicksPerSecond  = 0;

    m_pData->mCounterTotal[eUser]   = 0;
    m_pData->mCounterTotal[eSystem] = 0;
    m_pData->mCounterTotal[eNice]   = 0;
    m_pData->mCounterTotal[eIdle]   = 0;
    m_pData->mCounterTotal[eTotal]  = 0;

    m_pData->mpCounters[eUser]->clear();
    m_pData->mpCounters[eSystem]->clear();
    m_pData->mpCounters[eNice]->clear();
    m_pData->mpCounters[eIdle]->clear();
    m_pData->mpCounters[eTotal]->clear();

    m_pData->mValid = false;
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreUsageInfo::setCoreCount( size_t count )
 *  @param  count   Core count
 *
 *  Set the number of cores for which usage data is stored.
 */

void  MicCoreUsageInfo::setCoreCount( size_t count )
{
    m_pData->mCoreCount = count;
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreUsageInfo::setCoreThreadCount( size_t count )
 *  @param  count
 *
 *  Set the number of threads per core for this usage data.
 */

void  MicCoreUsageInfo::setCoreThreadCount( size_t count )
{
    m_pData->mCoreThreadCount = count;

    if (count > 0 && m_pData->mCoreCount > 0)
    {
        m_pData->mpCounters[eUser]->resize( m_pData->mCoreCount * count );
        m_pData->mpCounters[eSystem]->resize( m_pData->mCoreCount * count );
        m_pData->mpCounters[eNice]->resize( m_pData->mCoreCount * count);
        m_pData->mpCounters[eIdle]->resize( m_pData->mCoreCount * count );
        m_pData->mpCounters[eTotal]->resize( m_pData->mCoreCount * count );
    }
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreUsageInfo::setFrequency( uint32_t frequency )
 *  @param  frequency
 *
 *  Set the current CPU frequency for this usage data.
 */

void  MicCoreUsageInfo::setFrequency( uint32_t frequency )
{
    m_pData->mFrequency = MicFrequency(frequency, MicFrequency::eMega);
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreUsageInfo::setTickCount( uint64_t count )
 *  @param  count   Tick count
 *
 *  Set the tick count at the time of the usage data collection. The tick
 *  count is related to the processor OS on which the data collector runs.
 */

void  MicCoreUsageInfo::setTickCount( uint64_t count )
{
    m_pData->mTickCount = count;
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreUsageInfo::setTicksPerSecond( uint64_t ticks )
 *  @param  ticks   Tick count
 *
 *  Set the number of ticks per second that the underlying platform reports.
 */

void  MicCoreUsageInfo::setTicksPerSecond( uint64_t ticks )
{
    m_pData->mTicksPerSecond = ticks;
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreUsageInfo::setCounterTotal( Counter type, uint64_t total )
 *  @param  type    Counter type
 *  @param  total   Counter total
 *
 *  Set the counter total over all cores for specified counter \a type.
 */

void  MicCoreUsageInfo::setCounterTotal( Counter type, uint64_t total )
{
    m_pData->mCounterTotal[type] = total;
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreUsageInfo::setUsageCount( Counter type, size_t thread, uint64_t count )
 *  @param  type    Counter type
 *  @param  thread  Thread number
 *  @param  count   Usage count
 *
 *  Set the \a thread usage \a count for counter of specified \a type.
 */

void  MicCoreUsageInfo::setUsageCount( Counter type, size_t thread, uint64_t count )
{
    if (thread < m_pData->mCoreCount * m_pData->mCoreThreadCount)
        m_pData->mpCounters[type]->at( thread ) = count;
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreUsageInfo::setUsageCount( Counter type, const Counters& data )
 *  @param  type    Counter type
 *  @param  data    Counter data vector
 *
 *  Copy data from specified \a data vector. The size of the internal data
 *  vector is not adjusted. That means that data from the specified \a data
 *  vector may be partially ignored or internal data may be partially cleared,
 *  depending on difference in size of the specified data vector and the
 *  internal data vector.
 */

void  MicCoreUsageInfo::setUsageCount( Counter type, const Counters& data )
{
    if (data.size() == 0)
    {
        m_pData->mpCounters[type]->clear();
    }
    else if (data.size() < m_pData->mpCounters[type]->size())
    {
        for (size_t i=0; i<data.size(); i++)
            m_pData->mpCounters[type]->at( i ) = data.at( i );

        for (size_t i=data.size(); i<m_pData->mpCounters[type]->size(); i++)
            m_pData->mpCounters[type]->at( i ) = 0;
    }
    else if (data.size() > m_pData->mpCounters[type]->size())
    {
        for (size_t i=0; i<m_pData->mpCounters[type]->size(); i++)
            m_pData->mpCounters[type]->at( i ) = data.at( i );
    }
    else
    {
        *(m_pData->mpCounters[type]) = data;
    }
}


//----------------------------------------------------------------------------
/** @fn     void  MicCoreUsageInfo::setValid( bool state )
 *  @param  state   valid state
 *
 *  Set the data valid \a state.
 */

void  MicCoreUsageInfo::setValid( bool state )
{
    m_pData->mValid = state;
}


