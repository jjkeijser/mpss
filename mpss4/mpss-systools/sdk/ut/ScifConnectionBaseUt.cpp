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
#include <string>

// SCIF INCLUDES
//
#include <scif.h>

// UT INCLUDES
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mocks.hpp"


// PROJECT INCLUDES
//
#include "MicDeviceError.hpp"
#include "ScifConnectionBase.hpp"
#include "ScifFunctions.hpp"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

namespace
{
    const int DEVNUM = 2;
    const int PORTNUM = 4444;
    const int HANDLE = 7;
    const bool ONLINE = true;
    const std::string ERROR_TEXT("this is a nasty SCIF error");

    const std::string LIB_NAME("libmicdevice");
    const std::string LIB_ERROR("lib error");
    const std::string LIB_VERSION("1.2.3");


    // Function templates that will "agree" with libscif signatures
    template <int ret=HANDLE>
    int my_scif_open(void)
    {
        return ret;
    }

    template <int ret=0>
    int my_scif_close(scif_epd_t epd)
    {
        (void)epd;
        return ret;
    }

    template <int ret=PORTNUM>
    int my_scif_bind(scif_epd_t epd, uint16_t pn)
    {
        (void)epd;
        (void)pn;
        if(ret == -1)
            return ret;
        return pn;
    }

    template <int ret=PORTNUM>
    int my_scif_connect(scif_epd_t epd, scif_portID* dst)
    {
        (void)epd;
        (void)dst;
        return ret;
    }

    template <int ret=0, int retsent=-1>
    int my_scif_send(scif_epd_t epd, void* msg, int len, int flags)
    {
        (void)epd;
        (void)msg;
        (void)flags;
        // Do nothing
        if(ret == -1)
            return ret;

        // When retsent != -1 (default), return retsent; otherwise return len argument
        if(retsent == -1)
            return len;

        return retsent;
    }

    template <int ret=0, int retsent=-1>
    int my_scif_recv(scif_epd_t epd, void* msg, int len, int flags)
    {
        (void)epd;
        (void)flags;
        if(ret == -1)
            return -1;

        // If retsent is specified, copy only that much
        if(retsent != -1)
            len = retsent;

        const std::string dummy(static_cast<size_t>(len), 'A');
        memcpy(msg, dummy.c_str(), static_cast<size_t>(len));
        return len;
    }

}

namespace micmgmt
{

class TestScifConnection : public ScifConnectionBase
{
public:
    TestScifConnection( int devnum ) : ScifConnectionBase( devnum )
    {
        // Nothing to do
    }

    TestScifConnection( int devnum, int portnum, int handle, bool online,
            std::string errorText, ScifFunctions& functions ) :
        ScifConnectionBase( devnum, portnum, handle, online, errorText, functions )
    {
        // Nothing to do
    }

    // We don't care about this method
    virtual uint32_t request( ScifRequestInterface* request )
    {
        (void)request;
        return 0;
    }
};

class ScifConnectionBaseTest : public ::testing::Test
{
public:
    ScifConnectionBaseTest( ) :
        devnum(DEVNUM), portnum(PORTNUM), handle(HANDLE), online(ONLINE),
        errorText(ERROR_TEXT), functions()
    {
    }

protected:
    virtual void SetUp()
    {
        // "valid" functions
        functions.open = my_scif_open;
        functions.close = my_scif_close;
        functions.bind = my_scif_bind;
        functions.connect = my_scif_connect;
        functions.send = my_scif_send;
        functions.recv = my_scif_recv;
    }

    MockLoader* getLoader()
    {
        MockLoader* loader = new MockLoader;
        setLoaderDefaults(*loader);
        return loader;
    }

