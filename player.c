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

  switch (player->state) {
    case PLAYING:
      break;
    case STARTING:
      if (player->timing.pa == NULL)
        goto silence;

      if (player->timing.pa->playing == 0)
        goto silence;

      // Can't start playing when we don't have a timestamp
      if (info.start == 0)
        goto silence;

      // Determine whether we have enough data to start playing right away.
      int64_t start_at = info.start + info.latency_usec;

      uint64_t ts = (uint64_t)player->timing.pa->timestamp.tv_sec * 1000000 + player->timing.pa->timestamp.tv_usec;
      uint64_t playback_latency = player->timing.pa->sink_usec + player->timing.pa->transport_usec +
                                  pa_bytes_to_usec(player->timing.pa->write_index - player->timing.pa->read_index, ss);

      uint64_t play_at = ts + playback_latency;

      int64_t delta = play_at - start_at;

      printf("Request for %zd bytes (playing = %i), can start at %ld, would play at %ld, in %ld usec\n", request, player->timing.pa->playing, start_at, play_at, delta);

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

      // Prepare timing information
      player->timing.start_local_usec = play_at;
      player->timing.initial_net_offset = info.net_offset;
      player->timing.pa_offset_bytes = player->timing.pa->write_index;

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
  const pa_sample_spec *ss = pa_stream_get_sample_spec(s);

  if (info->timestamped) {
    pa_timing_info ti = *player->timing.pa;
    int frame_size = pa_frame_size(ss);

    int net_local_delta = player->timing.initial_net_offset - info->net_offset;
    int64_t ts = ti.timestamp.tv_sec * 1000000ULL + ti.timestamp.tv_usec;
    int local_audio_delta = ts - player->timing.start_local_usec - pa_bytes_to_usec(ti.read_index, ss) + pa_bytes_to_usec(player->timing.pa_offset_bytes, ss);
    int local_local_delta = ts - info->start;
    int write_latency = pa_bytes_to_usec(ti.write_index - ti.read_index, ss);
    int total_delta = local_local_delta + write_latency - info->latency_usec - local_audio_delta - net_local_delta;
    int recv_to_play = ts + write_latency - info->start - info->latency_usec;

    int total_delta_sgn = ((total_delta > 0) - (0 > total_delta));
    int frames_delta = total_delta_sgn * ((pa_usec_to_bytes(abs(total_delta), ss) + frame_size / 2) / frame_size);

    printf("Timing n-l-d: %6d usec l-a-d: %5d usec l-l-d: %6d usec w-l: %6d usec delta: %4d usec (%3d frames), r-t-p: %4d usec\n",
           net_local_delta, local_audio_delta, local_local_delta, write_latency, total_delta, frames_delta, recv_to_play);
  }

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

    if (!pa_sample_spec_equal(ss, &frame->ss)) {
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
