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
#include "DNA_listBase.h"

#include "BLI_enum_flags.hh"

namespace blender {

enum MaskParentType : int {
  MASK_PARENT_POINT_TRACK = 0, /* parenting happens to point track */
  MASK_PARENT_PLANE_TRACK = 1, /* parenting happens to plane track */
};

enum MaskSplineFlag : short {
  MASK_SPLINE_SELECT = (1 << 0),
  MASK_SPLINE_CYCLIC = (1 << 1),
  MASK_SPLINE_NOFILL = (1 << 2),
  MASK_SPLINE_NOINTERSECT = (1 << 3),
};
ENUM_OPERATORS(MaskSplineFlag)

enum MaskSplineInterp : char {
  MASK_SPLINE_INTERP_LINEAR = 1,
  MASK_SPLINE_INTERP_EASE = 2,
};

enum MaskSplineOffset : char {
  MASK_SPLINE_OFFSET_EVEN = 0,
  MASK_SPLINE_OFFSET_SMOOTH = 1,
};

enum MaskLayerVisibility : char {
  MASK_HIDE_VIEW = 1 << 0,   /* Note: match #OB_HIDE_VIEWPORT value. */
  MASK_HIDE_SELECT = 1 << 1, /* Note: match #OB_HIDE_SELECT value. */
  MASK_HIDE_RENDER = 1 << 2, /* Note: match #OB_HIDE_RENDER value. */
};
ENUM_OPERATORS(MaskLayerVisibility)

/* #MaskSpaceInfo.draw_flag */
enum MaskDrawFlag : char {
  MASK_DRAWFLAG_SMOOTH_DEPRECATED = 1 << 0, /* Deprecated. */
  MASK_DRAWFLAG_OVERLAY = 1 << 1,
  MASK_DRAWFLAG_SPLINE = 1 << 2,
};
ENUM_OPERATORS(MaskDrawFlag)

/* #MaskSpaceInfo.draw_type. Note: match values of #eSpaceImage_UVDT. */
enum MaskDrawType : char {
  MASK_DT_OUTLINE = 0,
  MASK_DT_DASH = 1,
  MASK_DT_BLACK = 2,
  MASK_DT_WHITE = 3,
};

/* #MaskSpaceInfo.overlay_mode */
enum MaskOverlayMode : char {
  MASK_OVERLAY_ALPHACHANNEL = 0,
  MASK_OVERLAY_COMBINED = 1,
};

enum MaskLayerBlend : char {
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

enum MaskLayerBlendFlag : char {
  MASK_BLENDFLAG_INVERT = (1 << 0),
};
ENUM_OPERATORS(MaskLayerBlendFlag)

enum MaskLayerFlag : char {
  MASK_LAYERFLAG_LOCKED = (1 << 4),
  MASK_LAYERFLAG_SELECT = (1 << 5),

  /* no holes */
  MASK_LAYERFLAG_FILL_DISCRETE = (1 << 6),
  /** Only for #the MASK_FILL_SOLVER_SWEEP_LINE solver. */
  MASK_LAYERFLAG_FILL_OVERLAP = static_cast<char>(1 << 7),
};
ENUM_OPERATORS(MaskLayerFlag)

/* masklay_shape->flag */
enum MaskLayerShapeFlag : char {
  MASK_SHAPE_SELECT = (1 << 0),
};
ENUM_OPERATORS(MaskLayerShapeFlag)

enum MaskAnimFlag : int {
  MASK_ANIMF_EXPAND = (1 << 4),
};
ENUM_OPERATORS(MaskAnimFlag)

/** #Mask.fill_solver */
enum MaskLayerFillSolverType : char {
  /**
   * Fast filling without support for self-intersections.
   * Uses `BLI_scanfill`.
   */
  MASK_FILL_SOLVER_SWEEP_LINE = 0,
  /**
   * Constrained Delaunay Triangulation with self-intersection and fill rule support.
   */
  MASK_FILL_SOLVER_CDT = 1,
};

struct MaskLayerShapeElem;

struct Mask_Runtime {
  /* The Depsgraph::update_count when this ID was last updated. Covers any IDRecalcFlag. */
  uint64_t last_update = 0;
};

struct Mask {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_MSK;
#endif

  ID id;
  struct AnimData *adt = nullptr;
  /** Mask layers. */
  ListBaseT<struct MaskLayer> masklayers = {nullptr, nullptr};
  /** Index of active mask layer (-1 == None). */
  int masklay_act = 0;
  /** Total number of mask layers. */
  int masklay_tot = 0;

