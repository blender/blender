/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class ConvertColorProfileOperation : public NodeOperation {
 private:
  /**
   * \brief color profile where to convert from
   */
  int from_profile_;

  /**
   * \brief color profile where to convert to
   */
  int to_profile_;

  /**
   * \brief is color predivided
   */
  bool predivided_;

 public:
  /**
   * Default constructor
   */
  ConvertColorProfileOperation();

  void set_from_color_profile(int color_profile)
  {
    from_profile_ = color_profile;
  }
  void set_to_color_profile(int color_profile)
  {
    to_profile_ = color_profile;
  }
  void set_predivided(bool predivided)
  {
    predivided_ = predivided;
  }
};

}  // namespace blender::compositor
