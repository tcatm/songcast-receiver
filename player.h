#pragma once

#include <time.h>
#include <stdint.h>

#include "ohm_v1.h"
#include "output.h"
#include "cache.h"
#include "audio_frame.h"
#include "kalman.h"

enum PlayerState {STOPPED, STARTING, PLAYING, HALT};
/*
  STOPPED
    There is no stream.

  STARTING
    A stream has been created and it is waiting for data.

  PLAYING
    Audio data is being passed to the stream.

  HALT
    The stream must be stopped.
*/

struct timing {
  const pa_timing_info *pa;
  int64_t start_local_usec;
  int64_t last_frame_ts;
  size_t pa_offset_bytes;
  size_t written;
  kalman_t kalman_netlocal_ratio;
  kalman_t kalman_rtp;
  pa_sample_spec ss;
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
