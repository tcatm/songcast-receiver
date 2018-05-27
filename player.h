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
  uint64_t avg_start_at;
  uint avg_start_at_j;
  uint64_t avg_play_at;
  uint64_t start_play_usec;
  int64_t start_local_usec;
  int64_t last_frame_ts;
  size_t pa_offset_bytes;
  size_t written_pre, written_post;
  pa_sample_spec ss;
  double ratio;
  double delta[100];
  int n_delta;

  double estimated_rate, avg_estimated_rate;

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
  int volume;
  int volume_limit;
  int mute;
} player_t;

void player_init(player_t *player);
void player_stop(player_t *player);
struct missing_frames *handle_frame(player_t *player, ohm1_audio *frame, struct timespec *ts);

void player_set_mute(player_t *player, int mute);
int player_get_mute(player_t *player);
void player_set_volume(player_t *player, int volume);
void player_inc_volume(player_t *player);
void player_dec_volume(player_t *player);
int player_get_volume(player_t *player);
int player_get_volume_max(player_t *player);
int player_get_volume_limit(player_t *player);