  /** Frames, used by the sequencer. */
  int sfra = 0, efra = 0;

  /** For anim info. */
  MaskAnimFlag flag = {};
  char _pad[4] = {};

  Mask_Runtime runtime;
};

struct MaskParent {
  /** Type of parenting. */
  int id_type = 0;
  /** Type of parenting. */
  MaskParentType type = MASK_PARENT_POINT_TRACK;
  /**
   * ID block of entity to which mask/spline is parented to.
   * In case of parenting to movie tracking data set to MovieClip datablock.
   */
  ID *id = nullptr;
  /**
   * Entity of parent to which parenting happened.
   * In case of parenting to movie tracking data contains name of layer.
   */
  char parent[64] = "";
  /**
   * Sub-entity of parent to which parenting happened.
   * In case of parenting to movie tracking data contains name of track.
   */
  char sub_parent[64] = "";
  /**
   * Track location at the moment of parenting,
   * stored in mask space.
   */
  float parent_orig[2] = {};

  /** Original corners of plane track at the moment of parenting. */
  float parent_corners_orig[4][2] = {};
};

struct MaskSplinePointUW {
  /** U coordinate along spline segment and weight of this point. */
  float u = 0, w = 0;
  /** Different flags of this point. */
  int flag = 0;
};

struct MaskSplinePoint {
  /** Actual point coordinates and its handles. */
  BezTriple bezt = {};
  char _pad[4] = {};
  /** Number of uv feather values. */
  int tot_uw = 0;
  /** Feather UV values. */
  MaskSplinePointUW *uw = nullptr;
  /** Parenting information of particular spline point. */
  MaskParent parent;
};

struct MaskSpline {
  struct MaskSpline *next = nullptr, *prev = nullptr;

  /** Spline flags. */
  MaskSplineFlag flag = {};
  /** Feather offset method. */
  MaskSplineOffset offset_mode = MASK_SPLINE_OFFSET_EVEN;
  /** Weight interpolation. */
  MaskSplineInterp weight_interp = {};

  /** Total number of points. */
  int tot_point = 0;
  /** Points which defines spline itself. */
  MaskSplinePoint *points = nullptr;
  /** Parenting information of the whole spline. */
  MaskParent parent;

  /** Deformed copy of 'points' BezTriple data - not saved. */
  MaskSplinePoint *points_deform = nullptr;
};

/* one per frame */
struct MaskLayerShape {
  struct MaskLayerShape *next = nullptr, *prev = nullptr;

  float *data = nullptr; /* Internally a #MaskLayerShapeElem struct for each vertex. */
  int tot_vert = 0;
  int frame = 0;
  MaskLayerShapeFlag flag = {};
  char _pad[7] = {};

#ifdef __cplusplus
  const MaskLayerShapeElem *vertices() const
  {
    return reinterpret_cast<const MaskLayerShapeElem *>(this->data);
  }
  MaskLayerShapeElem *vertices()
  {
    return reinterpret_cast<MaskLayerShapeElem *>(this->data);
  }
#endif
};

struct MaskLayer {
  struct MaskLayer *next = nullptr, *prev = nullptr;

  /** Name of the mask layer. */
  char name[/*MAX_NAME*/ 64] = "";

  /** List of splines which defines this mask layer. */
  ListBaseT<MaskSpline> splines = {nullptr, nullptr};
  ListBaseT<MaskLayerShape> splines_shapes = {nullptr, nullptr};

  /** Active spline. */
  struct MaskSpline *act_spline = nullptr;
  /**
   * Active point.
   *
   * \note By convention the active-point will be a point in `act_spline` however this isn't
   * guaranteed and cannot be assumed by logic that validates memory.
   */
  struct MaskSplinePoint *act_point = nullptr;

  /* blending options */
  float alpha = 0;
  MaskLayerBlend blend = MASK_BLEND_ADD;
  MaskLayerBlendFlag blend_flag = {};
  char falloff = 0;
  MaskLayerFillSolverType fill_solver = MASK_FILL_SOLVER_CDT;
  char _pad[6] = {};

  MaskLayerFlag flag = {};
  /** Matching 'Object' flag of the same name - eventually use in the outliner. */
  MaskLayerVisibility visibility_flag = {};
};

}  // namespace blender
