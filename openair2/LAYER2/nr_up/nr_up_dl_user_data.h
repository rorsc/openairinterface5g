/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_FLOW_CONTROL_H
#define NR_UP_FLOW_CONTROL_H

#include "nr_up.h"
#include "nr_up_ddds.h"

nr_up_flow_control_action_t nr_up_flow_control_precheck(const nr_up_dl_user_data_req_t *req);
void nr_up_dl_user_data(const nr_up_dl_user_data_req_t *req);

#endif /* NR_UP_FLOW_CONTROL_H */
