CFLAGS = -Wall -g -O2  -std=c++11

CC = c++
#CC = clang++ 

DEPS = http3_server.h bytes_array.h huffman.h

OBJS = http3_server.o \
	event_loop.o \
	index.o \
	cgi.o \
	http3.o \
	ssl.o \
	util.o \
	config.o \
	log.o \
	percent_coding.o \
	

http3_server: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -L/usr/local/lib/ -L/usr/local/lib64/ -lssl -lcrypto

http3_server.o: http3_server.cpp $(DEPS)
	$(CC) $(CFLAGS) -c http3_server.cpp -o $@

event_loop.o: event_loop.cpp $(DEPS)
	$(CC) $(CFLAGS) -c event_loop.cpp -o $@

index.o: index.cpp $(DEPS)
	$(CC) $(CFLAGS) -c index.cpp -o $@

http3.o: http3.cpp $(DEPS)
	$(CC) $(CFLAGS) -c http3.cpp -o $@

ssl.o: ssl.cpp $(DEPS)
	$(CC) $(CFLAGS) -c ssl.cpp -o $@

util.o: util.cpp $(DEPS)
	$(CC) $(CFLAGS) -c util.cpp -o $@

config.o: config.cpp $(DEPS)
	$(CC) $(CFLAGS) -c config.cpp -o $@

log.o: log.cpp $(DEPS)
	$(CC) $(CFLAGS) -c log.cpp -o $@

cgi.o: cgi.cpp $(DEPS)
	$(CC) $(CFLAGS) -c cgi.cpp -o $@

percent_coding.o: percent_coding.cpp $(DEPS)
	$(CC) $(CFLAGS) -c percent_coding.cpp -o $@

clean:
	rm -f http3_server
	rm -f *.o
