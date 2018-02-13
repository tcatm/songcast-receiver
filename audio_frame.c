#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "audio_frame.h"

bool same_format(struct audio_frame *a, struct audio_frame *b) {
  return pa_sample_spec_equal(&a->ss, &b->ss) && a->latency == b->latency;
}

double latency_to_usec(int samplerate, int64_t latency) {
  double multiplier = (samplerate%441) == 0 ? 44100 : 48000;
  return latency * 1000000.0 / (256.0 * multiplier);
}

void free_frame(struct audio_frame *frame) {
  free(frame->audio);
  free(frame);
}

bool frame_to_sample_spec(pa_sample_spec *ss, int rate, int channels, int bitdepth) {
  *ss = (pa_sample_spec){
    .rate = rate,
    .channels = channels
  };

  switch (bitdepth) {
    case 24:
      ss->format = PA_SAMPLE_S24BE;
      break;
    case 16:
      ss->format = PA_SAMPLE_S16BE;
      break;
    default:
      return false;
  }

  return true;
}

struct audio_frame *parse_frame(ohm1_audio *frame) {
  struct audio_frame *aframe = calloc(1, sizeof(struct audio_frame));

  if (aframe == NULL)
    return NULL;

  unsigned int framecount = ntohs(frame->samplecount);

  aframe->seqnum = ntohl(frame->frame);
  aframe->latency = ntohl(frame->media_latency);
  aframe->halt = frame->flags & OHM1_FLAG_HALT;
  aframe->resent = frame->flags & OHM1_FLAG_RESENT;
  aframe->timestamped = frame->flags & OHM1_FLAG_TIMESTAMPED;
  
  if (aframe->timestamped)
    printf("Timestamped\n");

  if (!frame_to_sample_spec(&aframe->ss, ntohl(frame->samplerate), frame->channels, frame->bitdepth)) {
    printf("Unsupported sample spec\n");
    free(aframe);
    return NULL;
  }

  aframe->ss.format = PA_SAMPLE_FLOAT32LE;
  aframe->bitdepth = frame->bitdepth;

  aframe->ts_network = ntohl(frame->network_timestamp);
  aframe->ts_media = ntohl(frame->media_timestamp);

  aframe->audio_length = pa_frame_size(&aframe->ss) * framecount;

  aframe->audio = malloc(aframe->audio_length);
  aframe->readptr = aframe->audio;

  if (aframe->audio == NULL) {
    free(aframe);
    return NULL;
  }

  unsigned int samplecount = framecount * aframe->ss.channels;
  uint8_t *src = frame->data + frame->codec_length;
  float *dst = aframe->audio;

  switch (aframe->bitdepth) {
    case 24:
      for (size_t i = 0; i < samplecount; i++) {
        int32_t s = src[3 * i + 0] << 24 | src[3 * i + 1] << 16 | src[3 * i + 2] << 8;
        *dst++ = s * (1.0f / (1U << 31));
      }
      break;
    case 16:
      for (size_t i = 0; i < samplecount; i++) {
        int32_t s = src[2 * i + 0] << 24 | src[2 * i + 1] << 16;
        *dst++ = s * (1.0f / (1U << 31));
      }
      break;
  }

//  memcpy(aframe->audio, frame->data + frame->codec_length, aframe->audio_length);


  // TODO convert from bitdepth to float here


  return aframe;
}
