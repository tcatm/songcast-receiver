#include <time.h>

int timespec_cmp(struct timespec *a, struct timespec *b) {
  if      (a->tv_sec  < b->tv_sec )  return -1;
  else if (a->tv_sec  > b->tv_sec )  return +1;
  else if (a->tv_nsec < b->tv_nsec)  return -1;
  else if (a->tv_nsec > b->tv_nsec)  return +1;

  return 0 ;
}

double timespec_sub(struct timespec *a, struct timespec *b) {
  return (double)(a->tv_sec - b->tv_sec) + (a->tv_nsec - b->tv_nsec) / 1e9;
}
