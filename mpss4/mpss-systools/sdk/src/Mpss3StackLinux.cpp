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
#include    "Mpss3StackLinux.hpp"
#include    "MicDeviceError.hpp"
#include    "MicPciConfigInfo.hpp"
#include    "MicSpeed.hpp"
#include    "MicPowerState.hpp"
#include    "SharedLibrary.hpp"
#include    "micmgmtCommon.hpp"
//
#include    "PciConfigData_p.hpp"

// MPSS INCLUDES
//
#ifdef  __cplusplus
extern "C" {    // Linux driver team wants to ignore C++
#endif
#include    "../../3rd-party/linux/drivers/mpss3/mpssconfig.h"  /// \todo Fix when final header path known
#ifdef  __cplusplus
}
#endif

// SYSTEM INCLUDES
//
#include    <list>
#include    <cstdio>
#include    <cstring>
#include    <fstream>
#include    <sstream>
#include    <string>
#include    <dirent.h>
#include    <sys/types.h>

// LOCAL CONSTANTS
//
namespace  {
const char* const  MPSS_DEFAULT_BOOT_DIR         = "boot";
const char* const  MPSS_DEFAULT_FLASH_DIR        = "flash";
const char* const  MPSSCONFIG_SHLIB_NAME         = "libmpssconfig";
const int          MPSSCONFIG_SHLIB_MAJOR        = 0;
const char* const  MPSSCONFIG_FNAME_READ_PM      = "mpssconfig_read_pm";
const char* const  MPSSCONFIG_FNAME_UPDATE_PM    = "mpssconfig_update_pm";
const char* const  SYSFS_MIC_MODULE_PATH         = "/sys/module/mic";
const char* const  SYSFS_MIC_CLASS_PATH          = "/sys/class/mic";
const char* const  PROPKEY_DRIVER_VERSION        = "ctrl/version";
const char* const  PROPKEY_DEVICE_TYPE           = "family";
const char* const  PROPKEY_DEVICE_STATE          = "state";
const char* const  PROPKEY_DEVICE_MODE           = "mode";
const char* const  PROPVAL_DEVICE_TYPE_KNC       = "x100";
const char* const  PROPVAL_KNCDEVICE_RESET_FORCE = "reset:force";
const char* const  PROPVAL_KNCDEVICE_RESET       = "reset";
const size_t       PCI_VENDOR_ID_OFFSET          = 0;
const size_t       PCI_DEVICE_ID_OFFSET          = 2;
const size_t       PCI_REVISION_ID_OFFSET        = 8;
const size_t       PCI_SUBSYSTEM_ID_OFFSET       = 0x2e;
const size_t       PCI_CTRL_CAP_OFFSET           = 0x54;
const size_t       PCI_LINK_CTRL_OFFSET          = 0x5c;
const size_t       PCI_LINK_STATS_OFFSET         = 0x5e;
const size_t       PCI_NOACCESS_LEN              = 0x40;
}

// PRIVATE DATA
//
namespace  micmgmt {
struct  Mpss3StackLinux::PrivData
{
    typedef int (*mpssconfig_read_pm)( char* name, int* cfreq, int* corec6, int* pc3, int* pc6 );
    typedef int (*mpssconfig_update_pm)( char* name, int cfreq, int corec6, int pc3, int pc6 );

    mpssconfig_read_pm    mpMpssReadPm;     // Function pointer
    mpssconfig_update_pm  mpMpssUpdatePm;   // Ditto
    SharedLibrary         mMpssConfigLib;
};
}

// NAMESPACES
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::Mpss3StackLinux  Mpss3StackLinux.hpp
 *  @ingroup  sdk
 *  @brief    The class represents a MPSS 3.x stack class for Linux
 *
 *  The \b %Mpss3StackLinux class encapsulates a Linux based MPSS 3.x stack.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     Mpss3StackLinux::Mpss3StackLinux( int device )
 *  @param  device  Device number
 *
 *  Construct a MPSS 3.x stack object using specified \a device number.
 */

