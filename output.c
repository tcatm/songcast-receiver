#include <pulse/pulseaudio.h>
#include <stdbool.h>
#include <stdio.h>

#include "output.h"

extern void write_data(size_t size);

void stream_trigger_cb(pa_mainloop_api *api, pa_time_event *e, const struct timeval *tv, void *userdata);

pa_usec_t latency_to_usec(int samplerate, int latency) {
  int multiplier = (samplerate%441) == 0 ? 44100 : 48000;
  return latency * 1e6 / (256 * multiplier);
}

void context_state_cb(pa_context* context, void* mainloop) {
  pa_threaded_mainloop_signal(mainloop, 0);
}

void stream_state_cb(pa_stream *s, void *mainloop) {
  printf("State %i\n", pa_stream_get_state(s));
  pa_threaded_mainloop_signal(mainloop, 0);
}

void success_cb(pa_stream *s, int success, void *mainloop) {
  pa_threaded_mainloop_signal(mainloop, 0);
}

void stream_underflow_cb(pa_stream *stream, void *userdata) {
  printf("Underflow\n");
//  assert(false);
}

void stream_request_cb(pa_stream *s, size_t size, void *mainloop) {
//  printf("Request for %i\n", size);
  write_data(size);
}

void drain(void) {
  #if 0
  printf("Draining.\n");

  if (G.stream == NULL)
    return;

  pa_threaded_mainloop_lock(G.mainloop);

  pa_operation *o = pa_stream_drain(G.stream, success_cb, G.mainloop);
  while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
      pa_threaded_mainloop_wait(G.mainloop);

  pa_operation_unref(o);

  assert(pa_stream_disconnect(G.stream) == 0);

  pa_stream_unref(G.stream);
  G.stream = NULL;

  pa_threaded_mainloop_unlock(G.mainloop);
  #endif
}

void create_stream(struct pulse *pulse, pa_sample_spec *ss) {
  assert(pulse->stream == NULL);

  pa_channel_map map;
  assert(pa_channel_map_init_auto(&map, ss->channels, PA_CHANNEL_MAP_DEFAULT));

  pulse->stream = pa_stream_new(pulse->context, "Songcast Receiver", ss, &map);
  pa_stream_set_state_callback(pulse->stream, stream_state_cb, pulse->mainloop);
  pa_stream_set_write_callback(pulse->stream, stream_request_cb, pulse->mainloop);
  pa_stream_set_underflow_callback(pulse->stream, stream_underflow_cb, NULL);

  char format[PA_SAMPLE_SPEC_SNPRINT_MAX];
  pa_sample_spec_snprint(format, sizeof(format), ss);
  printf("Stream created (%s)\n", format);
}

void connect_stream(struct pulse *pulse, const pa_buffer_attr *bufattr) {
  assert(pulse->stream != NULL);

  pa_threaded_mainloop_lock(pulse->mainloop);

  pa_stream_flags_t stream_flags;
  stream_flags =  PA_STREAM_START_CORKED | PA_STREAM_NOT_MONOTONIC |
                  PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY;
                  // | PA_STREAM_VARIABLE_RATE;

  // Connect stream to the default audio output sink
  assert(pa_stream_connect_playback(pulse->stream, NULL, bufattr, stream_flags, NULL, NULL) == 0);

  printf("Connect\n");
  // Wait for the stream to be ready
  while (true) {
      pa_stream_state_t stream_state = pa_stream_get_state(pulse->stream);
      assert(PA_STREAM_IS_GOOD(stream_state));
      if (stream_state == PA_STREAM_READY)
        break;
      pa_threaded_mainloop_wait(pulse->mainloop);
  }

  printf("Connected\n");

  pa_threaded_mainloop_unlock(pulse->mainloop);
}

void trigger_stream(struct pulse *pulse, pa_usec_t delay) {
  pa_usec_t when = pa_rtclock_now() + delay;
  printf(" at %uusec\n", when);

  pa_context_rttime_new(pulse->context, when, stream_trigger_cb, pulse);
}

void stream_trigger_cb(pa_mainloop_api *api, pa_time_event *e, const struct timeval *tv, void *userdata) {
  struct pulse *pulse = userdata;

  assert(pulse->stream != NULL);

  printf("now %uusec\n", pa_rtclock_now());

  pa_operation *o;

  assert(pa_stream_is_corked(pulse->stream));

  o = pa_stream_cork(pulse->stream, 0, success_cb, pulse->mainloop);
  if (o != NULL)
    pa_operation_unref(o);

  o = pa_stream_trigger(pulse->stream, success_cb, pulse->mainloop);
  if (o != NULL)
    pa_operation_unref(o);
}

