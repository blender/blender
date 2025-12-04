/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

struct MaskLayerShapeElem;

typedef struct Mask_Runtime {
  /* The Depsgraph::update_count when this ID was last updated. Covers any IDRecalcFlag. */
  uint64_t last_update;
} Mask_Runtime;

typedef struct Mask {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_MSK;
#endif

  ID id;
  struct AnimData *adt;
  /** Mask layers (#MaskLayer). */
  ListBase masklayers;
  /** Index of active mask layer (-1 == None). */
  int masklay_act;
  /** Total number of mask layers. */
  int masklay_tot;

  /** Frames, used by the sequencer. */
  int sfra, efra;

  /** For anim info, #MaskAnimFlag. */
  int flag;
  char _pad[4];

  Mask_Runtime runtime;
} Mask;

typedef struct MaskParent {
  /** Type of parenting. */
  int id_type;
  /** Type of parenting (#MaskParentType). */
  int type;
  /**
   * ID block of entity to which mask/spline is parented to.
   * In case of parenting to movie tracking data set to MovieClip datablock.
   */
  ID *id;
  /**
   * Entity of parent to which parenting happened.
   * In case of parenting to movie tracking data contains name of layer.
   */
  char parent[64];
  /**
   * Sub-entity of parent to which parenting happened.
   * In case of parenting to movie tracking data contains name of track.
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

  /** Spline flags (#MaskSplineFlag). */
  short flag;
  /** Feather offset method (#MaskSplineOffset). */
  char offset_mode;
  /** Weight interpolation (#MaskSplineInterp). */
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

  float *data; /* Internally a #MaskLayerShapeElem struct for each vertex. */
  int tot_vert;
  int frame;
  char flag; /* MaskLayerShapeFlag */
  char _pad[7];

#ifdef __cplusplus
  const MaskLayerShapeElem *vertices() const
  {
    return (const MaskLayerShapeElem *)this->data;
  }
  MaskLayerShapeElem *vertices()
  {
    return (MaskLayerShapeElem *)this->data;
  }
#endif
} MaskLayerShape;

typedef struct MaskLayer {
  struct MaskLayer *next, *prev;

  /** Name of the mask layer. */
  char name[/*MAX_NAME*/ 64];

  /** List of splines which defines this mask layer. */
  ListBase splines;
  ListBase splines_shapes;

  /** Active spline. */
  struct MaskSpline *act_spline;
  /**
   * Active point.
   *
   * \note By convention the active-point will be a point in `act_spline` however this isn't
   * guaranteed and cannot be assumed by logic that validates memory.
   */
  struct MaskSplinePoint *act_point;

  /* blending options */
  float alpha;
  char blend;      /* MaskLayerBlend */
  char blend_flag; /* MaskLayerBlendFlag */
  char falloff;
  char _pad[7];

  char flag; /* MaskLayerFlag */
  /** Matching 'Object' flag of the same name - eventually use in the outliner
   * (#MaskLayerVisibility). */
  char visibility_flag;
} MaskLayer;

typedef enum MaskParentType {
  MASK_PARENT_POINT_TRACK = 0, /* parenting happens to point track */
  MASK_PARENT_PLANE_TRACK = 1, /* parenting happens to plane track */
} MaskParentType;

typedef enum MaskSplineFlag {
  /* reserve (1 << 0) for SELECT */
  MASK_SPLINE_CYCLIC = (1 << 1),
  MASK_SPLINE_NOFILL = (1 << 2),
  MASK_SPLINE_NOINTERSECT = (1 << 3),
} MaskSplineFlag;

typedef enum MaskSplineInterp {
  MASK_SPLINE_INTERP_LINEAR = 1,
  MASK_SPLINE_INTERP_EASE = 2,
} MaskSplineInterp;

typedef enum MaskSplineOffset {
  MASK_SPLINE_OFFSET_EVEN = 0,
  MASK_SPLINE_OFFSET_SMOOTH = 1,
} MaskSplineOffset;

typedef enum MaskLayerVisibility {
  MASK_HIDE_VIEW = 1 << 0,   /* Note: match #OB_HIDE_VIEWPORT value. */
  MASK_HIDE_SELECT = 1 << 1, /* Note: match #OB_HIDE_SELECT value. */
  MASK_HIDE_RENDER = 1 << 2, /* Note: match #OB_HIDE_RENDER value. */
} MaskLayerVisibility;

/* #MaskSpaceInfo.draw_flag */
typedef enum MaskDrawFlag {
  MASK_DRAWFLAG_SMOOTH_DEPRECATED = 1 << 0, /* Deprecated. */
  MASK_DRAWFLAG_OVERLAY = 1 << 1,
  MASK_DRAWFLAG_SPLINE = 1 << 2,
} MaskDrawFlag;

/* #MaskSpaceInfo.draw_type. Note: match values of #eSpaceImage_UVDT. */
typedef enum MaskDrawType {
  MASK_DT_OUTLINE = 0,
  MASK_DT_DASH = 1,
  MASK_DT_BLACK = 2,
  MASK_DT_WHITE = 3,
} MaskDrawType;

/* #MaskSpaceInfo.overlay_mode */
typedef enum MaskOverlayMode {
  MASK_OVERLAY_ALPHACHANNEL = 0,
  MASK_OVERLAY_COMBINED = 1,
} MaskOverlayMode;

typedef enum MaskLayerBlend {
  MASK_BLEND_ADD = 0,
  MASK_BLEND_SUBTRACT = 1,
  MASK_BLEND_LIGHTEN = 2,
  MASK_BLEND_DARKEN = 3,
  MASK_BLEND_MUL = 4,
  MASK_BLEND_REPLACE = 5,
  MASK_BLEND_DIFFERENCE = 6,
  MASK_BLEND_MERGE_ADD = 7,
  MASK_BLEND_MERGE_SUBTRACT = 8,
} MaskLayerBlend;

typedef enum MaskLayerBlendFlag {
  MASK_BLENDFLAG_INVERT = (1 << 0),
} MaskLayerBlendFlag;

typedef enum MaskLayerFlag {
  MASK_LAYERFLAG_LOCKED = (1 << 4),
  MASK_LAYERFLAG_SELECT = (1 << 5),

  /* no holes */
  MASK_LAYERFLAG_FILL_DISCRETE = (1 << 6),
  MASK_LAYERFLAG_FILL_OVERLAP = (1 << 7),
} MaskLayerFlag;

/* masklay_shape->flag */
typedef enum MaskLayerShapeFlag {
  MASK_SHAPE_SELECT = (1 << 0),
} MaskLayerShapeFlag;

typedef enum MaskAnimFlag {
  MASK_ANIMF_EXPAND = (1 << 4),
} MaskAnimFlag;
