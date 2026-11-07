//
//  IntelBTPatcher.cpp
//  IntelBTPatcher
//
//  Created by zxystd <zxystd@foxmail.com> on 2021/2/8.
//
#define __ACIDANTHERA_MAC_SDK 2600
#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/plugin_start.hpp>
#include <libkern/OSAtomic.h>

#include "IntelBTPatcher.hpp"

static CIntelBTPatcher ibtPatcher;
static CIntelBTPatcher *callbackIBTPatcher = nullptr;

static const char *bootargOff[] {
    "-ibtcompatoff"
};

static const char *bootargDebug[] {
    "-ibtcompatdbg"
};

static const char *bootargBeta[] {
    "-ibtcompatbeta"
};

PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery | LiluAPI::AllowSafeMode,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::MountainLion,
    KernelVersion::Tahoe,
    []() {
        ibtPatcher.init();
    }
};

static const char *IntelBTPatcher_IOBluetoothFamily[] { "/System/Library/Extensions/IOBluetoothFamily.kext/Contents/MacOS/IOBluetoothFamily" };

static KernelPatcher::KextInfo IntelBTPatcher_IOBluetoothInfo {
    "com.apple.iokit.IOBluetoothFamily",
    IntelBTPatcher_IOBluetoothFamily,
    1,
    {true, true},
    {},
    KernelPatcher::KextInfo::Unloaded
};

static const char *IntelBTPatcher_IOUSBHostFamily[] {
    "/System/Library/Extensions/IOUSBHostFamily.kext/Contents/MacOS/IOUSBHostFamily" };

static KernelPatcher::KextInfo IntelBTPatcher_IOUsbHostInfo {
    "com.apple.iokit.IOUSBHostFamily",
    IntelBTPatcher_IOUSBHostFamily,
    1,
    {true, true},
    {},
    KernelPatcher::KextInfo::Unloaded
};

void *CIntelBTPatcher::_hookPipeInstance = nullptr;
IOUSBHostDevice *CIntelBTPatcher::_hookDeviceInstance = nullptr;
bool CIntelBTPatcher::_randomAddressInit = false;

bool CIntelBTPatcher::init()
{
    DBGLOG(DRV_NAME, "%s", __PRETTY_FUNCTION__);
    callbackIBTPatcher = this;
    if (getKernelVersion() < KernelVersion::Monterey) {
        lilu.onKextLoadForce(&IntelBTPatcher_IOBluetoothInfo, 1,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            callbackIBTPatcher->processKext(patcher, index, address, size);
        }, this);
    } else {
        lilu.onKextLoadForce(&IntelBTPatcher_IOUsbHostInfo, 1,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            callbackIBTPatcher->processKext(patcher, index, address, size);
        }, this);
    }
    return true;
}

void CIntelBTPatcher::free()
{
    DBGLOG(DRV_NAME, "%s", __PRETTY_FUNCTION__);
}