    void setLoaderDefaults( MockLoader& someLoader )
    {
        ON_CALL( someLoader, isLoaded() ).WillByDefault( Return(true) );
        ON_CALL( someLoader, fileName() ).WillByDefault( Return(LIB_NAME) );
        ON_CALL( someLoader, errorText() ).WillByDefault( Return(LIB_ERROR) );
        ON_CALL( someLoader, version() ).WillByDefault( Return(LIB_VERSION) );
        ON_CALL( someLoader, unload() ).WillByDefault( Return(true) );
        ON_CALL( someLoader, load(_) ).WillByDefault( Return(true) );
        ON_CALL( someLoader, lookup("scif_open") )
            .WillByDefault( Return((void*)(ScifFunctions::scif_open) my_scif_open) );
        ON_CALL( someLoader, lookup("scif_close") ).
            WillByDefault( Return((void*)(ScifFunctions::scif_close) my_scif_close) );
        ON_CALL( someLoader, lookup("scif_bind") ).
            WillByDefault( Return((void*)(ScifFunctions::scif_bind) my_scif_bind) );
        ON_CALL( someLoader, lookup("scif_connect") ).
            WillByDefault( Return((void*)(ScifFunctions::scif_connect) my_scif_connect) );
        ON_CALL( someLoader, lookup("scif_send") ).
            WillByDefault( Return((void*)(ScifFunctions::scif_send) my_scif_send) );
        ON_CALL( someLoader, lookup("scif_recv") ).
            WillByDefault( Return((void*)(ScifFunctions::scif_recv) my_scif_recv) );
    }

protected:
    int devnum;
    int portnum;
    int handle;
    bool online;
    std::string errorText;
    ScifFunctions functions;
};

TEST_F(ScifConnectionBaseTest, TC_isopen_001)
{
    {
        //non-zero handle
        TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
        ASSERT_TRUE(test.isOpen());
    }

    {
        //zero
        TestScifConnection test(devnum, portnum, 0, online, errorText, functions);
        ASSERT_FALSE(test.isOpen());

    }
}

TEST_F(ScifConnectionBaseTest, TC_devnum_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    ASSERT_EQ(DEVNUM, test.deviceNum());
}

TEST_F(ScifConnectionBaseTest, TC_errortext_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    ASSERT_EQ(ERROR_TEXT, test.errorText());
}

TEST_F(ScifConnectionBaseTest, TC_portnum_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    ASSERT_EQ(PORTNUM, test.portNum());
}

TEST_F(ScifConnectionBaseTest, TC_open_already_open_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    test.setLoader(getLoader());
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ), test.open() );
}

TEST_F(ScifConnectionBaseTest, TC_open_fail_loading_001)
{
    // Pass handle=0 so isOpen() returns false
    handle = 0;
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);

    MockLoader* loader = getLoader();
    EXPECT_CALL( *loader, isLoaded() ).WillRepeatedly( Return(false) );
    EXPECT_CALL( *loader, lookup(_) ).WillRepeatedly( Return((void*)NULL) );
    test.setLoader(loader);
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SHARED_LIBRARY_ERROR ), test.open() );
}

TEST_F(ScifConnectionBaseTest, TC_open_scifopen_fail_001)
{
    handle = 0;
    MockLoader* loader = getLoader();
    // Allow lookup() to be called many times
    EXPECT_CALL( *loader, lookup(_) ).Times( AtLeast(1) );
    // ... but when called with "scif_open", return a failing function
    EXPECT_CALL( *loader, lookup("scif_open") )
        .WillOnce( Return( (void*)(ScifFunctions::scif_open) my_scif_open<-1> ) );
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    test.setLoader(loader);
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR ), test.open() );
}

TEST_F(ScifConnectionBaseTest, TC_open_scifbind_fail_001)
{
    handle = 0;
    MockLoader* loader = getLoader();
    EXPECT_CALL( *loader, lookup(_) ).Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("scif_bind") )
        .WillOnce( Return( (void*)(ScifFunctions::scif_bind) my_scif_bind<-1> ) );
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    test.setLoader( loader );
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR ), test.open() );
}

