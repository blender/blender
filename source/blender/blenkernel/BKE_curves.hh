/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Low-level operations for curves.
 */

#include "BLI_array_utils.hh"
#include "BLI_bounds_types.hh"
#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_memory_counter_fwd.hh"
#include "BLI_offset_indices.hh"
#include "BLI_shared_cache.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array_fwd.hh"

#include "BKE_attribute_math.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_curves.h"

struct BlendDataReader;
struct BlendWriter;
struct MDeformVert;
namespace blender::bke {
class AttributeAccessor;
class MutableAttributeAccessor;
enum class AttrDomain : int8_t;
struct AttributeAccessorFunctions;
}  // namespace blender::bke
namespace blender::bke::bake {
struct BakeMaterialsList;
}
namespace blender {
class GVArray;
}

namespace blender::bke {

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
   * The result of #check_valid_eval_params, to avoid retrieving its inputs later on.
   * If this is true, the data above will be invalid, and original data should be copied
   * to the evaluated result.
   */
  bool invalid = false;
};

}  // namespace curves::nurbs

/**
 * Contains derived data, caches, and other information not saved in files.
 */
class CurvesGeometryRuntime {
 public:
  /** Implicit sharing user count for #CurvesGeometry::curve_offsets. */
  const ImplicitSharingInfo *curve_offsets_sharing_info = nullptr;

  /** Implicit sharing user count for #CurvesGeometry::custom_knots. */
  const ImplicitSharingInfo *custom_knots_sharing_info = nullptr;

  /**
   * The cached number of curves with each type. Unlike other caches here, this is not computed
   * lazily, since it is needed so often and types are not adjusted much anyway.
   */
  std::array<int, CURVE_TYPES_NUM> type_counts;

  /**
   * Cache of offsets into the evaluated array for each curve, accounting for all previous
   * evaluated points, Bezier curve vector segments, different resolutions per curve, etc.
   */
  struct EvaluatedOffsets {
    Vector<int> evaluated_offsets;
    Vector<int> all_bezier_offsets;
  };
  mutable SharedCache<EvaluatedOffsets> evaluated_offsets_cache;

  mutable SharedCache<bool> has_cyclic_curve_cache;

  mutable SharedCache<Vector<curves::nurbs::BasisCache>> nurbs_basis_cache;

  /**
   * Cache of evaluated positions for all curves. The positions span will
   * be used directly rather than the cache when all curves are poly type.
   */
  mutable SharedCache<Vector<float3>> evaluated_position_cache;

  /**
   * A cache of bounds shared between data-blocks with unchanged positions.
   * When data changes affect the bounds, the cache is "un-shared" with other geometries.
   * See #SharedCache comments.
   */
  mutable SharedCache<Bounds<float3>> bounds_cache;
  mutable SharedCache<Bounds<float3>> bounds_with_radius_cache;

  /**
   * Cache of lengths along each evaluated curve for each evaluated point. If a curve is
   * cyclic, it needs one more length value to correspond to the last segment, so in order to
   * make slicing this array for a curve fast, an extra float is stored for every curve.
   */
  mutable SharedCache<Vector<float>> evaluated_length_cache;

  /** Direction of the curve at each evaluated point. */
  mutable SharedCache<Vector<float3>> evaluated_tangent_cache;

  /** Normal direction vectors for each evaluated point. */
  mutable SharedCache<Vector<float3>> evaluated_normal_cache;

  /** The maximum of the "material_index" attribute. */
  mutable SharedCache<std::optional<int>> max_material_index_cache;

  /**
   * Offsets of custom knots in #CurvesGeometry::custom_knots for each curve in #CurvesGeometry.
   * For curves with no custom knots next offset value stays the same.
   */
  mutable SharedCache<Vector<int>> custom_knot_offsets_cache;

  /** Stores weak references to material data blocks. */
  std::unique_ptr<bake::BakeMaterialsList> bake_materials;

