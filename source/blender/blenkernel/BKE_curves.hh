/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.h"

/** \file
 * \ingroup bke
 * \brief Low-level operations for curves.
 */

#include <mutex>

#include "BLI_float4x4.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_vec_types.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "BKE_attribute_access.hh"

#include "FN_generic_virtual_array.hh"

namespace blender::bke {

/**
 * Contains derived data, caches, and other information not saved in files, besides a few pointers
 * to arrays that are kept in the non-runtime struct to avoid dereferencing this whenever they are
 * accessed.
 */
class CurvesGeometryRuntime {
 public:
  /** Cache of evaluated positions. */
  mutable Vector<float3> evaluated_position_cache;
  mutable std::mutex position_cache_mutex;
  mutable bool position_cache_dirty = true;

  /** Direction of the spline at each evaluated point. */
  mutable Vector<float3> evaluated_tangents_cache;
  mutable std::mutex tangent_cache_mutex;
  mutable bool tangent_cache_dirty = true;

  /** Normal direction vectors for each evaluated point. */
  mutable Vector<float3> evaluated_normals_cache;
  mutable std::mutex normal_cache_mutex;
  mutable bool normal_cache_dirty = true;
};

/**
 * A C++ class that wraps the DNA struct for better encapsulation and ease of use. It inherits
 * directly from the struct rather than storing a pointer to avoid more complicated ownership
 * handling.
 */
class CurvesGeometry : public ::CurvesGeometry {
 public:
  CurvesGeometry();
  /**
   * Create curves with the given size. Only the position attribute is created, along with the
   * offsets.
   */
  CurvesGeometry(int point_size, int curve_size);
  CurvesGeometry(const CurvesGeometry &other);
  CurvesGeometry(CurvesGeometry &&other);
  CurvesGeometry &operator=(const CurvesGeometry &other);
  CurvesGeometry &operator=(CurvesGeometry &&other);
  ~CurvesGeometry();

  static CurvesGeometry &wrap(::CurvesGeometry &dna_struct)
  {
    CurvesGeometry *geometry = reinterpret_cast<CurvesGeometry *>(&dna_struct);
    return *geometry;
  }
  static const CurvesGeometry &wrap(const ::CurvesGeometry &dna_struct)
  {
    const CurvesGeometry *geometry = reinterpret_cast<const CurvesGeometry *>(&dna_struct);
    return *geometry;
  }

  /* --------------------------------------------------------------------
   * Accessors.
   */

  int points_size() const;
  int curves_size() const;
  IndexRange points_range() const;
  IndexRange curves_range() const;

  /**
   * The total number of points in the evaluated poly curve.
   * This can depend on the resolution attribute if it exists.
   */
  int evaluated_points_size() const;

  /**
   * Access a range of indices of point data for a specific curve.
   */
  IndexRange range_for_curve(int index) const;
  IndexRange range_for_curves(IndexRange curves) const;

  /** The type (#CurveType) of each curve, or potentially a single if all are the same type. */
  VArray<int8_t> curve_types() const;
  /** Mutable access to curve types. Call #tag_topology_changed after changing any type. */
  MutableSpan<int8_t> curve_types();

  MutableSpan<float3> positions();
  Span<float3> positions() const;

  /**
   * Calculate the largest and smallest position values, only including control points
   * (rather than evaluated points). The existing values of `min` and `max` are taken into account.
   *
   * \return Whether there are any points. If the curve is empty, the inputs will be unaffected.
   */
  bool bounds_min_max(float3 &min, float3 &max) const;

  /**
   * The index of the first point in every curve. The size of this span is one larger than the
   * number of curves. Consider using #range_for_curve rather than using the offsets directly.
   */
  Span<int> offsets() const;
  MutableSpan<int> offsets();

  VArray<bool> cyclic() const;
  MutableSpan<bool> cyclic();

  /* --------------------------------------------------------------------
   * Operations.
   */

  /**
   * Change the number of elements. New values for existing attributes should be properly
   * initialized afterwards.
   */
  void resize(int point_size, int curve_size);

  /** Call after deforming the position attribute. */
  void tag_positions_changed();
  /**
   * Call after any operation that changes the topology
   * (number of points, evaluated points, or the total count).
   */
  void tag_topology_changed();
  /** Call after changing the "tilt" or "up" attributes. */
  void tag_normals_changed();

  void translate(const float3 &translation);
  void transform(const float4x4 &matrix);

  void update_customdata_pointers();

  void remove_curves(IndexMask curves_to_delete);

  /* --------------------------------------------------------------------
   * Attributes.
   */

  fn::GVArray adapt_domain(const fn::GVArray &varray,
                           AttributeDomain from,
                           AttributeDomain to) const;
};

Curves *curves_new_nomain(int point_size, int curves_size);

/**
 * Create a new curves data-block containing a single curve with the given length and type.
 */
Curves *curves_new_nomain_single(int point_size, CurveType type);

}  // namespace blender::bke
