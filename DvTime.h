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

DllExport THandle STDCALL DvProviderOpenhomeOrgTime1Create(DvDeviceC aDevice);
DllExport void STDCALL DvProviderAvOpenhomeOrgTime1Destroy(THandle aProvider);

#ifdef __cplusplus
} // extern "C"
#endif

