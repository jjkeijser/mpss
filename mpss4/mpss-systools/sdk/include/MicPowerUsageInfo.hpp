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

#ifndef MICMGMT_MICPOWERUSAGEINFO_HPP
#define MICMGMT_MICPOWERUSAGEINFO_HPP

//  SYSTEM INCLUDES
//
#include    <memory>
#include    <vector>

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
class   MicPower;
class   ThrottleInfo;
struct  PowerUsageData;


//----------------------------------------------------------------------------
//  CLASS:  MicPowerUsageInfo

class  MicPowerUsageInfo
{

public:

    MicPowerUsageInfo();
    explicit MicPowerUsageInfo( const PowerUsageData& data );
    MicPowerUsageInfo( const MicPowerUsageInfo& that );
   ~MicPowerUsageInfo();

    MicPowerUsageInfo&  operator = ( const MicPowerUsageInfo& that );

    bool                isValid() const;
    size_t              sensorCount() const;
    size_t              maximumSensorNameWidth() const;
    MicPower            maximumSensorValue() const;
    MicPower            sensorValueAt( size_t index ) const;
    MicPower            throttleThreshold() const;
    ThrottleInfo        throttleInfo() const;

    std::vector<MicPower>  sensors() const;


private:

    std::unique_ptr<PowerUsageData>  m_pData;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICPOWERUSAGEINFO_HPP
