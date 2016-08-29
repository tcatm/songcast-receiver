#pragma once

#include <pulse/sample.h>
#include <time.h>

#include "ohm_v1.h"

struct audio_frame {
  struct timespec ts_recv;
  unsigned int seqnum;
  unsigned int ts_network;
  pa_sample_spec ss;
  int latency;
  int samplecount;
  void *audio;
  size_t audio_length;
  bool halt;
  bool resent;
};

struct cache {
  struct audio_frame *frame;
  struct cache *next;
  struct cache *prev;
};

struct missing_frames {
  int count;
  unsigned int seqnums[];
};

void player_init(void);
void player_stop(void);
struct missing_frames *handle_frame(ohm1_audio *frame, struct timespec *ts);
