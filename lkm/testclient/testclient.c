#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/fcntl.h>

#define EXPLORER_DEV_PATH "/dev/explorer"
#define EXPLORER_SET_PATH 0x1337
#define EXPLORER_RUN 0x1338 

#define REPS 0x1000

int explorer_fd;
uint64_t result_buf[REPS];

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

static int explorer_set_path(uint32_t *path, size_t path_len) {
    struct setPathReq req;
    req.path = path;
    req.path_len = path_len;
    if (ioctl(explorer_fd, EXPLORER_SET_PATH, &req) < 0) {
        perror("ioctl");
        return -1;
    }
    return 0;
}

static int explorer_run(size_t cacheset_index, size_t reps) {
    struct runReq req;
    req.reps = reps;
    req.cacheset_index = cacheset_index;
    req.buffer = result_buf;
    req.buffer_len = sizeof(result_buf);
    if (ioctl(explorer_fd, EXPLORER_RUN, &req) < 0) {
        perror("ioctl");
        return -1;
    }
    return 0;
}

int main() {
    uint32_t path[] = {0,1,2,3,4,5,6,7,1};
    FILE *fp;
    if ((explorer_fd = open(EXPLORER_DEV_PATH, O_RDONLY)) == -1) {
        perror("open");
        return -1;
    }
    if ((fp = fopen("result.txt", "wb")) == NULL) {
        perror("fopen");
        return -1;
    }
    if (explorer_set_path(path, sizeof(path)/sizeof(path[0])) < 0) {
        fprintf(stderr, "[-] set_path fail\n");
        return -1;
    }
    for (int i = 0; i < 8; i++) {
        explorer_run(i, REPS);
        fwrite(result_buf, 1, sizeof(result_buf), fp);
    }
    fclose(fp);
    return 0;
}