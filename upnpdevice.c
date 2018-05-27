#include <OpenHome/Net/C/DvDevice.h>
#include <OpenHome/Net/C/DvAvOpenhomeOrgProduct1.h>

#include "DvVolume.h"
#include "DvInfo.h"
#include "player.h"

struct VolumeCtx {
    player_t *player;
    THandle volume;
};

struct ReceiverCtx {
    player_t *player;
    FILE *ctrl_stream;
};

int32_t setsender_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, char* aUri) {
    struct ReceiverCtx *rctx = aPtr;

    const char *cmd = "uri ";

    int l = strlen(aUri);

    char *s = malloc(l + strlen(cmd) + 1);

    strcpy(s, cmd);
    strcat(s, aUri);

    fputs(s, rctx->ctrl_stream);
    fflush(rctx->ctrl_stream);

    free(s);
    return 0;
}

int32_t volume_characteristics_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aVolumeMax, uint32_t *aVolumeUnity, uint32_t *aVolumeSteps, uint32_t *aVolumeMilliDbPerStep, uint32_t *aBalanceMax, uint32_t *aFadeMax) {
    struct VolumeCtx *vctx = aPtr;

    DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeMax(vctx->volume, aVolumeMax);
    DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeUnity(vctx->volume, aVolumeUnity);
    DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeSteps(vctx->volume, aVolumeSteps);
    DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeMilliDbPerStep(vctx->volume, aVolumeMilliDbPerStep);
    DvProviderAvOpenhomeOrgVolume1GetPropertyBalanceMax(vctx->volume, aBalanceMax);
    DvProviderAvOpenhomeOrgVolume1GetPropertyFadeMax(vctx->volume, aFadeMax);

    return 0;
}

int32_t volume_set_volume_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t aValue) {
    struct VolumeCtx *vctx = aPtr;
    int changed;
    player_set_volume(vctx->player, aValue);
    uint32_t newVolume = player_get_volume(vctx->player);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolume(vctx->volume, newVolume, &changed);

    return 0;
}

int32_t volume_volume_inc_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr) {
    struct VolumeCtx *vctx = aPtr;
    int changed;
    uint32_t aValue;
    player_inc_volume(vctx->player);
    aValue = player_get_volume(vctx->player);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolume(vctx->volume, aValue, &changed);

    return 0;
}

int32_t volume_volume_dec_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr) {
    struct VolumeCtx *vctx = aPtr;
    int changed;
    uint32_t aValue;
    player_dec_volume(vctx->player);
    aValue = player_get_volume(vctx->player);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolume(vctx->volume, aValue, &changed);

    return 0;
}

int32_t volume_volume_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aValue) {
    struct VolumeCtx *vctx = aPtr;
    DvProviderAvOpenhomeOrgVolume1GetPropertyVolume(vctx->volume, aValue);

    return 0;
}
int32_t volume_volume_limit_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aValue) {
    struct VolumeCtx *vctx = aPtr;
    DvProviderAvOpenhomeOrgVolume1GetPropertyVolumeLimit(vctx->volume, aValue);

    return 0;
}

int32_t volume_set_mute_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t aValue) {
    struct VolumeCtx *vctx = aPtr;
    int changed;
    player_set_mute(vctx->player, aValue);
    uint32_t newMute = player_get_mute(vctx->player);
    DvProviderAvOpenhomeOrgVolume1SetPropertyMute(vctx->volume, newMute, &changed);

    return 0;   
}

int32_t volume_mute_cb(void* aPtr, IDvInvocationC* aInvocation, void* aInvocationPtr, uint32_t *aValue) {
    struct VolumeCtx *vctx = aPtr;
    DvProviderAvOpenhomeOrgVolume1GetPropertyMute(vctx->volume, aValue);

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

void upnpdevice(player_t *player, int ctrl_fd) {

    FILE *ctrl_stream;
    ctrl_stream = fdopen(ctrl_fd, "w");

    int changed;
    OhNetHandleInitParams initParams = OhNetInitParamsCreate();
    OhNetLibraryInitialise(initParams);
    OhNetLibraryStartDv();

    DvDeviceC device = DvDeviceStandardCreateNoResources("fasdfkasdjfkl");
    DvDeviceSetAttribute(device, "Upnp.Domain", "av.openhome.org");
    DvDeviceSetAttribute(device, "Upnp.Type", "Source");
    DvDeviceSetAttribute(device, "Upnp.Version", "1");
    DvDeviceSetAttribute(device, "Upnp.Manufacturer", "OpenHome");
    DvDeviceSetAttribute(device, "Upnp.FriendlyName", "Spielzimmer");
    DvDeviceSetAttribute(device, "Upnp.ModelName", "Songcast Receiver");

    THandle product = DvProviderAvOpenhomeOrgProduct1Create(device);

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

    THandle info = DvProviderOpenhomeOrgInfo1Create(device);
    THandle time = DvProviderOpenhomeOrgTime1Create(device);
    THandle receiver = DvProviderOpenhomeOrgReceiver1Create(device);

    struct ReceiverCtx rctx = {
        .player = player,
        .ctrl_stream = ctrl_stream,
    };

    DvProviderAvOpenhomeOrgReceiver1SetSetSenderCallback(receiver, setsender_cb, &rctx);

    THandle volume = DvProviderOpenhomeOrgVolume1Create(device);

    struct VolumeCtx vctx = {
        .player = player,
        .volume = volume,
    };

    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolume(volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyMute(volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyBalance(volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyFade(volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeMax(volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeLimit(volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeUnity(volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeSteps(volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyVolumeMilliDbPerStep(volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyBalanceMax(volume);
    DvProviderAvOpenhomeOrgVolume1EnablePropertyFadeMax(volume);

    DvProviderAvOpenhomeOrgVolume1SetPropertyVolume(volume, player_get_volume(player), &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyMute(volume, player_get_mute(player), &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyBalance(volume, 0, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyFade(volume, 0, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeMax(volume, player_get_volume_max(player), &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeLimit(volume, player_get_volume_limit(player), &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeUnity(volume, player_get_volume_max(player), &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeSteps(volume, 1, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyVolumeMilliDbPerStep(volume, 1024, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyBalanceMax(volume, 0, &changed);
    DvProviderAvOpenhomeOrgVolume1SetPropertyFadeMax(volume, 0, &changed);

    DvProviderAvOpenhomeOrgVolume1EnableActionCharacteristics(volume, volume_characteristics_cb, &vctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionSetVolume(volume, volume_set_volume_cb, &vctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionVolumeInc(volume, volume_volume_inc_cb, &vctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionVolumeDec(volume, volume_volume_dec_cb, &vctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionVolume(volume, volume_volume_cb, &vctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionVolumeLimit(volume, volume_volume_limit_cb, &vctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionSetMute(volume, volume_set_mute_cb, &vctx);
    DvProviderAvOpenhomeOrgVolume1EnableActionMute(volume, volume_mute_cb, &vctx);

    DvDeviceSetEnabled(device);
}