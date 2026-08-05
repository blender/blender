/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_vector.hh"

#include "GPU_work_in_flight.hh"

#include "gpu_context_private.hh"
#include "vk_common.hh"

namespace blender::gpu {

class VKWorkInFlight : public WorkInFlight {
  size_t work_index_ = 0;
  Vector<TimelineValue> async_timeline_values_;

 public:
  VKWorkInFlight(unsigned int max_in_flight);
  ~VKWorkInFlight() override = default;

  void reset() override;
  void begin_work() override;
  void end_work() override;
};

}  // namespace blender::gpu
