# Makefile for PeerUdp2Raw tests

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -I..

# Include all necessary object files
TEST_SRC = PeerUdp2RawTest.cpp PeerUdp2Raw.cpp Path.cpp PathUdp2Raw.cpp Udp2RawConfig.cpp Udp2RawProtocol.cpp ../osdep/Udp2RawSocket.cpp
TEST_OBJ = $(TEST_SRC:.cpp=.o)
TEST_BIN = peer_udp2raw_test

.PHONY: all clean test

all: $(TEST_BIN)

$(TEST_BIN): $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(TEST_OBJ) $(TEST_BIN)
