CC = gcc
CFLAGS = -Wall -Wextra -g

SHELL_OBJS = myshell.o parser.o executor.o
CLIENT_OBJS = client.o protocol.o
SERVER_OBJS = server.o remote_executor.o parser.o executor.o protocol.o

# link the object files into the final executable
all: myshell client server

myshell: $(SHELL_OBJS)
	$(CC) $(CFLAGS) -o myshell $(SHELL_OBJS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJS)

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJS)

#each object file is rebuilt only when unsaved changes are made
myshell.o: myshell.c parser.h executor.h
	$(CC) $(CFLAGS) -c myshell.c

parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c

executor.o: executor.c executor.h parser.h
	$(CC) $(CFLAGS) -c executor.c

client.o: client.c protocol.h
	$(CC) $(CFLAGS) -c client.c

protocol.o: protocol.c protocol.h
	$(CC) $(CFLAGS) -c protocol.c

server.o: server.c parser.h protocol.h remote_executor.h
	$(CC) $(CFLAGS) -c server.c

remote_executor.o: remote_executor.c remote_executor.h parser.h executor.h protocol.h
	$(CC) $(CFLAGS) -c remote_executor.c

clean:
	rm -f myshell.o parser.o executor.o client.o protocol.o
	rm -f server.o remote_executor.o
	rm -f myshell client server

.PHONY: all clean

