#pragma once

struct uri {
  char *scheme;
  char *host;
  int port;
  char *path;
};

struct uri *parse_uri(char *uri_string);
void free_uri(struct uri *uri);
