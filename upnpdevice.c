#include <OpenHome/Net/C/DvDevice.h>
#include <OpenHome/Net/C/DvAvOpenhomeOrgProduct1.h>
#include <string.h>
#include <unistd.h>

#include "DvVolume.h"
#include "DvInfo.h"
#include "DvReceiver.h"
#include "DvTime.h"
#include "player.h"
#include "upnpdevice.h"
#include "ipc.h"

void device_set_volume_limit(struct DeviceContext *dctx, unsigned int volume) {
    int changed;
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeLimit(dctx->volume, volume, &changed);
}

void device_set_volume(struct DeviceContext *dctx, unsigned int volume) {
    int changed;
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolume(dctx->volume, volume, &changed);
}

void device_set_mute(struct DeviceContext *dctx, unsigned int mute) {
    int changed;
    DvProviderAvOpenhomeOrgVolume1SetPropertyMute(dctx->volume, mute, &changed);
}

void device_set_transport_state(struct DeviceContext *dctx, const char *state) {
    DvProviderAvOpenhomeOrgReceiver1SetPropertyTransportState(dctx->receiver, state);
}

int32_t setsender_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, const char* aUri) {
    struct DeviceContext *dctx = aPtr;

    char *uri = strdup(aUri);

    struct ReceiverMessage msg = {
        .cmd = PLAY,
        .argp = uri
    };

    write(dctx->ctrl_fd, &msg, sizeof(msg));

    return 0;
}

int32_t stop_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr) {
    struct DeviceContext *dctx = aPtr;

    struct ReceiverMessage msg = {
        .cmd = STOP
    };

    write(dctx->ctrl_fd, &msg, sizeof(msg));

    return 0;
}

int32_t volume_characteristics_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aVolumeMax, uint32_t *aVolumeUnity, uint32_t *aVolumeSteps, uint32_t *aVolumeMilliDbPerStep, uint32_t *aBalanceMax, uint32_t *aFadeMax) {
    struct DeviceContext *dctx = aPtr;

    DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeMax(dctx->volume, aVolumeMax);
    DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeUnity(dctx->volume, aVolumeUnity);
    DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeSteps(dctx->volume, aVolumeSteps);
    DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeMilliDbPerStep(dctx->volume, aVolumeMilliDbPerStep);
    DvProviderAvOpenhomeOrgVolume1GetPropertyBalanceMax(dctx->volume, aBalanceMax);
    DvProviderAvOpenhomeOrgVolume1GetPropertyFadeMax(dctx->volume, aFadeMax);

    return 0;
}

int32_t volume_set_volume_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t aValue) {
    struct DeviceContext *dctx = aPtr;
  
    struct ReceiverMessage msg = {
        .cmd = SET_VOLUME,
        .arg = aValue
    };

    write(dctx->ctrl_fd, &msg, sizeof(msg));

    return 0;
}

int32_t volume_volume_inc_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr) {
    struct DeviceContext *dctx = aPtr;

    struct ReceiverMessage msg = {
        .cmd = VOLUME_INC
    };

    write(dctx->ctrl_fd, &msg, sizeof(msg));

    return 0;
}

int32_t volume_volume_dec_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr) {
    struct DeviceContext *dctx = aPtr;
  
    struct ReceiverMessage msg = {
        .cmd = VOLUME_DEC
    };

    write(dctx->ctrl_fd, &msg, sizeof(msg));

    return 0;
}

int32_t volume_volume_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aValue) {
    struct DeviceContext *dctx = aPtr;
    DvProviderAvOpenhomeOrgVolume1GetPropertyVolume(dctx->volume, aValue);

    return 0;
}
int32_t volume_volume_limit_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aValue) {
    struct DeviceContext *dctx = aPtr;
    DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeLimit(dctx->volume, aValue);

    return 0;
}

int32_t volume_set_mute_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t aValue) {
    struct DeviceContext *dctx = aPtr;
  
    struct ReceiverMessage msg = {
        .cmd = SET_MUTE,
        .arg = aValue
    };

    write(dctx->ctrl_fd, &msg, sizeof(msg));

    return 0;   
}

int32_t volume_mute_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aValue) {
    struct DeviceContext *dctx = aPtr;
    DvProviderAvOpenhomeOrgVolume1GetPropertyMute(dctx->volume, aValue);

    return 0;
}

