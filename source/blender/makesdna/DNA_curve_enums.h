/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/** Used in `readfile.cc` and `editfont.cc`. */
#define MAXTEXTBOX 256

/* **************** CURVE ********************* */

/** #Curve.texspace_flag */
enum {
  CU_TEXSPACE_FLAG_AUTO = 1 << 0,
  CU_TEXSPACE_FLAG_AUTO_EVALUATED = 1 << 1,
};

/** #Curve.flag */
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

/** #Curve.twist_mode */
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

/** #Curve.spacemode */
enum {
  CU_ALIGN_X_LEFT = 0,
  CU_ALIGN_X_MIDDLE = 1,
  CU_ALIGN_X_RIGHT = 2,
  CU_ALIGN_X_JUSTIFY = 3,
  CU_ALIGN_X_FLUSH = 4,
};

/** #Curve.align_y */
enum {
  CU_ALIGN_Y_TOP_BASELINE = 0,
  CU_ALIGN_Y_TOP = 1,
  CU_ALIGN_Y_CENTER = 2,
  CU_ALIGN_Y_BOTTOM_BASELINE = 3,
  CU_ALIGN_Y_BOTTOM = 4,
};

/** #Curve.bevel_mode */
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

/** #Nurb.flag */
enum {
  CU_SMOOTH = 1 << 0,
};

/** #Nurb.type */
enum {
  CU_POLY = 0,
  CU_BEZIER = 1,
  CU_NURBS = 4,
  CU_TYPE = (CU_POLY | CU_BEZIER | CU_NURBS),

  /* only for adding */
  CU_PRIMITIVE = 0xF00,

  /* 2 or 4 points */
  CU_PRIM_CURVE = 0x100,
  /* 8 points circle */
  CU_PRIM_CIRCLE = 0x200,
  /* 4x4 patch NURB. */
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
  CU_NURB_CUSTOM = 1 << 3,
};

#define CU_ACT_NONE -1

/* *************** BEZTRIPLE **************** */

/** #BezTriple.f1, #BezTriple.f2, #BezTriple.f3. */
typedef enum eBezTriple_Flag {
  /* `SELECT = (1 << 0)` */
  BEZT_FLAG_TEMP_TAG = (1 << 1), /* always clear. */
  /* Can be used to ignore keyframe points for certain operations. */
  BEZT_FLAG_IGNORE_TAG = (1 << 2),
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
  /**
   * Key set by some automatic helper tool, marking that this key can be erased
   * and the tool re-run.
   */
  BEZT_KEYTYPE_GENERATED = 5,
} eBezTriple_KeyframeType;

/* *************** CHARINFO **************** */

/** #CharInfo.flag */
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

/** User adjustable as styles (not relating to run-time layout calculation). */
#define CU_CHINFO_STYLE_ALL \
  (CU_CHINFO_BOLD | CU_CHINFO_ITALIC | CU_CHINFO_UNDERLINE | CU_CHINFO_SMALLCAPS)

/* mixed with KEY_LINEAR but define here since only curve supports */
#define KEY_CU_EASE 3

/* indicates point has been seen during surface duplication */
#define SURF_SEEN (1 << 2)