  /**
   * Type counts have to be set eagerly after each operation. It's checked with asserts that the
   * type counts are correct when accessed. However, this check is expensive and shouldn't be done
   * all the time because it makes debug builds unusable in some situations that would be fine
   * otherwise.
   */
  bool check_type_counts = true;
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

  /* --------------------------------------------------------------------
   * Accessors.
   */

  /**
   * The total number of control points in all curves.
   */
  int points_num() const;
  /**
   * The number of curves in the data-block.
   */
  int curves_num() const;
  /**
   * Return true if there are no curves in the geometry.
   */
  bool is_empty() const;

  IndexRange points_range() const;
  IndexRange curves_range() const;

  /**
   * The index of the first point in every curve. The size of this span is one larger than the
   * number of curves, but the spans will be empty if there are no curves/points.
   *
   * Consider using #points_by_curve rather than these offsets directly.
   */
  Span<int> offsets() const;
  MutableSpan<int> offsets_for_write();

  /**
   * The offsets of every curve into arrays on the points domain.
   */
  OffsetIndices<int> points_by_curve() const;

  /** The type (#CurveType) of each curve, or potentially a single if all are the same type. */
  VArray<int8_t> curve_types() const;
  /**
   * Mutable access to curve types. Call #tag_topology_changed and #update_curve_types after
   * changing any type. Consider using the other methods to change types below.
   */
  MutableSpan<int8_t> curve_types_for_write();
  /** Set all curve types to the value and call #update_curve_types. */
  void fill_curve_types(CurveType type);
  /** Set the types for the curves in the selection and call #update_curve_types. */
  void fill_curve_types(const IndexMask &selection, CurveType type);
  /** Update the cached count of curves of each type, necessary after #curve_types_for_write. */
  void update_curve_types();

  bool has_curve_with_type(CurveType type) const;
  bool has_curve_with_type(Span<CurveType> types) const;
  /** Return true if all of the curves have the provided type. */
  bool is_single_type(CurveType type) const;
  /** Return the number of curves with each type. */
  const std::array<int, CURVE_TYPES_NUM> &curve_type_counts() const;
  /**
   * All of the curve indices for curves with a specific type.
   */
  IndexMask indices_for_curve_type(CurveType type, IndexMaskMemory &memory) const;
  IndexMask indices_for_curve_type(CurveType type,
                                   const IndexMask &selection,
                                   IndexMaskMemory &memory) const;

  Array<int> point_to_curve_map() const;

  Span<float3> positions() const;
  MutableSpan<float3> positions_for_write();

  VArray<float> radius() const;
  MutableSpan<float> radius_for_write();

  /** Whether the curve loops around to connect to itself, on the curve domain. */
  VArray<bool> cyclic() const;
  /** Mutable access to curve cyclic values. Call #tag_topology_changed after changes. */
  MutableSpan<bool> cyclic_for_write();

  /**
   * How many evaluated points to create for each segment when evaluating Bezier,
   * Catmull Rom, and NURBS curves. On the curve domain. Values must be one or greater.
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
  std::optional<Span<float3>> handle_positions_left() const;
  MutableSpan<float3> handle_positions_left_for_write();
  std::optional<Span<float3>> handle_positions_right() const;
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
  std::optional<Span<float>> nurbs_weights() const;
  MutableSpan<float> nurbs_weights_for_write();

  /**
   * UV coordinate for each curve that encodes where the curve is attached to the surface mesh.
   */
  std::optional<Span<float2>> surface_uv_coords() const;
  MutableSpan<float2> surface_uv_coords_for_write();

  /**
   * Custom knots for NURBS curves with knots mode #NURBS_KNOT_MODE_CUSTOM.
   */
  Span<float> nurbs_custom_knots() const;
  MutableSpan<float> nurbs_custom_knots_for_write();

  /**
   * The offsets of every curve into arrays on #CurvesGeometry::nurbs_custom_knots.
   * Curves with knot mode other than #NURBS_KNOT_MODE_CUSTOM will have zero sized #IndexRange.
   */
  OffsetIndices<int> nurbs_custom_knots_by_curve() const;