int32_t productCallback(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, char **aRoom, char **aName, char **aInfo, char **aUrl, char **aImageUri)
{
    THandle product = aPtr;

    DvProviderAvOpenhomeOrgProduct1GetPropertyProductRoom(product, aRoom);
    DvProviderAvOpenhomeOrgProduct1GetPropertyProductName(product, aName);
    DvProviderAvOpenhomeOrgProduct1GetPropertyProductInfo(product, aInfo);
    DvProviderAvOpenhomeOrgProduct1GetPropertyProductUrl(product, aUrl);
    DvProviderAvOpenhomeOrgProduct1GetPropertyProductImageUri(product, aImageUri);

    return 0;
}

int32_t modelCallback(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, char **aName, char **aInfo, char **aUrl, char **aImageUri)
{
    THandle product = aPtr;

    DvProviderAvOpenhomeOrgProduct1GetPropertyModelName(product, aName);
    DvProviderAvOpenhomeOrgProduct1GetPropertyModelInfo(product, aInfo);
    DvProviderAvOpenhomeOrgProduct1GetPropertyModelUrl(product, aUrl);
    DvProviderAvOpenhomeOrgProduct1GetPropertyModelImageUri(product, aImageUri);

    return 0;
}

int32_t manufacturerCallback(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, char **aName, char **aInfo, char **aUrl, char **aImageUri)
{
    THandle product = aPtr;

    DvProviderAvOpenhomeOrgProduct1GetPropertyManufacturerName(product, aName);
    DvProviderAvOpenhomeOrgProduct1GetPropertyManufacturerInfo(product, aInfo);
    DvProviderAvOpenhomeOrgProduct1GetPropertyManufacturerUrl(product, aUrl);
    DvProviderAvOpenhomeOrgProduct1GetPropertyManufacturerImageUri(product, aImageUri);

    return 0;
}

int32_t standby_cb(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, uint32_t *aValue)
{
    THandle product = aPtr;
    DvProviderAvOpenhomeOrgProduct1GetPropertyStandby(product, aValue);

    return 0;
}

int32_t set_standby_cb(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, uint32_t aValue)
{
    THandle product = aPtr;
    int changed;
    DvProviderAvOpenhomeOrgProduct1SetPropertyStandby(product, aValue, &changed);

    if (changed)
        printf("Standby changed to %i\n", aValue);

    return 0;
}

int32_t attributesCallback(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, char **aValue) {
    THandle product = aPtr;
    DvProviderAvOpenhomeOrgProduct1GetPropertyAttributes(product, aValue);

    return 0;
}

int32_t sourcecount_cb(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, uint32_t *aValue) {
    THandle product = aPtr;
    DvProviderAvOpenhomeOrgProduct1GetPropertySourceCount(product, aValue);
    return 0;
}

int32_t sourceindex_cb(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, uint32_t *aValue) {
    THandle product = aPtr;
    DvProviderAvOpenhomeOrgProduct1GetPropertySourceIndex(product, aValue);
    return 0;
}

int32_t sourcexml_cb(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, char **aValue) {
    THandle product = aPtr;
    DvProviderAvOpenhomeOrgProduct1GetPropertySourceXml(product, aValue);

    return 0;
}

int32_t sourcexmlchangecount_cb(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, uint32_t *aValue) {
    THandle product = aPtr;
    *aValue = 1;

    return 0;
}

int32_t source_cb(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, uint32_t aIndex, char **aSystemName, char **aType, char **aName, uint32_t *aVisible) {
    THandle product = aPtr;

    if (aIndex != 0)
        return 1;

    *aSystemName = strdup("Songcast");
    *aType = strdup("Receiver");
    *aName = strdup("Songcast");
    *aVisible = 1;

    return 0;
}

int32_t setsourceindex_cb(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, uint32_t aIndex) {
    return 0;
}

int32_t setsourceindexbyname_cb(void *aPtr, IDvInvocationC *aInvocation, void *aInvocationPtr, const char *aName) {
    return 0;
}

