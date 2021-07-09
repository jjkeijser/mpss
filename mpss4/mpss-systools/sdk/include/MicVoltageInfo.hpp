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

#ifndef MICMGMT_MICVOLTAGEINFO_HPP
#define MICMGMT_MICVOLTAGEINFO_HPP

//  SYSTEM INCLUDES
//
#include    <memory>

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
class   MicVoltage;
struct  VoltageInfoData;


//----------------------------------------------------------------------------
//  CLASS:  MicVoltageInfo

class  MicVoltageInfo
{

public:

    MicVoltageInfo();
    explicit MicVoltageInfo( const VoltageInfoData& data );
    MicVoltageInfo( const MicVoltageInfo& that );
   ~MicVoltageInfo();

    MicVoltageInfo&  operator = ( const MicVoltageInfo& that );

    bool             isValid() const;
    size_t           sensorCount() const;
    size_t           maximumSensorNameWidth() const;
    MicVoltage       maximumSensorValue() const;
    MicVoltage       sensorValueAt( size_t index ) const;


private:

    std::unique_ptr<VoltageInfoData>  m_pData;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICVOLTAGEINFO_HPP
