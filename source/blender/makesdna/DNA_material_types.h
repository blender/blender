/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifndef MAX_MTEX
#  define MAX_MTEX 18
#endif

struct AnimData;
struct Image;
struct bNodeTree;

/* WATCH IT: change type? also make changes in ipo.h */

typedef struct TexPaintSlot {
  DNA_DEFINE_CXX_METHODS(TexPaintSlot)

  /** Image to be painted on. Mutual exclusive with attribute_name. */
  struct Image *ima;
  struct ImageUser *image_user;

  /**
   * Custom-data index for uv layer, #MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX.
   * May reference #NodeShaderUVMap::uv_name.
   */
  char *uvname;
  /**
   * Color attribute name when painting using color attributes. Mutual exclusive with ima.
   * Points to the name of a CustomDataLayer.
   */
  char *attribute_name;
  /** Do we have a valid image and UV map or attribute. */
  int valid;
  /** Copy of node interpolation setting. */
  int interp;
} TexPaintSlot;

typedef struct MaterialGPencilStyle {
  DNA_DEFINE_CXX_METHODS(MaterialGPencilStyle)

  /** Texture image for strokes. */
  struct Image *sima;
  /** Texture image for filling. */
  struct Image *ima;
  /** Color for paint and strokes (alpha included). */
  float stroke_rgba[4];
  /** Color that should be used for drawing "fills" for strokes (alpha included). */
  float fill_rgba[4];
  /** Secondary color used for gradients and other stuff. */
  float mix_rgba[4];
  /** Settings. */
  short flag;
  /** Custom index for passes. */
  short index;
  /** Style for drawing strokes (used to select shader type). */
  short stroke_style;
  /** Style for filling areas (used to select shader type). */
  short fill_style;
  /** Factor used to define shader behavior (several uses). */
  float mix_factor;
  /** Angle used for gradients orientation. */
  float gradient_angle DNA_DEPRECATED;
  /** Radius for radial gradients. */
  float gradient_radius DNA_DEPRECATED;
  char _pad2[4];
  /** UV coordinates scale. */
  float gradient_scale[2] DNA_DEPRECATED;
  /** Factor to shift filling in 2d space. */
  float gradient_shift[2] DNA_DEPRECATED;
  /** Angle used for texture orientation. */
  float texture_angle;
  /** Texture scale (separated of uv scale). */
  float texture_scale[2];
  /** Factor to shift texture in 2d space. */
  float texture_offset[2];
  /** Texture opacity. */
  float texture_opacity DNA_DEPRECATED;
  /** Pixel size for uv along the stroke. */
  float texture_pixsize;
  /** Drawing mode (line or dots). */
  int mode;

  /** Type of gradient. */
  int gradient_type;

  /** Factor used to mix texture and stroke color. */
  float mix_stroke_factor;
  /** Mode used to align Dots and Boxes with stroke drawing path and object rotation */
  int alignment_mode;
  /** Rotation for texture for Dots and Squares. */
  float alignment_rotation;
} MaterialGPencilStyle;

/* MaterialGPencilStyle->flag */
typedef enum eMaterialGPencilStyle_Flag {
  /* Fill Texture is a pattern */
  GP_MATERIAL_FILL_PATTERN = (1 << 0),
  /* don't display color */
  GP_MATERIAL_HIDE = (1 << 1),
  /* protected from further editing */
  GP_MATERIAL_LOCKED = (1 << 2),
  /* do onion skinning */
  GP_MATERIAL_HIDE_ONIONSKIN = (1 << 3),
  /* clamp texture */
  GP_MATERIAL_TEX_CLAMP = (1 << 4),
  /* mix fill texture */
  GP_MATERIAL_FILL_TEX_MIX = (1 << 5),
  /* Flip fill colors */
  GP_MATERIAL_FLIP_FILL = (1 << 6),
  /* Stroke Texture is a pattern */
  GP_MATERIAL_STROKE_PATTERN = (1 << 7),
  /* Stroke show main switch */
  GP_MATERIAL_STROKE_SHOW = (1 << 8),
  /* Fill show main switch */
  GP_MATERIAL_FILL_SHOW = (1 << 9),
  /* mix stroke texture */
  GP_MATERIAL_STROKE_TEX_MIX = (1 << 11),
  /* disable stencil clipping (overlap) */
  GP_MATERIAL_DISABLE_STENCIL = (1 << 12),
  /* Material used as stroke masking. */
  GP_MATERIAL_IS_STROKE_HOLDOUT = (1 << 13),
  /* Material used as fill masking. */
  GP_MATERIAL_IS_FILL_HOLDOUT = (1 << 14),
} eMaterialGPencilStyle_Flag;

