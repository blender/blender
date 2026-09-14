/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <climits>
#include <iomanip>

#include "device/device.h"
#include "device/kernel.h"
#include "device/queue.h"

#include "util/algorithm.h"
#include "util/log.h"
#include "util/string.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

DeviceQueue::DeviceQueue(Device *device) : device(device)
{
  DCHECK_NE(device, nullptr);
  is_per_kernel_performance_ = getenv("CYCLES_DEBUG_PER_KERNEL_PERFORMANCE");
}

static void env_override(const char *name, int &value)
{
  if (const char *str = getenv(name)) {
    value = max(atoi(str), 0);
  }
}

int DeviceQueue::num_concurrent_states(const size_t state_size) const
{
  ConcurrentStatesParams params = concurrent_states_params();
  env_override("CYCLES_CONCURRENT_STATES_BASELINE", params.baseline);
  env_override("CYCLES_CONCURRENT_STATES_MIN", params.min);
  env_override("CYCLES_CONCURRENT_STATES_MAX", params.max);
  env_override("CYCLES_CONCURRENT_STATES_GROW_PERCENT", params.grow_percent);
  env_override("CYCLES_CONCURRENT_STATES_RESERVE_PERCENT", params.reserve_percent);

  const int64_t baseline = params.baseline;
  const int64_t min_num_states = params.min;
  const int64_t max_num_states = max(params.min, params.max);
  int64_t num_states = baseline;

  /* Adjust to available memory, unless the number of states is fixed. */
  if (min_num_states < max_num_states) {
    size_t total_memory = 0;
    size_t free_memory = 0;
    get_memory_info(total_memory, free_memory);

    const size_t reserve_memory = total_memory * size_t(params.reserve_percent) / 100;
    const size_t budget_memory = free_memory - std::min(reserve_memory, free_memory);

    /* Round down to a multiple of 65536 so sort partitions divide evenly. */
    size_t fit_num_states = budget_memory / state_size;
    if (fit_num_states > baseline) {
      fit_num_states = round_down(fit_num_states * size_t(params.grow_percent) / 100, 65536);
      num_states = std::max(int64_t(fit_num_states), baseline);
    }
    else {
      num_states = round_down(fit_num_states, 65536);
    }

    num_states = std::clamp(num_states, min_num_states, max_num_states);

    if (num_states != baseline) {
      LOG_INFO << "Adjusted concurrent states to available memory: " << baseline << " -> "
               << num_states << " (" << string_human_readable_size(num_states * state_size)
               << " of " << string_human_readable_size(free_memory) << " free)";
    }
  }

  if (const char *factor_str = getenv("CYCLES_CONCURRENT_STATES_FACTOR")) {
    const float factor = float(atof(factor_str));
    if (factor != 0.0f) {
      num_states = std::max(int64_t(double(num_states) * factor), int64_t(1024));
    }
    else {
      LOG_TRACE << "CYCLES_CONCURRENT_STATES_FACTOR evaluated to 0";
    }
  }

  num_states = std::min(num_states, int64_t(INT_MAX));

  LOG_TRACE << "GPU queue concurrent states: " << num_states << ", using up to "
            << string_human_readable_size(num_states * state_size);

  return num_states;
}

DeviceQueue::~DeviceQueue()
{
  if (LOG_IS_ON(LOG_LEVEL_TRACE)) {
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

    LOG_TRACE << "GPU queue stats:";
    double total_time = 0.0;
    for (const auto &[mask, time] : stats_sorted) {
      total_time += time;
      LOG_TRACE << "  " << std::setfill(' ') << std::setw(10) << std::fixed << std::setprecision(5)
                << std::right << time << "s: " << device_kernel_mask_as_string(mask);
    }

    if (is_per_kernel_performance_) {
      LOG_TRACE << "GPU queue total time: " << std::fixed << std::setprecision(5) << total_time;
    }
  }
}

void DeviceQueue::debug_init_execution()
{
  if (LOG_IS_ON(LOG_LEVEL_TRACE)) {
    last_sync_time_ = time_dt();
  }

  last_kernels_enqueued_.reset();
}

void DeviceQueue::debug_enqueue_begin(DeviceKernel kernel, const int work_size)
{
  if (LOG_IS_ON(LOG_LEVEL_TRACE)) {
    LOG_TRACE << "GPU queue launch " << device_kernel_as_string(kernel) << ", work_size "
              << work_size;
  }

  last_kernels_enqueued_.set(kernel, true);
}

void DeviceQueue::debug_enqueue_end()
{
  if (LOG_IS_ON(LOG_LEVEL_TRACE) && is_per_kernel_performance_) {
    synchronize();
  }
}

void DeviceQueue::debug_synchronize()
{
  if (LOG_IS_ON(LOG_LEVEL_TRACE)) {
    const double new_time = time_dt();
    const double elapsed_time = new_time - last_sync_time_;
    LOG_TRACE << "GPU queue synchronize, elapsed " << std::setw(10) << elapsed_time << "s";

    /* There is no sense to have an entries in the performance data
     * container without related kernel information. */
    if (last_kernels_enqueued_.any()) {
      stats_kernel_time_[last_kernels_enqueued_] += elapsed_time;
    }

    last_sync_time_ = new_time;
  }

  last_kernels_enqueued_.reset();
}

string DeviceQueue::debug_active_kernels()
{
  return device_kernel_mask_as_string(last_kernels_enqueued_);
}

CCL_NAMESPACE_END
