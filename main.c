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
#include "log.h"

#define OHM_NULL_URI "ohm://0.0.0.0:0"

/*
  Commands
    preset <number>
    uri <uri>
    stop
    quit
*/

struct handler {
  int fd;
  void (*func)(int fd, uint32_t events, void *userdata);
  void *userdata;
};

struct ReceiverData {
  int efd;
  int ohz_fd;
  int ohm_fd;
  unsigned int preset;
  char *zone_id;
  struct uri *uri;
  struct timespec last_preset_request, last_zone_request, last_playback_request, last_listen;

  bool unicast;
  int slave_count;
  struct sockaddr_in *my_slaves;
  struct handler ohm_handler;

  player_t player;
};

bool goto_uri(struct ReceiverData *receiver, const char *uri_string);
bool goto_preset(struct ReceiverData *receiver, unsigned int preset);
void receiver(const char *uri_string, unsigned int preset);
void handle_ohm(int fd, uint32_t events, void *userdata);
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
  result = xmlXPathEvalExpression((unsigned char*)"//*[local-name()='res']/text()", context);
  xmlXPathFreeContext(context);
  if (result == NULL || xmlXPathNodeSetIsEmpty(result->nodesetval)) {
    xmlXPathFreeObject(result);
    xmlFreeDoc(metadata);
    xmlCleanupParser();
    fprintf(stderr, "Could not find URI in metadata\n");
    return NULL;
  }

  xmlChar *uri = xmlXPathCastToString(result);
  char *s = strdup((char *)uri);
  xmlFree(uri);
  xmlXPathFreeObject(result);
  xmlFreeDoc(metadata);
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
	int s = epoll_ctl(efd, EPOLL_CTL_DEL, handler->fd, NULL);
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

  struct ip_mreq mreq = {{
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250")
  }};

  if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    error(1, 0, "Could not join multicast group");

  return fd;
}

