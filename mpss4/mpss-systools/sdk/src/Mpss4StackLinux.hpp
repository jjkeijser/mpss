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
#ifndef MICMGMT_MPSS4STACKLINUX_HPP
#define MICMGMT_MPSS4STACKLINUX_HPP

// SYSTEM INCLUDES
//
#include    <cstring>
#include    <map>
#include    <memory>
#include    <sstream>
#include    <string>

#include    <unistd.h>
#include    <sys/stat.h>
#include    <sys/types.h>

// PROJECT INCLUDES
//
#include    "DirentFunctions.hpp"
#include    "KnlCustomBootManager.hpp"
#include    "KnlPropKeys.hpp"
#include    "LoaderInterface.hpp"
#include    "MicDeviceError.hpp"
#include    "MicPciConfigInfo.hpp"
#include    "Mpss4StackBase.hpp"
#include    "MicLogger.hpp"
//
#include    "PciConfigData_p.hpp"


// COMMON FRAMEWORK INCLUDES
//
#include "micmgmtCommon.hpp"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif // UNIT_TESTS

// NAMESPACE
//
namespace  micmgmt
{

#ifdef UNIT_TESTS
class LibmpssconfigFunctions;
#endif // UNIT_TESTS


//============================================================================
//  CLASS:  Mpss4StackLinux

class  Mpss4StackLinux : public Mpss4StackBase
{
public:
    typedef uint32_t (*ParseFunctionT)( std::string*, const std::string&, const std::string& );
    typedef bool (*IsAdminT)();

public:

    explicit Mpss4StackLinux( int device=-1 );
#ifdef UNIT_TESTS
    explicit Mpss4StackLinux( int device, LibmpssconfigFunctions& functions );
#endif
   ~Mpss4StackLinux();

    std::string  mpssHomePath() const;
    std::string  mpssBootPath() const;
    std::string  mpssFlashPath() const;

    uint32_t     getSystemDeviceNumbers( std::vector<size_t>* list ) const;
    template     <class DirentFunctionsT>
    uint32_t     getSystemDeviceNumbers( std::vector<size_t>* list, DirentFunctionsT& dirent ) const;

    uint32_t     getSystemDriverVersion( std::string* version ) const;

    template     <class Class>
    uint32_t     getSystemDeviceType( int* type, size_t number, const Class& class_ ) const;
    uint32_t     getSystemDeviceType( int* type, size_t number ) const;

    template     <class Stream>
    uint32_t     getSystemProperty( std::string* data, const std::string& name,
                                    Stream& stream) const;
    uint32_t     getSystemProperty( std::string* data, const std::string& name ) const;

    template     <class Class>
    uint32_t     getDevicePciConfigData( PciConfigData* data, const Class& class_ ) const;
    uint32_t     getDevicePciConfigData( PciConfigData* data ) const;

    template     <class Stream,
                  ParseFunctionT ParseFunction>
    uint32_t     getDeviceProperty( std::string* data, const std::string& name,
                                    Stream& stream ) const;
    uint32_t     getDeviceProperty( std::string* data, const std::string& name ) const;

    uint32_t     getDeviceState( int* state ) const;

    uint32_t     deviceReset( bool force=false );

    template     <class BootInfo=MicBootConfigInfo,
                  class BootMgr=KnlCustomBootManager,
                  IsAdminT=&micmgmt::isAdministrator>
    uint32_t     deviceBoot( const BootInfo& info );
    uint32_t     deviceBoot( const MicBootConfigInfo& info );

    template     <class Stream=std::ofstream>
    uint32_t     setDeviceProperty( const std::string& name, const std::string& data );
    template     <class Stream>
    uint32_t     setDeviceProperty( const std::string& name, const std::string& data,
                        Stream& stream );

    uint32_t     setLoader( LoaderInterface* loader );


PRIVATE:

    template     <class Stream>
    uint32_t     getDeviceProperty( std::string* data, const std::string& name,
                                    const std::string& subkey, Stream& stream,
                                    char sep='=' ) const;

    uint32_t     getDeviceProperty( std::string* data, const std::string& name,
                                    const std::string& subkey, char sep='=' ) const;

