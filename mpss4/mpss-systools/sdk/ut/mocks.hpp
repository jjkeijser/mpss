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
#ifndef MICMGMT_MOCKS_HPP_
#define MICMGMT_MOCKS_HPP_

// SYSTEM INCLUDEs
//
#include <dirent.h>

// GTEST & GMOCK INCLUDES
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>


// PROJECT INCLUDES
//
#include "MicBootConfigInfo.hpp"
#include "RasConnection.hpp"
#include "ScifConnectionBase.hpp"
#include "ScifRequestInterface.hpp"
#include "SystoolsdConnection.hpp"

#include "PciConfigData_p.hpp"

using ::testing::NiceMock;

namespace micmgmt
{
    // The NotNice mock version of a ScifRequestInterface
    class NotNiceMockScifRequest : public ScifRequestInterface
    {
    public:
        MOCK_CONST_METHOD0( isValid, bool() );
        MOCK_CONST_METHOD0( isError, bool() );
        MOCK_CONST_METHOD0( isSendRequest, bool() );
        MOCK_CONST_METHOD0( command, int() );
        MOCK_CONST_METHOD0( parameter, uint32_t() );
        MOCK_CONST_METHOD0( buffer, char*() );
        MOCK_CONST_METHOD0( byteCount, size_t() );
        MOCK_CONST_METHOD0( errorCode, int() );

        void clearError()
        {
            mError = 0;
        }

        void setError( int err )
        {
            mError = err;
        }

        void setSendRequest( bool isSend )
        {
            mIsSendRequest = isSend;
        }

    private:
        int mError;
        bool mIsSendRequest;

    };

    // Define an interface just like the one provided by SystoolsdConnection
    // It will be used as the template argument to SystoolsdConnectionAbstract
    class MockScifConnectionInterface
    {
    public:
        virtual ~MockScifConnectionInterface() { }

        virtual bool              isOpen() const = 0;
        virtual int               deviceNum() const = 0;
        virtual std::string       errorText() const = 0;

        virtual uint32_t          open() = 0;
        virtual void              close() = 0;

    protected:
        virtual bool              lock() = 0;
        virtual bool              unlock() = 0;
        virtual int               send( const char* data, int len ) = 0;
        virtual int               receive( char* data, int len ) = 0;
        virtual void              setPortNum( int port ) = 0;
        virtual void              setErrorText( const std::string& text, int error=0 ) = 0;
    };

    // A NotNice mock implementation of a MockScifConnectionInterface
    class NotNiceMockScifConnection : public MockScifConnectionInterface
    {
    public:
        NotNiceMockScifConnection( int devnum ) :
            mDevNum(devnum), mPortNum(0) { }

        MOCK_CONST_METHOD0( isOpen, bool() );
        MOCK_CONST_METHOD0( deviceNum, int() );
        MOCK_CONST_METHOD0( errorText, std::string() );

        MOCK_METHOD0( open, uint32_t() );
        MOCK_METHOD0( close, void() );
        MOCK_METHOD1( request, uint32_t(ScifRequestInterface*) );

        MOCK_METHOD0( lock, bool() );
        MOCK_METHOD0( unlock, bool() );
        MOCK_METHOD2( send, int(const char*, int) );
        MOCK_METHOD2( receive, int(char*, int) );
        //
        // In SystoolsdConnection specialization
        MOCK_METHOD1( smcRequest, uint32_t(ScifRequestInterface*) );


        int deviceNum()
        {
            return mDevNum;
        }

    protected:
        void setErrorText( const std::string& error, int errorNum=0 )
        {
            (void)errorNum;
            mErrorText = error;
        }

        void setPortNum( int port )
        {
            mPortNum = port;
        }

    private:
        int mDevNum;
        int mPortNum;
        std::string mErrorText;
    };

    class NotNiceMockLoader : public LoaderInterface
    {
    private:
        std::string m_name;

    public:
        MOCK_CONST_METHOD0( isLoaded, bool() );
        MOCK_CONST_METHOD0( fileName, std::string() );
        MOCK_CONST_METHOD0( errorText, std::string() );
        MOCK_CONST_METHOD0( version, std::string() );

        MOCK_METHOD0( unload, bool() );
        MOCK_METHOD1( load, bool(int) );
        MOCK_METHOD1( lookup, void*(const std::string&) );
        MOCK_METHOD3( setVersion, void(int, int, int) );
        MOCK_METHOD1( setFileName, void(const std::string&) );
    };

