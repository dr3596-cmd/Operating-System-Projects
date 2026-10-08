CC = gcc
CFLAGS = -Wall -Wextra -g

OBJS = myshell.o parser.o executor.o

# link the object files into the final executable

myshell: $(OBJS)
	$(CC) $(CFLAGS) -o myshell $(OBJS)

#each object file is rebuilt only when unsaved changes are made
myshell.o: myshell.c parser.c parser.h executor.h
	$(CC) $(CFLAGS) -c myshell.c

parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c

executor.o: executor.c executor.h parser.h
	$(CC) $(CFLAGS) -c parser.c

clean:
	rm -f $(OBJS) myshell