void send_preset_query(int fd, unsigned int preset) {
  assert(preset != 0);

  log_printf("Resolving preset %d", preset);

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

void send_zone_query(int fd, const char *zone) {
  assert(zone != NULL);
  log_printf("Sending zone query: %s", zone);

  struct sockaddr_in dst = {
    .sin_family = AF_INET,
    .sin_port = htons(51972),
    .sin_addr.s_addr = inet_addr("239.255.255.250")
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

void handle_ohz(int fd, uint32_t events, void *userdata) {
  struct ReceiverData *receiver = userdata;
  // This function can change the state depending on whether
  // we are waiting for a preset or an URI.
  // Actually, it can only ever return an URI.
  uint8_t buf[4096];

  ssize_t n = recv(fd, buf, sizeof(buf), 0);

  if (n < 0) {
    if (errno == EAGAIN)
      return;

    error(1, errno, "recv");
  }

  if (n < sizeof(ohz1_zone_uri))
    return;

  ohz1_header *hdr = (void *)buf;

  if (strncmp((char *)hdr->signature, "Ohz ", 4) != 0)
    return;

  if (hdr->version != 1)
    return;

  char *uri = NULL;
  switch (hdr->type) {
    case OHZ1_ZONE_URI:
      if (receiver->zone_id != NULL)
        uri = handle_zone_uri((ohz1_zone_uri *)buf, receiver->zone_id);
      break;
    case OHZ1_PRESET_INFO:
      if (receiver->preset != 0)
        uri = handle_preset_info((ohz1_preset_info *)buf, receiver->preset);
      break;
    default:
      break;
  }

  if (uri != NULL)
    goto_uri(receiver, uri);

  free(uri);

  return;
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

    struct ip_mreq mreq = {{
      mreq.imr_multiaddr.s_addr = inet_addr(host)
    }};

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
  log_printf("Track URI: %.*s", htonl(track->uri_length), track->data);
  log_printf("Track Metadata: %.*s", htonl(track->metadata_length), track->data + htonl(track->uri_length));
}

void dump_metatext(ohm1_metatext *meta) {
  log_printf("Metatext: %.*s", htonl(meta->length), meta->data);
}

void update_slaves(struct ReceiverData *receiver, ohm1_slave *slave) {
  receiver->slave_count = htonl(slave->count);
  log_printf("Updating slaves: %d", receiver->slave_count);
  free(receiver->my_slaves);

  receiver->my_slaves = calloc(receiver->slave_count, sizeof(struct sockaddr_in));

  for (size_t i = 0; i < receiver->slave_count; i++) {
    receiver->my_slaves[i] = (struct sockaddr_in) {
      .sin_family = AF_INET,
      .sin_port = slave->slaves[i].port,
      .sin_addr.s_addr = slave->slaves[i].addr
    };

    char slave_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &receiver->my_slaves[i].sin_addr, slave_str, sizeof(slave_str));
    log_printf("Slave: %s", slave_str);
  }
}

void stop_playback(struct ReceiverData *receiver) {
  if (receiver->ohm_fd == 0)
    return;

  log_printf("Stopping playback.");

  if (receiver->unicast)
    ohm_send_event(receiver->ohm_fd, receiver->uri, OHM1_LEAVE);

  // This will remove the handler, too.
  close(receiver->ohm_fd);

  free(receiver->my_slaves);
  receiver->slave_count = 0;
  receiver->my_slaves = NULL;
  receiver->ohm_fd = 0;

  player_stop(&receiver->player);
}

void play_uri(struct ReceiverData *receiver) {
  log_printf("Playing %s://%s:%d/%s", receiver->uri->scheme, receiver->uri->host, receiver->uri->port, receiver->uri->path);

  assert(!is_ohm_null_uri(receiver->uri));

  receiver->unicast = strncmp(receiver->uri->scheme, "ohu", 3) == 0;
  receiver->slave_count = 0;
  receiver->ohm_fd = open_ohm_socket(receiver->uri->host, receiver->uri->port, receiver->unicast);

  receiver->ohm_handler = (struct handler) {
    .fd = receiver->ohm_fd,
    .func = handle_ohm,
    .userdata = receiver,
  };

  add_fd(receiver->efd, &receiver->ohm_handler, EPOLLIN);

  ohm_send_event(receiver->ohm_fd, receiver->uri, OHM1_JOIN);
  clock_gettime(CLOCK_MONOTONIC, &receiver->last_listen);
}

void handle_ohm(int fd, uint32_t events, void *userdata) {
  struct ReceiverData *receiver = userdata;
  struct sockaddr_storage src_addr;
  uint8_t buf[8192];

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
      return;

    return;
  }

  if (n < sizeof(ohm1_header))
    return;

  ohm1_header *hdr = (void *)buf;

  if (strncmp((char *)hdr->signature, "Ohm ", 4) != 0)
    return;

  if (hdr->version != 1)
    return;

  // Forwarding
  // TODO move forwarding to separate function
  if (receiver->slave_count > 0)
    switch (hdr->type) {
      case OHM1_AUDIO:
      case OHM1_TRACK:
      case OHM1_METATEXT:
        for (size_t i = 0; i < receiver->slave_count; i++) {
          // Ignore any errors when sending to slaves.
          // There is nothing we could do to help.
          sendto(receiver->ohm_fd, &buf, n, 0, (const struct sockaddr*) &receiver->my_slaves[i], sizeof(struct sockaddr));
        }
        break;
      default:
        break;
    }

  struct missing_frames *missing;

  switch (hdr->type) {
    case OHM1_LISTEN:
      if (!receiver->unicast)
        clock_gettime(CLOCK_MONOTONIC, &receiver->last_listen);
      break;
    case OHM1_AUDIO:
      missing = handle_frame(&receiver->player, (void*)buf, &ts_recv);

      if (missing)
        ohm_send_resend_request(receiver->ohm_fd, receiver->uri, missing);

      free(missing);
      break;
    case OHM1_TRACK:
      dump_track((void *)buf);
      break;
    case OHM1_METATEXT:
      dump_metatext((void *)buf);
      break;
    case OHM1_SLAVE:
      update_slaves(receiver, (void *)buf);
      break;
    case OHM1_RESEND_REQUEST:
    case OHM1_LEAVE:
    case OHM1_JOIN:
      // not used by receivers
      break;
    default:
      log_printf("Type %i not handled yet", hdr->type);
  }
}

void handle_stdin(int fd, uint32_t events, void *userdata) {
  char buf[256];
  struct ReceiverData *receiver = userdata;

  char *s = fgets(buf, sizeof(buf), stdin);
  if (s == NULL)
    error(1, errno, "fgets");

  char *saveptr = NULL, *cmd, *arg;

  cmd = strtok_r(s, " \t\r\n", &saveptr);
  arg = strtok_r(NULL, " \t\r\n", &saveptr);

  if (cmd == NULL)
    return;

  if (strcmp(cmd, "stop") == 0)
    goto_uri(receiver, OHM_NULL_URI);

  if (strcmp(cmd, "preset") == 0 && arg != NULL)
    goto_preset(receiver, atoi(arg));

  if (strcmp(cmd, "uri") == 0 && arg != NULL)
    goto_uri(receiver, arg);

  if (strcmp(cmd, "quit") == 0)
    exit(1);
}

bool goto_preset(struct ReceiverData *receiver, unsigned int preset) {
  if (preset == 0)
    return false;

  free(receiver->zone_id);
  receiver->preset = preset;
  receiver->zone_id = NULL;

  send_preset_query(receiver->ohz_fd, preset);
  clock_gettime(CLOCK_MONOTONIC, &receiver->last_preset_request);

  return true;
}

bool goto_uri(struct ReceiverData *receiver, const char *uri_string) {
  assert(uri_string != NULL);

  log_printf("goto_uri %s", uri_string);

  struct uri *uri = parse_uri(uri_string);

  if (strcmp(uri->scheme, "ohz") == 0) {
    log_printf("Got zone %s", uri->path);
    free(receiver->zone_id);
    receiver->preset = 0;
    receiver->zone_id = strdup(uri->path);
    send_zone_query(receiver->ohz_fd, uri->path);
    clock_gettime(CLOCK_MONOTONIC, &receiver->last_zone_request);
    // This will not stop playback.
    return true;
  } else if (strcmp(uri->scheme, "ohm") == 0 || strcmp(uri->scheme, "ohu") == 0) {
    receiver->preset = 0;

    if (receiver->uri == NULL || !uri_equal(uri, receiver->uri)) {
      free_uri(receiver->uri);
      stop_playback(receiver);
      receiver->uri = uri;

      if (!is_ohm_null_uri(uri))
        play_uri(receiver);
      else
        log_printf("Got null URI");
    } else
      free_uri(uri);

    return true;
  } else
    fprintf(stderr, "unknown URI scheme \"%s\"", uri->scheme);

  free_uri(uri);
  return false;
}

void receiver(const char *uri_string, unsigned int preset) {
  struct ReceiverData receiver = {
    .preset = 0,
    .zone_id = NULL,
    .uri = NULL,
    .my_slaves = NULL
  };

  player_init(&receiver.player);

  int maxevents = 64;
  struct epoll_event events[maxevents];

  receiver.efd = epoll_create1(0);
  if (receiver.efd == -1)
    error(1, errno, "epoll_create");

  receiver.ohz_fd = open_ohz_socket();

  struct handler stdin_handler = {
    .fd = STDIN_FILENO,
    .func = handle_stdin,
    .userdata = &receiver,
  };

  add_fd(receiver.efd, &stdin_handler, EPOLLIN);

  struct handler ohz_handler = {
    .fd = receiver.ohz_fd,
    .func = handle_ohz,
    .userdata = &receiver,
  };

  add_fd(receiver.efd, &ohz_handler, EPOLLIN);

  if (preset != 0)
    goto_preset(&receiver, preset);

  if (uri_string != NULL)
    goto_uri(&receiver, uri_string);

  while (1) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    bool bpreset = receiver.preset != 0;
    bool bzone = receiver.zone_id != NULL;
    bool buri = receiver.uri != NULL;

    if (bzone && !buri) {
      double diff = timespec_sub(&now, &receiver.last_zone_request);
      if (diff > 0.1) {
        send_zone_query(receiver.ohz_fd, receiver.zone_id);
        receiver.last_zone_request = now;
      }
    } else if (bpreset) {
      double diff = timespec_sub(&now, &receiver.last_preset_request);
      if (diff > 0.1) {
        send_preset_query(receiver.ohz_fd, receiver.preset);
        receiver.last_preset_request = now;
      }
    }

    if (buri && !is_ohm_null_uri(receiver.uri)) {
      if (timespec_sub(&now, &receiver.last_listen) > 1) {
        ohm_send_event(receiver.ohm_fd, receiver.uri, OHM1_LISTEN);
        clock_gettime(CLOCK_MONOTONIC, &receiver.last_listen);
      }
    }

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

  log_init();
  log_printf("===== START =====");

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

  receiver(uri, preset);
  free(uri);
}
