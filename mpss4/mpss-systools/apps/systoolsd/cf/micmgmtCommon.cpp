/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/


// PROJECT INCLUDES
//
#include    "micmgmtCommon.hpp"

// SYSTEM INCLUDES
//
#include <string>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <cstdlib>
#include <locale>
#include <utility>
#include <csignal>
#ifdef _WIN32
#include <windows.h>
#include <VersionHelpers.h> // new version APIs
#include <lmcons.h>
#include <Shlobj.h>     // home path code
#include <cstdio>
#include <direct.h>
#endif
#ifdef __linux__
#include "ToolSettings.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <pwd.h>
#include <cstdlib>
#include <fstream>
#endif

// LOCAL CONSTANTS
//
namespace
{
    const char* const    INTEL_COPYRIGHT_ASCII = "Copyright (C) 2017, Intel Corporation.";
    const char* const    INTEL_COPYRIGHT_UTF8  = "Copyright \xc2\xa9 2017, Intel Corporation.";
    const char* const    MIC_DEVICE_NAME       = "mic";
    const char* const    INTEL_MPSS_HOME       = "INTEL_MPSS_HOME";
#ifdef __linux__
    const char* const    LINUX_HOME_PATH       = "/usr/share/mpss";
    const char* const    PROC_HOST_VERSION     = "/proc/version";
    const char* const    PROC_MEMORY_INFO      = "/proc/meminfo";
    const char* const    PROC_PHYS_MEMORY      = "MemTotal";
    const unsigned long  BYTES_PER_KB          = 1024;
    const unsigned long  BYTES_PER_MB          = 1024 * BYTES_PER_KB;
    const unsigned long  BYTES_PER_GB          = 1024 * BYTES_PER_MB;
#endif

    micmgmt::trapSignalHandler gTrapHandler = NULL;
    size_t                     gTrapCount   = 0;

#ifdef _WIN32
    BOOL WINAPI signalHandler(DWORD /*dwCtrlType*/)
    {
        if (gTrapHandler != NULL)
        {
            gTrapHandler();
        }
        return TRUE;
    }
#else
    void signalHandler(int /*sigInt*/)
    {
        if (gTrapHandler != NULL)
        {
            gTrapHandler();
        }
    }
#endif

    bool matchWithMask(const std::string& file, const std::string& mask)
    {
        // split on "*" then check each part.  Match if all marts match.
        std::vector<std::string> parts;
        size_t star = mask.find('*');
        size_t last = 0;
        while (star != std::string::npos)
        {
            size_t start = star + 1;
            size_t end = mask.find('*', start);
            if (end != std::string::npos)
            {
                --end;
            }
            std::string p = mask.substr(start, (end - start + 1));
            last = start;
            if (p.empty() == false)
            {
                parts.push_back(p);
            }
            star = mask.find('*', last);
        }
        size_t count = 0;
        star = 0;
        for (auto it = parts.begin(); it != parts.end(); ++it)
        {
            std::string f = *it;
            if ((star = file.find(f, star)) != std::string::npos)
            {
                ++count;
                ++star;
            }
            else
            {
                break;
            }
        }
        return (count == parts.size());
    } // matchWithMask
} // empty namespace

// NAMESPACES
using namespace  micmgmt;
using namespace  std;


//============================================================================
/** @namespace  micmgmt
 *  @ingroup    common
 *  @brief  Namespace wide functions
 *
 *  The \b %micmgmt namespace provides a set of namespace wide utility
 *  functions in addition to a variety of frame work classes for %micmgmt.
 *
 *  All available functions in the %micmgmt namespace are platform independent.
 */


//============================================================================
//  N A M E S P A C E   W I D E   F U N C T I O N   I N T E R F A C E
//============================================================================

//----------------------------------------------------------------------------
/** @fn     bool  micmgmt::isAdministrator()
 *  @return administrator state
 *
 *  Returns \a true if the user has root or administrator rights.
 */