  /**
   * Builds mask of NURBS curves with knot mode #NURBS_KNOT_MODE_CUSTOM.
   */
  IndexMask nurbs_custom_knot_curves(IndexMaskMemory &memory) const;

  bool nurbs_has_custom_knots() const;

  /**
   * Resizes custom knots array depending on topological data.
   * Depends on curve offsets, knot modes, orders and cyclic data.
   * Used to resize internal knots array before writing knots.
   */
  void nurbs_custom_knots_update_size();

  /**
   * Resizes custom knots array. Used when knots number is known in advance and knot values are set
   * together with topological data.
   */
  void nurbs_custom_knots_resize(int knots_num);

  /**
   * Vertex group data, encoded as an array of indices and weights for every vertex.
   * \warning: May be empty.
   */
  Span<MDeformVert> deform_verts() const;
  MutableSpan<MDeformVert> deform_verts_for_write();

  /**
   * The largest and smallest position values of evaluated points.
   */
  std::optional<Bounds<float3>> bounds_min_max(bool use_radius = true) const;

  void count_memory(MemoryCounter &memory) const;

  /**
   * Get the largest material index used by the geometry or `nullopt` if there are none.
   * The returned value is clamped between 0 and MAXMAT even if the stored material indices may be
   * out of that range.
   */
  std::optional<int> material_index_max() const;

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
   * The offsets of every curve's evaluated points.
   */
  OffsetIndices<int> evaluated_points_by_curve() const;

  /**
   * Retrieve offsets into a Bezier curve's evaluated points for each control point. Stored in the
   * same format as #OffsetIndices. Call #evaluated_points_by_curve() first to ensure that the
   * evaluated offsets cache is current.
   */
  Span<int> bezier_evaluated_offsets_for_curve(int curve_index) const;

  bool has_cyclic_curve() const;

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

  void ensure_can_interpolate_to_evaluated() const;

  /**
   * Evaluate a generic data to the standard evaluated points of a specific curve,
   * defined by the resolution attribute or other factors, depending on the curve type.
   *
   * \warning This function expects offsets to the evaluated points for each curve to be
   * calculated. That can be ensured with #ensure_can_interpolate_to_evaluated.
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
   * Change the number of curves and/or points.
   *
   * \warning To avoid redundant writes, newly created attribute values are not initialized.
   * They must be initialized by the caller afterwards.
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
  /**
   * Call when making manual changes to the "radius" attribute. The attribute API will also call
   * this in #finish() calls.
   */
  void tag_radii_changed();
  /** Call after changing the "material_index" attribute. */
  void tag_material_index_changed();

  void translate(const float3 &translation);
  void transform(const float4x4 &matrix);

  void calculate_bezier_auto_handles();

  void remove_points(const IndexMask &points_to_delete, const AttributeFilter &attribute_filter);
  void remove_curves(const IndexMask &curves_to_delete, const AttributeFilter &attribute_filter);

  /**
   * Change the direction of selected curves (switch the start and end) without changing their
   * shape.
   */
  void reverse_curves(const IndexMask &curves_to_reverse);

  /**
   * Remove any attributes that are unused based on the types in the curves.
   */
  void remove_attributes_based_on_types();

  AttributeAccessor attributes() const;
  MutableAttributeAccessor attributes_for_write();

  /* --------------------------------------------------------------------
   * Attributes.
   */

  GVArray adapt_domain(const GVArray &varray, AttrDomain from, AttrDomain to) const;
  template<typename T>
  VArray<T> adapt_domain(const VArray<T> &varray, AttrDomain from, AttrDomain to) const
  {
    return this->adapt_domain(GVArray(varray), from, to).typed<T>();
  }

