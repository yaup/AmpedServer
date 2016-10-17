#ifndef _SERVER_H_
#define _SERVER_H_

#include <stdbool.h>

// 16 concurrent clients
#define MAX_CLIENTS 16
// 16 * 2 + 1 total file descriptors
#define MAX_FDS 33
// 254 characters for a file name
#define MAX_PATH_CHARACTERS 254
// maximum file size
#define FILE_SIZE 2097152

static void clean_shutdown(int sigs);
static bool is_client(int index);
static int find_empty_slot();
static void close_worker_connection(int i);
static void close_client_connection(int i);

#endif // _SERVER_H_
