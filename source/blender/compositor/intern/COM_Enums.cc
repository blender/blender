/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_Enums.h"

namespace blender::compositor {

void expand_area_for_sampler(rcti &area, PixelSampler sampler)
{
  switch (sampler) {
    case PixelSampler::Nearest:
      break;
    case PixelSampler::Bilinear:
      area.xmax += 1;
      area.ymax += 1;
      break;
    case PixelSampler::Bicubic:
      area.xmin -= 1;
      area.xmax += 2;
      area.ymin -= 1;
      area.ymax += 2;
      break;
  }
}

std::ostream &operator<<(std::ostream &os, const eCompositorPriority &priority)
{
  switch (priority) {
    case eCompositorPriority::High: {
      os << "Priority::High";
      break;
    }
    case eCompositorPriority::Medium: {
      os << "Priority::Medium";
      break;
    }
    case eCompositorPriority::Low: {
      os << "Priority::Low";
      break;
    }
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const eWorkPackageState &execution_state)
{
  switch (execution_state) {
    case eWorkPackageState::NotScheduled: {
      os << "ExecutionState::NotScheduled";
      break;
    }
    case eWorkPackageState::Scheduled: {
      os << "ExecutionState::Scheduled";
      break;
    }
    case eWorkPackageState::Executed: {
      os << "ExecutionState::Executed";
      break;
    }
  }
  return os;
}

}  // namespace blender::compositor
