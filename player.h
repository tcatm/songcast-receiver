#pragma once

#include <pulse/sample.h>
#include <time.h>
#include <stdint.h>

#include "ohm_v1.h"

struct audio_frame {
  uint64_t ts_recv_usec;
  uint64_t ts_due_usec;
  uint64_t ts_network;
  uint64_t ts_media;
  unsigned int seqnum;
  pa_sample_spec ss;
  int latency;
  int samplecount;
  void *audio;
  void *readptr;
  size_t audio_length;
  bool halt;
  bool resent;
};

struct missing_frames {
  int count;
  unsigned int seqnums[];
};

void player_init(void);
void player_stop(void);
struct missing_frames *handle_frame(ohm1_audio *frame, struct timespec *ts);
