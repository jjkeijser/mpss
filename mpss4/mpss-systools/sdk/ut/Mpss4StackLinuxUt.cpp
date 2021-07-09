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

// SYSTEM INCLUDES
//
#include <cstring>

#include <sys/types.h>
#include <dirent.h>

// GTEST & GMOCK INCLUDES
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// PROJECT INCLUDES
//
#include "KnlCustomBootManager.hpp"
#include "MpssStackBase.hpp"
#include "Mpss4StackLinux.hpp"
#include "LibmpssconfigFunctions.hpp"
#include "mocks.hpp"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;

namespace
{
    const int DEVNUM = 2;

    // card states
    const char* const  STATE_ONLINE   = "online";
    const char* const  STATE_READY    = "ready";
    const char* const  STATE_BOOTING  = "booting";
    const char* const  STATE_UNKNOWN  = "unknown";

    enum eState
    {
        eOnline=0,
        eReady,
        eBooting,
        eUnknown = 0x100,
        eError
    };

    // Signature-compatible functions/function templates to "mock" libmpssconfig
    int my_boot_linux( int node )
    {
        (void) node;
        return 0;
    }

    int my_boot_media( int node, const char* path )
    {
        (void) node;
        (void) path;
        return 0;
    }

    template <int state=eState::eOnline>
    char* my_get_state( int node )
    {
        (void) node;
        if(state == -1)
            return NULL;

        switch(state)
        {
            case eOnline:
                return (char*)STATE_ONLINE;
            case eReady:
                return (char*)STATE_READY;
            case eBooting:
                return (char*)STATE_BOOTING;
            case eUnknown:
                return (char*)STATE_UNKNOWN;
            case eError:
                return (char*)"nope";
            default:
                return NULL;
        }
    }

    int my_reset_node( int node )
    {
        (void) node;
        return 0;
    }

    template <bool admin>
    bool isAdmin()
    {
        return admin;
    }

    // Functions to use with member function pointers of FakeStack objects
    std::string baseName()
    {
        return "mic";
    }

    // argument ret -> return value
    // doSet -> whether or not to set 'data'
    template<uint32_t ret=0, bool doSet=true>
    uint32_t sysProp(std::string* data, const std::string& name)
    {
        (void) name;
        if(ret)
            return ret;

        if(data && doSet)
            *data = "x200";

        return 0;
    }

    template <uint32_t ret=0>
    uint32_t fakeParseFunction(std::string* data, const std::string& valstr,
            const std::string& name)
    {
        (void) data;
        (void) valstr;
        (void) name;
        return ret;
    }

    class FakeStack
    {
    public:
        typedef std::string (*pDeviceNameT)();
        typedef uint32_t (*pGetSystemPropertyT)(std::string*, const std::string&);

        pDeviceNameT pDeviceName;
        pGetSystemPropertyT pGetSystemProperty;

        FakeStack() : pDeviceName(baseName), pGetSystemProperty(sysProp)
        {

        }

        std::string deviceBaseName() const
        {
            return pDeviceName();
        }

        uint32_t getSystemProperty(std::string* data, const std::string& name) const
        {
            return pGetSystemProperty(data, name);
        }
    };

    class FakeCustomBootManager : public micmgmt::CustomBootManager
    {
    public:
        FakeCustomBootManager( const std::string&, const std::string& )
        {
            // Nothing to do
        }

        static std::shared_ptr<FakeCustomBootManager> create( const std::string& s1, const std::string& s2 )
        {
            return std::make_shared<FakeCustomBootManager>(FakeCustomBootManager( s1, s2 ));
        }

        std::string fwRelPath() const
        {
            return "somepath";
        }

        std::string fwAbsPath() const
        {
            return "someabspath";
        }
    };

} // unnamed namespace

namespace micmgmt
{
    using namespace mpss4linux_detail;
    typedef LibmpssconfigFunctions::libmpss_boot_linux libmpss_boot_linux;
    typedef LibmpssconfigFunctions::libmpss_boot_media libmpss_boot_media;
    typedef LibmpssconfigFunctions::libmpss_get_state libmpss_get_state;
    typedef LibmpssconfigFunctions::libmpss_reset_node libmpss_reset_node;
    typedef FakeCustomBootManager BootMgr;

class Mpss4StackLinuxTestBase : public ::testing::Test
{
protected:
    LibmpssconfigFunctions functions;
    Mpss4StackLinux mpss;
    MockLoader* loader;
    MockIOStream stream;

public:
    Mpss4StackLinuxTestBase() :
        functions(&my_boot_linux, &my_boot_media,
                  &my_get_state, &my_reset_node),
        mpss( DEVNUM, functions )
    {
    }

protected:
    virtual void SetUp()
    {
        loader = getLoader();
        setStreamDefaults( stream );
        mpss.setLoader( loader );
    }

