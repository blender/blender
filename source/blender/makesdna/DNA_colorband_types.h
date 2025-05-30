/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"

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

typedef struct CBData {
  float r, g, b, a, pos;
  int cur;
} CBData;

/**
 * 32 = #MAXCOLORBAND
 * \note that this has to remain a single struct, for UserDef.
 */
typedef struct ColorBand {
  short tot, cur;
  char ipotype, ipotype_hue;
  char color_mode;
  char _pad[1];

  CBData data[32];
} ColorBand;

/** \} */
