#include "test_gzread.h"

#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <zlib.h>

#include "testtools.h"

static bool test_gzread_helper(size_t size, unsigned int seed) {
    uint8_t *data = test_create(size, seed);
    uint8_t *decomp = (uint8_t *)malloc(size);
    bool ret = data && decomp;
    if (!ret) {
        free(data);
        free(decomp);
        return false;
    }

    gzFile gzf = gzopen("test.gz", "rb");
    if (!gzf) {
        printf("test_gzread_helper: failed to gzopen().\n");
        free(data);
        return false;
    }

    if (size != (size_t)gzread(gzf, decomp, size)) {
        gzclose(gzf);
        free(data);
        free(decomp);
        return false;
    }

    uint64_t errIdx = compare(data, decomp, size);
    if (size != errIdx) {
        printf("test_gzread_helper: errored at index %zd\n", errIdx);
        ret = false;
    }
    gzclose(gzf);
    free(data);
    free(decomp);
    return ret;
}

bool test_gzread() {
    size_t testSizes[30] = {
        1,       2,       3,       4,       5,       6,
        7,       8,       1023,    1024,    1025,    16383,
        16384,   16385,   32767,   32768,   32769,   65535,
        65536,   65537,   1048575, 1048576, 1048577, 2097151,
        2097152, 2097153, 4258475, 9652480, 0};
    for (int i = 0; i < 30; ++i) {
        if (testSizes[i] == 0) break;
        for (unsigned int seed = 0; seed < 10; ++seed) {
            bool res = test_gzread_helper(testSizes[i], seed);
            if (!res) {
                printf(
                    "test_gzread: failed at %d of size %zd with seed "
                    "%u.\n",
                    i, testSizes[i], seed);
                return false;
            }
        }
        printf("test_gzread: %d done.\n", i);
    }
    return true;
}
