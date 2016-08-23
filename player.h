#pragma once

#include <pulse/sample.h>

#include "ohm_v1.h"

struct audio_frame {
  unsigned int seqnum;
  pa_sample_spec ss;
  int latency;
  int samplecount;
  void *audio;
  size_t audio_length;
  bool halt;
};

void player_stop(void);
void handle_frame(ohm1_audio *frame);