typedef enum eMaterialGPencilStyle_Mode {
  GP_MATERIAL_MODE_LINE = 0,
  GP_MATERIAL_MODE_DOT = 1,
  GP_MATERIAL_MODE_SQUARE = 2,
} eMaterialGPencilStyle_Mode;

typedef struct MaterialLineArt {
  /* eMaterialLineArtFlags */
  int flags;

  /* Used to filter line art occlusion edges */
  unsigned char material_mask_bits;

  /** Maximum 255 levels of equivalent occlusion. */
  unsigned char mat_occlusion;

  unsigned char intersection_priority;

  char _pad;
} MaterialLineArt;

typedef enum eMaterialLineArtFlags {
  LRT_MATERIAL_MASK_ENABLED = (1 << 0),
  LRT_MATERIAL_CUSTOM_OCCLUSION_EFFECTIVENESS = (1 << 1),
  LRT_MATERIAL_CUSTOM_INTERSECTION_PRIORITY = (1 << 2),
} eMaterialLineArtFlags;

typedef struct Material {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Material)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_MA;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  short flag;
  /** Rendering modes for EEVEE. */
  char surface_render_method;
  char _pad1[1];

  /* Colors from Blender Internal that we are still using. */
  float r, g, b, a;
  float specr, specg, specb;
  float alpha DNA_DEPRECATED;
  float ray_mirror DNA_DEPRECATED;
  float spec;
  /** Renamed and inversed to roughness. */
  float gloss_mir DNA_DEPRECATED;
  float roughness;
  float metallic;

  /** Nodes */
  char use_nodes DNA_DEPRECATED;

  /** Preview render. */
  char pr_type;
  short pr_texture;
  short pr_flag;

  /** Index for render passes. */
  short index;

  struct bNodeTree *nodetree;
  struct PreviewImage *preview;

  /* Freestyle line settings. */
  float line_col[4];
  short line_priority;
  short vcol_alpha;

  /* Texture painting slots. */
  short paint_active_slot;
  short paint_clone_slot;
  short tot_slots;

  /* Displacement. */
  char displacement_method;

  /* Thickness. */
  char thickness_mode;

  /* Transparency. */
  float alpha_threshold;
  float refract_depth;
  char blend_method; /* TODO(fclem): Deprecate once we remove legacy EEVEE. */
  char blend_shadow; /* TODO(fclem): Deprecate once we remove legacy EEVEE. */
  char blend_flag;

  /* Volume. */
  char volume_intersection_method;

  /* Displacement. */
  float inflate_bounds;

  char _pad3[4];

  /**
   * Cached slots for texture painting, must be refreshed via
   * BKE_texpaint_slot_refresh_cache before using.
   */
  struct TexPaintSlot *texpaintslot;

  /** Runtime cache for GLSL materials. */
  ListBase gpumaterial;

  /** Grease pencil color. */
  struct MaterialGPencilStyle *gp_style;
  struct MaterialLineArt lineart;
} Material;

/* **************** MATERIAL ********************* */

/* maximum number of materials per material array.
 * (on object, mesh, light, etc.). limited by
 * short mat_nr in verts, faces.
 * -1 because for active material we store the index + 1 */
#define MAXMAT (32767 - 1)

/** #Material::flag */
enum {
  /** For render. */
  MA_IS_USED = 1 << 0, /* UNUSED */
  /** For dope-sheet. */
  MA_DS_EXPAND = 1 << 1,
  /**
   * For dope-sheet (texture stack expander)
   * NOTE: this must have the same value as other texture stacks,
   * otherwise anim-editors will not read correctly.
   */
  MA_DS_SHOW_TEXS = 1 << 2,
};

/* ramps */
enum {
  MA_RAMP_BLEND = 0,
  MA_RAMP_ADD = 1,
  MA_RAMP_MULT = 2,
  MA_RAMP_SUB = 3,
  MA_RAMP_SCREEN = 4,
  MA_RAMP_DIV = 5,
  MA_RAMP_DIFF = 6,
  MA_RAMP_DARK = 7,
  MA_RAMP_LIGHT = 8,
  MA_RAMP_OVERLAY = 9,
  MA_RAMP_DODGE = 10,
  MA_RAMP_BURN = 11,
  MA_RAMP_HUE = 12,
  MA_RAMP_SAT = 13,
  MA_RAMP_VAL = 14,
  MA_RAMP_COLOR = 15,
  MA_RAMP_SOFT = 16,
  MA_RAMP_LINEAR = 17,
  MA_RAMP_EXCLUSION = 18,
};

