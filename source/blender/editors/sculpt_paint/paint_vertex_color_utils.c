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
 */

/** \file
 * \ingroup edsculpt
 *
 * Intended for use by `paint_vertex.c` & `paint_vertex_color_ops.c`.
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math_base.h"
#include "BLI_math_color.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "BKE_context.h"
#include "BKE_mesh.h"

#include "DEG_depsgraph.h"

#include "ED_mesh.h"

#include "paint_intern.h" /* own include */

#define EPS_SATURATION 0.0005f

/**
 * Apply callback to each vertex of the active vertex color layer.
 */
bool ED_vpaint_color_transform(struct Object *ob,
                               VPaintTransform_Callback vpaint_tx_fn,
                               const void *user_data)
{
  Mesh *me;
  const MPoly *mp;
  int i, j;

  if (((me = BKE_mesh_from_object(ob)) == NULL) || (ED_mesh_color_ensure(me, NULL) == false)) {
    return false;
  }

  const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  mp = me->mpoly;
  for (i = 0; i < me->totpoly; i++, mp++) {
    MLoopCol *lcol = me->mloopcol + mp->loopstart;

    if (use_face_sel && !(mp->flag & ME_FACE_SEL)) {
      continue;
    }

    j = 0;
    do {
      uint vidx = me->mloop[mp->loopstart + j].v;
      if (!(use_vert_sel && !(me->mvert[vidx].flag & SELECT))) {
        float col_mix[3];
        rgb_uchar_to_float(col_mix, &lcol->r);

        vpaint_tx_fn(col_mix, user_data, col_mix);

        rgb_float_to_uchar(&lcol->r, col_mix);
      }
      lcol++;
      j++;
    } while (j < mp->totloop);
  }

  /* remove stale me->mcol, will be added later */
  BKE_mesh_tessface_clear(me);

  DEG_id_tag_update(&me->id, 0);

  return true;
}

/* -------------------------------------------------------------------- */
/** \name Color Blending Modes
 * \{ */

BLI_INLINE uint mcol_blend(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  if (fac >= 255) {
    return col_dst;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  /* Updated to use the rgb squared color model which blends nicer. */
  int r1 = cp_src[0] * cp_src[0];
  int g1 = cp_src[1] * cp_src[1];
  int b1 = cp_src[2] * cp_src[2];
  int a1 = cp_src[3] * cp_src[3];

  int r2 = cp_dst[0] * cp_dst[0];
  int g2 = cp_dst[1] * cp_dst[1];
  int b2 = cp_dst[2] * cp_dst[2];
  int a2 = cp_dst[3] * cp_dst[3];

  cp_mix[0] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * r1 + fac * r2), 255)));
  cp_mix[1] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * g1 + fac * g2), 255)));
  cp_mix[2] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * b1 + fac * b2), 255)));
  cp_mix[3] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * a1 + fac * a2), 255)));

  return col_mix;
}

BLI_INLINE uint mcol_add(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int temp;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  temp = cp_src[0] + divide_round_i((fac * cp_dst[0]), 255);
  cp_mix[0] = (temp > 254) ? 255 : temp;
  temp = cp_src[1] + divide_round_i((fac * cp_dst[1]), 255);
  cp_mix[1] = (temp > 254) ? 255 : temp;
  temp = cp_src[2] + divide_round_i((fac * cp_dst[2]), 255);
  cp_mix[2] = (temp > 254) ? 255 : temp;
  temp = cp_src[3] + divide_round_i((fac * cp_dst[3]), 255);
  cp_mix[3] = (temp > 254) ? 255 : temp;

  return col_mix;
}

BLI_INLINE uint mcol_sub(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int temp;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  temp = cp_src[0] - divide_round_i((fac * cp_dst[0]), 255);
  cp_mix[0] = (temp < 0) ? 0 : temp;
  temp = cp_src[1] - divide_round_i((fac * cp_dst[1]), 255);
  cp_mix[1] = (temp < 0) ? 0 : temp;
  temp = cp_src[2] - divide_round_i((fac * cp_dst[2]), 255);
  cp_mix[2] = (temp < 0) ? 0 : temp;
  temp = cp_src[3] - divide_round_i((fac * cp_dst[3]), 255);
  cp_mix[3] = (temp < 0) ? 0 : temp;

  return col_mix;
}

