#ifndef _SERVER_STRUCT_PRIV_H_
#define _SERVER_STRUCT_PRIV_H_
#include <pthread.h>
// documentation info for worker
typedef struct worker {
  int out_fd;
  char *file_path;
  char *file_content;
  int file_length;
} thread_args;

// struct to match fds
typedef struct conn_info {
  int client_index;
  int client_fd;
  int worker_index;
  int worker_fd;
  pthread_t worker_thread;
  thread_args *params;
} conn_info;

#endif // _SERVER_STRUCT_PRIV_H_
