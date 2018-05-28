#include <pulse/pulseaudio.h>
#include <error.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <complex.h>
#include <samplerate.h>

#include "player.h"
#include "output.h"
#include "cache.h"
#include "log.h"

// TODO determine CACHE_SIZE dynamically based on latency? 192/24 needs a larger cache
// TODO determine BUFFER_LATENCY automagically
#define CACHE_SIZE 500 // frames
#define BUFFER_LATENCY 80e3 // 50ms buffer latency

#define PLAYER_VOLUME_MAX 100
#define PLAYER_VOLUME_LIMIT 60
#define PLAYER_VOLUME_START 20

#define PA_CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))  


FILE *logfile, *clockfile;

// prototypes
bool process_frame(player_t *player, struct audio_frame *frame);
void play_audio(player_t *player, pa_stream *s, size_t writable, size_t *written_pre, size_t *written_post);
void write_data(player_t *player, pa_stream *s, size_t request);
void set_state(player_t *player, enum PlayerState new_state);
void update_pa_filter(player_t *player);

// callbacks
void write_cb(pa_stream *s, size_t request, void *userdata) {
  player_t *player = userdata;

  assert(s == player->pulse.stream);

  pthread_mutex_lock(&player->mutex);

  write_data(player, s, request);

  pa_operation *o = pa_stream_update_timing_info(s, NULL, NULL);

  if (o != NULL)
    pa_operation_unref(o);

  pthread_mutex_unlock(&player->mutex);
}

void underflow_cb(pa_stream *s, void *userdata) {
  player_t *player = userdata;

  assert(s == player->pulse.stream);

  log_printf("Underflow!");

  pthread_mutex_lock(&player->mutex);
  set_state(player, HALT);
  pthread_mutex_unlock(&player->mutex);
}

void latency_cb(pa_stream *s, void *userdata) {
  player_t *player = userdata;

  assert(s == player->pulse.stream);

  pthread_mutex_lock(&player->mutex);
  update_pa_filter(player);
  pthread_mutex_unlock(&player->mutex);
}

struct output_cb callbacks = {
  .write = write_cb,
  .underflow = underflow_cb,
  .latency = latency_cb,
};

// functions
uint64_t now_usec(void) {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  return (long long)now.tv_sec * 1000000 + (now.tv_nsec + 500) / 1000;
}

char *print_state(enum PlayerState state) {
  switch (state) {
    case STOPPED:
      return "STOPPED";
      break;
    case STARTING:
      return "STARTING";
      break;
    case PLAYING:
      return "PLAYING";
      break;
    case HALT:
      return "HALT";
      break;
    default:
      assert(false && "UNKNOWN STATE");
  }
}

void set_volume_limit(player_t *player, int limit) {
  if (limit < 0)
    limit = 0;

  if (limit > PLAYER_VOLUME_MAX)
    limit = PLAYER_VOLUME_MAX;

  player->volume_limit = limit;

  // Reduce volume if limit is less than current volume
  if (player->volume > limit)
    player_set_volume(player, limit);

  device_set_volume_limit(&player->dctx, limit);
}

void set_state(player_t *player, enum PlayerState new_state) {
  log_printf("--> State change: %s to %s <--", print_state(player->state), print_state(new_state));

  player->state = new_state;

  char *transport_state;

  transport_state = "Stopped";

  switch (player->state) {
    case STOPPED:
      transport_state = "Stopped";
      break;
    case STARTING:
      transport_state = "Buffering";
      break;
    case PLAYING:
      transport_state = "Playing";
      break;
    case HALT:
      transport_state = "Stopped";
      break;
    default:
      assert(false && "UNKNOWN STATE");
  }

  device_set_transport_state(&player->dctx, transport_state);
}

void reset_remote_clock(struct remote_clock *clock) {
  clock->invalid = true;
}

void stop(player_t *player) {
  log_printf("Stopping stream.");
  stop_stream(&player->pulse);
  src_delete(player->src);
  set_state(player, STOPPED);
}