TEST_F(ScifConnectionBaseTest, TC_open_scifconnect_fail_001)
{
    handle = 0;
    {
        // Set errno, errorText() should notify about "connection refused"
        errno = ECONNREFUSED;
        MockLoader* loader = getLoader();
        EXPECT_CALL( *loader, lookup(_) ).Times( AtLeast(1) );
        EXPECT_CALL( *loader, lookup("scif_connect") )
            .WillOnce( Return( (void*)(ScifFunctions::scif_connect) my_scif_connect<-1> ) );
        TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
        test.setLoader( loader );
        ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR ), test.open() );
        // Ensure that error message states that connection was refused
        ASSERT_NE( std::string::npos, test.errorText().find("refused") );
    }

    errno = 0;

    {
        MockLoader* loader = getLoader();
        EXPECT_CALL( *loader, lookup(_) ).Times( AtLeast(1) );
        EXPECT_CALL( *loader, lookup("scif_connect") )
            .WillOnce( Return( (void*)(ScifFunctions::scif_connect) my_scif_connect<-1> ) );
        TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
        test.setLoader( loader );
        ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_DEVICE_IO_ERROR ), test.open() );
        // Ensure error message does not state that connectio was refused
        ASSERT_EQ( std::string::npos, test.errorText().find("refused") );
    }
}

TEST_F(ScifConnectionBaseTest, TC_open_success_001)
{
    handle = 0;
    MockLoader* loader = getLoader();
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    test.setLoader( loader );
    ASSERT_EQ( MicDeviceError::errorCode( MICSDKERR_SUCCESS ), test.open() );
}

TEST_F(ScifConnectionBaseTest, TC_close_isopen_true_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    test.close();
}

TEST_F(ScifConnectionBaseTest, TC_close_isopen_false_001)
{
    handle = 0;
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    test.close();
}

TEST_F(ScifConnectionBaseTest, TC_lock_unlock_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    ASSERT_TRUE( test.lock() );
    ASSERT_TRUE( test.unlock() );
}

TEST_F(ScifConnectionBaseTest, TC_public_ctor_001)
{
    TestScifConnection test(devnum);
    ASSERT_EQ( false, test.isOpen() );
    ASSERT_EQ( DEVNUM, test.deviceNum() );
    ASSERT_EQ( "", test.errorText() );
}

TEST_F(ScifConnectionBaseTest, TC_send_001)
{
    const size_t size = 16;
    char buf[size];
    bzero(buf, size);
    functions.send = my_scif_send;
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    ASSERT_EQ(0, test.send(buf, size));
}

TEST_F(ScifConnectionBaseTest, TC_send_fail_001)
{
    const size_t size = 16;
    char buf[size];
    bzero(buf, size);
    functions.send = my_scif_send<-1>;
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    ASSERT_EQ(-1, test.send(buf, size));
}

TEST_F(ScifConnectionBaseTest, TC_send_fail_econnreset_fail_001)
{
    // Special case: when send() fails and errno == ECONNRESET, the connection will be closed
    // and open() will get called again. This means that the SharedLibrary will perform lookup()
    // of symbols again. So we set up a MockLoader for this.
    errno = ECONNRESET;
    const size_t size = 16;
    char buf[size];
    bzero(buf, size);
    // This version of my_scif_send() will return -1 and ECONNRESET will be checked
    functions.send = my_scif_send<-1>;

    // Set up the MockLoader
    MockLoader* loader = getLoader();
    EXPECT_CALL( *loader, lookup(_) ).Times( AtLeast(1) );

    // The new function returned by our MockLoader will also fail
    EXPECT_CALL( *loader, lookup("scif_send") )
        .WillOnce( Return( (void*)(ScifFunctions::scif_send) my_scif_send<-1> ) );

    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    test.setLoader(loader);
    EXPECT_EQ(-1, test.send(buf, size));
    errno = 0;
}

