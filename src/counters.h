static uint64_t inline __attribute__((always_inline)) rdtsc_begin() {
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

static uint64_t inline __attribute__((always_inline)) rdtsc_end() {
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

static uint64_t inline __attribute__((always_inline)) rdpmc_begin() {
    uint32_t high, low;
    asm volatile(
        "CPUID\n\t"
        "xor %%ecx, %%ecx\n\t"
        "RDPMC\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r" (high), "=r" (low)
        :
        : "rax", "rbx", "rcx", "rdx");
    return ((uint64_t)high << 32) | low;
}

static uint64_t inline __attribute__((always_inline)) rdpmc_end() {
    uint32_t high, low;
    asm volatile(
        "CPUID\n\t"
        "xor %%ecx, %%ecx\n\t"
        "RDPMC\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r" (high), "=r" (low)
        :
        : "rax", "rbx", "rcx", "rdx");
    return ((uint64_t)high << 32) | low;
}