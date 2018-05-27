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

typedef int32_t (STDCALL *CallbackVolume1Characteristics)(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aVolumeMax, uint32_t *aVolumeUnity, uint32_t *aVolumeSteps, uint32_t *aVolumeMilliDbPerStep, uint32_t *aBalanceMax, uint32_t *aFadeMax);
typedef int32_t (STDCALL *CallbackVolume1SetVolume)(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t aValue);
typedef int32_t (STDCALL *CallbackVolume1VolumeInc)(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr);
typedef int32_t (STDCALL *CallbackVolume1VolumeDec)(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr);
typedef int32_t (STDCALL *CallbackVolume1Volume)(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aValue);
typedef int32_t (STDCALL *CallbackVolume1VolumeLimit)(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aValue);
typedef int32_t (STDCALL *CallbackVolume1SetMute)(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t aValue);
typedef int32_t (STDCALL *CallbackVolume1Mute)(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aValue);


DllExport THandle STDCALL DvProviderOpenhomeOrgVolume1Create(DvDeviceC aDevice);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1Destroy(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnableActionCharacteristics(THandle aProvider, CallbackVolume1Characteristics aCallback, void* aPtr);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnableActionSetVolume(THandle aProvider, CallbackVolume1SetVolume aCallback, void* aPtr);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnableActionVolumeInc(THandle aProvider, CallbackVolume1VolumeInc aCallback, void* aPtr);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnableActionVolumeDec(THandle aProvider, CallbackVolume1VolumeDec aCallback, void* aPtr);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnableActionVolume(THandle aProvider, CallbackVolume1Volume aCallback, void* aPtr);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnableActionVolumeLimit(THandle aProvider, CallbackVolume1VolumeLimit aCallback, void* aPtr);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnableActionSetMute(THandle aProvider, CallbackVolume1SetMute aCallback, void* aPtr);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnableActionMute(THandle aProvider, CallbackVolume1Mute aCallback, void* aPtr);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyVolume(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyVolume(THandle aProvider, uint32_t* aValue);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyMute(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyMute(THandle aProvider, uint32_t* aValue);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyBalance(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyBalance(THandle aProvider, uint32_t* aValue);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyFade(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyFade(THandle aProvider, uint32_t* aValue);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeLimit(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeLimit(THandle aProvider, uint32_t* aValue);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeMax(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeMax(THandle aProvider, uint32_t* aValue);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeUnity(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeUnity(THandle aProvider, uint32_t* aValue);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeSteps(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeSteps(THandle aProvider, uint32_t* aValue);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeMilliDbPerStep(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeMilliDbPerStep(THandle aProvider, uint32_t* aValue);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyBalanceMax(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyBalanceMax(THandle aProvider, uint32_t* aValue);
DllExport int32_t STDCALL DvProviderAvOpenhomeOrgVolume1SetPropertyFadeMax(THandle aProvider, uint32_t aValue, uint32_t* aChanged);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1GetPropertyFadeMax(THandle aProvider, uint32_t* aValue);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyVolume(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyMute(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyBalance(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyFade(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeMax(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeLimit(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeUnity(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeSteps(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeMilliDbPerStep(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyBalanceMax(THandle aProvider);
DllExport void STDCALL DvProviderAvOpenhomeOrgVolume1EnablePropertyFadeMax(THandle aProvider);


#ifdef __cplusplus
} // extern "C"
#endif

