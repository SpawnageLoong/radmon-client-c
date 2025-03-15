CC = g++
CXXFLAGS = -Wall -g -O0 -std=c++20

bin/radmon-client:src/main.cpp
	    $(CC) $(CXXFLAGS) -o $@ $^

clean:
	    $(RM) bin/radmon-client .*.sw?
