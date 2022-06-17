/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_MTEX
#  define MAX_MTEX 18
#endif

struct AnimData;
struct CurveMapping;
struct Ipo;
struct bNodeTree;

typedef struct Light {
  DNA_DEFINE_CXX_METHODS(Light)

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  short type, flag;
  int mode;

  float r, g, b, k;
  float shdwr, shdwg, shdwb, shdwpad;

  float energy, dist, spotsize, spotblend;

  /** Quad1 and Quad2 attenuation. */
  float att1, att2;
  float coeff_const, coeff_lin, coeff_quad;
  char _pad0[4];
  struct CurveMapping *curfalloff;
  short falloff_type;
  char _pad2[2];

  float clipsta, clipend;
  float bias;
  float soft;      /* DEPRECATED kept for compatibility. */
  float bleedbias; /* DEPRECATED kept for compatibility. */
  float bleedexp;  /* DEPRECATED kept for compatibility. */
  short bufsize, samp, buffers, filtertype;
  char bufflag, buftype;

  short area_shape;
  float area_size, area_sizey, area_sizez;
  float area_spread;

  float sun_angle;

  /* texact is for buttons */
  short texact, shadhalostep;

  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;
  short pr_texture, use_nodes;
  char _pad6[4];

  /* Eevee */
  float cascade_max_dist;
  float cascade_exponent;
  float cascade_fade;
  int cascade_count;

  float contact_dist;
  float contact_bias;
  float contact_spread; /* DEPRECATED kept for compatibility. */
  float contact_thickness;

  float diff_fac, volume_fac;
  float spec_fac, att_dist;

  /* preview */
  struct PreviewImage *preview;

  /* nodes */
  struct bNodeTree *nodetree;
} Light;

/* **************** LIGHT ********************* */

/* flag */
#define LA_DS_EXPAND (1 << 0)
/* NOTE: this must have the same value as MA_DS_SHOW_TEXS,
 * otherwise anim-editors will not read correctly
 */
#define LA_DS_SHOW_TEXS (1 << 2)

/* type */
#define LA_LOCAL 0
#define LA_SUN 1
#define LA_SPOT 2
/* #define LA_HEMI          3 */ /* not used anymore */
#define LA_AREA 4

/* mode */
#define LA_SHADOW (1 << 0)
/* #define LA_HALO      (1 << 1) */ /* not used anymore */
/* #define LA_LAYER     (1 << 2) */ /* not used anymore */
/* #define LA_QUAD      (1 << 3) */ /* not used anymore */
/* #define LA_NEG       (1 << 4) */ /* not used anymore */
/* #define LA_ONLYSHADOW(1 << 5) */ /* not used anymore */
/* #define LA_SPHERE    (1 << 6) */ /* not used anymore */
#define LA_SQUARE (1 << 7)
/* #define LA_TEXTURE   (1 << 8) */      /* not used anymore */
/* #define LA_OSATEX    (1 << 9) */      /* not used anymore */
/* #define LA_DEEP_SHADOW   (1 << 10) */ /* not used anywhere */
/* #define LA_NO_DIFF       (1 << 11) */ /* not used anywhere */
/* #define LA_NO_SPEC       (1 << 12) */ /* not used anywhere */
/* #define LA_SHAD_RAY      (1 << 13) */ /* not used anywhere - cleaned */
/* YAFRAY: light shadow-buffer flag, soft-light. */
/* Since it is used with LOCAL light, can't use LA_SHAD */
/* #define LA_YF_SOFT       (1 << 14) */ /* not used anymore */
/* #define LA_LAYER_SHADOW  (1 << 15) */ /* not used anymore */
/* #define LA_SHAD_TEX      (1 << 16) */ /* not used anymore */
#define LA_SHOW_CONE (1 << 17)
/* #define LA_SHOW_SHADOW_BOX (1 << 18) */
#define LA_SHAD_CONTACT (1 << 19)
#define LA_CUSTOM_ATTENUATION (1 << 20)

/* falloff_type */
#define LA_FALLOFF_CONSTANT 0
#define LA_FALLOFF_INVLINEAR 1
#define LA_FALLOFF_INVSQUARE 2
#define LA_FALLOFF_CURVE 3
#define LA_FALLOFF_SLIDERS 4
#define LA_FALLOFF_INVCOEFFICIENTS 5

/* area shape */
#define LA_AREA_SQUARE 0
#define LA_AREA_RECT 1
/* #define LA_AREA_CUBE 2 */ /* UNUSED */
/* #define LA_AREA_BOX  3 */ /* UNUSED */
#define LA_AREA_DISK 4
#define LA_AREA_ELLIPSE 5

#ifdef __cplusplus
}
#endif
