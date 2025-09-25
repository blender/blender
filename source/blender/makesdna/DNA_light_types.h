/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"

#ifndef MAX_MTEX
#  define MAX_MTEX 18
#endif

struct AnimData;
struct bNodeTree;

typedef struct Light {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Light)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_LA;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /* Type and flags. */
  short type, flag;
  int mode;

  /* Color, temperature and energy. */
  float r, g, b;
  float temperature;
  float energy;
  float exposure;

  /* Point light. */
  float radius;

  /* Spot Light. */
  float spotsize;
  float spotblend;

  /* Area light. */
  short area_shape;
  short _pad1;
  float area_size;
  float area_sizey;
  float area_sizez;
  float area_spread;

  /* Sun light. */
  float sun_angle;

  /* Nodes. */
  short pr_texture, use_nodes;

  /* Eevee */
  float clipsta;
  float clipend_deprecated;

  float cascade_max_dist;
  float cascade_exponent;
  float cascade_fade;
  int cascade_count;

  float diff_fac;
  float spec_fac;
  float transmission_fac;
  float volume_fac;

  float att_dist;
  float shadow_filter_radius;
  float shadow_maximum_resolution;
  float shadow_jitter_overblur;

  /* Preview */
  struct PreviewImage *preview;

  /* Nodes */
  struct bNodeTree *nodetree;

  /* Deprecated. */
  float energy_deprecated DNA_DEPRECATED;
  float _pad2;
} Light;

/* **************** LIGHT ********************* */

/** #Light::flag */
enum {
  LA_DS_EXPAND = 1 << 0,
  /**
   * NOTE: this must have the same value as #MA_DS_SHOW_TEXS,
   * otherwise anim-editors will not read correctly.
   */
  LA_DS_SHOW_TEXS = 1 << 2,
};

/** #Light::type */
enum {
  LA_LOCAL = 0,
  LA_SUN = 1,
  LA_SPOT = 2,
  // LA_HEMI = 3, /* Deprecated. */
  LA_AREA = 4,
};

/** #Light::mode */
enum {
  LA_SHADOW = 1 << 0,
  // LA_HALO = 1 << 1, /* Deprecated. */
  // LA_LAYER = 1 << 2, /* Deprecated. */
  // LA_QUAD = 1 << 3, /* Deprecated. */
  // LA_NEG = 1 << 4, /* Deprecated. */
  // LA_ONLYSHADOW = 1 << 5, /* Deprecated. */
  // LA_SPHERE = 1 << 6, /* Deprecated. */
  LA_SQUARE = 1 << 7,
  // LA_TEXTURE = 1 << 8, /* Deprecated. */
  // LA_OSATEX = 1 << 9, /* Deprecated. */
  // LA_DEEP_SHADOW = 1 << 10, /* Deprecated. */
  // LA_NO_DIFF = 1 << 11, /* Deprecated. */
  // LA_NO_SPEC = 1 << 12, /* Deprecated. */
  LA_SHAD_RAY = 1 << 13, /* Deprecated, cleaned. */
  /**
   * YAFRAY: light shadow-buffer flag, soft-light.
   * Since it is used with LOCAL light, can't use LA_SHAD.
   */
  // LA_YF_SOFT = 1 << 14, /* Deprecated. */
  // LA_LAYER_SHADOW = 1 << 15, /* Deprecated. */
  // LA_SHAD_TEX = 1 << 16, /* Deprecated. */
  LA_SHOW_CONE = 1 << 17,
  // LA_SHOW_SHADOW_BOX = 1 << 18,
  // LA_SHAD_CONTACT = 1 << 19, /* Deprecated. */
  LA_CUSTOM_ATTENUATION = 1 << 20,
  LA_USE_SOFT_FALLOFF = 1 << 21,
  /** Use absolute resolution clamping instead of relative. */
  LA_SHAD_RES_ABSOLUTE = 1 << 22,
  LA_SHADOW_JITTER = 1 << 23,
  LA_USE_TEMPERATURE = 1 << 24,
  LA_UNNORMALIZED = 1 << 25,
};

/** #Light::falloff_type */
enum {
  LA_FALLOFF_CONSTANT = 0,
  LA_FALLOFF_INVLINEAR = 1,
  LA_FALLOFF_INVSQUARE = 2,
  LA_FALLOFF_CURVE = 3,
  LA_FALLOFF_SLIDERS = 4,
  LA_FALLOFF_INVCOEFFICIENTS = 5,
};

/** #Light::area_shape */
enum {
  LA_AREA_SQUARE = 0,
  LA_AREA_RECT = 1,
  // LA_AREA_CUBE = 2, /* Deprecated. */
  // LA_AREA_BOX = 3,  /* Deprecated. */
  LA_AREA_DISK = 4,
  LA_AREA_ELLIPSE = 5,
};
