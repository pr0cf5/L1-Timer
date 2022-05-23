#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/preempt.h>
#include <linux/interrupt.h>

#include "pmc.h"
#include "path.h"

#define EXPLORER_SET_PATH 0x1337
#define EXPLORER_RUN 0x1338 
#define PATH_LEN_MAX 0x10000l

#define CACHESET_SZ 0x10
#define CLEARSET_SZ 0x10
#define MAX_REPS 0x1000l

MODULE_LICENSE("GPL");
MODULE_AUTHOR("L1 Cache Lover");
MODULE_DESCRIPTION("Reverse Engineer the L1 Cache");
MODULE_VERSION("0.1");

/* static function definitions */
static bool cacheset_init(char **set, size_t set_size);
static int explorer_open(struct inode *, struct file *);
static int explorer_release(struct inode *, struct file *);
static long explorer_ioctl(struct file *, unsigned int, unsigned long);
static uint64_t explorer_single_run(size_t last_elem);
static uint64_t rdtsc_begin(void);
static uint64_t rdtsc_end(void);

/* global variable definitions */
char *g_cacheset[CACHESET_SZ];
char *g_clearset[CLEARSET_SZ];

static struct load_seq_path *g_path;
static uint64_t result_buf[MAX_REPS];

static struct miscdevice explorer_dev;
struct file_operations explorer_fops = 
{
    .unlocked_ioctl = explorer_ioctl,
    .open = explorer_open,
    .release = explorer_release,
};

struct setPathReq {
    uint32_t *path;
    size_t path_len;
};

struct runReq {
    size_t reps;
    size_t cacheset_index;
    uint64_t *buffer;
    size_t buffer_len;
};


static int __init explorer_init(void) {
    // WARNING: There are memory leaks
    if (!pmc_avail()) {
        printk(KERN_ERR "CPU does not support PMC: explorer module load fail\n");
        return -1;
    }
    if (!cacheset_init(g_cacheset, sizeof(g_cacheset)/sizeof(g_cacheset[0]))) {
        printk(KERN_ERR "CacheSet allocation failed\n");
        return -1;
    }
    if (!cacheset_init(g_clearset, sizeof(g_clearset)/sizeof(g_clearset[0]))) {
        printk(KERN_ERR "ClearSet allocation failed\n");
        return -1;
    }
    if (!path_init()) {
        printk(KERN_ERR "Path arena allocation failed\n");
        return -1;
    }
    if (misc_register(&explorer_dev) < 0) {
        printk(KERN_ERR "Failed to install explorer module\n");
        return -1;
    }
    printk(KERN_INFO "Explorer loaded successfully\n");
    return 0;
}

static void __exit explorer_exit(void) {
    path_fini();
    printk(KERN_INFO "Explorer unloaded\n");
}

static int explorer_open(struct inode *inod, struct file *filp) {
    return 0;
}

static int explorer_release(struct inode *inod, struct file *filp) {
    return 0;
}

static long explorer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case EXPLORER_SET_PATH: {  
            struct setPathReq req;
            uint32_t *path_buf;
            if (copy_from_user(&req, (void *)arg, sizeof(req)) != sizeof(req)) {
                printk(KERN_ERR "Invalid address in EXPLORER_SET_PATH (1)\n");
                return -EFAULT;
            }
            if (req.path_len > PATH_LEN_MAX) {
                printk(KERN_ERR "Path length is too long (%lu > %lu)\n", req.path_len, PATH_LEN_MAX);
                return -E2BIG;
            }
            if ((path_buf = (uint32_t *)kzalloc(req.path_len, GFP_KERNEL)) == NULL) {
                printk(KERN_ERR "OOM in EXPLORER_SET_PATH\n");
                return -ENOMEM;
            }
            if (copy_from_user(path_buf, req.path, sizeof(*path_buf) * req.path_len) != sizeof(*path_buf) * req.path_len) {
                printk(KERN_ERR "Invalid address in EXPLORER_SET_PATH (2)\n");
                kfree(path_buf);
                return -EFAULT;
            }
            if ((g_path = path_read(path_buf, req.path_len)) == NULL) {
                printk(KERN_ERR "Invalid path in EXPLORER_SET_PATH\n");
                kfree(path_buf);
                return -EINVAL;
            }
            printk(KERN_INFO "Path successfully loaded (length = %lu)\n", req.path_len);
            kfree(path_buf);
            return 0;
        }
        case EXPLORER_RUN: {
            struct runReq req;
            size_t i;
            if (copy_from_user(&req, (void *)arg, sizeof(req)) != sizeof(req)) {
                printk(KERN_ERR "Invalid address in EXPLORER_RUN (1)\n");
                return -EFAULT;
            }
            if (req.reps > MAX_REPS) {
                printk(KERN_ERR "Rep is too big (%lu > %lu)\n", req.reps, MAX_REPS);
                return -E2BIG;
            }
            if (req.reps * sizeof(result_buf[0]) > req.buffer_len) {
                printk(KERN_ERR "Result buffer size is insufficient\n");
                return -EINVAL;
            }
            for (i = 0; i < req.reps; i++) {
                result_buf[i] = explorer_single_run(req.cacheset_index);
            }
            if (copy_to_user(req.buffer, result_buf, req.reps * sizeof(result_buf[0])) != req.reps * sizeof(result_buf[0])) {
                printk(KERN_ERR "Invalid address in EXPLORER_RUN (2)\n");
                return -EFAULT;
            }
            return 0;
        }
        default: {
            printk(KERN_ERR "Invalid ioctl command %d\n", cmd);
            return -1;
        }
    }
}

static bool cacheset_init(char **set, size_t set_size) {
    size_t i,j;
    for (i = 0; i < set_size; i++) {
        if ((set[i] = (char *)get_zeroed_page(GFP_KERNEL)) == NULL) {
            for (j = 0; j < i; i++) {
                free_page((unsigned long)set[j]);
            }
            return false;
        }
    }
    return true;
}

static uint64_t explorer_single_run(size_t last_elem) {
    register int i;
    register uint32_t trash;
    register struct load_seq_path *cur;
    register char *ptr; 
    register uint64_t cycles;
    trash = 0;
    cur = g_path;
    ptr = g_cacheset[last_elem];
    asm volatile ("lfence");

    // enter world of peace and quiet
    // TODO: disable HT
    preempt_disable();

    // first, set up the initial cache for set 0 
    for (i = 0; i < 0x1000; i++) {
        trash = *(g_clearset[0] + trash);
        asm volatile ("lfence");
        trash = *(g_clearset[1] + trash);
        asm volatile ("lfence");
        trash = *(g_clearset[2] + trash);
        asm volatile ("lfence");
        trash = *(g_clearset[3] + trash);
        asm volatile ("lfence");
        trash = *(g_clearset[4] + trash);
        asm volatile ("lfence");
        trash = *(g_clearset[5] + trash);
        asm volatile ("lfence");
        trash = *(g_clearset[6] + trash);
        asm volatile ("lfence");
        trash = *(g_clearset[7] + trash);
        asm volatile ("lfence");
    }

    // second, execute the sequence of loads in path
    while(cur) {
        trash = *(cur->addr + trash);
        asm volatile ("lfence");
        cur = cur->next;
    }   

    // now, try to access one of the evsets
    asm volatile ("lfence");
    cycles = rdtsc_begin();
    trash = *(ptr + trash);
    cycles = rdtsc_end() - cycles;

    // exit peaceful world
    preempt_disable();

    return cycles;
}

module_init(explorer_init);
module_exit(explorer_exit);

static uint64_t rdtsc_begin() {
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

static uint64_t rdtsc_end() {
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