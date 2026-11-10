/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "nr_rlc_sdu.h"

#include <stdlib.h>
#include <string.h>

#include "LOG/log.h"
#include "common/utils/assertions.h"
#include "common/utils/mempool.h"

/* maximum SDU size: a 1500-byte IP packet with PDCP header and MAC-I */
#define NR_RLC_SDU_POOL_MAX_DATA (1500 + 3 + 4)

/* An SDU header with its first segment: STATUS PDU processing walks the
 * segments and their headers, which are next to each other this way, and an
 * SDU that is not resegmented takes one block for both. */
typedef struct {
  nr_rlc_sdu_segment_t first;
  nr_rlc_sdu_t sdu;
} sdu_block_t;

struct nr_rlc_sdu_pools_s {
  int max_sdus;
  /* headers with first segment, and data separately: STATUS PDU processing
   * does not touch the data, so the headers are dense this way */
  mempool_t sdus;
  mempool_t data;
  /* segments from resegmentation */
  mempool_t segments;
};

nr_rlc_sdu_pools_t *nr_rlc_new_sdu_pools(int max_sdus)
{
  nr_rlc_sdu_pools_t *p = calloc_or_fail(1, sizeof(*p));
  p->max_sdus = max_sdus;
  mp_init(&p->sdus, sizeof(sdu_block_t), max_sdus);
  mp_init(&p->data, NR_RLC_SDU_POOL_MAX_DATA, max_sdus);
  mp_init(&p->segments, sizeof(nr_rlc_sdu_segment_t), max_sdus);
  return p;
}

void nr_rlc_delete_sdu_pools(nr_rlc_sdu_pools_t *p)
{
  mp_destroy(&p->sdus);
  mp_destroy(&p->data);
  mp_destroy(&p->segments);
  free(p);
}

nr_rlc_sdu_segment_t *nr_rlc_alloc_sdu_segment(nr_rlc_sdu_pools_t *p)
{
  nr_rlc_sdu_segment_t *s = mp_alloc(&p->segments);
  AssertFatal(s != NULL, "out of SDU segments (%d)\n", p->max_sdus);
  return s;
}

nr_rlc_sdu_segment_t *nr_rlc_new_sdu(nr_rlc_sdu_pools_t *pools, char *buffer, int size, int upper_layer_id)
{
  AssertFatal(size <= NR_RLC_SDU_POOL_MAX_DATA, "SDU of %d bytes, maximum %d\n", size, NR_RLC_SDU_POOL_MAX_DATA);
  sdu_block_t *b = mp_alloc(&pools->sdus);
  if (b == NULL)
    return NULL;
  char *data = mp_alloc(&pools->data);
  DevAssert(data != NULL); // as many blocks as headers
  memcpy(data, buffer, size);
  b->sdu = (nr_rlc_sdu_t){.sn = -1, /* set later */
                          .upper_layer_id = upper_layer_id,
                          .data = data,
                          .size = size,
                          .retx_count = -1,
                          .ref_count = 1};
  b->first = (nr_rlc_sdu_segment_t){.sdu = &b->sdu, .size = size, .so = 0, .is_first = 1, .is_last = 1, .next = NULL};
  return &b->first;
}

int nr_rlc_free_sdu_segment(nr_rlc_sdu_pools_t *pools, nr_rlc_sdu_segment_t *sdu)
{
  int ret = 0;
  sdu_block_t *b = (sdu_block_t *)((char *)sdu->sdu - offsetof(sdu_block_t, sdu));
  /* the first segment is freed with its SDU */
  if (sdu != &b->first)
    mp_free(&pools->segments, sdu);

  b->sdu.free_count++;
  if (b->sdu.free_count == b->sdu.ref_count) {
    mp_free(&pools->data, b->sdu.data);
    mp_free(&pools->sdus, b);
    ret = 1;
  }

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
