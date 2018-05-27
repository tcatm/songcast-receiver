/**
 * Provider for the av.openhome.org:Volume:1 UPnP service
 */
#pragma once

#include <OpenHome/OsTypes.h>
#include <OpenHome/Net/C/DvDevice.h>
#include <OpenHome/Net/C/DvInvocation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (STDCALL *CallbackReceiver1SetSender)(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, const char* uri);

DllExport THandle STDCALL DvProviderOpenhomeOrgReceiver1Create(DvDeviceC aDevice);
DllExport void STDCALL DvProviderAvOpenhomeOrgReceiver1Destroy(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgReceiver1SetSetSenderCallback(THandle aProvider, CallbackReceiver1SetSender aCallback, void* aPtr);

#ifdef __cplusplus
} // extern "C"
#endif

