#include <error.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <uriparser/Uri.h>
#include <time.h>
#include <assert.h>

#include "timespec.h"
#include "ohz_v1.h"
#include "ohm_v1.h"
#include "player.h"

void open_uri(char *uri_string);

char *resolve_preset(int preset) {
  int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (fd <= 0)
    error(1, errno, "Could not open socket");

  struct sockaddr_in src = {
    .sin_family = AF_INET,
    .sin_port = htons(51972),
    .sin_addr.s_addr = htonl(INADDR_ANY),
  };

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
    error(1, 0, "setsockopt(SO_REUSEADDR) failed");

  if (bind(fd, (struct sockaddr *) &src, sizeof(src)) < 0)
    error(1, 0, "Could not bind socket");

  struct ip_mreq mreq = {
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250")
  };

  if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    error(1, 0, "Could not join multicast group");

  struct sockaddr_in dst = {
    .sin_family = AF_INET,
    .sin_port = htons(51972),
    .sin_addr.s_addr = inet_addr("239.255.255.250")
  };

  ohz1_preset_query query = {
    .hdr = {
      .signature = "Ohz ",
      .version = 1,
      .type = OHZ1_PRESET_QUERY,
      .length = htons(sizeof(ohz1_preset_query))
    },
    .preset = htonl(preset)
  };

  xmlDocPtr metadata = NULL;

  while (1) {
    if (sendto(fd, &query, sizeof(query), 0, (const struct sockaddr*) &dst, sizeof(dst)) < 0)
      error(1, errno, "sendto failed");

    uint8_t buf[4096];

    struct timeval timeout = {
      .tv_usec = 100000
    };

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
      error(1, errno, "setsockopt(SO_RCVTIMEO) failed");

    while (1) {
      ssize_t n = recv(fd, buf, sizeof(buf), 0);

      if (n < 0) {
        if (errno == EAGAIN)
          break;

        error(1, errno, "recv");
      }

      if (n < sizeof(ohz1_preset_info))
        continue;

      ohz1_preset_info *info = (void *)buf;

      if (strncmp(info->hdr.signature, "Ohz ", 4) != 0)
        continue;

      if (info->hdr.version != 1)
        continue;

      if (info->hdr.type != OHZ1_PRESET_INFO)
        continue;

      if (htonl(info->preset) != preset)
        continue;

      metadata = xmlReadMemory(info->metadata, htonl(info->length), "noname.xml", NULL, 0);
      if (metadata == NULL)
        error(1, 0, "Could not parse metadata");

      break;
    }

    if (metadata != NULL)
      break;
  }

  xmlXPathContextPtr context;
  xmlXPathObjectPtr result;

  context = xmlXPathNewContext(metadata);
  result = xmlXPathEvalExpression("//*[local-name()='res']/text()", context);
  xmlXPathFreeContext(context);
  if (result == NULL || xmlXPathNodeSetIsEmpty(result->nodesetval))
    error(1, 0, "Could not find URI in metadata");

  xmlChar *uri = xmlXPathCastToString(result);
  char *s = strdup(uri);
  xmlFree(uri);
  xmlCleanupParser();

  return s;
}

char *UriTextRangeString(UriTextRangeA *textrange) {
  return strndup(textrange->first, textrange->afterLast - textrange->first);
}

struct uri {
  char *scheme;
  char *host;
  int port;
  char *path;
};

struct uri *parse_uri(char *uri_string) {
  UriParserStateA state;
  UriUriA uri;

  state.uri = &uri;

  if (uriParseUriA(&state, uri_string) != URI_SUCCESS) {
    uriFreeUriMembersA(&uri);
    error(1, 0, "Could not parse URI");
  }

  struct uri *s = calloc(1, sizeof(struct uri));

  s->scheme = UriTextRangeString(&uri.scheme);
  s->host = UriTextRangeString(&uri.hostText);
  s->port = atoi(UriTextRangeString(&uri.portText));

