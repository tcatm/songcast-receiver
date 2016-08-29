#include <arpa/inet.h>
#include <pulse/pulseaudio.h>
#include <error.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "player.h"

#define CACHE_SIZE 100

enum PlayerState {STOPPED, PLAYING};

struct {
  enum PlayerState state;
  pa_threaded_mainloop *mainloop;
  pa_context *context;
  pa_stream *stream;
  pa_sample_spec ss;
  unsigned int last_played;
  unsigned int last_received;
  struct audio_frame *cache[CACHE_SIZE];
} G = {};

// prototypes
void play_frame(struct audio_frame *frame);
void process_frame(struct audio_frame *frame);

// functions
pa_usec_t latency_to_usec(int samplerate, int latency) {
  int multiplier = (samplerate%441) == 0 ? 44100 : 48000;
  return latency * 1e6 / (256 * multiplier);
}

void context_state_cb(pa_context* context, void* mainloop) {
  pa_threaded_mainloop_signal(mainloop, 0);
}

void stream_state_cb(pa_stream *s, void *mainloop) {
  pa_threaded_mainloop_signal(mainloop, 0);
}

void stream_write_cb(pa_stream *stream, size_t size, void *mainloop) {
  pa_threaded_mainloop_signal(mainloop, 0);
}

void stream_success_cb(pa_stream *stream, int success, void *mainloop) {
  pa_threaded_mainloop_signal(mainloop, 0);
}

void stream_underflow_cb(pa_stream *stream, void *userdata) {
  printf("Underflow\n");

  // TODO wait for succcess?
  pa_stream_cork(G.stream, 1, stream_success_cb, G.mainloop);

  G.state = STOPPED;
}

void player_init(void) {
  G.state = STOPPED;
  G.stream = NULL;
  G.last_played = 0;
  G.last_received = 0;
  G.mainloop = pa_threaded_mainloop_new();
  assert(G.mainloop);

  G.context = pa_context_new(pa_threaded_mainloop_get_api(G.mainloop), "Songcast Receiver");
  assert(G.context);

  pa_context_set_state_callback(G.context, &context_state_cb, G.mainloop);

  pa_threaded_mainloop_lock(G.mainloop);

  // Start the mainloop
  assert(pa_threaded_mainloop_start(G.mainloop) == 0);
  assert(pa_context_connect(G.context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) == 0);

  // Wait for the context to be ready
  while (true) {
      pa_context_state_t context_state = pa_context_get_state(G.context);

      if (context_state == PA_CONTEXT_READY)
        break;

      assert(PA_CONTEXT_IS_GOOD(context_state));

      pa_threaded_mainloop_wait(G.mainloop);
  }

  pa_threaded_mainloop_unlock(G.mainloop);
}

void player_stop(void) {
  // TODO stop the player
  // TODO tear down pulse
}

void drain(void) {
  // TODO in which cases do we need to drain the stream at all?
  printf("Draining.\n");

  if (G.stream == NULL)
    return;
/*
  pa_threaded_mainloop_lock(G.mainloop);

  pa_operation o = pa_stream_drain(G.stream, stream_success_cb, G.mainloop);

  while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
      pa_threaded_mainloop_wait(G.mainloop);

  pa_operation_unref(o);
  pa_threaded_mainloop_unlock(G.mainloop);

  G.pa_stream = NULL;
  */
}