bool  micmgmt::isAdministrator()
{
#ifdef __linux__

    return  geteuid() ? false : true;

#else

    bool                  result   = false;
    HANDLE                pToken   = NULL;
    PACL                  pAcl     = NULL;
    PSID                  pAdmin   = NULL;
    HANDLE                pHandle  = NULL;
    PSECURITY_DESCRIPTOR  pSecDesc = NULL;

    for (;;)       // Fake loop
    {
        if (!OpenThreadToken( GetCurrentThread(), TOKEN_DUPLICATE | TOKEN_QUERY, TRUE, &pToken ))
        {
            if (GetLastError() != ERROR_NO_TOKEN)
                break;

            if (!OpenProcessToken( GetCurrentProcess(), TOKEN_DUPLICATE|TOKEN_QUERY, &pToken ))
                break;
        }

        if (!DuplicateToken( pToken, SecurityImpersonation, &pHandle ))
            break;


        SID_IDENTIFIER_AUTHORITY sid = SECURITY_NT_AUTHORITY;
        if (!AllocateAndInitializeSid( &sid, 2,
                                       SECURITY_BUILTIN_DOMAIN_RID,
                                       DOMAIN_ALIAS_RID_ADMINS,
                                       0, 0, 0, 0, 0, 0, &pAdmin ))
            break;

        pSecDesc = LocalAlloc( LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH );
        if (pSecDesc == NULL)
            break;

        if (!InitializeSecurityDescriptor( pSecDesc, SECURITY_DESCRIPTOR_REVISION ))
            break;

        DWORD  dwACLSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(pAdmin) - sizeof(DWORD);
        pAcl = (PACL) LocalAlloc( LPTR, dwACLSize );
        if (pAcl == NULL)
            break;

        if (!InitializeAcl( pAcl, dwACLSize, ACL_REVISION2 ))
            break;

        const DWORD  ACCESS_READ  = 1;
        const DWORD  ACCESS_WRITE = 2;
        DWORD  dwMask = ACCESS_READ | ACCESS_WRITE;

        if (!AddAccessAllowedAce( pAcl, ACL_REVISION2, dwMask, pAdmin ))
            break;

        if (!SetSecurityDescriptorDacl( pSecDesc, TRUE, pAcl, FALSE ))
            break;

        SetSecurityDescriptorGroup( pSecDesc, pAdmin, FALSE );
        SetSecurityDescriptorOwner( pSecDesc, pAdmin, FALSE );

        if (!IsValidSecurityDescriptor( pSecDesc ))
            break;

        DWORD  dwAccess = ACCESS_READ;
        DWORD  dwStatus = 0;
        DWORD  dwSetSize = sizeof(PRIVILEGE_SET);
        PRIVILEGE_SET  ps;

        GENERIC_MAPPING  mapping;
        mapping.GenericRead    = ACCESS_READ;
        mapping.GenericWrite   = ACCESS_WRITE;
        mapping.GenericExecute = 0;
        mapping.GenericAll     = ACCESS_READ | ACCESS_WRITE;

        BOOL  check = FALSE;
        if (!AccessCheck( pSecDesc, pHandle, dwAccess, &mapping,
                          &ps, &dwSetSize, &dwStatus, &check))
        {
            result = false;
        }
        else
        {
            result = check ? true : false;
        }

        break;    // Don't loop
    }

    {
        if (pAcl)
            LocalFree( pAcl );
        if (pSecDesc)
            LocalFree( pSecDesc );
        if (pAdmin)
            FreeSid( pAdmin );
        if (pHandle)
            CloseHandle( pHandle );
        if (pToken)
            CloseHandle( pToken );
    }

    return  result;

#endif
}


//----------------------------------------------------------------------------
/** @fn     std::string  micmgmt::filePathSeparator()
 *  @return file path separator
 *
 *  Returns a platform independent file path separator.
 */

std::string  micmgmt::filePathSeparator()
{
    string  separator;

#ifdef __linux__
    separator = "/";
#else
    separator = "\\";
#endif

    return  separator;
}


