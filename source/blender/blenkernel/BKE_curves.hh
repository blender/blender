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

template<typename T, BLI_ENABLE_IF(std::is_integral_v<T>)>
constexpr IndexRange offsets_to_range(Span<T> offsets, int64_t index)
{
  BLI_assert(index >= 0);
  BLI_assert(index < offsets.size());

  const int offset = offsets[index];
  const int offset_next = offsets[index + 1];
  return {offset, offset_next - offset};
}

namespace curves::nurbs {

struct BasisCache {
  /**
   * For each evaluated point, the weight for all control points that influences it.
   * The vector's size is the evaluated point count multiplied by the spline's order.
   */
  Vector<float> weights;
  /**
   * For each evaluated point, an offset into the curve's control points for the start of #weights.
   * In other words, the index of the first control point that influences this evaluated point.
   */
  Vector<int> start_indices;
};

}  // namespace curves::nurbs

/**
 * Contains derived data, caches, and other information not saved in files, besides a few pointers
 * to arrays that are kept in the non-runtime struct to avoid dereferencing this whenever they are
 * accessed.
 */
class CurvesGeometryRuntime {
 public:
  /**
   * Cache of offsets into the evaluated array for each curve, accounting for all previous
   * evaluated points, Bezier curve vector segments, different resolutions per spline, etc.
   */
  mutable Vector<int> evaluated_offsets_cache;
  mutable Vector<int> bezier_evaluated_offsets;
  mutable std::mutex offsets_cache_mutex;
  mutable bool offsets_cache_dirty = true;

  mutable Vector<curves::nurbs::BasisCache> nurbs_basis_cache;
  mutable std::mutex nurbs_basis_cache_mutex;
  mutable bool nurbs_basis_cache_dirty = true;

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
   * The index of the first point in every curve. The size of this span is one larger than the
   * number of curves. Consider using #range_for_curve rather than using the offsets directly.
   */
  Span<int> offsets() const;
  MutableSpan<int> offsets();

  /**
   * Access a range of indices of point data for a specific curve.
   */
  IndexRange range_for_curve(int index) const;
  IndexRange range_for_curves(IndexRange curves) const;

  /** The type (#CurveType) of each curve, or potentially a single if all are the same type. */
  VArray<int8_t> curve_types() const;
  /** Mutable access to curve types. Call #tag_topology_changed after changing any type. */
  MutableSpan<int8_t> curve_types();

  bool has_curve_with_type(const CurveType type) const;

  MutableSpan<float3> positions();
  Span<float3> positions() const;

  /** Whether the curve loops around to connect to itself, on the curve domain. */
  VArray<bool> cyclic() const;
  /** Mutable access to curve cyclic values. Call #tag_topology_changed after changes. */
  MutableSpan<bool> cyclic();

  /**
   * How many evaluated points to create for each segment when evaluating Bezier,
   * Catmull Rom, and NURBS curves. On the curve domain.
   */
  VArray<int> resolution() const;
  /** Mutable access to curve resolution. Call #tag_topology_changed after changes. */
  MutableSpan<int> resolution();

  /**
   * Handle types for Bezier control points. Call #tag_topology_changed after changes.
   */
  VArray<int8_t> handle_types_left() const;
  MutableSpan<int8_t> handle_types_left();
  VArray<int8_t> handle_types_right() const;
  MutableSpan<int8_t> handle_types_right();

  /**
   * The positions of Bezier curve handles. Though these are really control points for the Bezier
   * segments, they are stored in separate arrays to better reflect user expectations. Note that
   * values may be generated automatically based on the handle types. Call #tag_positions_changed
   * after changes.
   */
  Span<float3> handle_positions_left() const;
  MutableSpan<float3> handle_positions_left();
  Span<float3> handle_positions_right() const;
  MutableSpan<float3> handle_positions_right();

  /**
   * The order (degree plus one) of each NURBS curve, on the curve domain.
   * Call #tag_topology_changed after changes.
   */
  VArray<int8_t> nurbs_orders() const;
  MutableSpan<int8_t> nurbs_orders();

