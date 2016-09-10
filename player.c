#include <arpa/inet.h>
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

// TODO determine CACHE_SIZE dynamically based on latency? 192/24 needs a larger cache

#define CACHE_SIZE 2000  // frames
#define BUFFER_LATENCY 60e3 // 60ms buffer latency

enum PlayerState {STOPPED, STARTING, PLAYING, STOPPING};

struct cache_info {
  size_t available;
  int64_t start;
  int64_t start_net;
  int halt_index;
  int format_change_index;
  bool halt;
  bool format_change;
  pa_usec_t latency_usec;
};

struct cache {
  int start_seqnum;
  int latest_index;
  int size;
  unsigned int offset;
  struct audio_frame *frames[CACHE_SIZE];
};

struct timing {
  const pa_timing_info *pa;
  int64_t pa_offset_bytes;
  int64_t start_net_usec;
  int64_t latency_usec;
  int64_t net_offset;
};

struct {
  pthread_mutex_t mutex;
  enum PlayerState state;
  struct cache *cache;
  struct pulse pulse;
  struct timing timing;
} G = {};

// prototypes
bool process_frame(struct audio_frame *frame);
bool same_format(struct audio_frame *a, struct audio_frame *b);
void free_frame(struct audio_frame *frame);
void remove_old_frames(struct cache *cache, uint64_t now_usec);
void play_audio(pa_stream *s, size_t writable, struct cache_info *info);
struct cache_info cache_continuous_size(struct cache *cache);
void discard_cache_through(struct cache *cache, int discard);

// functions
uint64_t now_usec(void) {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  return (long long)now.tv_sec * 1000000 + (now.tv_nsec + 500) / 1000;
}

int cache_pos(struct cache *cache, int index) {
  return (index + cache->offset) % cache->size;
}

void print_state(enum PlayerState state) {
  switch (state) {
    case STOPPED:
      puts("STOPPED");
      break;
    case STARTING:
      puts("STARTING");
      break;
    case PLAYING:
      puts("PLAYING");
      break;
    case STOPPING:
      puts("STOPPING");
      break;
    default:
      assert(false && "UNKNOWN STATE");
  }
}

void print_cache(struct cache *cache) {
  assert(cache != NULL);

  printf("%10u [", cache->start_seqnum);

  struct audio_frame *last = NULL;
  int end = cache->size;
  for (int index = 0; index < end; index++) {
    int pos = cache_pos(cache, index);

    if (index > 0 && index%100 == 0)
      printf("]\n           [");

    struct audio_frame *frame = cache->frames[pos];
    char c = '?';
    int fg = -1;
    if (frame == NULL)    c = index > cache->latest_index ? ' ' : '.';
    else {
      if (frame->halt)    fg = 1;
      if (frame->resent)  c = '/';
      else                c = '#';
    }

    if (last != NULL && frame != NULL && !same_format(last, frame))
      fg = 2;

    last = frame;

    if (fg != -1) {
      printf("\e[3%1im%c\e[m", fg, c);
    } else
      printf("%c", c);
  }

  struct cache_info info = cache_continuous_size(G.cache);

  printf("] (%i byte)\n", info.available);
}

void player_init(void) {
  G.state = STOPPED;
  G.cache = NULL;
  output_init(&G.pulse);
}

void player_stop(void) {
  // TODO stop the player
  // TODO tear down pulse
}

bool frame_to_sample_spec(pa_sample_spec *ss, int rate, int channels, int bitdepth) {
  *ss = (pa_sample_spec){
    .rate = rate,
    .channels = channels
  };

  switch (bitdepth) {
    case 24:
      ss->format = PA_SAMPLE_S24BE;
      break;
    case 16:
      ss->format = PA_SAMPLE_S16BE;
      break;
    default:
      return false;
  }

  return true;
}

struct audio_frame *parse_frame(ohm1_audio *frame) {
  struct audio_frame *aframe = malloc(sizeof(struct audio_frame));

  if (aframe == NULL)
    return NULL;

  aframe->seqnum = ntohl(frame->frame);
  aframe->latency = ntohl(frame->media_latency);
  aframe->audio_length = frame->channels * frame->bitdepth * ntohs(frame->samplecount) / 8;
  aframe->halt = frame->flags & OHM1_FLAG_HALT;
  aframe->resent = frame->flags & OHM1_FLAG_RESENT;

