#include <arpa/inet.h>
#include <pulse/simple.h>
#include <error.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "player.h"
#include "main.h"

#define CACHE_SIZE 200

struct {
  pa_simple *pa_stream;
  pa_sample_spec ss;
  unsigned int last_played;
  unsigned int last_received;
  struct audio_frame *cache[CACHE_SIZE];
} G = {};

// prototypes
void play_frame(struct audio_frame *frame);
void process_frame(struct audio_frame *frame);

// functions
int latency_helper(int samplerate, int latency) {
	int multiplier = (samplerate%441) == 0 ? 44100 : 48000;
  return ((unsigned long long int)latency * samplerate) / (256 * multiplier);
}

void player_init(void) {
  G.last_played = 0;
  G.last_received = 0;
}

void player_stop(void) {
  // TODO stop the player
}

void drain(void) {
  printf("Draining.\n");

  if (G.pa_stream == NULL)
    return;

  pa_simple_drain(G.pa_stream, NULL);
  pa_simple_free(G.pa_stream);
  G.pa_stream = NULL;
}

void create_stream(pa_sample_spec *ss, int latency) {
  printf("Start stream.\n");

  pa_buffer_attr bufattr = {
    .maxlength = -1,
    .minreq = -1,
    .prebuf = -1,
    .tlength = 2 * latency_helper(ss->rate, latency) * pa_frame_size(ss),
  };

	G.pa_stream = pa_simple_new(NULL, "Songcast Receiver", PA_STREAM_PLAYBACK,
	                            NULL, "Songcast Receiver", ss, NULL, &bufattr, NULL);
	G.ss = *ss;
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
  // TODO copy audio data
  struct audio_frame *aframe = malloc(sizeof(struct audio_frame));

  if (aframe == NULL)
    return NULL;

  aframe->seqnum = ntohl(frame->frame);
  aframe->latency = ntohl(frame->media_latency);
  aframe->audio_length = frame->channels * frame->bitdepth * ntohs(frame->samplecount) / 8;
  aframe->halt = frame->flags & OHM1_FLAG_HALT;
  aframe->resent = frame->flags & OHM1_FLAG_RESENT;

  if (!frame_to_sample_spec(&aframe->ss, ntohl(frame->samplerate), frame->channels, frame->bitdepth)) {
    printf("Unsupported sample spec\n");
    free(aframe);
    return NULL;
  }

  aframe->audio = malloc(aframe->audio_length);

  if (aframe->audio == NULL) {
    free(aframe);
    return NULL;
  }

  memcpy(aframe->audio, frame->data + frame->codec_length, aframe->audio_length);

  return aframe;
}

void free_frame(struct audio_frame *frame) {
  free(frame->audio);
  free(frame);
}

void request_frames(void) {
  unsigned int seqnums[CACHE_SIZE];
  int count = 0;

  int start = G.last_played + CACHE_SIZE - (long long int)G.last_received;
  if (start < 0)
    start = 0;

  for (int i = start; i < CACHE_SIZE; i++)
    if (G.cache[i] == NULL)
      seqnums[count++] = (long long int)G.last_received - CACHE_SIZE + i + 1;

  ohm_send_resend_request(seqnums, count);
}

void handle_frame(ohm1_audio *frame) {
  struct audio_frame *aframe = parse_frame(frame);

  if (aframe == NULL)
    return;

  process_frame(aframe);

  // Don't send resend requests when the frame was an answer.
  if (!aframe->resent)
    request_frames();

  // TODO could handle_frame return a list of missing frames instead of using callbacks?
}

void process_frame(struct audio_frame *frame) {
  printf("Handling frame %i %s\n", frame->seqnum, frame->resent ? "(resent)" : "");

  if (frame->seqnum <= G.last_played) {
    free_frame(frame);
    return;
  }

  if (frame->seqnum == G.last_played + 1 || G.last_played == 0) {
    // This is the next frame. Play it.
    printf("Playing %i                       ==== PLAY ===\n", frame->seqnum);
    play_frame(frame);

    int start = G.last_played + CACHE_SIZE - (long long int)G.last_received;
    if (start < 0)
      start = 0;

    for (int i = start; i < CACHE_SIZE; i++) {
      struct audio_frame *cframe = G.cache[i];

      if (cframe == NULL)
        break;

      printf("Cache frame %i, expecting %i\n", cframe->seqnum, G.last_played + 1);

      if (cframe->seqnum != G.last_played + 1)
        break;

      printf("Playing from cache %i\n", cframe->seqnum);

      play_frame(cframe);
      G.cache[i] = NULL;
    }

    return;
  }

  // Frame is too old to be cached.
  if (frame->seqnum < (long long int)G.last_received - CACHE_SIZE) {
    free_frame(frame);
    return;
  }

  if (frame->seqnum > G.last_received) {
    printf("Frame from the future %i.\n", frame->seqnum);
    // This is a frame from the future.
    // Shift cache.

    // aframe->seqnum - last_received frames at the start must be discarded
    int delta = frame->seqnum - G.last_received;

    for (int i = 0; i < CACHE_SIZE; i++) {
      if (i < delta && G.cache[i] != NULL)
        free_frame(G.cache[i]);

      if (i < CACHE_SIZE - delta)
        G.cache[i] = G.cache[i + delta];
      else
        G.cache[i] = NULL;
    }

    G.last_received = frame->seqnum;
  }

  int i = frame->seqnum + CACHE_SIZE - G.last_received - 1;

  printf("Caching frame %i\n", frame->seqnum);
  if (G.cache[i] == NULL)
    G.cache[i] = frame;
  else
    free_frame(frame);
}

void play_frame(struct audio_frame *frame) {
//    printf("frame halt %i frame %i audio_length %zi\n", frame->halt, frame->seqnum, frame->audio_length);

	if (!pa_sample_spec_equal(&G.ss, &frame->ss))
    drain();

	if (G.pa_stream == NULL)
    create_stream(&frame->ss, frame->latency);

	pa_simple_write(G.pa_stream, frame->audio, frame->audio_length, NULL);

	if (frame->halt)
    drain();

  G.last_played = frame->seqnum;
  free_frame(frame);
}
