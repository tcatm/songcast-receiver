#include <pulse/pulseaudio.h>
#include <error.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#include "player.h"
#include "output.h"
#include "cache.h"

// TODO determine CACHE_SIZE dynamically based on latency? 192/24 needs a larger cache
// TODO determine BUFFER_LATENCY automagically
#define CACHE_SIZE 2000 // frames
#define BUFFER_LATENCY 60e3 // 60ms buffer latency

// prototypes
bool process_frame(player_t *player, struct audio_frame *frame);
void play_audio(player_t *player, pa_stream *s, size_t writable, struct cache_info *info);
void write_data(player_t *player, pa_stream *s, size_t request);
void set_state(player_t *player, enum PlayerState new_state);

// callbacks
void write_cb(pa_stream *s, size_t request, void *userdata) {
  player_t *player = userdata;

  assert(s == player->pulse.stream);

  pthread_mutex_lock(&player->mutex);

  write_data(player, s, request);

  pthread_mutex_unlock(&player->mutex);
}

void underflow_cb(pa_stream *s, void *userdata) {
  player_t *player = userdata;

  assert(s == player->pulse.stream);

  printf("Underflow!\n");

  pthread_mutex_lock(&player->mutex);
  set_state(player, HALT);
  pthread_mutex_unlock(&player->mutex);
}

struct output_cb callbacks = {
  .write = write_cb,
  .underflow = underflow_cb,
};

// functions
uint64_t now_usec(void) {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  return (long long)now.tv_sec * 1000000 + (now.tv_nsec + 500) / 1000;
}

char *print_state(enum PlayerState state) {
  switch (state) {
    case STOPPED:
      return "STOPPED";
      break;
    case STARTING:
      return "STARTING";
      break;
    case PLAYING:
      return "PLAYING";
      break;
    case HALT:
      return "HALT";
      break;
    default:
      assert(false && "UNKNOWN STATE");
  }
}

void set_state(player_t *player, enum PlayerState new_state) {
  printf("--> State change: %s to %s <--\n", print_state(player->state), print_state(new_state));
  player->state = new_state;
}

void stop(player_t *player) {
  printf("Stopping stream.\n");
  stop_stream(&player->pulse);
  set_state(player, STOPPED);
}

void player_init(player_t *player) {
  pthread_mutex_init(&player->mutex, NULL);

  set_state(player, STOPPED);
  player->cache = cache_init(CACHE_SIZE);
  output_init(&player->pulse);
}

// Stop playback
void player_stop(player_t *player) {
  pthread_mutex_lock(&player->mutex);

  if (player->state != STOPPED) {
    stop(player);
    set_state(player, STOPPED);
  }

  cache_reset(player->cache);

  pthread_mutex_unlock(&player->mutex);
}

void try_prepare(player_t *player) {
  if (player->state != STOPPED)
    return;

  struct audio_frame *start = player->cache->frames[cache_pos(player->cache, 0)];

  if (start == NULL)
    return;

  printf("Preparing stream.\n");

  assert(player->state == STOPPED);

  set_state(player, STARTING);
  player->timing = (struct timing){};

  kalman_init(&player->timing.kalman_netlocal_ratio);
  kalman_init(&player->timing.kalman_rtp);
  kalman_init(&player->timing.kalman_rate);

  pa_buffer_attr bufattr = {
    .maxlength = -1,
    .minreq = -1,
    .prebuf = -1,
    .tlength = pa_usec_to_bytes(BUFFER_LATENCY, &start->ss),
  };

  pthread_mutex_unlock(&player->mutex);

  create_stream(&player->pulse, &start->ss, &bufattr, player, &callbacks);

  pthread_mutex_lock(&player->mutex);

  printf("Stream prepared\n");
}