BLI_INLINE uint mcol_mul(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  /* first mul, then blend the fac */
  cp_mix[0] = divide_round_i(mfac * cp_src[0] * 255 + fac * cp_dst[0] * cp_src[0], 255 * 255);
  cp_mix[1] = divide_round_i(mfac * cp_src[1] * 255 + fac * cp_dst[1] * cp_src[1], 255 * 255);
  cp_mix[2] = divide_round_i(mfac * cp_src[2] * 255 + fac * cp_dst[2] * cp_src[2], 255 * 255);
  cp_mix[3] = divide_round_i(mfac * cp_src[3] * 255 + fac * cp_dst[3] * cp_src[3], 255 * 255);

  return col_mix;
}

BLI_INLINE uint mcol_lighten(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }
  else if (fac >= 255) {
    return col_dst;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  /* See if we're lighter, if so mix, else don't do anything.
   * if the paint color is darker then the original, then ignore */
  if (IMB_colormanagement_get_luminance_byte(cp_src) >
      IMB_colormanagement_get_luminance_byte(cp_dst)) {
    return col_src;
  }

  cp_mix[0] = divide_round_i(mfac * cp_src[0] + fac * cp_dst[0], 255);
  cp_mix[1] = divide_round_i(mfac * cp_src[1] + fac * cp_dst[1], 255);
  cp_mix[2] = divide_round_i(mfac * cp_src[2] + fac * cp_dst[2], 255);
  cp_mix[3] = divide_round_i(mfac * cp_src[3] + fac * cp_dst[3], 255);

  return col_mix;
}

BLI_INLINE uint mcol_darken(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }
  else if (fac >= 255) {
    return col_dst;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  /* See if we're darker, if so mix, else don't do anything.
   * if the paint color is brighter then the original, then ignore */
  if (IMB_colormanagement_get_luminance_byte(cp_src) <
      IMB_colormanagement_get_luminance_byte(cp_dst)) {
    return col_src;
  }

  cp_mix[0] = divide_round_i((mfac * cp_src[0] + fac * cp_dst[0]), 255);
  cp_mix[1] = divide_round_i((mfac * cp_src[1] + fac * cp_dst[1]), 255);
  cp_mix[2] = divide_round_i((mfac * cp_src[2] + fac * cp_dst[2]), 255);
  cp_mix[3] = divide_round_i((mfac * cp_src[3] + fac * cp_dst[3]), 255);
  return col_mix;
}

BLI_INLINE uint mcol_colordodge(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac, temp;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  temp = (cp_dst[0] == 255) ? 255 : min_ii((cp_src[0] * 225) / (255 - cp_dst[0]), 255);
  cp_mix[0] = (mfac * cp_src[0] + temp * fac) / 255;
  temp = (cp_dst[1] == 255) ? 255 : min_ii((cp_src[1] * 225) / (255 - cp_dst[1]), 255);
  cp_mix[1] = (mfac * cp_src[1] + temp * fac) / 255;
  temp = (cp_dst[2] == 255) ? 255 : min_ii((cp_src[2] * 225) / (255 - cp_dst[2]), 255);
  cp_mix[2] = (mfac * cp_src[2] + temp * fac) / 255;
  temp = (cp_dst[3] == 255) ? 255 : min_ii((cp_src[3] * 225) / (255 - cp_dst[3]), 255);
  cp_mix[3] = (mfac * cp_src[3] + temp * fac) / 255;
  return col_mix;
}

BLI_INLINE uint mcol_difference(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac, temp;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  temp = abs(cp_src[0] - cp_dst[0]);
  cp_mix[0] = (mfac * cp_src[0] + temp * fac) / 255;
  temp = abs(cp_src[1] - cp_dst[1]);
  cp_mix[1] = (mfac * cp_src[1] + temp * fac) / 255;
  temp = abs(cp_src[2] - cp_dst[2]);
  cp_mix[2] = (mfac * cp_src[2] + temp * fac) / 255;
  temp = abs(cp_src[3] - cp_dst[3]);
  cp_mix[3] = (mfac * cp_src[3] + temp * fac) / 255;
  return col_mix;
}

BLI_INLINE uint mcol_screen(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac, temp;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  temp = max_ii(255 - (((255 - cp_src[0]) * (255 - cp_dst[0])) / 255), 0);
  cp_mix[0] = (mfac * cp_src[0] + temp * fac) / 255;
  temp = max_ii(255 - (((255 - cp_src[1]) * (255 - cp_dst[1])) / 255), 0);
  cp_mix[1] = (mfac * cp_src[1] + temp * fac) / 255;
  temp = max_ii(255 - (((255 - cp_src[2]) * (255 - cp_dst[2])) / 255), 0);
  cp_mix[2] = (mfac * cp_src[2] + temp * fac) / 255;
  temp = max_ii(255 - (((255 - cp_src[3]) * (255 - cp_dst[3])) / 255), 0);
  cp_mix[3] = (mfac * cp_src[3] + temp * fac) / 255;
  return col_mix;
}

