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
#include    "FlashDeviceInfo.hpp"

// LOCAL CONSTANTS
//
namespace  {
const int  FLASH_DEVICE_BRAND_ATMEL    = 0x1f;
const int  FLASH_DEVICE_BRAND_MACRONIX = 0xc2;
const int  FLASH_DEVICE_BRAND_WINBOND  = 0xef;
const int  FLASH_FAMILY_ATMEL_AT26DF   = 0x45;
const int  FLASH_FAMILY_ATMEL_AT25DF   = 0x46;
const int  FLASH_FAMILY_MACRONIX_MX25  = 0x20;
const int  FLASH_FAMILY_WINBOND_W25X   = 0x30;
const int  FLASH_FAMILY_WINBOND_W25Q   = 0x40;
const int  FLASH_DEVICE_MODEL_081      = 0x01;
const int  FLASH_DEVICE_MODEL_161      = 0x02;
const int  FLASH_DEVICE_MODEL_L8005D   = 0x14;
const int  FLASH_DEVICE_MODEL_L1605D   = 0x15;
const int  FLASH_DEVICE_MODEL_L3205D   = 0x16;
const int  FLASH_DEVICE_MODEL_L6405D   = 0x17;
const int  FLASH_DEVICE_MODEL_80       = 0x14;
const int  FLASH_DEVICE_MODEL_16       = 0x15;
const int  FLASH_DEVICE_MODEL_32       = 0x16;
const int  FLASH_DEVICE_MODEL_64       = 0x17;
}

// NAMESPACE
//
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @class    micmgmt::FlashDeviceInfo   FlashDeviceInfo.hpp
 *  @ingroup  sdk
 *  @brief    This class encapsulates flash device information
 *
 *  The \b %FlashDeviceInfo class encapsulates flash device information and
 *  provides accessors to the relevant information.
 *
 *  Note that this class only knows about flash devices used on KNC.
 */


//============================================================================
//  P U B L I C   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     FlashDeviceInfo::FlashDeviceInfo( uint32_t data )
 *  @param  data    Raw flash device data
 *
 *  Construct a flash device info object based on specified raw \a data.
 *
 *  The raw data is expected to contains three byte groups, one byte is for
 *  the manufacturer ID, one byte for the device family ID, and one byte for
 *  the flash device model ID.
 */

FlashDeviceInfo::FlashDeviceInfo( uint32_t data ) :
    m_data( data )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     FlashDeviceInfo::FlashDeviceInfo( const FlashDeviceInfo& that )
 *  @param  that    Other flash device info object
 *
 *  Construct a flash device info object as deep copy of \a that info object.
 */

FlashDeviceInfo::FlashDeviceInfo( const FlashDeviceInfo& that ) :
    m_data( that.m_data )
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     FlashDeviceInfo::~FlashDeviceInfo()
 *
 *  Cleanup.
 */

FlashDeviceInfo::~FlashDeviceInfo()
{
    // Nothing to do
}


//----------------------------------------------------------------------------
/** @fn     FlashDeviceInfo&  FlashDeviceInfo::operator = ( uint32_t data )
 *  @param  data    Device info data
 *  @return reference to this object
 *
 *  Assign specified device \a data to this object and return a reference to
 *  the updated object.
 */

