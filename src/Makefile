CC=gcc
CFLAGS=-O3

all: evset evset-gem5 cache-fillall

evset: evset.c
	$(CC) $(CFLAGS) -o $@ $^

evset-gem5: evset.c
	$(CC) $(CFLAGS) -DGEM5 -o $@ $^

clean:
	rm -rf evset evset-gem5
