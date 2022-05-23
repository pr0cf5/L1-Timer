#include <linux/types.h>

struct load_seq_path {
    char *addr;
    struct load_seq_path *next;
};

bool path_init(void);
void path_fini(void);
struct load_seq_path *path_read(uint32_t *path_buf, size_t path_len);
