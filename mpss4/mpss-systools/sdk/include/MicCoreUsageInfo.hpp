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

#ifndef MICMGMT_MICCOREUSAGEINFO_HPP
#define MICMGMT_MICCOREUSAGEINFO_HPP

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <memory>
#include    <vector>

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
class   MicFrequency;
struct  CoreUsageData;


//----------------------------------------------------------------------------
//  CLASS:  MicCoreUsageInfo

class  MicCoreUsageInfo
{

public:

    enum  Counter  { eUser=0, eSystem=1, eNice=2, eIdle=3, eTotal=4 };

    typedef std::vector<uint64_t>  Counters;


public:

    MicCoreUsageInfo();
    explicit MicCoreUsageInfo( Counters* user, Counters* system=0, Counters* nice=0,
            Counters* idle=0, Counters* total=0 );
   ~MicCoreUsageInfo();

    bool            isValid() const;
    size_t          coreCount() const;
    size_t          coreThreadCount() const;
    MicFrequency    frequency() const;
    uint64_t        tickCount() const;
    uint64_t        ticksPerSecond() const;
    uint64_t        counterTotal( Counter type ) const;
    uint64_t        counterValue( Counter type, size_t thread ) const;
    Counters        usageCounters( Counter type ) const;

    void            clear();
    void            setValid( bool state );
    void            setCoreCount( size_t count );
    void            setCoreThreadCount( size_t count );
    void            setFrequency( uint32_t frequency );
    void            setTickCount( uint64_t count );
    void            setTicksPerSecond( uint64_t ticks );
    void            setCounterTotal( Counter type, uint64_t total );
    void            setUsageCount( Counter type, size_t thread, uint64_t count );
    void            setUsageCount( Counter type, const Counters& data );


private:

    std::unique_ptr<CoreUsageData>  m_pData;


private:    // DISABLE

    MicCoreUsageInfo( const MicCoreUsageInfo& );
    MicCoreUsageInfo&  operator = ( const MicCoreUsageInfo& );

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICCOREUSAGEINFO_HPP