  if (&uri.pathHead->text != NULL)
    s->path = UriTextRangeString(&uri.pathHead->text);
  else
    s->path = strdup("");

  uriFreeUriMembersA(&uri);

  return s;
}

void free_uri(struct uri *uri) {
  free(uri->scheme);
  free(uri->host);
  free(uri->path);
  free(uri);
}

void resolve_ohz(struct uri *uri) {
  printf("Resolving OHZ\n");
  int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (fd <= 0)
    error(1, errno, "Could not open socket");

  struct sockaddr_in src = {
    .sin_family = AF_INET,
    .sin_port = htons(51972),
    .sin_addr.s_addr = htonl(INADDR_ANY),
  };

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
    error(1, 0, "setsockopt(SO_REUSEADDR) failed");

  if (bind(fd, (struct sockaddr *) &src, sizeof(src)) < 0)
    error(1, 0, "Could not bind socket");

  struct ip_mreq mreq = {
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250")
  };

  if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    error(1, 0, "Could not join multicast group");

  struct sockaddr_in dst = {
    .sin_family = AF_INET,
    .sin_port = htons(uri->port),
    .sin_addr.s_addr = inet_addr(uri->host)
  };

  ohz1_zone_query query = {
    .hdr = {
      .signature = "Ohz ",
      .version = 1,
      .type = OHZ1_ZONE_QUERY,
      .length = htons(sizeof(ohz1_preset_query))
    },
    .zone_length = htonl(strlen(uri->path))
  };

  size_t query_size = sizeof(ohz1_zone_query) + strlen(uri->path);
  uint8_t *query_with_id = malloc(query_size);

  struct iovec iov[] = {
    {
      .iov_base = &query,
      .iov_len = sizeof(query)
    },
    {
      .iov_base = uri->path,
      .iov_len = strlen(uri->path)
    }
  };

  struct msghdr message = {
    .msg_name = &dst,
    .msg_namelen = sizeof(dst),
    .msg_iov = iov,
    .msg_iovlen = 2
  };

  char *zone_uri = NULL;

  while (1) {
    if (sendmsg(fd, &message, 0) == -1)
    error(1, errno, "sendmsg failed");

    uint8_t buf[4096];

    struct timeval timeout = {
      .tv_usec = 100000
    };

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
      error(1, errno, "setsockopt(SO_RCVTIMEO) failed");

    while (1) {
      ssize_t n = recv(fd, buf, sizeof(buf), 0);

      if (n < 0) {
        if (errno == EAGAIN)
          break;

        error(1, errno, "recv");
      }

      if (n < sizeof(ohz1_zone_uri))
        continue;

      ohz1_zone_uri *info = (void *)buf;

      if (strncmp(info->hdr.signature, "Ohz ", 4) != 0)
        continue;

      if (info->hdr.version != 1)
        continue;

      if (info->hdr.type != OHZ1_ZONE_URI)
        continue;

      size_t zone_length = htonl(info->zone_length);
      size_t uri_length = htonl(info->uri_length);

      if (strlen(uri->path) != zone_length || strncmp(uri->path, info->data, zone_length) != 0)
        continue;

      zone_uri = strndup(info->data + zone_length, uri_length);

      break;
    }

    if (zone_uri != NULL)
      break;
  }

  open_uri(zone_uri);

  free(zone_uri);

  // de-dup socket code with presety_y_ foo
}

void ohm_send_event(int fd, const struct uri *uri, int event) {
  struct sockaddr_in dst = {
    .sin_family = AF_INET,
    .sin_port = htons(uri->port),
    .sin_addr.s_addr = inet_addr(uri->host)
  };

  ohm1_header message = {
    .signature = "Ohm ",
    .version = 1,
    .type = event,
    .length = htons(sizeof(ohm1_header))
  };

  if (sendto(fd, &message, sizeof(message), 0, (const struct sockaddr*) &dst, sizeof(dst)) < 0)
    error(1, errno, "Could not send message");
}