    void setLoaderDefaults( MockLoader& someLoader )
    {
        ON_CALL( someLoader, isLoaded() )
            .WillByDefault( Return(true) );
        ON_CALL( someLoader, fileName() )
            .WillByDefault( Return("libname") );
        ON_CALL( someLoader, errorText() )
            .WillByDefault( Return("liberror") );
        ON_CALL( someLoader, version() )
            .WillByDefault( Return("1.0.0") );

        ON_CALL( someLoader, load(_) )
            .WillByDefault( Return(true) );
        ON_CALL( someLoader, unload() )
            .WillByDefault( Return(true) );

        ON_CALL( someLoader, lookup( "boot_linux" ) )
            .WillByDefault( Return((void*)(libmpss_boot_linux) my_boot_linux) );
        ON_CALL( someLoader, lookup( "boot_media" ) )
            .WillByDefault( Return((void*)(libmpss_boot_media) my_boot_media) );
        ON_CALL( someLoader, lookup( "get_state" ) )
            .WillByDefault( Return((void*)(libmpss_get_state) my_get_state) );
        ON_CALL( someLoader, lookup( "reset_node" ) )
            .WillByDefault( Return((void*)(libmpss_reset_node) my_reset_node) );
    }

    void setStreamDefaults( MockIOStream& stream )
    {
        ON_CALL( stream, is_open() )
            .WillByDefault( Return(true) );
        ON_CALL( stream, good() )
            .WillByDefault( Return(true) );
        ON_CALL( stream, fail() )
            .WillByDefault( Return(false) );

        ON_CALL( stream, write(NotNull(), Gt(0)))
            .WillByDefault( ReturnRef(stream) );
        ON_CALL( stream, getline(NotNull(), Gt(0)) )
            .WillByDefault( ReturnRef(stream) );
    }

    MockLoader* getLoader()
    {
        MockLoader* loader = new MockLoader;
        setLoaderDefaults( *loader );
        return loader;
    }
};

typedef Mpss4StackLinuxTestBase Mpss4StackLinuxTest;

// Test fixture for parameterized tests
class Mpss4StackLinuxDeviceNumbersTest :
    public Mpss4StackLinuxTest,
    public ::testing::WithParamInterface<std::vector<std::string>>
{
};

class Mpss4StackLinuxDevPropTest :
    public Mpss4StackLinuxTest,
    public ::testing::WithParamInterface<std::string>
{
};

TEST_F(Mpss4StackLinuxTest, TC_loadlib_notloaded_success_001)
{
    EXPECT_CALL( *loader, isLoaded() )
        .WillOnce( Return(false) )
        .WillOnce( Return(true) ); // On destructor
    EXPECT_CALL( *loader, setFileName(_) )
        .Times(1);
    EXPECT_CALL( *loader, setVersion(_, _, _) )
        .Times(1);
    EXPECT_CALL( *loader, load(_) )
        .WillOnce( Return(true) );

    ASSERT_TRUE( mpss.loadLib() );
}

TEST_F(Mpss4StackLinuxTest, TC_loadlib_loaded_success_001)
{
    EXPECT_CALL( *loader, isLoaded() )
        .WillRepeatedly( Return(true) );
    ASSERT_TRUE( mpss.loadLib() );
}

TEST_F(Mpss4StackLinuxTest, TC_loadlib_load_fail_001)
{
    EXPECT_CALL( *loader, isLoaded() )
        .WillOnce( Return(false) )
        .WillOnce( Return(false) ); // On destructor
    EXPECT_CALL( *loader, setFileName(_) )
        .Times(1);
    EXPECT_CALL( *loader, setVersion(_, _, _) )
        .Times(1);
    EXPECT_CALL( *loader, load(_) )
        .WillOnce( Return(false) );
    ASSERT_FALSE( mpss.loadLib() );
}

TEST_F(Mpss4StackLinuxTest, TC_loadlib_lookup_bootlinux_fail_001)
{
    EXPECT_CALL( *loader, isLoaded() )
        .WillRepeatedly( Return(true) );
    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("boot_linux") )
        .WillOnce( Return((void*)NULL) );
    ASSERT_FALSE( mpss.loadLib() );
}

