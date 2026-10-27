/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef OPENAIRINTERFACE_NFAPI_NR_VNF_H
#define OPENAIRINTERFACE_NFAPI_NR_VNF_H

#include "nfapi_nr_vnf_interface.h"
#include "nfapi_common_vnf.h"

/* NR VNF entry point */
void configure_nr_nfapi_vnf(const char *vnf_addr, uint16_t vnf_p5_port, uint16_t vnf_p7_port);
void stop_nr_nfapi_vnf(void);
nfapi_nr_vnf_config_t *get_nr_config();
vnf_p7_t *get_p7_nr_vnf();
nfapi_nr_vnf_p7_config_t *get_p7_nr_vnf_config();

/* Temporary UL instrumentation of the VNF P7 path.  Only the P7 thread writes it (the P7 receive
 * loop and the callbacks it calls), so no locking.  Printed and reset once per second. */
typedef struct {
  uint64_t n;
  uint64_t ns;
  uint64_t max_ns;
} vnf_dur_t;

enum { VNF_MSG_SLOT, VNF_MSG_RX_DATA, VNF_MSG_CRC, VNF_MSG_UCI, VNF_MSG_OTHER, VNF_MSG_NUM };

typedef struct {
  uint64_t last_print_ns;
  /* P7 thread: time blocked in pselect() vs. everything else */
  uint64_t idle_ns;
  uint64_t busy_ns;
  /* socket */
  uint64_t dgrams;
  uint64_t dgram_bytes;
  uint64_t dgram_max;
  uint64_t recv_ns;
  /* segmentation/reassembly */
  uint64_t seg_rx;
  uint64_t seg_used;
  uint64_t seg_msgs;
  uint64_t seg_msgs_over64k;
  /* complete messages: unpack + callback */
  vnf_dur_t msg[VNF_MSG_NUM];
  /* callbacks only */
  vnf_dur_t sched; /* NR_slot_indication(): scheduler */
  vnf_dur_t send; /* DL_TTI/UL_TTI/TX_DATA/UL_DCI sending */
  vnf_dur_t rx_cb;
  vnf_dur_t crc_cb;
  vnf_dur_t uci_cb;
  /* UL grants the scheduler sent (UL_TTI.request PUSCH PDUs) */
  uint64_t pusch;
  uint64_t pusch_retx;
  uint64_t pusch_bytes;
  /* RX_DATA.indication / CRC.indication; lateness: VNF slot minus indication slot */
  uint64_t rx_ind;
  uint64_t rx_pdus;
  uint64_t rx_len0;
  uint64_t rx_bytes;
  int64_t rx_late_sum;
  int64_t rx_late_max;
  uint64_t crc_ind;
  uint64_t crcs;
  uint64_t crc_fail;
  int64_t crc_late_sum;
  int64_t crc_late_max;
  /* kernel counters, cumulative */
  uint64_t udp_drops;
  uint64_t reasm_fails;
  uint64_t rcvbuf_errors;
} vnf_ul_stats_t;

extern vnf_ul_stats_t vnf_ul_stats;

uint64_t vnf_now_ns(void);
void vnf_dur_add(vnf_dur_t *d, uint64_t ns);
void vnf_ul_stats_maybe_print(int p7_sock);

#endif /* OPENAIRINTERFACE_NFAPI_NR_VNF_H */
