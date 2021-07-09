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

#include "Daemon.hpp"
#include "handler/MicBiosRequestHandler.hpp"
#include "Syscfg.hpp"

#include <sstream>

namespace
{
    template<class T>
    void check_value_in_range(unsigned char value, T max)
    {
        if(value < 0 || value >= max)
        {
            std::stringstream ss;
            ss << "value out of range: " << (int)value << " max: " << max;
            throw SystoolsdException(SYSTOOLSD_INVAL_ARGUMENT, ss.str().c_str());
        }
    }
}

MicBiosRequestHandler::MicBiosRequestHandler(struct SystoolsdReq &req, DaemonSession::ptr sess, Daemon &owner,
                                             SyscfgInterface *syscfg_) :
    RequestHandlerBase(req, sess, owner), m_syscfg(syscfg_)
{
    if(!m_syscfg)
        throw std::invalid_argument("NULL m_syscfg");
}

void MicBiosRequestHandler::handle_request()
{
    if(!is_from_root())
    {
        reply_error(SYSTOOLSD_INSUFFICIENT_PRIVILEGES);
        return;
    }

    req.card_errno = 0;
    sess->get_client()->send((char*)&req, sizeof(req));

    //recv a MicBiosRequest from client
    bzero(&m_mbreq, sizeof(m_mbreq));
    try
    {
        int bytes_recv = sess->get_client()->recv((char*)&m_mbreq, sizeof(m_mbreq));
        if(bytes_recv != sizeof(m_mbreq))
        {
            //protocol mismatch
            log(ERROR, "expecting %u bytes for MicBiosRequest, received %d bytes", sizeof(m_mbreq), bytes_recv);
            reply_error(SYSTOOLSD_INVAL_STRUCT);
            sess->get_client()->close();
            return;
        }
        //detect read, write or change password request
        switch(m_mbreq.cmd)
        {
            case MicBiosCmd::micbios_read:
                log(INFO, "Serving micbios read request");
                serve_read_request();
                break;
            case MicBiosCmd::micbios_write:
                log(INFO, "Serving micbios write request");
                serve_write_request();
                break;
            case MicBiosCmd::micbios_change_pass:
                log(INFO, "Serving micbios change password request");
                serve_change_password_request();
                break;
            default:
                //reply with error
                reply_error(SYSTOOLSD_UNSUPPORTED_REQ);
                return;
        }
    }
    catch(SystoolsdException &excp)
    {
        log(ERROR, excp);
        reply_error(excp);
        return;
    }
}

void MicBiosRequestHandler::serve_read_request()
{
    if(m_mbreq.prop & mb_cluster)
    {
        m_mbreq.settings.cluster = m_syscfg->get_cluster_mode();
    }
    if(m_mbreq.prop & mb_ecc)
    {
        m_mbreq.settings.ecc = m_syscfg->get_ecc();
    }
    if(m_mbreq.prop & mb_apei_supp)
    {
        m_mbreq.settings.apei_supp = m_syscfg->apei_support_enabled() ?
        APEISupport::supp_enable : APEISupport::supp_disable;
    }
    if(m_mbreq.prop & mb_apei_ffm)
    {
        m_mbreq.settings.apei_ffm = m_syscfg->apei_ffm_logging_enabled() ?
        APEIFfm::ffm_enable : APEIFfm::ffm_disable;
    }
    if(m_mbreq.prop & mb_apei_einj)
    {
        m_mbreq.settings.apei_einj = m_syscfg->apei_pcie_errinject_enabled() ?
        APEIEinj::einj_enable : APEIEinj::einj_disable;
    }
    if(m_mbreq.prop & mb_apei_einjtable)
    {
        m_mbreq.settings.appei_einjtable = m_syscfg->apei_pcie_errinject_table_enabled() ?
        APEIEinjTable::table_enable : APEIEinjTable::table_disable;
    }
    if(m_mbreq.prop & mb_fwlock)
    {
        m_mbreq.settings.fwlock = m_syscfg->micfw_update_flag_enabled() ?
        Fwlock::fwlock_enable : Fwlock::fwlock_disable;
    }

    req.card_errno = 0;
    req.length = sizeof(m_mbreq);

    try
    {
        //Send SystoolsdReq struct back to client with updated fields
        sess->get_client()->send((char*)&req, sizeof(req));
        sess->get_client()->send((char*)&m_mbreq, sizeof(m_mbreq));
    }
    catch(SystoolsdException &excp)
    {
        log(WARNING, excp, "error serving micbios read request: %s", excp.what());
        reply_error(excp);
    }
    catch(...)
    {
        log(ERROR, "unknown error");
        reply_error(SYSTOOLSD_UNKOWN_ERROR);
    }
}

