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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_QualityStepHelper.h"

namespace blender::compositor {

QualityStepHelper::QualityStepHelper()
{
  quality_ = eCompositorQuality::High;
  step_ = 1;
  offsetadd_ = 4;
}

void QualityStepHelper::init_execution(QualityHelper helper)
{
  switch (helper) {
    case COM_QH_INCREASE:
      switch (quality_) {
        case eCompositorQuality::High:
        default:
          step_ = 1;
          offsetadd_ = 1;
          break;
        case eCompositorQuality::Medium:
          step_ = 2;
          offsetadd_ = 2;
          break;
        case eCompositorQuality::Low:
          step_ = 3;
          offsetadd_ = 3;
          break;
      }
      break;
    case COM_QH_MULTIPLY:
      switch (quality_) {
        case eCompositorQuality::High:
        default:
          step_ = 1;
          offsetadd_ = 4;
          break;
        case eCompositorQuality::Medium:
          step_ = 2;
          offsetadd_ = 8;
          break;
        case eCompositorQuality::Low:
          step_ = 4;
          offsetadd_ = 16;
          break;
      }
      break;
  }
}

}  // namespace blender::compositor
