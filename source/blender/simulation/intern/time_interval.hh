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
 */

#ifndef __SIM_TIME_INTERVAL_HH__
#define __SIM_TIME_INTERVAL_HH__

#include "BLI_utildefines.h"

namespace blender::sim {

/**
 * The start time is inclusive and the end time is exclusive. The duration is zero, the interval
 * describes a single point in time.
 */
class TimeInterval {
 private:
  float start_;
  float duration_;

 public:
  TimeInterval(float start, float duration) : start_(start), duration_(duration)
  {
    BLI_assert(duration_ >= 0.0f);
  }

  float start() const
  {
    return start_;
  }

  float end() const
  {
    return start_ + duration_;
  }

  float duration() const
  {
    return duration_;
  }
};

}  // namespace blender::sim

#endif /* __SIM_TIME_INTERVAL_HH__ */
