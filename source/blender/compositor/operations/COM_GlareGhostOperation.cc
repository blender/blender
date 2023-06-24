/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GlareGhostOperation.h"
#include "COM_FastGaussianBlurOperation.h"

namespace blender::compositor {

static float smooth_mask(float x, float y)
{
  float t;
  x = 2.0f * x - 1.0f;
  y = 2.0f * y - 1.0f;
  if ((t = 1.0f - sqrtf(x * x + y * y)) > 0.0f) {
    return t;
  }

  return 0.0f;
}

void GlareGhostOperation::generate_glare(float *data,
                                         MemoryBuffer *input_tile,
                                         const NodeGlare *settings)
{
  const int qt = 1 << settings->quality;
  const float s1 = 4.0f / float(qt), s2 = 2.0f * s1;
  int x, y, n, p, np;
  fRGB c, tc, cm[64];
  float sc, isc, u, v, sm, s, t, ofs, scalef[64];
  const float cmo = 1.0f - settings->colmod;

  MemoryBuffer gbuf(*input_tile);
  MemoryBuffer tbuf1(*input_tile);

  bool breaked = false;

  FastGaussianBlurOperation::IIR_gauss(&tbuf1, s1, 0, 3);
  if (!breaked) {
    FastGaussianBlurOperation::IIR_gauss(&tbuf1, s1, 1, 3);
  }
  if (is_braked()) {
    breaked = true;
  }
  if (!breaked) {
    FastGaussianBlurOperation::IIR_gauss(&tbuf1, s1, 2, 3);
  }

  MemoryBuffer tbuf2(tbuf1);

  if (is_braked()) {
    breaked = true;
  }
  if (!breaked) {
    FastGaussianBlurOperation::IIR_gauss(&tbuf2, s2, 0, 3);
  }
  if (is_braked()) {
    breaked = true;
  }
  if (!breaked) {
    FastGaussianBlurOperation::IIR_gauss(&tbuf2, s2, 1, 3);
  }
  if (is_braked()) {
    breaked = true;
  }
  if (!breaked) {
    FastGaussianBlurOperation::IIR_gauss(&tbuf2, s2, 2, 3);
  }

  ofs = (settings->iter & 1) ? 0.5f : 0.0f;
  for (x = 0; x < (settings->iter * 4); x++) {
    y = x & 3;
    cm[x][0] = cm[x][1] = cm[x][2] = 1;
    if (y == 1) {
      fRGB_rgbmult(cm[x], 1.0f, cmo, cmo);
    }
    if (y == 2) {
      fRGB_rgbmult(cm[x], cmo, cmo, 1.0f);
    }
    if (y == 3) {
      fRGB_rgbmult(cm[x], cmo, 1.0f, cmo);
    }
    scalef[x] = 2.1f * (1.0f - (x + ofs) / float(settings->iter * 4));
    if (x & 1) {
      scalef[x] = -0.99f / scalef[x];
    }
  }

  sc = 2.13;
  isc = -0.97;
  for (y = 0; y < gbuf.get_height() && (!breaked); y++) {
    v = (float(y) + 0.5f) / float(gbuf.get_height());
    for (x = 0; x < gbuf.get_width(); x++) {
      u = (float(x) + 0.5f) / float(gbuf.get_width());
      s = (u - 0.5f) * sc + 0.5f;
      t = (v - 0.5f) * sc + 0.5f;
      tbuf1.read_bilinear(c, s * gbuf.get_width(), t * gbuf.get_height());
      sm = smooth_mask(s, t);
      mul_v3_fl(c, sm);
      s = (u - 0.5f) * isc + 0.5f;
      t = (v - 0.5f) * isc + 0.5f;
      tbuf2.read_bilinear(tc, s * gbuf.get_width() - 0.5f, t * gbuf.get_height() - 0.5f);
      sm = smooth_mask(s, t);
      madd_v3_v3fl(c, tc, sm);

      gbuf.write_pixel(x, y, c);
    }
    if (is_braked()) {
      breaked = true;
    }
  }

  memset(tbuf1.get_buffer(),
         0,
         tbuf1.get_width() * tbuf1.get_height() * COM_DATA_TYPE_COLOR_CHANNELS * sizeof(float));
  for (n = 1; n < settings->iter && (!breaked); n++) {
    for (y = 0; y < gbuf.get_height() && (!breaked); y++) {
      v = (float(y) + 0.5f) / float(gbuf.get_height());
      for (x = 0; x < gbuf.get_width(); x++) {
        u = (float(x) + 0.5f) / float(gbuf.get_width());
        tc[0] = tc[1] = tc[2] = 0.0f;
        for (p = 0; p < 4; p++) {
          np = (n << 2) + p;
          s = (u - 0.5f) * scalef[np] + 0.5f;
          t = (v - 0.5f) * scalef[np] + 0.5f;
          gbuf.read_bilinear(c, s * gbuf.get_width() - 0.5f, t * gbuf.get_height() - 0.5f);
          mul_v3_v3(c, cm[np]);
          sm = smooth_mask(s, t) * 0.25f;
          madd_v3_v3fl(tc, c, sm);
        }
        tbuf1.add_pixel(x, y, tc);
      }
      if (is_braked()) {
        breaked = true;
      }
    }
    memcpy(gbuf.get_buffer(),
           tbuf1.get_buffer(),
           tbuf1.get_width() * tbuf1.get_height() * COM_DATA_TYPE_COLOR_CHANNELS * sizeof(float));
  }
  memcpy(data,
         gbuf.get_buffer(),
         gbuf.get_width() * gbuf.get_height() * COM_DATA_TYPE_COLOR_CHANNELS * sizeof(float));
}

}  // namespace blender::compositor
