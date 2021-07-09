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

#include <sstream>
#include <stdexcept>
#include <string>
#include <iomanip>
#include <cstring>

#include "Popen.hpp"
#include "Syscfg.hpp"
#include "SystoolsdException.hpp"

//avoid name clash with MemType elements
namespace syscfg_tokens
{
    const char *disabled = "Disable";
    const char *enabled = "Enable";
    const char *auto_ = "Auto";

    const char *cache = "Cache";
    const char *flat = "Flat";
    const char *hybrid = "Hybrid";
}

//Following Cluster enum
const char *cluster_names[] = {"All2All", "SNC-2", "SNC-4", "Hemisphere", "Quadrant", syscfg_tokens::auto_};

Syscfg::Syscfg() : syscfg_popen(NULL)
{
    syscfg_popen = new Popen();
}

//The PopenInterface pointer will be deleted in dtor
Syscfg::Syscfg(PopenInterface *popen_) : syscfg_popen(popen_)
{
    if(!syscfg_popen)
        throw std::invalid_argument("NULL PopenInterface object");
}

Syscfg::~Syscfg()
{
    delete syscfg_popen;
}

bool Syscfg::ecc_enabled() const
{
    std::string value = get_syscfg_param("ECC Support");
    if(value.find(syscfg_tokens::disabled) != std::string::npos)
        return false;
    else if(value.find(syscfg_tokens::enabled) != std::string::npos)
        return true;
    else if(value.find(syscfg_tokens::auto_) != std::string::npos)
        return true;
    else
    {
        std::stringstream ss;
        ss << "unknown ECC configuration: " << value;
        throw SystoolsdException(SYSTOOLSD_UNKOWN_ERROR, ss.str().c_str());
    }
}

bool Syscfg::setting_enabled(const char *param) const
{
    std::string value = get_syscfg_param(param);
    if(value.find(syscfg_tokens::disabled) != std::string::npos)
        return false;
    else if(value.find(syscfg_tokens::enabled) != std::string::npos)
        return true;
    else
    {
        std::stringstream ss;
        ss << "unknown " << param << " configuration: " << value;
        throw SystoolsdException(SYSTOOLSD_UNKOWN_ERROR, ss.str().c_str());
    }
}

bool Syscfg::errinject_enabled() const
{
    return setting_enabled("Error Injection");
}

bool Syscfg::eist_enabled() const
{
    return setting_enabled("EIST (GV3)");
}

bool Syscfg::micfw_update_flag_enabled() const
{
    return setting_enabled("MICFW Update Flag");
}

bool Syscfg::apei_support_enabled() const
{
    return setting_enabled("APEI Support");
}

bool Syscfg::apei_setting_enabled(const char *param) const
{
    if(apei_support_enabled())
    {
        return setting_enabled(param);
    }
    return false;
}

bool Syscfg::apei_ffm_logging_enabled() const
{
    return apei_setting_enabled("APEI FFM Logging");
}

bool Syscfg::apei_pcie_errinject_enabled() const
{
    return apei_setting_enabled("APEI PCIe Error Injection");
}

bool Syscfg::apei_pcie_errinject_table_enabled() const
{
    return apei_setting_enabled("APEI PCIe EInj Action Table");
}

Ecc Syscfg::get_ecc() const
{
    std::string value = get_syscfg_param("ECC Support");
    if(value.find(syscfg_tokens::disabled) != std::string::npos)
        return Ecc::ecc_disable;
    else if(value.find(syscfg_tokens::enabled) != std::string::npos)
        return Ecc::ecc_enable;
    else if(value.find(syscfg_tokens::auto_) != std::string::npos)
        return Ecc::ecc_auto_mode;
    else
    {
        std::stringstream ss;
        ss << "unknown ECC configuration: " << value;
        throw SystoolsdException(SYSTOOLSD_UNKOWN_ERROR, ss.str().c_str());
    }
}

MemType Syscfg::get_mem_type() const
{
    std::string value = get_syscfg_param("Memory Mode");
    if(value.find(syscfg_tokens::cache) != std::string::npos)
        return MemType::cache;
    else if(value.find(syscfg_tokens::flat) != std::string::npos)
        return MemType::flat;
    else if(value.find(syscfg_tokens::hybrid) != std::string::npos)
        return MemType::hybrid;
    else
    {
        std::stringstream ss;
        ss << "unknown Memory Mode: " << value;
        throw SystoolsdException(SYSTOOLSD_UNKOWN_ERROR, ss.str().c_str());
    }
}

Cluster Syscfg::get_cluster_mode() const
{
    std::string value = get_syscfg_param("Cluster Mode");
    if(value.find(cluster_names[Cluster::all2all]) != std::string::npos)
        return Cluster::all2all;
    else if(value.find(cluster_names[Cluster::snc2]) != std::string::npos)
        return Cluster::snc2;
    else if(value.find(cluster_names[Cluster::snc4]) != std::string::npos)
        return Cluster::snc4;
    else if(value.find(cluster_names[Cluster::hemisphere]) != std::string::npos)
        return Cluster::hemisphere;
    else if(value.find(cluster_names[Cluster::quadrant]) != std::string::npos)
        return Cluster::quadrant;
    else if(value.find(cluster_names[Cluster::auto_mode]) != std::string::npos)
        return Cluster::auto_mode;
    else
    {
        std::stringstream ss;
        ss << "unknown Cluster Mode: " << value;
        throw SystoolsdException(SYSTOOLSD_UNKOWN_ERROR, ss.str().c_str());
    }
}

