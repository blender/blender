/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.h"

/** \file
 * \ingroup bke
 * \brief Low-level operations for curves.
 */

#include <mutex>

#include "BLI_float4x4.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_vec_types.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "BKE_attribute_access.hh"

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
   * The vector's size is the evaluated point count multiplied by the curve's order.
   */
  Vector<float> weights;
  /**
   * For each evaluated point, an offset into the curve's control points for the start of #weights.
   * In other words, the index of the first control point that influences this evaluated point.
   */
  Vector<int> start_indices;

  /**
   * The result of #check_valid_num_and_order, to avoid retrieving its inputs later on.
   * If this is true, the data above will be invalid, and original data should be copied
   * to the evaluated result.
   */
  bool invalid = false;
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
   * The cached number of curves with each type. Unlike other caches here, this is not computed
   * lazily, since it is needed so often and types are not adjusted much anyway.
   */
  std::array<int, CURVE_TYPES_NUM> type_counts;

  /**
   * Cache of offsets into the evaluated array for each curve, accounting for all previous
   * evaluated points, Bezier curve vector segments, different resolutions per curve, etc.
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
  /**
   * The evaluated positions result, using a separate span in case all curves are poly curves,
   * in which case a separate array of evaluated positions is unnecessary.
   */
  mutable Span<float3> evaluated_positions_span;

  /**
   * Cache of lengths along each evaluated curve for for each evaluated point. If a curve is
   * cyclic, it needs one more length value to correspond to the last segment, so in order to
   * make slicing this array for a curve fast, an extra float is stored for every curve.
   */
  mutable Vector<float> evaluated_length_cache;
  mutable std::mutex length_cache_mutex;
  mutable bool length_cache_dirty = true;

  /** Direction of the curve at each evaluated point. */
  mutable Vector<float3> evaluated_tangent_cache;
  mutable std::mutex tangent_cache_mutex;
  mutable bool tangent_cache_dirty = true;

  /** Normal direction vectors for each evaluated point. */
  mutable Vector<float3> evaluated_normal_cache;
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
  CurvesGeometry(int point_num, int curve_num);
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

  int points_num() const;
  int curves_num() const;
  IndexRange points_range() const;
  IndexRange curves_range() const;

  /**
   * The index of the first point in every curve. The size of this span is one larger than the
   * number of curves. Consider using #points_for_curve rather than using the offsets directly.
   */
  Span<int> offsets() const;
  MutableSpan<int> offsets_for_write();

  /**
   * Access a range of indices of point data for a specific curve.
   */
  IndexRange points_for_curve(int index) const;
  IndexRange points_for_curves(IndexRange curves) const;

  /** The type (#CurveType) of each curve, or potentially a single if all are the same type. */
  VArray<int8_t> curve_types() const;
  /**
   * Mutable access to curve types. Call #tag_topology_changed and #update_curve_types after
   * changing any type. Consider using the other methods to change types below.
   * */
  MutableSpan<int8_t> curve_types_for_write();
  /** Set all curve types to the value and call #update_curve_types. */
  void fill_curve_types(CurveType type);
  /** Set the types for the curves in the selection and call #update_curve_types. */
  void fill_curve_types(IndexMask selection, CurveType type);
  /** Update the cached count of curves of each type, necessary after #curve_types_for_write. */
  void update_curve_types();

  bool has_curve_with_type(CurveType type) const;
  /** Return true if all of the curves have the provided type. */
  bool is_single_type(CurveType type) const;
  /** Return the number of curves with each type. */
  const std::array<int, CURVE_TYPES_NUM> &curve_type_counts() const;
  /**
   * All of the curve indices for curves with a specific type.
   */
  IndexMask indices_for_curve_type(CurveType type, Vector<int64_t> &r_indices) const;
  IndexMask indices_for_curve_type(CurveType type,
                                   IndexMask selection,
                                   Vector<int64_t> &r_indices) const;

  Span<float3> positions() const;
  MutableSpan<float3> positions_for_write();

  /** Whether the curve loops around to connect to itself, on the curve domain. */
  VArray<bool> cyclic() const;
  /** Mutable access to curve cyclic values. Call #tag_topology_changed after changes. */
  MutableSpan<bool> cyclic_for_write();

  /**
   * How many evaluated points to create for each segment when evaluating Bezier,
   * Catmull Rom, and NURBS curves. On the curve domain.
   */
  VArray<int> resolution() const;
  /** Mutable access to curve resolution. Call #tag_topology_changed after changes. */
  MutableSpan<int> resolution_for_write();

  /**
   * The angle used to rotate evaluated normals around the tangents after their calculation.
   * Call #tag_normals_changed after changes.
   */
  VArray<float> tilt() const;
  MutableSpan<float> tilt_for_write();

  /**
   * Which method to use for calculating the normals of evaluated points (#NormalMode).
   * Call #tag_normals_changed after changes.
   */
  VArray<int8_t> normal_mode() const;
  MutableSpan<int8_t> normal_mode_for_write();

  /**
   * Handle types for Bezier control points. Call #tag_topology_changed after changes.
   */
  VArray<int8_t> handle_types_left() const;
  MutableSpan<int8_t> handle_types_left_for_write();
  VArray<int8_t> handle_types_right() const;
  MutableSpan<int8_t> handle_types_right_for_write();

  /**
   * The positions of Bezier curve handles. Though these are really control points for the Bezier
   * segments, they are stored in separate arrays to better reflect user expectations. Note that
   * values may be generated automatically based on the handle types. Call #tag_positions_changed
   * after changes.
   */
  Span<float3> handle_positions_left() const;
  MutableSpan<float3> handle_positions_left_for_write();
  Span<float3> handle_positions_right() const;
  MutableSpan<float3> handle_positions_right_for_write();

  /**
   * The order (degree plus one) of each NURBS curve, on the curve domain.
   * Call #tag_topology_changed after changes.
   */
  VArray<int8_t> nurbs_orders() const;
  MutableSpan<int8_t> nurbs_orders_for_write();

  /**
   * The automatic generation mode for each NURBS curve's knots vector, on the curve domain.
   * Call #tag_topology_changed after changes.
   */
  VArray<int8_t> nurbs_knots_modes() const;
  MutableSpan<int8_t> nurbs_knots_modes_for_write();

  /**
   * The weight for each control point for NURBS curves. Call #tag_positions_changed after changes.
   */
  Span<float> nurbs_weights() const;
  MutableSpan<float> nurbs_weights_for_write();

  /**
   * The index of a triangle (#MLoopTri) that a curve is attached to.
   * The index is -1, if the curve is not attached.
   */
  VArray<int> surface_triangle_indices() const;
  MutableSpan<int> surface_triangle_indices_for_write();

  /**
   * Barycentric coordinates of the attachment point within a triangle.
   * Only the first two coordinates are stored. The third coordinate can be derived because the sum
   * of the three coordinates is 1.
   *
   * When the triangle index is -1, this coordinate should be ignored.
   * The span can be empty, when all triangle indices are -1.
   */
  Span<float2> surface_triangle_coords() const;
  MutableSpan<float2> surface_triangle_coords_for_write();

  /**
   * Calculate the largest and smallest position values, only including control points
   * (rather than evaluated points). The existing values of `min` and `max` are taken into account.
   *
   * \return Whether there are any points. If the curve is empty, the inputs will be unaffected.
   */
  bool bounds_min_max(float3 &min, float3 &max) const;

 private:
  /* --------------------------------------------------------------------
   * Evaluation.
   */

 public:
  /**
   * The total number of points in the evaluated poly curve.
   * This can depend on the resolution attribute if it exists.
   */
  int evaluated_points_num() const;

  /**
   * Access a range of indices of point data for a specific curve.
   * Call #evaluated_offsets() first to ensure that the evaluated offsets cache is current.
   */
  IndexRange evaluated_points_for_curve(int index) const;
  IndexRange evaluated_points_for_curves(IndexRange curves) const;

  /**
   * The index of the first evaluated point for every curve. The size of this span is one larger
   * than the number of curves. Consider using #evaluated_points_for_curve rather than using the
   * offsets directly.
   */
  Span<int> evaluated_offsets() const;

  /** Makes sure the data described by #evaluated_offsets if necessary. */
  void ensure_evaluated_offsets() const;

  /**
   * Retrieve offsets into a Bezier curve's evaluated points for each control point.
   * Call #ensure_evaluated_offsets() first to ensure that the evaluated offsets cache is current.
   */
  Span<int> bezier_evaluated_offsets_for_curve(int curve_index) const;

  Span<float3> evaluated_positions() const;
  Span<float3> evaluated_tangents() const;
  Span<float3> evaluated_normals() const;

  /**
   * Return a cache of accumulated lengths along the curve. Each item is the length of the
   * subsequent segment (the first value is the length of the first segment rather than 0).
   * This calculation is rather trivial, and only depends on the evaluated positions, but
   * the results are used often, and it is necessarily single threaded per curve, so it is cached.
   *
   * \param cyclic: This argument is redundant with the data stored for the curve,
   * but is passed for performance reasons to avoid looking up the attribute.
   */
  Span<float> evaluated_lengths_for_curve(int curve_index, bool cyclic) const;
  float evaluated_length_total_for_curve(int curve_index, bool cyclic) const;

  /** Calculates the data described by #evaluated_lengths_for_curve if necessary. */
  void ensure_evaluated_lengths() const;

  /**
   * Evaluate a generic data to the standard evaluated points of a specific curve,
   * defined by the resolution attribute or other factors, depending on the curve type.
   *
   * \warning This function expects offsets to the evaluated points for each curve to be
   * calculated. That can be ensured with #ensure_evaluated_offsets.
   */
  void interpolate_to_evaluated(int curve_index, GSpan src, GMutableSpan dst) const;
  /**
   * Evaluate generic data for curve control points to the standard evaluated points of the curves.
   */
  void interpolate_to_evaluated(GSpan src, GMutableSpan dst) const;

 private:
  /**
   * Make sure the basis weights for NURBS curve's evaluated points are calculated.
   */
  void ensure_nurbs_basis_cache() const;

  /** Return the slice of #evaluated_length_cache that corresponds to this curve index. */
  IndexRange lengths_range_for_curve(int curve_index, bool cyclic) const;

  /* --------------------------------------------------------------------
   * Operations.
   */

 public:
  /**
   * Change the number of elements. New values for existing attributes should be properly
   * initialized afterwards.
   */
  void resize(int points_num, int curves_num);

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

  void calculate_bezier_auto_handles();

  void update_customdata_pointers();

  void remove_curves(IndexMask curves_to_delete);

  /**
   * Change the direction of selected curves (switch the start and end) without changing their
   * shape.
   */
  void reverse_curves(IndexMask curves_to_reverse);

  /* --------------------------------------------------------------------
   * Attributes.
   */

  GVArray adapt_domain(const GVArray &varray, AttributeDomain from, AttributeDomain to) const;
};

