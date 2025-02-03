CC = gcc
CFLAGS = -Wall -g -O0

main:main.c 
	    $(CC) $(CFLAGS) -o $@ $^

clean:
	    $(RM) can_receive .*.sw?
