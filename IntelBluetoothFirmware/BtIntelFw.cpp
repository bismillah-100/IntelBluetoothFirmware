//
//  BtIntelFirmware.cpp
//  IntelBluetoothFirmware
//
//  Created by qcwap on 2021/6/16.
//  Copyright © 2021 zxystd. All rights reserved.
//

#include "Log.h"
#include "BtIntel.h"
#include "FwData.h"

OSData *BtIntel::
firmwareConvertion(OSData *originalFirmware)
{
    unsigned int srcLen = originalFirmware->getLength();
    if (srcLen == 0 || srcLen > (16U * 1024U * 1024U)) {
        return NULL;
    }
    unsigned int numBytes = srcLen * 4;
    const unsigned int maxBytes = 16U * 1024U * 1024U;
    if (numBytes > maxBytes) {
        numBytes = maxBytes;
    }
    OSData *fwData = NULL;
    while (true) {
        unsigned int actualBytes = numBytes;
        bool bufTooSmall = false;
        unsigned char* _fwBytes = (unsigned char *)IOMalloc(numBytes);
        if (!_fwBytes) {
            return NULL;
        }
        if (uncompressFirmware(_fwBytes, &actualBytes, (unsigned char *)originalFirmware->getBytesNoCopy(), srcLen, &bufTooSmall)) {
            fwData = OSData::withBytes(_fwBytes, actualBytes);
            IOFree(_fwBytes, numBytes);
            return fwData;
        }
        IOFree(_fwBytes, numBytes);
        if (!bufTooSmall || numBytes >= maxBytes) {
            return NULL;
        }
        numBytes = (numBytes > maxBytes / 2) ? maxBytes : (numBytes * 2);
    }
}

OSData *BtIntel::
requestFirmwareData(const char *fwName, bool noWarn)
{
    OSData * _fwData = getFWDescByName(fwName);
    if (!_fwData) {
        if (!noWarn)
            XYLog("Firmware: %s Not found!\n", fwName);
        return NULL;
    }
    XYLog("Found device firmware %s \n", fwName);
    OSData *fwData = firmwareConvertion(_fwData);
    OSSafeReleaseNULL(_fwData);
    if (fwData == NULL) {
        XYLog("Firmware %s uncompress fail!\n", fwName);
        return NULL;
    }
    return fwData;
}
