#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <err.h>
#include <stdio.h>
#include <stdbool.h>
#include <emmintrin.h>
#include <unistd.h>
#include <assert.h>

#include "l1-config.h"
#include "load_seq.h"
#include "fixed-addrs.h"
#include "counters.h"
#include "libpfc.h"

#define REPS 0x100
#define CACHESET_MAX_SZ 0x20
#define CLEARSET_MAX_SZ 8
#define MAX_BUFFER_SZ (1 << 25)

#define PGMASK 0xFFFFFFFFFFFFF000
#define PGSIZE 0x1000

static double average(uint64_t *array, size_t array_len);
static void shufle(char **array, size_t array_len);
static bool determine_replacement(struct load_seq *seq, uint64_t addr_idx);
static uint64_t explorer_single_run(struct load_seq *seq, uint64_t addr_idx);
static int explorer_algorithm_recurse(struct load_seq *seq, size_t remaining_len);

static double average(uint64_t *array, size_t array_len) {
    size_t i;
    double sum;
    sum = 0;
    for (i = 0; i < array_len; i++) {
        sum += (double)array[i];
    }
    return sum / (double)array_len;
}

static void shuffle(char **array, size_t array_len) {
    srand(time(NULL));
    size_t n = array_len;
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

static uint64_t explorer_single_run(struct load_seq *seq, uint64_t addr_idx) {
    register int i,j;
    register uint32_t trash;
    register uint64_t ctr;
    register volatile struct load_seq_entry *cur;
    register volatile char *ptr;
    register volatile char **clearset, **cacheset;
    //initialize variables
    trash = 0;
    cacheset = (volatile char **)CACHESET_ARRAY;
    clearset = (volatile char **)CLEARSET_ARRAY;
    cur = seq->head;
    ptr = cacheset[addr_idx];
    _mm_lfence();

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

    // execute the sequence of loads
    while(cur) {
        trash = *(cur->addr + trash);
        _mm_lfence();
        cur = cur->next;
    }    
#ifndef USE_PMC
    ctr = rdtsc_begin();
#else
    ctr = rdpmc_begin();
#endif
    // now, access the pointer to test
    trash = *(ptr + trash);
#ifndef USE_PMC
    ctr = rdtsc_end() - ctr;
#else
    ctr = rdpmc_end() - ctr;
#endif
    return ctr;
}

bool explorer_init() {
    char **clearset, **cacheset;
    uint64_t *misc_array;
    uint64_t cfg;
    char *buffer;
    int rv;
    // initialize buffers
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
    
    // pin thread to core
    if (pfcPinThread(3) != 0) {
        fprintf(stderr, "Failed to pin thread\n");
        exit(-1);
    }
    // initialize PMC
    if (pfcInit() < 0) {
        fprintf(stderr, "PFC kernel modules are not installed\n");
        exit(-1);
    }
    // program GP0 to ld1.replacement
    if ((cfg = pfcParseCfg("l1d.replacement")) == 0) {
        fprintf(stderr, "Invalid counter event/umask name\n");
        exit(-1);
    }
    // GP starts from index 3
    if ((rv = pfcWrCfgs(3, 1, &cfg)) != 0) {
        fprintf(stderr, "Failed to configure counter (rv = %d)\n", rv);
        exit(-1);
    }
}

void explorer_fini() {
    pfcFini();
}

// return true if cache miss occurred
static bool determine_replacement(struct load_seq *seq, uint64_t addr_idx) {
    static uint64_t results[REPS];
    size_t iters;
    float avg;
    for (iters = 0; iters < REPS; iters++) {
        results[iters] = explorer_single_run(seq, addr_idx);
        //assert(0 <= results[i] && results[i] <= 1);
    }
    avg = average(results, sizeof(results)/sizeof(results[0]));
    if (0.95 <= avg && avg <= 1.0) {
        return true;
    }
    else if (0.0 <= avg && avg <= 0.05) {
        return false;
    }
    else {
        // unreachable
        printf("avg: %lf\n", avg);
        assert(0);
    }
}

static int explorer_algorithm_recurse(struct load_seq *seq, size_t remaining_len) {
    float avg;
    size_t set;
    if (remaining_len == 0) {
        return 0;
    }
    for (set = 0; set < L1_ASSOC; set++) {
        if (determine_replacement(seq, set)) {
            load_seq_push(seq, set);
            if (explorer_algorithm_recurse(seq, remaining_len - 1) == 0) {
                return 0;
            }
            load_seq_pop(seq);
        }
    }
    if (set == L1_ASSOC) {
        // red not found: backtrack
        return -1;
    }
}

void explorer_algorithm_main(size_t goal_len) {
    uint64_t initial_sequence[] = {0,1,2,3,4,5,6,7,8};
    struct load_seq seq;
    struct load_seq_entry *cur;
    if (load_seq_init(&seq, initial_sequence, sizeof(initial_sequence)/sizeof(initial_sequence[0])) < 0) {
        return;
    }
    if (explorer_algorithm_recurse(&seq, goal_len) < 0) {
        fprintf(stderr, "could not find any paths!\n");
        return;
    }
    // print path
    for (cur = seq.head; cur; cur = cur->next) {
        if (cur->next) {
            printf("%ld --> ", cur->addr_idx);
        }
        else {
            printf("%ld\n", cur->addr_idx);
        }
    }
}

void explorer_test_seq(struct load_seq *seq) {
    struct load_seq_entry *cur;
    struct load_seq replay_seq;
    uint64_t iters;
    static uint64_t results[REPS];
    float avg;
    load_seq_init(&replay_seq, NULL, 0);
    for (cur = seq->head; cur; cur = cur->next) {
        for (iters = 0; iters < REPS; iters++) {
            results[iters] = explorer_single_run(seq, cur->addr_idx);
            //assert(0 <= results[i] && results[i] <= 1);
        }
        avg = average(results, sizeof(results)/sizeof(results[0]));
        if (cur->next) {
            if (determine_replacement(&replay_seq, cur->addr_idx)) {
                printf("%ld --M--> ", cur->addr_idx);
            }
            else {
                printf("%ld --H--> ", cur->addr_idx);
            }
            load_seq_push(&replay_seq, cur->addr_idx);
        }
        else {
            printf("%ld\n", cur->addr_idx);
        }
    }
}

int main(int argc, char **argv) {
    struct load_seq plru_seq;
    uint64_t array[] = {0,1,2,4,3,5,6,4,7,0,1,4,2,3,5,4,6,7,0,4,1,2,3,4,5,6,7,4,0,1,2,4,3,5,6,4,7,0,1,4,2,3,5,4,6,7,0,4,1,2,3,4,5,6,7,4};
    explorer_init();
    if (load_seq_init(&plru_seq, array, sizeof(array)/sizeof(array[0])) < 0) {
        assert(0);
    }
    explorer_test_seq(&plru_seq);
    explorer_fini();
}