    uint32_t     readPciConfigSpace( std::string* data, bool* hasAccess ) const;
    template     <class Stream>
    uint32_t     readPciConfigSpace( std::string* data, bool* hasAccess, Stream& stream ) const;

    bool         loadLib();
    void         unloadLib();


private:

    struct  PrivData;
    std::unique_ptr<PrivData>  m_pData;
    std::unique_ptr<LoaderInterface> m_pLoader;
    std::shared_ptr<CustomBootManager> m_customBootMgr;


private: // DISABLE

    Mpss4StackLinux( const Mpss4StackLinux& );
    Mpss4StackLinux&  operator = ( const Mpss4StackLinux& );

};





//============================================================================
//  BEGIN IMPLEMENTATION OF TEMPLATE METHODS
//============================================================================



// PRIVATE DATA
//
struct  Mpss4StackLinux::PrivData
{
    typedef int   (*boot_linux)( int node );
    typedef int   (*boot_media)( int node, const char* path );
    typedef char*  (*get_state)( int node );
    typedef int   (*reset_node)( int node );

    boot_linux     mpBootLinux;     // Function pointer
    boot_media     mpBootMedia;     // Ditto
    get_state      mpGetState;      // Ditto
    reset_node     mpReset;         // Ditto
};

// Constants for Linux MPSS 4
//
namespace mpss4linux_detail
{
const char* const  MPSS_DEFAULT_BOOT_DIR           = "boot";
const char* const  MPSS_DEFAULT_FLASH_DIR          = "flash";
const char* const  MPSSLIB_SHLIB_NAME              = "libmpssconfig";
const char* const  MPSSLIB_FNAME_BOOT_LINUX        = "boot_linux";
const char* const  MPSSLIB_FNAME_BOOT_MEDIA        = "boot_media";
const char* const  MPSSLIB_FNAME_GET_STATE         = "get_state";
const char* const  MPSSLIB_FNAME_RESET             = "reset_node";
const char* const  NUMERIC_CHARACTERS              = "1234567890";
const char* const  SYSFS_MIC_MODULE_PATH           = "/sys/module/mic_x200";
const char* const  FIRMWARE_PATH                   = "/lib/firmware/";
const char* const  SYSFS_MIC_CLASS_PATH            = "/sys/class/mic";
const char* const  SYSFS_MIC_DMI_FOLDER            = "device/dmi";
const char* const  SYSFS_MIC_SPAD_FOLDER           = "device/spad";
const char* const  SYSFS_MIC_DEVICE_MODE           = "bootmode";
const char* const  PROPVAL_DEVICE_TYPE_KNL         = "x200";
const char* const  PROPVAL_PCI_DEVICE              = "0x226";
const char* const  PROPVAL_PCI_VENDOR              = "0x8086";
const char* const  PROPVAL_DEVICE_STATE_ONLINE     = "online";
const char* const  PROPVAL_DEVICE_STATE_READY      = "ready";
const char* const  PROPVAL_DEVICE_STATE_BOOTING    = "booting";
const char* const  PROPVAL_DEVICE_STATE_RESETTING  = "resetting";
const char* const  PROPVAL_DEVICE_STATE_BOOTING_FW = "booting_firmware";
const char* const  PROPVAL_DEVICE_STATE_ONLINE_FW  = "online_firmware";
const char* const  PROPVAL_DEVICE_STATE_UNKNOWN    = "unknown";
const char* const  EMPTY_KEYWORD                   = "<empty>";

//
const size_t       PCI_VENDOR_ID_OFFSET          = 0;
const size_t       PCI_DEVICE_ID_OFFSET          = 2;
const size_t       PCI_REVISION_ID_OFFSET        = 8;
const size_t       PCI_SUBSYSTEM_ID_OFFSET       = 0x2e;
const size_t       PCI_CTRL_CAP_OFFSET           = 0x70;
const size_t       PCI_LINK_CTRL_OFFSET          = 0x78;
const size_t       PCI_LINK_STATS_OFFSET         = 0x7a;
const size_t       PCI_NOACCESS_LEN              = 0x40;

// PROPKEYs to sysfs path
const std::map<std::string, std::string> propKeyToPath = {
    { PROPKEY_DEVICE_TYPE,          "info/mic_family" },
    { PROPKEY_PROC_MODEL,           "info/processor_id_decoded" },
    { PROPKEY_PROC_TYPE,            "info/processor_id_decoded" },
    { PROPKEY_PROC_FAMILY,          "info/processor_id_decoded" },
    { PROPKEY_PROC_STEPPING,        "info/processor_id_decoded" },
    { PROPKEY_BOARD_STEPPING,       "info/processor_stepping" },
    { PROPKEY_BIOS_VERSION,         "info/bios_version" },
    { PROPKEY_BIOS_RELDATE,         "info/bios_release_date" },
    { PROPKEY_OEM_STRINGS,          "info/oem_strings" },
    { PROPKEY_SYS_SKU,              "info/system_sku_number" },
    { PROPKEY_SYS_UUID,             "info/system_uuid" },
    { PROPKEY_SYS_SERIAL,           "info/system_serial_number" },
    { PROPKEY_CPU_VENDOR,           "info/processor_manufacturer" },
    { PROPKEY_CPU_PARTNUMBER,       "info/processor_part_number" },
    { PROPKEY_POST_CODE,            "spad/post_code" },
    { PROPKEY_BOARD_TYPE,           "info/baseboard_type" },
    { PROPKEY_SMC_VERSION,          "info/bios_smc_version" },
    { PROPKEY_ME_VERSION,           "info/bios_me_version" },
    { PROPKEY_NTB_VERSION,          "info/ntb_eeprom_version" },
    { PROPKEY_FAB_VERSION,          "info/fab_version" },
    { PROPKEY_MCDRAM_VERSION,       "info/mcdram_version" },
    { PROPKEY_PCI_DEVICE,           "device/device"},
    { PROPKEY_PCI_VENDOR,           "device/vendor"}
};


} // mpss4linux_detail


//----------------------------------------------------------------------------
/** @fn     uint32_t  Mpss4StackLinux::setDeviceProperty( const std::string& name, const std::string& data )
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

// Stream defaults to std::ostream
template <class Stream>
uint32_t Mpss4StackLinux::setDeviceProperty( const std::string& name, const std::string& data )
{
    Stream stream;
    return setDeviceProperty<Stream>( name, data, stream );
}

template <class Stream>
uint32_t  Mpss4StackLinux::setDeviceProperty( const std::string& name, const std::string& data,
                Stream& strm)
{
    if (name.empty() || data.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (deviceNumber() < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );

    std::string  path = mpss4linux_detail::SYSFS_MIC_CLASS_PATH;
    path += '/' + deviceName();
    path += '/' + name;

    bool  success = false;
    int   errorno = 0;

    strm.open( path );
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


template <class DirentFunctionsT>
uint32_t Mpss4StackLinux::getSystemDeviceNumbers( std::vector<size_t>* list, DirentFunctionsT& dirent ) const
{
    if (!list)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    list->clear();

    DIR*  dir = dirent.opendir( mpss4linux_detail::SYSFS_MIC_MODULE_PATH );
    if (dir == NULL)
        return  MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_LOADED );
    else
        dirent.closedir( dir );

    dir = dirent.opendir( mpss4linux_detail::SYSFS_MIC_CLASS_PATH );
    if (dir == NULL)
        return  MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_INITIALIZED );

    std::list<size_t>  devices;

    struct dirent*  entry = NULL;
    while ((entry = dirent.readdir( dir )))
    {
        if(!entry || !entry->d_name)
            continue;

        std::string  filename = entry->d_name;
        if (filename.find( deviceBaseName() ) != 0)
            continue;   // mic*

        std::string::size_type  numpos = deviceBaseName().length();
        if (filename.length() == numpos)
            continue;   // mic

        std::string::size_type  numend = filename.find_first_not_of( "0123456789", numpos );
        if (numend == numpos)
            continue;   // mice or similar

        std::stringstream  strm( filename.substr( numpos, numend-numpos ) );
        unsigned int  micnum;
        strm >> micnum;
        if (!strm)
            continue;   // not a number

        devices.push_back( micnum );
    }

    dirent.closedir( dir );

    if (!devices.empty())
    {
        devices.sort();
        list->resize( devices.size() );
        std::copy( devices.begin(), devices.end(), list->begin() );
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );

}

template <class Stream,
          Mpss4StackLinux::ParseFunctionT ParseFunction>
uint32_t Mpss4StackLinux::getDeviceProperty( std::string* data, const std::string& name,
                                    Stream& strm ) const
{
    if (!data || name.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (deviceNumber() < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );

    data->clear();

    std::string path = mpss4linux_detail::SYSFS_MIC_CLASS_PATH;
    path += "/" + deviceName() + "/";

    // Handle special cases
    if ( name.find( "spad" ) == 0 )
        path += std::string( PROPKEY_SPAD_BASE ) +
                "/" + name;
    else
    {
        auto value    = mpss4linux_detail::propKeyToPath.find( name );
        auto notFound = mpss4linux_detail::propKeyToPath.end();
        if ( value == notFound )
            return MICSDKERR_PROPERTY_NOT_FOUND;

        path += value->second;
    }

    strm.open( path );
    if (!strm.is_open())
    {
        switch(errno)
        {
            case EACCES:
                return  MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );
            case ENOENT:
                return  MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_LOADED );
            default:
                return  MicDeviceError::errorCode( MICSDKERR_PROPERTY_NOT_FOUND );
        }
    }

    const size_t size = 80;
    char buf[size];
    bzero(buf, size);
    strm.getline( buf, size );
    *data = buf;

    if (strm.fail())
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    strm.close();

    // Special parsing requirements for these cases
    if (name == PROPKEY_PROC_MODEL  ||
        name == PROPKEY_PROC_TYPE   ||
        name == PROPKEY_PROC_FAMILY ||
        name == PROPKEY_PROC_STEPPING )
    {
        std::string  valstr = *data;     // In this case we need to return other data
        data->clear();              // Clean slate
        std::string internalName;

        if( name == PROPKEY_PROC_MODEL)
            internalName = "Model";
        else if( name == PROPKEY_PROC_TYPE)
            internalName = "Type";
        else if( name == PROPKEY_PROC_FAMILY)
            internalName = "Family";
        else if( name == PROPKEY_PROC_STEPPING)
            internalName = "Stepping";
        else
            return  MicDeviceError::errorCode( MICSDKERR_PROPERTY_NOT_FOUND );

        uint32_t result = ParseFunction( data, valstr, internalName );
        if (MicDeviceError::isError( result ))
            return  result;
    }
    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

template <class Stream>
uint32_t Mpss4StackLinux::getSystemProperty( std::string* data, const std::string& name,
                                Stream& strm) const
{
    if (!data || name.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    data->clear();

    std::string  path = mpss4linux_detail::SYSFS_MIC_CLASS_PATH;
    path += '/' + name;

    bool  success = false;
    const size_t size = 80;
    char buf[size];
    bzero(buf, size);

    strm.open( path );
    if (strm.is_open())
    {
        strm.getline(buf, size);
        if( (success = strm.fail() ? false : true) )
            *data = buf;
        strm.close();
    }

    if (!success)
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

template <class Class>
uint32_t Mpss4StackLinux::getDevicePciConfigData( PciConfigData* data, const Class& class_ ) const
{
    if (!data)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    data->clear();      // Clear existing data

    std::string    propval;
    uint32_t  result = class_.getDeviceProperty( &propval, "device/uevent", "PCI_CLASS" );
    if (MicDeviceError::isError( result ))
        return  result;

    std::stringstream  strm;
    strm << std::hex << propval;
    strm >> data->mClassCode;
    propval.clear();

    result = class_.getDeviceProperty( &propval, "device/uevent", "PCI_SLOT_NAME" );
    if (MicDeviceError::isError( result ))
        return  result;

    data->mPciAddress = propval;
    propval.clear();

    bool hasAccess = false;
    result = class_.readPciConfigSpace( &propval, &hasAccess );
    if (MicDeviceError::isError( result ))
        return  result;

    // Extra device info from binary data
    data->mVendorId    = *(reinterpret_cast<const uint16_t*>(
                &propval.c_str()[mpss4linux_detail::PCI_VENDOR_ID_OFFSET]));
    data->mDeviceId    = *(reinterpret_cast<const uint16_t*>(
                &propval.c_str()[mpss4linux_detail::PCI_DEVICE_ID_OFFSET]));
    data->mRevisionId  = *(reinterpret_cast<const uint8_t*>(
                &propval.c_str()[mpss4linux_detail::PCI_REVISION_ID_OFFSET]));
    data->mSubsystemId = *(reinterpret_cast<const uint16_t*>(
                &propval.c_str()[mpss4linux_detail::PCI_SUBSYSTEM_ID_OFFSET]));

    // Check how much we were able to read from device/config to determine mHasAccess
    if(!hasAccess)
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
    if (propval.length() >= mpss4linux_detail::PCI_LINK_STATS_OFFSET + 2)
    {
        linkstat = *(reinterpret_cast<const uint16_t*>(
                    &propval.c_str()[mpss4linux_detail::PCI_LINK_STATS_OFFSET] ));

        switch ((linkstat >> 0) & 0x0f)
        {
          case  1:
            data->mLinkSpeed = MicSpeed( 2500, MicSpeed::eMega );
            break;

          case  2:
            data->mLinkSpeed = MicSpeed( 5000, MicSpeed::eMega );
            break;

          case  3:
            data->mLinkSpeed = MicSpeed( 8000, MicSpeed::eMega );
            break;

          default:
            data->mLinkSpeed = MicSpeed();
            break;
        }

        data->mLinkWidth = ((linkstat >> 4) & 0x3f);
    }

    // Payload + Request Size (if available)
    if (propval.length() >= mpss4linux_detail::PCI_CTRL_CAP_OFFSET + 2)
    {
        ctrlcap = *(reinterpret_cast<const uint16_t*>(
                    &propval.c_str()[mpss4linux_detail::PCI_CTRL_CAP_OFFSET] ));

        data->mPayloadSize  = 128 * (1 << ((ctrlcap & 0x00e0) >>  5));
        data->mRequestSize  = 128 * (1 << ((ctrlcap & 0x7000) >> 12));
        data->mRelaxedOrder = ((ctrlcap & 0x0010) >>  4);
        data->mExtTagEnable = ((ctrlcap & 0x0100) >>  8);
        data->mNoSnoop      = ((ctrlcap & 0x0800) >> 11);

        linkctrl = *(reinterpret_cast<const uint16_t*>(
                    &propval.c_str()[mpss4linux_detail::PCI_LINK_CTRL_OFFSET] ));

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

template <class Stream>
uint32_t Mpss4StackLinux::getDeviceProperty( std::string* data, const std::string& name,
        const std::string& subkey, Stream& strm, char sep ) const
{
    if (!data || name.empty())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (deviceNumber() < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );

    if (subkey.empty())
        return  getDeviceProperty( data, name );

    data->clear();

    std::string  path = mpss4linux_detail::SYSFS_MIC_CLASS_PATH;
    path += '/' + deviceName();
    path += '/' + name;

    bool  success = false;
    const size_t size = 80;
    char buf[size];

    strm.open(path);
    if (strm.is_open())
    {
        std::string  line;
        while (strm.good())
        {
            bzero( buf, size );
            strm.getline( buf, size );
            line = buf;
            if (line.find( subkey ) != std::string::npos)    // Find line with subkey
            {
                std::string::size_type  valpos = line.find( sep );
                if ((valpos != std::string::npos) && (++valpos != std::string::npos))
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

template <class Stream>
uint32_t Mpss4StackLinux::readPciConfigSpace( std::string* data, bool* hasAccess, Stream& stream) const
{
    if (!data || !hasAccess)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (deviceNumber() < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );

    data->clear();

    std::string  path = mpss4linux_detail::SYSFS_MIC_CLASS_PATH;
    path += '/' + deviceName();
    path += "/device/config";

    stream.open(path, std::ios::binary);
    if(!stream.good())
        return MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    std::stringstream sStream;
    sStream << stream.rdbuf();

    data->assign(sStream.str());

    *hasAccess = true;
    if( data->size() <= mpss4linux_detail::PCI_NOACCESS_LEN )
        *hasAccess = false;

    return MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

template <class BootInfo, class BootMgr, Mpss4StackLinux::IsAdminT isAdmin>
uint32_t Mpss4StackLinux::deviceBoot( const BootInfo& info)
{
    if (!info.isValid())
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    if (deviceNumber() < 0)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_DEVICE_NUMBER );

    if (!m_pData->mpBootLinux || !m_pData->mpBootMedia)
        return  MicDeviceError::errorCode( MICSDKERR_SHARED_LIBRARY_ERROR );

    if (!isAdmin())
        return MicDeviceError::errorCode( MICSDKERR_NO_ACCESS );

    if (info.isCustom())
    {
        m_customBootMgr = BootMgr::create( deviceName(), info.imagePath() );
        auto status = m_pData->mpBootMedia( deviceNumber(),
                                            m_customBootMgr->fwRelPath().c_str() );
        if ( MicDeviceError::isError( status ) )
        {
            LOG( ERROR_MSG, "libmpssconfig::boot_media() failed: %d", status );
            return MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
        }
        return MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    }

    // else not custom
    auto status = m_pData->mpBootLinux( deviceNumber() );
    if ( MicDeviceError::isError( status ))
    {
        LOG( ERROR_MSG, "libmpssconfig::boot_linux() failed: %d", status );
        return  MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    }

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

template <class Class>
uint32_t Mpss4StackLinux::getSystemDeviceType( int* type, size_t number, const Class& class_ ) const
{
    if (!type)
        return  MicDeviceError::errorCode( MICSDKERR_INVALID_ARG );

    std::stringstream  strm;
    std::stringstream  strmDevice;
    std::stringstream  strmVendor;
    strm << class_.deviceBaseName() << number;
    strmDevice << strm.str();
    strmVendor << strm.str();

    // Get actual path to device type
    auto path = mpss4linux_detail::propKeyToPath.find( PROPKEY_DEVICE_TYPE );
    if ( path == mpss4linux_detail::propKeyToPath.end() )
        // INTERNAL_ERROR, this key should *really* be there...
        return MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );

    strm << '/' << path->second;

    std::string    propval;
    uint32_t  result = class_.getSystemProperty( &propval, strm.str() );
    if (MicDeviceError::isError( result ))
    {
        *type = eTypeUnknown;
        return  result;
    }

    if (propval == mpss4linux_detail::PROPVAL_DEVICE_TYPE_KNL)
        *type = eTypeKnl;
    else if (propval == mpss4linux_detail::EMPTY_KEYWORD)
    {
        // Path is not working, checking the pci values
        auto pathDevice = mpss4linux_detail::propKeyToPath.find ( PROPKEY_PCI_DEVICE );
        if ( pathDevice == mpss4linux_detail::propKeyToPath.end() )
           return MicDeviceError::errorCode ( MICSDKERR_INTERNAL_ERROR );

        auto pathVendor = mpss4linux_detail::propKeyToPath.find ( PROPKEY_PCI_VENDOR );
        if ( pathVendor == mpss4linux_detail::propKeyToPath.end() )
           return MicDeviceError::errorCode ( MICSDKERR_INTERNAL_ERROR );

        strmDevice << '/' << pathDevice->second;
        strmVendor << '/' << pathVendor->second;

        std::string deviceVal, vendorVal;

        uint32_t resultDevice = class_.getSystemProperty( &deviceVal, strmDevice.str());
        if (MicDeviceError::isError( resultDevice ))
        {
            *type = eTypeUnknown;
            return resultDevice;
        }

        uint32_t resultVendor = class_.getSystemProperty( &vendorVal, strmVendor.str());
        if (MicDeviceError::isError( resultVendor ))
        {
            *type = eTypeUnknown;
            return resultVendor;
        }

        if (deviceVal.find(mpss4linux_detail::PROPVAL_PCI_DEVICE) != std::string::npos &&
            vendorVal == mpss4linux_detail::PROPVAL_PCI_VENDOR)
        {
            *type = eTypeKnl;
        }
        else
        {
            *type = eTypeUnknown;
        }

        return MicDeviceError::errorCode ( MICSDKERR_INVALID_CONFIGURATION);

    }
    else
        *type = eTypeUnknown;

    return  MicDeviceError::errorCode( MICSDKERR_SUCCESS );
}

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MPSS4STACKLINUX_HPP
