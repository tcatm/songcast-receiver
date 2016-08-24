#pragma once

#include <stddef.h>

struct missing_frames {
  size_t length;
  size_t size;
  unsigned int *seqnums;
};
