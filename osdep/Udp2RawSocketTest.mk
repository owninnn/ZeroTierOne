# Makefile for Udp2RawSocket tests

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -I..

TEST_SRC = Udp2RawSocketTest.cpp Udp2RawSocket.cpp ../node/Udp2RawProtocol.cpp
TEST_OBJ = $(TEST_SRC:.cpp=.o)
TEST_BIN = udp2raw_socket_test

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
