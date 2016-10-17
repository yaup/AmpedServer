#include "server.h"
#include "socket.h"
#include "worker.h"
#include "server_struct_priv.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>

struct pollfd fds[MAX_FDS];
bool active_fds[MAX_FDS];
conn_info *fds_info[MAX_FDS];

int main(int argc, char **argv) {
  int res;
  char *hostname, *port;
  memset(active_fds, '\0', (MAX_FDS) * sizeof(int));

  // check for arguments
  if (argc < 3) {
    printf("missing arguments. Usage: 550server address port\n");
    exit(1);
  }

  hostname = argv[1];
  port = argv[2];
  
  // Setup signal handlers.
  struct sigaction act;
  memset(&act, '\0', sizeof(act)); 
  act.sa_handler = &clean_shutdown;
  

  if (sigaction(SIGTERM, &act, NULL) < 0) {
    perror("sigaction SIGTERM");
    return 1;
  }

  if (sigaction(SIGINT, &act, NULL) < 0) {
    perror("sigaction SIGINT");
    return 1;
  }

  // ignore SIGPIPE from kernel
  struct sigaction ignore;
  memset(&ignore, '\0', sizeof(act));
  ignore.sa_handler = SIG_IGN;
  ignore.sa_flags = 0;
  if (sigaction(SIGPIPE, &ignore, 0) == -1) {
    perror("sigaction SIGPIPE");
    return 1;
  }
  
  // setup listening socket
  int listening_fd = get_listening_socket(hostname, port, MAX_CLIENTS);
  if ((res = make_non_blocking_socket(listening_fd))) {
    return res;
  }

  // set up listening socket for poll
  fds[0].fd = listening_fd;
  fds[0].events = POLLIN;
  active_fds[0] = true;

  // infinite loop polling for event notification
  while(1) {
    // wait for event notification
    res = poll(fds, MAX_FDS, -1);
    if (res == -1) {
      perror("poll failure");
      clean_shutdown(0);
    }

    // try accepting a connection
    if (fds[0].revents == POLLIN) {
      printf("looping through fds[0] active\n");
      struct sockaddr_storage caddr;
      socklen_t caddr_len = sizeof(caddr);
      int client_fd = accept(listening_fd, (struct sockaddr *) &caddr, &caddr_len);

      int index = find_empty_slot();
      if (index == -1) {
        // max connections already
        close(client_fd);
      } else {
        int nb_res;
        printf("adding connection at slot %d\n", index);
        if ((nb_res = make_non_blocking_socket(client_fd))) {
          perror("failed to make client socket non blocking");
          close(client_fd);
        }

        conn_info *new_connection_info = (conn_info *) malloc(sizeof(conn_info));
        memset(new_connection_info, '\0', sizeof(conn_info));
        new_connection_info->client_index = index;
        new_connection_info->client_fd = client_fd;
        
        fds[index].fd = client_fd;
        fds[index].events = POLLIN;
        fds_info[index] = new_connection_info;
        active_fds[index] = true;
      }
    }

    // loop through all remaining fds
    for(int i = 1; i < MAX_FDS; i++) {

      if (!active_fds[i]) {
        continue;
      }
      printf("looping through fds[%d] active\n", i);

      if (active_fds[i]
          && fds[i].revents == POLLIN) {
        // It is active and event notification
        printf("Event reported from fd %d at slot %d\n", fds[i].fd, i);
 
        if (is_client(i)) {
          // read filepath
          printf("It's a client!\n");
          
          char *buf = NULL;
          int cnt = read_from_client(fds[i].fd, &buf); 
          
          if (cnt == 0 || cnt == -1) {
            if (cnt == 0) {
              close_client_connection(i);
            } else if (cnt == -1) {
              close(fds[i].fd);
              active_fds[i] = false; 
            }
            continue; 
          } 
          
          // validate the path
          buf[cnt] = '\0';
          if (access(buf, F_OK) == -1) {
            printf("invalid file path: %s\n", buf);
            free(buf);
            close_client_connection(i);
            continue;
          }

          // setup info for new thread
          int pipes[2];
          if (pipe(pipes) == -1) {
            perror("pipe error\n");
            free(buf);
            close_client_connection(i);
            continue;
          }
          make_non_blocking_socket(pipes[0]);
          // conn_info *curr_info = fds_info[i];

          // setup args
          int index = find_empty_slot();
          
          res = initialize_worker(&(fds_info[i]), 
                                  &(fds_info[index]), 
                                  pipes, 
                                  index, 
                                  &active_fds[index],
                                  &(fds[index]),
                                  buf);
          if (res != 1) {
            free(buf);
            close_client_connection(i);
            continue;
          }           
        } else { //it's worker
          printf("signal from worker\n");
          conn_info *curr_info = fds_info[i];

          // read it from the worker, which should be a 1 byte response
          char thread_res[1];
          ssize_t numread = 0;
          while(numread < 1) {
            numread = read(curr_info->worker_fd, &thread_res, 1);
            // only deal with -1 'cause it's either read nothing or read 1
            // pipe cannot send signal to read with 0 bits to read
            if (numread == -1) {
              if ((errno == EAGAIN) || (errno == EINTR)) {
                continue;
              }
            }
          }

          // check signal from thread
          if (*thread_res) { // should be '\1' or '\0'
            // join the thread
            printf("joining thread\n");
            pthread_join(curr_info->worker_thread, NULL);

            // send response back to client
            write(curr_info->client_fd,
                  curr_info->params->file_content,
                  curr_info->params->file_length);

            // close connection
            close_worker_connection(i);
            close_client_connection(curr_info->client_index);
            continue;
          } else {
            // close connection
            perror("read failed in thread. closing connections\n");
            close_worker_connection(i);
            close_client_connection(curr_info->client_index);
            continue;
          }
        }
      } 
    }
  } 
  return 0;
}

