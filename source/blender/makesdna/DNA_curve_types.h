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
#include "DNA_vec_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAXTEXTBOX 256 /* used in readfile.c and editfont.c */

struct AnimData;
struct CurveEval;
struct CurveProfile;
struct EditFont;
struct GHash;
struct Ipo;
struct Key;
struct Material;
struct Object;
struct VFont;

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct BevPoint {
  float vec[3], tilt, radius, weight, offset;
  /** 2D Only. */
  float sina, cosa;
  /** 3D Only. */
  float dir[3], tan[3], quat[4];
  short dupe_tag;
} BevPoint;

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct BevList {
  struct BevList *next, *prev;
  int nr, dupe_nr;
  int poly, hole;
  int charidx;
  int *segbevcount;
  float *seglen;
  BevPoint *bevpoints;
} BevList;

/**
 * Keyframes on F-Curves (allows code reuse of Bezier eval code) and
 * Points on Bezier Curves/Paths are generally BezTriples.
 *
 * \note #BezTriple.tilt location in struct is abused by Key system.
 *
 * \note vec in BezTriple looks like this:
 * - vec[0][0] = x location of handle 1
 * - vec[0][1] = y location of handle 1
 * - vec[0][2] = z location of handle 1 (not used for FCurve Points(2d))
 * - vec[1][0] = x location of control point
 * - vec[1][1] = y location of control point
 * - vec[1][2] = z location of control point
 * - vec[2][0] = x location of handle 2
 * - vec[2][1] = y location of handle 2
 * - vec[2][2] = z location of handle 2 (not used for FCurve Points(2d))
 */
typedef struct BezTriple {
  float vec[3][3];
  /** Tilt in 3D View. */
  float tilt;
  /** Used for softbody goal weight. */
  float weight;
  /** For bevel tapering & modifiers. */
  float radius;

  /** Ipo: interpolation mode for segment from this BezTriple to the next. */
  char ipo;

  /** H1, h2: the handle type of the two handles. */
  uint8_t h1, h2;
  /** F1, f2, f3: used for selection status. */
  uint8_t f1, f2, f3;

  /** Hide: used to indicate whether BezTriple is hidden (3D),
   * type of keyframe (eBezTriple_KeyframeType). */
  char hide;

  /** Easing: easing type for interpolation mode (eBezTriple_Easing). */
  char easing;
  /** BEZT_IPO_BACK. */
  float back;
  /** BEZT_IPO_ELASTIC. */
  float amplitude, period;

  /** Used during auto handle calculation to mark special cases (local extremes). */
  char auto_handle_type;
  char _pad[3];
} BezTriple;

/**
 * \note #BPoint.tilt location in struct is abused by Key system.
 */
typedef struct BPoint {
  float vec[4];
  /** Tilt in 3D View. */
  float tilt;
  /** Used for softbody goal weight. */
  float weight;
  /** F1: selection status,  hide: is point hidden or not. */
  uint8_t f1;
  char _pad1[1];
  short hide;
  /** User-set radius per point for beveling etc. */
  float radius;
  char _pad[4];
} BPoint;

/**
 * \note Nurb name is misleading, since it can be used for polygons too,
 * also, it should be NURBS (Nurb isn't the singular of Nurbs).
 */
typedef struct Nurb {
  /** Multiple nurbs per curve object are allowed. */
  struct Nurb *next, *prev;
  short type;
  /** Index into material list. */
  short mat_nr;
  short hide, flag;
  /** Number of points in the U or V directions. */
  int pntsu, pntsv;
  char _pad[4];
  /** Tessellation resolution in the U or V directions. */
  short resolu, resolv;
  short orderu, orderv;
  short flagu, flagv;

  float *knotsu, *knotsv;
  BPoint *bp;
  BezTriple *bezt;

  /** KEY_LINEAR, KEY_CARDINAL, KEY_BSPLINE. */
  short tilt_interp;
  short radius_interp;

  /* only used for dynamically generated Nurbs created from OB_FONT's */
  int charidx;
} Nurb;