void MicBiosRequestHandler::serve_write_request()
{
    const char *passwd = req.data;

    if(m_mbreq.prop & mb_cluster)
    {
        check_value_in_range(m_mbreq.settings.cluster, Cluster::cluster_max);
        Cluster value = (Cluster)m_mbreq.settings.cluster;
        m_syscfg->set_cluster_mode(value, passwd);
    }
    if(m_mbreq.prop & mb_ecc)
    {
        check_value_in_range(m_mbreq.settings.ecc, Ecc::ecc_max);
        Ecc value = (Ecc)m_mbreq.settings.ecc;
        m_syscfg->set_ecc(value, passwd);
    }
    if(m_mbreq.prop & mb_apei_supp)
    {
        check_value_in_range(m_mbreq.settings.apei_supp, APEISupport::supp_max);
        bool value = m_mbreq.settings.apei_supp == APEISupport::supp_enable;
        m_syscfg->set_apei_support(value, passwd);
    }
    if(m_mbreq.prop & mb_apei_ffm)
    {
        check_value_in_range(m_mbreq.settings.apei_ffm, APEIFfm::ffm_max);
        bool value = m_mbreq.settings.apei_ffm == APEIFfm::ffm_enable;
        m_syscfg->set_apei_ffm_logging(value, passwd);
    }
    if(m_mbreq.prop & mb_apei_einj)
    {
        check_value_in_range(m_mbreq.settings.apei_einj, APEIEinj::einj_max);
        bool value = m_mbreq.settings.apei_einj == APEIEinj::einj_enable;
        m_syscfg->set_apei_pcie_errinject(value, passwd);
    }
    if(m_mbreq.prop & mb_apei_einjtable)
    {
        check_value_in_range(m_mbreq.settings.appei_einjtable, APEIEinjTable::table_max);
        bool value = m_mbreq.settings.appei_einjtable == APEIEinjTable::table_enable;
        m_syscfg->set_apei_pcie_errinject_table(value, passwd);
    }
    if(m_mbreq.prop & mb_fwlock)
    {
        check_value_in_range(m_mbreq.settings.fwlock, Fwlock::fwlock_max);
        bool value = m_mbreq.settings.fwlock == Fwlock::fwlock_enable;
        m_syscfg->set_micfw_update_flag(value, passwd);
    }

    req.card_errno = 0;

    try
    {
        //Send SystoolsdReq struct back to client
        sess->get_client()->send((char*)&req, sizeof(req));
    }
    catch(SystoolsdException &excp)
    {
        log(WARNING, excp, "error serving micbios write request: %s", excp.what());
        reply_error(excp);
    }
    catch(...)
    {
        log(ERROR, "unknown error");
        reply_error(SYSTOOLSD_UNKOWN_ERROR);
    }
}

void MicBiosRequestHandler::serve_change_password_request()
{
    char *old_passwd = req.data;
    char  new_passwd[15];
    int   len_passwd = req.length;

    if(len_passwd > 14) //syscfg restriction
    {
        log(ERROR, "Syscfg password longer than 14 chars");
        reply_error(SYSTOOLSD_INVAL_ARGUMENT);
        return;
    }

    // Notify client of success
    req.card_errno = 0;
    sess->get_client()->send((char*)&req, sizeof(req));

    bzero(new_passwd, sizeof(new_passwd));
    if(len_passwd > 0)
    {
        int bytes_recv = sess->get_client()->recv(new_passwd, len_passwd);
        if(bytes_recv != len_passwd)
        {
            log(ERROR, "expecting %u bytes for new_passwd, received %d bytes",
                len_passwd, bytes_recv);
            reply_error(SYSTOOLSD_INVAL_STRUCT);
            return;
        }
    }
    m_syscfg->set_passwd(old_passwd, new_passwd);

    req.card_errno = 0;

    try
    {
        //Send SystoolsdReq struct back to client
        sess->get_client()->send((char*)&req, sizeof(req));
    }
    catch(SystoolsdException &excp)
    {
        log(WARNING, excp, "error serving micbios change password request: %s", excp.what());
        reply_error(excp);
    }
    catch(...)
    {
        log(ERROR, "unknown error");
        reply_error(SYSTOOLSD_UNKOWN_ERROR);
    }
}
