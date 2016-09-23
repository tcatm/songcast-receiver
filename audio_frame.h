#pragma once

#include <stdbool.h>
#include <pulse/sample.h>

#include "ohm_v1.h"

struct audio_frame {
  uint64_t ts_recv_usec;
  uint64_t ts_due_usec;
  uint64_t ts_network;
  uint64_t ts_media;
  uint64_t net_offset;
  unsigned int seqnum;
  pa_sample_spec ss;
  int latency;
  int samplecount;
  void *audio;
  void *readptr;
  size_t audio_length;
  bool halt;
  bool resent;
  bool timestamped;
};

bool same_format(struct audio_frame *a, struct audio_frame *b);
uint64_t latency_to_usec(int samplerate, uint64_t latency);
void free_frame(struct audio_frame *frame);
struct audio_frame *parse_frame(ohm1_audio *frame);