void player_init(player_t *player) {
  logfile = fopen("logfile", "w");
  clockfile = fopen("clockfile", "w");

  pthread_mutex_init(&player->mutex, NULL);

  reset_remote_clock(&player->remote_clock);
  set_state(player, STOPPED);
  // Set volume limit first, set_volume depends on it!
  set_volume_limit(player, PLAYER_VOLUME_LIMIT);
  player_set_volume(player, PLAYER_VOLUME_START);
  player->cache = cache_init(CACHE_SIZE);
  player->mute = 0;
  output_init(&player->pulse);
}

// Stop playback
void player_stop(player_t *player) {
  pthread_mutex_lock(&player->mutex);

  if (player->state != STOPPED) {
    stop(player);
    set_state(player, STOPPED);
  }

  cache_reset(player->cache);

  pthread_mutex_unlock(&player->mutex);
}

void player_set_mute(player_t *player, int mute) {
  player->mute = mute;

  device_set_mute(&player->dctx, mute);

  if (player->state != PLAYING)
    return;
  
  output_set_mute(&player->pulse, mute);
}

int player_get_mute(player_t *player) {
  return player->mute;
}

void player_set_volume(player_t *player, int volume) {
  if (volume > player->volume_limit)
    volume = player->volume_limit;

  if (volume < 0)
    volume = 0;

  player->volume = volume; 

  if (player->state == PLAYING)
    output_set_volume(&player->pulse, player->volume);

  log_printf("Volume: %i", player->volume);

  device_set_volume(&player->dctx, player->volume);
}

void player_inc_volume(player_t *player) {
  player_set_volume(player, player->volume + 1);
}

void player_dec_volume(player_t *player) {
  player_set_volume(player, player->volume - 1);
}

int player_get_volume(player_t *player) {
  return player->volume;
}

int player_get_volume_max(player_t *player) {
  return PLAYER_VOLUME_MAX;
}

int player_get_volume_limit(player_t *player) {
  return player->volume_limit;
}

void try_prepare(player_t *player) {
  if (player->state != STOPPED)
    return;

  struct audio_frame *start = player->cache->frames[cache_pos(player->cache, 0)];

  if (start == NULL)
    return;

  log_printf("Preparing stream.");

  assert(player->state == STOPPED);

  set_state(player, STARTING);

  player->timing = (struct timing){
    .ss = start->ss,
    .estimated_rate = start->ss.rate,
    .avg_estimated_rate = start->ss.rate,
    .ratio = 1,
  };

  reset_remote_clock(&player->remote_clock);
  kalman2d_init(&player->timing.pa_filter, (mat2d){0, 0, 1, 0}, (mat2d){1000, 0, 0, 0.0001}, 10);

  pa_buffer_attr bufattr = {
    .maxlength = -1,
    .minreq = -1,
    .prebuf = -1,
    .tlength = pa_usec_to_bytes(BUFFER_LATENCY, &start->ss),
  };

  pthread_mutex_unlock(&player->mutex);

  int error;
  player->src = src_new(SRC_SINC_MEDIUM_QUALITY, start->ss.channels, &error);
  assert(player->src != NULL);

  SRC_STATE* src_new (int converter_type, int channels, int *error) ;

  create_stream(&player->pulse, &start->ss, &bufattr, player, &callbacks, player->volume, player->mute);

  pthread_mutex_lock(&player->mutex);

  log_printf("Stream prepared");
}

static bool prepare_for_start(player_t *player, size_t request) {
  const pa_sample_spec *ss = pa_stream_get_sample_spec(player->pulse.stream);
  const pa_timing_info *ti = pa_stream_get_timing_info(player->pulse.stream);

  if (ti == NULL || ti->playing != 1)
    return false;

  struct cache_info info = cache_continuous_size(player->cache);

  if (!info.has_timing)
    return false;

  // TODO how many frames are in the cache that could be used for clock smoothing?

  uint64_t ts = (uint64_t)ti->timestamp.tv_sec * 1000000 + ti->timestamp.tv_usec;
  int playback_latency = ti->sink_usec + ti->transport_usec +
                         pa_bytes_to_usec(ti->write_index - ti->read_index, ss);

  // TODO remote_clock.filter might be invalid. can we ensure v = 1 in case if a non timestamped stream?
  uint64_t start_at;

  start_at = info.start + info.latency_usec;

  uint64_t play_at = ts + playback_latency;

  // TODO below this line only start_at, play_at and info.available are used.

  int delta = play_at - start_at;

  log_printf("Request for %zd bytes, can start at %ld, would play at %ld, in %d usec", request, start_at, play_at, delta);

  if (delta < 0) {
    if (info.halt) {
      log_printf("halt frame in cache at %i", info.halt_index);
    }
    return false;
  }

  size_t skip = pa_usec_to_bytes(delta, ss);

  log_printf("Need to skip %zd bytes, have %zd bytes", skip, info.available);

  if (info.available < request + skip) {
    log_printf("Not enough data in buffer to start (missing %zd bytes)", request - skip - info.available);
    return false;
  }

  log_printf("Request can be fullfilled.");

  player->timing.start_play_usec = play_at;
  player->timing.avg_start_at = play_at;
  player->timing.avg_play_at = play_at;
  player->timing.avg_start_at_j = 1;

  trim_cache(player->cache, skip);
  print_cache_fixed(player);

  return true;
}

