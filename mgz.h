#ifndef MGZ_H
#define MGZ_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    void *out;
    uint64_t size;
    uint64_t *lookup;
    uint64_t nBlocks;
} mgz_res_t;

/**
 * @brief Compresses INSIZE bytes of data from IN using compression
 * level LEVEL and stores the compressed data in a malloc'ed array at
 * *OUT. Assumes OUT points to a valid (void *). Sets *OUT to NULL and
 * returns 0 if INSIZE is 0 or an error occurs. It is the user's
 * responsibility to free() the output buffer at *OUT.
 *
 * @param out pointer to a valid (void *) which will be set to a
 * malloc'ed array storing the compressed result if compression is
 * successful.
 * @param in input buffer.
 * @param inSize size of the input buffer in bytes.
 * @param level compression level which can be any integer from -1
 * to 9. -1 gives zlib's default compression level, 0 gives no
 * compression, 1 gives best speed, and 9 gives best compression.
 * @return The size of the output in bytes or 0 if INSIZE is 0 or
 * an error occurs.
 *
 * @example
 * char in[8] = "abcdefg";
 * void *out;
 * uint64_t outSize = mgz_deflate(&out, in, 8, 9);
 * if (out) {
 *     fwrite(out, 1, outSize, outfile);
 *     free(out);
 * }
 */
uint64_t mgz_deflate(void **out, const void *in, uint64_t inSize,
                     int level);

/**
 * @brief Splits INSIZE bytes of data from IN into blocks of size
 * BLOCKSIZE, compresses each block using compression level LEVEL, and
 * stores the concatenated result to a malloc'ed array at
 * mgz_res_t.out. The return structure contains all zeros if INSIZE
 * is 0 or an error occurs. Otherwise, it is the user's responsibility
 * to free() the output buffer at mgz_res_t.out.
 *
 * If LOOKUP is set to true, the returned mgz_res_t
 * struct will also contain a malloc'ed lookup table at
 * mgz_res_t.lookup; otherwise mgz_res_t.lookup will be set to NULL.
 * It is the user's responsibility to free() the lookup table.
 *
 * The lookup table is an array of offsets of length
 * (mgz_res_t.nBlocks + 1). Each offset indicates the number of bytes
 * that proceeds the given block in the compressed buffer. For
 * example, if there are 3 compressed blocks of sizes 4, 6, and 2,
 * then the lookup array is of length 4 with lookup[0] = 0, lookup[1]
 * = 4, lookup[2] = 10, and lookup[3] = 12. The current version
 * of mgz does not store the last value into a standard lookup file.
 *
 * @param in input buffer.
 * @param inSize size of the input buffer in bytes.
 * @param level compression level which can be any integer from -1
 * to 9. -1 gives zlib's default compression level, 0 gives no
 * compression, 1 gives best speed, and 9 gives best compression.
 * @param blockSize size (in bytes) of each block of raw data. Minimum
 * block size is 16 KiB and no maximum block size is specified. If
 * BLOCKSIZE is smaller than the minimum block size, the minimum block
 * size is used instead. If BLOCKSIZE is set to 0, a default block
 * size of 1 MiB is used.
 * @param lookup lookup table is returned if set to true.
 * @return A mgz_res_t containing the output buffer, the size of the
 * output buffer in bytes, the lookup table if requested, and the
 * number of blocks that raw data was split into. If an error occurred
 * during the compression, the returned structure contains all zeros.
 * Specifically, mgz_res_t.out is guaranteed to be non-NULL if
 * compression is successful, and is guaranteed to be NULL if
 * an error occurred.
 *
 * @example
 * mgz_res_t res =
 *     mgz_parallel_deflate(in, inSize, level, blockSize, true);
 * fwrite(res.out, 1, outSize, outfile);
 * free(res.out);
 * fwrite(&blockSize, sizeof(uint64_t), 1, lookupFile);
 * fwrite(res.lookup, sizeof(uint64_t), res.nBlocks, lookupFile);
 * free(res.lookup);
 */
mgz_res_t mgz_parallel_deflate(const void *in, uint64_t inSize,
                               int level, uint64_t blockSize,
                               bool lookup);

/**
 * @brief Splits INSIZE bytes of data from IN into blocks of size
 * BLOCKSIZE, compresses each block using compression level LEVEL,
 * and writes the concatenated result into OUTFILE assuming
 * OUTFILE points to a valid writable stream.
 * Also writes the lookup table to LOOKUP if LOOKUP is not set to
 * NULL, in which case it is assumed to point to a valid writable
 * stream.
 *
 * @param in input buffer.
 * @param size size of the input buffer in bytes.
 * @param level compression level which can be any integer from -1
 * to 9. -1 gives zlib's default compression level, 0 gives no
 * compression, 1 gives best speed, and 9 gives best compression.
 * @param blockSize size (in bytes) of each block of raw data. Minimum
 * block size is 16 KiB and no maximum block size is specified. If
 * BLOCKSIZE is smaller than the minimum block size, the minimum block
 * size is used instead. If BLOCKSIZE is set to 0, a default block
 * size of 1 MiB is used.
 * @param outfile output file stream to which the compressed data
 * is written.
 * @param lookup lookup file stream to which the lookup table is
 * written, or NULL if no lookup table is needed.
 * @return Size written to OUTFILE in bytes. 0 if SIZE is 0 or an
 * error occurred during compression.
 */
uint64_t mgz_parallel_create(const void *in, uint64_t size, int level,
                             uint64_t blockSize, FILE *outfile,
                             FILE *lookup);

/**
 * @brief Reads SIZE bytes of data into BUF from a gzip file created
 * with mgz, which has file descriptor FD, starting at offset OFFSET
 * using LOOKUP as lookup table. Assumes BUF points to a valid space
 * of size at least SIZE bytes, FD is a valid file descriptor of
 * a valid mgz gzip file, and LOOKUP points to a readable stream that
 * contains the lookup table for the given gzip file.
 *
 * @param buf output buffer.
 * @param size size of data to read from FD in bytes.
 * @param offset offset into FD in bytes.
 * @param fd file descriptor of a mgz gzip file.
 * @param lookup readable stream containing the lookup table for FD.
 * @return Number of bytes read from FD. Returns 0 if size is 0 or
 * an error occurred.
 */
uint64_t mgz_read(void *buf, uint64_t size, uint64_t offset, int fd,
                  FILE *lookup);

#endif  // MGZ_H
