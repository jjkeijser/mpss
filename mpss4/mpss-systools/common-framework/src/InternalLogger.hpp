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

#ifndef MICMGMT_INTERNALLOGGER_HPP
#define MICMGMT_INTERNALLOGGER_HPP

#include <string>
#include <mutex>

namespace micmgmt
{
    class InternalLogger
    {
    private: // Fields
        std::mutex criticalSection_;
        std::string baseFilename_;
        bool disabled_;

    public: // Construction & Destruction
        InternalLogger(const std::string& nameOnly);
        ~InternalLogger();

        void logLine(const std::string& line);

        const std::string& getBaseFilename() const;

    private: // Disabled
        InternalLogger();
        InternalLogger(const InternalLogger&);

    private: // Implementation
        int lineCount();
        void deleteAndShiftBackups();
        std::string getBackupFilename(int n) const;
    }; // class InternalLogger
} // namespace micmgmt
#endif // MICMGMT_INTERNALLOGGER_HPP
