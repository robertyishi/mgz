#include "testtools.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../mgz.h"

void random_fill(void *space, size_t size, unsigned int seed) {
    srand(seed);
    for (size_t i = 0; i < size; ++i) {
        ((unsigned char *)space)[i] = rand() % UINT8_MAX;
    }
}

uint8_t *test_create(size_t size, unsigned int seed) {
    uint8_t *data = (uint8_t *)malloc(size);
    if (!data) {
        printf("test_create: oom\n");
        return NULL;
    }
    random_fill(data, size, seed);

    FILE *outfile = fopen("test.gz", "wb");
    FILE *lookup = fopen("test.lookup", "wb");
    if (!outfile || !lookup) {
        printf("test_create: failed to create outfile(s).\n");
        free(data);
        return NULL;
    }
    mgz_parallel_create(data, size, 9, 16384, outfile, lookup);
    fclose(outfile);
    fclose(lookup);
    return data;
}

uint64_t compare(void *buf1, void *buf2, uint64_t size) {
    for (uint64_t i = 0; i < size; ++i) {
        if (((uint8_t *)buf1)[i] != ((uint8_t *)buf2)[i]) return i;
    }
    return size;
}