static bool is_client(int index) {
  return (fds_info[index]->client_index == index);
}

static int find_empty_slot() {
  for(int i = 0; i < MAX_FDS; i++) {
    if (!active_fds[i]) {
      return i;
    }
  }
  return -1;
}

static void close_worker_connection(int worker_index) {
  int worker_fd = fds[worker_index].fd;
  conn_info *ci = fds_info[worker_index];
  printf("closing worker connection fd %d at slot %d...\n", worker_fd, worker_index);

  close(worker_fd);
  memset(fds + worker_index, '\0', sizeof(struct pollfd));
  active_fds[worker_index] = false;
  ci->worker_index = 0;
  ci->worker_fd = 0;
  if (ci->worker_thread){
    pthread_join(ci->worker_thread, NULL);
    ci->worker_thread = (pthread_t) 0;
  }
  if (ci->params) {
    free(ci->params->file_path);
    if (ci->params->file_content) {
      free(ci->params->file_content);
    }
    free(ci->params);
    ci->params = NULL;
  }

  fds_info[worker_index] = NULL;
}

static void close_client_connection(int client_index) {
  int client_fd = fds[client_index].fd;
  printf("closing client connection fd %d at slot %d...\n", client_fd, client_index);

  if (fds_info [client_index]) {
    free(fds_info[client_index]); // free the conn_info
  }
  
  close(client_fd);
  memset(fds + client_index, '\0', sizeof(struct pollfd));
  active_fds[client_index] = false;
}

static void clean_shutdown(int sigs) {
  // closing all open connections
  // close workers first because it resets fields in conn_info
  for (int i = 1; i < MAX_FDS; i++) {
    if (active_fds[i] && !is_client(i)) {
      close_worker_connection(i);
    }
  }
  for (int i = 1; i < MAX_FDS; i++) {
    if (active_fds[i] && is_client(i)) {
      close_client_connection(i);
    }
  }
  // close listening socket
  close(fds[0].fd);

  printf("clean_shutdown\n");
  exit(0);
}
