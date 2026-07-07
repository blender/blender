/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name #ColorBand Types
 * \{ */

/** #ColorBand::color_mode. */
enum {
  COLBAND_BLEND_RGB,
  COLBAND_BLEND_HSV = 1,
  COLBAND_BLEND_HSL = 2,
};

/** #ColorBand::ipotype (interpolation). */
enum {
  COLBAND_INTERP_LINEAR,
  COLBAND_INTERP_EASE = 1,
  COLBAND_INTERP_B_SPLINE = 2,
  COLBAND_INTERP_CARDINAL = 3,
  COLBAND_INTERP_CONSTANT = 4,
};

/** #ColorBand::ipotype_hue (hue interpolation). */
enum {
  COLBAND_HUE_NEAR,
  COLBAND_HUE_FAR = 1,
  COLBAND_HUE_CW = 2,
  COLBAND_HUE_CCW = 3,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #ColorBand
 * \{ */

#ifndef DNA_USHORT_FIX
#  define DNA_USHORT_FIX
/**
 * \deprecated This typedef serves to avoid badly typed functions when
 * \deprecated compiling while delivering a proper dna.c. Do not use
 * \deprecated it in any case.
 */
typedef unsigned short dna_ushort_fix;
#endif

struct CBData {
  float r = 0, g = 0, b = 0, a = 0, pos = 0;
  int cur = 0;
};

/**
 * 32 = #MAXCOLORBAND
 * \note that this has to remain a single struct, for UserDef.
 */
struct ColorBand {
  short tot = 0, cur = 0;
  char ipotype = 0, ipotype_hue = 0;
  char color_mode = 0;
  char _pad[1] = {};

  CBData data[32];
};

/** \} */

}  // namespace blender