BLI_INLINE uint mcol_hardlight(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac, temp;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  int i = 0;

  for (i = 0; i < 4; i++) {
    if (cp_dst[i] > 127) {
      temp = 255 - ((255 - 2 * (cp_dst[i] - 127)) * (255 - cp_src[i]) / 255);
    }
    else {
      temp = (2 * cp_dst[i] * cp_src[i]) >> 8;
    }
    cp_mix[i] = min_ii((mfac * cp_src[i] + temp * fac) / 255, 255);
  }
  return col_mix;
}

BLI_INLINE uint mcol_overlay(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac, temp;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  int i = 0;

  for (i = 0; i < 4; i++) {
    if (cp_src[i] > 127) {
      temp = 255 - ((255 - 2 * (cp_src[i] - 127)) * (255 - cp_dst[i]) / 255);
    }
    else {
      temp = (2 * cp_dst[i] * cp_src[i]) >> 8;
    }
    cp_mix[i] = min_ii((mfac * cp_src[i] + temp * fac) / 255, 255);
  }
  return col_mix;
}

BLI_INLINE uint mcol_softlight(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac, temp;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  int i = 0;

  for (i = 0; i < 4; i++) {
    if (cp_src[i] < 127) {
      temp = ((2 * ((cp_dst[i] / 2) + 64)) * cp_src[i]) / 255;
    }
    else {
      temp = 255 - (2 * (255 - ((cp_dst[i] / 2) + 64)) * (255 - cp_src[i]) / 255);
    }
    cp_mix[i] = (temp * fac + cp_src[i] * mfac) / 255;
  }
  return col_mix;
}

BLI_INLINE uint mcol_exclusion(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac, temp;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  int i = 0;

  for (i = 0; i < 4; i++) {
    temp = 127 - ((2 * (cp_src[i] - 127) * (cp_dst[i] - 127)) / 255);
    cp_mix[i] = (temp * fac + cp_src[i] * mfac) / 255;
  }
  return col_mix;
}

BLI_INLINE uint mcol_luminosity(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  float h1, s1, v1;
  float h2, s2, v2;
  float r, g, b;
  rgb_to_hsv(cp_src[0] / 255.0f, cp_src[1] / 255.0f, cp_src[2] / 255.0f, &h1, &s1, &v1);
  rgb_to_hsv(cp_dst[0] / 255.0f, cp_dst[1] / 255.0f, cp_dst[2] / 255.0f, &h2, &s2, &v2);

  v1 = v2;

  hsv_to_rgb(h1, s1, v1, &r, &g, &b);

  cp_mix[0] = ((int)(r * 255.0f) * fac + mfac * cp_src[0]) / 255;
  cp_mix[1] = ((int)(g * 255.0f) * fac + mfac * cp_src[1]) / 255;
  cp_mix[2] = ((int)(b * 255.0f) * fac + mfac * cp_src[2]) / 255;
  cp_mix[3] = ((int)(cp_dst[3]) * fac + mfac * cp_src[3]) / 255;
  return col_mix;
}

BLI_INLINE uint mcol_saturation(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  float h1, s1, v1;
  float h2, s2, v2;
  float r, g, b;
  rgb_to_hsv(cp_src[0] / 255.0f, cp_src[1] / 255.0f, cp_src[2] / 255.0f, &h1, &s1, &v1);
  rgb_to_hsv(cp_dst[0] / 255.0f, cp_dst[1] / 255.0f, cp_dst[2] / 255.0f, &h2, &s2, &v2);

  if (s1 > EPS_SATURATION) {
    s1 = s2;
  }

  hsv_to_rgb(h1, s1, v1, &r, &g, &b);

  cp_mix[0] = ((int)(r * 255.0f) * fac + mfac * cp_src[0]) / 255;
  cp_mix[1] = ((int)(g * 255.0f) * fac + mfac * cp_src[1]) / 255;
  cp_mix[2] = ((int)(b * 255.0f) * fac + mfac * cp_src[2]) / 255;
  return col_mix;
}

