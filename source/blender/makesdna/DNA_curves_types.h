/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_attribute_types.h"
#include "DNA_customdata_types.h"
#include "DNA_listBase.h"

#include "BLI_enum_flags.hh"

#ifdef __cplusplus
namespace blender::bke {
class CurvesGeometry;
class CurvesGeometryRuntime;
}  // namespace blender::bke
using CurvesGeometryRuntimeHandle = blender::bke::CurvesGeometryRuntime;
#else
typedef struct CurvesGeometryRuntimeHandle CurvesGeometryRuntimeHandle;
#endif

typedef enum CurveType {
  /**
   * Catmull Rom curves provide automatic smoothness, like Bezier curves with automatic handle
   * positions. This is the default type for the hair system because of the simplicity of
   * interaction and data storage.
   */
  CURVE_TYPE_CATMULL_ROM = 0,
  /**
   * Poly curves (often called "polylines") have no interpolation at all. They evaluate to the same
   * set of points as the original control points. They are a good choice for high-resolution
   * data-sets or when constrained by performance.
   */
  CURVE_TYPE_POLY = 1,
  /**
   * Bezier curves provide a common intuitive control system made up of handles and control points.
   * Handles are stored separately from positions, and do not store extra generic attribute values.
   * Bezier curves also give the flexibility to set handle types (see #HandleType) that influence
   * the number of evaluated points in each segment.
   */
  CURVE_TYPE_BEZIER = 2,
  /**
   * NURBS curves offer the most flexibility at the cost of increased complexity. Given the choice
   * of different knot modes (see #KnotsMode) and different orders (see "nurbs_order" attribute),
   * any of the other types can theoretically be created with a NURBS curve.
   *
   * Note that Blender currently does not support custom knot vectors, though that should be
   * supported in the long term.
   */
  CURVE_TYPE_NURBS = 3,
} CurveType;
/* The number of supported curve types. */
#define CURVE_TYPES_NUM 4

typedef enum HandleType {
  /** The handle can be moved anywhere, and doesn't influence the point's other handle. */
  BEZIER_HANDLE_FREE = 0,
  /** The location is automatically calculated to be smooth. */
  BEZIER_HANDLE_AUTO = 1,
  /** The location is calculated to point to the next/previous control point. */
  BEZIER_HANDLE_VECTOR = 2,
  /** The location is constrained to point in the opposite direction as the other handle. */
  BEZIER_HANDLE_ALIGN = 3,
} HandleType;
#define BEZIER_HANDLES_NUM 4

/** Method used to calculate a NURBS curve's knot vector. */
typedef enum KnotsMode {
  NURBS_KNOT_MODE_NORMAL = 0,
  NURBS_KNOT_MODE_ENDPOINT = 1,
  NURBS_KNOT_MODE_BEZIER = 2,
  NURBS_KNOT_MODE_ENDPOINT_BEZIER = 3,
  NURBS_KNOT_MODE_CUSTOM = 4,
} KnotsMode;

/** Method used to calculate the normals of a curve's evaluated points. */
typedef enum NormalMode {
  /** Calculate normals with the smallest twist around the curve tangent across the whole curve. */
  NORMAL_MODE_MINIMUM_TWIST = 0,
  /**
   * Calculate normals perpendicular to the Z axis and the curve tangent. If a series of points
   * is vertical, the X axis is used.
   */
  NORMAL_MODE_Z_UP = 1,
  /** Interpolate the stored "custom_normal" attribute for the final normals. */
  NORMAL_MODE_FREE = 2,
} NormalMode;

/**
 * A reusable data structure for geometry consisting of many curves. All control point data is
 * stored contiguously for better efficiency when there are many curves. Multiple curve types are
 * supported, as described in #CurveType. Data for each curve is accessed by slicing the main
 * point attribute data arrays.
 *
 * The data structure is meant to separate geometry data storage and processing from Blender
 * focused ID data-block handling. The struct can also be embedded to allow reusing it.
 */
