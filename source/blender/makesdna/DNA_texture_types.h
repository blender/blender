/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_colorband_types.h"
#include "DNA_defs.h"
#include "DNA_image_types.h" /* ImageUser */

struct AnimData;
struct ColorBand;
struct CurveMapping;
struct Image;
struct Object;
struct PreviewImage;
struct Tex;

/* -------------------------------------------------------------------- */
/** \name #MTex
 * \{ */

typedef struct MTex {
  DNA_DEFINE_CXX_METHODS(MTex)

  short texco, mapto, blendtype;
  char _pad2[2];
  struct Object *object;
  struct Tex *tex;
  char uvname[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];

  char projx, projy, projz, mapping;
  char brush_map_mode, brush_angle_mode;

  /**
   * Match against the texture node (#TEX_NODE_OUTPUT, #bNode::custom1 value).
   * otherwise zero when unspecified (default).
   */
  short which_output;

  float ofs[3], size[3], rot, random_angle;

  float r, g, b, k;
  float def_var;

  /* common */
  float colfac;
  float alphafac;

  /* particles */
  float timefac, lengthfac, clumpfac, dampfac;
  float kinkfac, kinkampfac, roughfac, padensfac, gravityfac;
  float lifefac, sizefac, ivelfac, fieldfac;
  float twistfac;
} MTex;

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Tex
 * \{ */

typedef struct Tex_Runtime {
  /* The Depsgraph::update_count when this ID was last updated. Covers any IDRecalcFlag. */
  uint64_t last_update;
} Tex_Runtime;

typedef struct Tex {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Tex)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_TE;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  void *_pad3;

  float noisesize, turbul;
  float bright, contrast, saturation, rfac, gfac, bfac;
  float filtersize;

  /* newnoise: musgrave parameters */
  float mg_H, mg_lacunarity, mg_octaves, mg_offset, mg_gain;

  /* newnoise: distorted noise amount, musgrave & voronoi output scale */
  float dist_amount, ns_outscale;

  /* newnoise: voronoi nearest neighbor weights, minkovsky exponent,
   * distance metric & color type */
  float vn_w1;
  float vn_w2;
  float vn_w3;
  float vn_w4;
  float vn_mexp;
  short vn_distm, vn_coltype;

  /* noisedepth MUST be <= 30 else we get floating point exceptions */
  short noisedepth, noisetype;

  /* newnoise: noisebasis type for clouds/marble/etc, noisebasis2 only used for distorted noise */
  short noisebasis, noisebasis2;

  short imaflag, flag;
  short type, stype;

  float cropxmin, cropymin, cropxmax, cropymax;
  short xrepeat, yrepeat;
  short extend;

  /* Variables only used for versioning, moved to struct member `iuser`. */
  short _pad0;
  int len DNA_DEPRECATED;
  int frames DNA_DEPRECATED;
  int offset DNA_DEPRECATED;
  int sfra DNA_DEPRECATED;

  float checkerdist, nabla;

  struct ImageUser iuser;

  struct bNodeTree *nodetree;
  struct Image *ima;
  struct ColorBand *coba;
  struct PreviewImage *preview;

  char use_nodes;
  char _pad[7];

  Tex_Runtime runtime;
} Tex;

/** Used for mapping and texture nodes. */
typedef struct TexMapping {
  float loc[3];
  /** Rotation in radians. */
  float rot[3];
  float size[3];
  int flag;
  char projx, projy, projz, mapping;
  int type;

  float mat[4][4];
  float min[3], max[3];
  struct Object *ob;

} TexMapping;

typedef struct ColorMapping {
  struct ColorBand coba;

  float bright, contrast, saturation;
  int flag;

  float blend_color[3];
  float blend_factor;
  int blend_type;
  char _pad[4];
} ColorMapping;

/** \} */

/* -------------------------------------------------------------------- */
/** \name #TexMapping Types
 * \{ */