void CIntelBTPatcher::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size)
{
    DBGLOG(DRV_NAME, "%s", __PRETTY_FUNCTION__);
    if (getKernelVersion() < KernelVersion::Monterey) {
        if (IntelBTPatcher_IOBluetoothInfo.loadIndex == index) {
            DBGLOG(DRV_NAME, "%s", IntelBTPatcher_IOBluetoothInfo.id);
            
            KernelPatcher::RouteRequest findQueueRequestRequest {
                "__ZN25IOBluetoothHostController17FindQueuedRequestEtP22BluetoothDeviceAddresstbPP21IOBluetoothHCIRequest",
                newFindQueueRequest,
                oldFindQueueRequest
            };
            patcher.routeMultiple(index, &findQueueRequestRequest, 1, address, size);
            if (patcher.getError() == KernelPatcher::Error::NoError) {
                DBGLOG(DRV_NAME, "routed %s", findQueueRequestRequest.symbol);
            } else {
                SYSLOG(DRV_NAME, "failed to resolve %s, error = %d", findQueueRequestRequest.symbol, patcher.getError());
                patcher.clearError();
            }
            
        }
    } else {
        if (IntelBTPatcher_IOUsbHostInfo.loadIndex == index) {
            SYSLOG(DRV_NAME, "%s", IntelBTPatcher_IOUsbHostInfo.id);
            
            KernelPatcher::RouteRequest hostDeviceRequest {
            "__ZN15IOUSBHostDevice13deviceRequestEP9IOServiceRN11StandardUSB13DeviceRequestEPvP18IOMemoryDescriptorRjP19IOUSBHostCompletionj",
                newHostDeviceRequest,
                oldHostDeviceRequest
            };
            patcher.routeMultiple(index, &hostDeviceRequest, 1, address, size);
            if (patcher.getError() == KernelPatcher::Error::NoError) {
                SYSLOG(DRV_NAME, "routed %s", hostDeviceRequest.symbol);
            } else {
                SYSLOG(DRV_NAME, "failed to resolve %s, error = %d", hostDeviceRequest.symbol, patcher.getError());
                patcher.clearError();
            }

            KernelPatcher::RouteRequest asyncIORequest {
            "__ZN13IOUSBHostPipe2ioEP18IOMemoryDescriptorjP19IOUSBHostCompletionj",
                newAsyncIO,
                oldAsyncIO
            };
            patcher.routeMultiple(index, &asyncIORequest, 1, address, size);
            if (patcher.getError() == KernelPatcher::Error::NoError) {
                SYSLOG(DRV_NAME, "routed %s", asyncIORequest.symbol);
            } else {
                SYSLOG(DRV_NAME, "failed to resolve %s, error = %d", asyncIORequest.symbol, patcher.getError());
                patcher.clearError();
            }

            // macOS 26.5+: third parameter became ConfigurationDescriptor (was SuperSpeedEndpointCompanionDescriptor)
            KernelPatcher::RouteRequest initPipeConfigRequest {
            "__ZN13IOUSBHostPipe28initWithDescriptorsAndOwnersEPKN11StandardUSB18EndpointDescriptorEPKNS0_23ConfigurationDescriptorEP22AppleUSBHostControllerP15IOUSBHostDeviceP18IOUSBHostInterfaceht",
                newInitPipeConfig,
                oldInitPipeConfig
            };
            patcher.routeMultiple(index, &initPipeConfigRequest, 1, address, size);
            if (patcher.getError() == KernelPatcher::Error::NoError) {
                SYSLOG(DRV_NAME, "routed %s", initPipeConfigRequest.symbol);
            } else {
                DBGLOG(DRV_NAME, "failed to resolve %s, error = %d", initPipeConfigRequest.symbol, patcher.getError());
                patcher.clearError();
            }

            KernelPatcher::RouteRequest initPipeRequest {
            "__ZN13IOUSBHostPipe28initWithDescriptorsAndOwnersEPKN11StandardUSB18EndpointDescriptorEPKNS0_37SuperSpeedEndpointCompanionDescriptorEP22AppleUSBHostControllerP15IOUSBHostDeviceP18IOUSBHostInterfaceht",
                newInitPipe,
                oldInitPipe
            };
            patcher.routeMultiple(index, &initPipeRequest, 1, address, size);
            if (patcher.getError() == KernelPatcher::Error::NoError) {
                SYSLOG(DRV_NAME, "routed %s", initPipeRequest.symbol);
            } else {
                DBGLOG(DRV_NAME, "failed to resolve %s, error = %d", initPipeRequest.symbol, patcher.getError());
                patcher.clearError();
            }
        }
    }
}

#pragma mark - For Bigsur-, patch unhandled 0x2019 opcode

#define HCI_OP_LE_START_ENCRYPTION 0x2019

IOReturn CIntelBTPatcher::newFindQueueRequest(void *that, unsigned short arg1, void *addr, unsigned short arg2, bool arg3, void **hciRequestPtr)
{
    IOReturn ret = FunctionCast(newFindQueueRequest, callbackIBTPatcher->oldFindQueueRequest)(that, arg1, addr, arg2, arg3, hciRequestPtr);
    if (ret != 0 && arg1 == HCI_OP_LE_START_ENCRYPTION) {
        ret = FunctionCast(newFindQueueRequest, callbackIBTPatcher->oldFindQueueRequest)(that, arg1, addr, 0xffff, arg3, hciRequestPtr);
        DBGLOG(DRV_NAME, "%s ret: %d arg1: 0x%04x arg2: 0x%04x arg3: %d ptr: %p", __FUNCTION__, ret, arg1, arg2, arg3, *hciRequestPtr);
    }
    return ret;
}

#pragma mark - For Monterey+ patch for intercept HCI REQ and RESP

// Hardcoded Random Address HCI Command
static const uint8_t randomAddressHci[9] = {0x05, 0x20, 0x06, 0x94, 0x50, 0x64, 0xD0, 0x78, 0x6B}; 