TEST_F(Mpss4StackLinuxTest, TC_loadlib_lookup_bootmedia_fail_001)
{
    EXPECT_CALL( *loader, isLoaded() )
        .WillRepeatedly( Return(true) );
    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("boot_media") )
        .WillOnce( Return((void*)NULL) );
    ASSERT_FALSE( mpss.loadLib() );
}

TEST_F(Mpss4StackLinuxTest, TC_loadlib_lookup_getstate_fail_001)
{
    EXPECT_CALL( *loader, isLoaded() )
        .WillRepeatedly( Return(true) );
    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("get_state") )
        .WillOnce( Return((void*)NULL) );
    ASSERT_FALSE( mpss.loadLib() );
}

TEST_F(Mpss4StackLinuxTest, TC_loadlib_lookup_resetnode_fail_001)
{
    EXPECT_CALL( *loader, isLoaded() )
        .WillRepeatedly( Return(true) );
    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("reset_node") )
        .WillOnce( Return((void*)NULL) );
    ASSERT_FALSE( mpss.loadLib() );
}

TEST_F(Mpss4StackLinuxTest, TC_mpsshomepath_001)
{
    ASSERT_NE( "", mpss.mpssHomePath() );
}

TEST_F(Mpss4StackLinuxTest, TC_mpssflashpath_001)
{
    ASSERT_NE( "", mpss.mpssFlashPath() );
}

TEST_F(Mpss4StackLinuxTest, TC_mpssbootpath_001)
{
    ASSERT_NE( "", mpss.mpssBootPath() );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemdriverversion_001)
{
    std::string ver;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_NOT_SUPPORTED ),
            mpss.getSystemDriverVersion( &ver ) );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemdriverversion_null_fail_001)
{
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ),
            mpss.getSystemDriverVersion( NULL ) );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemprop_nulldata_fail_001)
{
    std::string name;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ),
            mpss.getSystemProperty(NULL, name) );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemprop_emptyname_fail_001)
{
    std::string data;
    std::string name;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ),
            mpss.getSystemProperty(&data, name) );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemprop_openfails_001)
{
    std::string data;
    std::string name = "someprop";
    EXPECT_CALL( stream, open(_) )
        .Times(1);
    EXPECT_CALL( stream, is_open() )
        .WillOnce( Return(false) );
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR ),
            mpss.getSystemProperty( &data, name, stream ) );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemprop_getline_fail_001)
{
    std::string data;
    std::string name = "someprop";
    EXPECT_CALL( stream, open(_) )
        .Times(1);
    EXPECT_CALL( stream, is_open() )
        .WillOnce( Return(true) );
    EXPECT_CALL( stream, getline( NotNull(), Gt(0) ) )
        .WillOnce( ReturnRef(stream) );
    EXPECT_CALL( stream, fail() )
        .WillOnce( Return(true) );
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR ),
            mpss.getSystemProperty( &data, name, stream ) );
    ASSERT_EQ( "", data );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemprop_001)
{
    std::string data;
    std::string name = "someprop";
    const std::string expectedData = "this is what we wanted";
    EXPECT_CALL( stream, open(_) )
        .Times(1);
    EXPECT_CALL( stream, is_open() )
        .WillOnce( Return(true) );

    EXPECT_CALL( stream, getline( NotNull(), Gt(0) ) )
        .WillOnce( DoAll(
                    SetArrayArgument<0>( expectedData.c_str(),
                                          expectedData.c_str() + expectedData.size() ),
                    ReturnRef(stream) ) );
    EXPECT_CALL( stream, fail() )
        .WillOnce( Return(false) );
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getSystemProperty( &data, name, stream ) );
    ASSERT_EQ( expectedData, data );
}

TEST_F(Mpss4StackLinuxTest, TC_setdevprop_emptydata_fail_001)
{
    std::string name("something");
    std::string data;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ),
            mpss.setDeviceProperty(name, data, stream) );
}

TEST_F(Mpss4StackLinuxTest, TC_setdevprop_isopen_success_001)
{
    EXPECT_CALL( stream, open(_) )
        .Times(1);
    EXPECT_CALL( stream, is_open() )
        .WillOnce( Return(true) );
    EXPECT_CALL( stream, write( NotNull(), Gt(0) ) )
        .WillOnce( ReturnRef(stream) );
    EXPECT_CALL( stream, good() )
        .WillOnce( Return(true) );
    std::string name("something");
    std::string data("something");;

    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.setDeviceProperty(name, data, stream) );
}