BLI_INLINE uint mcol_hue(uint col_src, uint col_dst, int fac)
{
  uchar *cp_src, *cp_dst, *cp_mix;
  int mfac;
  uint col_mix = 0;

  if (fac == 0) {
    return col_src;
  }

  mfac = 255 - fac;

  cp_src = (uchar *)&col_src;
  cp_dst = (uchar *)&col_dst;
  cp_mix = (uchar *)&col_mix;

  float h1, s1, v1;
  float h2, s2, v2;
  float r, g, b;
  rgb_to_hsv(cp_src[0] / 255.0f, cp_src[1] / 255.0f, cp_src[2] / 255.0f, &h1, &s1, &v1);
  rgb_to_hsv(cp_dst[0] / 255.0f, cp_dst[1] / 255.0f, cp_dst[2] / 255.0f, &h2, &s2, &v2);

  h1 = h2;

  hsv_to_rgb(h1, s1, v1, &r, &g, &b);

  cp_mix[0] = ((int)(r * 255.0f) * fac + mfac * cp_src[0]) / 255;
  cp_mix[1] = ((int)(g * 255.0f) * fac + mfac * cp_src[1]) / 255;
  cp_mix[2] = ((int)(b * 255.0f) * fac + mfac * cp_src[2]) / 255;
  cp_mix[3] = ((int)(cp_dst[3]) * fac + mfac * cp_src[3]) / 255;
  return col_mix;
}

BLI_INLINE uint mcol_alpha_add(uint col_src, int fac)
{
  uchar *cp_src, *cp_mix;
  int temp;
  uint col_mix = col_src;

  if (fac == 0) {
    return col_src;
  }

  cp_src = (uchar *)&col_src;
  cp_mix = (uchar *)&col_mix;

  temp = cp_src[3] + fac;
  cp_mix[3] = (temp > 254) ? 255 : temp;

  return col_mix;
}

BLI_INLINE uint mcol_alpha_sub(uint col_src, int fac)
{
  uchar *cp_src, *cp_mix;
  int temp;
  uint col_mix = col_src;

  if (fac == 0) {
    return col_src;
  }

  cp_src = (uchar *)&col_src;
  cp_mix = (uchar *)&col_mix;

  temp = cp_src[3] - fac;
  cp_mix[3] = temp < 0 ? 0 : temp;

  return col_mix;
}

/* wpaint has 'ED_wpaint_blend_tool' */
uint ED_vpaint_blend_tool(const int tool, const uint col, const uint paintcol, const int alpha_i)
{
  switch ((IMB_BlendMode)tool) {
    case IMB_BLEND_MIX:
      return mcol_blend(col, paintcol, alpha_i);
    case IMB_BLEND_ADD:
      return mcol_add(col, paintcol, alpha_i);
    case IMB_BLEND_SUB:
      return mcol_sub(col, paintcol, alpha_i);
    case IMB_BLEND_MUL:
      return mcol_mul(col, paintcol, alpha_i);
    case IMB_BLEND_LIGHTEN:
      return mcol_lighten(col, paintcol, alpha_i);
    case IMB_BLEND_DARKEN:
      return mcol_darken(col, paintcol, alpha_i);
    case IMB_BLEND_COLORDODGE:
      return mcol_colordodge(col, paintcol, alpha_i);
    case IMB_BLEND_DIFFERENCE:
      return mcol_difference(col, paintcol, alpha_i);
    case IMB_BLEND_SCREEN:
      return mcol_screen(col, paintcol, alpha_i);
    case IMB_BLEND_HARDLIGHT:
      return mcol_hardlight(col, paintcol, alpha_i);
    case IMB_BLEND_OVERLAY:
      return mcol_overlay(col, paintcol, alpha_i);
    case IMB_BLEND_SOFTLIGHT:
      return mcol_softlight(col, paintcol, alpha_i);
    case IMB_BLEND_EXCLUSION:
      return mcol_exclusion(col, paintcol, alpha_i);
    case IMB_BLEND_LUMINOSITY:
      return mcol_luminosity(col, paintcol, alpha_i);
    case IMB_BLEND_SATURATION:
      return mcol_saturation(col, paintcol, alpha_i);
    case IMB_BLEND_HUE:
      return mcol_hue(col, paintcol, alpha_i);
    /* non-color */
    case IMB_BLEND_ERASE_ALPHA:
      return mcol_alpha_sub(col, alpha_i);
    case IMB_BLEND_ADD_ALPHA:
      return mcol_alpha_add(col, alpha_i);
    default:
      BLI_assert(0);
      return 0;
  }
}

/** \} */
