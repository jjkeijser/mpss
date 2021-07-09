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

#ifndef MICMGMT_MICMGMTCOMMON_HPP
#define MICMGMT_MICMGMTCOMMON_HPP

// SYSTEM INCLUDES
//
#include    <string>
#include    <vector>

// PROJECT NAMESPACE
//
namespace  micmgmt
{

//----------------------------------------------------------------------------
//  FUNCTIONS:

    bool           isAdministrator();
    bool           pathIsFolder(const std::string& path);
    std::string    filePathSeparator();
    std::string    fileNameFromPath( const std::string& path );
    std::string    userHomeFilePath( const std::string& file );
    std::string    userHomePath();
    std::string    userName();
    std::string    hostOsType();
    std::string    hostOsVersion();
    size_t         hostMemory();
    std::string    copyright( const char* year=0 );
    std::string    copyrightUtf8( const char* year=0 );
    std::string    deviceName( unsigned int number );

    void           feTrim(std::string& str);
    void           feNormalize(std::string& str);
    std::string    trim(const std::string& str);
    std::string    normalize(const std::string& str);
    bool           verifyCanonicalName(const std::string& name);

    std::vector<std::string> wrap(const std::string& text, std::string::size_type width, bool suppressInternalWSReduction = false);

    std::string toLower(const std::string& str);
    std::pair<bool, std::string> getEnvVar(const std::string& varName);
    std::string mpssInstallFolder();
    std::vector<std::string> findMatchingFilesInFolder(const std::string& folder, const std::string& extension);
    std::string normalizePath(const std::string& originalPath);
    std::string safeFilePath(const std::string& file);

    typedef void(*trapSignalHandler)();
    void registerTrapSignalHandler(trapSignalHandler callback);
    void unregisterTrapSignalHandler(trapSignalHandler callback);

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICMGMTCOMMON_HPP