//----------------------------------------------------------------------------
/** @fn     std::string  micmgmt::fileNameFromPath( const std::string& path )
 *  @param  path    Complete file path
 *  @return file name component
 *
 *  Strip any possible path information from specified file \a path and
 *  return only the file name component.
 */

std::string  micmgmt::fileNameFromPath( const std::string& path )
{
    // Return path if empty or has no path separators

    size_t  sep = path.rfind( filePathSeparator() );
    if (sep == string::npos)
        return  path;

    // Be sure that we have a potential file name
    if (++sep < path.length())
        return  path.substr( sep );

    // Return an empty string for all other cases
    return  "";
}


//----------------------------------------------------------------------------
/** @fn     std::string  micmgmt::userHomePath()
 *  @return user home path
 *
 *  Returns the user's home directory path.
 */

std::string  micmgmt::userHomePath()
{
    string  path;

#ifdef  __linux__

    char*  home = getenv( "$HOME" );
    if (home == 0)
    {
        struct passwd  pw;
        memset( &pw, 0, sizeof( pw ) );

        char  buffer[256];
        struct passwd*  pwret = 0;

        if (getpwuid_r( getuid(), &pw, buffer, 255, &pwret ) == 0)
            home = pw.pw_dir;
    }

    if (home)
        path = home;

#else

    char  home[MAX_PATH];
    if (SUCCEEDED( SHGetFolderPathA( NULL, CSIDL_PROFILE, NULL, 0, home ) ))
        path = home;

#endif

    return  path;
}


//----------------------------------------------------------------------------
/** @fn     std::string  micmgmt::hostOsType()
 *  @return operating system type name
 *
 *  Returns the underlaying operating system type name. (e.g. "Linux").
 */

std::string  micmgmt::hostOsType()
{
#ifdef  __linux__
    return  "Linux";
#else
    return  "Windows";
#endif
}


//----------------------------------------------------------------------------
/** @fn     std::string  micmgmt::hostOsVersion()
 *  @return OS version
 *
 *  Returns the OS version as string.
 */

std::string  micmgmt::hostOsVersion()
{
    string  version;

#ifdef  __linux__

    std::ifstream  strm( PROC_HOST_VERSION );
    if (strm.is_open())
    {
        string  line;
        getline( strm, line );
        strm.close();

        if (!line.empty())
        {
            // Extract third space separated element
            string::size_type  pos = line.find( " " );
            ++pos = line.find( " ", ++pos );
            string::size_type  end = line.find( " ", ++pos );

            version = line.substr( pos, end - pos );
        }
    }

#else // __linux__
    // DEV NOTE: Windows "GetVersion()" and "GetVersionEx()" APIs have been
    // deprecated as of Windows 8.1 and forward!  This is the new "recommended"
    // method. Ugly but it works.
    if (IsWindows7OrGreater() == TRUE)
    {
        if (IsWindowsServer() == FALSE)
        {
            version = "Windows 7";
        }
        else
        {
            version = "Windows Server 2008 R2";
        }
    }
    if (IsWindows7SP1OrGreater() == TRUE)
    {
        if (IsWindowsServer() == FALSE)
        {
            version = "Windows 7 SP1";
        }
        else
        {
            version = "Windows Server 2008 R2 SP1";
        }
    }
    if (IsWindows8OrGreater() == TRUE)
    {
        if (IsWindowsServer() == FALSE)
        {
            version = "Windows 8";
        }
        else
        {
            version = "Windows Server 2012";
        }
    }
    if (IsWindows8Point1OrGreater() == TRUE)
    {
        if (IsWindowsServer() == FALSE)
        {
            version = "Windows 8.1";
        }
        else
        {
            version = "Windows Server 2012 R2";
        }
    }
#if 0 // Windows 10 or greater: modify this code adding as needed...
    if (IsWindows10Point1OrGreater() == TRUE)
    {
        if (IsWindowsServer() == FALSE)
        {
            version = "Windows 10";
        }
        else
        {
            version = "Windows Server ????";
        }
    }
#endif // 0: Windows 10+
#endif // __linux__

    return  version;
}


