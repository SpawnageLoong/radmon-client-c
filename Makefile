CC = g++
CFLAGS = -Wall -g -O0

bin/radmon-client:src/main.c 
	    $(CC) $(CFLAGS) -o $@ $^

clean:
	    $(RM) bin/radmon-client .*.sw?
