#pragma once

#include <pulse/pulseaudio.h>

struct pulse {
  pa_threaded_mainloop *mainloop;
  pa_context *context;
  pa_stream *stream;
};

struct output_cb {
  void (*write)(pa_stream *s, size_t size, void *userdata);
  void (*underflow)(pa_stream *s, void *userdata);
};

void output_init(struct pulse *pulse);
void create_stream(struct pulse *pulse, pa_sample_spec *ss, const pa_buffer_attr *bufattr, void *userdata, struct output_cb *callbacks);
void stop_stream(pa_stream *s, void (*cb)(void));
