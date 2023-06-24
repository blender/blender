/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
