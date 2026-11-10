/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_rlc_sdu.h"

#include <stdlib.h>
#include <string.h>

#include "LOG/log.h"
#include "common/utils/assertions.h"
#include "common/utils/mempool.h"

/* maximum SDU size: a 1500-byte IP packet with PDCP header and MAC-I */
#define NR_RLC_SDU_POOL_MAX_DATA (1500 + 3 + 4)

struct nr_rlc_sdu_pools_s {
  /* at most max_sdus SDUs are stored, each with a first segment; the other
   * max_sdus segments are for resegmentation */
  int max_sdus;
  int n_sdus;
  mempool_t segments;
  /* headers and data separately: STATUS PDU processing walks the headers,
   * which are dense this way, and does not touch the data */
  mempool_t sdus;
  mempool_t data;
};

nr_rlc_sdu_pools_t *nr_rlc_new_sdu_pools(int max_sdus)
{
  nr_rlc_sdu_pools_t *p = calloc_or_fail(1, sizeof(*p));
  p->max_sdus = max_sdus;
  mp_init(&p->segments, sizeof(nr_rlc_sdu_segment_t), 2 * max_sdus);
  mp_init(&p->sdus, sizeof(nr_rlc_sdu_t), max_sdus);
  mp_init(&p->data, NR_RLC_SDU_POOL_MAX_DATA, max_sdus);
  return p;
}

void nr_rlc_delete_sdu_pools(nr_rlc_sdu_pools_t *p)
{
  mp_destroy(&p->segments);
  mp_destroy(&p->sdus);
  mp_destroy(&p->data);
  free(p);
}

nr_rlc_sdu_segment_t *nr_rlc_alloc_sdu_segment(nr_rlc_sdu_pools_t *p)
{
  nr_rlc_sdu_segment_t *s = mp_alloc(&p->segments);
  AssertFatal(s != NULL, "out of SDU segments (%d)\n", 2 * p->max_sdus);
  return s;
}

nr_rlc_sdu_segment_t *nr_rlc_new_sdu(nr_rlc_sdu_pools_t *pools, char *buffer, int size, int upper_layer_id)
{
  AssertFatal(size <= NR_RLC_SDU_POOL_MAX_DATA, "SDU of %d bytes, maximum %d\n", size, NR_RLC_SDU_POOL_MAX_DATA);
  if (pools->n_sdus == pools->max_sdus)
    return NULL;
  nr_rlc_sdu_t *sdu = mp_alloc(&pools->sdus);
  char *data = mp_alloc(&pools->data);
  DevAssert(sdu != NULL && data != NULL); // as many blocks as SDUs
  pools->n_sdus++;
  memcpy(data, buffer, size);
  *sdu = (nr_rlc_sdu_t){.sn = -1, /* set later */
                        .upper_layer_id = upper_layer_id,
                        .data = data,
                        .size = size,
                        .retx_count = -1,
                        .ref_count = 1};

  nr_rlc_sdu_segment_t *ret = nr_rlc_alloc_sdu_segment(pools);
  *ret = (nr_rlc_sdu_segment_t){.sdu = sdu, .size = size, .so = 0, .is_first = 1, .is_last = 1, .next = NULL};
  return ret;
}

int nr_rlc_free_sdu_segment(nr_rlc_sdu_pools_t *pools, nr_rlc_sdu_segment_t *sdu)
{
  int ret = 0;

  sdu->sdu->free_count++;
  if (sdu->sdu->free_count == sdu->sdu->ref_count) {
    mp_free(&pools->data, sdu->sdu->data);
    mp_free(&pools->sdus, sdu->sdu);
    pools->n_sdus--;
    ret = 1;
  }
  mp_free(&pools->segments, sdu);

  return ret;
}

void nr_rlc_sdu_segment_list_append(nr_rlc_sdu_segment_t **list,
                                    nr_rlc_sdu_segment_t **end,
                                    nr_rlc_sdu_segment_t *sdu)
{
  if (*list == NULL) {
    *list = sdu;
    *end = sdu;
    return;
  }

  (*end)->next = sdu;
  *end = sdu;
}

void nr_rlc_free_sdu_segment_list(nr_rlc_sdu_pools_t *pools, nr_rlc_sdu_segment_t *l)
{
  nr_rlc_sdu_segment_t *cur;

  while (l != NULL) {
    cur = l;
    l = l->next;
    nr_rlc_free_sdu_segment(pools, cur);
  }
}
