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

#ifndef MICMGMT_MICTHERMALINFO_HPP
#define MICMGMT_MICTHERMALINFO_HPP

//  SYSTEM INCLUDES
//
#include    <memory>
#include    <vector>

//  PROJECT INCLUDES
//
#include "MicValue.hpp"

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
class   MicTemperature;
class   ThrottleInfo;
struct  ThermalInfoData;


//----------------------------------------------------------------------------
//  CLASS:  MicThermalInfo

class  MicThermalInfo
{

public:

    MicThermalInfo();
    explicit MicThermalInfo( const ThermalInfoData& data );
    MicThermalInfo( const MicThermalInfo& that );
   ~MicThermalInfo();

    MicThermalInfo&  operator = ( const MicThermalInfo& that );

    bool             isValid() const;
    size_t           sensorCount() const;
    size_t           maximumSensorNameWidth() const;
    MicTemperature   maximumSensorValue() const;
    MicTemperature   sensorValueAt( size_t index ) const;
    MicTemperature   sensorValueByName(const std::string& name) const;
    MicValueUInt32   fanRpm() const;
    MicValueUInt32   fanPwm() const;
    MicValueUInt32   fanAdder() const;
    MicTemperature   fanBoostThreshold() const;
    MicTemperature   throttleThreshold() const;
    ThrottleInfo     throttleInfo() const;

    std::vector<MicTemperature>  sensors() const;


private:

    std::unique_ptr<ThermalInfoData>  m_pData;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICTHERMALINFO_HPP
