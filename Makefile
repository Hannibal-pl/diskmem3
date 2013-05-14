CC=gcc
CFLAGS=-Wall -g -std=gnu99
INCLUDES=-I/usr/include
LIBS=-lm
MOD=diskmem3.o

all: diskmem3

$(MOD): %.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

diskmem3: $(MOD)
	$(CC) $(CFLAGS) $(LIBS) -o diskmem3 $(MOD)

clean:
	rm -f *o

rebuild: clean all

.PHONY: all clean rebuild
