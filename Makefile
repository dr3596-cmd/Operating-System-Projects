CC = gcc
CFLAGS = -Wall -Wextra -g

# Phase 1 local shell
OBJS = myshell.o parser.o executor.o

#Phase 2;  the server uses phase 1 parser and executor, the clinet needs only the protocol side
SERVER_OBJS = server.o parser.o executor.o protocol.object
CLIENT_OBJS = client.o protocol.object

HEADERS = parser.h executor.h protocol.h 

# "make" build everything together
all: myshell server client

# link the object files into the final executable

myshell: $(OBJS)
	$(CC) $(CFLAGS) -o myshell $(OBJS)

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJS)

#each object file is rebuilt only when unsaved changes are made
myshell.o: myshell.c parser.c parser.h executor.h
	$(CC) $(CFLAGS) -c myshell.c

parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c

executor.o: executor.c executor.h parser.h
	$(CC) $(CFLAGS) -c executor.c

server.o: server.c parser.h executor.h protocol.h 
	$(CC) $(CFLAGS) -c server.c 

client.o: client.c protocol.h 
	$(CC) $(CFLAGS) -c client.c 


server.o: protocol.c protocol.h 
	$(CC) $(CFLAGS) -c protocol.c 


clean:
	rm -f $(OBJS) server.o client.o protocol.o myshell server client 

.PHONY: all clean