  /**
   * The automatic generation mode for each NURBS curve's knots vector, on the curve domain.
   * Call #tag_topology_changed after changes.
   */
  VArray<int8_t> nurbs_knots_modes() const;
  MutableSpan<int8_t> nurbs_knots_modes();

  /**
   * The weight for each control point for NURBS curves. Call #tag_positions_changed after changes.
   */
  Span<float> nurbs_weights() const;
  MutableSpan<float> nurbs_weights();

  /**
   * Calculate the largest and smallest position values, only including control points
   * (rather than evaluated points). The existing values of `min` and `max` are taken into account.
   *
   * \return Whether there are any points. If the curve is empty, the inputs will be unaffected.
   */
  bool bounds_min_max(float3 &min, float3 &max) const;

 private:
  /**
   * All of the curve indices for curves with a specific type.
   */
  IndexMask indices_for_curve_type(CurveType type, Vector<int64_t> &r_indices) const;

  /* --------------------------------------------------------------------
   * Evaluation.
   */

 public:
  /**
   * The total number of points in the evaluated poly curve.
   * This can depend on the resolution attribute if it exists.
   */
  int evaluated_points_size() const;

  /**
   * Access a range of indices of point data for a specific curve.
   * Call #evaluated_offsets() first to ensure that the evaluated offsets cache is current.
   */
  IndexRange evaluated_range_for_curve(int index) const;

  /**
   * The index of the first evaluated point for every curve. The size of this span is one larger
   * than the number of curves. Consider using #evaluated_range_for_curve rather than using the
   * offsets directly.
   */
  Span<int> evaluated_offsets() const;

  Span<float3> evaluated_positions() const;

 private:
  /**
   * Make sure the basis weights for NURBS curve's evaluated points are calculated.
   */
  void ensure_nurbs_basis_cache() const;

  /* --------------------------------------------------------------------
   * Operations.
   */