  if (!frame_to_sample_spec(&aframe->ss, ntohl(frame->samplerate), frame->channels, frame->bitdepth)) {
    printf("Unsupported sample spec\n");
    free(aframe);
    return NULL;
  }

  aframe->ts_network = latency_to_usec(aframe->ss.rate, ntohl(frame->network_timestamp));
  aframe->ts_media = latency_to_usec(aframe->ss.rate, ntohl(frame->media_timestamp));

  aframe->audio = malloc(aframe->audio_length);
  aframe->readptr = aframe->audio;

  if (aframe->audio == NULL) {
    free(aframe);
    return NULL;
  }

  memcpy(aframe->audio, frame->data + frame->codec_length, aframe->audio_length);

  return aframe;
}

void free_frame(struct audio_frame *frame) {
  free(frame->audio);
  free(frame);
}

struct missing_frames *request_frames(struct cache *cache) {
  assert(cache != NULL);

  struct missing_frames *d = calloc(1, sizeof(struct missing_frames) + sizeof(unsigned int) * cache->latest_index);
  assert(d != NULL);

  int end = cache->latest_index;
  for (int index = 0; index <= end; index++) {
    int pos = cache_pos(cache, index);
    if (G.cache->frames[pos] == NULL)
      d->seqnums[d->count++] = (long long int)index + cache->start_seqnum;
  }

  if (d->count == 0) {
    free(d);
    return NULL;
  }

  return d;
}

bool same_format(struct audio_frame *a, struct audio_frame *b) {
  return pa_sample_spec_equal(&a->ss, &b->ss) && a->latency == b->latency;
}

int uint64cmp(const void *aa, const void *bb) {
  const uint64_t *a = aa, *b = bb;
  return (*a < *b) ? -1 : (*a > *b);
}

struct cache_info cache_continuous_size(struct cache *cache) {
  assert(cache != NULL);

  struct audio_frame *last = NULL;

  struct cache_info info = {
    .available = 0,
    .halt = false,
    .format_change = false,
    .start = 0,
    .start_net = 0,
    .halt_index = -1,
    .format_change_index = -1,
    .latency_usec = 0,
  };

  uint64_t best_net = 0;

  int end = cache->latest_index;

  uint64_t tss[end];
  uint64_t ts_nets[end];
  int64_t ts_mean = 0;
  int64_t ts_net_mean = 0;
  int64_t ts_m2 = 0;
  int64_t ts_net_m2 = 0;
  int j = 0;

  for (int index = 0; index <= end; index++) {
    int pos = cache_pos(cache, index);
    if (G.cache->frames[pos] == NULL)
      break;

    struct audio_frame *frame = G.cache->frames[pos];

    if (last != NULL && !same_format(last, frame)) {
      info.format_change_index = index;
      info.format_change = true;
      break;
    }

    info.available += frame->audio_length;

    if (!frame->resent && frame->audio == frame->readptr && frame->audio_length > 0) {
      int64_t ts = frame->ts_recv_usec - pa_bytes_to_usec(info.available, &frame->ss);
      int64_t ts_net = frame->ts_network - pa_bytes_to_usec(info.available, &frame->ss);

      tss[j] = ts;
      ts_nets[j] = ts_net;

      if (j > 0) {
        int64_t d = ts - ts_mean;
        ts_mean += d / j;
        ts_m2 += d * (ts - ts_mean);

        d = ts_net - ts_net_mean;
        ts_net_mean += d / j;
        ts_net_m2 += d * (ts_net - ts_net_mean);
      } else {
        ts_mean = ts;
        ts_net_mean = ts_net;
      }

      j++;
    }

    info.latency_usec = latency_to_usec(frame->ss.rate, frame->latency);

    if (frame->halt) {
      info.halt_index = index;
      info.halt = true;
      break;
    }

    last = frame;
  }

  if (j == 0)
    return info;

  qsort(tss, j, sizeof(uint64_t), uint64cmp);
  qsort(ts_nets, j, sizeof(uint64_t), uint64cmp);

