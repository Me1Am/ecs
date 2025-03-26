#include <stdio.h>

#include "../klib/kvec.h"

int main(int argc, char** argv) {
    kvec_t(int) components;
    kv_init(components);

    int i;
    for(i = 0; i < 128; i++)
        kv_push(int, components, 128-i);

    for(i = 0; i < kv_size(components); i++)
        printf("%d: %d\n", i, kv_A(components, i));

    return 0;
}