void Syscfg::set_ecc(Ecc value, const char *pass)
{
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << value;
    set_syscfg_param(pass, "ECC Support", ss.str().c_str());
}

void Syscfg::set_errinject(bool value, const char *pass)
{
    set_syscfg_param(pass, "Error Injection", (value ? "01" : "00"));
}

void Syscfg::set_eist(bool value, const char *pass)
{
    set_syscfg_param(pass, "EIST (GV3)", (value ? "01" : "00"));
}

void Syscfg::set_micfw_update_flag(bool value, const char *pass)
{
    set_syscfg_param(pass, "MICFW Update Flag", (value ? "01" : "00"));
}

void Syscfg::set_apei_support(bool value, const char *pass)
{
    set_syscfg_param(pass, "APEI Support", (value ? "01" : "00"));
}

void Syscfg::set_apei_ffm_logging(bool value, const char *pass)
{
    set_syscfg_param(pass, "APEI FFM Logging", (value ? "01" : "00"));
}

void Syscfg::set_apei_pcie_errinject(bool value, const char *pass)
{
    set_syscfg_param(pass, "APEI PCIe Error Injection", (value ? "01" : "00"));
}

void Syscfg::set_apei_pcie_errinject_table(bool value, const char *pass)
{
    set_syscfg_param(pass, "APEI PCIe EInj Action Table", (value ? "01" : "00"));
}

void Syscfg::set_cluster_mode(Cluster value, const char *pass)
{
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << value;
    set_syscfg_param(pass, "Cluster Mode", ss.str().c_str());
}

std::string Syscfg::get_syscfg_param(const char *param) const
{
    std::string cmd = "syscfg -d BIOSSETTINGS ";
    // syscfg settings may have spaces... quote appropriately!
    cmd += "'" + std::string(param) + "'";

    syscfg_popen->run(cmd.c_str());
    if(syscfg_popen->get_retcode())
    {
        std::stringstream ss;
        ss << "syscfg failed: param: " << param;
        throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, ss.str().c_str());
    }
    std::string out = syscfg_popen->get_output();
    return extract_value(out);
}

std::string Syscfg::extract_value(const std::string &syscfg_output) const
{
    std::stringstream s(syscfg_output);
    std::string line;
    //read up to line in the form "Current Value : <value>",
    //and extract <value>
    while(std::getline(s, line))
    {
        if(line.find("Current Value") == std::string::npos)
            continue;
        size_t idx = 0;
        if((idx = line.find(":")) == std::string::npos)
        {
            break;
        }
        return line.substr(line.find_first_not_of(" ", idx + 1));
    }
    throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, "error parsing syscfg output");
}

bool Syscfg::passwd_correct(const char *pass) const
{
    //syscfg help states password must be up to 14 chars
    if(memchr(pass, '\0', 15) == NULL)
    {
        return false;
    }

    //avoid bash injection by not allowing single quotes
    static const char *specials = "!@#$%%^&*()-_+=?";
    size_t len = strlen(pass);
    for(size_t i = 0; i < len; i++)
    {
        if(!isalnum(pass[i]) && strchr(specials, pass[i]) == NULL)
        {
            return false;
        }
    }
    return true;
}

void Syscfg::set_syscfg_param(const char *pass, const char *param, const char *value)
{
    if(!passwd_correct(pass))
    {
        throw SystoolsdException(SYSTOOLSD_INVAL_ARGUMENT, "malformed password");
    }

    std::stringstream cmd;
    cmd << "syscfg -bcs '" << pass << "'"; // Password
    cmd << " '" << param << "'"; // Parameter to set
    cmd << " '" << value << "'"; // Value to set

    syscfg_popen->run(cmd.str().c_str());
    int retcode = syscfg_popen->get_retcode();
    if(7 == retcode) // Wrong password
    {
        std::string what = "syscfg failed: incorrect password";
        throw SystoolsdException(SYSTOOLSD_INVAL_ARGUMENT, what.c_str());
    }
    else if(retcode)
    {
        std::stringstream ss;
        ss << "syscfg failed setting param " << param << " to '" << value << "'";
        throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, ss.str().c_str());
    }
}

void Syscfg::set_passwd(const char *old_pass, const char *new_pass)
{
    if(!passwd_correct(old_pass) || !passwd_correct(new_pass))
    {
        throw SystoolsdException(SYSTOOLSD_INVAL_ARGUMENT, "malformed password");
    }

    std::stringstream cmd;
    cmd << "syscfg -bap '" << old_pass << "' '" << new_pass << "'";

    syscfg_popen->run(cmd.str().c_str());
    int retcode = syscfg_popen->get_retcode();
    if(7 == retcode) // Wrong old password
    {
        std::string what = "syscfg failed: incorrect password";
        throw SystoolsdException(SYSTOOLSD_INVAL_ARGUMENT, what.c_str());
    }
    else if(retcode)
    {
        std::string what = "syscfg failed setting param password";
        throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, what.c_str());
    }
}
