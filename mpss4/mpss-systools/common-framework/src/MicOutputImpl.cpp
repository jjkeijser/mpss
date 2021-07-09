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

#include "MicOutputImpl.hpp"
#include "ConsoleOutputFormatter.hpp"
#include "MicException.hpp"
#include <iostream>

namespace micmgmt
{
    using namespace std;

    MicOutputImpl::MicOutputImpl(const std::string& toolName, MicOutputFormatterBase* stdOut, MicOutputFormatterBase* stdErr,
                                 MicOutputFormatterBase* fileOut)
        : silentMode_(false), fileOut_(NULL), errorFormatter_(toolName)
    {
        stdOut_ = unique_ptr<ConsoleOutputFormatter>(new ConsoleOutputFormatter(&cout));
        stdErr_ = unique_ptr<ConsoleOutputFormatter>(new ConsoleOutputFormatter(&cerr));
        if (stdOut == NULL)
        {
            standardOut_ = stdOut_.get();
        }
        else
        {
            standardOut_ = stdOut;
        }
        if (stdErr == NULL)
        {
            standardErr_ = stdErr_.get();
        }
        else
        {
            standardErr_ = stdErr;
        }
        if (fileOut != NULL)
        {
            fileOut_ = fileOut;
        }
    }

    MicOutputImpl::~MicOutputImpl()
    {
    }

    void MicOutputImpl::outputLine(const string& line, bool suppressInternalWSReduction, bool flush)
    {
        lock_guard<mutex> lock(outputMutex_);
        try
        {
            if (silentMode_ == false)
            {
                standardOut_->outputLine(line, suppressInternalWSReduction, flush);
            }
            if (fileOut_)
            {
                fileOut_->outputLine(line, suppressInternalWSReduction, flush);
            }
        }
        catch (std::exception& e)
        {
            auto info = errorFormatter_.buildError(MicOutputErrorCode::eMicOutputOutputError, e.what());
            throw MicException(info.first, info.second);
        }
    }

    void MicOutputImpl::outputNameValuePair(const string& name, const string& valueStr, const string& units)
    {
        lock_guard<mutex> lock(outputMutex_);
        try
        {
            if (silentMode_ == false)
            {
                standardOut_->outputNameValuePair(name, valueStr, units);
            }
            if (fileOut_)
            {
                fileOut_->outputNameValuePair(name, valueStr, units);
            }
        }
        catch (std::exception& e)
        {
            auto info = errorFormatter_.buildError(MicOutputErrorCode::eMicOutputOutputError, e.what());
            throw MicException(info.first, info.second);
        }
    }

    void MicOutputImpl::outputError(const std::string& fullMessage, uint32_t code, bool flush)
    {
        lock_guard<mutex> lock(outputMutex_);
        try
        {
            standardErr_->outputError(fullMessage, code, flush);
            if (fileOut_)
            {
                fileOut_->outputError(fullMessage, code, flush);
            }
        }
        catch (std::exception& e)
        {
            auto info = errorFormatter_.buildError(MicOutputErrorCode::eMicOutputErrorError, e.what());
            throw MicException(info.first, info.second);
        }
    }

    void MicOutputImpl::startSection(const string& sectionTitle, bool suppressLeadingNewline)
    {
        lock_guard<mutex> lock(outputMutex_);
        try
        {
            if (silentMode_ == false)
            {
                standardOut_->startSection(sectionTitle, suppressLeadingNewline);
            }
            if (fileOut_)
            {
                fileOut_->startSection(sectionTitle, suppressLeadingNewline);
            }
        }
        catch (std::exception& e)
        {
            auto info = errorFormatter_.buildError(MicOutputErrorCode::eMicOutputStartSectionError, e.what());
            throw MicException(info.first, info.second);
        }
    }

    void MicOutputImpl::endSection()
    {
        lock_guard<mutex> lock(outputMutex_);
        try
        {
            if (silentMode_ == false)
            {
                standardOut_->endSection();
            }
            if (fileOut_)
            {
                fileOut_->endSection();
            }
        }
        catch (std::exception& e)
        {
            auto info = errorFormatter_.buildError(MicOutputErrorCode::eMicOutputEndSectionError, e.what());
            throw MicException(info.first, info.second);
        }
    }

    bool MicOutputImpl::isSilent()
    {
        lock_guard<mutex> lock(outputMutex_);
        return silentMode_;
    }

    void MicOutputImpl::setSilent(bool silent)
    {
        lock_guard<mutex> lock(outputMutex_);
        silentMode_ = silent;
    }

}; // namespace micmgmt
