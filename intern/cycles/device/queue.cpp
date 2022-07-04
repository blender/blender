/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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
  if (VLOG_DEVICE_STATS_IS_ON) {
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

    VLOG_DEVICE_STATS << "GPU queue stats:";
    for (const auto &[mask, time] : stats_sorted) {
      VLOG_DEVICE_STATS << "  " << std::setfill(' ') << std::setw(10) << std::fixed
                        << std::setprecision(5) << std::right << time
                        << "s: " << device_kernel_mask_as_string(mask);
    }
  }
}

void DeviceQueue::debug_init_execution()
{
  if (VLOG_DEVICE_STATS_IS_ON) {
    last_sync_time_ = time_dt();
  }

  last_kernels_enqueued_ = 0;
}

void DeviceQueue::debug_enqueue(DeviceKernel kernel, const int work_size)
{
  if (VLOG_DEVICE_STATS_IS_ON) {
    VLOG_DEVICE_STATS << "GPU queue launch " << device_kernel_as_string(kernel) << ", work_size "
                      << work_size;
  }

  last_kernels_enqueued_ |= (uint64_t(1) << (uint64_t)kernel);
}

void DeviceQueue::debug_synchronize()
{
  if (VLOG_DEVICE_STATS_IS_ON) {
    const double new_time = time_dt();
    const double elapsed_time = new_time - last_sync_time_;
    VLOG_DEVICE_STATS << "GPU queue synchronize, elapsed " << std::setw(10) << elapsed_time << "s";

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
