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

#ifndef MICMGMT_MICPOWERTHRESHOLDINFO_HPP
#define MICMGMT_MICPOWERTHRESHOLDINFO_HPP

//  SYSTEM INCLUDES
//
#include    <memory>

// NAMESPACE
//
namespace  micmgmt {

// FORWARD DECLARATIONS
//
class   MicPower;
struct  PowerThresholdData;


//----------------------------------------------------------------------------
//  CLASS:  MicPowerThresholdInfo

class  MicPowerThresholdInfo
{

public:

    MicPowerThresholdInfo();
    explicit MicPowerThresholdInfo( const PowerThresholdData& data );
    MicPowerThresholdInfo( const MicPowerThresholdInfo& that );
   ~MicPowerThresholdInfo();

    MicPowerThresholdInfo&  operator = ( const MicPowerThresholdInfo& that );

    bool                    isValid() const;
    bool                    isPersistent() const;
    MicPower                loThreshold() const;
    MicPower                hiThreshold() const;
    MicPower                maximum() const;
    MicPower                window0Threshold() const;
    MicPower                window1Threshold() const;
    int                     window0Time() const;
    int                     window1Time() const;


private:

    std::unique_ptr<PowerThresholdData>  m_pData;

};

//----------------------------------------------------------------------------

}

#endif // MICMGMT_MICPOWERTHRESHOLDINFO_HPP