    class NotNiceIOStream
    {
        std::string m_path;
    public:
        MOCK_CONST_METHOD0( is_open, bool());
        MOCK_CONST_METHOD0( good, bool() );
        MOCK_CONST_METHOD0( fail, bool() );

        MOCK_METHOD1( open, void(const std::string&) );
        MOCK_METHOD0( close, void() );
        MOCK_METHOD2( write, NotNiceIOStream& (const char*, size_t) );
        MOCK_METHOD2( getline, NotNiceIOStream& (char*, size_t) );
    };

    class NotNiceDirent
    {
    public:
        MOCK_METHOD1( opendir, DIR*(const char*) );
        MOCK_METHOD1( readdir, struct dirent*(DIR*) );
        MOCK_METHOD1( closedir, int(DIR*) );
    };

    class NotNiceBootInfo
    {
    public:
        MOCK_CONST_METHOD0( isValid, bool() );
        MOCK_CONST_METHOD0( isCustom, bool() );
        MOCK_CONST_METHOD0( initRamFsPath, std::string() );
        MOCK_CONST_METHOD0( imagePath, std::string() );
        MOCK_CONST_METHOD0( systemMapPath, std::string() );

        MOCK_METHOD0( clear, void() );
        MOCK_METHOD1( setCustomState, void(bool) );
        MOCK_METHOD1( setImagePath, void(const std::string&) );
        MOCK_METHOD1( setInitRamFsPath, void(const std::string&) );
        MOCK_METHOD1( setSystemMapPath, void(const std::string&) );
    };

    class NotNiceMockMpssStack
    {
    public:
        NotNiceMockMpssStack( int device ) {(void)device; }
        MOCK_CONST_METHOD2( getDeviceProperty, uint32_t(std::string*, const std::string&) );
        MOCK_CONST_METHOD3( getDeviceProperty, uint32_t(uint32_t*, const std::string&, int) );
        MOCK_CONST_METHOD1( getDeviceState, uint32_t(int*) );
        MOCK_CONST_METHOD1( getDevicePciConfigData, uint32_t(PciConfigData*) );
        MOCK_CONST_METHOD1( getSystemMpssVersion, uint32_t(std::string*) );
        MOCK_CONST_METHOD1( getSystemDriverVersion, uint32_t(std::string*) );
        MOCK_CONST_METHOD1( deviceBoot, uint32_t(const MicBootConfigInfo&) );
        MOCK_CONST_METHOD1( deviceReset, uint32_t(bool) );
    };

    class NotNiceFilesystem
    {
    public:
        MOCK_METHOD2( mkdir, int(const char*, mode_t) );
        MOCK_METHOD2( symlink, int(const char*, const char*) );
        MOCK_METHOD2( realpath, char*(const char*, char*) );
        MOCK_METHOD1( basename, char*(char*) );
        MOCK_METHOD1( rmdir, int(const char*) );
        MOCK_METHOD1( unlink, int(const char*) );
    };


    // A NiceMock ScifRequest
    typedef NiceMock< NotNiceMockScifRequest > MockScifRequest;
    // A NiceMock ScifConnection
    typedef NiceMock< NotNiceMockScifConnection > MockScifConnection;
    // A SystoolsdConnection using a MockScifConnection
    typedef SystoolsdConnectionAbstract< MockScifConnection > MockSystoolsdConnection;
    // A RasConnection using a MockScifConnection
    typedef RasConnectionAbstract< MockScifConnection > MockRasConnection;

    // A NiceMock Loader
    typedef NiceMock< NotNiceMockLoader > MockLoader;

    // A NiceMock std::ofstream-like object
    typedef NiceMock< NotNiceIOStream > MockIOStream;
    // A NiceMock DirentFunctions-like object
    typedef NiceMock< NotNiceDirent > MockDirent;
    // A NiceMock MicBootConfigInfo-like object
    typedef NiceMock< NotNiceBootInfo > MockBootInfo;

    // A NiceMock MpssStackBase-like object
    typedef NiceMock< NotNiceMockMpssStack > MockMpssStack;

    // A NiceMock FilesystemUtils-like object
    typedef NiceMock< NotNiceFilesystem > MockFilesystem;

} // namespace micmgmt

#endif
