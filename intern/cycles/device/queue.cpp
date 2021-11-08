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

#include "device/queue.h"

#include "util/algorithm.h"
#include "util/log.h"
#include "util/time.h"

#include <iomanip>

CCL_NAMESPACE_BEGIN

DeviceQueue::DeviceQueue(Device *device)
    : device(device), last_kernels_enqueued_(0), last_sync_time_(0.0)
{
  DCHECK_NE(device, nullptr);
}

DeviceQueue::~DeviceQueue()
{
  if (VLOG_IS_ON(3)) {
    /* Print kernel execution times sorted by time. */
    vector<pair<DeviceKernelMask, double>> stats_sorted;
    for (const auto &stat : stats_kernel_time_) {
      stats_sorted.push_back(stat);
    }

    sort(stats_sorted.begin(),
         stats_sorted.end(),
         [](const pair<DeviceKernelMask, double> &a, const pair<DeviceKernelMask, double> &b) {
           return a.second > b.second;
         });

    VLOG(3) << "GPU queue stats:";
    for (const auto &[mask, time] : stats_sorted) {
      VLOG(3) << "  " << std::setfill(' ') << std::setw(10) << std::fixed << std::setprecision(5)
              << std::right << time << "s: " << device_kernel_mask_as_string(mask);
    }
  }
}

void DeviceQueue::debug_init_execution()
{
  if (VLOG_IS_ON(3)) {
    last_sync_time_ = time_dt();
  }

  last_kernels_enqueued_ = 0;
}

void DeviceQueue::debug_enqueue(DeviceKernel kernel, const int work_size)
{
  if (VLOG_IS_ON(3)) {
    VLOG(4) << "GPU queue launch " << device_kernel_as_string(kernel) << ", work_size "
            << work_size;
  }

  last_kernels_enqueued_ |= (uint64_t(1) << (uint64_t)kernel);
}

void DeviceQueue::debug_synchronize()
{
  if (VLOG_IS_ON(3)) {
    const double new_time = time_dt();
    const double elapsed_time = new_time - last_sync_time_;
    VLOG(4) << "GPU queue synchronize, elapsed " << std::setw(10) << elapsed_time << "s";

    stats_kernel_time_[last_kernels_enqueued_] += elapsed_time;

    last_sync_time_ = new_time;
  }

  last_kernels_enqueued_ = 0;
}

string DeviceQueue::debug_active_kernels()
{
  return device_kernel_mask_as_string(last_kernels_enqueued_);
}

CCL_NAMESPACE_END
