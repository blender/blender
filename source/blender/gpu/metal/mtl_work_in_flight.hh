/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_work_in_flight.hh"

#include "gpu_context_private.hh"

#include "mtl_context.hh"

namespace blender::gpu {

class MTLWorkInFlight : public WorkInFlight {
  id<MTLSharedEvent> gpu_fence_ = nil;
  size_t work_index_ = 0;
  Vector<uint64_t> async_timeline_values_;
  uint64_t current_signal_value_ = 0;

 public:
  MTLWorkInFlight(unsigned int max_in_flight);
  ~MTLWorkInFlight() override;

  void reset() override;
  void begin_work() override;
  void end_work() override;
};

}  // namespace blender::gpu
