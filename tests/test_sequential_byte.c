#include <fcntl.h>
#include <malloc.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <zlib.h>

#include "../mgz.h"
#include "testtools.h"

/* gzip a random array of size bytes, save it to disk, and then
 * read it byte by byte sequentially using the random access
 * function. */
static bool test_sequential_byte_helper(size_t size, unsigned int seed) {
    uint8_t *data = test_create(size, seed);
    bool ret = data;
    if (!ret) return false;

#pragma omp parallel for
    for (size_t i = 0; i < size; ++i) {
        /* Validate data using random access. */
        int outfd = open("test.gz", O_RDONLY);
        if (outfd < 0) {
            printf(
                "test_sequential_byte_helper: failed to open().\n");
            ret = false;
        }
        FILE *lookup2 = fopen("test.lookup", "rb");
        uint8_t b;
        mgz_read(&b, 1, i, outfd, lookup2);
        if (b != data[i]) {
            printf(
                "test_sequential_byte_helper: test failed at "
                "index i = %zd; "
                "data decompressed = %u; original data = %u\n",
                i, b, data[i]);
            ret = false;
        }
        fclose(lookup2);
        if (close(outfd) < 0) {
            printf(
                "test_sequential_byte_helper: failed to close().\n");
        }
    }
    free(data);
    return ret;
}

bool test_sequential_byte() {
    size_t testSizes[30] = {
        1,       2,       3,       4,       5,       6,       7,
        8,       1023,    1024,    1025,    16383,   16384,   16385,
        32767,   32768,   32769,   65535,   65536,   65537,   1048575,
        1048576, 1048577, 2097151, 2097152, 2097153, 4258475, 0};
    for (int i = 0; i < 30; ++i) {
        if (testSizes[i] == 0) break;
        unsigned int seedMax = testSizes[i] > 100000 ? 1 : 10;
        for (unsigned int seed = 0; seed < seedMax; ++seed) {
            bool res =
                test_sequential_byte_helper(testSizes[i], seed);
            if (!res) {
                printf(
                    "test_sequential_byte: failed at %d of size %zd "
                    "with seed %u.\n",
                    i, testSizes[i], seed);
                return false;
            }
        }
        printf("test_sequential_byte: %d done.\n", i);
    }
    return true;
}
