#include "worker.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

void *thread_handler(void *args) {
  printf("worker: starting new worker thread\n");
  thread_args *worker_info = (thread_args*) args;
  struct stat filestat;
  char *buf;
  char success_sig = '\1';
  char failure_sig = '\0';
  if (stat(worker_info->file_path, &filestat) == -1) {
    // write back to main thread
    write(worker_info->out_fd, &failure_sig, 1);
    close(worker_info->out_fd);
    return NULL;
  }
  
  if (!S_ISREG(filestat.st_mode)) {
    // case : it isn't a regular file
    write(worker_info->out_fd, &failure_sig, 1); 
    close(worker_info->out_fd);
    return NULL;
  }
  int fd = open(worker_info->file_path, O_RDONLY);
  if (fd == -1) {
    // write back to main thread
    perror("open failed");
    write(worker_info->out_fd, &failure_sig, 1);
    close(worker_info->out_fd);
    return NULL;
  }
  
  buf = (char*) malloc(filestat.st_size + 1);
  if (buf == NULL) {
    close(fd);
    perror("out of memory");
    write(worker_info->out_fd, &failure_sig, 1);
    close(worker_info->out_fd);
    return NULL;
  } 
  // read from a file and put the content in memory
  size_t left_to_read = filestat.st_size;
  ssize_t numread = 0;
  while(left_to_read > 0) {
    numread = read(fd, buf + (filestat.st_size - left_to_read),
                  left_to_read);
    if (numread > 0) {
      left_to_read -= numread;
    } else if (numread == 0) {
      break;
    } else if (numread == -1) {
      if ( (errno == EAGAIN) || (errno == EINTR)) {
        continue;
      }
      close(fd);
      free(buf);
      // write back to thread
      write(worker_info->out_fd, &failure_sig, 1);
      close(worker_info->out_fd);
      return NULL;
    }
  }

  worker_info->file_length = filestat.st_size - left_to_read;
  buf[worker_info->file_length] = '\0';
  worker_info->file_content = buf;
  close(fd);
  // write back to thread
  printf("worker: writing back to main thread via pipe %d\n", worker_info->out_fd);
  write(worker_info->out_fd, &success_sig, 1);
  // close(worker_info->out_fd);
  return NULL;
}

int initialize_worker(conn_info **client_info_ptr, 
                      conn_info **thread_info_ptr,
                      int *pipes, 
                      int index, // empty slot index
                      bool *active_fds,
                      struct pollfd *pollfd_ptr,
                      char *filename_ptr) {

  thread_args *args = (thread_args *) malloc(sizeof(thread_args));
  if (args == NULL) {
    perror("out of memory");
    return 0;
  }

  // setup args
  args->out_fd = pipes[1];
  args->file_path = filename_ptr;
  args->file_content = NULL;
  args->file_length = 0;
  
  conn_info *curr_info = *client_info_ptr;
  curr_info->worker_fd = pipes[0];
  curr_info->worker_index = index;
  curr_info->worker_thread = (pthread_t) 0;
  curr_info->params = args;

  //add our end of pipe into event poll
  // printf("adding new thread fd %d to poll at slot %d\n", pipes[0], index);
  pollfd_ptr->fd = pipes[0];
  pollfd_ptr->events = POLLIN;
  *thread_info_ptr = curr_info;
  *active_fds = true;

  // create new thread
  int res = pthread_create(&(curr_info->worker_thread), 
                           NULL, 
                           thread_handler, 
                           (void *) args);
  
  if (res != 0) {
    perror("pthread_create");
    return -1;
  }
  
  return 1;
}
