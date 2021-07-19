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
#include "DNA_image_types.h" /* ImageUser */

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct ColorBand;
struct CurveMapping;
struct Image;
struct Ipo;
struct Object;
struct PreviewImage;
struct Tex;

typedef struct MTex {

  short texco, mapto, maptoneg, blendtype;
  struct Object *object;
  struct Tex *tex;
  /** MAX_CUSTOMDATA_LAYER_NAME. */
  char uvname[64];

  char projx, projy, projz, mapping;
  char brush_map_mode, brush_angle_mode;
  char _pad[2];
  float ofs[3], size[3], rot, random_angle;

  char _pad0[2];
  short colormodel;
  short normapspace, which_output;
  float r, g, b, k;
  float def_var;

  /* common */
  float colfac, varfac;

  /* material */
  float norfac, dispfac, warpfac;
  float colspecfac, mirrfac, alphafac;
  float difffac, specfac, emitfac, hardfac;
  float raymirrfac, translfac, ambfac;
  float colemitfac, colreflfac, coltransfac;
  float densfac, scatterfac, reflfac;

  /* particles */
  float timefac, lengthfac, clumpfac, dampfac;
  float kinkfac, kinkampfac, roughfac, padensfac, gravityfac;
  float lifefac, sizefac, ivelfac, fieldfac;
  float twistfac;

  /* light */
  float shadowfac;

  /* world */
  float zenupfac, zendownfac, blendfac;
} MTex;

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

/* 32 = MAXCOLORBAND */
/* note that this has to remain a single struct, for UserDef */
typedef struct ColorBand {
  short tot, cur;
  char ipotype, ipotype_hue;
  char color_mode;
  char _pad[1];

  CBData data[32];
} ColorBand;

typedef struct PointDensity {
  short flag;

  short falloff_type;
  float falloff_softness;
  float radius;
  short source;
  char _pad0[2];

  /** psys_color_source */
  short color_source;
  short ob_color_source;

  int totpoints;

  /** for 'Object' or 'Particle system' type - source object */
  struct Object *object;
  /** `index + 1` in ob.particlesystem, non-ID pointer not allowed */
  int psys;
  /** cache points in worldspace, object space, ... ? */
  short psys_cache_space;
  /** cache points in worldspace, object space, ... ? */
  short ob_cache_space;
  /** vertex attribute layer for color source, MAX_CUSTOMDATA_LAYER_NAME */
  char vertex_attribute_name[64];

  /** The acceleration tree containing points. */
  void *point_tree;
  /** Dynamically allocated extra for extra information, like particle age. */
  float *point_data;

  float noise_size;
  short noise_depth;
  short noise_influence;
  short noise_basis;
  char _pad1[6];
  float noise_fac;

  float speed_scale, falloff_speed_scale;
  char _pad2[4];
  /** For time -> color */
  struct ColorBand *coba;

  /** Falloff density curve. */
  struct CurveMapping *falloff_curve;
} PointDensity;

