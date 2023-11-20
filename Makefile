CC = gcc

EXECUTABLE1 = tftp-client
EXECUTABLE2 = tftp-server
OBJS1 = src/tftp-client.c
OBJS2 = src/tftp-server.c

all: $(EXECUTABLE1) $(EXECUTABLE2)

$(EXECUTABLE1): $(OBJS1)
	$(CC) $^ -o $@

$(EXECUTABLE2): $(OBJS2)
	$(CC) $^ -o $@

clean:
	rm $(EXECUTABLE1)
	rm $(EXECUTABLE2)