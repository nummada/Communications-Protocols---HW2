CFLAGS = -Wall -g

# Portul pe care asculta serverul
PORT = 8042

# Adresa IP a serverului
IP_SERVER = 127.0.0.1

ID = 345

all: server subscriber

# Compileaza server.c
server: server.c -lm

# Compileaza subscriber.c
subscriber: subscriber.c

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza subscriberul
run_subscriber:
	./subscriber ${ID} ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber
