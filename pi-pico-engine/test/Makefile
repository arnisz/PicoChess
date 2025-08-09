CXX=g++
CXXFLAGS=-std=c++17 -I../src -I. -DDEBUG_MODE

all: test

test: test_engine.o
$(CXX) $(CXXFLAGS) test_engine.o -o chess_engine_test

test_engine.o: test_engine.cpp
$(CXX) $(CXXFLAGS) -c test_engine.cpp

clean:
 rm -f *.o chess_engine_test