void ohm_send_resend_request(int fd, const struct uri *uri, const struct missing_frames *missing) {
  struct sockaddr_in dst = {
    .sin_family = AF_INET,
    .sin_port = htons(uri->port),
    .sin_addr.s_addr = inet_addr(uri->host)
  };

  size_t size = sizeof(ohm1_resend_request) + sizeof(uint32_t) * missing->count;
  ohm1_resend_request *msg = calloc(1, size);
  msg->hdr = (ohm1_header) {
    .signature = "Ohm ",
    .version = 1,
    .type = OHM1_RESEND_REQUEST,
    .length = htons(size)
  };

  msg->count = htonl(missing->count);

  for (int i = 0; i < missing->count; i++) {
    printf("Requesting %i\n", missing->seqnums[i]);
    msg->seqnums[i] = htonl(missing->seqnums[i]);
  }

  if (sendto(fd, msg, size, 0, (const struct sockaddr*) &dst, sizeof(dst)) < 0)
    error(1, errno, "Could not send message");

  free(msg);
}

void dump_track(ohm1_track *track) {
  printf("Track URI: %.*s\n", htonl(track->uri_length), track->data);
  printf("Track Metadata: %.*s\n", htonl(track->metadata_length), track->data + htonl(track->uri_length));
}

void dump_metatext(ohm1_metatext *meta) {
  printf("Metatext: %.*s\n", htonl(meta->length), meta->data);
}

size_t slave_count;
struct sockaddr_in *my_slaves;

void update_slaves(ohm1_slave *slave) {
  free(my_slaves);
  slave_count = htonl(slave->count);
  printf("Updating slaves: %zi\n", slave_count);

  my_slaves = calloc(slave_count, sizeof(struct sockaddr_in));

  for (size_t i = 0; i < slave_count; i++) {
    my_slaves[i] = (struct sockaddr_in) {
      .sin_family = AF_INET,
      .sin_port = slave->slaves[i].port,
      .sin_addr.s_addr = slave->slaves[i].addr
    };
  }
}

