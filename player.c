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

player_t G = {};

// prototypes
bool process_frame(struct audio_frame *frame);
bool same_format(struct audio_frame *a, struct audio_frame *b);
void free_frame(struct audio_frame *frame);
void play_audio(pa_stream *s, size_t writable, struct cache_info *info);


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
    case STOPPING:
      return "STOPPING";
      break;
    case HALT:
      return "HALT";
      break;
    default:
      assert(false && "UNKNOWN STATE");
  }
}

void set_state(enum PlayerState *state, enum PlayerState new_state) {
  printf("--> State change: %s to %s <--\n", print_state(*state), print_state(new_state));
  *state = new_state;
}

void player_init(player_t *player) {
  set_state(&G.state, STOPPED);
  G.cache = NULL;
  output_init(&G.pulse);
}

void player_stop(player_t *player) {
  pthread_mutex_lock(&G.mutex);

  if (G.cache != NULL)
    discard_cache_through(G.cache, G.cache->latest_index);

  free(G.cache);

  set_state(&G.state, STOPPED);
  G.cache = NULL;
  pthread_mutex_unlock(&G.mutex);
}

void try_prepare(void) {
  struct audio_frame *start = G.cache->frames[cache_pos(G.cache, 0)];

  if (start == NULL)
    return;

  printf("Preparing stream.\n");

  assert(G.state == STOPPED);

  set_state(&G.state, STARTING);
  G.timing = (struct timing){};

  pa_buffer_attr bufattr = {
    .maxlength = -1,
    .minreq = -1,
    .prebuf = 0,
    .tlength = pa_usec_to_bytes(BUFFER_LATENCY, &start->ss),
  };

  create_stream(&G.pulse, &start->ss, &bufattr);

  printf("Stream prepared\n");
}

void write_data(pa_stream *s, size_t request) {
  pthread_mutex_lock(&G.mutex);

  if (G.state == STOPPED || G.cache == NULL) {
    pthread_mutex_unlock(&G.mutex);
    return;
  }

  const pa_sample_spec *ss = pa_stream_get_sample_spec(s);

  if (G.timing.pa == NULL)
    G.timing.pa = pa_stream_get_timing_info(s);

  pa_operation *o = pa_stream_update_timing_info(G.pulse.stream, NULL, NULL);

  if (o != NULL)
    pa_operation_unref(o);

  struct cache_info info = cache_continuous_size(G.cache);

  switch (G.state) {
    case PLAYING:
      break;
    case STARTING:
      if (G.timing.pa == NULL)
        goto silence;

      if (G.timing.pa->playing == 0)
        goto silence;

      // Can't start playing when we don't have a timestamp
      if (info.start == 0)
        goto silence;

      // Determine whether we have enough data to start playing right away.
      int64_t start_at = info.start + info.latency_usec;

      uint64_t ts = (uint64_t)G.timing.pa->timestamp.tv_sec * 1000000 + G.timing.pa->timestamp.tv_usec;
      uint64_t playback_latency = G.timing.pa->sink_usec + G.timing.pa->transport_usec +
                                  pa_bytes_to_usec(G.timing.pa->write_index - G.timing.pa->read_index, ss);

      uint64_t play_at = ts + playback_latency;

      int64_t delta = play_at - start_at;

      printf("Request for %zd bytes (playing = %i), can start at %ld, would play at %ld, in %ld usec\n", request, G.timing.pa->playing, start_at, play_at, delta);

      if (delta < 0)
        goto silence;

      size_t skip = pa_usec_to_bytes(delta, ss);

      printf("Need to skip %zd bytes, have %zd bytes\n", skip, info.available);

      if (info.available < request + skip) {
        printf("Not enough data in buffer to start (missing %zd bytes)\n", request - skip - info.available);
        goto silence;
      }

      printf("Request can be fullfilled.\n");

      trim_cache(G.cache, skip);

      // Adjust cache info
      info.start += delta;
      info.available -= skip;

      // Prepare timing information
      G.timing.start_local_usec = play_at;
      G.timing.initial_net_offset = info.net_offset;
      G.timing.pa_offset_bytes = G.timing.pa->write_index;

      set_state(&G.state, PLAYING);
      break;
    case STOPPED:
    case STOPPING:
    case HALT:
    default:
      pthread_mutex_unlock(&G.mutex);
      return;
      break;
  }

  play_audio(s, request, &info);
  G.timing.written += request;

  pthread_mutex_unlock(&G.mutex);
  return;

uint8_t *silence;

silence:
  silence = calloc(1, request);
  pa_stream_write(s, silence, request, NULL, 0, PA_SEEK_RELATIVE);
  free(silence);

  pthread_mutex_unlock(&G.mutex);
}