  /* --------------------------------------------------------------------
   * File Read/Write.
   */
  void blend_read(BlendDataReader &reader);
  /**
   * Helper struct for `CurvesGeometry::blend_write_*` functions.
   */
  struct BlendWriteData {
    ResourceScope &scope;
    Vector<CustomDataLayer, 16> &point_layers;
    Vector<CustomDataLayer, 16> &curve_layers;
    AttributeStorage::BlendWriteData attribute_data;
    explicit BlendWriteData(ResourceScope &scope);
  };
  /**
   * This function needs to be called before `blend_write` and before the `CurvesGeometry` struct
   * is written because it can mutate the `CustomData` and `AttributeStorage` structs.
   */
  void blend_write_prepare(BlendWriteData &write_data);
  void blend_write(BlendWriter &writer, ID &id, const BlendWriteData &write_data);
};

static_assert(sizeof(blender::bke::CurvesGeometry) == sizeof(::CurvesGeometry));

/**
 * Used to propagate deformation data through modifier evaluation so that sculpt tools can work on
 * evaluated data.
 */
class CurvesEditHints {
 public:
  /**
   * Original data that the edit hints below are meant to be used for.
   */
  const Curves &curves_id_orig;
  /**
   * Evaluated positions for the points in #curves_orig. If this is empty, the positions from the
   * evaluated #Curves should be used if possible.
   */
  ImplicitSharingPtrAndData positions_data;
  /**
   * Matrices which transform point movement vectors from original data to corresponding movements
   * of evaluated data.
   */
  std::optional<Array<float3x3>> deform_mats;

  CurvesEditHints(const Curves &curves_id_orig) : curves_id_orig(curves_id_orig) {}

  std::optional<Span<float3>> positions() const;
  std::optional<MutableSpan<float3>> positions_for_write();

  /**
   * The edit hints have to correspond to the original curves, i.e. the number of deformed points
   * is the same as the number of original points.
   */
  bool is_valid() const;
};

void curves_normals_point_domain_calc(const CurvesGeometry &curves, MutableSpan<float3> normals);