pa_usec_t get_latency(struct pulse *pulse) {
  assert(pulse->stream != NULL);

  pa_threaded_mainloop_lock(pulse->mainloop);

  pa_operation *o;

  o = pa_stream_update_timing_info(pulse->stream, success_cb, pulse->mainloop);
  while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
      pa_threaded_mainloop_wait(pulse->mainloop);

  pa_operation_unref(o);

  const pa_sample_spec *ss = pa_stream_get_sample_spec(pulse->stream);
  const pa_timing_info *ti = pa_stream_get_timing_info(pulse->stream);

  assert(ti != NULL);
  assert(!ti->write_index_corrupt);

  pa_usec_t wt = pa_bytes_to_usec((uint64_t)ti->write_index, ss);
  pa_usec_t rt;

  assert(!pa_stream_get_time(pulse->stream, &rt));

  pa_threaded_mainloop_unlock(pulse->mainloop);

  return wt - rt;
}

void output_init(struct pulse *pulse) {
  pulse->mainloop = pa_threaded_mainloop_new();
  pulse->context = pa_context_new(pa_threaded_mainloop_get_api(pulse->mainloop), "Songcast Receiver");
  assert(pulse->context);

  pa_context_set_state_callback(pulse->context, &context_state_cb, pulse->mainloop);

  pa_threaded_mainloop_lock(pulse->mainloop);

  // Start the mainloop
  assert(pa_threaded_mainloop_start(pulse->mainloop) == 0);
  assert(pa_context_connect(pulse->context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) == 0);

  // Wait for the context to be ready
  while (true) {
      pa_context_state_t context_state = pa_context_get_state(pulse->context);

      if (context_state == PA_CONTEXT_READY)
        break;

      assert(PA_CONTEXT_IS_GOOD(context_state));

      pa_threaded_mainloop_wait(pulse->mainloop);
  }

  pa_threaded_mainloop_unlock(pulse->mainloop);

  printf("Pulseaudio ready.\n");
}

#if 0
  // TODO make everything below a separate function

  if (G.stream && !pa_sample_spec_equal(&G.ss, &last->ss)) {
    // TODO this needs to drain pretty fast...
    //      maybe drain as soon as sample_spec change is detected?
    // will this ever happen?
    // TODO close stream
    assert(pa_stream_is_corked(G.stream));
    assert(false);
  }

  pa_buffer_attr bufattr = {
    .maxlength = pa_usec_to_bytes(latency, &last->ss),
    .minreq = -1,
    .prebuf = pa_usec_to_bytes(latency, &last->ss),
    .tlength = 0.25 * pa_usec_to_bytes(latency, &last->ss),
  };

  pa_operation *o;

  if (G.stream == NULL)
    create_stream(&last->ss, &bufattr);
  else {
    pa_threaded_mainloop_lock(G.mainloop);
    o = pa_stream_cork(G.stream, 1, success_cb, G.mainloop);
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
        pa_threaded_mainloop_wait(G.mainloop);

    pa_operation_unref(o);

    o = pa_stream_flush(G.stream, success_cb, G.mainloop);
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
        pa_threaded_mainloop_wait(G.mainloop);

    pa_operation_unref(o);

    o = pa_stream_set_buffer_attr(G.stream, &bufattr, success_cb, G.mainloop);
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
        pa_threaded_mainloop_wait(G.mainloop);

    pa_operation_unref(o);

    printf("Bufattr set\n");
    pa_threaded_mainloop_unlock(G.mainloop);
  }

  G.first_run = false;
  G.state = PLAYING;

  // TODO wait for success
  // actually wait on all pa calls for success?
  // abstract pulse into seperate file?
  // TODO figure out where to write this in the buffer and start playback immediately?
  // The problem is like this:
  //   The stream is uncorked and waiting for the frame arriving after latency to trigger
  //   play back. This frame is then lost and playback never starts, yet the software thinks it has started.

  pa_threaded_mainloop_lock(G.mainloop);
  o = pa_stream_cork(G.stream, 0, success_cb, G.mainloop);
  while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
      pa_threaded_mainloop_wait(G.mainloop);

  pa_operation_unref(o);

  // alternatively, set prebuf to average processing time of frame?
  // moving average processing time?

  // TODO set buffer underrun callback. use this to set G.state = STOPPED
  // every frame should play at receive_time + latency (-some offset...)

  pa_threaded_mainloop_unlock(G.mainloop);
#endif
