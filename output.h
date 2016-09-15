#pragma once

#include <pulse/pulseaudio.h>

struct pulse {
  pa_threaded_mainloop *mainloop;
  pa_context *context;
  pa_stream *stream;
};

void output_init(struct pulse *pulse);
void create_stream(struct pulse *pulse, pa_sample_spec *ss, const pa_buffer_attr *bufattr);
void stop_stream(pa_stream *s, void (*cb)(void));