Mpss3StackLinux::Mpss3StackLinux( int device ) :
    Mpss3StackBase( device ),
    m_pData( new PrivData )
{
    m_pData->mpMpssReadPm   = 0;
    m_pData->mpMpssUpdatePm = 0;

    loadLib();
}


//----------------------------------------------------------------------------
/** @fn     Mpss3StackLinux::~Mpss3StackLinux()
 *
 *  Cleanup.
 */

Mpss3StackLinux::~Mpss3StackLinux()
{
    unloadLib();
}


//----------------------------------------------------------------------------
/** @fn     std::string  Mpss3StackBase::mpssHomePath() const
 *  @return home path
 *
 *  Returns the MPSS home directory path.
 */

std::string  Mpss3StackLinux::mpssHomePath() const
{
    return  micmgmt::mpssInstallFolder();
}


//----------------------------------------------------------------------------
/** @fn     std::string  Mpss3StackLinux::mpssBootPath() const
 *  @return boot path
 *
 *  Returns the MPSS boot image directory path.
 */

std::string  Mpss3StackLinux::mpssBootPath() const
{
    return  string( mpssHomePath() + '/' + MPSS_DEFAULT_BOOT_DIR );
}


//----------------------------------------------------------------------------
/** @fn     std::string  Mpss3StackLinux::mpssFlashPath() const
 *  @return flash path
 *
 *  Returns the MPSS flash image directory path.
 */

std::string  Mpss3StackLinux::mpssFlashPath() const
{
    return  string( mpssHomePath() + '/' + MPSS_DEFAULT_FLASH_DIR );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackLinux::getSystemDriverVersion( std::string* version ) const
 *  @param  version  Version string return
 *  @return error code
 *
 *  Determine the device driver version and return in specified \a version
 *  parameter.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DRIVER_NOT_LOADED
 *  - MICSDKERR_DRIVER_NOT_INITIALIZED
 */

uint32_t  Mpss3StackLinux::getSystemDriverVersion( std::string* version ) const
{
    if (!version)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    version->clear();

    string    sdata;

    // Long live(d) backward compatibility...

    DIR*  dir = opendir( SYSFS_MIC_MODULE_PATH );
    if (dir == NULL)
        return  MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_LOADED );
    else
        closedir( dir );

    dir = opendir( SYSFS_MIC_CLASS_PATH );
    if (dir == NULL)
        return  MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_INITIALIZED );

    bool  have_ctrl = false;
    bool  have_scif = false;

    struct dirent*  entry = NULL;
    while ((entry = readdir( dir )))
    {
        if (strcmp( "ctrl", entry->d_name ) == 0)
        {
            have_ctrl = true;
            if (have_scif)
                break;
        }
        else if (strcmp( "scif", entry->d_name ) == 0)
        {
            have_scif = true;
            if (have_ctrl)
                break;
        }
    }

    closedir( dir );

    if (have_ctrl)
    {
        // We found a KNC driver that provides version info
        uint32_t  result = getSystemProperty( &sdata, PROPKEY_DRIVER_VERSION );
        if (MicDeviceError::isError( result ))
            return  result;

        string::size_type  spacesep = sdata.find( " " );
        if (spacesep != string::npos)
            *version = sdata.substr( 0, spacesep );
        else
            *version = sdata;

        return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    }
    else if (have_scif)
    {
        // We found a MIC driver that does not provide version info

        // No version, but a kind of success anyway
        return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    }

    // We must have a inconsistent configuration somehow

    return  MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_INITIALIZED );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackLinux::getSystemDeviceNumbers( std::vector<size_t>* list ) const
 *  @param  list    Pointer to device number list return
 *  @return error code
 *
 *  Create and return a \a list of detected device numbers in the system.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_DRIVER_NOT_LOADED
 *  - MICSDKERR_DRIVER_NOT_INITIALIZED
 */