typedef struct CharInfo {
  short kern;
  /** Index start at 1, unlike mesh & nurbs. */
  short mat_nr;
  char flag;
  char _pad[3];
} CharInfo;

typedef struct TextBox {
  float x, y, w, h;
} TextBox;

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct EditNurb {
  /* base of nurbs' list (old Curve->editnurb) */
  ListBase nurbs;

  /* index data for shape keys */
  struct GHash *keyindex;

  /* shape key being edited */
  int shapenr;

  /**
   * ID data is older than edit-mode data.
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;

} EditNurb;

typedef struct Curve {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /** Actual data, called splines in rna. */
  ListBase nurb;

  /** Edited data, not in file, use pointer so we can check for it. */
  EditNurb *editnurb;

  struct Object *bevobj, *taperobj, *textoncurve;
  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;
  struct Key *key;
  struct Material **mat;

  struct CurveProfile *bevel_profile;

  /* texture space, copied as one block in editobject.c */
  float loc[3];
  float size[3];

  /** Creation-time type of curve datablock. */
  short type;

  /** Keep a short because of BKE_object_obdata_texspace_get(). */
  short texflag;
  char _pad0[6];
  short twist_mode;
  float twist_smooth, smallcaps_scale;

  int pathlen;
  short bevresol, totcol;
  int flag;
  float width, ext1, ext2;

  /* default */
  short resolu, resolv;
  short resolu_ren, resolv_ren;

  /* edit, index in nurb list */
  int actnu;
  /* edit, index in active nurb (BPoint or BezTriple) */
  int actvert;

  char overflow;
  char spacemode, align_y;
  char bevel_mode;
  /**
   * Determine how the effective radius of the bevel point is computed when a taper object is
   * specified. The effective radius is a function of the bevel point radius and the taper radius.
   */
  char taper_radius_mode;
  char _pad;

  /* font part */
  short lines;
  float spacing, linedist, shear, fsize, wordspace, ulpos, ulheight;
  float xof, yof;
  float linewidth;

  /* copy of EditFont vars (wchar_t aligned),
   * warning! don't use in editmode (storage only) */
  int pos;
  int selstart, selend;

  /* text data */
  /**
   * Number of characters (unicode code-points)
   * This is the length of #Curve.strinfo and the result of `BLI_strlen_utf8(cu->str)`.
   */
  int len_char32;
  /** Number of bytes: `strlen(Curve.str)`. */
  int len;
  char *str;
  struct EditFont *editfont;

  char family[64];
  struct VFont *vfont;
  struct VFont *vfontb;
  struct VFont *vfonti;
  struct VFont *vfontbi;

  struct TextBox *tb;
  int totbox, actbox;

  struct CharInfo *strinfo;
  struct CharInfo curinfo;
  /* font part end */

  /** Current evaltime - for use by Objects parented to curves. */
  float ctime;
  float bevfac1, bevfac2;
  char bevfac1_mapping, bevfac2_mapping;

  char _pad2[6];
  float fsize_realtime;

  /**
   * A pointer to curve data from evaluation. Owned by the object's #geometry_set_eval, either as a
   * geometry instance or the data of the evaluated #CurveComponent. The curve may also contain
   * data in the #nurb list, but for evaluated curves this is the proper place to retrieve data,
   * since it also contains the result of geometry nodes evaluation, and isn't just a copy of the
   * original object data.
   */
  struct CurveEval *curve_eval;

  void *batch_cache;
} Curve;

#define CURVE_VFONT_ANY(cu) ((cu)->vfont), ((cu)->vfontb), ((cu)->vfonti), ((cu)->vfontbi)

/* **************** CURVE ********************* */

/* Curve.texflag */
enum {
  CU_AUTOSPACE = 1,
  CU_AUTOSPACE_EVALUATED = 2,
};

#if 0 /* Moved to overlay options in 2.8 */
/* Curve.drawflag */
enum {
  CU_HIDE_HANDLES = 1 << 0,
  CU_HIDE_NORMALS = 1 << 1,
};
#endif

