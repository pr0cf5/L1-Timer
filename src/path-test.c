#include <stdio.h>
#include <stdlib.h>
#include "path.h"

int main(int argc, char **argv) {
    path_init();
    for (int i = 0; i < 0x1000; i++) {
        printf("%d: %p\n", i, path_alloc(NULL));
    }
}