#pragma once

#include <stdbool.h>

typedef struct {
  double est;
  double error;
  double rel;
  bool initialized;
} kalman_t;

void kalman_init(kalman_t *k);
double kalman_run(kalman_t *k, double measurement, double variance);
