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

#include "Application.hpp"
#include "CliParser.hpp"
#include "micmgmtCommon.hpp"
#include "MicLogger.hpp"

#include <iostream>

#define MICFW_DESCRIPTION "This tool is used to flash new firmware on Intel(R) Xeon Phi(TM) coprocessors."

using namespace std;
using namespace micmgmt;
using namespace micfw;

void trappedSignalCallback()
{
    std::cerr << "\r*** WARNING: The flashing operation(s) have begun and cannot be stopped!" << std::flush;
    LOG(WARNING_MSG, "Attempt to stop flashing was caught and ignored.");
}

int main(int argc, char* argv[])
{
    Application app(new CliParser(argc, argv, mpssBuildVersion(), string(__DATE__).erase(0, 7), "", MICFW_DESCRIPTION));
    app.setTrapSignalHandle(trappedSignalCallback);

    return app.run();
}
