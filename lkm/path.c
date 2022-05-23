#include <linux/kernel.h>
#include <linux/gfp.h>
#include "path.h"

#define L1_SETS 0x40
#define LINE_SZ 0x40
#define PATH_ARENA_SZ_BITS 20
#define PATH_ARENA_SZ (1 << PATH_ARENA_SZ_BITS)

static char *path_arena;
static off_t path_arena_head;

extern char *g_cacheset[];

static void path_arena_clear(void) {
    path_arena_head = 0;
}

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

bool path_init() {
    if ((path_arena = (char *)__get_free_pages(GFP_KERNEL, PATH_ARENA_SZ_BITS)) == NULL) {
        printk(KERN_ERR "OOM in path_init\n");
        return false;
    }
    path_arena_head = 0;
    return true;
}

void path_fini() {
    free_pages((unsigned long)path_arena, PATH_ARENA_SZ_BITS);
}

static struct load_seq_path *path_alloc(char *addr) {
    struct load_seq_path *cand;
    while(1) {
        cand = (struct load_seq_path *)(path_arena + path_arena_head);
        if ((uint64_t)cand > (uint64_t)path_arena + PATH_ARENA_SZ) {
            return NULL;
        }
        if (range_overlap((uint64_t)cand, (uint64_t)cand+sizeof(struct load_seq_path), 0x0, LINE_SZ)) {
            path_arena_head += LINE_SZ;
        }
        else {
            path_arena_head += sizeof(struct load_seq_path);
            cand->addr = addr;
            cand->next = NULL;
            return cand;
        }
    }
}

struct load_seq_path *path_read(uint32_t *path_buf, size_t path_len) {
    struct load_seq_path *cur, *prev, *begin;
    size_t i;
    path_arena_clear();
    for (i = 0; i < path_len; i++) {
        // potential OOB here
        if ((cur = path_alloc(g_cacheset[path_buf[i]])) == NULL) {
            return NULL;
        }
        if (prev) {
            prev->next = cur;
        }
        else {
            begin = cur;
        }
        prev = cur;
    }
    return begin;
}