TEST_F(Mpss4StackLinuxTest, TC_setdevprop_notgood_success_001)
{
    // With errno = EACCES
    errno = EACCES;
    {
        EXPECT_CALL( stream, open(_) )
            .Times(1);
        EXPECT_CALL( stream, is_open() )
            .WillOnce( Return(true) );
        EXPECT_CALL( stream, write( NotNull(), Gt(0) ) )
            .WillOnce( ReturnRef(stream) );
        EXPECT_CALL( stream, good() )
            .WillOnce( Return(false) );
        std::string name("something");
        std::string data("something");;

        EXPECT_EQ( MicDeviceError::errorCode( MICSDKERR_NO_ACCESS ),
                mpss.setDeviceProperty(name, data, stream) );
    }

    errno = 0;
    {
        EXPECT_CALL( stream, open(_) )
            .Times(1);
        EXPECT_CALL( stream, is_open() )
            .WillOnce( Return(true) );
        EXPECT_CALL( stream, write( NotNull(), Gt(0) ) )
            .WillOnce( ReturnRef(stream) );
        EXPECT_CALL( stream, good() )
            .WillOnce( Return(false) );
        std::string name("something");
        std::string data("something");;

        EXPECT_EQ( MicDeviceError::errorCode( MICSDKERR_PROPERTY_NOT_FOUND ),
                mpss.setDeviceProperty(name, data, stream) );
    }

}

TEST_F(Mpss4StackLinuxTest, TC_setdevprop_isopen_fail_001)
{
    EXPECT_CALL( stream, open(_) )
        .Times(1);
    EXPECT_CALL( stream, is_open() )
        .WillOnce( Return(false) );
    std::string name("something");
    std::string data("something");;

    errno = 0; // Make sure errno is clear
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_PROPERTY_NOT_FOUND ),
            mpss.setDeviceProperty(name, data, stream) );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemdevnumbers_listnull_fail_001)
{
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ),
        mpss.getSystemDeviceNumbers(NULL) );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemdevnumbers_opendir_fail_001)
{
    MockDirent dirent;
    EXPECT_CALL( dirent, opendir(_) )
        .WillOnce( Return((DIR*)NULL) );
    EXPECT_CALL( dirent, closedir(_) )
        .Times(0);
    std::vector<size_t> list;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_LOADED ),
            mpss.getSystemDeviceNumbers(&list, dirent) );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemdevicenumbers_opendir_fail_002)
{
    MockDirent dirent;
    void *dir = &dirent; // A dummy pointer to be returned by mock opendir
    EXPECT_CALL( dirent, opendir(_) )
        .WillOnce( Return((DIR*) &dir) )
        .WillOnce( Return((DIR*) NULL) );
    EXPECT_CALL( dirent, closedir(_) )
        .Times(1);
    std::vector<size_t> list;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_DRIVER_NOT_INITIALIZED ),
            mpss.getSystemDeviceNumbers(&list, dirent) );
}

TEST_F(Mpss4StackLinuxTest, TC_getdevprop_001)
{
    std::string data;
    const std::string name("someprop");
    const std::string subkey("i_am_a_subkey");
    const std::string otherLine("other_subkey=penguin");
    const std::string propline("i_am_a_subkey=qotsa");
    const std::string expected("qotsa");

    // Two lines, first without desired subkey
    EXPECT_CALL( stream, getline( NotNull(), Gt(0) ) )
        .WillOnce(
            DoAll(
                SetArrayArgument<0>(
                    otherLine.c_str(), otherLine.c_str() + otherLine.size() ),
                ReturnRef(stream)
            ))
        .WillOnce(
            DoAll(
                SetArrayArgument<0>(
                    propline.c_str(), propline.c_str() + propline.size() ),
                ReturnRef(stream)
            ));
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getDeviceProperty(&data, name, subkey, stream) );
    ASSERT_EQ( expected, data );
}

