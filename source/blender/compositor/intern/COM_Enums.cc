/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2021, Blender Foundation.
 */

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
