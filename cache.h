#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

struct cache_info {
  size_t available;
  int64_t start;
  int64_t net_offset;
  int halt_index;
  int format_change_index;
  bool halt;
  bool format_change;
  bool timestamped;
  int latency_usec;
};

struct cache {
  unsigned int start_seqnum;
  int latest_index;
  int size;
  unsigned int offset;
  struct audio_frame *frames[];
};

struct missing_frames {
  int count;
  unsigned int seqnums[];
};

struct cache *cache_init(unsigned int size);
void cache_reset(struct cache *cache);
void print_cache(struct cache *cache);
struct cache_info cache_continuous_size(struct cache *cache);
void cache_seek_forward(struct cache *cache, unsigned int seqnum);
int cache_pos(struct cache *cache, int index);
bool trim_cache(struct cache *cache, size_t trim);
void fixup_timestamps(struct cache *cache);
void discard_cache_through(struct cache *cache, int discard);
struct missing_frames *request_frames(struct cache *cache);
