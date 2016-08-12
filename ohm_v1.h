#pragma once

#include <stdint.h>

enum {
  OHM1_JOIN = 0,
  OHM1_LISTEN,
  OHM1_LEAVE,
  OHM1_AUDIO,
  OHM1_TRACK,
  OHM1_METATEXT,
  OHM1_SLAVE
};

#define OHM1_FLAG_HALT (1<<0)
#define OHM1_FLAG_LOSSLESS (1<<1)
#define OHM1_FLAG_TIMESTAMPED (1<<2)
#define OHM1_FLAG_RESENT (1<<3)

typedef struct __attribute__((__packed__)) {
  int8_t signature[4];
  uint8_t version;
  uint8_t type;
  uint16_t length;
} ohm1_header;

typedef struct __attribute__((__packed__)) {
  ohm1_header hdr;
  uint8_t audio_hdr_length;
  uint8_t flags;
  uint16_t samplecount;
  uint32_t frame;
  uint32_t network_timestamp;
  uint32_t media_latency;
  uint32_t media_timestamp;
  uint64_t start_sample;
  uint64_t total_samples;
  uint32_t samplerate;
  uint32_t bitrate;
  uint16_t volume_offset;
  uint8_t bitdepth;
  uint8_t channels;
  uint8_t reserved;
  uint8_t codec_length;
  uint8_t data[];
} ohm1_audio;

typedef struct __attribute__((__packed__)) {
  ohm1_header hdr;
  uint32_t sequence;
  uint32_t uri_length;
  uint32_t metadata_length;
  uint8_t data[];
} ohm1_track;

typedef struct __attribute__((__packed__)) {
  ohm1_header hdr;
  uint32_t sequence;
  uint32_t length;
  uint8_t data[];
} ohm1_metatext;

typedef struct __attribute__((__packed__)) {
  uint32_t addr;
  uint16_t port;
} ohm1_slave_entry;

typedef struct __attribute__((__packed__)) {
  ohm1_header hdr;
  uint32_t count;
  ohm1_slave_entry slaves[];
} ohm1_slave;
