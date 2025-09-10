/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_curve_enums.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#ifdef __cplusplus
#  include <optional>
#endif

struct AnimData;
struct Curves;
struct CurveProfile;
struct EditFont;
struct GHash;
struct Key;
struct Material;
struct Object;
struct VFont;

/* These two Lines with # tell `makesdna` this struct can be excluded. */
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

/* These two Lines with # tell `makesdna` this struct can be excluded. */
#
#
typedef struct BevList {
  struct BevList *next, *prev;
  int nr, dupe_nr;
  /** Cyclic when set to any value besides -1. */
  int poly;
  int hole;
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

  /**
   * Hide is used to indicate whether BezTriple is hidden (3D).
   *
   * \warning For #FCurve this is used to store the key-type, see #BEZKEYTYPE.
   */
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
 * Provide access to Keyframe Type info #eBezTriple_KeyframeType in #BezTriple::hide.
 * \note this is so that we can change it to another location.
 */
#define BEZKEYTYPE(bezt) (eBezTriple_KeyframeType((bezt)->hide))
#define BEZKEYTYPE_LVALUE(bezt) ((bezt)->hide)

/**
 * \note #BPoint.tilt location in struct is abused by Key system.
 */
typedef struct BPoint {
  float vec[4];
  /** Tilt in 3D View. */
  float tilt;
  /** Used for softbody goal weight. */
  float weight;
  /** F1: selection status, hide: is point hidden or not. */
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
  DNA_DEFINE_CXX_METHODS(Nurb)

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
  float kern;
  short mat_nr;
  char flag;
  char _pad[1];
} CharInfo;

typedef struct TextBox {
  float x, y, w, h;
} TextBox;

/* These two Lines with # tell `makesdna` this struct can be excluded. */
#
#
typedef struct EditNurb {
  DNA_DEFINE_CXX_METHODS(EditNurb)

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
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Curve)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_CU_LEGACY;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /** Actual data, called splines in rna. */
  ListBase nurb;

  /** Edited data, not in file, use pointer so we can check for it. */
  EditNurb *editnurb;

  struct Object *bevobj, *taperobj, *textoncurve;
  struct Key *key;
  struct Material **mat;

  struct CurveProfile *bevel_profile;

  float texspace_location[3];
  float texspace_size[3];

  /**
   * Object type of curve data-block (#ObjectType).
   * This must be one of:
   * - #OB_CURVES_LEGACY.
   * - #OB_FONT.
   * - #OB_SURF.
   */
  short ob_type;

  char texspace_flag;
  char _pad0[7];
  short twist_mode;
  float twist_smooth, smallcaps_scale;

  int pathlen;
  short bevresol, totcol;
  int flag;
  float offset, extrude, bevel_radius;

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
  char _pad[3];

  /* font part */
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

  /** Current evaluation-time, for use by Objects parented to curves. */
  float ctime;
  float bevfac1, bevfac2;
  char bevfac1_mapping, bevfac2_mapping;

  char _pad2[1];

  /**
   * If non-zero, the #editfont and #editnurb pointers are not owned by this #Curve. That means
   * this curve is a container for the result of object geometry evaluation. This only works
   * because evaluated object data never outlives original data.
   */
  char edit_data_from_original;

  /**
   * A pointer to curve data from evaluation. Owned by the object's #geometry_set_eval, either as a
   * geometry instance or the data of the evaluated #CurveComponent. The curve may also contain
   * data in the #nurb list, but for evaluated curves this is the proper place to retrieve data,
   * since it also contains the result of geometry nodes evaluation, and isn't just a copy of the
   * original object data.
   */
  const struct Curves *curve_eval;

  void *batch_cache;

#ifdef __cplusplus
  /** Get the largest material index used by the curves or `nullopt` if there are none. */
  std::optional<int> material_index_max() const;
#endif
} Curve;

/* **************** CURVE ********************* */

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
