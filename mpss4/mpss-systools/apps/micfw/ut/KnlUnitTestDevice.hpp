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

#ifndef MICMGMT_KNLUNITTESTDEVICE_HPP
#define MICMGMT_KNLUNITTESTDEVICE_HPP

// PROJECT INCLUDES
//
#include    "KnlMockDevice.hpp"
#include    "FlashStatus.hpp"
// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  KnlUnitTestDevice
class  KnlUnitTestDevice : public KnlMockDevice
{

public:

    explicit KnlUnitTestDevice( int number=0 );
   ~KnlUnitTestDevice();

// Programmable Scenarios...

    void  scenarioReset();

    void  scenarioStartFlashFails(uint32_t errorCode);

    void  scenarioSetState(FlashStatus::State state, uint32_t errorCode);

    void  scenarioFlashTimeChange(int iteration);

    void  scenarioGetDevicePlatformInfoFails(uint32_t errorCode);

    void  scenarioApiErrorInject(uint32_t errorCode);

    void  scenarioSetRegisterNSize(size_t size);

    void  scenarioSetPostCode(int postCode = -1);

    void  scenarioSetSMBARetrainTime(unsigned int timeToRetrain);

    void  scenarioSetPersistence(bool persist);

    void  setFabVersion(const std::string fab);

    void  setFSM(const unsigned int beginFSM);


private:

    bool      getMockStatus() const;
    uint32_t  getDeviceVersionInfo( MicVersionInfo* info ) const;
    uint32_t  getDevicePlatformInfo( MicPlatformInfo* info ) const;
    uint32_t  getDevicePowerThresholdInfo( MicPowerThresholdInfo* info ) const;
    uint32_t  getDeviceFlashUpdateStatus( FlashStatus* status ) const; // **
    uint32_t  getDeviceSmBusAddressTrainingStatus( bool* busy, int* eta ) const;
    uint32_t  getDeviceSmcRegisterData( uint8_t offset, uint8_t* data, size_t* size ) const;
    uint32_t  getDevicePostCode( std::string* code ) const;

    uint32_t  setDeviceSmcRegisterData( uint8_t offset, const uint8_t* data, size_t size );
    uint32_t  startDeviceFlashUpdate( FlashImage* image, bool force );
    uint32_t  startDeviceFlashUpdate( std::string & path, std::unique_ptr<micmgmt::FwToUpdate> & fwToUpdate );
    uint32_t  finalizeDeviceFlashUpdate( bool abort );
    uint32_t  setDevicePowerThreshold( MicDevice::PowerWindow window, uint32_t power, uint32_t time );
    uint32_t  startDeviceSmBusAddressTraining( int hint, int timeout );
    uint32_t  setDeviceSmcPersistenceEnabled( bool state );


private:

    struct PrivData;
    PrivData*  m_pData;
    mutable unsigned int m_FSM;
    mutable unsigned int m_Spad;
    mutable unsigned int m_StartDelay;


private:    // DISABLE

    KnlUnitTestDevice( const KnlUnitTestDevice& );
    KnlUnitTestDevice&  operator = ( const KnlUnitTestDevice& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_KNLUNITTESTDEVICE_HPP