/* Curve.flag */
enum {
  CU_3D = 1 << 0,
  CU_FRONT = 1 << 1,
  CU_BACK = 1 << 2,
  CU_PATH = 1 << 3,
  CU_FOLLOW = 1 << 4,
  CU_PATH_CLAMP = 1 << 5,
  CU_DEFORM_BOUNDS_OFF = 1 << 6,
  CU_STRETCH = 1 << 7,
  /* CU_OFFS_PATHDIST   = 1 << 8, */  /* DEPRECATED */
  CU_FAST = 1 << 9,                   /* Font: no filling inside editmode */
  /* CU_RETOPO          = 1 << 10, */ /* DEPRECATED */
  CU_DS_EXPAND = 1 << 11,
  /** make use of the path radius if this is enabled (default for new curves) */
  CU_PATH_RADIUS = 1 << 12,
  /* CU_DEFORM_FILL = 1 << 13, */ /* DEPRECATED */
  /** fill bevel caps */
  CU_FILL_CAPS = 1 << 14,
  /** map taper object to beveled area */
  CU_MAP_TAPER = 1 << 15,
};

/* Curve.twist_mode */
enum {
  CU_TWIST_Z_UP = 0,
  /* CU_TWIST_Y_UP      = 1, */ /* not used yet */
  /* CU_TWIST_X_UP      = 2, */
  CU_TWIST_MINIMUM = 3,
  CU_TWIST_TANGENT = 4,
};

/* Curve.bevfac1_mapping, Curve.bevfac2_mapping, bevel factor mapping */
enum {
  CU_BEVFAC_MAP_RESOLU = 0,
  CU_BEVFAC_MAP_SEGMENT = 1,
  CU_BEVFAC_MAP_SPLINE = 2,
};

/* Curve.spacemode */
enum {
  CU_ALIGN_X_LEFT = 0,
  CU_ALIGN_X_MIDDLE = 1,
  CU_ALIGN_X_RIGHT = 2,
  CU_ALIGN_X_JUSTIFY = 3,
  CU_ALIGN_X_FLUSH = 4,
};

/* Curve.align_y */
enum {
  CU_ALIGN_Y_TOP_BASELINE = 0,
  CU_ALIGN_Y_TOP = 1,
  CU_ALIGN_Y_CENTER = 2,
  CU_ALIGN_Y_BOTTOM_BASELINE = 3,
  CU_ALIGN_Y_BOTTOM = 4,
};

/* Curve.bevel_mode */
enum {
  CU_BEV_MODE_ROUND = 0,
  CU_BEV_MODE_OBJECT = 1,
  CU_BEV_MODE_CURVE_PROFILE = 2,
};

/** #Curve.taper_radius_mode */
enum {
  /** Override the radius of the bevel point with the taper radius. */
  CU_TAPER_RADIUS_OVERRIDE = 0,
  /** Multiply the radius of the bevel point by the taper radius. */
  CU_TAPER_RADIUS_MULTIPLY = 1,
  /** Add the radius of the bevel point to the taper radius. */
  CU_TAPER_RADIUS_ADD = 2,
};

/* Curve.overflow. */
enum {
  CU_OVERFLOW_NONE = 0,
  CU_OVERFLOW_SCALE = 1,
  CU_OVERFLOW_TRUNCATE = 2,
};

/* Nurb.flag */
enum {
  CU_SMOOTH = 1 << 0,
};

/* Nurb.type */
enum {
  CU_POLY = 0,
  CU_BEZIER = 1,
  CU_BSPLINE = 2,
  CU_CARDINAL = 3,
  CU_NURBS = 4,
  CU_TYPE = (CU_POLY | CU_BEZIER | CU_BSPLINE | CU_CARDINAL | CU_NURBS),

  /* only for adding */
  CU_PRIMITIVE = 0xF00,

