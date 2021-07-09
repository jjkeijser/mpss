#ifndef SYSTOOLS_SDK_KNLCUSTOMBOOTMANAGER_HPP
#define SYSTOOLS_SDK_KNLCUSTOMBOOTMANAGER_HPP

#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Filesystem.hpp"
#include "MicLogger.hpp"

namespace
{
    const std::string FIRMWARE_DIR = "/lib/firmware";
    const std::string SYSTOOLS_FIRMWARE_DIR = "systools_flash";
    const std::string FULL_SYSTOOLS_FIRMWARE_DIR =
        FIRMWARE_DIR + "/" + SYSTOOLS_FIRMWARE_DIR;
}

namespace micmgmt
{

class CustomBootManager
{
public:
    virtual ~CustomBootManager() { }
    virtual std::string fwRelPath() const = 0;
    virtual std::string fwAbsPath() const = 0;
};

template <class Filesystem>
class KnlCustomBootManagerT : public CustomBootManager
{
public:
    KnlCustomBootManagerT( Filesystem *fs, const std::string& deviceName,
                           const std::string& path );
    KnlCustomBootManagerT( KnlCustomBootManagerT &&other );
    virtual ~KnlCustomBootManagerT();

    std::string fwRelPath() const;
    std::string fwAbsPath() const;

    static std::shared_ptr<KnlCustomBootManagerT> create( const std::string& deviceName, const std::string& path );

protected:
    void createDirectory();
    void createSymlinks();
    void cleanUp();


private:
    std::unique_ptr<Filesystem> m_fs;
    std::string m_deviceName;
    std::string m_path;
    std::string m_fwRelPath;
    std::string m_fwAbsPath;
    bool m_dirCreated;
    bool m_symlinkCreated;

private:
    KnlCustomBootManagerT(const KnlCustomBootManagerT&) = delete;
    KnlCustomBootManagerT &operator=(const KnlCustomBootManagerT&) = delete;
};

///////////////////////////////////////////////////////////////////////////////////////////
//
// START IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////////////////

template <class Filesystem>
KnlCustomBootManagerT<Filesystem>::KnlCustomBootManagerT( Filesystem *fs,
        const std::string &deviceName, const std::string& path ) :
    m_fs(fs), m_deviceName(deviceName), m_path(path), m_dirCreated(false),
    m_symlinkCreated(false)
{
    std::vector<char> resolvedName(PATH_MAX);
    if ( m_fs->realpath( path.c_str(), &resolvedName[0] ) == nullptr )
    {
        LOG( ERROR_MSG, "realpath() failed" );
        throw std::runtime_error("realpath()");
    }

    m_path = &resolvedName[0];
    try{
        createDirectory();
        createSymlinks();
    }
    catch(const std::runtime_error& err)
    {
        (void)err; // -Wall
        cleanUp();
        throw;
    }
}

template <class Filesystem>
KnlCustomBootManagerT<Filesystem>::KnlCustomBootManagerT( KnlCustomBootManagerT &&other ) :
    m_fs(std::move(other.m_fs)),
    m_deviceName(std::move(other.m_deviceName)), m_path(std::move(other.m_path)),
    m_fwRelPath(std::move(other.m_fwRelPath)), m_fwAbsPath(std::move(other.m_fwAbsPath)),
    m_dirCreated(other.m_dirCreated), m_symlinkCreated(other.m_symlinkCreated)
{
    other.m_deviceName = other.m_path = other.m_fwRelPath = other.m_fwAbsPath = "";
    other.m_dirCreated = other.m_symlinkCreated = false;
}

template <class Filesystem>
KnlCustomBootManagerT<Filesystem>::~KnlCustomBootManagerT()
{
    cleanUp();
}

template<class Filesystem>
std::shared_ptr<KnlCustomBootManagerT<Filesystem>> KnlCustomBootManagerT<Filesystem>::create( const std::string& deviceName, const std::string& path )
{
    return std::make_shared<KnlCustomBootManagerT>(
            static_cast<KnlCustomBootManagerT> (
                KnlCustomBootManagerT( new Filesystem, deviceName, path ) ) );
}


template <class Filesystem>
std::string KnlCustomBootManagerT<Filesystem>::fwRelPath() const
{
    return m_fwRelPath;
}

template <class Filesystem>
std::string KnlCustomBootManagerT<Filesystem>::fwAbsPath() const
{
    return m_fwAbsPath;
}

template <class Filesystem>
void KnlCustomBootManagerT<Filesystem>::createDirectory()
{
    // if errno == EEXIST, another instance already created it
    if ( m_fs->mkdir( FULL_SYSTOOLS_FIRMWARE_DIR.c_str(), S_IRWXU ) == -1 &&
         errno != EEXIST )
    {
        LOG( ERROR_MSG, "mkdir() failed: %d : %s", errno, strerror(errno) );
        throw std::runtime_error("mkdir()");
    }
    m_dirCreated = true;
}

template <class Filesystem>
void KnlCustomBootManagerT<Filesystem>::createSymlinks()
{
    // basename modifies the string passed to it, so copy it
    std::vector<char> buf(m_path.size() + 1); // for null terminator
    std::strcpy( &buf[0], m_path.c_str() );

    std::string fileName = m_fs->basename(&buf[0]);
    // The actual symlink will be named in the form:
    // <file basename>_micN
    std::string linkForDevice = fileName + "_" + m_deviceName;

    // Now, build the symlink
    std::string linkPath = FIRMWARE_DIR + "/" + SYSTOOLS_FIRMWARE_DIR + "/";
    linkPath += linkForDevice;

    // So, our symlink will be something like:
    // /lib/firmware/systools_flash/<some_file_name.ext>_micN
    m_fs->unlink(linkPath.c_str()); //Make sure no dups!
    if ( m_fs->symlink( m_path.c_str(), linkPath.c_str() ) == -1 )
    {
        LOG( ERROR_MSG, "symlink() failed: %d : %s", errno, strerror(errno) );
        throw std::runtime_error("symlink()");
    }
    m_fwAbsPath = linkPath;
    m_fwRelPath = SYSTOOLS_FIRMWARE_DIR + "/" + linkForDevice;
    m_symlinkCreated = true;
}

template <class Filesystem>
void KnlCustomBootManagerT<Filesystem>::cleanUp()
{
    if ( m_symlinkCreated && (m_fs->unlink(m_fwAbsPath.c_str()) == -1) )
        LOG( WARNING_MSG, "failed to remove symlink(): %d : %s",
             errno, strerror(errno) );

    if ( m_dirCreated &&
       ( m_fs->rmdir(FULL_SYSTOOLS_FIRMWARE_DIR.c_str()) == -1 &&
         errno != ENOTEMPTY ) )
        LOG( WARNING_MSG, "failed to remove directory(): %d : %s",
             errno, strerror(errno) );
}

typedef KnlCustomBootManagerT<FilesystemUtils> KnlCustomBootManager;

}

#endif // SYSTOOLS_SDK_KNLCUSTOMBOOTMANAGER_HPP
