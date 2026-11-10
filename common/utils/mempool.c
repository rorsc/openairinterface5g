/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sanitizer/asan_interface.h>

#include "mempool.h"

#define mempool_assert(COND)                                                \
  do {                                                                      \
    if (!(COND)) {                                                          \
      fprintf(stderr, "%s:%d: mempool fatal assert\n", __FILE__, __LINE__); \
      fprintf(stderr, "%s\n", #COND);                                       \
      abort();                                                              \
    }                                                                       \
  } while (0)

/* TODO
 * - growabability?
 * - Asan redzones?
 */

typedef union block {
  union block *next;
  uint8_t data[];
} block_t;

void mp_init(mempool_t *mp, size_t block_size, size_t n_blocks)
{
  mempool_assert(mp != NULL);
  mempool_assert(n_blocks > 0);

  /* important: need min size 8 for asan! */
  if (block_size < 8)
    block_size = 8;
  /* round up to a power of two */
  block_size = (size_t)1 << (64 - __builtin_clzl(block_size - 1));

  memset(mp, 0, sizeof(*mp));
  /* large: malloc() maps pages, backed only when touched */
  mp->data = malloc(n_blocks * block_size);
  mempool_assert(mp->data != NULL);
  mp->block_size = block_size;
  mp->n_blocks = n_blocks;
  /* the free list starts empty: blocks never handed out come from n_used, so
   * that mp_init() does not touch all memory */
  mp->freelist = NULL;
  mp->n_used = 0;
  pthread_mutex_init(&mp->mtx, NULL);

  ASAN_POISON_MEMORY_REGION(mp->data, n_blocks * block_size);
}

void mp_destroy(mempool_t *mp)
{
  mempool_assert(mp->alloc == 0);
  ASAN_UNPOISON_MEMORY_REGION(mp->data, mp->n_blocks * mp->block_size);
  free(mp->data);
  pthread_mutex_destroy(&mp->mtx);
  memset(mp, 0, sizeof(*mp));
}

void *mp_alloc(mempool_t *mp)
{
  void *new;
  mp_multi_alloc(mp, 1, &new);
  return new;
}

void mp_free(mempool_t *mp, void *mem)
{
  mp_multi_free(mp, 1, &mem);
}

size_t mp_multi_alloc(mempool_t *mp, size_t n, void *data[n])
{
  size_t ret = 0;
  mempool_assert(pthread_mutex_lock(&mp->mtx) == 0);
  while (ret < n && (mp->freelist || mp->n_used < mp->n_blocks)) {
    block_t *block = mp->freelist;
    if (block) {
      ASAN_UNPOISON_MEMORY_REGION(block, mp->block_size);
      mp->freelist = block->next;
    } else {
      block = (block_t *)((uint8_t *)mp->data + mp->n_used++ * mp->block_size);
      ASAN_UNPOISON_MEMORY_REGION(block, mp->block_size);
    }
    mp->alloc++;
    mempool_assert(mp->alloc <= mp->n_blocks);
    data[ret++] = block;
  }
  mempool_assert(pthread_mutex_unlock(&mp->mtx) == 0);
  /* set rest to NULL */
  for (size_t i = ret; i < n; ++i)
    data[i] = NULL;

  return ret;
}

void mp_multi_free(mempool_t *mp, size_t n, void *data[n])
{
  mempool_assert(pthread_mutex_lock(&mp->mtx) == 0);
  for (size_t i = 0; i < n; ++i) {
    block_t *block = data[i];
    if (!block)
      continue; // e.g. could not allocate all in mp_multi_alloc
    // check is within range
    uintptr_t off = (uintptr_t)block - (uintptr_t)mp->data;
    mempool_assert(off < mp->n_blocks * mp->block_size);
    // check is on memblock boundary
    mempool_assert((off & (mp->block_size - 1)) == 0);
    block->next = mp->freelist;
    mp->freelist = block;
    ASAN_POISON_MEMORY_REGION(block, mp->block_size);
    mempool_assert(mp->alloc > 0);
    mp->alloc--;
  }
  mempool_assert(pthread_mutex_unlock(&mp->mtx) == 0);
}
