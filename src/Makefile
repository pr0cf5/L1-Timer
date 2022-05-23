CC=gcc
CFLAGS=-O3 -g

all: explorer l1-l2 path-test evset evset-gem5

explorer: main.c path.c
	$(CC) $(CFLAGS) -o $@ $^

l1-l2: l1-l2.c
	$(CC) $(CFLAGS) -o $@ $^

path-test: path-test.c path.c
	$(CC) $(CFLAGS) -o $@ $^

evset: evset.c
	$(CC) $(CFLAGS) -o $@ $^

evset-gem5: evset.c
	$(CC) $(CFLAGS) -DGEM5 -o $@ $^

clean:
	rm -rf explorer l1-l2 path-test evset evset-gem5 *.png *.txt
