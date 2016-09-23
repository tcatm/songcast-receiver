#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#define CACHE_SIZE 2000     // frames

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
  int start_seqnum;
  int latest_index;
  int size;
  unsigned int offset;
  struct audio_frame *frames[CACHE_SIZE];
};

struct missing_frames {
  int count;
  unsigned int seqnums[];
};

void print_cache(struct cache *cache);
void remove_old_frames(struct cache *cache, uint64_t now_usec);
struct cache_info cache_continuous_size(struct cache *cache);
void discard_cache_through(struct cache *cache, int discard);
int cache_pos(struct cache *cache, int index);
bool trim_cache(struct cache *cache, size_t trim);
void fixup_timestamps(struct cache *cache);
void discard_cache_through(struct cache *cache, int discard);
struct missing_frames *request_frames(struct cache *cache);
void remove_old_frames(struct cache *cache, uint64_t now_usec);
