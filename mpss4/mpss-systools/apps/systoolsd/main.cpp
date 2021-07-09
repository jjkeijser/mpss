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

#include <iostream>

#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include "Daemon.hpp"
#include "I2cAccess.hpp"
#include "I2cToolsImpl.hpp"
#include "info/KernelInfo.hpp"
#include "PThresh.hpp"
#include "smbios/EntryPointEfi.hpp"
#include "smbios/EntryPointMemoryScan.hpp"
#include "ScifImpl.hpp"
#include "Syscfg.hpp"
#include "SystoolsdException.hpp"
#include "TurboSettings.hpp"

#ifdef SYSTOOLSD_HOST_TEST
#include "FakeI2c.hpp"
#include "FakePThresh.hpp"
#include "FakeTurbo.hpp"
#include "FakeSmBiosInfo.hpp"
#include "FakeSyscfg.hpp"
#endif //SYSTOOLSD_HOST_TEST

#include "daemonlog.h"

namespace
{
    std::unique_ptr<Daemon> gsystoolsd;
}

void handle_signals(int s)
{
    //TODO: use sigaction
    log(INFO, "received signal %d, shutting down daemon instance", s);
    if(gsystoolsd)
        gsystoolsd->stop();
}

void make_me_daemon()
{
// when debugging, don't daemonize
#ifndef SYSTOOLSD_DEBUG
    if (daemon(0, 0) != 0)
    {
        std::cerr << "daemon failed: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
#endif
}

SmBiosInfo *init_smbios()
{
    //we have two available methods to find the SMBIOS entry point
    EntryPointEfi efi;
    EntryPointMemoryScan memscan("/dev/mem");
    EntryPointFinderInterface *finder = NULL; //pointer to the successful finder method
    SmBiosInfo *smbios = new SmBiosInfo;
    for(;;) //fake loop
    {
        try
        {
            //First, attempt EFI
            efi.open();
            finder = &efi; //use EFI as the finder
            break; //skip memscan
        }
        catch(SystoolsdException &excp)
        {
            //nothing to do, try with regular memory scan
            (void)excp;
        }

        try
        {
            memscan.open();
            finder = &memscan; //use memscan as the finder
            break;
        }
        catch(SystoolsdException &excp)
        {
            (void)excp;
            throw;
        }
    }
    smbios->find_entry_point(finder);
    smbios->parse();
    return smbios;
}

I2cBase *init_i2c()
{
#ifdef FAKE_I2C
    return new FakeI2c;
#else
    return new I2cAccess(new I2cToolsImpl, 0);
#endif //FAKE_I2C
}

PThreshInterface *init_pthresh()
{
#ifdef FAKE_PTHRESH
    return new FakePThresh;
#else
    return new PThresh;
#endif //FAKE_PTHRESH
}

KernelInterface *init_kernel()
{
    return new KernelInfo;
}

TurboSettingsInterface *init_turbo()
{
#ifdef FAKE_TURBO
    return new FakeTurbo;
#else
    return new TurboSettings;
#endif //FAKE_TURBO
}

SyscfgInterface *init_syscfg()
{
#ifdef FAKE_SYSCFG
    return new FakeSyscfg;
#else
    return new Syscfg;
#endif //FAKE_SYSCFG
}

int main(void)
{
    make_me_daemon();
    initlog();

    try
    {
        if(signal(SIGINT, handle_signals) == SIG_ERR ||
           signal(SIGHUP, handle_signals) == SIG_ERR ||
           signal(SIGQUIT, handle_signals) == SIG_ERR ||
           signal(SIGABRT, handle_signals) == SIG_ERR ||
           signal(SIGTERM, handle_signals) == SIG_ERR)
        {
            throw SystoolsdException(SYSTOOLSD_INTERNAL_ERROR, "failed installing signal handlers");
        }

        auto i2c = std::unique_ptr<I2cBase>(init_i2c());
        auto smbios = std::unique_ptr<SmBiosInterface>(init_smbios());
        auto pthresh = std::unique_ptr<PThreshInterface>(init_pthresh());
        auto kernel = std::unique_ptr<KernelInterface>(init_kernel());
        auto turbo = std::unique_ptr<TurboSettingsInterface>(init_turbo());
        auto syscfg = std::unique_ptr<SyscfgInterface>(init_syscfg());

        auto services = SystoolsdServices::ptr(
            new SystoolsdServices(std::move(i2c),
                                  std::move(smbios),
                                  std::move(pthresh),
                                  std::move(turbo),
                                  std::move(syscfg),
                                  std::move(kernel)));
        auto scif = ScifEp::ptr(new ScifEp(new ScifImpl));
        gsystoolsd = std::unique_ptr<Daemon>(new Daemon(scif, std::move(services)));
        gsystoolsd->start();
        log(INFO, "started systoolsd, protocol version %u.%u", SYSTOOLSD_MAJOR_VER, SYSTOOLSD_MINOR_VER);
        //serve_forever() will exit if systoolsd instance receives SIGINT
        gsystoolsd->serve_forever();
        log(INFO, "successfully stopped systoolsd");
    }
    catch(SystoolsdException &rerror)
    {
        log(ERROR, "unhandled exception: %d : %s : %s", rerror.error_nr(), rerror.error_type(), rerror.what());
        shutdownlog();
        return -1;
    }
    catch(...)
    {
        log(ERROR, "unhandled exception");
        shutdownlog();
        return -1;
    }
    return 0;
}
