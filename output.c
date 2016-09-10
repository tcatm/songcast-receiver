#include <pulse/pulseaudio.h>
#include <stdbool.h>
#include <stdio.h>

#include "output.h"

extern void write_data(size_t size);
extern void underflow(void);

void stream_trigger_cb(pa_mainloop_api *api, pa_time_event *e, const struct timeval *tv, void *userdata);

pa_usec_t latency_to_usec(int samplerate, uint64_t latency) {
  int multiplier = (samplerate%441) == 0 ? 44100 : 48000;
  return latency * 1000000 / (256 * multiplier);
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
  underflow();
//  assert(false);
}

void stream_request_cb(pa_stream *s, size_t size, void *mainloop) {
  printf("Request for %i\n", size);
  write_data(size);
  pa_threaded_mainloop_signal(mainloop, 0);
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

  pa_stream_flags_t stream_flags;
  stream_flags =  PA_STREAM_NOT_MONOTONIC |
                  PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY;
                  // | PA_STREAM_VARIABLE_RATE;

  // Connect stream to the default audio output sink
  assert(pa_stream_connect_playback(pulse->stream, NULL, bufattr, stream_flags, NULL, NULL) == 0);
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

void stop_stream(struct pulse *pulse) {
  printf("Draining.\n");

  assert(pulse->stream != NULL);

  pa_operation *o = pa_stream_drain(pulse->stream, NULL, NULL);
  if (o != NULL)
    pa_operation_unref(o);

  printf("Drain complete\n");

  pa_stream_disconnect(pulse->stream);

  pa_stream_unref(pulse->stream);
  pulse->stream = NULL;
}

void update_timing_stream(struct pulse *pulse) {
  pa_operation *o = pa_stream_update_timing_info(pulse->stream, success_cb, pulse->mainloop);
  while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
    pa_threaded_mainloop_wait(pulse->mainloop);

  pa_operation_unref(o);
}
