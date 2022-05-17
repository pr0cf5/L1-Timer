#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include "l1-config.h"
#include "path.h"
#include "fixed-addrs.h"

#define PATH_ARENA_SZ 0x10000

static char *path_arena;
static off_t path_arena_head;

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

void path_init() {
    if ((path_arena = mmap(NULL, PATH_ARENA_SZ, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0)) == MAP_FAILED) {
        perror("mmap(PATH_ARENA)");
        exit(-1);
    }
    path_arena_head = 0;
}

struct path *path_alloc(char *addr) {
    struct path *cand;
    while(1) {
        cand = (struct path *)(path_arena + path_arena_head);
        if ((uint64_t)cand > (uint64_t)path_arena + PATH_ARENA_SZ) {
            fprintf(stderr, "path: out of memory\n");
            exit(-1);
        }
        if (range_overlap((uint64_t)cand, (uint64_t)cand+sizeof(struct path), 0x0, LINE_SZ)) {
            path_arena_head += LINE_SZ;
        }
        else {
            path_arena_head += sizeof(struct path);
            cand->addr = addr;
            cand->next = NULL;
            return cand;
        }
    }
}

struct path *path_read() {
    char **clearset, **evset;
    uint32_t path_length, path_elem;
    FILE *fp;
    struct path *cur, *prev, *begin;
    evset = (char **)EVSET_ARRAY;
    prev = NULL;
    begin = NULL;
    path_init();
    if ((fp = fopen("path.txt", "rb")) == NULL) {
        perror("fopen(path)");
        exit(-1);
    }
    if (fread(&path_length, 1, sizeof(path_length), fp) != sizeof(path_length)) {
        perror("fread(path_length)");
        exit(-1);
    }
    printf("[*] path_length: %d\n", path_length);
    for (uint32_t i = 0; i < path_length; i++) {
        if (fread(&path_elem, 1, sizeof(path_elem), fp) != sizeof(path_elem)) {
            perror("fread(path_elem)");
            exit(-1);
        }
        if (path_elem >= L1_ASSOC) {
            printf("[-] corrupted path file (%d)\n", path_elem);
        }
        cur = path_alloc(evset[path_elem]);
        if (prev) {
            prev->next = cur;
        }
        else {
            begin = cur;
        }
        prev = cur;
    }
    // validate
    cur = begin;
    for (uint32_t i = 0; i < path_length; i++) {
        if (i < path_length - 1 && cur->next == NULL) {
            printf("[-] corrupted singly linked list (1)\n");
            exit(-1);
        }
        else if (i == path_length - 1 && cur->next != NULL) {
            printf("[-] corrupted singly linked list (2)\n");
            exit(-1);
        }
        cur = cur->next;
    }
    fclose(fp);
    printf("[+] path validated, length = %d\n", path_length);
    return begin;
}