FlashDeviceInfo&  FlashDeviceInfo::operator = ( uint32_t data )
{
    m_data = data;

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     FlashDeviceInfo&  FlashDeviceInfo::operator = ( const FlashDeviceInfo& that )
 *  @param  that    Other info object
 *  @return reference to this object
 *
 *  Assign \a that object to this and return a reference to this object.
 */

FlashDeviceInfo&  FlashDeviceInfo::operator = ( const FlashDeviceInfo& that )
{
    if (&that != this)
        m_data = that.m_data;

    return  *this;
}


//----------------------------------------------------------------------------
/** @fn     bool  FlashDeviceInfo::isValid() const
 *  @return valid state
 *
 *  Returns \c true if the flash device info is valid.
 */

bool  FlashDeviceInfo::isValid() const
{
    if (manufacturerId() < 0)
        return  false;

    if (familyId() < 0)
        return  false;

    if (modelId() < 0)
        return  false;

    return  true;
}


//----------------------------------------------------------------------------
/** @fn     int  FlashDeviceInfo::manufacturerId() const
 *  @return manufacturer ID
 *
 *  Returns the manufacturer ID or -1 if no valid manufacturer ID could be
 *  extracted from the registered device data.
 */

int  FlashDeviceInfo::manufacturerId() const
{
    int  id = m_data & 0xff;
    switch (id)
    {
      case  FLASH_DEVICE_BRAND_ATMEL:
      case  FLASH_DEVICE_BRAND_MACRONIX:
      case  FLASH_DEVICE_BRAND_WINBOND:
        return  id;

      default:
        break;
    }

    return  -1;
}


//----------------------------------------------------------------------------
/** @fn     int  FlashDeviceInfo::familyId() const
 *  @return family ID
 *
 *  Returns the flash device family ID or -1 if no valid family ID could be
 *  extracted from the registered device data.
 */

int  FlashDeviceInfo::familyId() const
{
    int  id = (m_data >> 8) & 0xff;
    switch (id)
    {
      case  FLASH_FAMILY_ATMEL_AT26DF:
      case  FLASH_FAMILY_ATMEL_AT25DF:
      case  FLASH_FAMILY_MACRONIX_MX25:
      case  FLASH_FAMILY_WINBOND_W25X:
      case  FLASH_FAMILY_WINBOND_W25Q:
        return  id;

      default:
        break;
      ;
    }

    return  -1;
}


//----------------------------------------------------------------------------
/** @fn     int  FlashDeviceInfo::modelId() const
 *  @return model ID
 *
 *  Returns the flash device model ID or -1 if no valid model ID could be
 *  extracted from the registered device data.
 */

int  FlashDeviceInfo::modelId() const
{
    int  id = (m_data >> 16) & 0xff;
    switch (id)
    {
      case  0x01:
      case  0x02:
      case  0x14:
      case  0x15:
      case  0x16:
      case  0x17:
        return  id;

      default:
        break;
    }

    return  -1;
}


//----------------------------------------------------------------------------
/** @fn     std::string  FlashDeviceInfo::manufacturer() const
 *  @return name string
 *
 *  Returns the flash device manufacturer's name.
 */

std::string  FlashDeviceInfo::manufacturer() const
{
    switch (manufacturerId())
    {
      case  FLASH_DEVICE_BRAND_ATMEL:
        return  "Atmel";

      case  FLASH_DEVICE_BRAND_MACRONIX:
        return  "Macronix";

      case  FLASH_DEVICE_BRAND_WINBOND:
        return  "Winbond";

      default:
        break;
    }

    return  "?";
}


//----------------------------------------------------------------------------
/** @fn     std::string  FlashDeviceInfo::family() const
 *  @return name string
 *
 *  Returns the flash device family's name.
 */

std::string  FlashDeviceInfo::family() const
{
    if (manufacturerId() == FLASH_DEVICE_BRAND_ATMEL)
    {
        switch (familyId())
        {
          case  FLASH_FAMILY_ATMEL_AT26DF:
            return  "AT26DF";

          case  FLASH_FAMILY_ATMEL_AT25DF:
            return  "AT25DF";

          default:
            break;
        }
    }
    else if (manufacturerId() == FLASH_DEVICE_BRAND_MACRONIX)
    {
        switch (familyId())
        {
          case  FLASH_FAMILY_MACRONIX_MX25:
            return  "MX25";

          default:
            break;
        }
    }
    else if (manufacturerId() == FLASH_DEVICE_BRAND_WINBOND)
    {
        switch (familyId())
        {
          case  FLASH_FAMILY_WINBOND_W25X:
            return  "W25X";

          case  FLASH_FAMILY_WINBOND_W25Q:
            return  "W25Q";

          default:
            break;
        }
    }

    return  "?";
}


//----------------------------------------------------------------------------
/** @fn     std::string  FlashDeviceInfo::model() const
 *  @return model string
 *
 *  Returns the model name as string.
 */

std::string  FlashDeviceInfo::model() const
{
    if (manufacturerId() == FLASH_DEVICE_BRAND_ATMEL)
    {
        switch (modelId())
        {
          case  FLASH_DEVICE_MODEL_081:
            return  "081";

          case  FLASH_DEVICE_MODEL_161:
            return  "161";

          default:
            return  "xxx";
        }
    }
    else if (manufacturerId() == FLASH_DEVICE_BRAND_MACRONIX)
    {
        switch (modelId())
        {
          case  FLASH_DEVICE_MODEL_L8005D:
            return  "L8005D";

          case  FLASH_DEVICE_MODEL_L1605D:
            return  "L1605D";

          case  FLASH_DEVICE_MODEL_L3205D:
            return  "L3205D";

          case  FLASH_DEVICE_MODEL_L6405D:
            return  "L6405D";

          default:
            return  "xxxxxx";
        }
    }
    else if (manufacturerId() == FLASH_DEVICE_BRAND_WINBOND)
    {
        switch (modelId())
        {
          case  FLASH_DEVICE_MODEL_80:
            return  "80";

          case  FLASH_DEVICE_MODEL_16:
            return  "16";

          case  FLASH_DEVICE_MODEL_32:
            return  "32";

          case  FLASH_DEVICE_MODEL_64:
            return  "64";

          default:
            return  "xx";
        }
    }

    return  "?";
}


//----------------------------------------------------------------------------
/** @fn     std::string  FlashDeviceInfo::info() const
 *  @return device info
 *
 *  Returns the complete flash device info as string. This is a concatination
 *  of manufacturer name, family name, and model.
 */

std::string  FlashDeviceInfo::info() const
{
    if (isValid())
        return  string( manufacturer() + " " + family() + model() );

    return  "<unknown> <unknown>";  // As was reported in the past
}


//----------------------------------------------------------------------------
/** @fn     void  FlashDeviceInfo::clear()
 *
 *  Clear flash device info.
 */

void  FlashDeviceInfo::clear()
{
    m_data = 0;
}