/** #TexMapping::flag bit-mask. */
enum {
  TEXMAP_CLIP_MIN = 1 << 0,
  TEXMAP_CLIP_MAX = 1 << 1,
  TEXMAP_UNIT_MATRIX = 1 << 2,
};

/** #TexMapping::type. */
enum {
  TEXMAP_TYPE_POINT = 0,
  TEXMAP_TYPE_TEXTURE = 1,
  TEXMAP_TYPE_VECTOR = 2,
  TEXMAP_TYPE_NORMAL = 3,
};

/** #ColorMapping::flag bit-mask. */
enum {
  COLORMAP_USE_RAMP = 1,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Tex Types
 * \{ */

/** #Tex::type. */
enum {
  TEX_CLOUDS = 1,
  TEX_WOOD = 2,
  TEX_MARBLE = 3,
  TEX_MAGIC = 4,
  TEX_BLEND = 5,
  TEX_STUCCI = 6,
  TEX_NOISE = 7,
  TEX_IMAGE = 8,
  // TEX_PLUGIN = 9,  /* Deprecated */
  // TEX_ENVMAP = 10, /* Deprecated */
  TEX_MUSGRAVE = 11,
  TEX_VORONOI = 12,
  TEX_DISTNOISE = 13,
  // TEX_POINTDENSITY = 14, /* Deprecated */
  // TEX_VOXELDATA = 15,    /* Deprecated */
  // TEX_OCEAN = 16,        /* Deprecated */
};

/** #Tex::stype musgrave. */
enum {
  TEX_MFRACTAL = 0,
  TEX_RIDGEDMF = 1,
  TEX_HYBRIDMF = 2,
  TEX_FBM = 3,
  TEX_HTERRAIN = 4,
};

/** #Tex::noisebasis, #Tex::noisebasis2. */
enum {
  TEX_BLENDER = 0,
  TEX_STDPERLIN = 1,
  TEX_NEWPERLIN = 2,
  TEX_VORONOI_F1 = 3,
  TEX_VORONOI_F2 = 4,
  TEX_VORONOI_F3 = 5,
  TEX_VORONOI_F4 = 6,
  TEX_VORONOI_F2F1 = 7,
  TEX_VORONOI_CRACKLE = 8,
  TEX_CELLNOISE = 14,
};

/** #Tex::vn_distm voronoi distance metrics. */
enum {
  TEX_DISTANCE = 0,
  TEX_DISTANCE_SQUARED = 1,
  TEX_MANHATTAN = 2,
  TEX_CHEBYCHEV = 3,
  TEX_MINKOVSKY_HALF = 4,
  TEX_MINKOVSKY_FOUR = 5,
  TEX_MINKOVSKY = 6,
};

/** #Tex::imaflag bit-mask. */
enum {
  TEX_INTERPOL = 1 << 0,
  TEX_USEALPHA = 1 << 1,
  TEX_IMAROT = 1 << 4,
  TEX_CALCALPHA = 1 << 5,
  TEX_NORMALMAP = 1 << 11,
  TEX_DERIVATIVEMAP = 1 << 14,
};

/** #Tex::flag bit-mask. */
enum {
  TEX_COLORBAND = 1 << 0,
  TEX_FLIPBLEND = 1 << 1,
  TEX_NEGALPHA = 1 << 2,
  TEX_CHECKER_ODD = 1 << 3,
  TEX_CHECKER_EVEN = 1 << 4,
  TEX_PRV_ALPHA = 1 << 5,
  TEX_PRV_NOR = 1 << 6,
  TEX_REPEAT_XMIR = 1 << 7,
  TEX_REPEAT_YMIR = 1 << 8,
  TEX_DS_EXPAND = 1 << 9,
  TEX_NO_CLAMP = 1 << 10,
};

/** #Tex::extend (starts with 1 because of backward compatibility). */
enum {
  TEX_EXTEND = 1,
  TEX_CLIP = 2,
  TEX_REPEAT = 3,
  TEX_CLIPCUBE = 4,
  TEX_CHECKER = 5,
};

/** #Tex::noisetype type. */
enum {
  TEX_NOISESOFT = 0,
  TEX_NOISEPERL = 1,
};

/** #Tex::noisebasis2 wood waveforms. */
enum {
  TEX_SIN = 0,
  TEX_SAW = 1,
  TEX_TRI = 2,
};

/** #Tex::stype wood types. */
enum {
  TEX_BAND = 0,
  TEX_RING = 1,
  TEX_BANDNOISE = 2,
  TEX_RINGNOISE = 3,
};

/** #Tex::stype cloud types. */
enum {
  TEX_DEFAULT = 0,
  TEX_COLOR = 1,
};

/** #Tex::stype marble types. */
enum {
  TEX_SOFT = 0,
  TEX_SHARP = 1,
  TEX_SHARPER = 2,
};

/** #Tex::stype blend types. */
enum {
  TEX_LIN = 0,
  TEX_QUAD = 1,
  TEX_EASE = 2,
  TEX_DIAG = 3,
  TEX_SPHERE = 4,
  TEX_HALO = 5,
  TEX_RAD = 6,
};

/** #Tex::stype stucci types. */
enum {
  TEX_PLASTIC = 0,
  TEX_WALLIN = 1,
  TEX_WALLOUT = 2,
};

/** #Tex::vn_coltype voronoi color types. */
enum {
  TEX_INTENSITY = 0,
  TEX_COL1 = 1,
  TEX_COL2 = 2,
  TEX_COL3 = 3,
};

/** Return value. */
enum {
  TEX_INT = 0,
  TEX_RGB = 1,
};

/**
 * - #Material::pr_texture
 * - #Light::pr_texture
 * - #World::pr_texture
 * - #FreestyleLineStyle::pr_texture
 */
enum {
  TEX_PR_TEXTURE = 0,
  TEX_PR_OTHER = 1,
  TEX_PR_BOTH = 2,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #TexMapping Types
 * \{ */

/**
 * #TexMapping::projx
 * #TexMapping::projy
 * #TexMapping::projz
 */
enum {
  PROJ_N = 0,
  PROJ_X = 1,
  PROJ_Y = 2,
  PROJ_Z = 3,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MTex Types
 * \{ */

/** #MTex::mapping. */
enum {
  MTEX_FLAT = 0,
  MTEX_CUBE = 1,
  MTEX_TUBE = 2,
  MTEX_SPHERE = 3,
};

/** #MTex::blendtype. */
enum {
  MTEX_BLEND = 0,
  MTEX_MUL = 1,
  MTEX_ADD = 2,
  MTEX_SUB = 3,
  MTEX_DIV = 4,
  MTEX_DARK = 5,
  MTEX_DIFF = 6,
  MTEX_LIGHT = 7,
  MTEX_SCREEN = 8,
  MTEX_OVERLAY = 9,
  MTEX_BLEND_HUE = 10,
  MTEX_BLEND_SAT = 11,
  MTEX_BLEND_VAL = 12,
  MTEX_BLEND_COLOR = 13,
  MTEX_SOFT_LIGHT = 15,
  MTEX_LIN_LIGHT = 16,
};

/** #MTex::brush_map_mode. */
enum {
  MTEX_MAP_MODE_VIEW = 0,
  MTEX_MAP_MODE_TILED = 1,
  MTEX_MAP_MODE_3D = 2,
  MTEX_MAP_MODE_AREA = 3,
  MTEX_MAP_MODE_RANDOM = 4,
  MTEX_MAP_MODE_STENCIL = 5,
};

/** #MTex::brush_angle_mode. */
enum {
  MTEX_ANGLE_RANDOM = 1,
  MTEX_ANGLE_RAKE = 2,
};

/** \} */
