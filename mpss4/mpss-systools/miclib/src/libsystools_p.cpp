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

#include "libsystools.h"
#include "libsystools_p.hpp"
#include "MicDeviceFactory.hpp"
#include "MicDeviceManager.hpp"
#include "MicDeviceError.hpp"
#ifdef LIBDEBUG
    #include <stdio.h>
#endif

unique_ptr<miclib> miclib::m_instance;

miclib* miclib::getInstance()
{
    if (m_instance.get() == NULL)
        m_instance.reset(new miclib);
    return m_instance.get();
}

#ifdef UNIT_TESTS
miclib* miclib::resetInstance()
{
    m_instance.reset(new miclib);
    return m_instance.get();
}
#endif

miclib::miclib()
{
    m_pDevContainer  = NULL;
    m_pMicList       = NULL;
    m_isInitialized  = false;
    m_nDevices       = 0;
    m_errCode        = MicDeviceError::errorCode( MICSDKERR_NO_DEVICES );
}

#ifdef LIBDEBUG
void miclib::debug()
{
  printf("m_nDevices = %d\n",(int)m_nDevices);
  printf("m_pDevContainer = %p\n",m_pDevContainer);
  printf("m_pMicList = %p\n",m_pMicList);

  for (int idx = 0; idx < (int)m_nDevices; idx++)
     printf ("  m_pMicList[%d] = %p, val = %d\n",idx,&m_pMicList[idx],m_pMicList[idx]);

  for (int idx = 0; idx < (int)m_nDevices; idx++)
     printf ("  ADDR = %p, m_pDevContainer[%d] = %p \n",&m_pDevContainer[idx],idx,m_pDevContainer[idx]);
}
#endif

miclib::~miclib()
{
    delete[] m_pMicList;
    delete[] m_pDevContainer;
}

void miclib::init()
{
    MicDeviceManager deviceManager;

    m_errCode = deviceManager.initialize();

    if (MICSDKERR_SUCCESS != m_errCode)
        return;

    m_nDevices = deviceManager.deviceCount();

    if (0 == m_nDevices) {
        m_errCode = MicDeviceError::errorCode( MICSDKERR_NO_DEVICES );
        return;
    }

    // Create MicDevice container for ndevices.
    m_pDevContainer = new MicDevice* [m_nDevices]();

    if (NULL == m_pDevContainer) {
        m_errCode =  MicDeviceError::errorCode( MICSDKERR_NO_MEMORY );
        return;
    }

    // create MIClist.
    size_t micListSize = m_nDevices * MAX_DEVICE_OPEN;
    m_pMicList = new int [micListSize];

    if (NULL == m_pMicList) {
        m_errCode =  MicDeviceError::errorCode( MICSDKERR_NO_MEMORY );
        delete[] m_pDevContainer;
    }
    else {
       m_isInitialized = true;
       // Initialize elements of MICList to -1
       for (size_t idx = 0; idx < micListSize; idx++)
          m_pMicList[idx] = INVALID_HANDLE;
    }

    return;
}

uint32_t miclib::getLastErrCode() const
{
    return m_errCode;
}

bool miclib::isInitialized()
{
    if (false == m_isInitialized &&
        (m_errCode ==  MicDeviceError::errorCode( MICSDKERR_NO_DEVICES )))
    {
        // Try initializing again if the device manager fails to get
        // devices in the first attempt.
        init();
    }
    return m_isInitialized;
}

uint32_t miclib::isDeviceOpen(int dev_num)
{
    size_t devIter = 0;
    m_errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );

    while (devIter < m_nDevices) {
        // Error if the device is already open.
        if ((NULL != m_pDevContainer[devIter]) && (dev_num == (m_pDevContainer[devIter])->deviceNum())) {
            m_errCode = MicDeviceError::errorCode( MICSDKERR_DEVICE_ALREADY_OPEN );
            break;
        }
        ++devIter;
    }
    return m_errCode;
}

uint32_t miclib::updateDevContainer(int dev_num, MicDevice *micObj, MIC **device)
{
    m_errCode = MicDeviceError::errorCode( MICSDKERR_INTERNAL_ERROR );
    int insertSlot = INVALID_HANDLE;

    if (NULL == device)
        return m_errCode;

    *device = NULL; //Initialize

    // Find the first free slot in the container and insert micobj.
    for (size_t idx =0; idx < m_nDevices; idx++) {
        if ((NULL == m_pDevContainer[idx]) && (INVALID_HANDLE == insertSlot)) {
            insertSlot = (int)idx; // remember this.
        }
        else if ( (NULL !=  m_pDevContainer[idx]) &&
                  (dev_num == (m_pDevContainer[idx])->deviceNum())) {
            // This may happen in threaded applications. Safe to fail.
            insertSlot = INVALID_HANDLE; // invalidate index.
            m_errCode = MicDeviceError::errorCode( MICSDKERR_DEVICE_ALREADY_OPEN );
            break;
        }
    }

    if (INVALID_HANDLE == insertSlot)
        return m_errCode;

    m_pDevContainer[insertSlot] = micObj;
    // Next update the MICList and return the handle.
    size_t listSize = m_nDevices * MAX_DEVICE_OPEN;
    insertSlot = INVALID_HANDLE;
    for (size_t idx=0; idx < listSize; idx++) {
        if  (dev_num == m_pMicList[idx]) {
            // This should never happen. But if it happens then
            // we have to return the handle at this index.
            insertSlot = (int)idx;
            break;
        }
        else if ((INVALID_HANDLE == m_pMicList[idx]) && (INVALID_HANDLE == insertSlot))
        {
           // first free slot. Remember this.
           insertSlot = (int)idx;
        }
    }

    if (INVALID_HANDLE != insertSlot) {
        *device = (MIC*)&m_pMicList[insertSlot]; //Get the handle
        if (NULL != *device)
            m_pMicList[insertSlot] = dev_num;
        m_errCode = MicDeviceError::errorCode( MICSDKERR_SUCCESS );
    }
    return m_errCode;
}


MicDevice *miclib::getOpenDevice(MIC *devHandle, bool invalidate)
{
    bool isValid = false;
    size_t idx = 0;
    MicDevice *device = NULL;
    int dev_num = INVALID_HANDLE;
    size_t listSize = m_nDevices * MAX_DEVICE_OPEN;
    for (; idx < listSize; idx++) {
        if ((devHandle == (MIC*)&m_pMicList[idx]) && (INVALID_HANDLE != m_pMicList[idx])) {
            isValid = true; // Valid handle
            dev_num = m_pMicList[idx]; // Remember this.
            if (true == invalidate) {
               m_pMicList[idx] = INVALID_HANDLE; //reset Handle
            }
            break;
        }
    }

    if (false == isValid)
        return device;

    for (idx=0; idx < m_nDevices; idx++) {
        if ((NULL != m_pDevContainer[idx]) &&
            (dev_num == (m_pDevContainer[idx])->deviceNum()))
        {
            device = m_pDevContainer[idx];
            if (true == invalidate)
               m_pDevContainer[idx] = NULL; // remove device from container.
            break;
        }
    }
    return device;
}