/** #MTex::texco */
enum {
  TEXCO_ORCO = 1 << 0,
  // TEXCO_REFL = 1 << 1, /* Deprecated. */
  // TEXCO_NORM = 1 << 2, /* Deprecated. */
  TEXCO_GLOB = 1 << 3,
  TEXCO_UV = 1 << 4,
  TEXCO_OBJECT = 1 << 5,
  // TEXCO_LAVECTOR = 1 << 6, /* Deprecated. */
  // TEXCO_VIEW = 1 << 7,     /* Deprecated. */
  // TEXCO_STICKY = 1 << 8,   /* Deprecated. */
  // TEXCO_OSA = 1 << 9,      /* Deprecated. */
  TEXCO_WINDOW = 1 << 10,
  // NEED_UV = 1 << 11,       /* Deprecated. */
  // TEXCO_TANGENT = 1 << 12, /* Deprecated. */
  /** still stored in `vertex->accum`, 1 D. */
  TEXCO_STRAND = 1 << 13,
  /** strand is used for normal materials, particle for halo materials */
  TEXCO_PARTICLE = 1 << 13,
  // TEXCO_STRESS = 1 << 14, /* Deprecated. */
  // TEXCO_SPEED = 1 << 15,  /* Deprecated. */
};

/** #MTex::mapto */
enum {
  MAP_COL = 1 << 0,
  MAP_ALPHA = 1 << 7,
};

/** #Material::pr_type */
typedef enum ePreviewType {
  MA_FLAT = 0,
  MA_SPHERE = 1,
  MA_CUBE = 2,
  MA_SHADERBALL = 3,
  MA_SPHERE_A = 4, /* Used for icon renders only. */
  MA_TEXTURE = 5,
  MA_LAMP = 6,
  MA_SKY = 7,
  MA_HAIR = 10,
  MA_ATMOS = 11,
  MA_CLOTH = 12,
  MA_FLUID = 13,
} ePreviewType;

/** #Material::pr_flag */
enum {
  MA_PREVIEW_WORLD = 1 << 0,
};

/** #Material::surface_render_method */
enum {
  MA_SURFACE_METHOD_DEFERRED = 0,
  MA_SURFACE_METHOD_FORWARD = 1,
};

/** #Material::volume_intersection_method */
enum {
  MA_VOLUME_ISECT_FAST = 0,
  MA_VOLUME_ISECT_ACCURATE = 1,
};

/** #Material::blend_method */
enum {
  MA_BM_SOLID = 0,
  // MA_BM_ADD = 1, /* deprecated */
  // MA_BM_MULTIPLY = 2,  /* deprecated */
  MA_BM_CLIP = 3,
  MA_BM_HASHED = 4,
  MA_BM_BLEND = 5,
};

/** #Material::blend_flag */
enum {
  MA_BL_HIDE_BACKFACE = (1 << 0),
  MA_BL_SS_REFRACTION = (1 << 1),
  MA_BL_CULL_BACKFACE = (1 << 2),
  MA_BL_TRANSLUCENCY = (1 << 3),
  MA_BL_LIGHTPROBE_VOLUME_DOUBLE_SIDED = (1 << 4),
  MA_BL_CULL_BACKFACE_SHADOW = (1 << 5),
  MA_BL_TRANSPARENT_SHADOW = (1 << 6),
  MA_BL_THICKNESS_FROM_SHADOW = (1 << 7),
};

/** #Material::blend_shadow */
enum {
  MA_BS_NONE = 0,
  MA_BS_SOLID = 1,
  MA_BS_CLIP = 2,
  MA_BS_HASHED = 3,
};

/** #Material::displacement_method */
enum {
  MA_DISPLACEMENT_BUMP = 0,
  MA_DISPLACEMENT_DISPLACE = 1,
  MA_DISPLACEMENT_BOTH = 2,
};

/** #Material::thickness_mode */
enum {
  MA_THICKNESS_SPHERE = 0,
  MA_THICKNESS_SLAB = 1,
};

/* Grease Pencil Stroke styles */
enum {
  GP_MATERIAL_STROKE_STYLE_SOLID = 0,
  GP_MATERIAL_STROKE_STYLE_TEXTURE = 1,
};

/* Grease Pencil Fill styles */
enum {
  GP_MATERIAL_FILL_STYLE_SOLID = 0,
  GP_MATERIAL_FILL_STYLE_GRADIENT = 1,
  GP_MATERIAL_FILL_STYLE_CHECKER = 2, /* DEPRECATED (only for convert old files) */
  GP_MATERIAL_FILL_STYLE_TEXTURE = 3,
};

/* Grease Pencil Gradient Types */
enum {
  GP_MATERIAL_GRADIENT_LINEAR = 0,
  GP_MATERIAL_GRADIENT_RADIAL = 1,
};

/* Grease Pencil Follow Drawing Modes */
enum {
  GP_MATERIAL_FOLLOW_PATH = 0,
  GP_MATERIAL_FOLLOW_OBJ = 1,
  GP_MATERIAL_FOLLOW_FIXED = 2,
};