TEST_F(ScifConnectionBaseTest, TC_send_fail_econnreset_success_001)
{
    // Special case: when send() fails and errno == ECONNRESET, the connection will be closed,
    // and open() will get called again. This means that the SharedLibrary will perform lookup()
    // of symbols again. So we set up a MockLoader for this.
    errno = ECONNRESET;
    const size_t size = 16;
    char buf[size];
    bzero(buf, size);
    // This version of my_scif_send() will return -1 and ECONNRESET will be checked
    functions.send = my_scif_send<-1>;

    // Set up the MockLoader
    MockLoader* loader = getLoader();
    EXPECT_CALL( *loader, lookup(_) ).Times( AtLeast(1) );

    // The new function returned by our MockLoader will succeed
    EXPECT_CALL( *loader, lookup("scif_send") )
        .WillOnce( Return( (void*)(ScifFunctions::scif_send) my_scif_send ) );

    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    test.setLoader(loader);
    EXPECT_EQ(0, test.send(buf, size));
    errno = 0;
}

TEST_F(ScifConnectionBaseTest, TC_send_fail_econnreset_reopen_fail_001)
{
    // Special case: when send() fails and errno == ECONNRESET, the connection will be closed,
    // and open() will get called again. This means that the SharedLibrary will perform lookup()
    // of symbols again. So we set up a MockLoader for this.
    errno = ECONNRESET;
    const size_t size = 16;
    char buf[size];
    bzero(buf, size);
    // This version of my_scif_send() will return -1 and ECONNRESET will be checked
    functions.send = my_scif_send<-1>;

    // Set up the MockLoader
    MockLoader* loader = getLoader();
    // Return NULL so that reopen() fails
    EXPECT_CALL( *loader, lookup(_) )
        .WillRepeatedly( Return( (void*)NULL ) );

    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    test.setLoader(loader);
    EXPECT_EQ(-1, test.send(buf, size));
    errno = 0;
}

TEST_F(ScifConnectionBaseTest, TC_recv_001)
{
    const size_t size = 16;
    char buf[size];
    bzero(buf, size);
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    ASSERT_EQ(0, test.receive(buf, size));
}

TEST_F(ScifConnectionBaseTest, TC_recv_fail_001)
{
    const size_t size = 16;
    char buf[size];
    bzero(buf, size);
    functions.recv = my_scif_recv<-1>;
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    ASSERT_EQ(-1, test.receive(buf, size));
}

TEST_F(ScifConnectionBaseTest, TC_setloader_null_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    ASSERT_EQ( static_cast<int>( MicDeviceError::errorCode( MICSDKERR_INVALID_ARG ) ),
            test.setLoader(NULL) );
}

#ifdef __linux__
TEST_F(ScifConnectionBaseTest, TC_systemerrno_001)
{
    errno = 0;
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    ASSERT_EQ( 0, test.systemErrno() );
}
#endif // __linux__

TEST_F(ScifConnectionBaseTest, TC_loadlib_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    MockLoader* loader = getLoader();
    test.setLoader( loader );
    ASSERT_TRUE( test.loadLib() );
}

