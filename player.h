#pragma once

#include <time.h>
#include <stdint.h>
#include <samplerate.h>

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

struct remote_clock {
  unsigned int ts_remote_last;
  uint64_t ts_local_last;
  uint64_t ts_local_0;
  double ts_remote;
  int delta;

  kalman2d_t filter;

  bool invalid;
};

struct timing {
  uint64_t start_play_usec;
  int64_t start_local_usec;
  int64_t last_frame_ts;
  size_t pa_offset_bytes;
  size_t written;
  pa_sample_spec ss;
  double ratio;

  uint64_t local_last;

  kalman2d_t pa_filter;
};

typedef struct {
  pthread_mutex_t mutex;
  enum PlayerState state;
  struct cache *cache;
  struct pulse pulse;
  struct timing timing;
  struct remote_clock remote_clock;
  SRC_STATE *src;
} player_t;

void player_init(player_t *player);
void player_stop(player_t *player);
struct missing_frames *handle_frame(player_t *player, ohm1_audio *frame, struct timespec *ts);