TEST_F(Mpss4StackLinuxTest, TC_getdevprop_002)
{
    std::string data;
    const std::string name("someprop");
    const std::string subkey("i_am_a_subkey");

    //Ensure the next case is handled well
    const std::string propline("i_am_a_subkey=");

    // Two lines, first without desired subkey
    EXPECT_CALL( stream, getline( NotNull(), Gt(0) ) )
        .WillOnce(
            DoAll(
                SetArrayArgument<0>(
                    propline.c_str(), propline.c_str() + propline.size() ),
                ReturnRef(stream)
            ));
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getDeviceProperty(&data, name, subkey, stream) );
    ASSERT_EQ( "", data );
}

TEST_F(Mpss4StackLinuxTest, TC_getdevprop_fail_001)
{
    std::string data;
    const std::string name("someprop");
    const std::string subkey("i_am_a_subkey");

    // This case won't be handled
    const std::string propline("i_am_a_subkey");

    // Two lines, first without desired subkey
    EXPECT_CALL( stream, getline( NotNull(), Gt(0) ) )
        .WillOnce(
            DoAll(
                SetArrayArgument<0>(
                    propline.c_str(), propline.c_str() + propline.size() ),
                ReturnRef(stream)
            ));
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR ),
            mpss.getDeviceProperty(&data, name, subkey, stream) );
    ASSERT_EQ( "", data );
}

TEST_F(Mpss4StackLinuxTest, TC_getdevprop_nulldata_001)
{
    const std::string name("no");
    const std::string subkey("subkey");
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ),
            mpss.getDeviceProperty(NULL, name, subkey, stream) );
}

TEST_F(Mpss4StackLinuxTest, TC_getdevprop_emptyname_001)
{
    std::string data;
    const std::string name("");
    const std::string subkey("subkey");
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ),
            mpss.getDeviceProperty(&data, name, subkey, stream) );
}

TEST_F(Mpss4StackLinuxTest, TC_getdevprop_open_fail_001)
{
    std::string data;
    const std::string name("name");
    const std::string subkey("subkey");

    // Set errno
    errno = EACCES;
    {
        EXPECT_CALL( stream, is_open() )
            .WillOnce( Return(false) );
        EXPECT_EQ( MicDeviceError::errorCode( MICSDKERR_NO_ACCESS ),
                mpss.getDeviceProperty(&data, name, subkey, stream) );
    }
    errno = 0;
    {
        EXPECT_CALL( stream, is_open() )
            .WillOnce( Return(false) );
        ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_PROPERTY_NOT_FOUND ),
                mpss.getDeviceProperty(&data, name, subkey, stream) );
    }
}

TEST_F(Mpss4StackLinuxTest, TC_getdevprop_notgood_fail_001)
{
    std::string data;
    const std::string name("name");
    const std::string subkey("subkey");
    EXPECT_CALL( stream, good() )
        .WillOnce( Return(false) );
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR ),
            mpss.getDeviceProperty(&data, name, subkey, stream) );
}

TEST_F(Mpss4StackLinuxTest, TC_deviceboot_001)
{
    MockBootInfo bootinfo;
    EXPECT_CALL( bootinfo, isValid() )
        .WillOnce( Return(false) );
    auto err = mpss.deviceBoot<MockBootInfo, BootMgr, isAdmin<true> >( bootinfo );
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ), err );
}

TEST_F(Mpss4StackLinuxTest, TC_deviceboot_null_bootlinux_001)
{
    MockBootInfo bootinfo;
    functions.boot_linux = nullptr;
    Mpss4StackLinux temp( DEVNUM, functions );
    temp.setLoader( getLoader() );
    EXPECT_CALL( bootinfo, isValid() )
        .WillOnce( Return(true) );
    auto err = temp.deviceBoot<MockBootInfo, BootMgr, isAdmin<true> >( bootinfo );
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SHARED_LIBRARY_ERROR ), err );
}

TEST_F(Mpss4StackLinuxTest, TC_deviceboot_null_bootmedia_001)
{
    MockBootInfo bootinfo;
    functions.boot_media = NULL;
    Mpss4StackLinux temp( DEVNUM, functions );
    temp.setLoader( getLoader() );
    EXPECT_CALL( bootinfo, isValid() )
        .WillOnce( Return(true) );
    auto err = temp.deviceBoot<MockBootInfo, BootMgr, isAdmin<true> > ( bootinfo );
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SHARED_LIBRARY_ERROR ), err );
}

TEST_F(Mpss4StackLinuxTest, TC_deviceboot_notadmin_001)
{
    MockBootInfo bootinfo;
    EXPECT_CALL( bootinfo, isValid() )
        .WillOnce( Return(true) );
    // Save error code and use ASSERT later; googletest has issues when
    // passing two template arguments to deviceBoot<>
    auto err = mpss.deviceBoot<MockBootInfo, BootMgr, isAdmin<false>>(bootinfo);
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_NO_ACCESS ), err );
}

