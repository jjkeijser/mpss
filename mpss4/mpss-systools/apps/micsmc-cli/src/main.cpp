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

#include "CliApplication.hpp"

#include "CliParser.hpp"
#include "micmgmtCommon.hpp"
#include "MicLogger.hpp"

#include "MicDeviceManager.hpp"

#include <iostream>

#define MICSMCCLI_DESCRIPTION "This tool is designed to provide the user the capability to monitor card \
status and settings and, with sufficient permissions, to change specific settings on the Intel(r) Xeon \
Phi(tm) coprocessors line of products."

using namespace std;
using namespace micmgmt;
using namespace micsmccli;

void trappedSignalCallback()
{
    std::cerr << "*** WARNING: Setting ECC mode operation, please wait for completion!" << std::endl;
    LOG(WARNING_MSG, "Attempt to stop during setting ECC mode operation was caught and ignored.");
}

int main(int argc, char* argv[])
{
    int rv = 0;
    try
    {
        CliApplication app(argc, argv, mpssBuildVersion(), "", MICSMCCLI_DESCRIPTION);
        app.setTrapSignalHandler(trappedSignalCallback);
        rv = app.run(0, 0);
    }
    catch (exception& ex)
    {
        cerr << ex.what() << endl;
        rv = 1;
    }
    return rv;
}
