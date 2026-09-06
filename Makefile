CXXFLAGS = -Wall -g -O2  -std=c++11

CXX = c++
#CXX = clang++ 

DEPS = http3_server.h bytes_array.h huffman.h

OBJS = http3_server.o \
	event_loop.o \
	index.o \
	cgi.o \
	scgi.o \
	http3.o \
	ssl.o \
	util.o \
	config.o \
	log.o \
	percent_coding.o \
	socket.o \
	

http3_server: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) -lssl -lcrypto

http3_server.o: http3_server.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c http3_server.cpp -o $@

event_loop.o: event_loop.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c event_loop.cpp -o $@

index.o: index.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c index.cpp -o $@

http3.o: http3.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c http3.cpp -o $@

ssl.o: ssl.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c ssl.cpp -o $@

socket.o: socket.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c socket.cpp -o $@

util.o: util.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c util.cpp -o $@

config.o: config.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c config.cpp -o $@

log.o: log.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c log.cpp -o $@

cgi.o: cgi.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c cgi.cpp -o $@

scgi.o: scgi.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c scgi.cpp -o $@

percent_coding.o: percent_coding.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c percent_coding.cpp -o $@

clean:
	rm -f http3_server
	rm -f *.o