TEST_F(Mpss4StackLinuxTest, TC_deviceboot_success_bootlinux_001)
{
    MockBootInfo bootinfo;
    EXPECT_CALL( bootinfo, isValid() )
        .WillOnce( Return(true) );
    EXPECT_CALL( bootinfo, isCustom() )
        .WillOnce( Return(false) );
    // Save error code and use ASSERT later; googletest has issues when
    // passing two template arguments to deviceBoot<>
    auto err = mpss.deviceBoot<MockBootInfo, BootMgr, isAdmin<true>>(bootinfo);
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ), err );
}

TEST_F(Mpss4StackLinuxTest, TC_deviceboot_success_bootmedia_001)
{
    MockBootInfo bootinfo;
    EXPECT_CALL( bootinfo, isValid() )
        .WillOnce( Return(true) );
    EXPECT_CALL( bootinfo, isCustom() )
        .WillOnce( Return(true) );
    EXPECT_CALL( bootinfo,imagePath() )
        .Times(1)
        .WillOnce( Return("/lib/firmware/flash/dummy.hddimg") );
    // Save error code and use ASSERT later; googletest has issues when
    // passing two template arguments to deviceBoot<>
    auto err = mpss.deviceBoot<MockBootInfo, BootMgr, isAdmin<true> >(bootinfo);
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ), err );
}

TEST_F(Mpss4StackLinuxTest, TC_getdevstate_null_arg_001)
{
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ),
            mpss.getDeviceState(NULL) );
}

TEST_F(Mpss4StackLinuxTest, TC_getdevstate_null_getstate_001)
{
    functions.get_state = 0;
    Mpss4StackLinux temp( DEVNUM, functions );
    temp.setLoader( getLoader() );
    int state;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SHARED_LIBRARY_ERROR ),
            temp.getDeviceState(&state) );
}

TEST_F(Mpss4StackLinuxTest, TC_getdevstate_success_ready_001)
{
    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("get_state") )
        .WillOnce( Return((void*)(libmpss_get_state) my_get_state<eReady>) );
    // "Reload" library
    mpss.loadLib();
    int state;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getDeviceState(&state) );
    ASSERT_EQ(MpssStackBase::DeviceState::eStateReady, state);
}

TEST_F(Mpss4StackLinuxTest, TC_getdevstate_success_booting_001)
{
    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("get_state") )
        .WillOnce( Return((void*)(libmpss_get_state) my_get_state<eBooting>) );
    // "Reload" library
    mpss.loadLib();
    int state;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getDeviceState(&state) );
    ASSERT_EQ(MpssStackBase::DeviceState::eStateBooting, state);
}

TEST_F(Mpss4StackLinuxTest, TC_getdevstate_success_online_001)
{
    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("get_state") )
        .WillOnce( Return((void*)(libmpss_get_state) my_get_state<eOnline>) );
    // "Reload" library
    mpss.loadLib();
    int state;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getDeviceState(&state) );
    ASSERT_EQ(MpssStackBase::DeviceState::eStateOnline, state);
}

TEST_F(Mpss4StackLinuxTest, TC_getdevstate_success_invalid_001)
{
    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("get_state") )
        .WillOnce( Return((void*)(libmpss_get_state) my_get_state<eUnknown>) );
    // "Reload" library
    mpss.loadLib();
    int state;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getDeviceState(&state) );
    ASSERT_EQ(MpssStackBase::DeviceState::eStateInvalid, state);
}

TEST_F(Mpss4StackLinuxTest, TC_getdevstate_internalerror_invalid_001)
{
    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("get_state") )
        .WillOnce( Return((void*)(libmpss_get_state) my_get_state<eError>) );
    // "Reload" library
    mpss.loadLib();
    int state = -1;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR ),
            mpss.getDeviceState(&state) );
    ASSERT_EQ(MpssStackBase::DeviceState::eStateInvalid, state);
}

TEST_F(Mpss4StackLinuxTest, TC_devicereset_001)
{
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.deviceReset(false) );
}

