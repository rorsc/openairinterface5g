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

/* The free list links live in mp->next, outside the blocks: a pop may read the
 * link of a block that another thread pops concurrently (its CAS then fails),
 * which must not touch the block itself (it is poisoned for ASAN, or in use).
 * The tag in the upper half of mp->head changes on every update, against ABA. */
#define HEAD(tag, idx1) (((uint64_t)(tag) << 32) | (idx1))
#define HEAD_TAG(h) ((uint32_t)((h) >> 32))
#define HEAD_IDX1(h) ((uint32_t)(h))

static uint32_t idx1_of(const mempool_t *mp, const void *mem)
{
  uint64_t off = (uintptr_t)mem - (uintptr_t)mp->data;
  // check is on memblock boundary
  mempool_assert((off & (mp->block_size - 1)) == 0);
  // check is within range
  uint64_t idx = off >> mp->shift;
  mempool_assert(idx < mp->n_blocks);
  return idx + 1;
}

void mp_init(mempool_t *mp, size_t block_size, size_t n_blocks)
{
  mempool_assert(mp != NULL);
  mempool_assert(n_blocks > 0 && n_blocks < UINT32_MAX);

  if (block_size < 8)
    block_size = 8;
  /* round up to a power of two */
  block_size = (size_t)1 << (64 - __builtin_clzl(block_size - 1));

  memset(mp, 0, sizeof(*mp));
  /* large: malloc() maps pages, backed only when touched */
  mp->data = malloc(n_blocks * block_size);
  mempool_assert(mp->data != NULL);
  mp->next = malloc(n_blocks * sizeof(*mp->next));
  mempool_assert(mp->next != NULL);
  mp->block_size = block_size;
  mp->shift = __builtin_ctzl(block_size);
  mp->n_blocks = n_blocks;
  /* the free list starts empty: blocks never handed out come from n_used, so
   * that mp_init() does not touch all memory */
  atomic_init(&mp->head, 0);
  atomic_init(&mp->n_used, 0);

  ASAN_POISON_MEMORY_REGION(mp->data, n_blocks * block_size);
}

void mp_destroy(mempool_t *mp)
{
  /* all blocks must have been freed (and none twice, which would loop) */
  const size_t n_used = atomic_load(&mp->n_used);
  size_t n_free = 0;
  uint32_t i = HEAD_IDX1(atomic_load(&mp->head));
  while (i != 0 && n_free <= n_used) {
    n_free++;
    i = atomic_load_explicit(&mp->next[i - 1], memory_order_relaxed);
  }
  mempool_assert(n_free == n_used);
  ASAN_UNPOISON_MEMORY_REGION(mp->data, mp->n_blocks * mp->block_size);
  free(mp->data);
  free(mp->next);
  memset(mp, 0, sizeof(*mp));
}

void *mp_alloc(mempool_t *mp)
{
  uint64_t old = atomic_load_explicit(&mp->head, memory_order_acquire);
  uint64_t new;
  uint32_t idx1;
  do {
    idx1 = HEAD_IDX1(old);
    if (idx1 == 0)
      break;
    uint32_t next = atomic_load_explicit(&mp->next[idx1 - 1], memory_order_relaxed);
    new = HEAD(HEAD_TAG(old) + 1, next);
  } while (!atomic_compare_exchange_weak_explicit(&mp->head, &old, new, memory_order_acquire, memory_order_acquire));
  if (idx1 == 0) {
    /* free list empty: take a block never handed out, if any */
    size_t used = atomic_load_explicit(&mp->n_used, memory_order_relaxed);
    do {
      if (used == mp->n_blocks)
        return NULL;
    } while (!atomic_compare_exchange_weak_explicit(&mp->n_used, &used, used + 1, memory_order_relaxed, memory_order_relaxed));
    idx1 = used + 1;
  }
  uint8_t *block = mp->data + (size_t)(idx1 - 1) * mp->block_size;
  ASAN_UNPOISON_MEMORY_REGION(block, mp->block_size);
  return block;
}

/* push the chain first -> ... -> last, already linked in mp->next */
static void push_chain(mempool_t *mp, uint32_t first, uint32_t last)
{
  uint64_t old = atomic_load_explicit(&mp->head, memory_order_relaxed);
  do {
    atomic_store_explicit(&mp->next[last - 1], HEAD_IDX1(old), memory_order_relaxed);
  } while (!atomic_compare_exchange_weak_explicit(&mp->head,
                                                  &old,
                                                  HEAD(HEAD_TAG(old) + 1, first),
                                                  memory_order_release,
                                                  memory_order_relaxed));
}

void mp_free(mempool_t *mp, void *mem)
{
  uint32_t idx1 = idx1_of(mp, mem);
  ASAN_POISON_MEMORY_REGION(mem, mp->block_size);
  push_chain(mp, idx1, idx1);
}

size_t mp_multi_alloc(mempool_t *mp, size_t n, void *data[n])
{
  size_t ret = 0;
  while (ret < n && (data[ret] = mp_alloc(mp)) != NULL)
    ret++;
  /* set rest to NULL */
  for (size_t i = ret; i < n; ++i)
    data[i] = NULL;
  return ret;
}

void mp_multi_free(mempool_t *mp, size_t n, void *data[n])
{
  /* link the blocks locally, then push them with a single CAS */
  uint32_t first = 0, last = 0;
  for (size_t i = 0; i < n; ++i) {
    if (!data[i])
      continue; // e.g. could not allocate all in mp_multi_alloc
    uint32_t idx1 = idx1_of(mp, data[i]);
    ASAN_POISON_MEMORY_REGION(data[i], mp->block_size);
    if (last != 0)
      atomic_store_explicit(&mp->next[last - 1], idx1, memory_order_relaxed);
    else
      first = idx1;
    last = idx1;
  }
  if (first != 0)
    push_chain(mp, first, last);
}
