#pragma once

#include <stdbool.h>

struct uri {
  char *scheme;
  char *host;
  int port;
  char *path;
};

struct uri *parse_uri(const char *uri_string);
void free_uri(struct uri *uri);
bool uri_equal(const struct uri *a, const struct uri *b);
