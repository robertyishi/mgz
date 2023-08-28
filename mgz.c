#include "mgz.h"

#include <assert.h>
#include <malloc.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "gz64.h"

#define CHUNK_SIZE 16384  // 16 KiB
#define ILLEGAL_CHUNK_SIZE (CHUNK_SIZE + 1)
#define DEFAULT_OUT_CAPACITY (CHUNK_SIZE << 1)
#define MIN_BLOCK_SIZE CHUNK_SIZE
#define DEFAULT_BLOCK_SIZE (1ULL << 20)  // 1 MiB
#define ILLEGAL_BLOCK_SIZE SIZE_MAX

static inline void *voidp_shift(const void *p, uint64_t offset) {
    return (void *)((uint8_t *)p + offset);
}

static uInt load_input(uint8_t *inBuf, const void *in, uint64_t offset,
                       uint64_t *remSize) {
    uInt loadSize = 0;
    if (*remSize == 0) return 0;
    if (*remSize < CHUNK_SIZE) {
        loadSize = *remSize;
    } else {
        loadSize = CHUNK_SIZE;
    }
    *remSize -= loadSize;
    memcpy(inBuf, voidp_shift(in, offset), loadSize);
    return loadSize;
}

static uInt copy_output(void **out, uint64_t outOffset, uint64_t *outCapacity,
                        uint8_t *outBuf, uInt have) {
    if (outOffset + have > *outCapacity) {
        /* Not enough space, reallocate output array. */
        do {
            (*outCapacity) *= 2;
        } while (outOffset + have > *outCapacity);
        void *newOut = realloc(*out, *outCapacity);
        if (!newOut) return ILLEGAL_CHUNK_SIZE;
        *out = newOut;
    }
    memcpy(voidp_shift(*out, outOffset), outBuf, have);
    return have;
}

uint64_t mgz_deflate(void **out, const void *in, uint64_t inSize, int level) {
    if (inSize == 0) {
        *out = NULL;
        return 0;
    }
    int zRet = Z_OK, flush = Z_NO_FLUSH;
    uint64_t inOffset = 0, outOffset = 0;
    uint64_t outCapacity = DEFAULT_OUT_CAPACITY;
    z_stream strm;
    uint8_t *inBuf = (uint8_t *)malloc(CHUNK_SIZE);
    uint8_t *outBuf = (uint8_t *)malloc(CHUNK_SIZE);
    *out = malloc(outCapacity);
    if (!inBuf || !outBuf || !(*out)) {
        fprintf(stderr, "mgz_deflate: malloc failed.\n");
        free(*out);
        *out = NULL;
        goto _bailout;
    }

    /* Allocate deflate state. */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    zRet = deflateInit2(&strm, level, Z_DEFLATED, 15 + 16, 8,
                        Z_DEFAULT_STRATEGY);  // +16 for gzip header.
    if (zRet != Z_OK) {
        free(*out);
        *out = NULL;
        goto _bailout;
    }

    /* Compress until the end of IN. */
    do {
        strm.avail_in = load_input(inBuf, in, inOffset, &inSize);
        inOffset += strm.avail_in;
        if (strm.avail_in < CHUNK_SIZE) flush = Z_FINISH;
        strm.next_in = inBuf;

        /* Run deflate() on input until output buffer not full; finish
         * compression if all of source has been read in. */
        do {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = outBuf;
            zRet = deflate(&strm, flush);
            if (zRet == Z_STREAM_ERROR) {
                fprintf(stderr,
                        "mgz_deflate: (FATAL) deflate returned "
                        "Z_STREAM_ERROR.\n");
                exit(1);
            }
            uInt have = CHUNK_SIZE - strm.avail_out;
            uInt copied =
                copy_output(out, outOffset, &outCapacity, outBuf, have);
            if (copied == ILLEGAL_CHUNK_SIZE) {
                fprintf(stderr, "mgz_deflate: output realloc failed.\n");
                (void)deflateEnd(&strm);
                free(*out);
                *out = NULL;
                outOffset = 0;
                goto _bailout;
            }
            assert(copied == have);
            outOffset += copied;
        } while (strm.avail_out == 0);
    } while (flush != Z_FINISH);

    /* Clean up and return. */
    (void)deflateEnd(&strm);

_bailout:
    free(inBuf);
    free(outBuf);
    return outOffset;
}

static uint64_t get_correct_block_size(uint64_t blockSize) {
    if (blockSize == 0) return DEFAULT_BLOCK_SIZE;
    if (blockSize < MIN_BLOCK_SIZE) {
        printf(
            "mgz_parallel_deflate: resetting block size %zu to the "
            "minimum required block size %d",
            blockSize, MIN_BLOCK_SIZE);
        return MIN_BLOCK_SIZE;
    }
    return blockSize;
}

static uint64_t convert_out_block_sizes_to_lookup(uint64_t *outBlockSizes,
                                                  uint64_t nBlocks) {
    uint64_t t1 = 0, t2 = 0;
    for (uint64_t i = 0; i < nBlocks; ++i) {
        t2 = t1 + outBlockSizes[i];
        outBlockSizes[i] = t1;
        t1 = t2;
    }
    outBlockSizes[nBlocks] = t1;
    return t1;
}

