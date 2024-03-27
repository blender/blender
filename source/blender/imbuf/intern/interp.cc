/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_math_vector.h"
#include "IMB_colormanagement.hh"
#include "IMB_interp.hh"

void IMB_sampleImageAtLocation(ImBuf *ibuf, float x, float y, bool make_linear_rgb, float color[4])
{
  using namespace blender;
  if (ibuf->float_buffer.data) {
    imbuf::interpolate_nearest_border_fl(ibuf, color, x, y);
  }
  else {
    uchar4 byte_color = imbuf::interpolate_nearest_border_byte(ibuf, x, y);
    rgba_uchar_to_float(color, byte_color);
    if (make_linear_rgb) {
      IMB_colormanagement_colorspace_to_scene_linear_v4(
          color, false, ibuf->byte_buffer.colorspace);
    }
  }
}
