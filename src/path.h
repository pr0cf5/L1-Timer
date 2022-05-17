struct path {
    char *addr;
    struct path *next;
};

struct path *path_read();
void path_init(void);
struct path *path_alloc(char *addr);