#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <assert.h>
#include "l1-config.h"
#include "fixed-addrs.h"
#include "load_seq.h"

#define PATH_ARENA_SZ 0x10000

static off_t path_arena_head;

// an assumption made here is that the L1 cache fits within page size
static bool range_overlap(uint64_t x1, uint64_t x2, uint64_t y1, uint64_t y2) {
    x1 = x1 % (L1_SETS*LINE_SZ);
    x2 = x2 % (L1_SETS*LINE_SZ);
    y1 = y1 % (L1_SETS*LINE_SZ);
    y2 = y2 % (L1_SETS*LINE_SZ);
    if (x2 < x1) {
        x2 += (L1_SETS*LINE_SZ);
    }
    if (y2 < y1) {
        y2 += (L1_SETS*LINE_SZ);
    }
    if (y1 > x2 || x1 > y2) {
        return false;
    }
    else {
        return true;
    }
}

static struct load_seq_entry *load_seq_entry_alloc(char *addr, uint64_t addr_idx) {
    struct load_seq_entry *cand;
    uint64_t begin, end;
    while (1) {
        if ((cand = calloc(1,sizeof(struct load_seq_entry))) == NULL) {
            perror("calloc");
            exit(-1);
        }
        begin = (uint64_t)cand;
        end = begin + sizeof(struct load_seq_entry);
        if (!range_overlap(begin, end, 0, 0x40)) {
            cand->addr_idx = addr_idx;
            cand->addr = addr;
            cand->next = NULL;
            return cand;
        }
        // don't free cand if it overlaps, so that it is forgotten by malloc completely
    }
}

int load_seq_init(struct load_seq *seq, uint64_t *addrs_list, size_t addrs_list_cnt) {
    char **cacheset;
    uint32_t addr_idx, i;
    struct load_seq_entry *cur, *prev;
    cacheset = (char **)CACHESET_ARRAY;
    prev = NULL;
    if (addrs_list_cnt == 0) {
        seq->head = NULL;
        return 0;
    }
    for (i = 0; i < addrs_list_cnt; i++) {
        addr_idx = addrs_list[i];
        cur = load_seq_entry_alloc(cacheset[addr_idx], addr_idx);
        if (prev == NULL) {
            seq->head = cur;
        }
        else {
            prev->next = cur;
        }
        prev = cur;
    }
    // validate
    cur = seq->head;
    for (i = 0; i < addrs_list_cnt; i++) {
        assert(!(i < addrs_list_cnt - 1 && cur->next == NULL));
        assert(!(i == addrs_list_cnt - 1 && cur->next != NULL));
        cur = cur->next;
    }
    return 0;
}

int load_seq_push(struct load_seq *seq, uint64_t addr_idx) {
    char **cacheset;
    struct load_seq_entry *cur, *new;
    cacheset = (char **)CACHESET_ARRAY;
    new = load_seq_entry_alloc(cacheset[addr_idx], addr_idx);
    cur = seq->head;
    // no entries
    if (seq->head == NULL) {
        seq->head = new;
        return 0;
    }
    while (cur->next) {
        cur = cur->next;
    }
    cur->next = new;
    return 0;
}

int load_seq_pop(struct load_seq *seq) {
    struct load_seq_entry *cur, *prev;
    if (seq->head == NULL) {
        return -1;
    }
    else  if (seq->head->next == NULL) {
        free(seq->head);
        seq->head = NULL;
        return 0;
    }
    else {
        prev = seq->head;
        cur = seq->head;
        while (cur->next) {
            cur = cur->next;
            prev = cur;
        }
        prev->next = NULL;
        return 0;
    }
}

