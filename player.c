#include <arpa/inet.h>
#include <pulse/simple.h>
#include <error.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "player.h"

struct {
  pa_simple *pa_stream;
  pa_sample_spec ss;
  unsigned int last_frame;
} G = {};

// prototypes
void play_frame(struct audio_frame *frame);

// functions
int latency_helper(int samplerate, int latency) {
	int multiplier = (samplerate%441) == 0 ? 44100 : 48000;
  return ((unsigned long long int)latency * samplerate) / (256 * multiplier);
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

void handle_frame(ohm1_audio *frame) {
  struct audio_frame *aframe = parse_frame(frame);

  if (aframe == NULL)
    return;

  if (aframe->seqnum == G.last_frame + 1 || G.last_frame == 0) {
    // next frame. play immediately
    play_frame(aframe);
    G.last_frame = aframe->seqnum;

    free_frame(aframe);
  } else if (aframe->seqnum < G.last_frame) {
     // ignore old frames
    return;
  } else {
    printf("Gap! %i\n", aframe->seqnum - G.last_frame - 1);
    G.last_frame = 0;
    // cache could be a linked list
    // insertion sort?
    // skipped frame
    // TODO put to cache
    //       update list of missing frames, send resend request(s)
    //       check if cache holds frame_latency amount of data
    //       if so, pull from cache
    //       generally, we only pull from cache sequentially
    //       if we can't do that, a buffer underrun occurs and the stream corks

    // Buffer Underruns
    // - a buffer underrun will cork the stream
    // Precondition: Stream is corked.
    // - start again when we have enough data to fill the buffer
    // - this will be triggered by an incoming frame
    // - fill the buffer
    // - discard any old cached frames
    // - calculate and set readpointer, then uncork stream
    // - this may skip some samples
    // - what is the exact time the playback should resume?
    // - if we have not timestamps, we can only infer that the frame just received should play after the latency has passed
    // - does this even differ from a real start? could this behaviour be merged?
  }

  // TODO try pull from cache
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
}
