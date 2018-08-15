#pragma once

#include <pulse/pulseaudio.h>

struct pulse {
  pa_threaded_mainloop *mainloop;
  pa_context *context;
  pa_stream *stream;
  int operation_success;
};

struct output_cb {
  pa_stream_request_cb_t write;
  pa_stream_notify_cb_t underflow;
  pa_stream_notify_cb_t latency;
};

void output_init(struct pulse *pulse);
void create_stream(struct pulse *pulse, pa_sample_spec *ss, const pa_buffer_attr *bufattr, void *userdata, struct output_cb *callbacks, int volume, int mute);
void stop_stream(struct pulse *pulse);
void output_set_mute(struct pulse *pulse, int mute);
void output_set_volume(struct pulse *pulse, int volume);
