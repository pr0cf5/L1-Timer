#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <err.h>
#include <stdio.h>
#include <stdbool.h>
#include <emmintrin.h>
#include <unistd.h>

// L1 cache is 32KiB
#define LINE_SZ 64
#define L1_SETS 64
#ifdef GEM5
#define REPS 2
#else
#define REPS 0x1000
#endif
#define EVSET_MAX_SZ 0x80
#define MAX_BUFFER_SZ (1<< 25)

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
char *g_evset[EVSET_MAX_SZ];
size_t g_evset_size;
char *g_victim;
uint64_t g_t1,g_t2;
uint64_t results[REPS*2];
uint8_t g_junk;

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

// For preventing TLB from affecting results, 
// read all of the eviction sets in to cache once, and flush them
void cleanup() {
  g_junk &= *g_victim;
  for (int i = 0; i < EVSET_MAX_SZ; i++) {
    g_junk &= *g_evset[i];
  }
  _mm_clflush(g_victim);
  for (int i = 0; i < EVSET_MAX_SZ; i++) {
    _mm_clflush(g_evset[i]);
  }
}

void measure() {
  register int i,j;
  register uint32_t trash;
  trash = 0;
  // put victim in cache
  trash = force_read(g_victim+trash);
  // measure the L1 cache latency
  g_t1 = rdtsc_begin();
  trash = force_read(g_victim+trash);
  g_t1 = rdtsc_end() - g_t1;
  // put the entire page into cache
  for (i = 0; i < g_evset_size; i++) {
    trash = force_read(g_evset[i] + trash);
  }
  // now, this should be L2 cache latency
  g_t2 = rdtsc_begin();
  trash = force_read(g_victim+trash);
  g_t2 = rdtsc_end() - g_t2;
}

int main(int argc, char **argv) {
  size_t nlines;
  FILE *fp;

  if ((g_hpage = mmap(NULL, MAX_BUFFER_SZ, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_POPULATE, -1, 0)) == MAP_FAILED) {
    perror("mmap");
    exit(-1);
  }
  printf("[+] page: %p\n", g_hpage);
  #ifndef GEM5
  if ((fp = fopen("result.txt", "wb")) == NULL) {
    perror("fopen");
    exit(-1);
  }
  #endif
  // initialize the eviction set
  for (int i = 0; i < EVSET_MAX_SZ; i++) {
    g_evset[i] = g_hpage + i * LINE_SZ * L1_SETS;
  }
  shuffle(g_evset, EVSET_MAX_SZ);
  g_victim = g_hpage + EVSET_MAX_SZ*LINE_SZ*L1_SETS;

  for (int i = 0; i < EVSET_MAX_SZ; i++) {
    g_evset_size = i;
    for (int j = 0; j < REPS; j++) {
      //cleanup();
      measure();
      results[j*2] = g_t1;
      results[j*2+1] = g_t2;
      #ifdef GEM5
      printf("[%d] %ld %ld\n", j, g_t1,g_t2);
      #endif
    }
    #ifndef GEM5
    fwrite(results, 1, sizeof(results), fp);
    #endif
  }
  #ifndef GEM5
  fclose(fp);
  #endif
  return 0;
}