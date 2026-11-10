/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <assert.h>
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


int main(void)
{
  simple();
}