  int64_t ts_var = j > 1 ? ts_m2 / (j - 1) : 0;
  int64_t ts_net_var = j > 1 ? ts_net_m2 / (j - 1) : 0;

//  printf("ts     mean %lu median %lu std %g\n", ts_mean, tss[j/2], sqrt(ts_var));
//  printf("ts_net mean %lu median %lu std %g\n", ts_net_mean, ts_nets[j/2], sqrt(ts_net_var));

  info.start = tss[j/2] - sqrt(ts_var);
  info.start_net = ts_nets[j/2] - sqrt(ts_net_var);

  return info;
}

// Remove trim amount of bytes from the start of the cache.
// Returns true on success; false if there is not enough data to trim.
static bool trim_cache(struct cache *cache, size_t trim) {
  if (trim == 0)
    return true;

  int end = cache->latest_index;
  int adjust = 0;
  for (int index = 0; index <= end && trim > 0; index++) {
    int pos = cache_pos(cache, index);
    struct audio_frame *frame = G.cache->frames[pos];

    printf("Trimming %zd bytes\n", trim);

    if (frame == NULL)
      break;

    printf("%i Frame length %zd\n", index, frame->audio_length);

    if (frame->audio_length < trim) {
      printf("Trimming whole frame.\n");
      trim -= frame->audio_length;
      free_frame(frame);
      G.cache->frames[pos] = NULL;
      adjust++;
    } else {
      printf("Cutting frame.\n");
      frame->readptr = (uint8_t*)frame->readptr + trim;
      frame->audio_length -= trim;
      trim = 0;
    }
  }

  G.cache->offset += adjust;
  G.cache->start_seqnum += adjust;
  G.cache->latest_index -= adjust;

  assert(trim == 0);

  if (trim != 0)
    return false;

  return true;
  // iterate over frames
  // free any fully used frames
  // cut next frame in half if neccessary
}

void try_prepare(void) {
  struct audio_frame *start = G.cache->frames[cache_pos(G.cache, 0)];

  if (start == NULL)
    return;

  printf("Preparing stream.\n");

  assert(G.state == STOPPED);

  G.state = STARTING;
  G.timing = (struct timing){};

  pa_buffer_attr bufattr = {
    .maxlength = -1,
    .minreq = -1,
    .prebuf = 0,
    .tlength = pa_usec_to_bytes(BUFFER_LATENCY, &start->ss),
  };

  create_stream(&G.pulse, &start->ss, &bufattr);

  printf("Stream prepared: ");
  print_state(G.state);
}

void write_data(pa_stream *s, size_t request) {
  pthread_mutex_lock(&G.mutex);

  if (G.state == STOPPED) {
    pthread_mutex_unlock(&G.mutex);
    return;
  }

  const pa_sample_spec *ss = pa_stream_get_sample_spec(s);

  if (G.timing.pa == NULL)
    G.timing.pa = pa_stream_get_timing_info(s);

  struct cache_info info = cache_continuous_size(G.cache);

  G.timing.net_offset = (int64_t)info.start_net - (int64_t)info.start;

  switch (G.state) {
    case PLAYING:
      break;
    case STARTING:
      if (G.timing.pa == NULL)
        goto silence;

      if (G.timing.pa->playing == 0)
        goto silence;

      // Can't start playing when we don't have a timestamp
      if (info.start_net == 0 && info.start == 0)
        goto silence;

      pa_usec_t available_usec = pa_bytes_to_usec(info.available, pa_stream_get_sample_spec(s));

      // Determine whether we have enough data to start playing right away.
      int64_t start_at = info.latency_usec;

      if (info.start_net == 0)
        start_at += info.start;
      else
        start_at += info.start_net - G.timing.net_offset;

      uint64_t ts = (uint64_t)G.timing.pa->timestamp.tv_sec * 1000000 + G.timing.pa->timestamp.tv_usec;
      uint64_t playback_latency = G.timing.pa->sink_usec + G.timing.pa->transport_usec +
                                  pa_bytes_to_usec(G.timing.pa->write_index - G.timing.pa->read_index - request, ss);

      uint64_t play_at = ts + playback_latency;

      int64_t delta = play_at - start_at;

      printf("Request for %u bytes (playing = %i), can start at %ld, would play at %ld, in %ld usec\n", request, G.timing.pa->playing, start_at, play_at, delta);

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
      info.start_net += delta;
      info.available -= skip;

      // Prepare timing information
      G.timing.pa_offset_bytes = G.timing.pa->write_index;
      G.timing.start_net_usec = play_at + G.timing.net_offset;

      G.state = PLAYING;
      break;
    case STOPPED:
    case STOPPING:
    default:
      pthread_mutex_unlock(&G.mutex);
      return;
      break;
  }

  play_audio(s, request, &info);
  pthread_mutex_unlock(&G.mutex);
  return;

silence:
  printf("Playing silence.\n");
  uint8_t *silence = calloc(1, request);
  pa_stream_write(s, silence, request, NULL, 0, PA_SEEK_RELATIVE);
  free(silence);

  pthread_mutex_unlock(&G.mutex);
}

