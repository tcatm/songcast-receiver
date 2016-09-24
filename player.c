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
void play_audio(player_t *player, pa_stream *s, size_t writable);
void write_data(player_t *player, pa_stream *s, size_t request);
void set_state(player_t *player, enum PlayerState new_state);
void update_timing(player_t *player);

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

void latency_cb(pa_stream *s, void *userdata) {
  player_t *player = userdata;

  assert(s == player->pulse.stream);

  pthread_mutex_lock(&player->mutex);
  update_timing(player);
  pthread_mutex_unlock(&player->mutex);
}

struct output_cb callbacks = {
  .write = write_cb,
  .underflow = underflow_cb,
  .latency = latency_cb,
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
  player->timing = (struct timing){
    .ss = start->ss
  };

  kalman_init(&player->timing.kalman_netlocal_ratio);
  kalman_init(&player->timing.kalman_rtp);

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

static bool prepare_for_start(player_t *player, size_t request) {
  const pa_sample_spec *ss = pa_stream_get_sample_spec(player->pulse.stream);
  const pa_timing_info *ti = pa_stream_get_timing_info(player->pulse.stream);

  if (ti == NULL || ti->playing != 1)
    return false;

  struct cache_info info = cache_continuous_size(player->cache);

  uint64_t ts = (uint64_t)ti->timestamp.tv_sec * 1000000 + ti->timestamp.tv_usec;
  int playback_latency = ti->sink_usec + ti->transport_usec +
                         pa_bytes_to_usec(ti->write_index - ti->read_index, ss);

  uint64_t start_at = info.start + info.latency_usec;
  uint64_t play_at = ts + playback_latency;
  int delta = play_at - start_at;

  printf("Request for %zd bytes), can start at %ld, would play at %ld, in %d usec\n", request, start_at, play_at, delta);

  if (delta < 0)
    return false;

  size_t skip = pa_usec_to_bytes(delta, ss);

  printf("Need to skip %zd bytes, have %zd bytes\n", skip, info.available);

  if (info.available < request + skip) {
    printf("Not enough data in buffer to start (missing %zd bytes)\n", request - skip - info.available);
    return false;
  }

  printf("Request can be fullfilled.\n");

  trim_cache(player->cache, skip);
  return true;
}

// TODO wants player. is it really needed? cache is used. timing is used.
// TODO what pre-conditions need to be met? cache must be present, stream is required
void write_data(player_t *player, pa_stream *s, size_t request) {
  if (player->state == STOPPED)
    return;

  pa_operation *o = pa_stream_update_timing_info(s, NULL, NULL);

  if (o != NULL)
    pa_operation_unref(o);

  switch (player->state) {
    case PLAYING:
      goto play;
      break;
    case STARTING:
      if (prepare_for_start(player, request)) {
        set_state(player, PLAYING);
        goto play;
      }

      goto silence;
      break;
    case STOPPED:
    case HALT:
    default:
      pthread_mutex_unlock(&player->mutex);
      return;
      break;
  }

  return;

uint8_t *silence;

play:
  play_audio(player, s, request);
  player->timing.written += request;
  return;

silence:
  silence = calloc(1, request);
  pa_stream_write(s, silence, request, NULL, 0, PA_SEEK_RELATIVE);
  free(silence);
  return;
}

void play_audio(player_t *player, pa_stream *s, size_t writable) {
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

  // TODO can we do netlocal ratio calculation here?
  // TODO needs three frames (predecessor and successor)
  // TODO this could simplify cache.c and also avoid fixup_timestamps
  // TODO HALT frame should reset timing, also reset on large gap?
  // TODO maybe reset when cache is empty?

  return true;
}

void update_timing(player_t *player) {
  const pa_sample_spec *ss = pa_stream_get_sample_spec(player->pulse.stream);
  pa_timing_info ti = *pa_stream_get_timing_info(player->pulse.stream);
  struct cache_info info = cache_continuous_size(player->cache);

  if (ti.playing != 1)
    return;

  int frame_size = pa_frame_size(&player->timing.ss);

  uint64_t ts = (uint64_t)ti.timestamp.tv_sec * 1000000 + ti.timestamp.tv_usec;
  int playback_latency = ti.sink_usec + ti.transport_usec +
                         pa_bytes_to_usec(ti.write_index - ti.read_index, ss);

  uint64_t start_at = info.start + info.latency_usec;

  uint64_t play_at = ts + playback_latency;
  int delta = play_at - start_at;
  double rtp;

  if (player->timing.pa_offset_bytes == 0) {
    // Prepare timing information
    player->timing.start_local_usec = ts;
    player->timing.pa_offset_bytes = ti.read_index;
  } else {
    double elapsed_audio = pa_bytes_to_usec(ti.read_index - player->timing.pa_offset_bytes, ss);
    if (elapsed_audio > 0) {
      double elapsed_audio_rel = pa_bytes_to_usec(frame_size, ss) / elapsed_audio;
      double elapsed_local = ts - player->timing.start_local_usec;
      // Assume 800µs jitter in local clock
      double elapsed_local_rel = 800 / elapsed_local;
      double alr = elapsed_audio / elapsed_local;
      double alr_error =  fabs(alr) * sqrt(elapsed_audio_rel * elapsed_audio_rel + elapsed_local_rel * elapsed_local_rel);

      if (info.has_timing) {
        kalman_run(&player->timing.kalman_netlocal_ratio, info.clock_ratio, info.clock_ratio_error);

        int write_latency = pa_bytes_to_usec(ti.write_index - ti.read_index, ss);

        // Receive-To-Play is only defined while audio is playing
        if (player->state == PLAYING) {
          rtp = kalman_run(&player->timing.kalman_rtp, delta, info.start_error);
        }

        if (!isnan(alr)) {
          double ratio = player->timing.kalman_netlocal_ratio.est / alr;
          double nlr_rel = player->timing.kalman_netlocal_ratio.rel;
          double alr_rel = alr_error / alr;
          double ratio_error = fabs(ratio) * sqrt(nlr_rel * nlr_rel + alr_rel * alr_rel);
          double rate = player->timing.ss.rate * ratio;
          double rate_error = player->timing.ss.rate * ratio_error;
          double dhz = rate - ss->rate;

          printf("nlr %.6f±%.6f\talr %.6f±%.6f\tratio %.6f±%.6f\trtp %.2f±%.2f (%d) usec\trate %f±%f Hz (\tΔ %f Hz)\t\n",
                 player->timing.kalman_netlocal_ratio.est, player->timing.kalman_netlocal_ratio.error,
                 alr, alr_error, ratio, ratio_error,
                 rtp, player->timing.kalman_rtp.error, delta, rate, rate_error, dhz);

          //  pa_stream_update_sample_rate(s, 44129, NULL, NULL);
        }
      }
    }
  }
}
