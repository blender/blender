/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_math_vector.h"
#include "BLI_task.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

/* -------------------------------------------------------------------- */
/** \name Alpha-under
 * \{ */

void IMB_alpha_under_color_float(float *rect_float, int x, int y, float backcol[3])
{
  using namespace blender;
  threading::parallel_for(IndexRange(int64_t(x) * y), 32 * 1024, [&](const IndexRange i_range) {
    float *pix = rect_float + i_range.first() * 4;
    for ([[maybe_unused]] const int i : i_range) {
      const float mul = 1.0f - pix[3];
      madd_v3_v3fl(pix, backcol, mul);
      pix[3] = 1.0f;
      pix += 4;
    }
  });
}

void IMB_alpha_under_color_byte(uchar *rect, int x, int y, const float backcol[3])
{
  using namespace blender;
  threading::parallel_for(IndexRange(int64_t(x) * y), 32 * 1024, [&](const IndexRange i_range) {
    uchar *pix = rect + i_range.first() * 4;
    for ([[maybe_unused]] const int i : i_range) {
      if (pix[3] == 255) {
        /* pass */
      }
      else if (pix[3] == 0) {
        pix[0] = backcol[0] * 255;
        pix[1] = backcol[1] * 255;
        pix[2] = backcol[2] * 255;
      }
      else {
        float alpha = pix[3] / 255.0;
        float mul = 1.0f - alpha;

        pix[0] = (pix[0] * alpha) + mul * backcol[0];
        pix[1] = (pix[1] * alpha) + mul * backcol[1];
        pix[2] = (pix[2] * alpha) + mul * backcol[2];
      }
      pix[3] = 255;
      pix += 4;
    }
  });
}

/** \} */
