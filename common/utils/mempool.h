/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef MEMPOOL_H_
#define MEMPOOL_H_

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

/* Fixed-size block pool. Thread-safe and lock-free: the free blocks form a
 * stack whose head is updated with compare-and-swap. */
typedef struct mempool {
  _Atomic uint64_t head; /* (ABA tag << 32) | (index of top free block + 1), 0: empty */
  _Atomic uint32_t *next; /* next[i]: index + 1 of the free block below block i */
  uint8_t *data;
  size_t block_size; /* power of two */
  unsigned shift; /* log2(block_size) */
  size_t n_blocks;
  _Atomic size_t n_used; /* blocks [0, n_used) have been handed out at least once */
} mempool_t;

/* block_size is rounded up to a power of two. Memory is only touched when
 * blocks are handed out. */
void mp_init(mempool_t *mp, size_t block_size, size_t n_blocks);
void mp_destroy(mempool_t *mp);

void *mp_alloc(mempool_t *mp);
void mp_free(mempool_t *mp, void *mem);

size_t mp_multi_alloc(mempool_t *mp, size_t n, void *data[n]);
void mp_multi_free(mempool_t *mp, size_t n, void *data[n]);

#endif /* MEMPOOL_H_ */
