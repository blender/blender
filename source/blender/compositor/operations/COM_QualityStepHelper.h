/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Enums.h"

namespace blender::compositor {

typedef enum QualityHelper {
  COM_QH_INCREASE,
  COM_QH_MULTIPLY,
} QualityHelper;

class QualityStepHelper {
 private:
  eCompositorQuality quality_;
  int step_;
  int offsetadd_;

 protected:
  /**
   * Initialize the execution
   */
  void init_execution(QualityHelper helper);

  inline int get_step() const
  {
    return step_;
  }
  inline int get_offset_add() const
  {
    return offsetadd_;
  }

 public:
  QualityStepHelper();

  void set_quality(eCompositorQuality quality)
  {
    quality_ = quality;
  }
};

}  // namespace blender::compositor
