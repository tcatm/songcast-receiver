#include <stdio.h>

#include "kalman.h"

static mat2d mat2d_transpose(mat2d m) {
  return __builtin_shuffle(m, (vector(long long, 4)){0, 2, 1, 3});
}

static mat2d mat2d_mul(mat2d a, mat2d b) {
  mat2d a0 = __builtin_shuffle(a, (vector(long long, 4)){0, 0, 2, 2});
  mat2d b0 = __builtin_shuffle(b, (vector(long long, 4)){0, 1, 0, 1});
  mat2d a1 = __builtin_shuffle(a, (vector(long long, 4)){1, 1, 3, 3});
  mat2d b1 = __builtin_shuffle(b, (vector(long long, 4)){2, 3, 2, 3});

  return a0 * b0 + a1 * b1;
}

void print_mat2d(mat2d m) {
  union vec4d *v = (union vec4d*)&m;
  printf("⎡%f\t%f⎤\n⎣%f\t%f⎦\n", v->d[0], v->d[1], v->d[2], v->d[3]);
}

void kalman2d_init(kalman2d_t *k, mat2d X, mat2d P, double R) {
  k->X = X;
  k->P = P;
  k->R = R;
  k->Q = (mat2d){0, 0, 0, 0};
}

void kalman2d_run(kalman2d_t *k, double dt, double z) {
  mat2d H = {1, 0, 0, 0};
  mat2d A = {1, dt, 0, 1};
  mat2d I = {1, 0, 0, 1};

  mat2d X = mat2d_mul(A, k->X);

  mat2d P = mat2d_mul(mat2d_mul(A, k->P), mat2d_transpose(A)) + k->Q;

  double s = ((union vec4d)P).d[0] + k->R;
  mat2d K = 1.0 / s * mat2d_mul(P, mat2d_transpose(H));

  double y = ((union vec4d)X).d[0];
  k->X = (X + K * z) - K * y;
  k->P = mat2d_mul((I - mat2d_mul(K, H)), P);
}

double kalman2d_get_x(kalman2d_t *k) {
  union vec4d *v = (union vec4d*)&k->X;
  return v->d[0];
}

double kalman2d_get_v(kalman2d_t *k) {
  union vec4d *v = (union vec4d*)&k->X;
  return v->d[2];
}

double kalman2d_get_p(kalman2d_t *k) {
  union vec4d *v = (union vec4d*)&k->P;
  return v->d[2];
}

void kalman_init(kalman_t *k) {
  *k = (kalman_t){
    .initialized = false
  };
}

double kalman_run(kalman_t *k, double x, double x_error) {
  double est, error;

  if (k->initialized) {
    double gain = k->error / (k->error + x_error);
    est = k->est + gain * (x - k->est);
    error = (1 - gain) * k->error;
  } else {
    est = x;
    error = x_error;
    k->initialized = true;
  }

  k->est = est;
  k->error = error;
  k->rel = error / est;

  return est;
}