// TODO wants player. is it really needed? cache is used. timing is used.
// TODO what pre-conditions need to be met? cache must be present, stream is required
void write_data(player_t *player, pa_stream *s, size_t request) {
  if (player->state == STOPPED)
    return;

  switch (player->state) {
    case PLAYING:
      goto play;
      break;
    case STARTING:
      if (prepare_for_start(player, request)) {
        set_state(player, PLAYING);
        goto play;
      }

      goto silence;
      break;
    case STOPPED:
    case HALT:
    default:
      pthread_mutex_unlock(&player->mutex);
      return;
      break;
  }

  return;

uint8_t *silence;
size_t written_pre, written_post;

play:
  play_audio(player, s, request, &written_pre, &written_post);
  player->timing.written_pre += written_pre;
  player->timing.written_post += written_post;
  return;

silence:
  silence = calloc(1, request);
  pa_stream_write(s, silence, request, NULL, 0, PA_SEEK_RELATIVE);
  free(silence);
  return;
}

int min(int a, int b) {
  return a < b ? a : b;
}

void play_audio(player_t *player, pa_stream *s, size_t writable, size_t *written_pre, size_t *written_post) {
  *written_pre = 0;
  *written_post = 0;

  size_t frame_size = pa_frame_size(&player->timing.ss);

  // post has jitter due to SRC!
  double time_remote = pa_usec_to_bytes(player->timing.written_pre, &player->timing.ss);
  double time_pulseaudio = pa_usec_to_bytes(player->timing.written_post, &player->timing.ss) / kalman2d_get_v(&player->timing.pa_filter);

  double delta = time_remote - time_pulseaudio;

  player->timing.delta[player->timing.n_delta++%30] = delta;

  double avg_delta = 0;

  for (int i = 0; i < min(player->timing.n_delta, 30); i++)
    avg_delta += player->timing.delta[i];
  
  avg_delta /= min(player->timing.n_delta, 30);

  double clock_ratio = 1;
  double latency_ratio = 1;

  if (kalman2d_get_p(&player->timing.pa_filter) < 1e-7 && player->timing.n_delta > 30) {
    clock_ratio = 1.0 / kalman2d_get_v(&player->timing.pa_filter);

    double RATE_UPDATE_INTERVAL = 30e6;

    double current_rate = (double)player->timing.ss.rate;
    double estimated_rate = current_rate / ((double) RATE_UPDATE_INTERVAL / (RATE_UPDATE_INTERVAL + avg_delta));

    double alpha = 0.02;

    if (fabs(player->timing.estimated_rate - player->timing.avg_estimated_rate) > 1) {
      double ratio = (estimated_rate + player->timing.estimated_rate - 2*player->timing.avg_estimated_rate) 
                    / (player->timing.estimated_rate - player->timing.avg_estimated_rate);
      alpha = PA_CLAMP(2 * (ratio + fabs(ratio)) / (4.0 + ratio*ratio), 0.02, 0.8);
    }

    player->timing.avg_estimated_rate = alpha * estimated_rate + (1-alpha) * player->timing.avg_estimated_rate;
    player->timing.estimated_rate = estimated_rate;

    double new_rate = (RATE_UPDATE_INTERVAL + avg_delta/4.0) / RATE_UPDATE_INTERVAL * player->timing.avg_estimated_rate;

    latency_ratio = new_rate / current_rate;
  }

  double ratio = latency_ratio * clock_ratio;
  double effective_rate = ratio * player->timing.ss.rate;

  printf("\033[8;0H");
  printf("pre %f post %f avg_delta %5.0f cratio %f lratio %f est_rate %.2f eff_rate %.2f", time_remote, time_pulseaudio, avg_delta, clock_ratio, latency_ratio, player->timing.avg_estimated_rate, effective_rate);
  printf("\033[K");

  while (writable > 0) {
    int pos = cache_pos(player->cache, 0);
    struct audio_frame *frame = player->cache->frames[pos];

    if (frame == NULL) {
      log_printf("Missing frame.");
      break;
    }

    if (!pa_sample_spec_equal(&player->timing.ss, &frame->ss)) {
      log_printf("Sample spec mismatch.");
      break;
    }

    SRC_DATA src_data = {
      .data_in = frame->readptr,
      .input_frames = frame->audio_length / frame_size,
      .src_ratio = ratio,
      .end_of_input = frame->halt,
    };

    size_t out_size = writable;

    src_set_ratio(player->src, ratio);

    pa_stream_begin_write(s, &src_data.data_out, &out_size);

    src_data.output_frames = out_size / frame_size;

    assert(out_size == writable);

    src_process(player->src, &src_data);

    //printf("in %d used %d. out %d gen %d\n", src_data.input_frames, src_data.input_frames_used,
  //         src_data.output_frames, src_data.output_frames_gen);

    pa_stream_write(s, src_data.data_out, src_data.output_frames_gen * frame_size, NULL, 0LL, PA_SEEK_RELATIVE);

    // TODO check how much src actually consumed

    size_t bytes_consumed = src_data.input_frames_used * frame_size;
    size_t leftover = frame->audio_length - bytes_consumed;

    bool consumed = leftover == 0;

    if (!consumed) {
      frame->readptr = (uint8_t*)frame->readptr + bytes_consumed;
      frame->audio_length -= bytes_consumed;
    }

    *written_pre += src_data.input_frames_used * frame_size;
    *written_post += src_data.output_frames_gen * frame_size;
    writable -= src_data.output_frames_gen * frame_size;

    assert(writable >= 0);

    if (consumed) {
      bool halt = frame->halt;

      free_frame(frame);
      player->cache->frames[pos] = NULL;
      player->cache->offset++;
      player->cache->start_seqnum++;
      if (player->cache->latest_index > 0)
        player->cache->latest_index--;

      if (halt) {
        log_printf("HALT received.");
        set_state(player, HALT);
        return;
      }
    }
  }

  if (writable > 0) {
   // log_printf("Not enough data. Stopping.");
   // set_state(player, HALT);
   // return;
  }
}

