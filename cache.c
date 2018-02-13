#include <math.h>

#include "cache.h"
#include "audio_frame.h"

int cache_pos(struct cache *cache, int index) {
  return (index + cache->offset) % cache->size;
}

struct cache *cache_init(unsigned int size) {
  struct cache *cache = calloc(1, sizeof(struct cache) + sizeof(struct audio_frame*) * size);
  assert(cache != NULL);

  cache->size = size;
  cache->latest_index = 0;
  cache->start_seqnum = 0;
  cache->offset = 0;

  printf("Cache initialized\n");

  return cache;
}

void cache_reset(struct cache *cache) {
  int end = cache->size;
  for (int index = 0; index < end; index++) {
    struct audio_frame *frame = cache->frames[index];
    if (frame != NULL) {
      free_frame(frame);
      cache->frames[index] = NULL;
    }
  }

  cache->latest_index = 0;
  cache->start_seqnum = 0;
  cache->offset = 0;

  printf("Cache reset\n");
}

void print_cache(struct cache *cache) {
  assert(cache != NULL);

  printf("%10u [", cache->start_seqnum);

  struct audio_frame *last = NULL;
  int end = cache->size;
  for (int index = 0; index < end; index++) {
    int pos = cache_pos(cache, index);

    if (index > 0 && index%100 == 0)
      printf("]\n           [");

    struct audio_frame *frame = cache->frames[pos];
    char c = '?';
    int fg = -1;
    if (frame == NULL)    c = index > cache->latest_index ? ' ' : '.';
    else {
      if (frame->halt)    fg = 1;
      if (frame->resent)  c = '/';
      else                c = '#';
    }

    if (last != NULL && frame != NULL && !same_format(last, frame))
      fg = 2;

    last = frame;

    if (fg != -1) {
      printf("\e[3%1im%c\e[m", fg, c);
    } else
      printf("%c", c);
  }

  printf("]\n");
}

struct cache_info cache_continuous_size(struct cache *cache) {
  assert(cache != NULL);

  struct audio_frame *last = NULL;

  struct cache_info info = {
    .available = 0,
    .halt = false,
    .format_change = false,
    .start = 0,
    .halt_index = -1,
    .format_change_index = -1,
    .latency_usec = 0,
    .timestamped = true,
    .has_timing = false,
  };

  double ts_mean = 0;

  int64_t ts_min, ts_max;

  int j = 0;

  // Ignore the last frame. Its net_offset won't be ready yet.
  // play_audio might still play it, though.
  int end = cache->latest_index - 1;
  for (int index = 0; index <= end; index++) {
    int pos = cache_pos(cache, index);
    struct audio_frame *frame = cache->frames[pos];

    if (frame == NULL)
      break;

    if (!frame->timestamped)
      info.timestamped = false;

    if (last != NULL && !same_format(last, frame)) {
      info.format_change_index = index;
      info.format_change = true;
      break;
    }

    info.available += frame->audio_length;

    if (!frame->resent && frame->audio == frame->readptr && frame->audio_length > 0 &&
        (!frame->timestamped || frame->timestamp_is_good)) {
      int64_t ts;

      if (frame->timestamped)
        ts = frame->ts_due_usec - pa_bytes_to_usec(info.available, &frame->ss);
      else
        ts = frame->ts_recv_usec - pa_bytes_to_usec(info.available, &frame->ss);

      double d;

      // TODO use Kalman Filter + RTS to estimate start?
      j++;

      d = ts - ts_mean;
      ts_mean += d / j;
    }

    info.latency_usec = latency_to_usec(frame->ss.rate, frame->latency);

    if (frame->halt) {
      info.halt_index = index;
      info.halt = true;
      break;
    }

    last = frame;
  }

  if (j < 2)
    return info;

  info.has_timing = true;
  info.start = ts_mean + 0.5;

  return info;
}

// Remove trim amount of bytes from the start of the cache.
// Returns true on success; false if there is not enough data to trim.
bool trim_cache(struct cache *cache, size_t trim) {
  if (trim == 0)
    return true;

  printf("Trimming %zd bytes\n", trim);

  int end = cache->latest_index;
  int adjust = 0;
  for (int index = 0; index <= end && trim > 0; index++) {
    int pos = cache_pos(cache, index);
    struct audio_frame *frame = cache->frames[pos];

    if (frame == NULL)
      break;

    if (frame->audio_length < trim) {
      trim -= frame->audio_length;
      free_frame(frame);
      cache->frames[pos] = NULL;
      adjust++;
    } else {
      frame->readptr = (uint8_t*)frame->readptr + trim;
      frame->audio_length -= trim;
      trim = 0;
    }
  }

  cache->offset += adjust;
  cache->start_seqnum += adjust;
  cache->latest_index -= adjust;

  assert(trim == 0);

  if (trim != 0)
    return false;

  return true;
}

struct missing_frames *request_frames(struct cache *cache) {
  assert(cache != NULL);

  struct missing_frames *d = calloc(1, sizeof(struct missing_frames) + sizeof(unsigned int) * cache->latest_index);
  assert(d != NULL);

  int end = cache->latest_index;
  for (int index = 0; index <= end; index++) {
    int pos = cache_pos(cache, index);
    if (cache->frames[pos] == NULL)
      d->seqnums[d->count++] = (long long int)index + cache->start_seqnum;
  }

  if (d->count == 0) {
    free(d);
    return NULL;
  }

  return d;
}

// Adjusts cache such that the seqnum will fit within the cache, possibly
// at the end.
void cache_seek_forward(struct cache *cache, unsigned int seqnum) {
  assert(cache != NULL);

  if (seqnum < cache->start_seqnum + cache->size)
    return;

  int offset = seqnum - cache->start_seqnum - cache->size;

  int discard = offset < cache->latest_index ? offset : cache->latest_index;

  for (int index = 0; index <= discard; index++) {
    int pos = cache_pos(cache, index);

    if (cache->frames[pos] != NULL) {
      free_frame(cache->frames[pos]);
      cache->frames[pos] = NULL;
    }
  }

  if (offset >= cache->latest_index) {
    cache->offset = 0;
    cache->start_seqnum = seqnum;
    cache->latest_index = 0;
  } else {
    cache->offset += offset + 1;
    cache->start_seqnum += offset + 1;
    cache->latest_index -= offset + 1;
  }

}