void set_stopped(void) {
  if (G.state != STOPPING) {
    return;
  }
  set_state(&G.state, STOPPED);
  printf("Playback stopped.\n");
}

void stop(pa_stream *s) {
  printf("stop()\n");
  pthread_mutex_lock(&G.mutex);

  if (G.state == STOPPING || G.state == STOPPED) {
    pthread_mutex_unlock(&G.mutex);
    return;
  }

  set_state(&G.state, STOPPING);
  stop_stream(s, set_stopped);
  pthread_mutex_unlock(&G.mutex);
}

void play_audio(pa_stream *s,size_t writable, struct cache_info *info) {
  const pa_sample_spec *ss = pa_stream_get_sample_spec(s);

  if (info->timestamped) {
    pa_timing_info ti = *G.timing.pa;
    int frame_size = pa_frame_size(ss);

    int net_local_delta = G.timing.initial_net_offset - info->net_offset;
    int64_t ts = ti.timestamp.tv_sec * 1000000ULL + ti.timestamp.tv_usec;
    int local_audio_delta = ts - G.timing.start_local_usec - pa_bytes_to_usec(ti.read_index, ss) + pa_bytes_to_usec(G.timing.pa_offset_bytes, ss);
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
    int pos = cache_pos(G.cache, 0);

    if (G.cache->frames[pos] == NULL) {
      printf("Missing frame.\n");
      break;
    }

    struct audio_frame *frame = G.cache->frames[pos];

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
      G.cache->frames[pos] = NULL;
      G.cache->offset++;
      G.cache->start_seqnum++;
      G.cache->latest_index--;

      if (halt) {
        printf("HALT received.\n");
        set_state(&G.state, HALT);
        return;
      }
    }
  }

  if (writable > 0) {
    printf("Not enough data. Stopping.\n");
    set_state(&G.state, HALT);
    return;
  }

  //printf("written %i byte, %uusec\n", written, pa_bytes_to_usec(written, pa_stream_get_sample_spec(s)));
}

struct missing_frames *handle_frame(player_t *player, ohm1_audio *frame, struct timespec *ts) {
  struct audio_frame *aframe = parse_frame(frame);

  if (aframe == NULL)
    return NULL;

  uint64_t now = now_usec();

  // TODO incorporate any network latencies and such into ts_due_usec
  aframe->ts_recv_usec = (long long)ts->tv_sec * 1000000 + (ts->tv_nsec + 500) / 1000;
  aframe->ts_due_usec = latency_to_usec(aframe->ss.rate, aframe->latency) + aframe->ts_recv_usec;

  pthread_mutex_lock(&G.mutex);
  if (G.cache != NULL)
    remove_old_frames(G.cache, now);

  bool consumed = process_frame(aframe);

  if (!consumed)
    free_frame(aframe);

  if (G.cache != NULL)
    fixup_timestamps(G.cache);

  if (G.cache != NULL && G.state == STOPPED) {
    print_cache(G.cache);
    try_prepare();
  }

  pthread_mutex_unlock(&G.mutex);

  // Don't send resend requests when the frame was an answer.
  if (!aframe->resent && G.cache != NULL)
    return request_frames(G.cache);

  return NULL;
}

bool process_frame(struct audio_frame *frame) {
  if (G.cache == NULL) {
    if (frame->resent)
      return false;

    G.cache = calloc(1, sizeof(struct cache));
    assert(G.cache != NULL);

    printf("start receiving\n");
    G.cache->size = CACHE_SIZE;
    goto reset_cache;
  }

  // The index stays fixed while this function runs.
  // Only the output thread can change start_seqnum after
  // the cache has been created.

  if (frame->seqnum - G.cache->start_seqnum >= G.cache->size) {
    discard_cache_through(G.cache, G.cache->latest_index);
    printf("clean\n");
  reset_cache:
    G.cache->latest_index = 0;
    G.cache->start_seqnum = frame->seqnum;
    G.cache->offset = 0;
  }

  int index = frame->seqnum - G.cache->start_seqnum;
  int pos = cache_pos(G.cache, index);
  if (index < 0)
    return false;

//  printf("Handling frame %i %s\n", frame->seqnum, frame->resent ? "(resent)" : "");

  if (G.cache->frames[pos] != NULL)
    return false;

  G.cache->frames[pos] = frame;

  if (index > G.cache->latest_index)
    G.cache->latest_index = index;

  return true;
}
