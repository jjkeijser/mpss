#ifndef SYSTOOLS_FILESYSTEM_HPP
#define SYSTOOLS_FILESYSTEM_HPP

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class FilesystemUtils
{
public:
    FilesystemUtils()
    {
        // Nothing to do
    }

    int mkdir( const char *pathname, mode_t mode )
    {
        return ::mkdir( pathname, mode );
    }

    int symlink( const char *target, const char *linkpath )
    {
        return ::symlink( target, linkpath );
    }

    // Beware: resolved_name must be a buffer of PATH_MAX bytes, defined in limits.h
    char *realpath( const char *filename, char *resolvedName )
    {
        return ::realpath( filename, resolvedName );
    }

    char *basename(char *path)
    {
        return ::basename(path);
    }

    int rmdir(const char *path)
    {
        return ::rmdir(path);
    }

    int unlink(const char *path)
    {
        return ::unlink(path);
    }
};

#endif // SYSTOOLS_FILESYSTEM_HPP
