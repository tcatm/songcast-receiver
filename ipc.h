#pragma once

enum ReceiverCommand {PLAY, STOP, SET_MUTE, VOLUME_INC, VOLUME_DEC, SET_VOLUME};

struct ReceiverMessage {
    enum ReceiverCommand cmd;
    int arg;
    void *argp;
};