void upnpdevice(player_t *player, struct DeviceContext *dctx, int ctrl_fd) {
    int changed;

    OhNetHandleInitParams initParams = OhNetInitParamsCreate();
    OhNetLibraryInitialise(initParams);
    OhNetLibraryStartDv();

    dctx->ctrl_fd = ctrl_fd;

    dctx->device = DvDeviceStandardCreateNoResources("fasdfkasdjfkl");
    DvDeviceSetAttribute(dctx->device, "Upnp.Domain", "av.openhome.org");
    DvDeviceSetAttribute(dctx->device, "Upnp.Type", "Source");
    DvDeviceSetAttribute(dctx->device, "Upnp.Version", "1");
    DvDeviceSetAttribute(dctx->device, "Upnp.Manufacturer", "OpenHome");
    DvDeviceSetAttribute(dctx->device, "Upnp.FriendlyName", "Spielzimmer");
    DvDeviceSetAttribute(dctx->device, "Upnp.ModelName", "Songcast Receiver");

    THandle product = DvProviderAvOpenhomeOrgProduct1Create(dctx->device);

    DvProviderAvOpenhomeOrgProduct1EnablePropertySourceCount(product);
    DvProviderAvOpenhomeOrgProduct1SetPropertySourceCount(product, 1, &changed);
    DvProviderAvOpenhomeOrgProduct1EnableActionSourceCount(product, sourcecount_cb, product);

    DvProviderAvOpenhomeOrgProduct1EnablePropertySourceIndex(product);
    DvProviderAvOpenhomeOrgProduct1SetPropertySourceIndex(product, 0, &changed);
    DvProviderAvOpenhomeOrgProduct1EnableActionSourceIndex(product, sourceindex_cb, product);

    DvProviderAvOpenhomeOrgProduct1EnableActionSetSourceIndex(product, setsourceindex_cb, product);
    DvProviderAvOpenhomeOrgProduct1EnableActionSetSourceIndexByName(product, setsourceindexbyname_cb, product);

    DvProviderAvOpenhomeOrgProduct1EnablePropertySourceXml(product);
    DvProviderAvOpenhomeOrgProduct1SetPropertySourceXml(product, "<SourceList><Source><Name>Songcast</Name><Type>Receiver</Type><Visible>True</Visible><SystemName>Songcast</SystemName></Source></SourceList>", &changed);
    DvProviderAvOpenhomeOrgProduct1EnableActionSourceXml(product, sourcexml_cb, product);
    DvProviderAvOpenhomeOrgProduct1EnableActionSourceXmlChangeCount(product, sourcexmlchangecount_cb, product);
    DvProviderAvOpenhomeOrgProduct1EnableActionSource(product, source_cb, product);

    DvProviderAvOpenhomeOrgProduct1EnablePropertyAttributes(product);
    DvProviderAvOpenhomeOrgProduct1SetPropertyAttributes(product, "Volume Info Time Receiver", &changed);
    DvProviderAvOpenhomeOrgProduct1EnableActionAttributes(product, attributesCallback, product);

    DvProviderAvOpenhomeOrgProduct1EnablePropertyManufacturerName(product);
    DvProviderAvOpenhomeOrgProduct1EnablePropertyManufacturerInfo(product);
    DvProviderAvOpenhomeOrgProduct1EnablePropertyManufacturerUrl(product);
    DvProviderAvOpenhomeOrgProduct1EnablePropertyManufacturerImageUri(product);

    DvProviderAvOpenhomeOrgProduct1SetPropertyManufacturerName(product, "DIY", &changed);
    DvProviderAvOpenhomeOrgProduct1SetPropertyManufacturerInfo(product, "", &changed);
    DvProviderAvOpenhomeOrgProduct1SetPropertyManufacturerUrl(product, "", &changed);
    DvProviderAvOpenhomeOrgProduct1SetPropertyManufacturerImageUri(product, "", &changed);

    DvProviderAvOpenhomeOrgProduct1EnablePropertyModelName(product);
    DvProviderAvOpenhomeOrgProduct1EnablePropertyModelInfo(product);
    DvProviderAvOpenhomeOrgProduct1EnablePropertyModelUrl(product);
    DvProviderAvOpenhomeOrgProduct1EnablePropertyModelImageUri(product);

    DvProviderAvOpenhomeOrgProduct1SetPropertyModelName(product, "Raspberry Pi", &changed);
    DvProviderAvOpenhomeOrgProduct1SetPropertyModelInfo(product, "", &changed);
    DvProviderAvOpenhomeOrgProduct1SetPropertyModelUrl(product, "", &changed);
    DvProviderAvOpenhomeOrgProduct1SetPropertyModelImageUri(product, "", &changed);

    DvProviderAvOpenhomeOrgProduct1EnablePropertyProductRoom(product);
    DvProviderAvOpenhomeOrgProduct1EnablePropertyProductName(product);
    DvProviderAvOpenhomeOrgProduct1EnablePropertyProductInfo(product);
    DvProviderAvOpenhomeOrgProduct1EnablePropertyProductUrl(product);
    DvProviderAvOpenhomeOrgProduct1EnablePropertyProductImageUri(product);

    DvProviderAvOpenhomeOrgProduct1SetPropertyProductRoom(product, "Spielzimmer", &changed);
    DvProviderAvOpenhomeOrgProduct1SetPropertyProductName(product, "Raspberry Pi", &changed);
    DvProviderAvOpenhomeOrgProduct1SetPropertyProductInfo(product, "", &changed);
    DvProviderAvOpenhomeOrgProduct1SetPropertyProductUrl(product, "", &changed);
    DvProviderAvOpenhomeOrgProduct1SetPropertyProductImageUri(product, "", &changed);

    DvProviderAvOpenhomeOrgProduct1EnableActionModel(product, modelCallback, product);
    DvProviderAvOpenhomeOrgProduct1EnableActionManufacturer(product, manufacturerCallback, product);
    DvProviderAvOpenhomeOrgProduct1EnableActionProduct(product, productCallback, product);

    DvProviderAvOpenhomeOrgProduct1EnablePropertyStandby(product);
    DvProviderAvOpenhomeOrgProduct1SetPropertyStandby(product, 0, &changed);
    DvProviderAvOpenhomeOrgProduct1EnableActionStandby(product, standby_cb, product);
    DvProviderAvOpenhomeOrgProduct1EnableActionSetStandby(product, set_standby_cb, product);

    THandle info = DvProviderOpenhomeOrgInfo1Create(dctx->device);
    THandle time = DvProviderOpenhomeOrgTime1Create(dctx->device);
    dctx->receiver = DvProviderOpenhomeOrgReceiver1Create(dctx->device);

    DvProviderAvOpenhomeOrgReceiver1SetSetSenderCallback(dctx->receiver, setsender_cb, dctx);
    DvProviderAvOpenhomeOrgReceiver1SetStopCallback(dctx->receiver, stop_cb, dctx);

    dctx->volume = DvProviderOpenhomeOrgVolume1Create(dctx->device);

    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolume(dctx->volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyMute(dctx->volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyBalance(dctx->volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyFade(dctx->volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeMax(dctx->volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeLimit(dctx->volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeUnity(dctx->volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeSteps(dctx->volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeMilliDbPerStep(dctx->volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyBalanceMax(dctx->volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyFadeMax(dctx->volume);

    DvProviderAvOpenhomeOrgVolume1SetPropertyVolume(dctx->volume, 0, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyMute(dctx->volume, 0, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyBalance(dctx->volume, 0, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyFade(dctx->volume, 0, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeMax(dctx->volume, 100, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeLimit(dctx->volume, 0, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeUnity(dctx->volume, 100, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeSteps(dctx->volume, 1, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeMilliDbPerStep(dctx->volume, 1024, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyBalanceMax(dctx->volume, 0, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyFadeMax(dctx->volume, 0, &changed);

    DvProviderAvOpenhomeOrgVolume1EnableActionCharacteristics(dctx->volume, volume_characteristics_cb, dctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionSetVolume(dctx->volume, volume_set_volume_cb, dctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionVolumeInc(dctx->volume, volume_volume_inc_cb, dctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionVolumeDec(dctx->volume, volume_volume_dec_cb, dctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionVolume(dctx->volume, volume_volume_cb, dctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionVolumeLimit(dctx->volume, volume_volume_limit_cb, dctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionSetMute(dctx->volume, volume_set_mute_cb, dctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionMute(dctx->volume, volume_mute_cb, dctx);
}

void device_enable(struct DeviceContext *dctx) {
    DvDeviceSetEnabled(dctx->device);
}
