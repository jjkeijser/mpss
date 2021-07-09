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

#ifndef MICMGMT_MPSSSTACKBASE_HPP
#define MICMGMT_MPSSSTACKBASE_HPP

// SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>
#include    <vector>
#include    <list>

// NAMESPACE
//
namespace  micmgmt
{

// FORWARD DECLARATIONS
//
class   MicBootConfigInfo;
class   MicPowerState;
struct  PciConfigData;


//============================================================================
//  CLASS:  MpssStackBase

class  MpssStackBase
{

public:

    enum  DeviceType   { eTypeUnknown, eTypeKnc, eTypeKnl };
    enum  DeviceMode   { eModeUnknown, eModeLinux, eModeElf, eModeFlash };
    enum  DeviceState  { eStateReady, eStateBooting, eStateNoResponse, eStateBootFailed,
                         eStateOnline, eStateShutdown, eStateNodeLost, eStateResetting,
                         eStateResetFailed, eStateBootingFW, eStateOnlineFW, eStateInvalid };


public:

    virtual ~MpssStackBase();

    std::string            deviceName() const;
    bool                   isKncDevice() const;
    bool                   isKnlDevice() const;
    int                    deviceNumber() const;
    virtual std::string    deviceBaseName() const;
    virtual std::string    mpssHomePath() const = 0;
    virtual std::string    mpssBootPath() const = 0;
    virtual std::string    mpssFlashPath() const = 0;

    uint32_t               getSystemMpssVersion( std::string* version ) const;
    virtual uint32_t       getSystemDriverVersion( std::string* version ) const = 0;
    virtual uint32_t       getSystemDeviceNumbers( std::vector<size_t>* list ) const = 0;
    virtual uint32_t       getSystemDeviceType( int* type, size_t number ) const = 0;
    virtual uint32_t       getSystemProperty( std::string* data, const std::string& name ) const = 0;

    virtual uint32_t       getDeviceType( int* type ) const;
    virtual uint32_t       getDeviceState( int* state ) const = 0;
    virtual uint32_t       getDevicePciConfigData( PciConfigData* data ) const = 0;
    virtual uint32_t       getDeviceProperty( std::string* data, const std::string& name ) const = 0;
    virtual uint32_t       getDeviceProperty( uint32_t* data, const std::string& name, int base=10 ) const;
    virtual uint32_t       getDeviceProperty( uint16_t* data, const std::string& name, int base=10 ) const;
    virtual uint32_t       getDeviceProperty( uint8_t* data, const std::string& name, int base=10 ) const;

    virtual uint32_t       deviceReset( bool force=false );
    virtual uint32_t       deviceBoot( const MicBootConfigInfo& info );
    virtual uint32_t       setDeviceProperty( const std::string& name, const std::string& data );
    virtual uint32_t       setDevicePowerStates( const std::list<MicPowerState>& states );


public: // STATIC

    static MpssStackBase*  createMpssStack( int device=-1 );
    static std::string     mpssVersion(int* mpssStack);
    static bool            isMockStack();


protected:

    explicit MpssStackBase( int device=-1 );


private:

    int  m_deviceNum;


private: // DISABLE

    MpssStackBase( const MpssStackBase& );
    MpssStackBase&  operator = ( const MpssStackBase& );

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MPSSSTACKBASE_HPP
