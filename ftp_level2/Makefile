CC = gcc
CFLAGS = -O3 -march=native -flto -ffast-math -DNDEBUG -Wall -Wextra
LDFLAGS = -s -Wl,-O1 -Wl,--as-needed

ftp: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

clean:
	rm -f server client

.PHONY: clean ftp