/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_defines.h"

#include <ostream>

struct rcti;

namespace blender::compositor {

/**
 * \brief Possible quality settings
 * \see CompositorContext.quality
 * \ingroup Execution
 */
enum class eCompositorQuality {
  /** \brief High quality setting */
  High = 0,
  /** \brief Medium quality setting */
  Medium = 1,
  /** \brief Low quality setting */
  Low = 2,
};

/**
 * \brief Possible priority settings
 * \ingroup Execution
 */
enum class eCompositorPriority {
  /** \brief High quality setting */
  High = 2,
  /** \brief Medium quality setting */
  Medium = 1,
  /** \brief Low quality setting */
  Low = 0,
};

/**
 * \brief the execution state of a chunk in an ExecutionGroup
 * \ingroup Execution
 */
enum class eWorkPackageState {
  /**
   * \brief chunk is not yet scheduled
   */
  NotScheduled = 0,
  /**
   * \brief chunk is scheduled, but not yet executed
   */
  Scheduled = 1,
  /**
   * \brief chunk is executed.
   */
  Executed = 2,
};

/**
 * \brief Work type to execute.
 * \ingroup Execution
 */
enum class eWorkPackageType {
  /**
   * \brief Executes an execution group tile.
   */
  Tile = 0,
  /**
   * \brief Executes a custom function.
   */
  CustomFunction = 1
};

enum class PixelSampler {
  Nearest = 0,
  Bilinear = 1,
  Bicubic = 2,
};
void expand_area_for_sampler(rcti &area, PixelSampler sampler);

std::ostream &operator<<(std::ostream &os, const eCompositorPriority &priority);
std::ostream &operator<<(std::ostream &os, const eWorkPackageState &execution_state);

}  // namespace blender::compositor
