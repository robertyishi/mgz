#include "test_all.h"
#include <stdio.h>

int main() {
    if (!test_sequential_byte()) return 1;
    if (!test_gzread()) return 1;
    printf("passed\n");
    return 0;
}
