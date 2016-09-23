#pragma once

#include <time.h>
#include <stdint.h>

#include "ohm_v1.h"
#include "output.h"
#include "cache.h"
#include "audio_frame.h"

#define BUFFER_LATENCY 60e3 // 60ms buffer latency

enum PlayerState {STOPPED, STARTING, PLAYING, HALT, STOPPING};
/*
  STOPPED
    There is no stream.

  STARTING
    A stream has been created and it is waiting for data.

  PLAYING
    Audio data is being passed to the stream.

  HALT
    The stream has run out of audio data.

  STOPPING
    The stream is being drained and will be shut down.
*/

struct timing {
  const pa_timing_info *pa;
  int64_t start_local_usec;
  int64_t initial_net_offset;
  int64_t last_frame_ts;
  size_t pa_offset_bytes;
  size_t written;
};

typedef struct {
  pthread_mutex_t mutex;
  enum PlayerState state;
  struct cache *cache;
  struct pulse pulse;
  struct timing timing;
} player_t;

void player_init(player_t *player);
void player_stop(player_t *player);
struct missing_frames *handle_frame(player_t *player, ohm1_audio *frame, struct timespec *ts);
