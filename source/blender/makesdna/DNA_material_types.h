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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_MTEX
#  define MAX_MTEX 18
#endif

struct AnimData;
struct Image;
struct Ipo;
struct bNodeTree;

/* WATCH IT: change type? also make changes in ipo.h */

typedef struct TexPaintSlot {
  /** Image to be painted on. */
  struct Image *ima;
  /** Custom-data index for uv layer, #MAX_NAME. */
  char *uvname;
  /** Do we have a valid image and UV map. */
  int valid;
  /** Copy of node interpolation setting. */
  int interp;
} TexPaintSlot;

typedef struct MaterialGPencilStyle {
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
  /** Uv coordinates scale. */
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

  unsigned char _pad[2];
} MaterialLineArt;

typedef enum eMaterialLineArtFlags {
  LRT_MATERIAL_MASK_ENABLED = (1 << 0),

  /* Deprecated, kept for versioning code. */
  LRT_MATERIAL_CUSTOM_OCCLUSION_EFFECTIVENESS = (1 << 1),
} eMaterialLineArtFlags;

typedef struct Material {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  short flag;
  char _pad1[2];

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
  char use_nodes;

  /** Preview render. */
  char pr_type;
  short pr_texture;
  short pr_flag;

  /** Index for render passes. */
  short index;

  struct bNodeTree *nodetree;
  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;
  struct PreviewImage *preview;

  /* Freestyle line settings. */
  float line_col[4];
  short line_priority;
  short vcol_alpha;

  /* Texture painting slots. */
  short paint_active_slot;
  short paint_clone_slot;
  short tot_slots;
  char _pad2[2];

  /* Transparency. */
  float alpha_threshold;
  float refract_depth;
  char blend_method;
  char blend_shadow;
  char blend_flag;
  char _pad3[1];

  /**
   * Cached slots for texture painting, must be refreshed in
   * refresh_texpaint_image_cache before using.
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

/* flag */
/* for render */
/* #define MA_IS_USED      (1 << 0) */ /* UNUSED */
                                       /* for dopesheet */
#define MA_DS_EXPAND (1 << 1)
/* for dopesheet (texture stack expander)
 * NOTE: this must have the same value as other texture stacks,
 * otherwise anim-editors will not read correctly
 */
#define MA_DS_SHOW_TEXS (1 << 2)

/* ramps */
#define MA_RAMP_BLEND 0
#define MA_RAMP_ADD 1
#define MA_RAMP_MULT 2
#define MA_RAMP_SUB 3
#define MA_RAMP_SCREEN 4
#define MA_RAMP_DIV 5
#define MA_RAMP_DIFF 6
#define MA_RAMP_DARK 7
#define MA_RAMP_LIGHT 8
#define MA_RAMP_OVERLAY 9
#define MA_RAMP_DODGE 10
#define MA_RAMP_BURN 11
#define MA_RAMP_HUE 12
#define MA_RAMP_SAT 13
#define MA_RAMP_VAL 14
#define MA_RAMP_COLOR 15
#define MA_RAMP_SOFT 16
#define MA_RAMP_LINEAR 17

/* texco */
#define TEXCO_ORCO (1 << 0)
/* #define TEXCO_REFL      (1 << 1) */ /* deprecated */
/* #define TEXCO_NORM      (1 << 2) */ /* deprecated */
#define TEXCO_GLOB (1 << 3)
#define TEXCO_UV (1 << 4)
#define TEXCO_OBJECT (1 << 5)
/* #define TEXCO_LAVECTOR  (1 << 6) */ /* deprecated */
/* #define TEXCO_VIEW      (1 << 7) */ /* deprecated */
/* #define TEXCO_STICKY   (1 << 8) */  /* deprecated */
/* #define TEXCO_OSA       (1 << 9) */ /* deprecated */
#define TEXCO_WINDOW (1 << 10)
/* #define NEED_UV         (1 << 11) */ /* deprecated */
/* #define TEXCO_TANGENT   (1 << 12) */ /* deprecated */
/* still stored in vertex->accum, 1 D */
#define TEXCO_STRAND (1 << 13)
/** strand is used for normal materials, particle for halo materials */
#define TEXCO_PARTICLE (1 << 13)
/* #define TEXCO_STRESS    (1 << 14) */ /* deprecated */
/* #define TEXCO_SPEED     (1 << 15) */ /* deprecated */

/* mapto */
#define MAP_COL (1 << 0)
#define MAP_ALPHA (1 << 7)

/* pr_type */
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

/* pr_flag */
#define MA_PREVIEW_WORLD (1 << 0)

/* blend_method */
enum {
  MA_BM_SOLID = 0,
  // MA_BM_ADD = 1, /* deprecated */
  // MA_BM_MULTIPLY = 2,  /* deprecated */
  MA_BM_CLIP = 3,
  MA_BM_HASHED = 4,
  MA_BM_BLEND = 5,
};

/* blend_flag */
enum {
  MA_BL_HIDE_BACKFACE = (1 << 0),
  MA_BL_SS_REFRACTION = (1 << 1),
  MA_BL_CULL_BACKFACE = (1 << 2),
  MA_BL_TRANSLUCENCY = (1 << 3),
};

/* blend_shadow */
enum {
  MA_BS_NONE = 0,
  MA_BS_SOLID = 1,
  MA_BS_CLIP = 2,
  MA_BS_HASHED = 3,
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

#ifdef __cplusplus
}
#endif