#define MAX_HCI_BUF_LEN                 255
#define HCI_OP_RESET                    0x0c03
#define HCI_OP_LE_SET_SCAN_PARAM        0x200B
#define HCI_OP_LE_SET_SCAN_ENABLE       0x200C
#define HCI_OP_LE_READ_REMOTE_FEATURES  0x2016

static volatile SInt32 remoteReadSCount = 0;
static volatile SInt32 leMetaStage = 0;

static inline void decrementRemoteReadCount()
{
    SInt32 oldVal;
    do {
        oldVal = remoteReadSCount;
        if (oldVal <= 0)
            break;
    } while (!OSCompareAndSwap(oldVal, oldVal - 1, &remoteReadSCount));
}

IOReturn CIntelBTPatcher::newHostDeviceRequest(void *that, IOService *provider, StandardUSB::DeviceRequest &request, void *data, IOMemoryDescriptor *descriptor, unsigned int &length, IOUSBHostCompletion *completion, unsigned int timeout)
{
    HciCommandHdr *hdr = nullptr;
    uint32_t hdrLen = 0;
    char hciBuf[MAX_HCI_BUF_LEN] = {0};

    bool isIntelBT = (_hookDeviceInstance == nullptr || that == _hookDeviceInstance) &&
                     ((request.bmRequestType & kDeviceRequestDirectionMask) == kRequestDirectionOut) &&
                     (((request.bmRequestType & kDeviceRequestTypeMask) >> kDeviceRequestTypePhase) == kRequestTypeClass);

    if (isIntelBT) {
        if (data == nullptr) {
            if (descriptor != nullptr) {
                bool prepared = (getKernelVersion() < KernelVersion::Sequoia) ||
                                (descriptor->prepare(kIODirectionOut) == kIOReturnSuccess);
                if (prepared) {
                    if (descriptor->getLength() > 0) {
                        hdrLen = (uint32_t)min(descriptor->getLength(), (IOByteCount)MAX_HCI_BUF_LEN);
                        descriptor->readBytes(0, hciBuf, hdrLen);
                    }
                    if (getKernelVersion() >= KernelVersion::Sequoia)
                        descriptor->complete(kIODirectionOut);
                }
            }
            if (hdrLen >= sizeof(HciCommandHdr))
                hdr = (HciCommandHdr *)hciBuf;
        } else if (request.wLength >= sizeof(HciCommandHdr)) {
            hdr = (HciCommandHdr *)data;
            hdrLen = request.wLength;
        }
    }

    if (hdr) {
        // HCI reset, we need to send Random address again and reset states
        if (hdr->opcode == HCI_OP_RESET) {
            _randomAddressInit = false;
            remoteReadSCount = 0;
            leMetaStage = 0;
        } else if (hdrLen >= 9 && hdr->opcode == HCI_OP_LE_SET_SCAN_PARAM && hdr->data[5] == 0x1) {
            if (!_randomAddressInit) {
                StandardUSB::DeviceRequest randReq = {};
                randReq.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionOut, kRequestTypeClass, kRequestRecipientInterface);
                randReq.bRequest = 0xE0;
                randReq.wIndex = 0;
                randReq.wValue = 0;
                randReq.wLength = sizeof(randomAddressHci);
                unsigned int randLen = sizeof(randomAddressHci);

                IOBufferMemoryDescriptor *randDesc = IOBufferMemoryDescriptor::withBytes(randomAddressHci, sizeof(randomAddressHci), kIODirectionInOut);
                if (randDesc) {
                    bool randPrepared = (getKernelVersion() < KernelVersion::Sequoia) ||
                                        (randDesc->prepare(kIODirectionInOut) == kIOReturnSuccess);
                    if (randPrepared) {
                        IOReturn ret = FunctionCast(newHostDeviceRequest, callbackIBTPatcher->oldHostDeviceRequest)(that, provider, randReq, nullptr, randDesc, randLen, nullptr, timeout);
                        if (getKernelVersion() >= KernelVersion::Sequoia)
                            randDesc->complete(kIODirectionInOut);
                        const char *randAddressDump = _hexDumpHCIData((uint8_t *)randomAddressHci, sizeof(randomAddressHci));
                        if (randAddressDump) {
                            SYSLOG(DRV_NAME, "[PATCH] Sending Random Address HCI %d %s", ret, randAddressDump);
                            IOFree((void *)randAddressDump, sizeof(randomAddressHci) * 3 + 1);
                        }
                        if (ret == kIOReturnSuccess)
                            _randomAddressInit = true;
                        SYSLOG(DRV_NAME, "[PATCH] Resend LE SCAN PARAM HCI %d", ret);
                    }
                    randDesc->release();
                }
            }
        } else if (hdr->opcode == HCI_OP_LE_READ_REMOTE_FEATURES) {
            // #514: send TWO extra reads -> two duplicate 0x04 events to rewrite
            // (1st dup -> fake PHY Update Complete, 2nd dup -> fake LE Data Length Change).
            for (int i = 0; i < 2; i++) {
                unsigned int dupLen = length;
                OSIncrementAtomic(&remoteReadSCount);
                IOReturn dupRet = FunctionCast(newHostDeviceRequest, callbackIBTPatcher->oldHostDeviceRequest)(that, provider, request, data, descriptor, dupLen, nullptr, timeout);
                if (dupRet != kIOReturnSuccess)
                    decrementRemoteReadCount();
            }
        }
#if DEBUG
        DBGLOG(DRV_NAME, "[%s] bRequest: 0x%x direction: %s type: %s recipient: %s wValue: 0x%02x wIndex: 0x%02x opcode: 0x%04x len: %d length: %d async: %d", provider->getName(), request.bRequest, requestDirectionNames[(request.bmRequestType & kDeviceRequestDirectionMask) >> kDeviceRequestDirectionPhase], requestRecipientNames[(request.bmRequestType & kDeviceRequestRecipientMask) >> kDeviceRequestRecipientPhase], requestTypeNames[(request.bmRequestType & kDeviceRequestTypeMask) >> kDeviceRequestTypePhase], request.wValue, request.wIndex, hdr->opcode, hdr->len, request.wLength, completion != nullptr);
        if (hdrLen) {
            const char *dump = _hexDumpHCIData((uint8_t *)hdr, hdrLen);
            if (dump) {
                DBGLOG(DRV_NAME, "[Request]: %s", dump);
                IOFree((void *)dump, hdrLen * 3 + 1);
            }
        }
#endif
    }
    return FunctionCast(newHostDeviceRequest, callbackIBTPatcher->oldHostDeviceRequest)(that, provider, request, data, descriptor, length, completion, timeout);
}