namespace curves {

/* -------------------------------------------------------------------- */
/** \name Inline Curve Methods
 * \{ */

/**
 * The number of segments between control points, accounting for the last segment of cyclic
 * curves. The logic is simple, but this function should be used to make intentions clearer.
 */
inline int curve_segment_num(const int points_num, const bool cyclic)
{
  BLI_assert(points_num > 0);
  return (cyclic && points_num > 1) ? points_num : points_num - 1;
}

inline float2 encode_surface_bary_coord(const float3 &v)
{
  BLI_assert(std::abs(v.x + v.y + v.z - 1.0f) < 0.00001f);
  return {v.x, v.y};
}

inline float3 decode_surface_bary_coord(const float2 &v)
{
  return {v.x, v.y, 1.0f - v.x - v.y};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Poly Methods
 * \{ */

namespace poly {

/**
 * Calculate the direction at every point, defined as the normalized average of the two neighboring
 * segments (and if non-cyclic, the direction of the first and last segments). This is different
 * than evaluating the derivative of the basis functions for curve types like NURBS, Bezier, or
 * Catmull Rom, though the results may be similar.
 */
void calculate_tangents(Span<float3> positions, bool is_cyclic, MutableSpan<float3> tangents);

/**
 * Calculate directions perpendicular to the tangent at every point by rotating an arbitrary
 * starting vector by the same rotation of each tangent. If the curve is cyclic, propagate a
 * correction through the entire to make sure the first and last normal align.
 */
void calculate_normals_minimum(Span<float3> tangents, bool cyclic, MutableSpan<float3> normals);

/**
 * Calculate a vector perpendicular to every tangent on the X-Y plane (unless the tangent is
 * vertical, in that case use the X direction).
 */
void calculate_normals_z_up(Span<float3> tangents, MutableSpan<float3> normals);

}  // namespace poly

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Bezier Methods
 * \{ */

namespace bezier {

/**
 * Return true if the handles that make up a segment both have a vector type. Vector segments for
 * Bezier curves have special behavior because they aren't divided into many evaluated points.
 */
bool segment_is_vector(Span<int8_t> handle_types_left,
                       Span<int8_t> handle_types_right,
                       int segment_index);

/**
 * Return true if the curve's last cyclic segment has a vector type.
 * This only makes a difference in the shape of cyclic curves.
 */
bool last_cyclic_segment_is_vector(Span<int8_t> handle_types_left, Span<int8_t> handle_types_right);

/**
 * Return true if the handle types at the index are free (#BEZIER_HANDLE_FREE) or vector
 * (#BEZIER_HANDLE_VECTOR). In these cases, directional continuities from the previous and next
 * evaluated segments is assumed not to be desired.
 */
bool point_is_sharp(Span<int8_t> handle_types_left, Span<int8_t> handle_types_right, int index);

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
 * Recalculate all auto (#BEZIER_HANDLE_AUTO) and vector (#BEZIER_HANDLE_VECTOR) handles with
 * positions automatically derived from the neighboring control points, and update aligned
 * (#BEZIER_HANDLE_ALIGN) handles to line up with neighboring non-aligned handles. The choices
 * made here are relatively arbitrary, but having standardized behavior is essential.
 */
void calculate_auto_handles(bool cyclic,
                            Span<int8_t> types_left,
                            Span<int8_t> types_right,
                            Span<float3> positions,
                            MutableSpan<float3> positions_left,
                            MutableSpan<float3> positions_right);

/**
 * Change the handles of a single control point, aligning any aligned (#BEZIER_HANDLE_ALIGN)
 * handles on the other side of the control point.
 *
 * \note This ignores the inputs if the handle types are automatically calculated,
 * so the types should be updated before-hand to be editable.
 */
void set_handle_position(const float3 &position,
                         HandleType type,
                         HandleType type_other,
                         const float3 &new_handle,
                         float3 &handle,
                         float3 &handle_other);

/**
 * Evaluate a cubic Bezier segment, using the "forward differencing" method.
 * A generic Bezier curve is made up by four points, but in many cases the first and last
 * points are referred to as the control points, and the middle points are the corresponding
 * handles.
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

/**
 * Evaluate generic data to the evaluated points, with counts for each segment described by
 * #evaluated_offsets. Unlike other curve types, for Bezier curves generic data and positions
 * are treated separately, since attribute values aren't stored for the handle control points.
 */
void interpolate_to_evaluated(GSpan src, Span<int> evaluated_offsets, GMutableSpan dst);

}  // namespace bezier

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Catmull-Rom Methods
 * \{ */

namespace catmull_rom {

/**
 * Calculate the number of evaluated points that #interpolate_to_evaluated is expected to produce.
 * \param points_num: The number of points in the curve.
 * \param resolution: The resolution for each segment.
 */
int calculate_evaluated_num(int points_num, bool cyclic, int resolution);

/**
 * Evaluate the Catmull Rom curve. The length of the #dst span should be calculated with
 * #calculate_evaluated_num and is expected to divide evenly by the #src span's segment size.
 */
void interpolate_to_evaluated(GSpan src, bool cyclic, int resolution, GMutableSpan dst);

}  // namespace catmull_rom

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve NURBS Methods
 * \{ */

namespace nurbs {

/**
 * Checks the conditions that a NURBS curve needs to evaluate.
 */
bool check_valid_num_and_order(int points_num, int8_t order, bool cyclic, KnotsMode knots_mode);

/**
 * Calculate the standard evaluated size for a NURBS curve, using the standard that
 * the resolution is multiplied by the number of segments between the control points.
 *
 * \note Though the number of evaluated points is rather arbitrary, it's useful to have a standard
 * for predictability and so that cached basis weights of NURBS curves with these properties can be
 * shared.
 */
int calculate_evaluated_num(
    int points_num, int8_t order, bool cyclic, int resolution, KnotsMode knots_mode);

/**
 * Calculate the length of the knot vector for a NURBS curve with the given properties.
 * The knots must be longer for a cyclic curve, for example, in order to provide weights for the
 * last evaluated points that are also influenced by the first control points.
 */
int knots_num(int points_num, int8_t order, bool cyclic);

/**
 * Calculate the knots for a curve given its properties, based on built-in standards defined by
 * #KnotsMode.
 *
 * \note Theoretically any sorted values can be used for NURBS knots, but calculating based
 * on standard modes allows useful presets, automatic recalculation when the number of points
 * changes, and is generally more intuitive than defining the knot vector manually.
 */
void calculate_knots(
    int points_num, KnotsMode mode, int8_t order, bool cyclic, MutableSpan<float> knots);

/**
 * Based on the knots, the order, and other properties of a NURBS curve, calculate a cache that can
 * be used to more simply interpolate attributes to the evaluated points later. The cache includes
 * two pieces of information for every evaluated point: the first control point that influences it,
 * and a weight for each control point.
 */
void calculate_basis_cache(int points_num,
                           int evaluated_num,
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
                              GSpan src,
                              GMutableSpan dst);

}  // namespace nurbs

/** \} */

}  // namespace curves

Curves *curves_new_nomain(int points_num, int curves_num);
Curves *curves_new_nomain(CurvesGeometry curves);

/**
 * Create a new curves data-block containing a single curve with the given length and type.
 */
Curves *curves_new_nomain_single(int points_num, CurveType type);

std::array<int, CURVE_TYPES_NUM> calculate_type_counts(const VArray<int8_t> &types);

/* -------------------------------------------------------------------- */
/** \name #CurvesGeometry Inline Methods
 * \{ */

inline int CurvesGeometry::points_num() const
{
  return this->point_num;
}
inline int CurvesGeometry::curves_num() const
{
  return this->curve_num;
}
inline IndexRange CurvesGeometry::points_range() const
{
  return IndexRange(this->points_num());
}
inline IndexRange CurvesGeometry::curves_range() const
{
  return IndexRange(this->curves_num());
}

inline bool CurvesGeometry::is_single_type(const CurveType type) const
{
  return this->curve_type_counts()[type] == this->curves_num();
}

inline bool CurvesGeometry::has_curve_with_type(const CurveType type) const
{
  return this->curve_type_counts()[type] > 0;
}

inline const std::array<int, CURVE_TYPES_NUM> &CurvesGeometry::curve_type_counts() const
{
  BLI_assert(this->runtime->type_counts == calculate_type_counts(this->curve_types()));
  return this->runtime->type_counts;
}

inline IndexRange CurvesGeometry::points_for_curve(const int index) const
{
  /* Offsets are not allocated when there are no curves. */
  BLI_assert(this->curve_num > 0);
  BLI_assert(this->curve_offsets != nullptr);
  const int offset = this->curve_offsets[index];
  const int offset_next = this->curve_offsets[index + 1];
  return {offset, offset_next - offset};
}

inline IndexRange CurvesGeometry::points_for_curves(const IndexRange curves) const
{
  /* Offsets are not allocated when there are no curves. */
  BLI_assert(this->curve_num > 0);
  BLI_assert(this->curve_offsets != nullptr);
  const int offset = this->curve_offsets[curves.start()];
  const int offset_next = this->curve_offsets[curves.one_after_last()];
  return {offset, offset_next - offset};
}

inline int CurvesGeometry::evaluated_points_num() const
{
  /* This could avoid calculating offsets in the future in simple circumstances. */
  return this->evaluated_offsets().last();
}

inline IndexRange CurvesGeometry::evaluated_points_for_curve(int index) const
{
  BLI_assert(!this->runtime->offsets_cache_dirty);
  return offsets_to_range(this->runtime->evaluated_offsets_cache.as_span(), index);
}

inline IndexRange CurvesGeometry::evaluated_points_for_curves(const IndexRange curves) const
{
  BLI_assert(!this->runtime->offsets_cache_dirty);
  BLI_assert(this->curve_num > 0);
  const int offset = this->runtime->evaluated_offsets_cache[curves.start()];
  const int offset_next = this->runtime->evaluated_offsets_cache[curves.one_after_last()];
  return {offset, offset_next - offset};
}

inline Span<int> CurvesGeometry::bezier_evaluated_offsets_for_curve(const int curve_index) const
{
  const IndexRange points = this->points_for_curve(curve_index);
  return this->runtime->bezier_evaluated_offsets.as_span().slice(points);
}

inline IndexRange CurvesGeometry::lengths_range_for_curve(const int curve_index,
                                                          const bool cyclic) const
{
  BLI_assert(cyclic == this->cyclic()[curve_index]);
  const IndexRange points = this->evaluated_points_for_curve(curve_index);
  const int start = points.start() + curve_index;
  return {start, curves::curve_segment_num(points.size(), cyclic)};
}

inline Span<float> CurvesGeometry::evaluated_lengths_for_curve(const int curve_index,
                                                               const bool cyclic) const
{
  BLI_assert(!this->runtime->length_cache_dirty);
  const IndexRange range = this->lengths_range_for_curve(curve_index, cyclic);
  return this->runtime->evaluated_length_cache.as_span().slice(range);
}

inline float CurvesGeometry::evaluated_length_total_for_curve(const int curve_index,
                                                              const bool cyclic) const
{
  const Span<float> lengths = this->evaluated_lengths_for_curve(curve_index, cyclic);
  if (lengths.is_empty()) {
    return 0.0f;
  }
  return lengths.last();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bezier Inline Methods
 * \{ */

namespace curves::bezier {

inline bool point_is_sharp(const Span<int8_t> handle_types_left,
                           const Span<int8_t> handle_types_right,
                           const int index)
{
  return ELEM(handle_types_left[index], BEZIER_HANDLE_VECTOR, BEZIER_HANDLE_FREE) ||
         ELEM(handle_types_right[index], BEZIER_HANDLE_VECTOR, BEZIER_HANDLE_FREE);
}

/** \} */

}  // namespace curves::bezier

}  // namespace blender::bke