typedef struct Tex {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  float noisesize, turbul;
  float bright, contrast, saturation, rfac, gfac, bfac;
  float filtersize;
  char _pad2[4];

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
  int texfilter;
  int afmax; /* anisotropic filter maximum value, ewa -> max eccentricity, feline -> max probes */
  short xrepeat, yrepeat;
  short extend;

  /* variables disabled, moved to struct iuser */
  short _pad0;
  int len;
  int frames, offset, sfra;

  float checkerdist, nabla;
  char _pad1[4];

  struct ImageUser iuser;

  struct bNodeTree *nodetree;
  /* old animation system, deprecated for 2.5 */
  struct Ipo *ipo DNA_DEPRECATED;
  struct Image *ima;
  struct ColorBand *coba;
  struct PreviewImage *preview;

  char use_nodes;
  char _pad[7];

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

/* texmap->flag */
#define TEXMAP_CLIP_MIN 1
#define TEXMAP_CLIP_MAX 2
#define TEXMAP_UNIT_MATRIX 4

/* texmap->type */
#define TEXMAP_TYPE_POINT 0
#define TEXMAP_TYPE_TEXTURE 1
#define TEXMAP_TYPE_VECTOR 2
#define TEXMAP_TYPE_NORMAL 3

/* colormap->flag */
#define COLORMAP_USE_RAMP 1

/* **************** TEX ********************* */

/* type */
#define TEX_CLOUDS 1
#define TEX_WOOD 2
#define TEX_MARBLE 3
#define TEX_MAGIC 4
#define TEX_BLEND 5
#define TEX_STUCCI 6
#define TEX_NOISE 7
#define TEX_IMAGE 8
//#define TEX_PLUGIN        9 /* Deprecated */
//#define TEX_ENVMAP        10 /* Deprecated */
#define TEX_MUSGRAVE 11
#define TEX_VORONOI 12
#define TEX_DISTNOISE 13
//#define TEX_POINTDENSITY  14 /* Deprecated */
//#define TEX_VOXELDATA     15 /* Deprecated */
//#define TEX_OCEAN         16 /* Deprecated */

/* musgrave stype */
#define TEX_MFRACTAL 0
#define TEX_RIDGEDMF 1
#define TEX_HYBRIDMF 2
#define TEX_FBM 3
#define TEX_HTERRAIN 4

/* newnoise: noisebasis 1 & 2 */
#define TEX_BLENDER 0
#define TEX_STDPERLIN 1
#define TEX_NEWPERLIN 2
#define TEX_VORONOI_F1 3
#define TEX_VORONOI_F2 4
#define TEX_VORONOI_F3 5
#define TEX_VORONOI_F4 6
#define TEX_VORONOI_F2F1 7
#define TEX_VORONOI_CRACKLE 8
#define TEX_CELLNOISE 14

/* newnoise: Voronoi distance metrics, vn_distm */
#define TEX_DISTANCE 0
#define TEX_DISTANCE_SQUARED 1
#define TEX_MANHATTAN 2
#define TEX_CHEBYCHEV 3
#define TEX_MINKOVSKY_HALF 4
#define TEX_MINKOVSKY_FOUR 5
#define TEX_MINKOVSKY 6

/* imaflag */
#define TEX_INTERPOL (1 << 0)
#define TEX_USEALPHA (1 << 1)
#define TEX_MIPMAP (1 << 2)
#define TEX_IMAROT (1 << 4)
#define TEX_CALCALPHA (1 << 5)
#define TEX_NORMALMAP (1 << 11)
#define TEX_GAUSS_MIP (1 << 12)
#define TEX_FILTER_MIN (1 << 13)
#define TEX_DERIVATIVEMAP (1 << 14)

/* texfilter */
#define TXF_BOX 0 /* Blender's old texture filtering method. */
#define TXF_EWA 1
#define TXF_FELINE 2
#define TXF_AREA 3

/* flag */
#define TEX_COLORBAND (1 << 0)
#define TEX_FLIPBLEND (1 << 1)
#define TEX_NEGALPHA (1 << 2)
#define TEX_CHECKER_ODD (1 << 3)
#define TEX_CHECKER_EVEN (1 << 4)
#define TEX_PRV_ALPHA (1 << 5)
#define TEX_PRV_NOR (1 << 6)
#define TEX_REPEAT_XMIR (1 << 7)
#define TEX_REPEAT_YMIR (1 << 8)
#define TEX_FLAG_MASK \
  (TEX_COLORBAND | TEX_FLIPBLEND | TEX_NEGALPHA | TEX_CHECKER_ODD | TEX_CHECKER_EVEN | \
   TEX_PRV_ALPHA | TEX_PRV_NOR | TEX_REPEAT_XMIR | TEX_REPEAT_YMIR)
#define TEX_DS_EXPAND (1 << 9)
#define TEX_NO_CLAMP (1 << 10)

/* extend (starts with 1 because of backward comp.) */
#define TEX_EXTEND 1
#define TEX_CLIP 2
#define TEX_REPEAT 3
#define TEX_CLIPCUBE 4
#define TEX_CHECKER 5

/* noisetype */
#define TEX_NOISESOFT 0
#define TEX_NOISEPERL 1

/* tex->noisebasis2 in texture.c - wood waveforms */
#define TEX_SIN 0
#define TEX_SAW 1
#define TEX_TRI 2

/* tex->stype in texture.c - wood types */
#define TEX_BAND 0
#define TEX_RING 1
#define TEX_BANDNOISE 2
#define TEX_RINGNOISE 3

/* tex->stype in texture.c - cloud types */
#define TEX_DEFAULT 0
#define TEX_COLOR 1

/* tex->stype in texture.c - marble types */
#define TEX_SOFT 0
#define TEX_SHARP 1
#define TEX_SHARPER 2

/* tex->stype in texture.c - blend types */
#define TEX_LIN 0
#define TEX_QUAD 1
#define TEX_EASE 2
#define TEX_DIAG 3
#define TEX_SPHERE 4
#define TEX_HALO 5
#define TEX_RAD 6

/* tex->stype in texture.c - stucci types */
#define TEX_PLASTIC 0
#define TEX_WALLIN 1
#define TEX_WALLOUT 2

/* tex->stype in texture.c - voronoi types */
#define TEX_INTENSITY 0
#define TEX_COL1 1
#define TEX_COL2 2
#define TEX_COL3 3

/* mtex->normapspace */
#define MTEX_NSPACE_CAMERA 0
#define MTEX_NSPACE_WORLD 1
#define MTEX_NSPACE_OBJECT 2
#define MTEX_NSPACE_TANGENT 3

/* wrap */
#define MTEX_FLAT 0
#define MTEX_CUBE 1
#define MTEX_TUBE 2
#define MTEX_SPHERE 3

/* return value */
#define TEX_INT 0
#define TEX_RGB (1 << 0)
#define TEX_NOR (1 << 1)

/* pr_texture in material, world, light. */
#define TEX_PR_TEXTURE 0
#define TEX_PR_OTHER 1
#define TEX_PR_BOTH 2

/* **************** MTEX ********************* */

/* proj */
#define PROJ_N 0
#define PROJ_X 1
#define PROJ_Y 2
#define PROJ_Z 3

/* blendtype */
#define MTEX_BLEND 0
#define MTEX_MUL 1
#define MTEX_ADD 2
#define MTEX_SUB 3
#define MTEX_DIV 4
#define MTEX_DARK 5
#define MTEX_DIFF 6
#define MTEX_LIGHT 7
#define MTEX_SCREEN 8
#define MTEX_OVERLAY 9
#define MTEX_BLEND_HUE 10
#define MTEX_BLEND_SAT 11
#define MTEX_BLEND_VAL 12
#define MTEX_BLEND_COLOR 13
#define MTEX_SOFT_LIGHT 15
#define MTEX_LIN_LIGHT 16

/* brush_map_mode */
#define MTEX_MAP_MODE_VIEW 0
#define MTEX_MAP_MODE_TILED 1
#define MTEX_MAP_MODE_3D 2
#define MTEX_MAP_MODE_AREA 3
#define MTEX_MAP_MODE_RANDOM 4
#define MTEX_MAP_MODE_STENCIL 5

/* brush_angle_mode */
#define MTEX_ANGLE_RANDOM 1
#define MTEX_ANGLE_RAKE 2

/* **************** ColorBand ********************* */

/* colormode */
enum {
  COLBAND_BLEND_RGB = 0,
  COLBAND_BLEND_HSV = 1,
  COLBAND_BLEND_HSL = 2,
};

/* interpolation */
enum {
  COLBAND_INTERP_LINEAR = 0,
  COLBAND_INTERP_EASE = 1,
  COLBAND_INTERP_B_SPLINE = 2,
  COLBAND_INTERP_CARDINAL = 3,
  COLBAND_INTERP_CONSTANT = 4,
};

/* color interpolation */
enum {
  COLBAND_HUE_NEAR = 0,
  COLBAND_HUE_FAR = 1,
  COLBAND_HUE_CW = 2,
  COLBAND_HUE_CCW = 3,
};

/* **************** PointDensity ********************* */

/* source */
#define TEX_PD_PSYS 0
#define TEX_PD_OBJECT 1
#define TEX_PD_FILE 2

/* falloff_type */
#define TEX_PD_FALLOFF_STD 0
#define TEX_PD_FALLOFF_SMOOTH 1
#define TEX_PD_FALLOFF_SOFT 2
#define TEX_PD_FALLOFF_CONSTANT 3
#define TEX_PD_FALLOFF_ROOT 4
#define TEX_PD_FALLOFF_PARTICLE_AGE 5
#define TEX_PD_FALLOFF_PARTICLE_VEL 6

/* psys_cache_space */
#define TEX_PD_OBJECTLOC 0
#define TEX_PD_OBJECTSPACE 1
#define TEX_PD_WORLDSPACE 2

/* flag */
#define TEX_PD_TURBULENCE 1
#define TEX_PD_FALLOFF_CURVE 2

/* noise_influence */
#define TEX_PD_NOISE_STATIC 0
/* #define TEX_PD_NOISE_VEL     1 */ /* Deprecated */
/* #define TEX_PD_NOISE_AGE     2 */ /* Deprecated */
/* #define TEX_PD_NOISE_TIME    3 */ /* Deprecated */

/* color_source */
enum {
  TEX_PD_COLOR_CONSTANT = 0,
  /* color_source: particles */
  TEX_PD_COLOR_PARTAGE = 1,
  TEX_PD_COLOR_PARTSPEED = 2,
  TEX_PD_COLOR_PARTVEL = 3,
  /* color_source: vertices */
  TEX_PD_COLOR_VERTCOL = 1,
  TEX_PD_COLOR_VERTWEIGHT = 2,
  TEX_PD_COLOR_VERTNOR = 3,
};

#define POINT_DATA_VEL 1
#define POINT_DATA_LIFE 2
#define POINT_DATA_COLOR 4

#ifdef __cplusplus
}
#endif
