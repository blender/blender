/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GlareStreaksOperation.h"
#include "BLI_math_rotation.h"

namespace blender::compositor {

void GlareStreaksOperation::generate_glare(float *data,
                                           MemoryBuffer *input_tile,
                                           const NodeGlare *settings)
{
  int x, y, n;
  // uint nump = 0; /* UNUSED. */
  float c1[4], c2[4], c3[4], c4[4];
  float a, ang = DEG2RADF(360.0f) / float(settings->streaks);

  int size = input_tile->get_width() * input_tile->get_height();
  int size4 = size * 4;

  bool breaked = false;

  MemoryBuffer tsrc(*input_tile);
  MemoryBuffer tdst(DataType::Color, input_tile->get_rect());
  tdst.clear();
  memset(data, 0, size4 * sizeof(float));

  for (a = 0.0f; a < DEG2RADF(360.0f) && (!breaked); a += ang) {
    const float an = a + settings->angle_ofs;
    const float vx = cos(double(an)), vy = sin(double(an));
    for (n = 0; n < settings->iter && (!breaked); n++) {
      const float p4 = pow(4.0, double(n));
      const float vxp = vx * p4, vyp = vy * p4;
      const float wt = pow(double(settings->fade), double(p4));

      /* Color-modulation amount relative to current pass. */
      const float cmo = 1.0f - float(pow(double(settings->colmod), double(n) + 1));

      float *tdstcol = tdst.get_buffer();
      for (y = 0; y < tsrc.get_height() && (!breaked); y++) {
        for (x = 0; x < tsrc.get_width(); x++, tdstcol += 4) {
          /* First pass no offset, always same for every pass, exact copy,
           * otherwise results in uneven brightness, only need once. */
          if (n == 0) {
            tsrc.read(c1, x, y);
          }
          else {
            c1[0] = c1[1] = c1[2] = 0;
          }
          tsrc.read_bilinear(c2, x + vxp, y + vyp);
          tsrc.read_bilinear(c3, x + vxp * 2.0f, y + vyp * 2.0f);
          tsrc.read_bilinear(c4, x + vxp * 3.0f, y + vyp * 3.0f);
          /* Modulate color to look vaguely similar to a color spectrum. */
          c2[1] *= cmo;
          c2[2] *= cmo;

          c3[0] *= cmo;
          c3[1] *= cmo;

          c4[0] *= cmo;
          c4[2] *= cmo;

          tdstcol[0] = 0.5f * (tdstcol[0] + c1[0] + wt * (c2[0] + wt * (c3[0] + wt * c4[0])));
          tdstcol[1] = 0.5f * (tdstcol[1] + c1[1] + wt * (c2[1] + wt * (c3[1] + wt * c4[1])));
          tdstcol[2] = 0.5f * (tdstcol[2] + c1[2] + wt * (c2[2] + wt * (c3[2] + wt * c4[2])));
          tdstcol[3] = 1.0f;
        }
        if (is_braked()) {
          breaked = true;
        }
      }
      memcpy(tsrc.get_buffer(), tdst.get_buffer(), sizeof(float) * size4);
    }

    float *sourcebuffer = tsrc.get_buffer();
    float factor = 1.0f / float(6 - settings->iter);
    for (int i = 0; i < size4; i += 4) {
      madd_v3_v3fl(&data[i], &sourcebuffer[i], factor);
      data[i + 3] = 1.0f;
    }

    tdst.clear();
    memcpy(tsrc.get_buffer(), input_tile->get_buffer(), sizeof(float) * size4);
    // nump++; /* UNUSED. */
  }
}

}  // namespace blender::compositor
