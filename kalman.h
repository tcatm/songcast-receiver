#pragma once

#include <stdbool.h>

#define vector(type,size) type __attribute__ ((vector_size(sizeof(type)*(size))))

typedef double mat2d __attribute__ ((vector_size(32)));

union vec4d {
  mat2d m;
  double d[4];
};

typedef struct {
  double est;
  double error;
  double rel;
  bool initialized;
} kalman_t;

void kalman_init(kalman_t *k);
double kalman_run(kalman_t *k, double measurement, double variance);

typedef struct {
  mat2d X, P, Q;
  double R;
} kalman2d_t;

void kalman2d_init(kalman2d_t *k, mat2d X, mat2d P, double R);
void kalman2d_run(kalman2d_t *k, double dt, double z);
double kalman2d_get_x(kalman2d_t *k);
double kalman2d_get_v(kalman2d_t *k);
double kalman2d_get_p(kalman2d_t *k);