//----------------------------------------------------------------------------
/** @fn     size_t  micmgmt::hostMemory()
 *  @return physical host memory
 *
 *  Returns the amount of physical host memory in bytes.
 */

size_t  micmgmt::hostMemory()
{
    size_t  bytes = 0;

#ifdef  __linux__

   std::ifstream  strm( PROC_MEMORY_INFO );
   if (strm.is_open())
   {
       string  line;
       while (getline( strm, line ))
       {
           string::size_type  pos = line.find( PROC_PHYS_MEMORY );
           if (pos != string::npos)
           {
               // Extract physical memory value
               pos = line.find_first_of( "1234567890", pos );
               if (pos == string::npos)
                   break;       // Corrupted line

               string::size_type  end = line.find_first_not_of( "1234567890", pos );
               if (end == string::npos)
                   break;       // Corrupted line

               string  value = line.substr( pos, end - pos );
               char*   unit  = NULL;
               bytes = strtoul( value.c_str(), &unit, 10 );

               // Extract memory value unit
               string  unitstring;
               while (unit && *unit)
               {
                   char  c = *unit++;
                   if (isalpha( c ))
                       unitstring.append( 1, tolower( c ) );
               }

               if (unitstring.empty())
               {
                   bytes *= BYTES_PER_KB;   // Assume the most common unit
               }
               else
               {
                   if (unitstring.find( "kb" ) != string::npos)
                       bytes *= BYTES_PER_KB;
                   else if (unitstring.find( "mb" ) != string::npos)
                       bytes *= BYTES_PER_MB;
                   else if (unitstring.find( "gb" ) != string::npos)
                       bytes *= BYTES_PER_GB;
               }

               break;   // We got what we need
           }
       }

       strm.close();
   }

#else

   MEMORYSTATUSEX  memstat;
   ZeroMemory((PVOID)&memstat, (SIZE_T)sizeof(memstat));
   memstat.dwLength = (DWORD)sizeof(memstat);
   if (GlobalMemoryStatusEx(&memstat) == TRUE)
   {
       bytes = (size_t)memstat.ullTotalPhys;
   }

#endif

   return  bytes;
}


//----------------------------------------------------------------------------
/** @fn     std::string  micmgmt::copyright( const char* year )
 *  @param  year    Pointer to 4 digit year string (optional)
 *  @return copyright string
 *
 *  Returns the copyright string. The default string uses the year in which
 *  this library was compiled for the copyright string. The copyright year
 *  can be overwritten by a custom year by specifying a 4 digit \a year
 *  string as parameter to this function.
 *
 *  The returned copyright string looks like:
 *  "Copyright (C) 2017, Intel Corporation."
 *
 *  In case the year should be extracted from the \c \_\_DATE\_\_ keyword to
 *  be used with this function, it is recommended to do this in the following
 *  way:
 *
 *  @verbatim
 *      const char*  compileDate = __DATE__;
 *      std::string  myText = micmgmt::copyright( &compileDate[7] );
 *  @endverbatim
 *
 *  The offset \b 7 is a fixed offset in any ANSI standard conformant compiler.
 */

std::string  micmgmt::copyright( const char* year )
{
    string  retstr( INTEL_COPYRIGHT_ASCII );

    if (year && (strlen( year ) == 4))
    {
        string::size_type  yearpos = retstr.find( "2017" );
        if (yearpos != string::npos)
            retstr.replace( yearpos, 4, year );
    }

    return  retstr;
}


//----------------------------------------------------------------------------
/** @fn     std::string  micmgmt::copyrightUtf8( const char* year )
 *  @param  year    Pointer to 4 digit year string (optional)
 *  @return copyright string
 *
 *  Returns the current copyright string in UTF-8 notation. The default
 *  string uses the year in which this library was compiled for the copyright
 *  string. The copyright year can be overwritten by a custom year by
 *  specifying a 4 digit \a year string as parameter to this function.
 *
 *  The returned copyright string looks like:
 *  "Copyright © 2017, Intel Corporation."
 *
 *  In case the year should be extracted from the \c \_\_DATE\_\_ keyword to
 *  be used with this function, it is recommended to do this in the following
 *  way:
 *
 *  @verbatim
 *      const char*  compileDate = __DATE__;
 *      std::string  myText = micmgmt::copyrightUtf8( &compileDate[7] );
 *  @endverbatim
 *
 *  The offset \b 7 is a fixed offset in any ANSI standard conformant compiler.
 */