TEST_F(Mpss4StackLinuxTest, TC_devicereset_null_resetnode_001)
{
    functions.reset_node = NULL;
    Mpss4StackLinux temp( DEVNUM, functions );
    temp.setLoader( getLoader() );
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SHARED_LIBRARY_ERROR ),
            temp.deviceReset(false) );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemdevtype_001)
{
    int type;
    const size_t number = 0;

    FakeStack stack;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getSystemDeviceType<FakeStack>(&type, number, stack) );
    ASSERT_EQ( MpssStackBase::DeviceType::eTypeKnl, type );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemdevtype_getsysprop_fail_001)
{
    int type;
    const size_t number = 0;

    FakeStack stack;
    // our fake stack will fail
    stack.pGetSystemProperty = sysProp<MICSDKERR_INTERNAL_ERROR>;

    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR ),
            mpss.getSystemDeviceType<FakeStack>(&type, number, stack) );
    ASSERT_EQ( MpssStackBase::DeviceType::eTypeUnknown, type );
}

TEST_F(Mpss4StackLinuxTest, TC_getsystemdevtype_devunknown_fail_001)
{
    int type;
    const size_t number = 0;

    FakeStack stack;
    // our fake stack will succeed, but device will be unknown
    stack.pGetSystemProperty = sysProp<0, false>;

    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getSystemDeviceType<FakeStack>(&type, number, stack) );
    ASSERT_EQ( MpssStackBase::DeviceType::eTypeUnknown, type );
}

// Parameterized test: googletest-provided GetParam() method will return (in this case)
// an std::vector<std::string> contaning valid mic card names.
TEST_P(Mpss4StackLinuxDeviceNumbersTest, TC_getsystemdevicenumbers_sucess_001)
{
    MockDirent dirent;
    void *dir = &dirent; // A dummy pointer to be returned by mock opendir

    // This dirent is the one that will be returned by our MockDirent
    struct dirent aDirent;
    bzero(&aDirent, sizeof(aDirent));
    std::vector<std::string> cards = GetParam();

    // A collection of dirent objects to be "populated" by our MockDirent
    std::vector<struct dirent> dirents(cards.size());
    // Calls to opendir will succeed
    EXPECT_CALL( dirent, opendir(_) )
        .WillOnce( Return((DIR*) &dir) )
        .WillOnce( Return((DIR*) &dir) );
    // Two opendirs happen, we expect two closedir
    EXPECT_CALL( dirent, closedir(_) )
        .Times(2);

    // Keep InSequence object scoped
    {
        InSequence seq;
        std::vector<std::string>::const_iterator mic;
        std::vector<struct dirent>::iterator d;
        for(mic = cards.begin(), d = dirents.begin();
            mic != cards.end() && d != dirents.end();
            ++mic, ++d)
        {
            // Copy card name to dirent::d_name
            strncpy(d->d_name, mic->c_str(), mic->size());
            EXPECT_CALL( dirent, readdir( NotNull() ) )
                .WillOnce( Return( &dirents[ std::distance(dirents.begin(), d) ] ) );
        }
        EXPECT_CALL( dirent, readdir( NotNull() ) )
            .WillOnce( Return((struct dirent*) NULL) );
    }

    std::vector<size_t> list;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getSystemDeviceNumbers(&list, dirent) );
    ASSERT_EQ(cards.size(), list.size());
}

INSTANTIATE_TEST_CASE_P(ValidCardNames,
        Mpss4StackLinuxDeviceNumbersTest,
        ::testing::Values(
            std::vector<std::string>( {"mic0"} ),
            std::vector<std::string>( {"mic10", "mic12", "mic14"} ),
            std::vector<std::string>( {"mic0", "mic1"} )
        ));

TEST_F(Mpss4StackLinuxDeviceNumbersTest, TC_getsystemdevice_numbers_nodevs_001)
{
    MockDirent dirent;
    void *dir = &dirent; // A dummy pointer to be returned by mock opendir

    // This dirent is the one that will be returned by our MockDirent
    struct dirent aDirent;
    bzero(&aDirent, sizeof(aDirent));

    std::vector<std::string> invalidNames({"michael", "mica0", "what", "MIC0"});
    std::vector<struct dirent> dirents(invalidNames.size());
    // Calls to opendir will succeed
    EXPECT_CALL( dirent, opendir(_) )
        .WillOnce( Return((DIR*) &dir) )
        .WillOnce( Return((DIR*) &dir) );
    // Two opendirs happen, we expect to closedir
    EXPECT_CALL( dirent, closedir(_) )
        .Times(2);

    // Keep InSequence object scoped
    {
        InSequence seq;
        std::vector<std::string>::const_iterator name;
        std::vector<struct dirent>::iterator d;
        for(name = invalidNames.begin(), d = dirents.begin();
            name != invalidNames.end() && d != dirents.end();
            ++name, ++d)
        {
            strncpy(d->d_name, name->c_str(), name->size());
            EXPECT_CALL( dirent, readdir( NotNull() ) )
                .WillOnce( Return( &dirents[ std::distance(dirents.begin(), d) ] ) );
        }
        EXPECT_CALL( dirent, readdir( NotNull() ) )
            .WillOnce( Return((struct dirent*) NULL) );
    }

    std::vector<size_t> list;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ),
            mpss.getSystemDeviceNumbers(&list, dirent) );
    ASSERT_EQ(0, list.size());
}

