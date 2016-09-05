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

#include "player.h"
#include "output.h"

// TODO determine CACHE_SIZE dynamically based on latency? 192/24 needs a larger cache

#define CACHE_SIZE 2000  // frames

enum PlayerState {STOPPED, STARTING, PLAYING};

struct cache_info {
  size_t available;
  uint64_t start;
  int halt_index;
  bool halt;
  bool format_change;
};

struct cache {
  int start_seqnum;
  int latest_index;
  int size;
  unsigned int offset;
  struct audio_frame *frames[CACHE_SIZE];
};

struct {
  pthread_mutex_t mutex;
  enum PlayerState state;
  struct cache *cache;
  struct pulse pulse;
} G = {};

// prototypes
bool process_frame(struct audio_frame *frame);
bool same_format(struct audio_frame *a, struct audio_frame *b);
void free_frame(struct audio_frame *frame);
void remove_old_frames(struct cache *cache, uint64_t now_usec);
void play_audio(size_t writable);
struct cache_info cache_continuous_size(struct cache *cache);

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
    default:
      assert(false && "UNKNOWN STATE");
  }
}

void print_cache(struct cache *cache) {
  pthread_mutex_lock(&G.mutex);

  assert(cache != NULL);

  printf("%10u [", cache->start_seqnum);

  for (int index = 0; index < cache->size; index++) {
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

    if (fg != -1) {
      printf("\e[3%1im%c\e[m", fg, c);
    } else
      printf("%c", c);
  }

  struct cache_info info = cache_continuous_size(G.cache);

  printf("] (%i byte)\n", info.available);
  pthread_mutex_unlock(&G.mutex);
}