std::string  micmgmt::copyrightUtf8( const char* year )
{
    string  retstr( INTEL_COPYRIGHT_UTF8 );

    if (year && (strlen( year ) == 4))
    {
        string::size_type  yearpos = retstr.find( "2017" );
        if (yearpos != string::npos)
            retstr.replace( yearpos, 4, year );
    }

    return  retstr;
}


//----------------------------------------------------------------------------
/** @fn     std::string  micmgmt::deviceName( unsigned int number )
 *  @param  number  Device number [0...]
 *  @return device name
 *
 *  Compose and return the device name based on specified device \a number.
 */

std::string  micmgmt::deviceName( unsigned int number )
{
    stringstream  strm;
    strm << "mic" << number;

    return  strm.str();
}


//----------------------------------------------------------------------------
/// @brief Trim leading and trailing whitespace from a std::string
/// @param [in,out] str The string to trim whitespace from beginning and end (\a str is modified).
///
/// This function is formatted for use with the algorithm std::for_each().  It
/// modifies the input string and trims all leading and trailing whitespace.  Whitespace
/// is defined as any of \c "\\n\\r\\f\\v\\t\\s" where \c '\\s' is a space character.
void micmgmt::feTrim(std::string& str)
{
    const std::string whiteSpace = "\n\r\f\v\t ";
    // Trim Left
    string::size_type pos = str.find_first_not_of(whiteSpace);
    if (pos == string::npos)
    {
        str.clear();
    }
    else
    {
        str.erase(0, pos);

        // Trim Right
        pos = str.find_last_not_of(whiteSpace);
        if (pos != string::npos)
        {
            str.erase(pos + 1);
        }
    }
} // feTrim

//----------------------------------------------------------------------------
/// @brief Trim leading and trailing whitespace and reduce internal multiple spaces.
/// @param [in,out] str The string to modify in place.
///
/// This function is formatted for use with the algorithm std::for_each().  It
/// modified the input string and trims all leading and trailing whitespace.  Whitespace
/// is defined as any of \c "\\n\\r\\f\\v\\t\\s" where \c '\\s' is a space character.
/// This function will also remove extra white space from within the input string.
void micmgmt::feNormalize(std::string& str)
{
    const std::string nonSpaceWhiteSpace = "\n\r\f\v\t";
    feTrim(str);

    // Remove non-space whitespace.
    string::size_type pos = str.find_first_of(nonSpaceWhiteSpace);
    while (pos != string::npos)
    {
        str.erase(pos, 1);
        pos = str.find_first_of(nonSpaceWhiteSpace, pos);
    }

    // Remove extra white space runs in the text.
    pos = str.find("  ");
    string::size_type pos2 = 0;
    while (pos != string::npos)
    {
        pos2 = str.find_first_not_of(" ", pos);
        if (pos2 != string::npos)
        {
            str.erase(pos, pos2 - pos - 1);
        }
        else
        {
            str.erase(pos);
        }
        pos = str.find("  ");
    }
} // feNormalize

//----------------------------------------------------------------------------
/// @brief Trim leading and trailing whitespace from a std::string returning a new string.
/// @param [in] str The string to trim whitespace from beginning and end (\a str is not modified).
/// @return the new trimmed string.
///
/// It modifies a copy of the input string and trims all leading and trailing whitespace.  Whitespace
/// is defined as any of \c "\\n\\r\\f\\v\\t\\s" where \c '\\s' is a space character.
string micmgmt::trim(const std::string& str)
{
    string rv = str;
    feTrim(rv);
    return rv;
} // trim