uint32_t  Mpss3StackLinux::getSystemDeviceNumbers( std::vector<size_t>* list ) const
{
    if (!list)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    list->clear();

    DIR*  dir = opendir( SYSFS_MIC_MODULE_PATH );
    if (dir == NULL)
        return  MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_LOADED );
    else
        closedir( dir );

    dir = opendir( SYSFS_MIC_CLASS_PATH );
    if (dir == NULL)
        return  MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_INITIALIZED );

    std::list<size_t>  devices;

    struct dirent*  entry = NULL;
    while ((entry = readdir( dir )))
    {
        string  filename = entry->d_name;
        if (filename.find( deviceBaseName() ) != 0)
            continue;   // mic*

        string::size_type  numpos = deviceBaseName().length();
        if (filename.length() == numpos)
            continue;   // mic

        string::size_type  numend = filename.find_first_not_of( "0123456789", numpos );
        if (numend == numpos)
            continue;   // mice or similar

        stringstream  strm( filename.substr( numpos, numend-numpos ) );
        unsigned int  micnum;
        strm >> micnum;
        if (!strm)
            continue;   // not a number

        devices.push_back( micnum );
    }

    closedir( dir );

    if (!devices.empty())
    {
        devices.sort();
        list->resize( devices.size() );
        std::copy( devices.begin(), devices.end(), list->begin() );
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackLinux::getSystemDeviceType( int* type, size_t number ) const
 *  @param  type    Device type return
 *  @param  number  Device number
 *  @return error code
 *
 *  Determine and return the device \a type for device with given \a number.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss3StackLinux::getSystemDeviceType( int* type, size_t number ) const
{
    if (!type)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    stringstream  strm;
    strm << deviceBaseName() << number;
    strm << '/' << PROPKEY_DEVICE_TYPE;

    string    propval;
    uint32_t  result = getSystemProperty( &propval, strm.str() );
    if (MicDeviceError::isError( result ))
    {
        *type = eTypeUnknown;
        return  result;
    }

    if (propval == PROPVAL_DEVICE_TYPE_KNC)
        *type = eTypeKnc;
    else
        *type = eTypeUnknown;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackLinux::getSystemProperty( std::string* data, const std::string& name ) const
 *  @param  data    Data return
 *  @param  name    Property name
 *  @return error code
 *
 *  Get system property with given \a name and return property \a data.
 *
 *  Under Linux, device properties are stored in the SYSFS system. In case of
 *  MIC devices, the property is mapped to /sys/class/mic/\<name\>.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss3StackLinux::getSystemProperty( std::string* data, const std::string& name ) const
{
    if (!data || name.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    data->clear();

    string  path = SYSFS_MIC_CLASS_PATH;
    path += '/' + name;

    bool  success = false;

    std::ifstream  strm( path );
    if (strm.is_open())
    {
        getline( strm, *data );
        success = strm.fail() ? false : true;
        strm.close();
    }

    if (!success)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackLinux::getDevicePciConfigData( PciConfigData* data ) const
 *  @param  data    Pointer to PCI config data return
 *  @return error code
 *
 *  Determine and return PCI configuration for the associated device into
 *  specified PCI \a data structure.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss3StackLinux::getDevicePciConfigData( PciConfigData* data ) const
{
    if (!data)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    data->clear();      // Clear existing data

    string    propval;
    uint32_t  result = getDeviceProperty( &propval, "device/uevent", "PCI_CLASS" );
    if (MicDeviceError::isError( result ))
        return  result;

    stringstream  strm;
    strm << std::hex << propval;
    strm >> data->mClassCode;
    propval.clear();

    result = getDeviceProperty( &propval, "device/uevent", "PCI_SLOT_NAME" );
    if (MicDeviceError::isError( result ))
        return  result;

    data->mPciAddress = propval;
    propval.clear();

    result = getDeviceProperty( &propval, "device/config" );
    if (MicDeviceError::isError( result ))
        return  result;

    // Extra device info from binary data
    data->mVendorId    = *(reinterpret_cast<const uint16_t*>( &propval.c_str()[PCI_VENDOR_ID_OFFSET]    ));
    data->mDeviceId    = *(reinterpret_cast<const uint16_t*>( &propval.c_str()[PCI_DEVICE_ID_OFFSET]    ));
    data->mRevisionId  = *(reinterpret_cast<const uint8_t*>(  &propval.c_str()[PCI_REVISION_ID_OFFSET]  ));
    data->mSubsystemId = *(reinterpret_cast<const uint16_t*>( &propval.c_str()[PCI_SUBSYSTEM_ID_OFFSET] ));

    // Check how much we were able to read from device/config to determine mHasAccess
    if(propval.length() < PCI_NOACCESS_LEN + 1)
    {
        data->mHasAccess = false;
        data->mValid     = true;
        return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    }

    // Link Info and Capabilities
    uint16_t  linkstat = 0;
    uint16_t  ctrlcap  = 0;
    uint16_t  linkctrl = 0;

    // Link Info (if available)
    if (propval.length() >= PCI_LINK_STATS_OFFSET + 2)
    {
        linkstat = *(reinterpret_cast<const uint16_t*>( &propval.c_str()[PCI_LINK_STATS_OFFSET] ));

        switch ((linkstat >> 0) & 0x0f)
        {
          case  1:
            data->mLinkSpeed = MicSpeed( 2500, MicSpeed::eMega );
            break;

          case  2:
            data->mLinkSpeed = MicSpeed( 5000, MicSpeed::eMega );
            break;

          default:
            data->mLinkSpeed = MicSpeed();
            break;
        }

        data->mLinkWidth = ((linkstat >> 4) & 0x3f);
    }

    // Payload + Request Size (if available)
    if (propval.length() >= PCI_CTRL_CAP_OFFSET + 2)
    {
        ctrlcap = *(reinterpret_cast<const uint16_t*>( &propval.c_str()[PCI_CTRL_CAP_OFFSET] ));

        data->mPayloadSize  = 128 * (1 << ((ctrlcap & 0x00e0) >>  5));
        data->mRequestSize  = 128 * (1 << ((ctrlcap & 0x7000) >> 12));
        data->mRelaxedOrder = ((ctrlcap & 0x0010) >>  4);
        data->mExtTagEnable = ((ctrlcap & 0x0100) >>  8);
        data->mNoSnoop      = ((ctrlcap & 0x0800) >> 11);

        linkctrl = *(reinterpret_cast<const uint16_t*>( &propval.c_str()[PCI_LINK_CTRL_OFFSET] ));

        switch (linkctrl & 0x0003)
        {
          case  0:
            data->mAspmControl = MicPciConfigInfo::eL0sL1Disabled;
            break;

          case  1:
            data->mAspmControl = MicPciConfigInfo::eL0sEnabled;
            break;

          case  2:
            data->mAspmControl = MicPciConfigInfo::eL1Enabled;
            break;

          case  3:
            data->mAspmControl = MicPciConfigInfo::eL0sL1Enabled;
            break;

          default:
            data->mAspmControl = MicPciConfigInfo::eUnknown;
            break;
        }
    }

    // If we've gotten this far, we definitely have access
    data->mHasAccess = true;
    data->mValid     = true;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackLinux::getDeviceProperty( std::string* data, const std::string& name ) const
 *  @param  data    Data return
 *  @param  name    Property name
 *  @return error code
 *
 *  Get device property with given \a name and return property \a data.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INTERNAL_ERROR
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_PROPERTY_NOT_FOUND
 */

uint32_t  Mpss3StackLinux::getDeviceProperty( std::string* data, const std::string& name ) const
{
    if (!data || name.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (deviceNumber() < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );

    data->clear();

    string  path = SYSFS_MIC_CLASS_PATH;
    path += '/' + deviceName();
    path += '/' + name;

    bool  success = false;

    std::ifstream  strm( path );
    if (!strm.is_open())
    {
        if (errno == EACCES)
            return  MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );
        else
            return  MicDeviceError::errorCode( MICSDKERR_PROPERTY_NOT_FOUND );
    }

    getline( strm, *data );
    success = strm.fail() ? false : true;
    strm.close();

    if (!success)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackLinux::deviceReset( bool force )
 *  @param  force   Force flag (optional)
 *  @return error code
 *
 *  Reset device. This brings a device from online state to ready state, if
 *  all goes well.
 *
 *  This function is semi-blocking that will return as soon as the reset
 *  has been accepted by the device driver. This, however, can take up to
 *  around a second for a device. Therefore, if this is not an acceptable
 *  behavior for the caller's application, we recommend to run the device
 *  in a separate thread.
 *
 *  The caller should call the getDeviceState() function to observe the
 *  device state transitions until the final MicDevice::eReady state has
 *  been reached.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss3StackLinux::deviceReset( bool force )
{
    if (isKncDevice())
    {
        string  reset = force ? PROPVAL_KNCDEVICE_RESET : PROPVAL_KNCDEVICE_RESET_FORCE;
        return  setDeviceProperty( PROPKEY_DEVICE_STATE, reset );
    }

    return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackLinux::setDeviceProperty( const std::string& name, const std::string& data )
 *  @param  name    Property name
 *  @param  data    Property data
 *  @return error code
 *
 *  Write \a data to the device property with given \a name.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_PROPERTY_NOT_FOUND
 */

uint32_t  Mpss3StackLinux::setDeviceProperty( const std::string& name, const std::string& data )
{
    if (name.empty() || data.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (deviceNumber() < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );

    string  path = SYSFS_MIC_CLASS_PATH;
    path += '/' + deviceName();
    path += '/' + name;

    bool  success = false;
    int   errorno = 0;

    std::ofstream  strm( path );
    if (strm.is_open())
    {
        strm.write( data.c_str(), data.size() );    // No LF required
        if (strm.good())
            success = true;
        else
            errorno = errno;
        strm.close();
    }
    else
    {
        errorno = errno;
    }

    if (!success)
    {
        if (errorno == EACCES)
            return  MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );
        else
            return  MicDeviceError::errorCode( MICSDKERR_PROPERTY_NOT_FOUND );
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackLinux::setDevicePowerStates( const std::list<MicPowerState>& states )
 *  @param  states  Power states
 *  @return error code
 *
 *  Set device power states according given power \a states.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_SHARED_LIBRARY_ERROR
 *  - MICSDKERR_DEVICE_IO_ERROR
 */

uint32_t  Mpss3StackLinux::setDevicePowerStates( const std::list<MicPowerState>& states )
{
    if (states.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (deviceNumber() < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );

    if (!m_pData->mpMpssUpdatePm)
        return  MicDeviceError::errorCode( MICSDKERR_SHARED_LIBRARY_ERROR );

    int  cpu = 0;
    int  co6 = 0;
    int  pc3 = 0;
    int  pc6 = 0;

    for (auto ps=states.begin(); ps!=states.end(); ps++)
    {
        if ((ps->state() == MicPowerState::eCpuFreq) && ps->isEnabled())
            cpu = 1;

        if ((ps->state() == MicPowerState::eCoreC6) && ps->isEnabled())
            co6 = 1;

        if ((ps->state() == MicPowerState::ePc3) && ps->isEnabled())
            pc3 = 1;

        if ((ps->state() == MicPowerState::ePc6) && ps->isEnabled())
            pc6 = 1;
    }

    int  result = m_pData->mpMpssUpdatePm( const_cast<char*>( deviceName().c_str() ), cpu, co6, pc3, pc6 );

//  The following error code seems to have been removed from the current SCIF code
//
//    if (result == CONFIG_ERROR_EXIST)
//        return  MicDeviceError::errorCode( MICSDKERR_NO_SUCH_DEVICE );

    if (result != 0)
        return  MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//============================================================================
//  P R I V A T E   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss3StackLinux::getDeviceProperty( std::string* data, const std::string& name, const std::string& subkey, char sep ) const
 *  @param  data    Data return
 *  @param  name    Property name
 *  @param  subkey  Subkey
 *  @param  sep     Name/value separator character (optional)
 *  @return error code
 *
 *  Get device property with given \a name for device with specified \a device
 *  number and \a subkey. The property data is returned in \a data.
 *
 *  Optionally, the character used as separator between key and value can be
 *  specified. The default character is '='.
 *
 *  On success, MICSDKERR_SUCCESS is returned.
 *  On failure, one of the following error codes may be returned:
 *  - MICSDKERR_INVALID_ARG
 *  - MICSDKERR_INVALID_DEVICE_NUMBER
 *  - MICSDKERR_NO_ACCESS
 *  - MICSDKERR_PROPERTY_NOT_FOUND
 *  - MICSDKERR_INTERNAL_ERROR
 */

uint32_t  Mpss3StackLinux::getDeviceProperty( std::string* data, const std::string& name, const std::string& subkey, char sep ) const
{
    if (!data || name.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (deviceNumber() < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );

    if (subkey.empty())
        return  getDeviceProperty( data, name );

    data->clear();

    string  path = SYSFS_MIC_CLASS_PATH;
    path += '/' + deviceName();
    path += '/' + name;

    bool  success = false;

    std::ifstream  strm( path );
    if (strm.is_open())
    {
        string  line;
        while (strm.good())
        {
            getline( strm, line );
            if (line.find( subkey ) != string::npos)    // Find line with subkey
            {
                string::size_type  valpos = line.find( sep );
                if ((valpos != string::npos) && (++valpos != string::npos))
                {
                    *data = line.substr( valpos );      // Extract data
                    success = true;
                }

                break;
            }
        }

        strm.close();
    }
    else
    {
        if (errno == EACCES)
            return  MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );
        else
            return  MicDeviceError::errorCode( MICSDKERR_PROPERTY_NOT_FOUND );
    }

    if (!success)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}


//----------------------------------------------------------------------------
/** @fn     bool  Mpss3StackLinux::loadLib()
 *  @return success state
 *
 *  Load shared library, resolve symbols and return success state.
 */

bool  Mpss3StackLinux::loadLib()
{
    if (!m_pData->mMpssConfigLib.isLoaded())
    {
        m_pData->mMpssConfigLib.setFileName( MPSSCONFIG_SHLIB_NAME );
        m_pData->mMpssConfigLib.setVersion( MPSSCONFIG_SHLIB_MAJOR );
        if (!m_pData->mMpssConfigLib.load())
            return  false;
    }

    /// \todo In case the LIBMPSSCONFIG team also comes to the conclusion that having a
    ///       library versioning scheme is a good thing, we will have to add code here
    ///       to check the compile library version against the runtime version.

    m_pData->mpMpssReadPm   = (PrivData::mpssconfig_read_pm) m_pData->mMpssConfigLib.lookup( MPSSCONFIG_FNAME_READ_PM );
    m_pData->mpMpssUpdatePm = (PrivData::mpssconfig_update_pm) m_pData->mMpssConfigLib.lookup( MPSSCONFIG_FNAME_UPDATE_PM );

    if (!m_pData->mpMpssReadPm || !m_pData->mpMpssUpdatePm)
        return  false;

    return  true;
}


//----------------------------------------------------------------------------
/** @fn     void  Mpss3StackLinux::unloadLib()
 *
 *  Unload the shared library.
 *
 *  This function will most likely not really unload the shared library, but
 *  only decrease the shared library's reference count.
 */

void  Mpss3StackLinux::unloadLib()
{
    if (m_pData->mMpssConfigLib.isLoaded())
        m_pData->mMpssConfigLib.unload();
}

