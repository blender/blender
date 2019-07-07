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

#ifndef __DNA_MATERIAL_TYPES_H__
#define __DNA_MATERIAL_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_listBase.h"

#ifndef MAX_MTEX
#  define MAX_MTEX 18
#endif

struct AnimData;
struct Image;
struct Ipo;
struct bNodeTree;

/* WATCH IT: change type? also make changes in ipo.h  */

typedef struct TexPaintSlot {
  /** Image to be painted on. */
  struct Image *ima;
  /** Customdata index for uv layer, MAX_NAM.E*/
  char *uvname;
  /** Do we have a valid image and UV map. */
  int valid;
  /** Copy of node inteporlation setting. */
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
  float gradient_angle;
  /** Radius for radial gradients. */
  float gradient_radius;
  /** Cheesboard size. */
  float pattern_gridsize;
  /** Uv coordinates scale. */
  float gradient_scale[2];
  /** Factor to shift filling in 2d space. */
  float gradient_shift[2];
  /** Angle used for texture orientation. */
  float texture_angle;
  /** Texture scale (separated of uv scale). */
  float texture_scale[2];
  /** Factor to shift texture in 2d space. */
  float texture_offset[2];
  /** Texture opacity. */
  float texture_opacity;
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
  char _pad[4];
} MaterialGPencilStyle;

/* MaterialGPencilStyle->flag */
typedef enum eMaterialGPencilStyle_Flag {
  /* Fill Texture is a pattern */
  GP_STYLE_FILL_PATTERN = (1 << 0),
  /* don't display color */
  GP_STYLE_COLOR_HIDE = (1 << 1),
  /* protected from further editing */
  GP_STYLE_COLOR_LOCKED = (1 << 2),
  /* do onion skinning */
  GP_STYLE_COLOR_ONIONSKIN = (1 << 3),
  /* clamp texture */
  GP_STYLE_COLOR_TEX_CLAMP = (1 << 4),
  /* mix fill texture */
  GP_STYLE_FILL_TEX_MIX = (1 << 5),
  /* Flip fill colors */
  GP_STYLE_COLOR_FLIP_FILL = (1 << 6),
  /* Stroke Texture is a pattern */
  GP_STYLE_STROKE_PATTERN = (1 << 7),
  /* Stroke show main switch */
  GP_STYLE_STROKE_SHOW = (1 << 8),
  /* Fill  show main switch */
  GP_STYLE_FILL_SHOW = (1 << 9),
  /* mix stroke texture */
  GP_STYLE_STROKE_TEX_MIX = (1 << 11),
} eMaterialGPencilStyle_Flag;

typedef enum eMaterialGPencilStyle_Mode {
  GP_STYLE_MODE_LINE = 0, /* line */
  GP_STYLE_MODE_DOTS = 1, /* dots */
  GP_STYLE_MODE_BOX = 2,  /* rectangles */
} eMaterialGPencilStyle_Mode;

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

/* pmapto */
/* init */
#define MAP_PA_INIT ((1 << 5) - 1)
#define MAP_PA_TIME (1 << 0)
#define MAP_PA_LIFE (1 << 1)
#define MAP_PA_DENS (1 << 2)
#define MAP_PA_SIZE (1 << 3)
#define MAP_PA_LENGTH (1 << 4)
/* reset */
#define MAP_PA_IVEL (1 << 5)
/* physics */
#define MAP_PA_PVEL (1 << 6)
/* path cache */
#define MAP_PA_CLUMP (1 << 7)
#define MAP_PA_KINK (1 << 8)
#define MAP_PA_ROUGH (1 << 9)
#define MAP_PA_FREQ (1 << 10)

/* pr_type */
#define MA_FLAT 0
#define MA_SPHERE 1
#define MA_CUBE 2
#define MA_SHADERBALL 3
#define MA_SPHERE_A 4 /* Used for icon renders only. */
#define MA_TEXTURE 5
#define MA_LAMP 6
#define MA_SKY 7
#define MA_HAIR 10
#define MA_ATMOS 11
#define MA_CLOTH 12
#define MA_FLUID 13

/* pr_flag */
#define MA_PREVIEW_WORLD (1 << 0)

/* blend_method */
enum {
  MA_BM_SOLID,
  MA_BM_ADD,
  MA_BM_MULTIPLY,
  MA_BM_CLIP,
  MA_BM_HASHED,
  MA_BM_BLEND,
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
  MA_BS_SOLID,
  MA_BS_CLIP,
  MA_BS_HASHED,
};

/* Grease Pencil Stroke styles */
enum {
  GP_STYLE_STROKE_STYLE_SOLID = 0,
  GP_STYLE_STROKE_STYLE_TEXTURE,
};

/* Grease Pencil Fill styles */
enum {
  GP_STYLE_FILL_STYLE_SOLID = 0,
  GP_STYLE_FILL_STYLE_GRADIENT,
  GP_STYLE_FILL_STYLE_CHECKER,
  GP_STYLE_FILL_STYLE_TEXTURE,
};

/* Grease Pencil Gradient Types */
enum {
  GP_STYLE_GRADIENT_LINEAR = 0,
  GP_STYLE_GRADIENT_RADIAL,
};

/* Grease Pencil Follow Drawing Modes */
enum {
  GP_STYLE_FOLLOW_PATH = 0,
  GP_STYLE_FOLLOW_OBJ,
  GP_STYLE_FOLLOW_FIXED,
};
#endif
