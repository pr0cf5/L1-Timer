#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <err.h>
#include <stdio.h>
#include <stdbool.h>
#include <x86intrin.h>
#include <unistd.h>

#include "l1-config.h"
#include "path.h"
#include "fixed-addrs.h"

#define REPS 0x1000
#define CACHESET_MAX_SZ 0x10
#define CLEARSET_MAX_SZ 8
#define MAX_BUFFER_SZ (1 << 25)

#define PGMASK 0xFFFFFFFFFFFFF000
#define PGSIZE 0x1000

static uint64_t results[REPS];

static uint64_t inline __attribute__((always_inline)) rdtsc_begin() {
uint32_t high, low;
asm volatile (
    "lfence\n\t"
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
    "lfence\n\t"
    : "=r" (high), "=r" (low)
    :
    : "rax", "rbx", "rcx", "rdx");
return ((uint64_t)high << 32) | low;
}

int main(int argc, char **argv) {
    FILE *fp;
    char *l1_page, *l2_page;
    register uint64_t cycles;
    register uint32_t trash;
    trash = 0;
    if ((fp = fopen("result.txt", "wb")) == NULL) {
        perror("fopen");
        exit(-1);
    }
    if ((l1_page = mmap(NULL, PGSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_POPULATE, -1, 0)) == NULL) {
        perror("mmap");
        exit(-1);
    }
    if ((l2_page = mmap(NULL, PGSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_POPULATE, -1, 0)) == NULL) {
        perror("mmap");
        exit(-1);
    }
    // install into page table
    *l1_page = 0;
    *l2_page = 0;
    // https://www.intel.com/content/dam/doc/manual/64-ia-32-architectures-optimization-manual.pdf pg.368
    // https://doc.rust-lang.org/beta/core/arch/x86_64/fn._mm_prefetch.html
    for (int i = 0; i <REPS; i++) {
        _mm_clflush(l2_page);
        asm volatile ("cpuid");
        _mm_prefetch(l2_page, _MM_HINT_T2);
        asm volatile ("cpuid");
        cycles = rdtsc_begin();
        trash = *(l2_page + trash);
        cycles = rdtsc_end() - cycles;
        results[i] = cycles;
    }
    fwrite(results, 1, sizeof(results), fp);
    for (int i = 0; i <REPS; i++) {
        _mm_clflush(l1_page);
        asm volatile ("cpuid");
        _mm_prefetch(l1_page, _MM_HINT_T0);
        asm volatile ("cpuid");
        cycles = rdtsc_begin();
        trash = *(l1_page + trash);
        cycles = rdtsc_end() - cycles;
        results[i] = cycles;
    }
    fwrite(results, 1, sizeof(results), fp);
    fclose(fp);
}