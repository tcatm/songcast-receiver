#include <error.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/epoll.h>
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
#include <fcntl.h>

#include "timespec.h"
#include "ohz_v1.h"
#include "ohm_v1.h"
#include "player.h"

#define OHM_NULL_URI "ohm://0.0.0.0:0"

/*
  States:
    IDLE
      In this state only the OHZ socket is connected.
      The player is inactive and no stream is played.
      The receiver is waiting for a command on stdin
      to either resolve a preset or play an URI.

      Enter
        -

      Leave
        -

    RESOLVING_PRESET
      In this state the receiver is trying to resolve
      a preset number to an URI. It will continuously
      send requests every 100ms until a response matching
      the preset number is received.

      When a response is received and the URI is an OHZ URI
      the state is changed to WATCHING_ZONE.

      If the response contains and OHM/OHU URI the state
      is changed to PLAYING_OHM.

      Enter
        The preset number is set when entering the state.
        The 100ms timer is started.

      Leave
        The 100ms timer is stopped.

    RESOLVING_ZONE
      In this state the receiver is resolving a zone.
      When a response is received, the state is changed to
      WATCHING_ZONE and the URI is played.

      The URI must be a OHM/OHU URI.

      If no response is received within 100ms, the request
      is sent again.

      Enter
        The zone id is set when entering the state.
        The 100ms timer is started.

      Leave
        The 100ms timer is stopped.

    WATCHING_ZONE
      In this state the receiver is watching a zone.
      If a new URI is received, the player URI is changed.

      Enter
        The zone id is set when entering the state.

      Leave
        The player is stopped.


  Commands
    preset <number>
    uri <uri>
    stop
*/

enum ReceiverState {INVALID, IDLE, RESOLVING_PRESET, RESOLVING_ZONE, WATCHING_ZONE};

struct ReceiverData {
  enum ReceiverState state;
  enum ReceiverState next_state;
  unsigned int preset;
  char *zone_id;
};

struct handler {
  void (*f)(void *userdata);
  void *userdata;
};

void receiver(const char *uri_string, unsigned int preset);
int open_ohz_socket(void);

void add_fd(int efd, int fd, uint32_t events) {
	struct epoll_event event = {};
	event.data.fd = fd;
	event.events = events;

  fcntl(fd, F_SETFL, O_NONBLOCK);

	int s = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
	if (s == -1)
		error(1, errno, "epoll_ctl");
}

char *parse_preset_metadata(char *data, size_t length) {
  xmlDocPtr metadata = xmlReadMemory(data, length, "noname.xml", NULL, 0);

  if (metadata == NULL) {
    fprintf(stderr, "Could not parse metadata\n");
    return NULL;
  }

  xmlXPathContextPtr context;
  xmlXPathObjectPtr result;

  context = xmlXPathNewContext(metadata);
  result = xmlXPathEvalExpression("//*[local-name()='res']/text()", context);
  xmlXPathFreeContext(context);
  if (result == NULL || xmlXPathNodeSetIsEmpty(result->nodesetval)) {
    xmlCleanupParser();
    fprintf(stderr, "Could not find URI in metadata\n");
    return NULL;
  }

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

bool is_ohm_null_uri(struct uri *uri) {
  if (strcmp(uri->host, "0.0.0.0") == 0 && uri->port == 0)
    return true;

  return false;
}

int open_ohz_socket(void) {
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

  return fd;
}

void send_preset_query(int fd, unsigned int preset) {
  assert(preset != 0);

  printf("Resolving preset %d\n", preset);

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
    .preset = htonl(preset),
  };

  struct iovec iov[] = {
    {
      .iov_base = &query,
      .iov_len = sizeof(query)
    }
  };

  struct msghdr message = {
    .msg_name = &dst,
    .msg_namelen = sizeof(dst),
    .msg_iov = iov,
    .msg_iovlen = 1
  };

  if (sendmsg(fd, &message, 0) == -1)
    error(1, errno, "sendmsg failed");
}