  /* 2 or 4 points */
  CU_PRIM_CURVE = 0x100,
  /* 8 points circle */
  CU_PRIM_CIRCLE = 0x200,
  /* 4x4 patch Nurb */
  CU_PRIM_PATCH = 0x300,
  CU_PRIM_TUBE = 0x400,
  CU_PRIM_SPHERE = 0x500,
  CU_PRIM_DONUT = 0x600,
  /* 5 points,  5th order straight line (for anim path) */
  CU_PRIM_PATH = 0x700,
};

/* Nurb.flagu, Nurb.flagv */
enum {
  CU_NURB_CYCLIC = 1 << 0,
  CU_NURB_ENDPOINT = 1 << 1,
  CU_NURB_BEZIER = 1 << 2,
};

#define CU_ACT_NONE -1

/* *************** BEZTRIPLE **************** */

/* BezTriple.f1,2,3 */
typedef enum eBezTriple_Flag {
  /* SELECT */
  BEZT_FLAG_TEMP_TAG = (1 << 1), /* always clear. */
} eBezTriple_Flag;

/* h1 h2 (beztriple) */
typedef enum eBezTriple_Handle {
  HD_FREE = 0,
  HD_AUTO = 1,
  HD_VECT = 2,
  HD_ALIGN = 3,
  HD_AUTO_ANIM = 4,        /* auto-clamped handles for animation */
  HD_ALIGN_DOUBLESIDE = 5, /* align handles, displayed both of them. used for masks */
} eBezTriple_Handle;

/* auto_handle_type (beztriple) */
typedef enum eBezTriple_Auto_Type {
  /* Normal automatic handle that can be refined further. */
  HD_AUTOTYPE_NORMAL = 0,
  /* Handle locked horizontal due to being an Auto Clamped local
   * extreme or a curve endpoint with Constant extrapolation.
   * Further smoothing is disabled. */
  HD_AUTOTYPE_LOCKED_FINAL = 1,
} eBezTriple_Auto_Type;

/* interpolation modes (used only for BezTriple->ipo) */
typedef enum eBezTriple_Interpolation {
  /* traditional interpolation */
  BEZT_IPO_CONST = 0, /* constant interpolation */
  BEZT_IPO_LIN = 1,   /* linear interpolation */
  BEZT_IPO_BEZ = 2,   /* bezier interpolation */

  /* easing equations */
  BEZT_IPO_BACK = 3,
  BEZT_IPO_BOUNCE = 4,
  BEZT_IPO_CIRC = 5,
  BEZT_IPO_CUBIC = 6,
  BEZT_IPO_ELASTIC = 7,
  BEZT_IPO_EXPO = 8,
  BEZT_IPO_QUAD = 9,
  BEZT_IPO_QUART = 10,
  BEZT_IPO_QUINT = 11,
  BEZT_IPO_SINE = 12,
} eBezTriple_Interpolation;

/* easing modes (used only for Keyframes - BezTriple->easing) */
typedef enum eBezTriple_Easing {
  BEZT_IPO_EASE_AUTO = 0,

  BEZT_IPO_EASE_IN = 1,
  BEZT_IPO_EASE_OUT = 2,
  BEZT_IPO_EASE_IN_OUT = 3,
} eBezTriple_Easing;

/* types of keyframe (used only for BezTriple->hide when BezTriple is used in F-Curves) */
typedef enum eBezTriple_KeyframeType {
  BEZT_KEYTYPE_KEYFRAME = 0,  /* default - 'proper' Keyframe */
  BEZT_KEYTYPE_EXTREME = 1,   /* 'extreme' keyframe */
  BEZT_KEYTYPE_BREAKDOWN = 2, /* 'breakdown' keyframe */
  BEZT_KEYTYPE_JITTER = 3,    /* 'jitter' keyframe (for adding 'filler' secondary motion) */
  BEZT_KEYTYPE_MOVEHOLD = 4,  /* one end of a 'moving hold' */
} eBezTriple_KeyframeType;

/* checks if the given BezTriple is selected */
#define BEZT_ISSEL_ANY(bezt) \
  (((bezt)->f2 & SELECT) || ((bezt)->f1 & SELECT) || ((bezt)->f3 & SELECT))
