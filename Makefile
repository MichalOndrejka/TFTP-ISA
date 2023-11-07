CC = gcc
CFLAGS = -I include

EXECUTABLE1 = bin/tftp-client
EXECUTABLE2 = bin/tftp-server
OBJS1 = src/tftp-client.c
OBJS2 = src/tftp-server.c

all: $(EXECUTABLE1) $(EXECUTABLE2)

$(EXECUTABLE1): $(OBJS1)
	$(CC) $^ -o $@ $(CFLAGS)

$(EXECUTABLE2): $(OBJS2)
	$(CC) $^ -o $@ $(CFLAGS)

clean:
	rm $(EXECUTABLE1)
	rm $(EXECUTABLE2)