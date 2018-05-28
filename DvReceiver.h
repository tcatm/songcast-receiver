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
typedef int32_t (STDCALL *CallbackReceiver1Stop)(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr);

DllExport THandle STDCALL DvProviderOpenhomeOrgReceiver1Create(DvDeviceC aDevice);
DllExport void STDCALL DvProviderAvOpenhomeOrgReceiver1Destroy(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgReceiver1SetSetSenderCallback(THandle aProvider, CallbackReceiver1SetSender aCallback, void* aPtr);
DllExport void STDCALL DvProviderAvOpenhomeOrgReceiver1SetStopCallback(THandle aProvider, CallbackReceiver1Stop aCallback, void* aPtr);
DllExport void STDCALL DvProviderAvOpenhomeOrgReceiver1SetPropertyTransportState(THandle aProvider, const char *aValue);

#ifdef __cplusplus
} // extern "C"
#endif