void set_stopped(void) {
  G.state = STOPPED;
  printf("Playback stopped.\n");
}

void underflow(pa_stream *s) {
  printf("Underflow: ");
  print_state(G.state);
  stop_stream(s, set_stopped);
}

void play_audio(pa_stream *s,size_t writable, struct cache_info *info) {
  const pa_sample_spec *ss = pa_stream_get_sample_spec(s);

  uint64_t write_index = G.timing.pa->write_index;
  uint64_t clock_remote = info->start_net + info->latency_usec;
  uint64_t clock_local = G.timing.start_net_usec + pa_bytes_to_usec(write_index - G.timing.pa_offset_bytes, ss);

  printf("Timing r: %" PRIu64 " l: %" PRIu64 ", delta: %4" PRId64 " usec\n", clock_remote, clock_local, (int64_t)clock_local - clock_remote);

  size_t written = 0;

  pa_usec_t available_usec = pa_bytes_to_usec(info->available, pa_stream_get_sample_spec(s));
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

    int r = pa_stream_write(s, data, length, NULL, 0LL, PA_SEEK_RELATIVE);

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
        G.state = STOPPING;
        break;
      }
    }
  }

  if (writable > 0) {
    printf("Not enough data. Stopping.\n");
    G.state = STOPPING;
  }

  //printf("written %i byte, %uusec\n", written, pa_bytes_to_usec(written, pa_stream_get_sample_spec(s)));
}

struct missing_frames *handle_frame(ohm1_audio *frame, struct timespec *ts) {
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

void discard_cache_through(struct cache *cache, int discard) {
  assert(cache != NULL);
  assert(discard < cache->size);

  if (discard < 0)
    return;

  for (int index = 0; index <= discard; index++) {
    int pos = cache_pos(cache, index);

    if (cache->frames[pos] != NULL) {
      free_frame(cache->frames[pos]);
      cache->frames[pos] = NULL;
    }
  }

  cache->offset += discard + 1;
  cache->start_seqnum += discard + 1;
  cache->latest_index -= discard + 1;
}

void remove_old_frames(struct cache *cache, uint64_t now_usec) {
  assert(cache != NULL);

  int discard = -1;

  int end = cache->latest_index;
  for (int index = 0; index <= end; index++) {
    int pos = cache_pos(cache, index);
    if (G.cache->frames[pos] == NULL)
      continue;

    if (G.cache->frames[pos]->ts_due_usec < now_usec)
      discard = index;
    else
      break;
  }

  discard_cache_through(cache, discard);
}

bool process_frame(struct audio_frame *frame) {
  if (G.cache == NULL) {
    if (frame->resent)
      return false;

    G.cache = calloc(1, sizeof(struct cache));
    assert(G.cache != NULL);

    printf("start receiving\n");
    G.cache->latest_index = 0;
    G.cache->start_seqnum = frame->seqnum;
    G.cache->size = CACHE_SIZE;
    G.cache->offset = 0;
  }

  // The index stays fixed while this function runs.
  // Only the output thread can change start_seqnum after
  // the cache has been created.
  int index = frame->seqnum - G.cache->start_seqnum;
  int pos = cache_pos(G.cache, index);
  if (index < 0 || index >= G.cache->size)
    return false;

//  printf("Handling frame %i %s\n", frame->seqnum, frame->resent ? "(resent)" : "");

  if (G.cache->frames[pos] != NULL)
    return false;

  G.cache->frames[pos] = frame;

  if (index > G.cache->latest_index)
    G.cache->latest_index = index;

  return true;
}
