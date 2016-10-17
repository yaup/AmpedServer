#ifndef _WORKER_H_
#define _WORKER_H_

#include "server_struct_priv.h"
#include <stdbool.h>
#include <poll.h>
// initialize important data structure for a new thread
// in server
// return 0 : out of memory
// return -1: pthread create failure
// return 1: everything looks good
int initialize_worker(conn_info **curr_info_ptr,
                      conn_info **thread_info_ptr, 
                      int *pipes, 
                      int index, // empty slot index 
                      bool *active_fds, 
                      struct pollfd *pollfd_ptr,
                      char *filename_ptr);

// Thread handler function
void *thread_handler(void *args);

#endif //_WORKER_H_
