#include <pulse/pulseaudio.h>
#include <stdbool.h>
#include <stdio.h>

#include "output.h"

extern void write_data(pa_stream *s, size_t size);
extern void underflow(pa_stream *s);

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

void stream_underflow_cb(pa_stream *s, void *userdata) {
  printf("Underflow\n");
  underflow(s);
//  assert(false);
}

void stream_request_cb(pa_stream *s, size_t size, void *mainloop) {
  pa_stream_ref(s);
  write_data(s, size);
  pa_stream_unref(s);
  pa_threaded_mainloop_signal(mainloop, 0);
}

void create_stream(struct pulse *pulse, pa_sample_spec *ss, const pa_buffer_attr *bufattr) {
  pa_channel_map map;
  assert(pa_channel_map_init_auto(&map, ss->channels, PA_CHANNEL_MAP_DEFAULT));

  pa_stream *s = pa_stream_new(pulse->context, "Songcast Receiver", ss, &map);
  pa_stream_set_state_callback(s, stream_state_cb, pulse->mainloop);
  pa_stream_set_write_callback(s, stream_request_cb, pulse->mainloop);
  pa_stream_set_underflow_callback(s, stream_underflow_cb, NULL);

  char format[PA_SAMPLE_SPEC_SNPRINT_MAX];
  pa_sample_spec_snprint(format, sizeof(format), ss);
  printf("Stream created (%s)\n", format);

  pa_stream_flags_t stream_flags;
  stream_flags =  PA_STREAM_NOT_MONOTONIC |
                  PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY;

  // Connect stream to the default audio output sink
  assert(pa_stream_connect_playback(s, NULL, bufattr, stream_flags, NULL, NULL) == 0);
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

void drain_cb(pa_stream *s, int success, void *data) {
  void (*cb)(void) = data;

  pa_stream_disconnect(s);
  pa_stream_unref(s);
  cb();
  printf("Drain complete\n");
}

void stop_stream(pa_stream *s, void (*cb)(void)) {
  printf("Draining.\n");

  pa_operation *o = pa_stream_drain(s, drain_cb, cb);
  if (o != NULL)
    pa_operation_unref(o);
  else
    drain_cb(s, 0, NULL);
}