#define HCI_EVT_LE_META                               0x3E
#define HCI_EVT_LE_META_READ_REMOTE_FEATURES_COMPLETE 0x04

static void asyncIOCompletion(void* owner, void* parameter, IOReturn status, uint32_t bytesTransferred)
{
    AsyncOwnerData *asyncOwner = (AsyncOwnerData *)owner;
    IOMemoryDescriptor* dataBuffer = asyncOwner->dataBuffer;
    // #514 3-stage rewrite of the repeated LE Read Remote Features Complete (0x04) events:
    //   stage 0 = the real one (pass through), 1 = rewrite to fake PHY Update Complete,
    //   2 = rewrite to fake LE Data Length Change (clears bluetoothd's LE_READY_WAIT_FOR_LENGTH).

    if (status == kIOReturnSuccess && dataBuffer && bytesTransferred >= 6 && bytesTransferred <= MAX_HCI_BUF_LEN) {
        uint8_t buffer[MAX_HCI_BUF_LEN] = {0};
        bool prepared = (getKernelVersion() < KernelVersion::Sequoia) ||
                        (dataBuffer->prepare(kIODirectionInOut) == kIOReturnSuccess);
        if (prepared) {
            dataBuffer->readBytes(0, buffer, bytesTransferred);
            HciEventHdr *hdr = (HciEventHdr *)buffer;
            if (hdr->evt == HCI_EVT_LE_META && hdr->data[0] == HCI_EVT_LE_META_READ_REMOTE_FEATURES_COMPLETE) {
                SInt32 pendingCount = OSAddAtomic(0, &remoteReadSCount);
                SInt32 currentStage = leMetaStage;
                if (pendingCount <= 0) {
                    leMetaStage = 0;            // no duplicate reads pending -> pass through & ensure stage is 0
                } else if (currentStage == 0) {
                    leMetaStage = 1;            // real features complete -> pass through
                } else if (currentStage == 1) {  // 1st duplicate -> fake PHY Update Complete
                    uint8_t localPhyEvent[8] = {0x3E, 0x06, 0x0C, 0x00, hdr->data[2], hdr->data[3], 0x02, 0x02};
                    dataBuffer->writeBytes(0, localPhyEvent, sizeof(localPhyEvent));
                    bytesTransferred = sizeof(localPhyEvent);
                    leMetaStage = 2;
                    decrementRemoteReadCount();
                } else {                        // 2nd duplicate -> fake LE Data Length Change
                    uint8_t localDLCEvent[13] = {0x3E, 0x0B, 0x07, hdr->data[2], hdr->data[3], 0x1B, 0x00, 0x48, 0x01, 0x1B, 0x00, 0x48, 0x01};
                    dataBuffer->writeBytes(0, localDLCEvent, sizeof(localDLCEvent));
                    bytesTransferred = sizeof(localDLCEvent);
                    leMetaStage = 0;
                    decrementRemoteReadCount();
                }
            }
            if (getKernelVersion() >= KernelVersion::Sequoia)
                dataBuffer->complete(kIODirectionInOut);
        }
    }
    if (asyncOwner->action)
        asyncOwner->action(asyncOwner->owner, parameter, status, bytesTransferred);
    delete asyncOwner;
}