void send_zone_query(int fd, const char *host, unsigned int port, const char *zone) {
  struct sockaddr_in dst = {
    .sin_family = AF_INET,
    .sin_port = htons(port),
    .sin_addr.s_addr = inet_addr(host)
  };

  ohz1_zone_query query = {
    .hdr = {
      .signature = "Ohz ",
      .version = 1,
      .type = OHZ1_ZONE_QUERY,
      .length = htons(sizeof(ohz1_zone_query))
    },
    .zone_length = htonl(strlen(zone))
  };

  size_t query_size = sizeof(ohz1_zone_query) + strlen(zone);
  uint8_t *query_with_id = malloc(query_size);

  struct iovec iov[] = {
    {
      .iov_base = &query,
      .iov_len = sizeof(query)
    },
    {
      .iov_base = (char *)zone,
      .iov_len = strlen(zone)
    }
  };

  struct msghdr message = {
    .msg_name = &dst,
    .msg_namelen = sizeof(dst),
    .msg_iov = iov,
    .msg_iovlen = 2
  };

  if (sendmsg(fd, &message, 0) == -1)
    error(1, errno, "sendmsg failed");
}

char *handle_zone_uri(ohz1_zone_uri *info, const char *zone) {
  if (zone == NULL)
    return NULL;

  size_t zone_length = htonl(info->zone_length);
  size_t uri_length = htonl(info->uri_length);

  if (strlen(zone) == zone_length && strncmp(zone, info->data, zone_length) == 0)
    return strndup(info->data + zone_length, uri_length);

  return NULL;
}

char *handle_preset_info(ohz1_preset_info *info, unsigned int preset) {
  if (preset != 0 && htonl(info->preset) == preset)
    return parse_preset_metadata(info->metadata, htonl(info->length));

  return NULL;
}

char *handle_ohz(int fd, const char *zone, unsigned int preset) {
  // This function can change the state depending on whether
  // we are waiting for a preset or an URI.
  // Actually, it can only ever return an URI.
  uint8_t buf[4096];

  ssize_t n = recv(fd, buf, sizeof(buf), 0);

  if (n < 0) {
    if (errno == EAGAIN)
      return NULL;

    error(1, errno, "recv");
  }

  if (n < sizeof(ohz1_zone_uri))
    return NULL;

  ohz1_header *hdr = (void *)buf;

  if (strncmp(hdr->signature, "Ohz ", 4) != 0)
    return NULL;

  if (hdr->version != 1)
    return NULL;

  switch (hdr->type) {
    case OHZ1_ZONE_URI:
      return handle_zone_uri((ohz1_zone_uri *)buf, zone);
      break;
    case OHZ1_PRESET_INFO:
      return handle_preset_info((ohz1_preset_info *)buf, preset);
      break;
    default:
      break;
  }

  return NULL;
}

int open_ohm_socket(const char *host, unsigned int port, bool unicast) {
  int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (fd <= 0)
    error(1, errno, "Could not open socket");

  if (!unicast) {
    // Join multicast group
    struct sockaddr_in src = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr.s_addr = inet_addr(host)
    };

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
      error(1, 0, "setsockopt(SO_REUSEADDR) failed");

    if (bind(fd, (struct sockaddr *) &src, sizeof(src)) < 0)
      error(1, 0, "Could not bind socket");

    struct ip_mreq mreq = {
      mreq.imr_multiaddr.s_addr = inet_addr(host)
    };

    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
      error(1, 0, "Could not join multicast group");
  }

  if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, &(int){ 1 }, sizeof(int)) < 0)
    error(1, errno, "setsockopt(SO_TIMESTAMP) failed");

  return fd;
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

  for (int i = 0; i < missing->count; i++)
    msg->seqnums[i] = htonl(missing->seqnums[i]);

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

int update_slaves(struct sockaddr_in *my_slaves[], ohm1_slave *slave) {
  free(my_slaves);
  int slave_count = htonl(slave->count);
  printf("Updating slaves: %zi\n", slave_count);

  my_slaves = calloc(slave_count, sizeof(struct sockaddr_in));

  for (size_t i = 0; i < slave_count; i++) {
    *my_slaves[i] = (struct sockaddr_in) {
      .sin_family = AF_INET,
      .sin_port = slave->slaves[i].port,
      .sin_addr.s_addr = slave->slaves[i].addr
    };
  }

  return slave_count;
}

