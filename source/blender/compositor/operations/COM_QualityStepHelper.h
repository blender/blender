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