void stop_playback(void) {
  print_state(G.state);

  G.state = STOPPED;
  stop_stream(&G.pulse);
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

  aframe->ts_network = ntohl(frame->network_timestamp);
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

  for (int index = 0; index <= cache->latest_index; index++) {
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

struct cache_info cache_continuous_size(struct cache *cache) {
  assert(cache != NULL);

  struct audio_frame *last = NULL;

  struct cache_info info = {
    .available = 0,
    .halt = false,
    .format_change = false,
    .start = 0,
    .halt_index = -1,
  };

  for (int index = 0; index <= cache->latest_index; index++) {
    int pos = cache_pos(cache, index);
    if (G.cache->frames[pos] == NULL)
      break;

    struct audio_frame *frame = G.cache->frames[pos];

    if (last != NULL && !same_format(last, frame)) {
      info.format_change = true;
      break;
    }

    if (!frame->resent && frame->audio == frame->readptr) {
      uint64_t ts = frame->ts_due_usec - pa_bytes_to_usec(info.available, &frame->ss);
      if (info.start == 0 || ts < info.start)
        info.start = ts;
    }

    info.available += frame->audio_length;

    if (frame->halt) {
      info.halt_index = index;
      info.halt = true;
      break;
    }

    last = frame;
  }

  return info;
}

void try_start(void) {
  pthread_mutex_lock(&G.mutex);
  struct cache_info info = cache_continuous_size(G.cache);

  if (info.available == 0) {
    pthread_mutex_unlock(&G.mutex);
    return;
  }

  struct audio_frame *start = G.cache->frames[cache_pos(G.cache, 0)];

  pa_usec_t available_usec = pa_bytes_to_usec(info.available, &start->ss);
  pa_usec_t latency_usec = latency_to_usec(start->ss.rate, start->latency);
  pthread_mutex_unlock(&G.mutex);

  // Don't start if there is less than 5 seconds of audio to play before
  // encountering the next halt frame.
  if (info.halt && available_usec < 5e6) {
    printf("halt index %i\n", info.halt_index);
    discard_cache_through(G.cache, info.halt_index);
    return;
  }

  // TODO determine how much data we need and how much we possibly could have (latency!)
  // TODO maybe create stream on first valid frame, play silence and fill buffer later?
  int64_t offset = 100e3;

  int64_t time_left = info.start - now_usec();

  printf("Time left %liusec, available_usec %uusec, -> %liusec\n", time_left, available_usec, available_usec + time_left);

  if (available_usec + time_left < offset || info.start == 0)
    return;

  printf("latency %iusec, available %uusec\n", latency_usec, available_usec);

  G.state = STARTING;

  pa_buffer_attr bufattr = {
    .maxlength = -1,
    .minreq = -1,
    .prebuf = 0,
    .tlength = pa_usec_to_bytes(offset, &start->ss),
  };

  create_stream(&G.pulse, &start->ss);
  connect_stream(&G.pulse, &bufattr);

  pa_threaded_mainloop_lock(G.pulse.mainloop);

  size_t request = pa_stream_writable_size(G.pulse.stream);
  size_t silence_length = request;
  uint8_t *silence = calloc(1, silence_length);

  int64_t due = info.start - now_usec();

  ssize_t seek = pa_usec_to_bytes(due < 0 ? -due : due, &start->ss);

  if (due < 0)
    seek = -seek;

  int r = pa_stream_write(G.pulse.stream, silence, silence_length, NULL, seek - silence_length, PA_SEEK_RELATIVE_ON_READ);
  free(silence);

  G.state = PLAYING;

  pa_threaded_mainloop_unlock(G.pulse.mainloop);

  printf("due in %liusec, seeking %li bytes\n", due, seek);

  printf("try_start finished.\n");
}

void write_data(size_t writable) {
  if (G.state != PLAYING)
    return;

  play_audio(writable);
}

void play_audio(size_t writable) {
  pthread_mutex_lock(&G.mutex);

  struct cache_info info = cache_continuous_size(G.cache);
  size_t written = 0;

  pa_usec_t available_usec = pa_bytes_to_usec(info.available, pa_stream_get_sample_spec(G.pulse.stream));
  printf("asked for %6i (%6iusec), available %6i (%6iusec)\n",
         writable, pa_bytes_to_usec(writable, pa_stream_get_sample_spec(G.pulse.stream)),
         info.available, available_usec);

  while (writable > 0) {
    int pos = cache_pos(G.cache, 0);

    if (G.cache->frames[pos] == NULL) {
      printf("Missing frame.\n");
      break;
    }

    struct audio_frame *frame = G.cache->frames[pos];

    if (!pa_sample_spec_equal(pa_stream_get_sample_spec(G.pulse.stream), &frame->ss)) {
      printf("Sample spec mismatch.\n");
      break;
    }

    if (frame->halt) {
      printf("HALT received.\n");
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

    int r = pa_stream_write(G.pulse.stream, data, length, NULL, 0LL, PA_SEEK_RELATIVE);

    written += length;
    writable -= length;

    if (consumed) {
      free_frame(frame);
      G.cache->frames[pos] = NULL;
      G.cache->offset++;
      G.cache->start_seqnum++;
      G.cache->latest_index--;
    }
  }

  if (writable > 0) {
    printf("Not enough data. Stopping.\n");
    stop_playback();
  }

  //printf("written %i byte, %uusec\n", written, pa_bytes_to_usec(written, pa_stream_get_sample_spec(G.pulse.stream)));

  pthread_mutex_unlock(&G.mutex);
  return;
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

  pthread_mutex_unlock(&G.mutex);

  if (G.cache != NULL && G.state == STOPPED) {
      print_cache(G.cache);
      try_start();
  }

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

  for (int index = 0; index <= cache->latest_index; index++) {
    int pos = cache_pos(cache, index);
    if (G.cache->frames[pos] == NULL)
      continue;

    if (G.cache->frames[pos]->ts_due_usec < now_usec)
      discard = index;
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

  //printf("Handling frame %i %s\n", frame->seqnum, frame->resent ? "(resent)" : "");

  if (G.cache->frames[pos] != NULL)
    return false;

  G.cache->frames[pos] = frame;

  if (index > G.cache->latest_index)
    G.cache->latest_index = index;

  return true;
}
