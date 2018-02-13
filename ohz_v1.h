#pragma once

#include <stdint.h>

enum {
  OHZ1_ZONE_QUERY = 0,
  OHZ1_ZONE_URI,
  OHZ1_PRESET_QUERY,
  OHZ1_PRESET_INFO
};

typedef struct __attribute__((__packed__)) {
  int8_t signature[4];
  uint8_t version;
  uint8_t type;
  uint16_t length;
} ohz1_header;

typedef struct __attribute__((__packed__))  {
  ohz1_header hdr;
  uint32_t zone_length;
} ohz1_zone_query;

typedef struct __attribute__((__packed__))  {
  ohz1_header hdr;
  uint32_t zone_length;
  uint32_t uri_length;
  char data[];
} ohz1_zone_uri;

typedef struct __attribute__((__packed__)) {
  ohz1_header hdr;
  uint32_t preset;
} ohz1_preset_query;

typedef struct __attribute__((__packed__)) {
  ohz1_header hdr;
  uint32_t preset;
  uint32_t length;
  char metadata[];
} ohz1_preset_info;