#define BEZT_ISSEL_ALL(bezt) \
  (((bezt)->f2 & SELECT) && ((bezt)->f1 & SELECT) && ((bezt)->f3 & SELECT))
#define BEZT_ISSEL_ALL_HIDDENHANDLES(v3d, bezt) \
  ((((v3d) != NULL) && ((v3d)->overlay.handle_display == CURVE_HANDLE_NONE)) ? \
       (bezt)->f2 & SELECT : \
       BEZT_ISSEL_ALL(bezt))
#define BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt) \
  ((((v3d) != NULL) && ((v3d)->overlay.handle_display == CURVE_HANDLE_NONE)) ? \
       (bezt)->f2 & SELECT : \
       BEZT_ISSEL_ANY(bezt))

#define BEZT_ISSEL_IDX(bezt, i) \
  ((i == 0 && (bezt)->f1 & SELECT) || (i == 1 && (bezt)->f2 & SELECT) || \
   (i == 2 && (bezt)->f3 & SELECT))

#define BEZT_SEL_ALL(bezt) \
  { \
    (bezt)->f1 |= SELECT; \
    (bezt)->f2 |= SELECT; \
    (bezt)->f3 |= SELECT; \
  } \
  ((void)0)
#define BEZT_DESEL_ALL(bezt) \
  { \
    (bezt)->f1 &= ~SELECT; \
    (bezt)->f2 &= ~SELECT; \
    (bezt)->f3 &= ~SELECT; \
  } \
  ((void)0)
#define BEZT_SEL_INVERT(bezt) \
  { \
    (bezt)->f1 ^= SELECT; \
    (bezt)->f2 ^= SELECT; \
    (bezt)->f3 ^= SELECT; \
  } \
  ((void)0)

#define BEZT_SEL_IDX(bezt, i) \
  { \
    switch (i) { \
      case 0: \
        (bezt)->f1 |= SELECT; \
        break; \
      case 1: \
        (bezt)->f2 |= SELECT; \
        break; \
      case 2: \
        (bezt)->f3 |= SELECT; \
        break; \
      default: \
        break; \
    } \
  } \
  ((void)0)

#define BEZT_DESEL_IDX(bezt, i) \
  { \
    switch (i) { \
      case 0: \
        (bezt)->f1 &= ~SELECT; \
        break; \
      case 1: \
        (bezt)->f2 &= ~SELECT; \
        break; \
      case 2: \
        (bezt)->f3 &= ~SELECT; \
        break; \
      default: \
        break; \
    } \
  } \
  ((void)0)

#define BEZT_IS_AUTOH(bezt) \
  (ELEM((bezt)->h1, HD_AUTO, HD_AUTO_ANIM) && ELEM((bezt)->h2, HD_AUTO, HD_AUTO_ANIM))

/* *************** CHARINFO **************** */

/* CharInfo.flag */
enum {
  /* NOTE: CU_CHINFO_WRAP, CU_CHINFO_SMALLCAPS_TEST and CU_CHINFO_TRUNCATE are set dynamically. */
  CU_CHINFO_BOLD = 1 << 0,
  CU_CHINFO_ITALIC = 1 << 1,
  CU_CHINFO_UNDERLINE = 1 << 2,
  /** Word-wrap occurred here. */
  CU_CHINFO_WRAP = 1 << 3,
  CU_CHINFO_SMALLCAPS = 1 << 4,
  /** Set at runtime, checks if case switching is needed. */
  CU_CHINFO_SMALLCAPS_CHECK = 1 << 5,
  /** Set at runtime, indicates char that doesn't fit in text boxes. */
  CU_CHINFO_OVERFLOW = 1 << 6,
};

/* mixed with KEY_LINEAR but define here since only curve supports */
#define KEY_CU_EASE 3

/* indicates point has been seen during surface duplication */
#define SURF_SEEN 4

#ifdef __cplusplus
}
#endif
