/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../mempool.h"

void simple(void)
{
  size_t size = 16, n = 3;
  mempool_t mp;
  mp_init(&mp, size, n);
  void *data[n];
  for (size_t i = 0; i < n; ++i) {
    data[i] = mp_alloc(&mp);
    assert(data[i]);
    printf("i=%lu => %p\n", i, data[i]);
  }
  ptrdiff_t diff = data[1] - data[0];
  assert(diff == size);

  void *nomem = mp_alloc(&mp);
  assert(nomem == NULL);

  for (size_t i = 0; i < n; ++i) {
    mp_free(&mp, data[i]);
  }
  mp_destroy(&mp);
}

/* block sizes are rounded up to a power of two; freed blocks are reused */
void rounding_reuse(void)
{
  size_t n = 4;
  mempool_t mp;
  mp_init(&mp, 1580, n);
  void *data[n];
  assert(mp_multi_alloc(&mp, n, data) == n);
  assert(mp_alloc(&mp) == NULL);
  for (size_t i = 1; i < n; ++i)
    assert((char *)data[i] - (char *)data[i - 1] == 2048);
  mp_free(&mp, data[1]);
  assert(mp_alloc(&mp) == data[1]);
  assert(mp_alloc(&mp) == NULL);
  mp_multi_free(&mp, n, data);
  assert(mp_multi_alloc(&mp, n, data) == n);
  mp_multi_free(&mp, n, data);
  mp_destroy(&mp);
}

/* a large pool is cheap as long as it is not used (would take long if
 * mp_init() touched all 1 GiB) */
void large_unused(void)
{
  mempool_t mp;
  for (int i = 0; i < 10; ++i) {
    mp_init(&mp, 2048, 1 << 19);
    void *b = mp_alloc(&mp);
    assert(b);
    mp_free(&mp, b);
    mp_destroy(&mp);
  }
}

/* threads concurrently allocate and free; a block owned by two threads at once
 * (e.g., ABA on the free list) shows as a corrupted owner tag */
#define STRESS_THREADS 8
#define STRESS_BLOCKS 64
#define STRESS_ITER 200000
typedef struct {
  mempool_t *mp;
  uintptr_t id;
  atomic_size_t *n_alloc;
} stress_arg_t;

static void *stress(void *arg)
{
  stress_arg_t *a = arg;
  uintptr_t *held[STRESS_BLOCKS / STRESS_THREADS * 2];
  size_t n_held = 0;
  for (size_t i = 0; i < STRESS_ITER; ++i) {
    if (n_held < sizeof(held) / sizeof(held[0]) && (i % 3 != 2 || n_held == 0)) {
      uintptr_t *b = mp_alloc(a->mp);
      if (!b)
        continue;
      atomic_fetch_add(a->n_alloc, 1);
      *b = a->id;
      held[n_held++] = b;
    } else {
      uintptr_t *b = held[--n_held];
      assert(*b == a->id);
      mp_free(a->mp, b);
    }
    for (size_t j = 0; j < n_held; ++j)
      assert(*held[j] == a->id);
  }
  while (n_held > 0)
    mp_free(a->mp, held[--n_held]);
  return NULL;
}

void concurrent(void)
{
  mempool_t mp;
  mp_init(&mp, sizeof(uintptr_t), STRESS_BLOCKS);
  atomic_size_t n_alloc = 0;
  pthread_t t[STRESS_THREADS];
  stress_arg_t a[STRESS_THREADS];
  for (int i = 0; i < STRESS_THREADS; ++i) {
    a[i] = (stress_arg_t){.mp = &mp, .id = i + 1, .n_alloc = &n_alloc};
    assert(pthread_create(&t[i], NULL, stress, &a[i]) == 0);
  }
  for (int i = 0; i < STRESS_THREADS; ++i)
    pthread_join(t[i], NULL);
  printf("concurrent: %zu allocations\n", atomic_load(&n_alloc));
  mp_destroy(&mp); // asserts that all blocks are free
}

int main(void)
{
  simple();
  rounding_reuse();
  large_unused();
  concurrent();
}