void print_cache_fixed(player_t *player) {
	return;
  printf("\033[0;0H");
  print_cache(player->cache);
}

struct missing_frames *handle_frame(player_t *player, ohm1_audio *frame, struct timespec *ts) {
  struct missing_frames *missing = NULL;
  struct audio_frame *aframe = parse_frame(frame);

  if (aframe == NULL)
    return NULL;

  // TODO incorporate any network latencies and such into ts_due_usec
  aframe->ts_recv_usec = (long long)ts->tv_sec * 1000000 + (ts->tv_nsec + 500) / 1000;

  if (player->state == HALT)
    stop(player);

  pthread_mutex_lock(&player->mutex);

  bool consumed = process_frame(player, aframe);

  print_cache_fixed(player);

  if (consumed) {
    // Don't send resend requests when the frame was an answer.
    if (!aframe->resent)
      missing = request_frames(player->cache);

    try_prepare(player);
  } else {
    free_frame(aframe);
  }

  pthread_mutex_unlock(&player->mutex);

  return missing;
}

void estimate_remote_clock(struct remote_clock *clock, struct audio_frame *frame, struct audio_frame *successor) {
  assert(frame != NULL);
  assert(successor != NULL);
  assert(successor->seqnum > frame->seqnum);

  uint64_t ts_local = frame->ts_recv_usec;
  int delta_remote_raw = successor->ts_network - clock->ts_remote_last;

  // Assume wrap around if delta is negative.
  if (delta_remote_raw < 0)
    delta_remote_raw += 1ULL<<32;

  double delta_local = ts_local - clock->ts_local_last;
  double delta_remote = latency_to_usec(successor->ss.rate, delta_remote_raw);

  clock->ts_remote += delta_remote;

  clock->ts_local_last = ts_local;
  clock->ts_remote_last = successor->ts_network;

  double ratio = abs(delta_local - delta_remote) / (double)delta_remote;

  // If both clocks differ by more than 5% skip this frame.
  if (ratio > 0.05) {
    clock->delta += delta_local;
    return;
  }

  delta_local += clock->delta;
  clock->delta = 0;

  uint64_t elapsed_local = ts_local - clock->ts_local_0;

  if (clock->invalid) {
    clock->ts_remote = 0;
    clock->ts_local_0 = ts_local;
    clock->invalid = false;
    // TODO determine good values here...
    kalman2d_init(&clock->filter, (mat2d){0, 0, 1, 0}, (mat2d){0, 0, 0, 0.0001}, 300);
    return;
  }

  if (delta_remote > 100e3) {
    reset_remote_clock(clock);
    return;
  }

//  printf("%" PRIu64 "\t%" PRIu64 " %d\t", elapsed_local, clock->ts_remote, delta_remote);

  kalman2d_run(&clock->filter, delta_local, clock->ts_remote);

  fprintf(clockfile, "%f %f %f %f %f\n",
          (double)delta_local, (double)delta_remote, (double)clock->ts_remote,
          kalman2d_get_x(&clock->filter), kalman2d_get_v(&clock->filter)
         );
  fflush(clockfile);
// printf("%f\n", kalman2d_get_v(&clock->filter));

  if (kalman2d_get_p(&clock->filter) < 0.001) {
    // TODO figure out what to do without network timestamps
    frame->ts_due_usec = clock->ts_local_0 + kalman2d_get_x(&clock->filter) / kalman2d_get_v(&clock->filter);
    frame->timestamp_is_good = true;
  }
}

