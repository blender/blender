/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_math_color.h"

#include "IMB_colormanagement.hh"
#include "IMB_interp.hh"

namespace blender {

void IMB_sampleImageAtLocation(ImBuf *ibuf, float x, float y, float scene_linear_rgb[3])
{
  if (ibuf->float_buffer.data) {
    float rgba[4];
    imbuf::interpolate_nearest_border_fl(ibuf, rgba, x, y);
    premul_to_straight_v4_v4(rgba, rgba);
    copy_v3_v3(scene_linear_rgb, rgba);
  }
  else {
    uchar4 byte_color = imbuf::interpolate_nearest_border_byte(ibuf, x, y);
    rgb_uchar_to_float(scene_linear_rgb, byte_color);
    IMB_colormanagement_colorspace_to_scene_linear_v3(scene_linear_rgb,
                                                      ibuf->byte_buffer.colorspace);
  }
}

}  // namespace blender
