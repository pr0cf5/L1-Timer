#include <linux/kernel.h>
#include <asm/processor.h>

#include "pmc.h"

#define CPUID_PERFORMANCE_MONITORING_LEAF 0xA

bool pmc_avail() {
    unsigned eax;
    char identifier;
    eax = cpuid_eax(CPUID_PERFORMANCE_MONITORING_LEAF);
    identifier = eax & 0xFF;
    return identifier > 0;
}