#pragma once

#include "player.h"

void upnpdevice(player_t *player, struct DeviceContext *dctx, int ctrl_fd);
void device_enable(struct DeviceContext *dctx);
void device_set_volume_limit(struct DeviceContext *dctx, unsigned int volume);
void device_set_volume(struct DeviceContext *dctx, unsigned int volume);
void device_set_mute(struct DeviceContext *dctx, unsigned int mute);
void device_set_transport_state(struct DeviceContext *dctx, const char *state);
