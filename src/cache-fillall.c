#include <stdint.h>
#include <sys/mman.h>
#include <err.h>
#include <stdio.h>
#include <stdbool.h>
#include <emmintrin.h>
#include <unistd.h>

// L1 cache is 32KiB
#define LINE_SZ 64
#define PGSIZE 0x1000
#define REPS 0x1000
#define MAX_BUFFER_SIZE (0x100 * PGSIZE)

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

static uint32_t __attribute__((always_inline)) force_read(const char *p) {
  return (uint32_t)*(const volatile uint32_t*)(p);
}

char *g_hpage;
char *g_small_page;
uint64_t g_t1, g_t2;

void measure(size_t buffer_size) {

  uint32_t trash;
  char *ptr, *victim;
  trash = 0;

  victim = g_small_page + 0x2c0;
  trash = force_read(victim+trash);

  // measure the L1 cache latency
  g_t1 = rdtsc_begin();
  trash = force_read(victim+trash);
  g_t1 = rdtsc_end() - g_t1;

  // put the entire page into cache
  for (int i = 0; i < buffer_size; i += LINE_SZ) {
    ptr = g_hpage + i;
    trash = force_read(ptr + trash);
  }

  // now, this should be L2 cache latency
  g_t2 = rdtsc_begin();
  trash = force_read(victim+trash);
  g_t2 = rdtsc_end() - g_t2;
}

int main(int argc, char **argv) {
  size_t nlines;
  uint64_t t1, t2;
  int i;

  if ((g_hpage = mmap(NULL, MAX_BUFFER_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_HUGETLB|MAP_POPULATE, -1, 0)) == MAP_FAILED) {
    perror("mmap");
    exit(-1);
  }
  if ((g_small_page = mmap(NULL, PGSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_POPULATE, -1, 0)) == MAP_FAILED) {
    perror("mmap");
    exit(-1);
  }
  
  for (nlines = 1; nlines < 0x1000; nlines++) {
    t1 = 0;
    t2 = 0;
    for (i = 0; i < REPS; i++) {
      measure(nlines * LINE_SZ);
      t1 += g_t1;
      t2 += g_t2;
    }
    printf("0x%lx %ld %ld\n", nlines * LINE_SZ, t1/REPS,t2/REPS);
  }
    
  return 0;
}