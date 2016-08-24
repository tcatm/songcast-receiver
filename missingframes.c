// iterate

#include <assert.h>

#include "missingframes.h"

void resize(struct missing_frames *d, size_t n) {
  d->size = n;
  d->seqnums = realloc(d->seqnums, d->size * sizeof(unsigned int));
  assert(d->seqnums != NULL);
}

void mf_init(struct missing_frames *d) {
  resize(d, 8);
}

void mf_add(struct missing_frames *d, unsigned int x) {
  // add x to set
  // update length
  // possibly doubling array size (actually * 1.5)
}

void mf_remove(struct missing_frames *d, unsigned int x) {
  // remove x if present
  // update length
  // possible halve array size
}

void mf_destroy(struct missing_frames *d) {
  free(d->seqnums);
}

void mf_discard_before(struct missing_frames *d, unsigned int x) {
  // remove all elements up to (but excluding) x
}
