#include "kalman.h"

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
