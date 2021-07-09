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

#ifndef MICLIB_SRC_MICLIB_INT_H_
#define MICLIB_SRC_MICLIB_INT_H_

#include "MicDevice.hpp"
#include <cstring>

using namespace micmgmt;
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif
    MicDevice *getOpenMicDev(MIC *devHandle, uint32_t *err);
#ifdef __cplusplus
}
#endif

// Singleton Class to initialize library.
class miclib {
public:
    static miclib* getInstance();
#ifdef UNIT_TESTS
    static miclib* resetInstance();
#endif
    ~miclib();
    void init();
    void debug();
    uint32_t isDeviceOpen(int dev_num);
    uint32_t updateDevContainer(int dev_num, MicDevice *micObj, MIC **device);
    MicDevice* getOpenDevice(MIC *devHandle, bool invalidate=false);
    uint32_t getLastErrCode() const;
    bool     isInitialized();

private:
    miclib();
    miclib(const miclib&);
    miclib& operator=(const miclib&);

private:
    static unique_ptr<miclib> m_instance;
    static const int INVALID_HANDLE = -1;
    static const size_t MAX_DEVICE_OPEN = 1; // Open Limit per device per process.
    MicDevice   **m_pDevContainer;           // MicDevice obj container.
    int         *m_pMicList;                 // Dynamic array to hold dev open handle.
    bool        m_isInitialized;
    uint32_t    m_errCode;
    size_t      m_nDevices;
};
#endif //MICLIB_SRC_MICLIB_INT_H_