TEST_P(Mpss4StackLinuxDevPropTest, TC_getdevprop_dmifolder_001)
{
    const std::string prop(GetParam());
    std::string data;
    auto err = mpss.getDeviceProperty<MockIOStream, fakeParseFunction>( &data, prop, stream ) ;
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ), err );
}

INSTANTIATE_TEST_CASE_P(AllProps,
        Mpss4StackLinuxDevPropTest,
        ::testing::Values(
            std::string(PROPKEY_BIOS_VERSION),
            std::string(PROPKEY_BIOS_RELDATE),
            std::string(PROPKEY_CPU_PARTNUMBER),
            std::string(PROPKEY_CPU_VENDOR),
            std::string(PROPKEY_OEM_STRINGS),
            std::string(PROPKEY_SYS_SKU),
            std::string(PROPKEY_ME_VERSION),
            std::string(PROPKEY_SMC_VERSION),
            std::string(PROPKEY_MCDRAM_VERSION),
            std::string(PROPKEY_BOARD_TYPE),
            std::string(PROPKEY_POST_CODE),
            std::string(PROPKEY_PROC_MODEL),
            std::string(PROPKEY_PROC_TYPE),
            std::string(PROPKEY_PROC_FAMILY),
            std::string(PROPKEY_PROC_STEPPING),
            std::string(PROPKEY_DEVICE_TYPE),
            std::string("spad4")
            ));

TEST_F(Mpss4StackLinuxDevPropTest, TC_getdevprop_nulldata_001)
{
    const std::string prop = "prop";
    auto err = mpss.getDeviceProperty<MockIOStream, fakeParseFunction>( NULL, prop, stream);
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ), err );
}

TEST_F(Mpss4StackLinuxDevPropTest, TC_getdevprop_emptyname_001)
{
    std::string data;
    const std::string prop = "";
    auto err = mpss.getDeviceProperty<MockIOStream, fakeParseFunction>( &data, prop, stream);
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ), err );
}

TEST_F(Mpss4StackLinuxDevPropTest, TC_getdevprop_open_fail_001)
{
    std::string data;
    const std::string prop = PROPKEY_BIOS_VERSION; // Any property...

    EXPECT_CALL( stream, is_open() )
        .WillRepeatedly( Return(false) );

    errno = EACCES;
    {
        auto err = mpss.getDeviceProperty<MockIOStream, fakeParseFunction>( &data, prop, stream);
        ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_NO_ACCESS ), err );
    }

    errno = 0;
    {
        auto err = mpss.getDeviceProperty<MockIOStream, fakeParseFunction>( &data, prop, stream);
        ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_PROPERTY_NOT_FOUND ), err );
    }
}

TEST_F(Mpss4StackLinuxDevPropTest, TC_getdevprop_stream_fail_001)
{
    std::string data;
    const std::string prop = PROPKEY_BIOS_VERSION; // Any property...

    EXPECT_CALL( stream, fail() )
        .WillRepeatedly( Return(true) );

    auto err = mpss.getDeviceProperty<MockIOStream, fakeParseFunction>( &data, prop, stream);
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR ), err );
}

TEST_F(Mpss4StackLinuxDevPropTest, TC_getdevprop_parsefunction_fail_001)
{
    std::string data;
    // Request a property that needs additional parsing,
    // i.e. that it will require calling fakeParseFunction
    const std::string prop = PROPKEY_PROC_MODEL;

    // Note fakeParseFunction returns error
    auto err = mpss.getDeviceProperty<MockIOStream, fakeParseFunction<-1>>( &data, prop, stream);
    ASSERT_EQ( MicDeviceError::errorCode( -1 ), err );
}

} // namespace micmgmt