void create_stream(pa_sample_spec *ss) {
  printf("Start stream.\n");

  pa_threaded_mainloop_lock(G.mainloop);

  pa_channel_map map;
  assert(pa_channel_map_init_auto(&map, ss->channels, PA_CHANNEL_MAP_DEFAULT));

  G.stream = pa_stream_new(G.context, "Songcast Receiver", ss, &map);
  pa_stream_set_state_callback(G.stream, stream_state_cb, G.mainloop);
  pa_stream_set_write_callback(G.stream, stream_write_cb, G.mainloop);

  pa_stream_flags_t stream_flags;
  stream_flags = PA_STREAM_START_CORKED | PA_STREAM_NOT_MONOTONIC |
                 PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY;

  // Connect stream to the default audio output sink
  assert(pa_stream_connect_playback(G.stream, NULL, NULL, stream_flags, NULL, NULL) == 0);

  // Wait for the stream to be ready
  while (true) {
      pa_stream_state_t stream_state = pa_stream_get_state(G.stream);
      assert(PA_STREAM_IS_GOOD(stream_state));
      if (stream_state == PA_STREAM_READY) break;
      pa_threaded_mainloop_wait(G.mainloop);
  }

  pa_stream_set_underflow_callback(G.stream, stream_underflow_cb, NULL);

  pa_threaded_mainloop_unlock(G.mainloop);

  G.ss = *ss;
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
  // TODO copy audio data
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

struct missing_frames *request_frames(void) {
  struct missing_frames *d = calloc(1, sizeof(struct missing_frames) + sizeof(unsigned int) * CACHE_SIZE);
  assert(d != NULL);

  int start = G.last_played + CACHE_SIZE - (long long int)G.last_received;
  if (start < 0)
    start = 0;

  for (int i = start; i < CACHE_SIZE; i++)
    if (G.cache[i] == NULL)
      d->seqnums[d->count++] = (long long int)G.last_received - CACHE_SIZE + i + 1;

  if (d->count == 0) {
    free(d);
    return NULL;
  }

  return d;
}

bool same_format(struct audio_frame *a, struct audio_frame *b) {
  return pa_sample_spec_equal(&a->ss, &b->ss) && a->latency == b->latency;
}

void try_start(void) {
  printf("stopped\n");

  int start_offset = CACHE_SIZE - 1;
  struct audio_frame *last = G.cache[start_offset];

  if (last == NULL)
    return;

  if (last->halt)
    return;

  size_t length = last->audio_length;

  int start = G.last_played + CACHE_SIZE - (long long int)G.last_received;
  if (start < 0)
    start = 0;

  for (int i = CACHE_SIZE - 2; i >= start; i--) {
    if (G.cache[i] == NULL)
      break;

    if (!same_format(last, G.cache[i]))
      break;

    if (G.cache[i]->halt)
      break;

    start_offset = i;
    length += G.cache[i]->audio_length;
  }

  pa_usec_t usec = pa_bytes_to_usec(length, &last->ss);
  pa_usec_t latency = latency_to_usec(last->ss.rate, last->latency);

  float fill = (float)usec / latency;

  printf("fill %uusec, latency %uusec (%f%%)\n", usec, latency, fill * 100);

  // start if we have only 20ms left to start the stream
  if (usec < latency - 20e3)
    return;

  printf("START\n");

  if (G.stream && !pa_sample_spec_equal(&G.ss, &last->ss)) {
    // TODO this needs to drain pretty fast...
    // will this ever happen?
    // TODO close stream
    assert(pa_stream_is_corked(G.stream));
    assert(false);
  }

  if (G.stream == NULL)
    create_stream(&last->ss);

  assert(pa_stream_is_corked(G.stream));

  // This should reflect any network and processing delays.
  pa_usec_t delay = 0;

  pa_buffer_attr bufattr = {
    .maxlength = 2 * pa_usec_to_bytes(latency, &last->ss),
    .minreq = -1,
    .prebuf = pa_usec_to_bytes(latency - delay, &last->ss),
    .tlength = 2 * pa_usec_to_bytes(latency, &last->ss),
  };

  pa_stream_set_buffer_attr(G.stream, &bufattr, NULL, NULL);


  for (int i = start_offset; i < CACHE_SIZE; i++) {
    play_frame(G.cache[i]);
    G.cache[i] = NULL;
  }

  G.state = PLAYING;
  pa_stream_cork(G.stream, 0, stream_success_cb, G.mainloop);

  // alternatively, set prebuf to average processing time of frame?
  // moving average processing time?

  // TODO set buffer underrun callback. use this to set G.state = STOPPED
  // every frame should play at receive_time + latency (-some offset...)
}

struct missing_frames *handle_frame(ohm1_audio *frame, struct timespec *ts) {
  struct audio_frame *aframe = parse_frame(frame);

  if (aframe == NULL)
    return NULL;

  aframe->ts_recv = *ts;

  process_frame(aframe);

  if (G.state == STOPPED)
    try_start();

  // Don't send resend requests when the frame was an answer.
  if (!aframe->resent && G.state == PLAYING)
    return request_frames();

  return NULL;
}

void process_frame(struct audio_frame *frame) {
  //printf("Handling frame %i %s\n", frame->seqnum, frame->resent ? "(resent)" : "");

  if (frame->seqnum <= G.last_played) {
    free_frame(frame);
    return;
  }

  if (frame->seqnum == G.last_played + 1 && G.state == PLAYING) {
    // This is the next frame. Play it.
    //printf("Playing %i                       ==== PLAY ===\n", frame->seqnum);
    play_frame(frame);

    int start = G.last_played + CACHE_SIZE - (long long int)G.last_received;
    if (start < 0)
      start = 0;

    for (int i = start; i < CACHE_SIZE; i++) {
      struct audio_frame *cframe = G.cache[i];

      if (cframe == NULL)
        break;

      //printf("Cache frame %i, expecting %i\n", cframe->seqnum, G.last_played + 1);

      if (cframe->seqnum != G.last_played + 1)
        break;

      printf("Playing from cache %i\n", cframe->seqnum);

      play_frame(cframe);
      G.cache[i] = NULL;
    }

    return;
  }

  // Frame is too old to be cached.
  if (frame->seqnum < (long long int)G.last_received - CACHE_SIZE) {
    free_frame(frame);
    return;
  }

  if (frame->seqnum > G.last_received) {
    printf("Frame from the future %i.\n", frame->seqnum);
    // This is a frame from the future.
    // Shift cache.

    // aframe->seqnum - last_received frames at the start must be discarded
    int delta = frame->seqnum - G.last_received;

    for (int i = 0; i < CACHE_SIZE; i++) {
      if (i < delta && G.cache[i] != NULL)
        free_frame(G.cache[i]);

      if (i < CACHE_SIZE - delta)
        G.cache[i] = G.cache[i + delta];
      else
        G.cache[i] = NULL;
    }

    G.last_received = frame->seqnum;
  }

  int i = frame->seqnum + CACHE_SIZE - G.last_received - 1;

  printf("Caching frame %i\n", frame->seqnum);
  if (G.cache[i] == NULL)
    G.cache[i] = frame;
  else
    free_frame(frame);
}

void play_data(const void *data, size_t length) {
  pa_threaded_mainloop_lock(G.mainloop);

  while (length > 0) {
      size_t l;
      int r;

      while (!(l = pa_stream_writable_size(G.stream)))
          pa_threaded_mainloop_wait(G.mainloop);

      if (l > length)
          l = length;

      r = pa_stream_write(G.stream, data, l, NULL, 0LL, PA_SEEK_RELATIVE);

      data = (const uint8_t*) data + l;
      length -= l;
  }

  pa_threaded_mainloop_unlock(G.mainloop);
}

void play_frame(struct audio_frame *frame) {
  play_data(frame->audio, frame->audio_length);

  // wait for buffer underrun?
  //if (frame->halt)
  //  drain();

  G.last_played = frame->seqnum;
  free_frame(frame);
}
