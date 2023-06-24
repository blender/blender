/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GlareSimpleStarOperation.h"

namespace blender::compositor {

void GlareSimpleStarOperation::generate_glare(float *data,
                                              MemoryBuffer *input_tile,
                                              const NodeGlare *settings)
{
  int i, x, y, ym, yp, xm, xp;
  float c[4] = {0, 0, 0, 0}, tc[4] = {0, 0, 0, 0};
  const float f1 = 1.0f - settings->fade;
  const float f2 = (1.0f - f1) * 0.5f;

  MemoryBuffer tbuf1(*input_tile);
  MemoryBuffer tbuf2(*input_tile);

  bool breaked = false;
  for (i = 0; i < settings->iter && (!breaked); i++) {
    //      // (x || x-1, y-1) to (x || x+1, y+1)
    //      // F
    for (y = 0; y < this->get_height() && (!breaked); y++) {
      ym = y - i;
      yp = y + i;
      for (x = 0; x < this->get_width(); x++) {
        xm = x - i;
        xp = x + i;
        tbuf1.read(c, x, y);
        mul_v3_fl(c, f1);
        tbuf1.read(tc, (settings->star_45 ? xm : x), ym);
        madd_v3_v3fl(c, tc, f2);
        tbuf1.read(tc, (settings->star_45 ? xp : x), yp);
        madd_v3_v3fl(c, tc, f2);
        c[3] = 1.0f;
        tbuf1.write_pixel(x, y, c);

        tbuf2.read(c, x, y);
        mul_v3_fl(c, f1);
        tbuf2.read(tc, xm, (settings->star_45 ? yp : y));
        madd_v3_v3fl(c, tc, f2);
        tbuf2.read(tc, xp, (settings->star_45 ? ym : y));
        madd_v3_v3fl(c, tc, f2);
        c[3] = 1.0f;
        tbuf2.write_pixel(x, y, c);
      }
      if (is_braked()) {
        breaked = true;
      }
    }
    //      // B
    for (y = this->get_height() - 1; y >= 0 && (!breaked); y--) {
      ym = y - i;
      yp = y + i;
      for (x = this->get_width() - 1; x >= 0; x--) {
        xm = x - i;
        xp = x + i;
        tbuf1.read(c, x, y);
        mul_v3_fl(c, f1);
        tbuf1.read(tc, (settings->star_45 ? xm : x), ym);
        madd_v3_v3fl(c, tc, f2);
        tbuf1.read(tc, (settings->star_45 ? xp : x), yp);
        madd_v3_v3fl(c, tc, f2);
        c[3] = 1.0f;
        tbuf1.write_pixel(x, y, c);

        tbuf2.read(c, x, y);
        mul_v3_fl(c, f1);
        tbuf2.read(tc, xm, (settings->star_45 ? yp : y));
        madd_v3_v3fl(c, tc, f2);
        tbuf2.read(tc, xp, (settings->star_45 ? ym : y));
        madd_v3_v3fl(c, tc, f2);
        c[3] = 1.0f;
        tbuf2.write_pixel(x, y, c);
      }
      if (is_braked()) {
        breaked = true;
      }
    }
  }

  for (i = 0; i < this->get_width() * this->get_height() * 4; i++) {
    data[i] = tbuf1.get_buffer()[i] + tbuf2.get_buffer()[i];
  }
}

}  // namespace blender::compositor
