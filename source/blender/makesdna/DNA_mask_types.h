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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 *
 * Mask data-blocks are collections of 2D curves to be used
 * for image masking in the compositor and sequencer.
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_curve_types.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mask {
  ID id;
  struct AnimData *adt;
  /** Mask layers. */
  ListBase masklayers;
  /** Index of active mask layer (-1 == None). */
  int masklay_act;
  /** Total number of mask layers. */
  int masklay_tot;

  /** Frames, used by the sequencer. */
  int sfra, efra;

  /** For anim info. */
  int flag;
  char _pad[4];
} Mask;

typedef struct MaskParent {
  //* /* Parenting flags */ /* not used. */
  // int flag;
  /** Type of parenting. */
  int id_type;
  /** Type of parenting. */
  int type;
  /**
   * ID block of entity to which mask/spline is parented to
   * in case of parenting to movie tracking data set to MovieClip datablock.
   */
  ID *id;
  /**
   * Entity of parent to which parenting happened
   * in case of parenting to movie tracking data contains name of layer.
   */
  char parent[64];
  /**
   * Sub-entity of parent to which parenting happened
   * in case of parenting to movie tracking data contains name of track.
   */
  char sub_parent[64];
  /**
   * Track location at the moment of parenting,
   * stored in mask space.
   */
  float parent_orig[2];

  /** Original corners of plane track at the moment of parenting. */
  float parent_corners_orig[4][2];
} MaskParent;

typedef struct MaskSplinePointUW {
  /** U coordinate along spline segment and weight of this point. */
  float u, w;
  /** Different flags of this point. */
  int flag;
} MaskSplinePointUW;

typedef struct MaskSplinePoint {
  /** Actual point coordinates and its handles. */
  BezTriple bezt;
  char _pad[4];
  /** Number of uv feather values. */
  int tot_uw;
  /** Feather UV values. */
  MaskSplinePointUW *uw;
  /** Parenting information of particular spline point. */
  MaskParent parent;
} MaskSplinePoint;

typedef struct MaskSpline {
  struct MaskSpline *next, *prev;

  /** Different spline flag (closed, ...). */
  short flag;
  /** Feather offset method. */
  char offset_mode;
  /** Weight interpolation. */
  char weight_interp;

  /** Total number of points. */
  int tot_point;
  /** Points which defines spline itself. */
  MaskSplinePoint *points;
  /** Parenting information of the whole spline. */
  MaskParent parent;

  /** Deformed copy of 'points' BezTriple data - not saved. */
  MaskSplinePoint *points_deform;
} MaskSpline;

/* one per frame */
typedef struct MaskLayerShape {
  struct MaskLayerShape *next, *prev;

  /** U coordinate along spline segment and weight of this point. */
  float *data;
  /** To ensure no buffer overrun's: alloc size is `(tot_vert * MASK_OBJECT_SHAPE_ELEM_SIZE)`. */
  int tot_vert;
  /** Different flags of this point. */
  int frame;
  /** Animation flag. */
  char flag;
  char _pad[7];
} MaskLayerShape;

/* cast to this for convenience, not saved */
#define MASK_OBJECT_SHAPE_ELEM_SIZE 8 /* 3x 2D points + weight + radius == 8 */

#
#
typedef struct MaskLayerShapeElem {
  float value[MASK_OBJECT_SHAPE_ELEM_SIZE];
} MaskLayerShapeElem;

typedef struct MaskLayer {
  struct MaskLayer *next, *prev;

  /** Name of the mask layer (64 = MAD_ID_NAME - 2). */
  char name[64];

  /** List of splines which defines this mask layer. */
  ListBase splines;
  ListBase splines_shapes;

  /** Active spline. */
  struct MaskSpline *act_spline;
  /** Active point. */
  struct MaskSplinePoint *act_point;

  /* blending options */
  float alpha;
  char blend;
  char blend_flag;
  char falloff;
  char _pad[7];

  /** For animation. */
  char flag;
  /** Matching 'Object' flag of the same name - eventually use in the outliner. */
  char visibility_flag;
} MaskLayer;

/* MaskParent->flag */
/* #define MASK_PARENT_ACTIVE  (1 << 0) */ /* UNUSED */

/* MaskParent->type */
enum {
  MASK_PARENT_POINT_TRACK = 0, /* parenting happens to point track */
  MASK_PARENT_PLANE_TRACK = 1, /* parenting happens to plane track */
};

/* MaskSpline->flag */
/* reserve (1 << 0) for SELECT */
enum {
  MASK_SPLINE_CYCLIC = (1 << 1),
  MASK_SPLINE_NOFILL = (1 << 2),
  MASK_SPLINE_NOINTERSECT = (1 << 3),
};

/* MaskSpline->weight_interp */
enum {
  MASK_SPLINE_INTERP_LINEAR = 1,
  MASK_SPLINE_INTERP_EASE = 2,
};

/* MaskSpline->offset_mode */
enum {
  MASK_SPLINE_OFFSET_EVEN = 0,
  MASK_SPLINE_OFFSET_SMOOTH = 1,
};

/* MaskLayer->visibility_flag */
#define MASK_HIDE_VIEW (1 << 0)
#define MASK_HIDE_SELECT (1 << 1)
#define MASK_HIDE_RENDER (1 << 2)

/* SpaceClip->mask_draw_flag */
#define MASK_DRAWFLAG_SMOOTH (1 << 0)
#define MASK_DRAWFLAG_OVERLAY (1 << 1)

/* copy of eSpaceImage_UVDT */
/* SpaceClip->mask_draw_type */
enum {
  MASK_DT_OUTLINE = 0,
  MASK_DT_DASH = 1,
  MASK_DT_BLACK = 2,
  MASK_DT_WHITE = 3,
};

/* MaskSpaceInfo->overlay_mode */
typedef enum eMaskOverlayMode {
  MASK_OVERLAY_ALPHACHANNEL = 0,
  MASK_OVERLAY_COMBINED = 1,
} eMaskOverlayMode;

/* masklay->blend */
enum {
  MASK_BLEND_ADD = 0,
  MASK_BLEND_SUBTRACT = 1,
  MASK_BLEND_LIGHTEN = 2,
  MASK_BLEND_DARKEN = 3,
  MASK_BLEND_MUL = 4,
  MASK_BLEND_REPLACE = 5,
  MASK_BLEND_DIFFERENCE = 6,
  MASK_BLEND_MERGE_ADD = 7,
  MASK_BLEND_MERGE_SUBTRACT = 8,
};

/* masklay->blend_flag */
enum {
  MASK_BLENDFLAG_INVERT = (1 << 0),
};

/* masklay->flag */
enum {
  MASK_LAYERFLAG_LOCKED = (1 << 4),
  MASK_LAYERFLAG_SELECT = (1 << 5),

  /* no holes */
  MASK_LAYERFLAG_FILL_DISCRETE = (1 << 6),
  MASK_LAYERFLAG_FILL_OVERLAP = (1 << 7),
};

/* masklay_shape->flag */
enum {
  MASK_SHAPE_SELECT = (1 << 0),
};

/* mask->flag */
enum {
  MASK_ANIMF_EXPAND = (1 << 4),
};

#ifdef __cplusplus
}
#endif
