/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef MEMPOOL_H_
#define MEMPOOL_H_

#include <stddef.h>
#include <pthread.h>

union block;
typedef union block block_t;
typedef struct mempool {
  block_t *freelist;
  block_t *data;
  pthread_mutex_t mtx;
  size_t block_size; /* power of two */
  size_t n_blocks;
  size_t n_used; /* blocks [0, n_used) have been handed out at least once */
  size_t alloc;
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
