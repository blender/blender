/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/vector.h"

CCL_NAMESPACE_BEGIN

struct WorkBalanceInfo {
  /* Time spent performing corresponding work. */
  double time_spent = 0;

  /* Average occupancy of the device while performing the work. */
  float occupancy = 1.0f;

  /* Normalized weight, which is ready to be used for work balancing (like calculating fraction of
   * the big tile which is to be rendered on the device). */
  double weight = 1.0;
};

/* Balance work for an initial render integration, before any statistics is known. */
void work_balance_do_initial(vector<WorkBalanceInfo> &work_balance_infos);

/* Rebalance work after statistics has been accumulated.
 * Returns true if the balancing did change. */
bool work_balance_do_rebalance(vector<WorkBalanceInfo> &work_balance_infos);

CCL_NAMESPACE_END