// TODO wants player. is it really needed? cache is used. timing is used.
// TODO what pre-conditions need to be met? cache must be present, stream is required
void write_data(player_t *player, pa_stream *s, size_t request) {
  if (player->state == STOPPED)
    return;

  const pa_sample_spec *ss = pa_stream_get_sample_spec(s);

  if (player->timing.pa == NULL)
    player->timing.pa = pa_stream_get_timing_info(s);

  pa_operation *o = pa_stream_update_timing_info(s, NULL, NULL);

  if (o != NULL)
    pa_operation_unref(o);

  struct cache_info info = cache_continuous_size(player->cache);

  int64_t start_at = info.start + info.latency_usec;
  int delta;
  uint64_t play_at;

  double rtp;

  if (player->timing.pa != NULL && player->timing.pa->playing == 1) {
    pa_timing_info ti = *player->timing.pa;

    uint64_t ts = (uint64_t)ti.timestamp.tv_sec * 1000000 + ti.timestamp.tv_usec;
    int playback_latency = ti.sink_usec + ti.transport_usec +
                           pa_bytes_to_usec(ti.write_index - ti.read_index, ss);

    play_at = ts + playback_latency;
    delta = play_at - start_at;

    if (player->timing.pa_offset_bytes == 0) {
      // Prepare timing information
      player->timing.ss = *ss;
      player->timing.start_local_usec = play_at;
      player->timing.pa_offset_bytes = ti.write_index;
    } else {
      double elapsed_local = ts + playback_latency - player->timing.start_local_usec;
      double elapsed_audio = pa_bytes_to_usec(ti.write_index, ss) - pa_bytes_to_usec(player->timing.pa_offset_bytes, ss);
      double audio_local_ratio = elapsed_audio / elapsed_local;

      if (info.has_timing) {
        kalman_run(&player->timing.kalman_netlocal_ratio, info.clock_ratio, info.clock_ratio_error);

        int write_latency = pa_bytes_to_usec(ti.write_index - ti.read_index, ss);

        // Receive-To-Play is only defined while audio is playing
        if (player->state == PLAYING) {
          rtp = kalman_run(&player->timing.kalman_rtp, delta, info.start_error);
        }

        if (!isnan(audio_local_ratio)) {
          double netlocal_error_rel = player->timing.kalman_netlocal_ratio.error / player->timing.kalman_netlocal_ratio.est;
          double ratio = player->timing.kalman_netlocal_ratio.est / audio_local_ratio;
          double ratio_error = fabs(ratio) * netlocal_error_rel;
          double rate = player->timing.ss.rate * ratio;
          double rate_error = player->timing.ss.rate * ratio_error;
          double est_rate = kalman_run(&player->timing.kalman_rate, rate, rate_error);
          double est_rate_error = player->timing.kalman_rate.error;

          printf("rtp %.2f±%.2f (%d) usec\trate %f±%f Hz\ttotal_ratio %.20g\n",
                 rtp, player->timing.kalman_rtp.error, delta, est_rate, est_rate_error, ratio);

          pa_stream_update_sample_rate(s, 44129, NULL, NULL);
        }
      }
    }
  }

  switch (player->state) {
    case PLAYING:
      break;
    case STARTING:
      if (player->timing.pa == NULL || player->timing.pa->playing == 0)
        goto silence;

      printf("Request for %zd bytes (playing = %i), can start at %ld, would play at %ld, in %d usec\n", request, player->timing.pa->playing, start_at, play_at, delta);

      // Can't start playing when we don't have a timing information
      if (!info.has_timing)
        goto silence;

      if (delta < 0)
        goto silence;

      size_t skip = pa_usec_to_bytes(delta, ss);

      printf("Need to skip %zd bytes, have %zd bytes\n", skip, info.available);

      if (info.available < request + skip) {
        printf("Not enough data in buffer to start (missing %zd bytes)\n", request - skip - info.available);
        goto silence;
      }

      printf("Request can be fullfilled.\n");

      trim_cache(player->cache, skip);

      // Adjust cache info
      info.start += delta;
      info.available -= skip;

      set_state(player, PLAYING);
      break;
    case STOPPED:
    case HALT:
    default:
      pthread_mutex_unlock(&player->mutex);
      return;
      break;
  }

  play_audio(player, s, request, &info);
  player->timing.written += request;

  return;

uint8_t *silence;

silence:
  silence = calloc(1, request);
  pa_stream_write(s, silence, request, NULL, 0, PA_SEEK_RELATIVE);
  free(silence);
}

