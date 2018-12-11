LINK=gcc
CC=gcc
CFLAGS=-c -g3 -ggdb -std=gnu99 -fPIC -Wall -I. #-I/opt/local/include
LINKFLAGS=-L. -g3 -ggdb -std=gnu99 -pthread #-L/opt/local/lib
LIBFLAGS=-shared -Wall -pthread
LINKLIBS=-lm -lrt -lpthread
ARCHIVE=ar
TARGETS=server client blocking_client client17
OBJECTS= server-main.o \
			cache.o \
			c1.o \
			c0.o \
			journal.o \
			server-part1.o \
			client_common.o \
			abd.o \
			pqueue.o \
			blocking_node.o
TEST_TARGETS=abdunit

all: $(TARGETS)

test: $(TEST_TARGETS)

.c.o:
	$(CC) $(CFLAGS) -o $@ $<

server: $(OBJECTS)
	$(LINK) $(LINKFLAGS) $^ $(LINKLIBS) -o $@

client: client.c abd.c
	gcc -g -std=gnu99 abd.c client.c -o client -pthread -lm

client_blocking: blocking_node.o $(LIBS)
	$(LINK) $(LINKFLAGS) $^ $(LINKLIBS) -o $@

client7: client7.c
	gcc -g -std=gnu99 client7.c -o client7 -pthread -lm

client2: client2.c
	gcc -g -std=gnu99 client2.c -o client2 -pthread

client3: client3.c
	gcc -g -std=gnu99 client3.c -o client3 -pthread

client4: client4.c
	gcc -g -std=gnu99 client4.c -o client4 -pthread

client5: client5.c
	gcc -g -std=gnu99 client5.c -o client5 -pthread

client6: client6.c
	gcc -g -std=gnu99 client6.c -o client6 -pthread

client8: client8.c
	gcc -g -std=gnu99 client8.c -o client8 -pthread -lm

client17: client17.c
	gcc -g -std=gnu99 client17.c -o client17 -pthread

blocking_client: blocking_client.c
	gcc -g -std=gnu99 blocking_client.c -o blocking_client -pthread -lm

clean:
	rm server client *.o client2 client3 client4 client5 client6 client7 client8 client17 blocking_client