//----------------------------------------------------------------------------
/// @brief Trim leading and trailing whitespace and reduce internal multiple spaces.
/// @param [in] str The string to modify and return.
/// @return the trimmed and reduced string.
///
/// It modified a copy of the input string and trims all leading and trailing whitespace.
/// Whitespace is defined as any of \c "\\n\\r\\f\\v\\t\\s" where \c '\\s' is a space character.
/// This function will also remove extra white space from within the input string.
string micmgmt::normalize(const std::string& str)
{
    string rv = str;
    feNormalize(rv);
    return rv;
} // normalize

//----------------------------------------------------------------------------
/// @brief Normalize \a str and then wrap it to fit inside the given \a width.
/// @param [in] text The raw input string that needs to be wrapped to a specific \a width.
/// @param [in] width The width to wrap the passed \a str to.
/// @param [in] suppressInternalWSReduction Skip reducing inner text whitespace in a line.
/// @return The vector of strings after wrapping the text.
///
/// This function performs a "word" wrap of the input string using a specific set of meaning
/// for the \c '-' character in the string.  It will not split \c --option at any of the dashs
/// but will wrap \c compound-word if it is on the line in the right location.
///
/// @exception std::out_of_range If the wrapping length is less than 12.
/// @exception std::invalid_argument If the string input to wrap \a str is empty before or after it is Normalized.
vector<string> micmgmt::wrap(const std::string& text, std::string::size_type width, bool suppressInternalWSReduction)
{
    string txt;
    if (suppressInternalWSReduction == false)
    {
        txt = normalize(text);
    }
    else
    {
        txt = trim(text);
    }
    string::size_type remaining = txt.length();
    string::size_type currentPos = 0;
    string::size_type scanFrom = 0;
    string::size_type loc = 0;
    vector<string> lines;

    if (width < 12)
    {
        throw out_of_range("Wrapping 'width' cannot be less than 12.");
    }
    if (remaining == 0)
    {
        throw invalid_argument("Cannot wrap a normalized string that is empty.");
    }

    while (remaining > width)
    {
        scanFrom = currentPos + std::min<std::string::size_type>(width, txt.length() - currentPos - 1);
        loc = currentPos;
        for (string::size_type i = scanFrom; i > currentPos; --i)
        {
            if (txt[i] == ' ' || ((txt[i] == '-' && i != width) && i > 0 && txt[i - 1] != '-' && i < (txt.length() - 1) && txt[i + 1] != '-'))
            {
                loc = i;
                break;
            }
        }
        if (loc == currentPos)
        { // Can't wrap normally, brute force it...
            string partial = txt.substr(currentPos, width);
            feTrim(partial);
            if (partial.length() > 0)
            {
                lines.push_back(partial);
            }
            remaining = remaining - width;
            currentPos = currentPos + width;
        }
        else if (txt[loc] == ' ')
        {
            remaining = remaining - (loc - currentPos + 1);
            string partial = txt.substr(currentPos, (loc - currentPos));
            feTrim(partial);
            if (partial.length() > 0)
            {
                lines.push_back(partial);
            }
            currentPos += (loc - currentPos + 1);
        }
        else if (txt[loc] == '-')
        {
            bool addEndingDash = false;
            if (txt[loc - 1] == ' ')
            {
                --loc;
            }
            else if (txt[loc - 1] == '-' && txt[loc - 2] == ' ')
            {
                loc -= 2;
            }
            else if (txt[loc - 1] == '-' && isalpha(txt[loc - 2]))
            {
                loc -= 2;
            }
            else if (isalpha(txt[loc - 1]))
            {
                --loc;
                addEndingDash = true;
            }
            remaining = remaining - (loc - currentPos + 1);
            string partial = txt.substr(currentPos, (loc - currentPos + 1));
            feTrim(partial);
            if (addEndingDash == true)
            {
                partial += "-";
            }
            if (partial.length() > 0)
            {
                lines.push_back(partial);
            }
            currentPos += (loc - currentPos + 1);
            if (addEndingDash == true)
            {
                ++currentPos;
            }
        }
    }
    string partial2 = txt.substr(currentPos);
    feTrim(partial2);
    if (partial2.length() > 0)
    {
        lines.push_back(partial2);
    }
    return lines;
} // wrap

//----------------------------------------------------------------------------
/// @brief This method determines if the passed string is a cannonical name.
/// @param [in] name This is the string to check.
/// @return Returns \c true if the name is valid or \c false otherwise.
///
/// This method determines if the passed string is a cannonical name. This means only alpha-numeric plus
/// underscore and dash. A valid name cannot be empty.
bool micmgmt::verifyCanonicalName(const std::string& name)
{
    if (name.empty() == true ||
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != string::npos ||
        name[0] == '-' || isdigit(name[0]))
    {
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------
/// @brief This method returns the install path for MPSS.
/// @return This method returns the install path for MPSS or empty if the "INTEL_MPSS_INSTALL"
/// environment variable is not set.
///
/// This method returns the install path for MPSS.  This is a cross platform method.
std::string micmgmt::mpssInstallFolder()
{
#ifdef _WIN32
    return getEnvVar(INTEL_MPSS_HOME).second;
#else
    // Requires a installed file at "/etc/mpss/systools.conf" with line
    // "INTEL_MPSS_HOME=<location>" where location defaults to "/usr/share/mpss".
    ToolSettings cfg;
    if (cfg.isLoaded() == true)
    {
        return cfg.getValue(INTEL_MPSS_HOME);
    }
    else
    {
        return string(LINUX_HOME_PATH);
    }
#endif
}

// Private method to match a file with a file mask.
namespace
{ // Forced to not use regex due to RHEL 6.x...
} // empty namespace

//----------------------------------------------------------------------------
/// @brief This method will search for files with the passed \a mask in the passed folder and return the list.
/// @param [in] folder This is the folder in which to search for requested file type.
/// @param [in] mask This is the file mask used to match files in the requested folder.
/// @return The list of matching files is returned or an empty list if 1) there were no files or 2) there was an
/// error searching the folder.
///
/// This method will search for files with the passed \a mask in the passed folder and return the list. This is a cross-
/// platform method.
std::vector<std::string> micmgmt::findMatchingFilesInFolder(const std::string& folder, const std::string& mask)
{
    vector<string> list;
    if (folder.empty() == true || mask.empty() == true)
    {
        return list;
    }
    string sep = filePathSeparator();

#ifdef _WIN32
    string searchPath; //  = folder + filePathSeparator() + pattern
    string localFolder;
    if (folder[folder.size() - 1] == sep[0])
    {
        localFolder = folder;
        searchPath = folder + "*.*"; // All files!
    }
    else
    {
        localFolder = folder + sep;
        searchPath = folder + sep + "*.*"; // All files!
    }
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA((LPCSTR)searchPath.c_str(), &ffd); // All Files
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return list;
    }
    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }
        string file = ffd.cFileName;
        if(matchWithMask(file, mask) == true)
        {
            list.push_back(localFolder + file);
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    string localFolder = folder;
    if(localFolder[localFolder.size() - 1] == sep[0])
    {
        localFolder.erase(localFolder.size() - 1);
    }
    DIR *dp = opendir(localFolder.c_str());
    if (dp == NULL)
    {
        return list;
    }
    struct dirent *dirp;

    while ((dirp = readdir(dp)) != NULL)
    {
        string file = dirp->d_name;
        if((file.length() > mask.length()) && matchWithMask(file, mask) == true)
        {
            list.push_back(localFolder + sep + file);
        }
    }
    closedir(dp);

#endif
    return list;
}

//----------------------------------------------------------------------------
/// @brief Return a copy of the string with uppercase turned into lowercase based on locale.
/// @param [in] str String to convert to lower case.
/// @return Copy of string with all lowercase based on locale.
///
/// Return a copy of the string with uppercase turned into lowercase based on locale.
std::string micmgmt::toLower(const std::string& str)
{
    locale loc;
    string localStr = str;
    for (auto it = localStr.begin(); it != localStr.end(); ++it)
    {
        *it = std::tolower<char>(*it, loc);
    }
    return localStr;
}

//----------------------------------------------------------------------------
/// @brief This function returns both if the environment variable exists and the value of an evironment variable if
/// it exists in a cross platform way.
/// @param [in] varName The name of the environment variable.  This may be case sensitive depending on the platform.
/// @returns A pair where \c first represents the existence of the environment variable (\c true if it exists \c false 
/// otherwise) and a value which will be the value of the variable if it exists.
///
/// This function returns both if the environment variable exists and the value of an evironment variable if
/// it exists in a cross platform way.  No exceptions will be thrown.
std::pair<bool, std::string> micmgmt::getEnvVar(const std::string& varName)
{
#ifdef _WIN32
    // Using _dupenv_s to avoid security compiler errors in VS2013 when using std::getenv().
    DWORD size = GetEnvironmentVariableA((LPCSTR)varName.c_str(), NULL, 0);
    if (size > 0)
    {
        LPSTR buffer = new char[size];
        size = GetEnvironmentVariableA((LPCSTR)varName.c_str(), buffer, size);
        string sval = buffer;
        delete[] buffer;
        return make_pair<bool,std::string>(true, sval.c_str());
    }
    else
    {
        return make_pair<bool,std::string>(false, string().c_str());
    }
#else // Linux
    char* result = std::getenv(varName.c_str());
    if (result != NULL)
    {
        return make_pair<bool,string>(true, string(result).c_str());
    }
    else
    {
        return make_pair<bool,string>(false, string().c_str());
    }
#endif
}


//----------------------------------------------------------------------------
/// @brief This function registers signal handlers in a platform independent way.
/// @param [in] callback The function pointer defined by the \a trapSignalHandler
/// typedef.
///
/// This function registers signal handlers in a platform independent way.
void micmgmt::registerTrapSignalHandler(trapSignalHandler callback)
{
    if (callback != NULL && (gTrapHandler == NULL || gTrapHandler == callback))
    {
        if (gTrapHandler == NULL)
        {
            gTrapHandler = callback;
            gTrapCount = 1;
        }
        else
        {
            ++gTrapCount;
        }
#ifdef _WIN32
        SetConsoleCtrlHandler(&signalHandler, TRUE);
#else
        signal(SIGINT, signalHandler);
        signal(SIGABRT, signalHandler);
        signal(SIGTERM, signalHandler);
#endif
    }
}

//----------------------------------------------------------------------------
/// @brief This function unregisters signal handlers in a platform independent way.
/// @param [in] callback The function pointer defined by the \a trapSignalHandler
/// typedef that was previously added with \a registerTrapSignalHandler.
///
/// This function unregisters signal handlers in a platform independent way.
void micmgmt::unregisterTrapSignalHandler(trapSignalHandler callback)
{
    if (callback != NULL && gTrapHandler == callback && gTrapCount > 0)
    {
#ifdef _WIN32
        SetConsoleCtrlHandler(&signalHandler, FALSE);
#else
        signal(SIGINT, SIG_DFL);
        signal(SIGABRT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
#endif
        --gTrapCount;
        if (gTrapCount == 0)
        {
            gTrapHandler = NULL;
        }
    }
}

//----------------------------------------------------------------------------
/// @brief This function tests a file path to see if the end of the path is a folder.
/// @param [in] path The file path to test.
/// @return Returns \c true if the path represents a folder or \c false if the path is
/// a file or not found.
///
/// This function tests a file path to see if the end of the path is a folder.
bool micmgmt::pathIsFolder(const std::string& path)
{
#ifdef _WIN32
    DWORD dw = GetFileAttributesA((LPCSTR)path.c_str());
    if(dw != INVALID_FILE_ATTRIBUTES)
    {
        return ((dw & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)?true:false;
    }
    return false;
#else
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
    {
        if ((s.st_mode & S_IFDIR) == S_IFDIR)
        {
            return true;
        }
    }
    return false;
#endif
}
