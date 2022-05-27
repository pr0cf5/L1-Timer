CC=gcc
CFLAGS=-O3 -g -DUSE_PMC
INCLUDEFLAGS=-I libpfc/include

all: explorer

explorer: explorer_main.c load_seq.c libpfc/libpfc.o
	$(CC) $(INCLUDEFLAGS) $(CFLAGS) -o $@ $^

libpfc/libpfc.o:
	make -C libpfc

clean:
	rm -rf explorer *.png *.txt
