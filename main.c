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
#include <time.h>
#include <assert.h>
#include <fcntl.h>

#include "timespec.h"
#include "ohz_v1.h"
#include "ohm_v1.h"
#include "player.h"
#include "uri.h"

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
    quit
*/

struct ReceiverData {
  int efd;
  int ohz_fd;
  unsigned int preset;
  char *zone_id;
  struct uri *uri;
};

struct handler {
  int fd;
  void (*func)(int fd, uint32_t events, void *userdata);
  void *userdata;
};

void receiver(const char *uri_string, unsigned int preset);
int open_ohz_socket(void);

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

void add_fd(int efd, struct handler *handler, uint32_t events) {
	struct epoll_event event = {};
	event.data.ptr = handler;
	event.events = events;

  fcntl(handler->fd, F_SETFL, O_NONBLOCK);

	int s = epoll_ctl(efd, EPOLL_CTL_ADD, handler->fd, &event);
	if (s == -1)
		error(1, errno, "epoll_ctl");
}

void del_fd(int efd, struct handler *handler) {
	int s = epoll_ctl(efd, EPOLL_CTL_ADD, handler->fd, NULL);
	if (s == -1)
		error(1, errno, "epoll_ctl");
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

void handle_stdin(int fd, uint32_t events, void *userdata) {
  char buf[256];
  char *s = fgets(buf, sizeof(buf), stdin);
  if (s == NULL)
    error(1, errno, "fgets");

  printf("got: %s\n", s);

  // Commands need to reset preset and zone_id, set URI to OHM_NULL_URI
}

bool goto_preset(struct ReceiverData *receiver, unsigned int preset) {
  if (preset == 0)
    return false;

  free_uri(receiver->uri);
  free(receiver->zone_id);
  receiver->preset = preset;
}

bool goto_uri(struct ReceiverData *receiver, char *uri_string) {
  printf("goto_uri %s\n", uri_string);

  struct uri *uri = parse_uri(uri_string);

  if (strcmp(uri->scheme, "ohz") == 0) {
    printf("Got zone %s\n", uri->path);
    free(receiver->zone_id);
    receiver->preset = 0;
    receiver->zone_id = strdup(uri->path);
    // TODO trigger zone query here using zone_id
    // This will not stop playback.
    return true;
  } else if (strcmp(uri->scheme, "ohm") == 0 || strcmp(uri->scheme, "ohu") == 0) {
    free_uri(receiver->uri);
    receiver->preset = 0;

    // TODO check if uri differs from current uri
    // TODO stop player, reset track and metadata
    // TODO change player URI
    if (is_ohm_null_uri(uri)) {
      // TODO Stop playback.
      printf("Got null URI\n");
    } else if (!uri_equal(uri, receiver->uri)) {
      //stop_playback()
      //play_uri(uri);
    }

    return true;
  } else
    fprintf(stderr, "unknown URI scheme \"%s\"", uri->scheme);

  free_uri(uri);
  return false;
}

// Wenn eine zone id gesetzt ist, wird auf Änderungen in dieser Zone ID gehorcht.
// Wenn ein preset gesetzt ist, wird auf eine Antwort für das Preset gewartet.
// Wenn ein preset gesetzt ist und noch keine Antwort kam, wird noch 100ms nochmal gefragt.
// Wenn eine zone id gesetzt ist und noch keine antwort kam, wird nach 100ms nochmal gefragt.

// Resolve preset
// - add OHZ handler
// - add retry timer
// - send request
// - when URI received
// - remove try timer
// - remove OHZ handler


void receiver(const char *uri_string, unsigned int preset) {
  struct ReceiverData receiver = {
    .preset = 0,
    .zone_id = NULL,
    .uri = NULL,
  };

  int maxevents = 64;
  struct epoll_event events[maxevents];

  receiver.efd = epoll_create1(0);
  if (receiver.efd == -1)
    error(1, errno, "epoll_create");

  struct handler stdin_handler = {
    .fd = STDIN_FILENO,
    .func = handle_stdin,
    .userdata = &receiver,
  };

  add_fd(receiver.efd, &stdin_handler, EPOLLIN);

  receiver.ohz_fd = open_ohz_socket();

  if (preset != 0)
    goto_preset(&receiver, preset);

  if (uri_string != NULL)
    goto_uri(&receiver, uri_string);

  while (1) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // 2 timers
    // - 100ms retry timer
    // - calculate delay
    // - use epoll_wait timeout
    // - check in start of while loop
    // preset, zone_id, uri_string
    //      0     NULL        NULL no action
    //      0     NULL         set no action (uri should be playing)
    //      0      set        NULL resolve uri after 100ms timeout, watch zone
    //      0      set         set watch zone
    //      1     NULL        NULL resolve preset after 100ms
    //      1     NULL         set resolve preset after 100ms
    //x      1      set        NULL invalid state, no action
    //x      1      set         set invalid state, no action

    // Invalid state
    assert(!(receiver.preset != 0 && receiver.zone_id != NULL));

		int n = epoll_wait(receiver.efd, events, maxevents, 100);

    if (n < 0)
      error(1, errno, "epoll_wait");

    for(int i = 0; i < n; i++) {
      if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP))
        error(1, 0, "epoll error\n");

      struct handler *handler = events[i].data.ptr;
      handler->func(handler->fd, events[i].events, handler->userdata);
    }
    /*

      } else if (events[i].data.fd == STDIN_FILENO) {
        handle_stdin(events[i].data.fd);
      } else if (events[i].data.fd == ohz_fd) {
        char *uri = handle_ohz(events[i].data.fd, zone, preset);
        if (uri != NULL) {
          goto_uri(&receiver, uri);
          free(uri_string);
        }
      }
      */
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