void play_uri(struct uri *uri) {
  bool unicast = strncmp(uri->scheme, "ohu", 3) == 0;

  int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (fd <= 0)
    error(1, errno, "Could not open socket");

  if (!unicast) {
    // Join multicast group
    struct sockaddr_in src = {
      .sin_family = AF_INET,
      .sin_port = htons(uri->port),
      .sin_addr.s_addr = inet_addr(uri->host)
    };

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
      error(1, 0, "setsockopt(SO_REUSEADDR) failed");

    if (bind(fd, (struct sockaddr *) &src, sizeof(src)) < 0)
      error(1, 0, "Could not bind socket");

    struct ip_mreq mreq = {
      mreq.imr_multiaddr.s_addr = inet_addr(uri->host)
    };

    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
      error(1, 0, "Could not join multicast group");
  }

  uint8_t buf[8192];

  struct timeval timeout = {
    .tv_usec = 100000
  };

  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    error(1, errno, "setsockopt(SO_RCVTIMEO) failed");

  if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, &(int){ 1 }, sizeof(int)) < 0)
    error(1, errno, "setsockopt(SO_TIMESTAMP) failed");

  ohm_send_event(fd, uri, OHM1_JOIN);
  ohm_send_event(fd, uri, OHM1_LISTEN);

  struct timespec last_listen;
  clock_gettime(CLOCK_MONOTONIC, &last_listen);

  player_init();

  struct missing_frames *missing;

  while (1) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    now.tv_nsec -= 750000000; // send listen every 750ms
    if (timespec_cmp(now, last_listen) > 0) {
      // TODO do not repeat join. Instead detect when we're not receving data anymore
      //ohm_send_event(fd, uri, OHM1_JOIN);
      ohm_send_event(fd, uri, OHM1_LISTEN);
      clock_gettime(CLOCK_MONOTONIC, &last_listen);
    }

    struct sockaddr_storage src_addr;

    char ctrl[CMSG_SPACE(sizeof(struct timeval))];
    struct cmsghdr *cmsg = (struct cmsghdr *)&ctrl;

    struct iovec iov[1] = {
      {
        .iov_base = &buf,
        .iov_len = sizeof(buf)
      }
    };

    struct msghdr msg = {
      .msg_name = &src_addr,
      .msg_namelen = sizeof(src_addr),
      .msg_iov = iov,
      .msg_iovlen = 1,
      .msg_control = ctrl,
      .msg_controllen = sizeof(ctrl)
    };

    // TODO determine whether to send listen here
    ssize_t n = recvmsg(fd, &msg, 0);

    struct timespec ts_recv;

    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMP &&
      cmsg->cmsg_len == CMSG_LEN(sizeof(struct timeval))) {
        struct timeval *tv_recv = CMSG_DATA(cmsg);
        ts_recv.tv_sec = tv_recv->tv_sec;
        ts_recv.tv_nsec = (long long int)tv_recv->tv_usec * 1000;

        // TODO convert to monotonic here?
      } else {
        // We always require the timestamp for now.
        assert(false);
      }

      if (n < 0) {
        if (errno == EAGAIN)
          continue;

        error(1, errno, "recv");
      }

      if (n < sizeof(ohm1_header))
        continue;

      ohm1_header *hdr = (void *)buf;

      if (strncmp(hdr->signature, "Ohm ", 4) != 0)
        continue;

      if (hdr->version != 1)
        continue;

      // Forwarding
      if (slave_count > 0)
        switch (hdr->type) {
          case OHM1_AUDIO:
          case OHM1_TRACK:
          case OHM1_METATEXT:
            for (size_t i = 0; i < slave_count; i++) {
              // Ignore any errors when sending to slaves.
              // There is nothing we could do to help.
              sendto(fd, &buf, n, 0, (const struct sockaddr*) &my_slaves[i], sizeof(struct sockaddr));
            }
            break;
          default:
            break;
        }

        switch (hdr->type) {
          case OHM1_LEAVE:
          case OHM1_JOIN:
            // ignore join and leave
            break;
          case OHM1_LISTEN:
            if (!unicast)
              clock_gettime(CLOCK_MONOTONIC, &last_listen);
            break;
          case OHM1_AUDIO:
            missing = handle_frame((void*)buf, &ts_recv);

            if (missing)
              ohm_send_resend_request(fd, uri, missing);

            free(missing);
            break;
          case OHM1_TRACK:
            dump_track((void *)buf);
            break;
          case OHM1_METATEXT:
            dump_metatext((void *)buf);
            break;
          case OHM1_SLAVE:
            update_slaves((void *)buf);
            break;
          case OHM1_RESEND_REQUEST:
            break;
          default:
            printf("Type %i not handled yet\n", hdr->type);
        }
    }
  }

  void open_uri(char *uri_string) {
    printf("Attemping to open %s\n", uri_string);

    struct uri *uri = parse_uri(uri_string);

    if (strcmp(uri->scheme, "ohz") == 0)
      resolve_ohz(uri);
    else if (strcmp(uri->scheme, "ohm") == 0 || strcmp(uri->scheme, "ohu") == 0)
      play_uri(uri);
    else
      error(1, 0, "unknown URI scheme \"%s\"", uri->scheme);

    free_uri(uri);
  }

  int main(int argc, char *argv[]) {
    LIBXML_TEST_VERSION

    int preset = 0;
    char *uri = NULL;

    int c;
    while ((c = getopt(argc, argv, "p:u:")) != -1)
    switch (c) {
      case 'p':
        preset = atoi(optarg);
        break;
      case 'u':
        uri = strdup(optarg);
        break;
    }

    if (uri != NULL && preset != 0)
      error(1, 0, "Can not specify both preset and URI!");


    if (preset != 0)
      uri = resolve_preset(preset);

    my_slaves = NULL;

    open_uri(uri);

    free(uri);
  }
