/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_math_constants.h"

#include "DNA_ID.h"
#include "DNA_colorband_types.h"
#include "DNA_defs.h"
#include "DNA_image_types.h" /* ImageUser */
#include "DNA_material_types.h"

struct AnimData;
struct ColorBand;
struct CurveMapping;
struct Image;
struct Object;
struct PreviewImage;
struct Tex;

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name #TexMapping Types
 * \{ */

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

/* -------------------------------------------------------------------- */
/** \name #MTex
 * \{ */

struct MTex {
  DNA_DEFINE_CXX_METHODS(MTex)

  short texco = TEXCO_UV, mapto = MAP_COL, blendtype = MTEX_BLEND;
  char _pad2[2] = {};
  struct Object *object = nullptr;
  struct Tex *tex = nullptr;
  char uvname[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68] = "";

  char projx = PROJ_X, projy = PROJ_Y, projz = PROJ_Z, mapping = MTEX_FLAT;
  char brush_map_mode = MTEX_MAP_MODE_VIEW, brush_angle_mode = 0;

  /**
   * Match against the texture node (#TEX_NODE_OUTPUT, #bNode::custom1 value).
   * otherwise zero when unspecified (default).
   */
  short which_output = 0;

  float ofs[3] = {0.0f, 0.0f, 0.0f};
  float size[3] = {1.0f, 1.0f, 1.0f};
  float rot = 0, random_angle = 2.0f * (float)M_PI;

  float r = 1.0, g = 0.0, b = 1.0, k = 1.0;
  float def_var = 1.0;

  /* common */
  float colfac = 1.0;
  float alphafac = 1.0f;

  /* particles */
  float timefac = 1.0f, lengthfac = 1.0f, clumpfac = 1.0f, dampfac = 1.0f;
  float kinkfac = 1.0f, kinkampfac = 1.0f, roughfac = 1.0f, padensfac = 1.0f, gravityfac = 1.0f;
  float lifefac = 1.0f, sizefac = 1.0f, ivelfac = 1.0f, fieldfac = 1.0f;
  float twistfac = 1.0f;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Tex
 * \{ */

struct Tex_Runtime {
  /* The Depsgraph::update_count when this ID was last updated. Covers any IDRecalcFlag. */
  uint64_t last_update = 0;
};

struct Tex {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Tex)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_TE;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt = nullptr;

  void *_pad3 = nullptr;

  float noisesize = 0.25, turbul = 5.0;
  float bright = 1.0, contrast = 1.0, saturation = 1.0, rfac = 1.0, gfac = 1.0, bfac = 1.0;
  float filtersize = 1.0;

  /* newnoise: musgrave parameters */
  float mg_H = 1.0, mg_lacunarity = 2.0, mg_octaves = 2.0, mg_offset = 1.0, mg_gain = 1.0;

  /* newnoise: distorted noise amount, musgrave & voronoi output scale */
  float dist_amount = 1.0, ns_outscale = 1.0;

  /* newnoise: voronoi nearest neighbor weights, minkovsky exponent,
   * distance metric & color type */
  float vn_w1 = 1.0;
  float vn_w2 = 0.0;
  float vn_w3 = 0.0;
  float vn_w4 = 0.0;
  float vn_mexp = 2.5;
  short vn_distm = 0, vn_coltype = 0;

  /* noisedepth MUST be <= 30 else we get floating point exceptions */
  short noisedepth = 2, noisetype = 0;

  /* newnoise: noisebasis type for clouds/marble/etc, noisebasis2 only used for distorted noise */
  short noisebasis = 0, noisebasis2 = 0;

  short imaflag = TEX_INTERPOL | TEX_USEALPHA, flag = TEX_CHECKER_ODD | TEX_NO_CLAMP;
  short type = TEX_IMAGE, stype = 0;

  float cropxmin = 0.0, cropymin = 0.0, cropxmax = 1.0, cropymax = 1.0;
  short xrepeat = 1, yrepeat = 1;
  short extend = TEX_REPEAT;

  /* Variables only used for versioning, moved to struct member `iuser`. */
  short _pad0 = {};
  DNA_DEPRECATED int len = 0;
  DNA_DEPRECATED int frames = 0;
  DNA_DEPRECATED int offset = 0;
  DNA_DEPRECATED int sfra = 1;

  float checkerdist = 0, nabla = 0.025; /* also in do_versions. */

  struct ImageUser iuser;

  struct bNodeTree *nodetree = nullptr;
  struct Image *ima = nullptr;
  struct ColorBand *coba = nullptr;
  struct PreviewImage *preview = nullptr;

  char use_nodes = 0;
  char _pad[7] = {};

  Tex_Runtime runtime;
};

/** Used for mapping and texture nodes. */
struct TexMapping {
  float loc[3] = {};
  /** Rotation in radians. */
  float rot[3] = {};
  float size[3] = {};
  int flag = 0;
  char projx = 0, projy = 0, projz = 0, mapping = 0;
  int type = 0;

  float mat[4][4] = {};
  float min[3] = {}, max[3] = {};
  struct Object *ob = nullptr;
};

struct ColorMapping {
  struct ColorBand coba;

  float bright = 0, contrast = 0, saturation = 0;
  int flag = 0;

  float blend_color[3] = {};
  float blend_factor = 0;
  int blend_type = 0;
  char _pad[4] = {};
};

/** \} */
