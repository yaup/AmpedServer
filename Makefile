CC = gcc
CFLAGS = -Wall -g -lpthread

#define common dependencies
OBJS= server.o socket.o worker.o
HEADERS= server.h socket.h worker.h server_struct_priv.h

# compile everything
all: 550server FORCE

550server: $(OBJS) 
	$(CC) $(CFLAGS) -o 550server $(OBJS)

server.o: server.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o server.o

socket.o: socket.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o socket.o

worker.o: worker.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o worker.o

clean: FORCE
	/bin/rm -f *.o *~ 550server

FORCE:
