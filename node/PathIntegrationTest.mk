# Makefile for Path Integration tests

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -I..

TEST_SRC = PathIntegrationTest.cpp PathUdp2Raw.cpp Udp2RawConfig.cpp Udp2RawProtocol.cpp ../osdep/Udp2RawSocket.cpp
TEST_OBJ = $(TEST_SRC:.cpp=.o)
TEST_BIN = path_integration_test

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