typedef struct CurvesGeometry {
  /**
   * The start index of each curve in the point data. The size of each curve can be calculated by
   * subtracting the offset from the next offset. That is valid even for the last curve because
   * this array is allocated with a length one larger than the number of curves. This is allowed
   * to be null when there are no curves.
   *
   * Every curve offset must be at least one larger than the previous. In other words, every curve
   * must have at least one point. The first value is 0 and the last value is #point_num.
   *
   * This array is shared based on the bke::CurvesGeometryRuntime::curve_offsets_sharing_info.
   * Avoid accessing directly when possible.
   *
   * \note This is *not* stored as an attribute because its size is one larger than #curve_num.
   */
  int *curve_offsets;

  /** Curve and point domain attributes. */
  struct AttributeStorage attribute_storage;

  /**
   * Generic attributes are stored in #attribute_storage. This is still used for vertex groups.
   */
  CustomData point_data;

  /** Used only for backward compatibility with old files. */
  CustomData curve_data_legacy;

  /**
   * The total number of control points in all curves.
   */
  int point_num;
  /**
   * The number of curves.
   */
  int curve_num;

  /**
   * List of vertex group (#bDeformGroup) names and flags only.
   */
  ListBase vertex_group_names;
  /** The active index in the #vertex_group_names list. */
  int vertex_group_active_index;

  /** Set to -1 when none is active. */
  int attributes_active_index;

  /**
   * Runtime data for curves, stored as a pointer to allow defining this as a C++ class.
   */
  CurvesGeometryRuntimeHandle *runtime;

  /**
   * Knot values for NURBS curves with NURBS_KNOT_MODE_CUSTOM mode.
   * Array is allocated with bke::CurvesGeometry::nurbs_custom_knots_update_size() or
   * bke::CurvesGeometry::nurbs_custom_knots_resize().
   * Indexed with bke::CurvesGeometry::nurbs_custom_knots_by_curve().
   */
  float *custom_knots;

  int custom_knot_num;

  char _pad[4];

#ifdef __cplusplus
  blender::bke::CurvesGeometry &wrap();
  const blender::bke::CurvesGeometry &wrap() const;
#endif
} CurvesGeometry;

/**
 * A data-block corresponding to a number of curves of various types with various attributes.
 * Geometry data (as opposed to pointers to other data-blocks and higher level data for user
 * interaction) is embedded in the #CurvesGeometry struct.
 */
typedef struct Curves {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_CV;
#endif

  ID id;
  /** Animation data (must be immediately after #id). */
  struct AnimData *adt;

  /** Geometry data. */
  CurvesGeometry geometry;

  int flag;
  int attributes_active_index_legacy;

  /* Materials. */
  struct Material **mat;
  short totcol;

  /**
   * User-defined symmetry flag (#eCurvesSymmetryType) that causes editing operations to maintain
   * symmetrical geometry.
   */
  char symmetry;
  /**
   * #AttrDomain. The active domain for edit/sculpt mode selection. Only one selection mode can
   * be active at a time.
   */
  char selection_domain;
  char _pad[4];

  /**
   * Used as base mesh when curves represent e.g. hair or fur. This surface is used in edit modes.
   * When set, the curves will have attributes that indicate a position on this surface. This is
   * used for deforming the curves when the surface is deformed dynamically.
   *
   * This is expected to be a mesh object.
   */
  struct Object *surface;

  /**
   * The name of the attribute on the surface #Mesh used to give meaning to the UV attachment
   * coordinates stored for each curve. Expected to be a 2D vector attribute on the face corner
   * domain.
   */
  char *surface_uv_map;

  /* Distance to keep the curves away from the surface. */
  float surface_collision_distance;
  char _pad2[4];

  /* Draw cache to store data used for viewport drawing. */
  void *batch_cache;
} Curves;

/** #Curves.flag */
enum {
  HA_DS_EXPAND = (1 << 0),
  CV_SCULPT_COLLISION_ENABLED = (1 << 1),
};

/** #Curves.symmetry */
typedef enum eCurvesSymmetryType {
  CURVES_SYMMETRY_X = 1 << 0,
  CURVES_SYMMETRY_Y = 1 << 1,
  CURVES_SYMMETRY_Z = 1 << 2,
} eCurvesSymmetryType;
ENUM_OPERATORS(eCurvesSymmetryType)

/* Only one material supported currently. */
#define CURVES_MATERIAL_NR 1
