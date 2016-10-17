#define _POSIX_C_SOURCE 200112L
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
void setup_addrinfo_hint(char *hostname, char *port, struct addrinfo *hints, struct addrinfo **res) {
  int status;

  memset(hints, 0, sizeof(struct addrinfo));
  hints->ai_family = AF_UNSPEC;
  hints->ai_socktype = SOCK_STREAM;
  hints->ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(hostname, port, hints, res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
}

int get_socket(char *hostname, char *port) {
  // setup addrinfo
  struct addrinfo hints, *res;

  setup_addrinfo_hint(hostname, port, &hints, &res);

  int listen_fd = -1;
  for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
    listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

    if (listen_fd == -1) {
      perror("socket() failed");
      listen_fd = 0;
      continue;
    }

    // break from loop if bind successful
    if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }
    // bind failed. 
    close(listen_fd);
    listen_fd = -1;
  }

  freeaddrinfo(res);
  return listen_fd;
}

int get_listening_socket(char *hostname, char* port, int max_client_count) {
  int fd = get_socket(hostname, port);
  if (listen(fd, max_client_count) == -1) {
    perror("cannot listen to socket");
    close(fd);
    exit(1);
  }

  return fd;
}

int make_non_blocking_socket(int lfd) {
  int flags, res;
  flags = fcntl(lfd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl");
    return -1;
  }
  flags |= O_NONBLOCK;
  res = fcntl(lfd, F_SETFL, flags);
  if (res == -1) {
    perror("fcntl");
    return -1;
  }
  
  return 0;
}


int read_from_client(int client_fd, char **buf_ptr) {
  char *buf = (char *) malloc(MAX_PATH_CHARACTERS);
  int done = 0;
  while (done < MAX_PATH_CHARACTERS) {
    ssize_t count;
    count = read(client_fd, buf+done, sizeof(buf));
    printf("reading from client\n");
    done += count;
    if (count == -1) {
      if (errno != EAGAIN) {
        // can't read from a client
        perror("read error\n");
      }
      break;
    } else if (count == 0) {
      // close_client_connection(i);
      free(buf);
      break;
    }
  }
  *buf_ptr = buf;
  return done;
}
