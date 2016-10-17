#ifndef _SOCKET_H_
#define _SOCKET_H_

// 254 characters for a file name
#define MAX_PATH_CHARACTERS 254

int get_socket(char *hostname, char *port);
int get_listening_socket(char *hostname, char* port, int max_client_count);
// make the network socket non-blocking. Mainly accept() blocks.
// For the purpose of polling fds
int make_non_blocking_socket(int lfd);
// read from client socket
// return # of bytes read from client. Or -1 is return if a error occured
int read_from_client(int client_fd, char **buf_ptr);
#endif // _SOCKET_H_
