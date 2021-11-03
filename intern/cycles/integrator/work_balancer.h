/*
 * Copyright 2011-2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
