#ifndef TESTTOOLS_H
#define TESTTOOLS_H
#include <stdint.h>
#include <string.h>

void random_fill(void *space, size_t size, unsigned int seed);
uint8_t *test_create(size_t size, unsigned int seed);
uint64_t compare(void *buf1, void *buf2, uint64_t size);

#endif  // TESTTOOLS_H
