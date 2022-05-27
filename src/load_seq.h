struct load_seq {
    struct load_seq_entry *head;
};

struct load_seq_entry {
    uint64_t addr_idx;
    char *addr;
    struct load_seq_entry *next;
};

int load_seq_init(struct load_seq *seq, uint64_t *addrs_list, size_t addrs_list_len);
// Pushes cacheset[elem] into seq. Returns 0 on success and -1 on failure
int load_seq_push(struct load_seq *seq, uint64_t addr_idx);
// Pops the last entry. If there were less than 2 entries in {seq}, return -1. Else return 0.
int load_seq_pop(struct load_seq *seq);