mgz_res_t mgz_parallel_deflate(const void *in, uint64_t inSize, int level,
                               uint64_t blockSize, bool lookup) {
    mgz_res_t ret = {0};
    blockSize = get_correct_block_size(blockSize);
    uint64_t nBlocks =
        (inSize + blockSize - 1) / blockSize;  // Round up division.
    if (nBlocks == 0) return ret;              // INSIZE is 0.
    void *out = NULL;

    /* Allocate space for the output of each block. */
    void **outBlocks = (void **)calloc(nBlocks, sizeof(void *));

    /* Shared space for outBlockSizes and lookup. outBlockSizes
       is later converted to lookup in-place. */
    uint64_t *space = (uint64_t *)malloc((nBlocks + 1) * sizeof(uint64_t));
    if (!outBlocks || !space) goto _bailout;

    /* Compress each block. */
    bool oom = false;
#pragma omp parallel for
    for (uint64_t i = 0; i < nBlocks; ++i) {
        uint64_t thisBlockSize =
            (i == nBlocks - 1) ? inSize - i * blockSize : blockSize;
        space[i] = mgz_deflate(&outBlocks[i], voidp_shift(in, i * blockSize),
                               thisBlockSize, level);
        if (space[i] == 0) oom = true;
    }
    if (oom) goto _bailout;

    /* Concatenate blocks to form the final output. */
    uint64_t outSize = convert_out_block_sizes_to_lookup(space, nBlocks);
    out = malloc(outSize);
    if (!out) goto _bailout;
#pragma omp parallel for
    for (uint64_t i = 0; i < nBlocks; ++i) {
        memcpy(voidp_shift(out, space[i]), outBlocks[i],
               space[i + 1] - space[i]);
        free(outBlocks[i]);
        outBlocks[i] = NULL;
    }

    /* Reach here only if compression was successful.
     * Setup return value. */
    ret.out = out;
    out = NULL;  // Prevent freeing.
    ret.size = outSize;
    if (lookup) {
        ret.lookup = space;
        space = NULL;  // Prevent freeing.
    }                  // else ret.lookup is already set to NULL.
    ret.nBlocks = nBlocks;

_bailout:
    free(out);
    if (outBlocks)
        for (uint64_t i = 0; i < nBlocks; ++i) free(outBlocks[i]);
    free(outBlocks);
    free(space);
    return ret;
}

uint64_t mgz_parallel_create(const void *in, uint64_t size, int level,
                             uint64_t blockSize, FILE *outfile, FILE *lookup) {
    blockSize = get_correct_block_size(blockSize);
    mgz_res_t res = mgz_parallel_deflate(in, size, level, blockSize, lookup);
    if (!res.out) return 0;
    if (fwrite(res.out, 1, res.size, outfile) != res.size) {
        fprintf(stderr,
                "mgz_parallel_create: (FATAL) failed to write to "
                "outfile.\n");
        exit(1);
    }
    if (lookup) {
        /* Write block size. */
        if (fwrite(&blockSize, sizeof(uint64_t), 1, lookup) != 1) {
            fprintf(stderr,
                    "mgz_parallel_create: (FATAL) failed to write to "
                    "lookup.\n");
            exit(1);
        }

        /* Write lookup table. */
        if (fwrite(res.lookup, sizeof(uint64_t), res.nBlocks, lookup) !=
            res.nBlocks) {
            fprintf(stderr,
                    "mgz_parallel_create: (FATAL) failed to write to "
                    "lookup.\n");
            exit(1);
        }
    }
    free(res.out);
    free(res.lookup);
    return res.size;
}

uint64_t mgz_read(void *buf, uint64_t size, uint64_t offset, int fd,
                  FILE *lookup) {
    if (!buf || !size || !lookup) return 0;

    /* Read block size from lookup file. */
    uint64_t blockSize;
    if (fseek(lookup, 0, SEEK_SET) < 0) {
        fprintf(stderr,
                "mgz_read: failed to seek to the beginning of lookup "
                "file.\n");
        return 0;
    }
    if (fread(&blockSize, sizeof(uint64_t), 1, lookup) != 1) {
        fprintf(stderr, "mgz_read: failed to read block size from lookup.\n");
        return 0;
    }

    /* Seek to the correct location. */
    uint64_t block = offset / blockSize;
    off_t into = offset % blockSize;
    if (fseek(lookup, block * sizeof(uint64_t), SEEK_CUR) < 0) {
        fprintf(stderr,
                "mgz_read: failed to seek to the given block in lookup "
                "file.\n");
        return 0;
    }
    uint64_t gzOff;
    if (fread(&gzOff, sizeof(uint64_t), 1, lookup) != 1) {
        fprintf(stderr, "mgz_read: failed to read gz offset from lookup.\n");
        return 0;
    }

    if (lseek(fd, gzOff, SEEK_SET) != (off_t)gzOff) {
        fprintf(stderr, "mgz_read: lseek failed.\n");
        return 0;
    }
    int gzfd = dup(fd);
    if (gzfd == -1) {
        fprintf(stderr, "mgz_read: failed to dup() fd.\n");
        return 0;
    }
    gzFile archive = gzdopen(gzfd, "rb");
    if (!archive) {
        fprintf(stderr, "mgz_read: failed to gzdopen() the new fd.\n");
        return 0;
    }
    if (gzseek(archive, into, SEEK_CUR) != into) {
        fprintf(stderr,
                "mgz_read: gzseek not returning an offset equal to "
                "into.\n");
    }

    /* Read the data. */
    if (gz64_read(archive, buf, size) <= 0) {
        fprintf(stderr, "mgz_read: failed to gz64_read().\n");
        gzclose(archive);
        return 0;
    }

    gzclose(archive);
    return size;
}