TEST_F(ScifConnectionBaseTest, TC_loadlib_notloaded_load_success_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    MockLoader* loader = getLoader();
    // Library is not loaded
    EXPECT_CALL( *loader, isLoaded() )
        .WillOnce( Return(false) );
        //.WillOnce( Return(true) ); // On destruction, return true FIXME: Not called on destruction

    EXPECT_CALL( *loader, setFileName(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, setVersion(_, _, _) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, load(_) )
        .WillOnce( Return(true) );

    test.setLoader( loader );
    ASSERT_TRUE( test.loadLib() );
}

TEST_F(ScifConnectionBaseTest, TC_loadlib_notloaded_load_fail_001)
{
    const std::string errmsg("expect this error");
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    MockLoader* loader = getLoader();
    // Library is not loaded
    EXPECT_CALL( *loader, isLoaded() )
        .WillOnce( Return(false) );
        //.WillOnce( Return(false) ); // Will be called on destruction FIXME: Not called on destruction

    EXPECT_CALL( *loader, setFileName(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, setVersion(_, _, _) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, load(_) )
        .WillOnce( Return(false) ); // Loader will fail to load the library
    EXPECT_CALL( *loader, errorText() )
        .WillOnce( Return(errmsg) );

    test.setLoader( loader );
    ASSERT_FALSE( test.loadLib() );
    ASSERT_EQ( errmsg, test.errorText() );
}

TEST_F(ScifConnectionBaseTest, TC_loadlib_lookup_scifopen_fail_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    MockLoader* loader = getLoader();

    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("scif_open") )
        .WillOnce( Return( (void*) NULL ) );
    test.setLoader( loader );
    ASSERT_FALSE( test.loadLib() );
    ASSERT_NE( "", test.errorText() );
}

TEST_F(ScifConnectionBaseTest, TC_loadlib_lookup_scifclose_fail_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    MockLoader* loader = getLoader();

    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("scif_close") )
        .WillOnce( Return( (void*) NULL ) );
    test.setLoader( loader );
    ASSERT_FALSE( test.loadLib() );
    ASSERT_NE( "", test.errorText() );
}

TEST_F(ScifConnectionBaseTest, TC_loadlib_lookup_scifbind_fail_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    MockLoader* loader = getLoader();

    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("scif_bind") )
        .WillOnce( Return( (void*) NULL ) );
    test.setLoader( loader );
    ASSERT_FALSE( test.loadLib() );
    ASSERT_NE( "", test.errorText() );
}

TEST_F(ScifConnectionBaseTest, TC_loadlib_lookup_scifconnect_fail_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    MockLoader* loader = getLoader();

    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("scif_connect") )
        .WillOnce( Return( (void*) NULL ) );
    test.setLoader( loader );
    ASSERT_FALSE( test.loadLib() );
    ASSERT_NE( "", test.errorText() );
}

TEST_F(ScifConnectionBaseTest, TC_loadlib_lookup_scifsend_fail_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    MockLoader* loader = getLoader();

    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("scif_send") )
        .WillOnce( Return( (void*) NULL ) );
    test.setLoader( loader );
    ASSERT_FALSE( test.loadLib() );
    ASSERT_NE( "", test.errorText() );
}

TEST_F(ScifConnectionBaseTest, TC_loadlib_lookup_scifrecv_fail_001)
{
    TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
    MockLoader* loader = getLoader();

    EXPECT_CALL( *loader, lookup(_) )
        .Times( AtLeast(1) );
    EXPECT_CALL( *loader, lookup("scif_recv") )
        .WillOnce( Return( (void*) NULL ) );
    test.setLoader( loader );
    ASSERT_FALSE( test.loadLib() );
    ASSERT_NE( "", test.errorText() );
}

TEST_F(ScifConnectionBaseTest, TC_unloadlib_001)
{
    // When library is loaded
    {
        TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
        MockLoader* loader = getLoader();

        EXPECT_CALL( *loader, isLoaded() )
            .WillOnce( Return(true) );
            //.WillOnce( Return(false) ); // On destruction FIXME: Not called on destruction
        EXPECT_CALL( *loader, unload() )
            .Times(1);
        test.setLoader(loader);
        test.unloadLib();
    }

    // When library is not loaded
    {
        TestScifConnection test(devnum, portnum, handle, online, errorText, functions);
        MockLoader* loader = getLoader();

        EXPECT_CALL( *loader, isLoaded() )
            .WillRepeatedly( Return(false) );
        EXPECT_CALL( *loader, unload() )
            .Times(0);
        test.setLoader(loader);
        test.unloadLib();
    }
}

} // namespace micmgmt
