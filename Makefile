CC = g++
CFLAGS = -Wall -g -O0

radmon-client:src/main.c 
	    $(CC) $(CFLAGS) -o $@ $^

clean:
	    $(RM) radmon-client .*.sw?