IOReturn CIntelBTPatcher::
newAsyncIO(void *that, IOMemoryDescriptor* dataBuffer, uint32_t bytesTransferred, IOUSBHostCompletion* completion, uint32_t completionTimeoutMs)
{
    AsyncOwnerData *ownerData = nullptr;
    IOUSBHostCompletion originalCompletion;

    if (OSAddAtomic(0, &remoteReadSCount) > 0 && that == _hookPipeInstance && completion) {
        ownerData = new AsyncOwnerData;
        if (ownerData) {
            originalCompletion = *completion;
            ownerData->action = completion->action;
            ownerData->owner = completion->owner;
            ownerData->dataBuffer = dataBuffer;
            completion->action = asyncIOCompletion;
            completion->owner = ownerData;
        }
    }
    IOReturn ret = FunctionCast(newAsyncIO, callbackIBTPatcher->oldAsyncIO)(that, dataBuffer, bytesTransferred, completion, completionTimeoutMs);
    if (ret != kIOReturnSuccess && ownerData) {
        *completion = originalCompletion;
        delete ownerData;
    }
    return ret;
}

#define VENDOR_USB_INTEL 0x8087

void CIntelBTPatcher::captureIntelInterruptPipe(void *that, StandardUSB::EndpointDescriptor const *descriptor, IOUSBHostDevice *device)
{
    if (!device || !descriptor)
        return;
    // Only capture pipe after IntelBluetoothFirmware has finished loading firmware and published "FirmwareLoaded" property.
    // During early boot, IntelBluetoothFirmware uses temporary pipes to upload firmware; we must never intercept those.
    if (!device->getProperty("FirmwareLoaded"))
        return;
    const StandardUSB::DeviceDescriptor *deviceDescriptor = device->getDeviceDescriptor();
    if (deviceDescriptor && deviceDescriptor->idVendor == VENDOR_USB_INTEL) {
        uint8_t epType = StandardUSB::getEndpointType(descriptor);
        uint8_t epDir = StandardUSB::getEndpointDirection(descriptor);
        if (epType == kIOUSBEndpointTypeInterrupt && epDir == StandardUSB::kEndpointDescriptorDirectionIn) {
            CIntelBTPatcher::_hookPipeInstance = that;
            CIntelBTPatcher::_hookDeviceInstance = device;
            _randomAddressInit = false;
            remoteReadSCount = 0;
            leMetaStage = 0;
        }
    }
}

int CIntelBTPatcher::
newInitPipe(void *that, StandardUSB::EndpointDescriptor const *descriptor, StandardUSB::SuperSpeedEndpointCompanionDescriptor const *superDescriptor, AppleUSBHostController *controller, IOUSBHostDevice *device, IOUSBHostInterface *interface, unsigned char a7, unsigned short a8)
{
    int ret = FunctionCast(newInitPipe, callbackIBTPatcher->oldInitPipe)(that, descriptor, superDescriptor, controller, device, interface, a7, a8);
    if (ret)
        captureIntelInterruptPipe(that, descriptor, device);
    return ret;
}

int CIntelBTPatcher::
newInitPipeConfig(void *that, StandardUSB::EndpointDescriptor const *descriptor, StandardUSB::ConfigurationDescriptor const *configDescriptor, AppleUSBHostController *controller, IOUSBHostDevice *device, IOUSBHostInterface *interface, unsigned char a7, unsigned short a8)
{
    int ret = FunctionCast(newInitPipeConfig, callbackIBTPatcher->oldInitPipeConfig)(that, descriptor, configDescriptor, controller, device, interface, a7, a8);
    if (ret)
        captureIntelInterruptPipe(that, descriptor, device);
    return ret;
}