bool process_frame(player_t *player, struct audio_frame *frame) {
  cache_seek_forward(player->cache, frame->seqnum);

  // The index stays fixed while this function runs.
  int index = frame->seqnum - player->cache->start_seqnum;
  int pos = cache_pos(player->cache, index);
  if (index < 0)
    return false;

  printf("\033[7;0H");
  printf("Handling frame %i %s %s media %" PRId64 " network %" PRId64 " ", frame->seqnum, frame->resent ? "(resent)" : "", frame->timestamped ? ("timestamped") : "no_timestamp"), frame->ts_media, frame->ts_network;
  printf("latency: %.0fms", latency_to_usec(frame->ss.rate, frame->latency) / 1e3);
  printf("\033[K");

  if (player->cache->frames[pos] != NULL)
    return false;

  player->cache->frames[pos] = frame;

  if (index > player->cache->latest_index)
    player->cache->latest_index = index;

  // Get previous frame to calculate timing.

  if (index < 1)
    return true;

  struct audio_frame *predecessor = player->cache->frames[cache_pos(player->cache, index - 1)];

  if (predecessor == NULL)
    return true;

  if (!frame->timestamped || !predecessor->timestamped ||
      predecessor->resent || frame->resent || predecessor->halt ||
      !same_format(frame, predecessor))
    return true;

  estimate_remote_clock(&player->remote_clock, predecessor, frame);

  return true;
}

void update_pa_filter(player_t *player) {
  const pa_sample_spec *ss = pa_stream_get_sample_spec(player->pulse.stream);
  pa_timing_info ti = *pa_stream_get_timing_info(player->pulse.stream);
  uint64_t ts = (uint64_t)ti.timestamp.tv_sec * 1000000 + ti.timestamp.tv_usec;

  if (player->timing.start_local_usec == 0) {
    // Prepare timing information
    player->timing.start_local_usec = ts;
    player->timing.pa_offset_bytes = ti.read_index;
    player->timing.local_last = ts;
  } else {
    double elapsed_audio = pa_bytes_to_usec(ti.read_index - player->timing.pa_offset_bytes, ss);
    double delta_local = (ts - player->timing.local_last);
    player->timing.local_last = ts;
    kalman2d_run(&player->timing.pa_filter, delta_local, elapsed_audio);
  }
}