 public:
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

namespace curves {

/**
 * The number of segments between control points, accounting for the last segment of cyclic curves,
 * and the fact that curves with two points cannot be cyclic. The logic is simple, but this
 * function should be used to make intentions clearer.
 */
inline int curve_segment_size(const int size, const bool cyclic)
{
  return (cyclic && size > 2) ? size : size - 1;
}

namespace bezier {

/**
 * Return true if the handles that make up a segment both have a vector type. Vector segments for
 * Bezier curves have special behavior because they aren't divided into many evaluated points.
 */
bool segment_is_vector(Span<int8_t> handle_types_left,
                       Span<int8_t> handle_types_right,
                       int segment_index);

/**
 * Return true if the curve's last cylic segment has a vector type.
 * This only makes a difference in the shape of cyclic curves.
 */
bool last_cylic_segment_is_vector(Span<int8_t> handle_types_left, Span<int8_t> handle_types_right);

/**
 * Calculate offsets into the curve's evaluated points for each control point. While most control
 * point edges generate the number of edges specified by the resolution, vector segments only
 * generate one edge.
 *
 * The size of the offsets array must be the same as the number of points. The value at each index
 * is the evaluated point offset including the following segment.
 */
void calculate_evaluated_offsets(Span<int8_t> handle_types_left,
                                 Span<int8_t> handle_types_right,
                                 bool cyclic,
                                 int resolution,
                                 MutableSpan<int> evaluated_offsets);

/**
 * Evaluate a cubic Bezier segment, using the "forward differencing" method.
 * A generic Bezier curve is made up by four points, but in many cases the first and last points
 * are referred to as the control points, and the middle points are the corresponding handles.
 */
void evaluate_segment(const float3 &point_0,
                      const float3 &point_1,
                      const float3 &point_2,
                      const float3 &point_3,
                      MutableSpan<float3> result);

/**
 * Calculate all evaluated points for the Bezier curve.
 *
 * \param evaluated_offsets: The index in the evaluated points array for each control point,
 * including the points from the corresponding segment. Used to vary the number of evaluated
 * points per segment, i.e. to make vector segment only have one edge. This is expected to be
 * calculated by #calculate_evaluated_offsets, and is the reason why this function doesn't need
 * arguments like "cyclic" and "resolution".
 */
void calculate_evaluated_positions(Span<float3> positions,
                                   Span<float3> handles_left,
                                   Span<float3> handles_right,
                                   Span<int> evaluated_offsets,
                                   MutableSpan<float3> evaluated_positions);

}  // namespace bezier

namespace catmull_rom {

/**
 * Calculate the number of evaluated points that #interpolate_to_evaluated is expected to produce.
 * \param size: The number of points in the curve.
 * \param resolution: The resolution for each segment.
 */
int calculate_evaluated_size(int size, bool cyclic, int resolution);

/**
 * Evaluate the Catmull Rom curve. The length of the #dst span should be calculated with
 * #calculate_evaluated_size and is expected to divide evenly by the #src span's segment size.
 */
void interpolate_to_evaluated(fn::GSpan src, bool cyclic, int resolution, fn::GMutableSpan dst);

}  // namespace catmull_rom

namespace nurbs {

/**
 * Checks the conditions that a NURBS curve needs to evaluate.
 */
bool check_valid_size_and_order(int size, int8_t order, bool cyclic, KnotsMode knots_mode);

/**
 * Calculate the standard evaluated size for a NURBS curve, using the standard that
 * the resolution is multiplied by the number of segments between the control points.
 *
 * \note Though the number of evaluated points is rather arbitrary, it's useful to have a standard
 * for predictability and so that cached basis weights of NURBS curves with these properties can be
 * shared.
 */
int calculate_evaluated_size(
    int size, int8_t order, bool cyclic, int resolution, KnotsMode knots_mode);

/**
 * Calculate the length of the knot vector for a NURBS curve with the given properties.
 * The knots must be longer for a cyclic curve, for example, in order to provide weights for the
 * last evaluated points that are also influenced by the first control points.
 */
int knots_size(int size, int8_t order, bool cyclic);

/**
 * Calculate the knots for a spline given its properties, based on built-in standards defined by
 * #KnotsMode.
 *
 * \note Theoretically any sorted values can be used for NURBS knots, but calculating based
 * on standard modes allows useful presets, automatic recalculation when the number of points
 * changes, and is generally more intuitive than defining the knot vector manually.
 */
void calculate_knots(
    int size, KnotsMode mode, int8_t order, bool cyclic, MutableSpan<float> knots);

/**
 * Based on the knots, the order, and other properties of a NURBS curve, calculate a cache that can
 * be used to more simply interpolate attributes to the evaluated points later. The cache includes
 * two pieces of information for every evaluated point: the first control point that influences it,
 * and a weight for each control point.
 */
void calculate_basis_cache(int size,
                           int evaluated_size,
                           int8_t order,
                           bool cyclic,
                           Span<float> knots,
                           BasisCache &basis_cache);

/**
 * Using a "basis cache" generated by #BasisCache, interpolate attribute values to the evaluated
 * points. The number of evaluated points is determined by the #basis_cache argument.
 *
 * \param control_weights: An optional span of control point weights, which must have the same size
 * as the number of control points in the curve if provided. Using this argument gives a NURBS
 * curve the "Rational" behavior that's part of its acronym; otherwise it is a NUBS.
 */
void interpolate_to_evaluated(const BasisCache &basis_cache,
                              int8_t order,
                              Span<float> control_weights,
                              fn::GSpan src,
                              fn::GMutableSpan dst);

}  // namespace nurbs

}  // namespace curves

Curves *curves_new_nomain(int point_size, int curves_size);

/**
 * Create a new curves data-block containing a single curve with the given length and type.
 */
Curves *curves_new_nomain_single(int point_size, CurveType type);

}  // namespace blender::bke