void play_uri(struct uri *uri) {
  printf("Playing %s://%s:%d/%p", uri->scheme, uri->host, uri->port, uri->path);

  // TODO check if URI is null URI and stop playback
  bool unicast = strncmp(uri->scheme, "ohu", 3) == 0;
  int slave_count = 0;
  struct sockaddr_in *my_slaves;

  int fd = open_ohm_socket(uri->host, uri->port, unicast);

  uint8_t buf[8192];

  struct timeval timeout = {
    .tv_usec = 100000
  };

  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    error(1, errno, "setsockopt(SO_RCVTIMEO) failed");

  struct timespec last_listen;

  ohm_send_event(fd, uri, OHM1_JOIN);
  ohm_send_event(fd, uri, OHM1_LISTEN);
  clock_gettime(CLOCK_MONOTONIC, &last_listen);

  struct missing_frames *missing;

  while (1) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    now.tv_nsec -= 750000000; // send listen every 750ms
    if (timespec_cmp(now, last_listen) > 0) {
      // TODO resend join if no audio for a few seconds
      //      can this be implemented using the ohz state machine?
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
      struct timeval *tv_recv = (struct timeval *)CMSG_DATA(cmsg);
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
        slave_count = update_slaves(&my_slaves, (void *)buf);
        break;
      case OHM1_RESEND_REQUEST:
      case OHM1_LEAVE:
      case OHM1_JOIN:
        // not used by receivers
        break;
      default:
        printf("Type %i not handled yet\n", hdr->type);
    }
  }
}

void handle_stdin(int fd) {
  char buf[256];
  char *s = fgets(buf, sizeof(buf), stdin);
  if (s == NULL)
    error(1, errno, "fgets");

  printf("got: %s\n", s);
}

bool goto_preset(struct ReceiverData *receiver, unsigned int preset) {
  if (preset == 0)
    return false;

  receiver->next_state = RESOLVING_PRESET;
  receiver->preset = preset;
}

void goto_uri(struct ReceiverData *receiver, char *uri_string) {
  printf("goto_uri %s\n", uri_string);

  struct uri *uri = parse_uri(uri_string);

  receiver->next_state = IDLE;

  if (strcmp(uri->scheme, "ohz") == 0) {
    printf("Got zone %s\n", uri->path);
    receiver->zone_id = uri->path;
    receiver->next_state = RESOLVING_ZONE;
  } else if (strcmp(uri->scheme, "ohm") == 0 || strcmp(uri->scheme, "ohu") == 0) {
    if (receiver->zone_id != NULL)
      receiver->next_state = WATCHING_ZONE;

    // TODO stop player, reset track and metadata
    // TODO change player URI
    if (is_ohm_null_uri(uri)) {
      printf("Got null URI\n");

    } else {
      //play_uri(uri);
    }
  } else
    fprintf(stderr, "unknown URI scheme \"%s\"", uri->scheme);

  free_uri(uri);
}

void receiver(const char *uri_string, unsigned int preset) {
  struct ReceiverData receiver = {
    .state = IDLE,
    .next_state = INVALID,
    .preset = 0,
    .zone_id = NULL,
  };

  int efd;
  int maxevents = 64;
  struct epoll_event events[maxevents];

  efd = epoll_create1(0);
  if (efd == -1)
    error(1, errno, "epoll_create");

  add_fd(efd, STDIN_FILENO, EPOLLIN);

  int ohz_fd = open_ohz_socket();
  add_fd(efd, ohz_fd, EPOLLIN);

  char *zone = "";

  if (preset != 0)
    goto_preset(&receiver, preset);

  if (uri_string != NULL)
    goto_uri(&receiver, uri_string);

  while (1) {
    // TODO handle any state work here
    // check if next_state is not INVALID, then we have a state change
    // exit existing state
    // enter new state
    // this needs to reset preset/zone?2
    // needs some kind of pointer to the current state...
    // for handling state specific fds
    // timer is really only about sending a packet. request or listen...
    // stuff pointer to some functions in event.data.ptr?
    // call handle from that struct?
    // stdin is handled outside of the state machine

    // There will be two timers
    //  - request retry timer
    //  - listen timer
    // Can we share the same timer?
    // states create their own timerfd

		int n = epoll_wait(efd, events, maxevents, 100);

    if (n < 0)
      error(1, errno, "epoll_wait");

    for(int i = 0; i < n; i++) {
      if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
        error(1, 0, "epoll error\n");
      } else if (events[i].data.fd == STDIN_FILENO) {
        handle_stdin(events[i].data.fd);
      } else if (events[i].data.fd == ohz_fd) {
        char *uri = handle_ohz(events[i].data.fd, zone, preset);
        if (uri != NULL) {
          goto_uri(&receiver, uri);
          free(uri_string);
        }
      }
    }
  }
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

  player_init();

  receiver(uri, preset);

  free(uri);
}
