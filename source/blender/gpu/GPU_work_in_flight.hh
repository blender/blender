/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <memory>

#include "BLI_assert.hh"

namespace blender::gpu {

class WorkInFlight;

class WorkInFlightDeleter {
 public:
  void operator()(WorkInFlight *work_in_flight);
};

using WorkInFlightPtr = std::unique_ptr<gpu::WorkInFlight, gpu::WorkInFlightDeleter>;

/**
 * Limiter for the maximum amount of work packets simultaneously in flight on the GPU.
 * Base class which is then specialized for each implementation (GL, VK, ...).
 */
class WorkInFlight {
 public:
  static WorkInFlightPtr create(unsigned int max_in_flight);

  virtual ~WorkInFlight() = default;

  /* Reset the internal state. Needs to be called before the first or after the last work packet.
   */
  virtual void reset() = 0;

  /* Needs to be called before beginning a work packet. Blocks if the maximum number of work
   * packets in flight is reached. */
  virtual void begin_work() = 0;

  /* Needs to be called after the end of a work packet. */
  virtual void end_work() = 0;
};

}  // namespace blender::gpu
