/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_vector.hh"

#include "GPU_work_in_flight.hh"

#include <epoxy/gl.h>

namespace blender::gpu {

class GLWorkInFlight : public WorkInFlight {
  size_t work_index_ = 0;
  Vector<GLsync> async_fences_;
  void delete_fences();

 public:
  GLWorkInFlight(unsigned int max_in_flight);
  ~GLWorkInFlight() override;

  void reset() override;
  void begin_work() override;
  void end_work() override;
};

}  // namespace blender::gpu
