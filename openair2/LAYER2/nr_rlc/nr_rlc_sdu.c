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

struct nr_rlc_sdu_pools_s {
  /* at most max_sdus SDUs are stored, each with a first segment; the other
   * max_sdus segments are for resegmentation */
  int max_sdus;
  int n_sdus;
  mempool_t segments;
};

nr_rlc_sdu_pools_t *nr_rlc_new_sdu_pools(int max_sdus)
{
  nr_rlc_sdu_pools_t *p = calloc_or_fail(1, sizeof(*p));
  p->max_sdus = max_sdus;
  mp_init(&p->segments, sizeof(nr_rlc_sdu_segment_t), 2 * max_sdus);
  return p;
}

void nr_rlc_delete_sdu_pools(nr_rlc_sdu_pools_t *p)
{
  mp_destroy(&p->segments);
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
  if (pools->n_sdus == pools->max_sdus)
    return NULL;
  pools->n_sdus++;

  /* allocate sdu header and data together */
  nr_rlc_sdu_t *sdu = malloc(sizeof(nr_rlc_sdu_t) + size);
  nr_rlc_sdu_segment_t *ret = nr_rlc_alloc_sdu_segment(pools);

  DevAssert(sdu != NULL);

  /* only memset the header */
  memset(sdu, 0 , sizeof(*sdu));

  sdu->ref_count      = 1;
  sdu->sn             = -1;                 /* set later */
  sdu->upper_layer_id = upper_layer_id;
  sdu->data           = (char*)(sdu + 1);
  memcpy(sdu->data, buffer, size);
  sdu->size           = size;
  sdu->retx_count     = -1;

  *ret = (nr_rlc_sdu_segment_t){.sdu = sdu, .size = size, .so = 0, .is_first = 1, .is_last = 1, .next = NULL};

  return ret;
}

int nr_rlc_free_sdu_segment(nr_rlc_sdu_pools_t *pools, nr_rlc_sdu_segment_t *sdu)
{
  int ret = 0;

  sdu->sdu->free_count++;
  if (sdu->sdu->free_count == sdu->sdu->ref_count) {
    free(sdu->sdu);
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
