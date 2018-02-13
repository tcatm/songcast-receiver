#include <pulse/pulseaudio.h>
#include <stdbool.h>
#include <stdio.h>

#include "output.h"

#define CHECK_SUCCESS_GOTO(p, rerror, expression, label)        \
    do {                                                        \
        if (!(expression)) {                                    \
            if (rerror)                                         \
                *(rerror) = pa_context_errno((p)->context);     \
            goto label;                                         \
        }                                                       \
    } while(false);

#define CHECK_DEAD_GOTO(p, rerror, label)                               \
    do {                                                                \
        if (!(p)->context || !PA_CONTEXT_IS_GOOD(pa_context_get_state((p)->context)) || \
            !(p)->stream || !PA_STREAM_IS_GOOD(pa_stream_get_state((p)->stream))) { \
            if (((p)->context && pa_context_get_state((p)->context) == PA_CONTEXT_FAILED) || \
                ((p)->stream && pa_stream_get_state((p)->stream) == PA_STREAM_FAILED)) { \
                if (rerror)                                             \
                    *(rerror) = pa_context_errno((p)->context);         \
            } else                                                      \
                if (rerror)                                             \
                    *(rerror) = PA_ERR_BADSTATE;                        \
            goto label;                                                 \
        }                                                               \
    } while(false);


void context_state_cb(pa_context *context, void *mainloop) {
  pa_threaded_mainloop_signal(mainloop, 0);
}

static void success_cb(pa_stream *s, int success, void *userdata) {
  struct pulse *pulse = userdata;

  assert(s);
  assert(pulse);

  pulse->operation_success = success;
  pa_threaded_mainloop_signal(pulse->mainloop, 0);
}

void stream_state_cb(pa_stream *s, void *mainloop) {
  printf("----------------------------------------- Stream %p ", s);

  switch (pa_stream_get_state(s)) {
    case PA_STREAM_UNCONNECTED:
      printf("UNCONNECTED\n");
      break;
    case PA_STREAM_CREATING:
      printf("CREATING\n");
      break;
    case PA_STREAM_READY:
      printf("READY\n");
      break;
    case PA_STREAM_FAILED:
      printf("FAILED\n");
      break;
    case PA_STREAM_TERMINATED:
      printf("TERMINATED\n");
      break;
    default:
      printf("INVALID\n");
  }

  pa_threaded_mainloop_signal(mainloop, 0);
}

void create_stream(struct pulse *pulse, pa_sample_spec *ss, const pa_buffer_attr *bufattr, void *userdata, struct output_cb *callbacks) {
  pa_channel_map map;
  assert(pa_channel_map_init_auto(&map, ss->channels, PA_CHANNEL_MAP_DEFAULT));

  pa_threaded_mainloop_lock(pulse->mainloop);

  pulse->stream = pa_stream_new(pulse->context, "Songcast Receiver", ss, &map);
  pa_stream_set_state_callback(pulse->stream, stream_state_cb, pulse->mainloop);
  pa_stream_set_write_callback(pulse->stream, callbacks->write, userdata);
  pa_stream_set_underflow_callback(pulse->stream, callbacks->underflow, userdata);
  pa_stream_set_latency_update_callback(pulse->stream, callbacks->latency, userdata);

  char format[PA_SAMPLE_SPEC_SNPRINT_MAX];
  pa_sample_spec_snprint(format, sizeof(format), ss);
  printf("Stream created (%s)\n", format);

  pa_stream_flags_t stream_flags;
  stream_flags =  PA_STREAM_NOT_MONOTONIC |
                  PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY;

  // Connect stream to the default audio output sink
  assert(pa_stream_connect_playback(pulse->stream, NULL, bufattr, stream_flags, NULL, NULL) == 0);

  for (;;) {
    pa_stream_state_t state;

    state = pa_stream_get_state(pulse->stream);

    if (state == PA_STREAM_READY)
      break;

    /* Wait until the stream has terminated. */
    pa_threaded_mainloop_wait(pulse->mainloop);
  }

  pa_threaded_mainloop_unlock(pulse->mainloop);
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
  printf("Disconnecting stream.\n");

  // TODO drain stream. this seems to cause deadlocks.

  pa_threaded_mainloop_lock(pulse->mainloop);
  pa_stream_disconnect(pulse->stream);

  for (;;) {
    pa_stream_state_t state;

    state = pa_stream_get_state(pulse->stream);

    if (state == PA_STREAM_TERMINATED)
      break;

    /* Wait until the stream has terminated. */
    pa_threaded_mainloop_wait(pulse->mainloop);
  }

  pa_stream_unref(pulse->stream);
  pa_threaded_mainloop_unlock(pulse->mainloop);

  pulse->stream = NULL;

  printf("Stream disconnected.\n");
}
