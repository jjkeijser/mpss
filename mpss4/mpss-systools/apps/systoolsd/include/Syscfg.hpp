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

#ifndef SYSTOOLS_SYSTOOLSD_SYSCFG_HPP_
#define SYSTOOLS_SYSTOOLSD_SYSCFG_HPP_

#include <string>

#include "systoolsd_api.h"

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif //UNIT_TESTS

enum MemType
{
    cache = 0,
    flat = 1,
    hybrid = 2
};

class PopenInterface;

class SyscfgInterface
{
public:
    virtual ~SyscfgInterface() { };
    virtual bool ecc_enabled() const = 0;
    virtual bool errinject_enabled() const = 0;
    virtual bool eist_enabled() const = 0;
    virtual bool micfw_update_flag_enabled() const = 0;
    virtual bool apei_support_enabled() const = 0;
    virtual bool apei_ffm_logging_enabled() const = 0;
    virtual bool apei_pcie_errinject_enabled() const = 0;
    virtual bool apei_pcie_errinject_table_enabled() const = 0;
    virtual Ecc get_ecc() const = 0;
    virtual MemType get_mem_type() const = 0;
    virtual Cluster get_cluster_mode() const = 0;

    virtual void set_errinject(bool value, const char *pass) = 0;
    virtual void set_eist(bool value, const char *pass) = 0;
    virtual void set_micfw_update_flag(bool value, const char *pass) = 0;
    virtual void set_apei_support(bool value, const char *pass) = 0;
    virtual void set_apei_ffm_logging(bool value, const char *pass) = 0;
    virtual void set_apei_pcie_errinject(bool value, const char *pass) = 0;
    virtual void set_apei_pcie_errinject_table(bool value, const char *pass) = 0;
    virtual void set_ecc(Ecc value, const char *pass) = 0;
    virtual void set_cluster_mode(Cluster value, const char *pass) = 0;
    virtual void set_passwd(const char *old_pass, const char *new_pass) = 0;
};

class Syscfg : public SyscfgInterface
{
public:
    Syscfg();
    virtual ~Syscfg();
    bool ecc_enabled() const;
    bool errinject_enabled() const;
    //Enhanced Intel SpeedStep Technology
    bool eist_enabled() const;
    bool micfw_update_flag_enabled() const;
    bool apei_support_enabled() const;
    bool apei_ffm_logging_enabled() const;
    bool apei_pcie_errinject_enabled() const;
    bool apei_pcie_errinject_table_enabled() const;
    Ecc get_ecc() const;
    MemType get_mem_type() const;
    Cluster get_cluster_mode() const;

    void set_errinject(bool value, const char *pass);
    void set_eist(bool value, const char *pass);
    void set_micfw_update_flag(bool value, const char *pass);
    void set_apei_support(bool value, const char *pass);
    void set_apei_ffm_logging(bool value, const char *pass);
    void set_apei_pcie_errinject(bool value, const char *pass);
    void set_apei_pcie_errinject_table(bool value, const char *pass);
    void set_ecc(Ecc value, const char *pass);
    void set_cluster_mode(Cluster value, const char *pass);
    void set_passwd(const char *old_pass, const char *new_pass);

//for unit tests
PRIVATE:
    //The PopenInterface pointer will be deleted in dtor
    Syscfg(PopenInterface *popen);

private:
    Syscfg(Syscfg&);
    Syscfg &operator=(Syscfg&);
    PopenInterface *syscfg_popen;
    bool setting_enabled(const char *param) const;
    bool apei_setting_enabled(const char *param) const;
    std::string get_syscfg_param(const char *param) const;
    void set_syscfg_param(const char *pass, const char *param, const char *value);
    bool passwd_correct(const char *pass) const;
    std::string extract_value(const std::string &syscfg_output) const;
};

#endif //SYSTOOLS_SYSTOOLSD_SYSCFG_HPP_
