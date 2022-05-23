#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <err.h>
#include <stdio.h>
#include <stdbool.h>
#include <emmintrin.h>
#include <unistd.h>

#include "l1-config.h"
#include "path.h"
#include "fixed-addrs.h"

#define REPS 0x1000
#define CACHESET_MAX_SZ 0x20
#define CLEARSET_MAX_SZ 8
#define MAX_BUFFER_SZ (1 << 25)

#define PGMASK 0xFFFFFFFFFFFFF000
#define PGSIZE 0x1000

static uint64_t results[REPS];

static uint64_t inline __attribute__((always_inline)) rdtsc_begin() {
uint32_t high, low;
asm volatile (
    "CPUID\n\t"
    "RDTSC\n\t"
    "mov %%edx, %0\n\t"
    "mov %%eax, %1\n\t"
    : "=r" (high), "=r" (low)
    :
    : "rax", "rbx", "rcx", "rdx");
return ((uint64_t)high << 32) | low;
}

static uint64_t inline __attribute__((always_inline)) rdtsc_end() {
uint32_t high, low;
asm volatile(
    "RDTSCP\n\t"
    "mov %%edx, %0\n\t"
    "mov %%eax, %1\n\t"
    "CPUID\n\t"
    : "=r" (high), "=r" (low)
    :
    : "rax", "rbx", "rcx", "rdx");
return ((uint64_t)high << 32) | low;
}

void shuffle(char **array, size_t array_length) {
    srand(time(NULL));
    size_t n = array_length;
    if (n > 1) {
        size_t i, j;
        char *tmp;
        for (i = 0; i < n-1; i++) {
            j =  i + rand() / (RAND_MAX/(n-i)+1);
            tmp = array[j];
            array[j] = array[i];
            array[i] = tmp;
        }
    }
}

uint64_t measure(struct path *path, uint64_t next_elem) {
    register int i,j;
    register uint32_t trash;
    register uint64_t cycles;
    register struct path *cur;
    register volatile char *ptr;
    volatile register char **clearset, **cacheset;
    trash = 0;
    cacheset = (volatile char **)CACHESET_ARRAY;
    clearset = (volatile char **)CLEARSET_ARRAY;
    cur = path;
#ifdef VALIDATE
    uint64_t *res;
    res = (uint64_t *)MISC_ARRAY;
#endif
    
    // first, set up the initial cache for set 0 
    for (i = 0; i < 0x1000; i++) {
        trash = *(clearset[0] + trash);
        _mm_lfence();
        trash = *(clearset[1] + trash);
        _mm_lfence();
        trash = *(clearset[2] + trash);
        _mm_lfence();
        trash = *(clearset[3] + trash);
        _mm_lfence();
        trash = *(clearset[4] + trash);
        _mm_lfence();
        trash = *(clearset[5] + trash);
        _mm_lfence();
        trash = *(clearset[6] + trash);
        _mm_lfence();
        trash = *(clearset[7] + trash);
        _mm_lfence();
    }

#ifdef VALIDATE
    // validate the initial cache state
    for (i = 0; i < L1_ASSOC; i++) {
        cycles = rdtsc_begin();
        trash = *(clearset[i] + trash);
        cycles = rdtsc_end() - cycles;
        res[i] = cycles;
    }
    for (int i = 0; i < L1_ASSOC; i++) {
        printf("%ld\n", res[i]);
    }
#endif
    // execute the sequence of loads
    while(cur) {
        trash = *(cur->addr + trash);
        _mm_lfence();
        cur = cur->next;
    }    

    // now, try to access one of the evsets
    ptr = cacheset[next_elem];
    _mm_lfence();
    cycles = rdtsc_begin();
    trash = *(ptr + trash);
    cycles = rdtsc_end() - cycles;
    return cycles;
}

int main(int argc, char **argv) {
    size_t nlines;
    FILE *fp;
    char **clearset, **cacheset;
    uint64_t *misc_array;
    struct path *path;
    char *buffer;
    if ((buffer = (char *)mmap(NULL, MAX_BUFFER_SZ, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_POPULATE, -1, 0)) == MAP_FAILED) {
        perror("mmap (g_hpage)");
        exit(-1);
    }
    if (mmap((void *)(CACHESET_ARRAY & PGMASK), PGSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0) == MAP_FAILED) {
        perror("mmap (CACHESET_ARRAY)");
        exit(-1);
    }
    if (mmap((void *)(CLEARSET_ARRAY & PGMASK), PGSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0) == MAP_FAILED) {
        perror("mmap (CLEARSET_ARRAY)");
        exit(-1);
    }
    if (mmap((void *)(MISC_ARRAY & PGMASK), PGSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0) == MAP_FAILED) {
        perror("mmap (RESULTS_ARRAY)");
        exit(-1);
    }
    if ((fp = fopen("result.txt", "wb")) == NULL) {
        perror("fopen(result)");
        exit(-1);
    }
    // initialize the eviction set
    cacheset = (char **)CACHESET_ARRAY;
    clearset = (char **)CLEARSET_ARRAY;
    for (int i = 0; i < CACHESET_MAX_SZ; i++) {
        cacheset[i] = buffer + i * LINE_SZ * L1_SETS;
    }
    for (int i = 0; i < CLEARSET_MAX_SZ; i++) {
        clearset[i] = buffer + (i + CACHESET_MAX_SZ) * LINE_SZ * L1_SETS;
    }
    shuffle(cacheset, CACHESET_MAX_SZ);
    shuffle(clearset, CLEARSET_MAX_SZ);
    // read path
    path = path_read();
    
    for (int j = 0; j < REPS; j++) {
        for (int i = 0; i < L1_ASSOC; i++) {
            results[i] = measure(path, i);
        }
        fwrite(results, 1, L1_ASSOC*sizeof(results[0]), fp);
    }
    fclose(fp);
}