namespace curves {

/* -------------------------------------------------------------------- */
/** \name Inline Curve Methods
 * \{ */

/**
 * The number of segments between control points, accounting for the last segment of cyclic
 * curves. The logic is simple, but this function should be used to make intentions clearer.
 */
inline int segments_num(const int points_num, const bool cyclic)
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

/**
 * Return a range used to retrieve values from an array of values stored per point, but with an
 * extra element at the end of each curve. This is useful for offsets within curves, where it is
 * convenient to store the first 0 and have the last offset be the total result curve size, using
 * the same rules as #OffsetIndices.
 */
inline IndexRange per_curve_point_offsets_range(const IndexRange points, const int curve_index)
{
  return {curve_index + points.start(), points.size() + 1};
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
bool segment_is_vector(const HandleType left, const HandleType right);
bool segment_is_vector(const int8_t left, const int8_t right);
bool segment_is_vector(Span<int8_t> handle_types_left,
                       Span<int8_t> handle_types_right,
                       int segment_index);

/**
 * True if the Bezier curve contains polygonal segments of HandleType::BEZIER_HANDLE_VECTOR.
 *
 * \param num_curve_points: Number of points in the curve.
 * \param evaluated_size: Number of evaluated points in the curve.
 * \param cyclic: If curve is cyclic.
 * \param resolution: Curve resolution.
 */
bool has_vector_handles(int num_curve_points, int64_t evaluated_size, bool cyclic, int resolution);

/**
 * Return true if the curve's last cyclic segment has a vector type.
 * This only makes a difference in the shape of cyclic curves.
 */
bool last_cyclic_segment_is_vector(Span<int8_t> handle_types_left,
                                   Span<int8_t> handle_types_right);

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
 * The expectations for the result \a evaluated_offsets are the same as for #OffsetIndices, so the
 * size must be one greater than the number of points. The value at each index is the evaluated
 * point at the start of that segment.
 */
void calculate_evaluated_offsets(Span<int8_t> handle_types_left,
                                 Span<int8_t> handle_types_right,
                                 bool cyclic,
                                 int resolution,
                                 MutableSpan<int> evaluated_offsets);

/** Knot insertion result, see #insert. */
struct Insertion {
  float3 handle_prev;
  float3 left_handle;
  float3 position;
  float3 right_handle;
  float3 handle_next;
};

/**
 * Compute the insertion of a control point and handles in a Bezier segment without changing its
 * shape.
 * \param parameter: Factor in from 0 to 1 defining the insertion point within the segment.
 * \return Inserted point parameters including position, and both new and updated handles for
 * neighboring control points.
 *
 * <pre>
 *           handle_prev         handle_next
 *                x-----------------x
 *               /                   \
 *              /      x---O---x      \
 *             /        result         \
 *            /                         \
 *           O                           O
 *       point_prev                   point_next
 * </pre>
 */
Insertion insert(const float3 &point_prev,
                 const float3 &handle_prev,
                 const float3 &handle_next,
                 const float3 &point_next,
                 float parameter);

/**
 * Calculate the automatically defined positions for a vector handle (#BEZIER_HANDLE_VECTOR). While
 * this can be calculated automatically with #calculate_auto_handles, when more context is
 * available, it can be preferable for performance reasons to calculate it for a single segment
 * when necessary.
 */
float3 calculate_vector_handle(const float3 &point, const float3 &next_point);

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

void calculate_aligned_handles(const IndexMask &selection,
                               Span<float3> positions,
                               Span<float3> align_by,
                               MutableSpan<float3> align);

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
template<typename T>
void evaluate_segment(
    const T &point_0, const T &point_1, const T &point_2, const T &point_3, MutableSpan<T> result);

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
                                   OffsetIndices<int> evaluated_offsets,
                                   MutableSpan<float3> evaluated_positions);

/**
 * Evaluate generic data to the evaluated points, with counts for each segment described by
 * #evaluated_offsets. Unlike other curve types, for Bezier curves generic data and positions
 * are treated separately, since attribute values aren't stored for the handle control points.
 */
void interpolate_to_evaluated(GSpan src, OffsetIndices<int> evaluated_offsets, GMutableSpan dst);

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

/**
 * Evaluate the Catmull Rom curve. The placement of each segment in the #dst span is described by
 * #evaluated_offsets.
 */
void interpolate_to_evaluated(const GSpan src,
                              const bool cyclic,
                              const OffsetIndices<int> evaluated_offsets,
                              GMutableSpan dst);

float4 calculate_basis(const float parameter);

/**
 * Interpolate the control point values for the given parameter on the piecewise segment.
 * \param a: Value associated with the first control point influencing the segment.
 * \param d: Value associated with the fourth control point.
 * \param parameter: Parameter in range [0, 1] to compute the interpolation for.
 */
template<typename T>
T interpolate(const T &a, const T &b, const T &c, const T &d, const float parameter)
{
  BLI_assert(0.0f <= parameter && parameter <= 1.0f);
  const float4 weights = calculate_basis(parameter);
  if constexpr (is_same_any_v<T, float, float2, float3>) {
    /* Save multiplications by adjusting weights after mix. */
    return 0.5f * attribute_math::mix4<T>(weights, a, b, c, d);
  }
  else {
    return attribute_math::mix4<T>(weights * 0.5f, a, b, c, d);
  }
}

}  // namespace catmull_rom

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve NURBS Methods
 * \{ */

namespace nurbs {

/**
 * Checks the conditions that a NURBS curve needs to evaluate.
 */
bool check_valid_eval_params(
    int points_num, int8_t order, bool cyclic, KnotsMode knots_mode, int resolution);

/**
 * Calculate the standard evaluated size for a NURBS curve, using the standard that
 * the resolution is multiplied by the number of segments between the control points.
 *
 * \note Though the number of evaluated points is rather arbitrary, it's useful to have a standard
 * for predictability and so that cached basis weights of NURBS curves with these properties can be
 * shared.
 */
int calculate_evaluated_num(int points_num,
                            int8_t order,
                            bool cyclic,
                            int resolution,
                            KnotsMode knots_mode,
                            Span<float> knots);

/**
 * Calculate the length of the knot vector for a NURBS curve with the given properties.
 * The knots must be longer for a cyclic curve, for example, in order to provide weights for the
 * last evaluated points that are also influenced by the first control points.
 */
int knots_num(int points_num, int8_t order, bool cyclic);

/**
 * Calculate the total number of control points for a NURBS curve including virtual/repeated points
 * for a cyclic/closed curve.
 */
int control_points_num(int num_control_points, int8_t order, bool cyclic);

/**
 * Depending on KnotsMode calculates knots or copies custom knots into given `MutableSpan`.
 * Adds `order - 1` length tail for cyclic curves.
 */
void load_curve_knots(KnotsMode mode,
                      int points_num,
                      int8_t order,
                      bool cyclic,
                      IndexRange curve_knots,
                      Span<float> custom_knots,
                      MutableSpan<float> knots);

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
 * Compute the number of occurrences of each unique knot value (so knot multiplicity),
 * forming a sequence for which: `sum(multiplicity) == knots.size()`.
 *
 * Example:
 * Knots: [0, 0, 0, 0.1, 0.3, 0.4, 0.4, 0.4]
 * Result: [3, 1, 1, 3]
 */
Vector<int> calculate_multiplicity_sequence(Span<float> knots);

/**
 * Based on the knots, the order, and other properties of a NURBS curve, calculate a cache that can
 * be used to more simply interpolate attributes to the evaluated points later. The cache includes
 * two pieces of information for every evaluated point: the first control point that influences it,
 * and a weight for each control point.
 */
void calculate_basis_cache(int points_num,
                           int evaluated_num,
                           int8_t order,
                           int resolution,
                           bool cyclic,
                           KnotsMode knots_mode,
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

/**
 * Copy data from #src to #dst, except the geometry data in #CurvesGeometry. Typically used to
 * copy high-level parameters when a geometry-altering operation creates a new curves data-block.
 */
void curves_copy_parameters(const Curves &src, Curves &dst);

CurvesGeometry curves_copy_point_selection(const CurvesGeometry &curves,
                                           const IndexMask &points_to_copy,
                                           const AttributeFilter &attribute_filter);

CurvesGeometry curves_copy_curve_selection(const CurvesGeometry &curves,
                                           const IndexMask &curves_to_copy,
                                           const AttributeFilter &attribute_filter);

CurvesGeometry curves_new_no_attributes(int point_num, int curve_num);

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
inline bool CurvesGeometry::nurbs_has_custom_knots() const
{
  return this->custom_knot_num != 0;
}
inline bool CurvesGeometry::is_empty() const
{
  /* Each curve must have at least one point. */
  BLI_assert((this->curve_num == 0) == (this->point_num == 0));
  return this->curve_num == 0;
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

inline bool CurvesGeometry::has_curve_with_type(const Span<CurveType> types) const
{
  return std::any_of(
      types.begin(), types.end(), [&](CurveType type) { return this->has_curve_with_type(type); });
}

inline const std::array<int, CURVE_TYPES_NUM> &CurvesGeometry::curve_type_counts() const
{
#ifndef NDEBUG

  if (this->runtime->check_type_counts) {
    const std::array<int, CURVE_TYPES_NUM> actual_type_counts = calculate_type_counts(
        this->curve_types());
    BLI_assert(this->runtime->type_counts == actual_type_counts);
    this->runtime->check_type_counts = false;
  }
#endif
  return this->runtime->type_counts;
}

inline OffsetIndices<int> CurvesGeometry::points_by_curve() const
{
  return OffsetIndices<int>({this->curve_offsets, this->curve_num + 1},
                            offset_indices::NoSortCheck{});
}

inline int CurvesGeometry::evaluated_points_num() const
{
  /* This could avoid calculating offsets in the future in simple circumstances. */
  return this->evaluated_points_by_curve().total_size();
}

inline Span<int> CurvesGeometry::bezier_evaluated_offsets_for_curve(const int curve_index) const
{
  const OffsetIndices points_by_curve = this->points_by_curve();
  const IndexRange points = points_by_curve[curve_index];
  const IndexRange range = curves::per_curve_point_offsets_range(points, curve_index);
  const Span<int> offsets = this->runtime->evaluated_offsets_cache.data().all_bezier_offsets;
  return offsets.slice(range);
}

inline IndexRange CurvesGeometry::lengths_range_for_curve(const int curve_index,
                                                          const bool cyclic) const
{
  BLI_assert(cyclic == this->cyclic()[curve_index]);
  const IndexRange points = this->evaluated_points_by_curve()[curve_index];
  const int start = points.start() + curve_index;
  return {start, curves::segments_num(points.size(), cyclic)};
}

inline Span<float> CurvesGeometry::evaluated_lengths_for_curve(const int curve_index,
                                                               const bool cyclic) const
{
  const IndexRange range = this->lengths_range_for_curve(curve_index, cyclic);
  return this->runtime->evaluated_length_cache.data().as_span().slice(range);
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

namespace curves {

/* -------------------------------------------------------------------- */
/** \name Bezier Inline Methods
 * \{ */

namespace bezier {

inline bool point_is_sharp(const Span<int8_t> handle_types_left,
                           const Span<int8_t> handle_types_right,
                           const int index)
{
  return ELEM(handle_types_left[index], BEZIER_HANDLE_VECTOR, BEZIER_HANDLE_FREE) ||
         ELEM(handle_types_right[index], BEZIER_HANDLE_VECTOR, BEZIER_HANDLE_FREE);
}

inline bool segment_is_vector(const HandleType left, const HandleType right)
{
  return left == BEZIER_HANDLE_VECTOR && right == BEZIER_HANDLE_VECTOR;
}

inline bool segment_is_vector(const int8_t left, const int8_t right)
{
  return segment_is_vector(HandleType(left), HandleType(right));
}

inline bool has_vector_handles(const int num_curve_points,
                               const int64_t evaluated_size,
                               const bool cyclic,
                               const int resolution)
{
  return evaluated_size - !cyclic != int64_t(segments_num(num_curve_points, cyclic)) * resolution;
}

inline float3 calculate_vector_handle(const float3 &point, const float3 &next_point)
{
  return math::interpolate(point, next_point, 1.0f / 3.0f);
}

}  // namespace bezier

/** \} */

/* -------------------------------------------------------------------- */
/** \name NURBS Inline Methods
 * \{ */

namespace nurbs {

inline int knots_num(const int points_num, const int8_t order, const bool cyclic)
{
  /* Cyclic: points_num + order * 2 - 1 */
  return points_num + order + cyclic * (order - 1);
}

inline int control_points_num(const int points_num, const int8_t order, const bool cyclic)
{
  return points_num + cyclic * (order - 1);
}

}  // namespace nurbs

/** \} */

const AttributeAccessorFunctions &get_attribute_accessor_functions();

}  // namespace curves

struct CurvesSurfaceTransforms {
  float4x4 curves_to_world;
  float4x4 curves_to_surface;
  float4x4 world_to_curves;
  float4x4 world_to_surface;
  float4x4 surface_to_world;
  float4x4 surface_to_curves;
  float4x4 surface_to_curves_normal;

  CurvesSurfaceTransforms() = default;
  CurvesSurfaceTransforms(const Object &curves_ob, const Object *surface_ob);
};

}  // namespace blender::bke

inline blender::bke::CurvesGeometry &CurvesGeometry::wrap()
{
  return *reinterpret_cast<blender::bke::CurvesGeometry *>(this);
}
inline const blender::bke::CurvesGeometry &CurvesGeometry::wrap() const
{
  return *reinterpret_cast<const blender::bke::CurvesGeometry *>(this);
}