void play_audio(player_t *player, pa_stream *s, size_t writable, struct cache_info *info) {
  size_t written = 0;

  //pa_usec_t available_usec = pa_bytes_to_usec(info->available, pa_stream_get_sample_spec(s));
  //printf("asked for %6i (%6iusec), available %6i (%6iusec)\n",
//         writable, pa_bytes_to_usec(writable, pa_stream_get_sample_spec(s)),
//         info.available, available_usec);

  while (writable > 0) {
    int pos = cache_pos(player->cache, 0);
    struct audio_frame *frame = player->cache->frames[pos];

    if (frame == NULL) {
      printf("Missing frame.\n");
      break;
    }

    if (!pa_sample_spec_equal(&player->timing.ss, &frame->ss)) {
      printf("Sample spec mismatch.\n");
      break;
    }

    bool consumed = true;
    void *data = frame->readptr;
    size_t length = frame->audio_length;

    if (writable < frame->audio_length) {
      consumed = false;
      length = writable;
      frame->readptr = (uint8_t*)frame->readptr + length;
      frame->audio_length -= length;
    }

    pa_stream_write(s, data, length, NULL, 0LL, PA_SEEK_RELATIVE);

    written += length;
    writable -= length;

    if (consumed) {
      bool halt = frame->halt;

      free_frame(frame);
      player->cache->frames[pos] = NULL;
      player->cache->offset++;
      player->cache->start_seqnum++;
      if (player->cache->latest_index > 0)
        player->cache->latest_index--;

      if (halt) {
        printf("HALT received.\n");
        set_state(player, HALT);
        return;
      }
    }
  }

  if (writable > 0) {
    printf("Not enough data. Stopping.\n");
    set_state(player, HALT);
    return;
  }

  //printf("written %i byte, %uusec\n", written, pa_bytes_to_usec(written, pa_stream_get_sample_spec(s)));
}

struct missing_frames *handle_frame(player_t *player, ohm1_audio *frame, struct timespec *ts) {
  struct missing_frames *missing = NULL;
  struct audio_frame *aframe = parse_frame(frame);

  if (aframe == NULL)
    return NULL;

  // TODO incorporate any network latencies and such into ts_due_usec
  aframe->ts_recv_usec = (long long)ts->tv_sec * 1000000 + (ts->tv_nsec + 500) / 1000;

  pthread_mutex_lock(&player->mutex);

  if (player->state == HALT)
    stop(player);

  bool consumed = process_frame(player, aframe);

  if (consumed) {
    fixup_timestamps(player->cache);

    // Don't send resend requests when the frame was an answer.
    if (!aframe->resent)
      missing = request_frames(player->cache);

    try_prepare(player);
  } else {
    free_frame(aframe);
  }

  pthread_mutex_unlock(&player->mutex);

  return missing;
}

bool process_frame(player_t *player, struct audio_frame *frame) {
  cache_seek_forward(player->cache, frame->seqnum);

  // The index stays fixed while this function runs.
  int index = frame->seqnum - player->cache->start_seqnum;
  int pos = cache_pos(player->cache, index);
  if (index < 0)
    return false;

  //printf("Handling frame %i %s\n", frame->seqnum, frame->resent ? "(resent)" : "");

  if (player->cache->frames[pos] != NULL)
    return false;

  player->cache->frames[pos] = frame;

  if (index > player->cache->latest_index)
    player->cache->latest_index = index;

  return true;
}
