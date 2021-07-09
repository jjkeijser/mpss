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

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "info/ProcStatCore.hpp"

ProcStatCoreBase::ProcStatCoreBase(const std::string &stat_line) :
    m_stat_line(stat_line), m_core_id(-1)
{
    m_core_id = core_id();
}

ProcStatCoreBase::ProcStatCoreBase() : m_stat_line(""), m_core_id(0)
{
    // nothing to do
}

int16_t ProcStatCoreBase::core_id() const
{
    if(m_stat_line.empty())
        return 0;
    std::stringstream ss(m_stat_line);
    std::string cpu_id;

    //Line in the form:
    //   cpu0 2815 104 2830 1439560 16565 0 69 0 0

    // Extract cpuX
    ss >> cpu_id;
    if(ss.fail())
        throw std::invalid_argument("invalid stat line");

    if(cpu_id.find("cpu") != 0)
        throw std::invalid_argument("invalid stat line");

    // Discard "cpu" prefix
    cpu_id = cpu_id.substr(3);
    if(cpu_id.empty())
        // This is an aggregate line, i.e. sum of all cores
        return -1;

    ss.str(cpu_id);
    ss.clear();

    int16_t id;
    ss >> id;

    if(ss.fail())
        throw std::invalid_argument("invalid stat line");

    return id;
}

uint64_t ProcStatCoreBase::system() const
{
    return compute_system();
}

uint64_t ProcStatCoreBase::user() const
{
    return compute_user();
}

uint64_t ProcStatCoreBase::nice() const
{
    return compute_nice();
}

uint64_t ProcStatCoreBase::idle() const
{
    return compute_idle();
}

uint64_t ProcStatCoreBase::total() const
{
    return compute_total();
}

ProcStatCoreBase::ptr ProcStatCoreBase::get_procstat_core()
{
    // just return 4.1 implementation for now
    return std::unique_ptr<ProcStatCore_4_1>(new ProcStatCore_4_1);
}

void ProcStatCoreBase::reset(const std::string &stat_line)
{
    m_stat_line = stat_line;
    m_core_id = core_id();
    reset_values(stat_line);
}

////////////////////////////////////////////////////////////////////////
// ProcStatCore_4_1
// Note: the computations in this class are based on htop's source code
////////////////////////////////////////////////////////////////////////
ProcStatCore_4_1::ProcStatCore_4_1(const std::string &stat_line) :
    ProcStatCoreBase(stat_line), m_user(0), m_nice(0), m_system(0),
    m_idle(0), m_iowait(0), m_irq(0), m_softirq(0), m_steal(0),
    m_guest(0), m_guest_nice(0)
{
    reset_values(stat_line);
}

ProcStatCore_4_1::ProcStatCore_4_1() : ProcStatCoreBase(), m_user(0), m_nice(0),
    m_system(0), m_idle(0), m_iowait(0), m_irq(0), m_softirq(0), m_steal(0),
    m_guest(0), m_guest_nice(0)
{
    //nothing to do
}

uint64_t ProcStatCore_4_1::compute_system() const
{
    return m_system + m_irq + m_softirq;
}

uint64_t ProcStatCore_4_1::compute_user() const
{
    return m_user - m_guest;
}

uint64_t ProcStatCore_4_1::compute_nice() const
{
    return m_nice - m_guest_nice;
}

uint64_t ProcStatCore_4_1::compute_idle() const
{
    return m_idle + m_iowait;
}

uint64_t ProcStatCore_4_1::compute_total() const{
    uint64_t user = compute_user();
    uint64_t nice = compute_nice();
    uint64_t idle = compute_idle();
    uint64_t system = compute_system();
    uint64_t virt = m_guest + m_guest_nice;

    return user + nice + system + idle + m_steal + virt;
}

void ProcStatCore_4_1::reset_values(const std::string &stat_line)
{
    std::stringstream ss(stat_line);
    std::string s;

    //Discard cpuX portion
    ss >> s;

    ss >> m_user;
    ss >> m_nice;
    ss >> m_system;
    ss >> m_idle;
    ss >> m_iowait;
    ss >> m_irq;
    ss >> m_softirq;
    ss >> m_steal;
    ss >> m_guest;
    ss >> m_guest_nice;

    if(ss.fail())
        throw std::invalid_argument("could not extract stat values");

}
