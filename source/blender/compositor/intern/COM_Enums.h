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
  High = 2,
  Medium = 1,
  Low = 0,
};

enum class PixelSampler {
  Nearest = 0,
  Bilinear = 1,
  Bicubic = 2,
};
void expand_area_for_sampler(rcti &area, PixelSampler sampler);

std::ostream &operator<<(std::ostream &os, const eCompositorPriority &priority);

}  // namespace blender::compositor
