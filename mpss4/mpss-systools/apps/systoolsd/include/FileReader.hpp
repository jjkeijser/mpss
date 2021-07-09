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
#ifndef SYSTOOLS_SYSTOOLSD_FILEREADER_HPP_
#define SYSTOOLS_SYSTOOLSD_FILEREADER_HPP_

#include <ios>

class FileReader
{
public:
    virtual ~FileReader() { }

    //non-const member functions
    virtual void open(const char *path, std::ios_base::openmode mode = std::ios_base::in) = 0;
    virtual void open(const std::string &path, std::ios_base::openmode mode = std::ios_base::in) = 0;
    virtual void clear() = 0;
    virtual void close() = 0;

    virtual FileReader &getline(char *s, size_t n) = 0;
    virtual FileReader &seekg(std::streampos pos) = 0;;
    virtual FileReader &seekg(std::streamoff off, std::ios_base::seekdir way) = 0;
    virtual FileReader &read(char *s, std::streamsize n) = 0;

    virtual std::streampos tellg() = 0;

    //const member functions
    virtual bool good() const = 0;
    virtual bool is_open() const = 0;

};

template <class T>
class FileReaderStream : public FileReader
{
public:
    FileReaderStream() { }
    virtual ~FileReaderStream() { }

    virtual void open(const char *path, std::ios_base::openmode mode = std::ios_base::in);
    virtual void open(const std::string &path, std::ios_base::openmode mode = std::ios_base::in);
    virtual void clear();
    virtual void close();

    virtual FileReader &getline(char *s, size_t n);
    virtual FileReader &seekg(std::streampos pos);
    virtual FileReader &seekg(std::streamoff off, std::ios_base::seekdir way);
    virtual FileReader &read(char *s, std::streamsize n);

    virtual std::streampos tellg();

    virtual bool good() const;
    virtual bool is_open() const;

private:
    FileReaderStream &operator=(const FileReaderStream&);
    FileReaderStream(const FileReaderStream&);

    T m_stream;
};

template <class T>
void FileReaderStream<T>::open(const char *path, std::ios_base::openmode mode)
{
    m_stream.open(path, mode);
}

template <class T>
void FileReaderStream<T>::open(const std::string &path, std::ios_base::openmode mode)
{
    m_stream.open(path, mode);
}

template <class T>
FileReader &FileReaderStream<T>::getline(char *s, size_t n)
{
    m_stream.getline(s, n);
    return *this;
}

template <class T>
FileReader &FileReaderStream<T>::seekg(std::streampos pos)
{
    m_stream.seekg(pos);
    return *this;
}

template <class T>
FileReader &FileReaderStream<T>::seekg(std::streamoff off, std::ios_base::seekdir way)
{
    m_stream.seekg(off, way);
    return *this;
}

template <class T>
FileReader &FileReaderStream<T>::read(char *s, std::streamsize n)
{
    m_stream.read(s, n);
    return *this;
}

template <class T>
std::streampos FileReaderStream<T>::tellg()
{
    return m_stream.tellg();
}

template <class T>
void FileReaderStream<T>::clear()
{
    m_stream.clear();
}

template <class T>
void FileReaderStream<T>::close()
{
    m_stream.close();
}

template <class T>
bool FileReaderStream<T>::good() const
{
    return m_stream.good();
}

template <class T>
bool FileReaderStream<T>::is_open() const
{
    return m_stream.is_open();
}

#endif // SYSTOOLS_SYSTOOLSD_FILEREADER_HPP_
