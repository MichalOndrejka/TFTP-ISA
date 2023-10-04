CC = gcc
CFLAGS = -lpcap

EXECUTABLE1 = tftp-client
EXECUTABLE2 = tftp-server
OBJS1 = tftp-client.c
OBJS2 = tftp-server.c

all: $(EXECUTABLE1) $(EXECUTABLE2)

$(EXECUTABLE1): $(OBJS1)
	$(CC) $^ -o $@ $(CFLAGS)

$(EXECUTABLE2): $(OBJS2)
	$(CC) $^ -o $@ $(CFLAGS)

clean:
	rm bin/$(EXECUTABLE1)
	rm bin/$(EXECUTABLE2)