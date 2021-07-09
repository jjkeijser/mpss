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

#ifndef SYSTOOLS_SYSTOOLSD_PROCSTATCORE_HPP
#define SYSTOOLS_SYSTOOLSD_PROCSTATCORE_HPP

#include <memory>
#include <string>

class ProcStatCoreBase
{
public:
    explicit ProcStatCoreBase(const std::string &stat_line);
    ProcStatCoreBase();
    virtual ~ProcStatCoreBase(){ };
    int16_t core_id() const;
    uint64_t system() const;
    uint64_t user() const;
    uint64_t nice() const;
    uint64_t idle() const;
    uint64_t total() const;

    void reset(const std::string &stat_line);

    typedef std::unique_ptr<ProcStatCoreBase> ptr;

public: //static
    static ProcStatCoreBase::ptr get_procstat_core();

protected:
    virtual uint64_t compute_system() const = 0;
    virtual uint64_t compute_user() const = 0;
    virtual uint64_t compute_nice() const = 0;
    virtual uint64_t compute_idle() const = 0;
    virtual uint64_t compute_total() const = 0;
    virtual void reset_values(const std::string &stat_line) = 0;

protected: // members
    std::string m_stat_line;
    int16_t m_core_id;
};

class ProcStatCore_4_1 : public ProcStatCoreBase
{
public:
    explicit ProcStatCore_4_1(const std::string &stat_line);
    ProcStatCore_4_1();
    virtual ~ProcStatCore_4_1() { };

    uint64_t compute_system() const;
    uint64_t compute_user() const;
    uint64_t compute_nice() const;
    uint64_t compute_idle() const;
    uint64_t compute_total() const;
    void reset_values(const std::string &stat_line);

private:
    uint64_t m_user;
    uint64_t m_nice;
    uint64_t m_system;
    uint64_t m_idle;
    uint64_t m_iowait;
    uint64_t m_irq;
    uint64_t m_softirq;
    uint64_t m_steal;
    uint64_t m_guest;
    uint64_t m_guest_nice;
};

#endif // SYSTOOLS_SYSTOOLSD_PROCSTATCORE_HPP
