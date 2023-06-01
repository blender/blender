/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <mutex>
#include <utility>

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_bounds.hh"
#include "BLI_index_mask.hh"
#include "BLI_length_parameterize.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation_legacy.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_task.hh"

#include "BLO_read_write.h"

#include "DNA_curves_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_customdata.h"

namespace blender::bke {

static const std::string ATTR_POSITION = "position";
static const std::string ATTR_RADIUS = "radius";
static const std::string ATTR_TILT = "tilt";
static const std::string ATTR_CURVE_TYPE = "curve_type";
static const std::string ATTR_CYCLIC = "cyclic";
static const std::string ATTR_RESOLUTION = "resolution";
static const std::string ATTR_NORMAL_MODE = "normal_mode";
static const std::string ATTR_HANDLE_TYPE_LEFT = "handle_type_left";
static const std::string ATTR_HANDLE_TYPE_RIGHT = "handle_type_right";
static const std::string ATTR_HANDLE_POSITION_LEFT = "handle_left";
static const std::string ATTR_HANDLE_POSITION_RIGHT = "handle_right";
static const std::string ATTR_NURBS_ORDER = "nurbs_order";
static const std::string ATTR_NURBS_WEIGHT = "nurbs_weight";
static const std::string ATTR_NURBS_KNOTS_MODE = "knots_mode";
static const std::string ATTR_SURFACE_UV_COORDINATE = "surface_uv_coordinate";

/* -------------------------------------------------------------------- */
/** \name Constructors/Destructor
 * \{ */

CurvesGeometry::CurvesGeometry() : CurvesGeometry(0, 0) {}

CurvesGeometry::CurvesGeometry(const int point_num, const int curve_num)
{
  this->point_num = point_num;
  this->curve_num = curve_num;
  CustomData_reset(&this->point_data);
  CustomData_reset(&this->curve_data);

  CustomData_add_layer_named(
      &this->point_data, CD_PROP_FLOAT3, CD_CONSTRUCT, this->point_num, ATTR_POSITION.c_str());

  this->runtime = MEM_new<CurvesGeometryRuntime>(__func__);

  if (curve_num > 0) {
    this->curve_offsets = static_cast<int *>(
        MEM_malloc_arrayN(this->curve_num + 1, sizeof(int), __func__));
    this->runtime->curve_offsets_sharing_info = implicit_sharing::info_for_mem_free(
        this->curve_offsets);
#ifdef DEBUG
    this->offsets_for_write().fill(-1);
#endif
    /* Set common values for convenience. */
    this->curve_offsets[0] = 0;
    this->curve_offsets[this->curve_num] = this->point_num;
  }
  else {
    this->curve_offsets = nullptr;
  }

  /* Fill the type counts with the default so they're in a valid state. */
  this->runtime->type_counts[CURVE_TYPE_CATMULL_ROM] = curve_num;
}

/**
 * \note Expects `dst` to be initialized, since the original attributes must be freed.
 */
static void copy_curves_geometry(CurvesGeometry &dst, const CurvesGeometry &src)
{
  CustomData_free(&dst.point_data, dst.point_num);
  CustomData_free(&dst.curve_data, dst.curve_num);
  dst.point_num = src.point_num;
  dst.curve_num = src.curve_num;
  CustomData_copy(&src.point_data, &dst.point_data, CD_MASK_ALL, dst.point_num);
  CustomData_copy(&src.curve_data, &dst.curve_data, CD_MASK_ALL, dst.curve_num);

  implicit_sharing::copy_shared_pointer(src.curve_offsets,
                                        src.runtime->curve_offsets_sharing_info,
                                        &dst.curve_offsets,
                                        &dst.runtime->curve_offsets_sharing_info);

  dst.tag_topology_changed();

  /* Though type counts are a cache, they must be copied because they are calculated eagerly. */
  dst.runtime->type_counts = src.runtime->type_counts;
  dst.runtime->evaluated_offsets_cache = src.runtime->evaluated_offsets_cache;
  dst.runtime->nurbs_basis_cache = src.runtime->nurbs_basis_cache;
  dst.runtime->evaluated_position_cache = src.runtime->evaluated_position_cache;
  dst.runtime->bounds_cache = src.runtime->bounds_cache;
  dst.runtime->evaluated_length_cache = src.runtime->evaluated_length_cache;
  dst.runtime->evaluated_tangent_cache = src.runtime->evaluated_tangent_cache;
  dst.runtime->evaluated_normal_cache = src.runtime->evaluated_normal_cache;
}

CurvesGeometry::CurvesGeometry(const CurvesGeometry &other) : CurvesGeometry()
{
  copy_curves_geometry(*this, other);
}

CurvesGeometry &CurvesGeometry::operator=(const CurvesGeometry &other)
{
  if (this != &other) {
    copy_curves_geometry(*this, other);
  }
  return *this;
}

/* The source should be empty, but in a valid state so that using it further will work. */
static void move_curves_geometry(CurvesGeometry &dst, CurvesGeometry &src)
{
  dst.point_num = src.point_num;
  std::swap(dst.point_data, src.point_data);
  CustomData_free(&src.point_data, src.point_num);
  src.point_num = 0;

  dst.curve_num = src.curve_num;
  std::swap(dst.curve_data, src.curve_data);
  CustomData_free(&src.curve_data, src.curve_num);
  src.curve_num = 0;

  std::swap(dst.curve_offsets, src.curve_offsets);

  std::swap(dst.runtime, src.runtime);
}

CurvesGeometry::CurvesGeometry(CurvesGeometry &&other) : CurvesGeometry()
{
  move_curves_geometry(*this, other);
}

CurvesGeometry &CurvesGeometry::operator=(CurvesGeometry &&other)
{
  if (this != &other) {
    move_curves_geometry(*this, other);
  }
  return *this;
}

CurvesGeometry::~CurvesGeometry()
{
  CustomData_free(&this->point_data, this->point_num);
  CustomData_free(&this->curve_data, this->curve_num);
  implicit_sharing::free_shared_data(&this->curve_offsets,
                                     &this->runtime->curve_offsets_sharing_info);
  MEM_delete(this->runtime);
  this->runtime = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Accessors
 * \{ */

static int domain_num(const CurvesGeometry &curves, const eAttrDomain domain)
{
  return domain == ATTR_DOMAIN_POINT ? curves.points_num() : curves.curves_num();
}

static CustomData &domain_custom_data(CurvesGeometry &curves, const eAttrDomain domain)
{
  return domain == ATTR_DOMAIN_POINT ? curves.point_data : curves.curve_data;
}

static const CustomData &domain_custom_data(const CurvesGeometry &curves, const eAttrDomain domain)
{
  return domain == ATTR_DOMAIN_POINT ? curves.point_data : curves.curve_data;
}

template<typename T>
static VArray<T> get_varray_attribute(const CurvesGeometry &curves,
                                      const eAttrDomain domain,
                                      const StringRefNull name,
                                      const T default_value)
{
  const int num = domain_num(curves, domain);
  const eCustomDataType type = cpp_type_to_custom_data_type(CPPType::get<T>());
  const CustomData &custom_data = domain_custom_data(curves, domain);

  const T *data = (const T *)CustomData_get_layer_named(&custom_data, type, name.c_str());
  if (data != nullptr) {
    return VArray<T>::ForSpan(Span<T>(data, num));
  }
  return VArray<T>::ForSingle(default_value, num);
}

template<typename T>
static Span<T> get_span_attribute(const CurvesGeometry &curves,
                                  const eAttrDomain domain,
                                  const StringRefNull name)
{
  const int num = domain_num(curves, domain);
  const CustomData &custom_data = domain_custom_data(curves, domain);
  const eCustomDataType type = cpp_type_to_custom_data_type(CPPType::get<T>());

  T *data = (T *)CustomData_get_layer_named(&custom_data, type, name.c_str());
  if (data == nullptr) {
    return {};
  }
  return {data, num};
}

template<typename T>
static MutableSpan<T> get_mutable_attribute(CurvesGeometry &curves,
                                            const eAttrDomain domain,
                                            const StringRefNull name,
                                            const T default_value = T())
{
  const int num = domain_num(curves, domain);
  const eCustomDataType type = cpp_type_to_custom_data_type(CPPType::get<T>());
  CustomData &custom_data = domain_custom_data(curves, domain);

  T *data = (T *)CustomData_get_layer_named_for_write(&custom_data, type, name.c_str(), num);
  if (data != nullptr) {
    return {data, num};
  }
  data = (T *)CustomData_add_layer_named(&custom_data, type, CD_SET_DEFAULT, num, name.c_str());
  MutableSpan<T> span = {data, num};
  if (num > 0 && span.first() != default_value) {
    span.fill(default_value);
  }
  return span;
}

VArray<int8_t> CurvesGeometry::curve_types() const
{
  return get_varray_attribute<int8_t>(
      *this, ATTR_DOMAIN_CURVE, ATTR_CURVE_TYPE, CURVE_TYPE_CATMULL_ROM);
}

MutableSpan<int8_t> CurvesGeometry::curve_types_for_write()
{
  return get_mutable_attribute<int8_t>(*this, ATTR_DOMAIN_CURVE, ATTR_CURVE_TYPE);
}

void CurvesGeometry::fill_curve_types(const CurveType type)
{
  if (type == CURVE_TYPE_CATMULL_ROM) {
    /* Avoid creating the attribute for Catmull Rom which is the default when the attribute doesn't
     * exist anyway. */
    this->attributes_for_write().remove("curve_type");
  }
  else {
    this->curve_types_for_write().fill(type);
  }
  this->runtime->type_counts.fill(0);
  this->runtime->type_counts[type] = this->curves_num();
  this->tag_topology_changed();
}

void CurvesGeometry::fill_curve_types(const IndexMask &selection, const CurveType type)
{
  if (selection.size() == this->curves_num()) {
    this->fill_curve_types(type);
    return;
  }
  if (std::optional<int8_t> single_type = this->curve_types().get_if_single()) {
    if (single_type == type) {
      this->fill_curve_types(type);
      return;
    }
  }
  /* A potential performance optimization is only counting the changed indices. */
  index_mask::masked_fill<int8_t>(this->curve_types_for_write(), type, selection);
  this->update_curve_types();
  this->tag_topology_changed();
}

std::array<int, CURVE_TYPES_NUM> calculate_type_counts(const VArray<int8_t> &types)
{
  using CountsType = std::array<int, CURVE_TYPES_NUM>;
  CountsType counts;
  counts.fill(0);

  if (types.is_single()) {
    counts[types.get_internal_single()] = types.size();
    return counts;
  }

  Span<int8_t> types_span = types.get_internal_span();
  return threading::parallel_reduce(
      types.index_range(),
      2048,
      counts,
      [&](const IndexRange curves_range, const CountsType &init) {
        CountsType result = init;
        for (const int curve_index : curves_range) {
          result[types_span[curve_index]]++;
        }
        return result;
      },
      [](const CountsType &a, const CountsType &b) {
        CountsType result = a;
        for (const int i : IndexRange(CURVE_TYPES_NUM)) {
          result[i] += b[i];
        }
        return result;
      });
}

void CurvesGeometry::update_curve_types()
{
  this->runtime->type_counts = calculate_type_counts(this->curve_types());
}

Span<float3> CurvesGeometry::positions() const
{
  return get_span_attribute<float3>(*this, ATTR_DOMAIN_POINT, ATTR_POSITION);
}
MutableSpan<float3> CurvesGeometry::positions_for_write()
{
  return get_mutable_attribute<float3>(*this, ATTR_DOMAIN_POINT, ATTR_POSITION);
}

Span<int> CurvesGeometry::offsets() const
{
  return {this->curve_offsets, this->curve_num + 1};
}
MutableSpan<int> CurvesGeometry::offsets_for_write()
{
  if (this->curve_num == 0) {
    return {};
  }
  implicit_sharing::make_trivial_data_mutable(
      &this->curve_offsets, &this->runtime->curve_offsets_sharing_info, this->curve_num + 1);
  return {this->curve_offsets, this->curve_num + 1};
}

VArray<bool> CurvesGeometry::cyclic() const
{
  return get_varray_attribute<bool>(*this, ATTR_DOMAIN_CURVE, ATTR_CYCLIC, false);
}
MutableSpan<bool> CurvesGeometry::cyclic_for_write()
{
  return get_mutable_attribute<bool>(*this, ATTR_DOMAIN_CURVE, ATTR_CYCLIC, false);
}

VArray<int> CurvesGeometry::resolution() const
{
  return get_varray_attribute<int>(*this, ATTR_DOMAIN_CURVE, ATTR_RESOLUTION, 12);
}
MutableSpan<int> CurvesGeometry::resolution_for_write()
{
  return get_mutable_attribute<int>(*this, ATTR_DOMAIN_CURVE, ATTR_RESOLUTION, 12);
}

VArray<int8_t> CurvesGeometry::normal_mode() const
{
  return get_varray_attribute<int8_t>(*this, ATTR_DOMAIN_CURVE, ATTR_NORMAL_MODE, 0);
}
MutableSpan<int8_t> CurvesGeometry::normal_mode_for_write()
{
  return get_mutable_attribute<int8_t>(*this, ATTR_DOMAIN_CURVE, ATTR_NORMAL_MODE);
}

VArray<float> CurvesGeometry::tilt() const
{
  return get_varray_attribute<float>(*this, ATTR_DOMAIN_POINT, ATTR_TILT, 0.0f);
}
MutableSpan<float> CurvesGeometry::tilt_for_write()
{
  return get_mutable_attribute<float>(*this, ATTR_DOMAIN_POINT, ATTR_TILT);
}

VArray<int8_t> CurvesGeometry::handle_types_left() const
{
  return get_varray_attribute<int8_t>(*this, ATTR_DOMAIN_POINT, ATTR_HANDLE_TYPE_LEFT, 0);
}
MutableSpan<int8_t> CurvesGeometry::handle_types_left_for_write()
{
  return get_mutable_attribute<int8_t>(*this, ATTR_DOMAIN_POINT, ATTR_HANDLE_TYPE_LEFT, 0);
}

VArray<int8_t> CurvesGeometry::handle_types_right() const
{
  return get_varray_attribute<int8_t>(*this, ATTR_DOMAIN_POINT, ATTR_HANDLE_TYPE_RIGHT, 0);
}
MutableSpan<int8_t> CurvesGeometry::handle_types_right_for_write()
{
  return get_mutable_attribute<int8_t>(*this, ATTR_DOMAIN_POINT, ATTR_HANDLE_TYPE_RIGHT, 0);
}

Span<float3> CurvesGeometry::handle_positions_left() const
{
  return get_span_attribute<float3>(*this, ATTR_DOMAIN_POINT, ATTR_HANDLE_POSITION_LEFT);
}
MutableSpan<float3> CurvesGeometry::handle_positions_left_for_write()
{
  return get_mutable_attribute<float3>(*this, ATTR_DOMAIN_POINT, ATTR_HANDLE_POSITION_LEFT);
}

Span<float3> CurvesGeometry::handle_positions_right() const
{
  return get_span_attribute<float3>(*this, ATTR_DOMAIN_POINT, ATTR_HANDLE_POSITION_RIGHT);
}
MutableSpan<float3> CurvesGeometry::handle_positions_right_for_write()
{
  return get_mutable_attribute<float3>(*this, ATTR_DOMAIN_POINT, ATTR_HANDLE_POSITION_RIGHT);
}

VArray<int8_t> CurvesGeometry::nurbs_orders() const
{
  return get_varray_attribute<int8_t>(*this, ATTR_DOMAIN_CURVE, ATTR_NURBS_ORDER, 4);
}
MutableSpan<int8_t> CurvesGeometry::nurbs_orders_for_write()
{
  return get_mutable_attribute<int8_t>(*this, ATTR_DOMAIN_CURVE, ATTR_NURBS_ORDER, 4);
}

Span<float> CurvesGeometry::nurbs_weights() const
{
  return get_span_attribute<float>(*this, ATTR_DOMAIN_POINT, ATTR_NURBS_WEIGHT);
}
MutableSpan<float> CurvesGeometry::nurbs_weights_for_write()
{
  return get_mutable_attribute<float>(*this, ATTR_DOMAIN_POINT, ATTR_NURBS_WEIGHT);
}

VArray<int8_t> CurvesGeometry::nurbs_knots_modes() const
{
  return get_varray_attribute<int8_t>(*this, ATTR_DOMAIN_CURVE, ATTR_NURBS_KNOTS_MODE, 0);
}
MutableSpan<int8_t> CurvesGeometry::nurbs_knots_modes_for_write()
{
  return get_mutable_attribute<int8_t>(*this, ATTR_DOMAIN_CURVE, ATTR_NURBS_KNOTS_MODE, 0);
}

Span<float2> CurvesGeometry::surface_uv_coords() const
{
  return get_span_attribute<float2>(*this, ATTR_DOMAIN_CURVE, ATTR_SURFACE_UV_COORDINATE);
}

MutableSpan<float2> CurvesGeometry::surface_uv_coords_for_write()
{
  return get_mutable_attribute<float2>(*this, ATTR_DOMAIN_CURVE, ATTR_SURFACE_UV_COORDINATE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation
 * \{ */

template<typename CountFn> void build_offsets(MutableSpan<int> offsets, const CountFn &count_fn)
{
  int offset = 0;
  for (const int i : offsets.drop_back(1).index_range()) {
    offsets[i] = offset;
    offset += count_fn(i);
  }
  offsets.last() = offset;
}

static void calculate_evaluated_offsets(const CurvesGeometry &curves,
                                        MutableSpan<int> offsets,
                                        MutableSpan<int> all_bezier_offsets)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<int8_t> types = curves.curve_types();
  const VArray<int> resolution = curves.resolution();
  const VArray<bool> cyclic = curves.cyclic();

  VArraySpan<int8_t> handle_types_left;
  VArraySpan<int8_t> handle_types_right;
  if (curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    handle_types_left = curves.handle_types_left();
    handle_types_right = curves.handle_types_right();
  }

  const VArray<int8_t> nurbs_orders = curves.nurbs_orders();
  const VArray<int8_t> nurbs_knots_modes = curves.nurbs_knots_modes();

  build_offsets(offsets, [&](const int curve_index) -> int {
    const IndexRange points = points_by_curve[curve_index];
    switch (types[curve_index]) {
      case CURVE_TYPE_CATMULL_ROM:
        return curves::catmull_rom::calculate_evaluated_num(
            points.size(), cyclic[curve_index], resolution[curve_index]);
      case CURVE_TYPE_POLY:
        return points.size();
      case CURVE_TYPE_BEZIER: {
        const IndexRange offsets = curves::per_curve_point_offsets_range(points, curve_index);
        curves::bezier::calculate_evaluated_offsets(handle_types_left.slice(points),
                                                    handle_types_right.slice(points),
                                                    cyclic[curve_index],
                                                    resolution[curve_index],
                                                    all_bezier_offsets.slice(offsets));
        return all_bezier_offsets[offsets.last()];
      }
      case CURVE_TYPE_NURBS:
        return curves::nurbs::calculate_evaluated_num(points.size(),
                                                      nurbs_orders[curve_index],
                                                      cyclic[curve_index],
                                                      resolution[curve_index],
                                                      KnotsMode(nurbs_knots_modes[curve_index]));
    }
    BLI_assert_unreachable();
    return 0;
  });
}

OffsetIndices<int> CurvesGeometry::evaluated_points_by_curve() const
{
  const bke::CurvesGeometryRuntime &runtime = *this->runtime;
  if (this->is_single_type(CURVE_TYPE_POLY)) {
    /* When all the curves are poly curves, the evaluated offsets are the same as the control
     * point offsets, so it's possible to completely avoid building a new offsets array. */
    runtime.evaluated_offsets_cache.ensure([&](CurvesGeometryRuntime::EvaluatedOffsets &r_data) {
      r_data.evaluated_offsets.clear_and_shrink();
      r_data.all_bezier_offsets.clear_and_shrink();
    });
    return this->points_by_curve();
  }

  runtime.evaluated_offsets_cache.ensure([&](CurvesGeometryRuntime::EvaluatedOffsets &r_data) {
    r_data.evaluated_offsets.resize(this->curves_num() + 1);

    if (this->has_curve_with_type(CURVE_TYPE_BEZIER)) {
      r_data.all_bezier_offsets.resize(this->points_num() + this->curves_num());
    }
    else {
      r_data.all_bezier_offsets.clear_and_shrink();
    }

    calculate_evaluated_offsets(*this, r_data.evaluated_offsets, r_data.all_bezier_offsets);
  });

  return OffsetIndices<int>(runtime.evaluated_offsets_cache.data().evaluated_offsets);
}

IndexMask CurvesGeometry::indices_for_curve_type(const CurveType type,
                                                 IndexMaskMemory &memory) const
{
  return this->indices_for_curve_type(type, this->curves_range(), memory);
}

IndexMask CurvesGeometry::indices_for_curve_type(const CurveType type,
                                                 const IndexMask &selection,
                                                 IndexMaskMemory &memory) const
{
  return curves::indices_for_type(
      this->curve_types(), this->curve_type_counts(), type, selection, memory);
}

Array<int> CurvesGeometry::point_to_curve_map() const
{
  Array<int> map(this->points_num());
  offset_indices::build_reverse_map(this->points_by_curve(), map);
  return map;
}

void CurvesGeometry::ensure_nurbs_basis_cache() const
{
  const bke::CurvesGeometryRuntime &runtime = *this->runtime;
  runtime.nurbs_basis_cache.ensure([&](Vector<curves::nurbs::BasisCache> &r_data) {
    IndexMaskMemory memory;
    const IndexMask nurbs_mask = this->indices_for_curve_type(CURVE_TYPE_NURBS, memory);
    if (nurbs_mask.is_empty()) {
      r_data.clear_and_shrink();
      return;
    }

    r_data.resize(this->curves_num());

    const OffsetIndices<int> points_by_curve = this->points_by_curve();
    const OffsetIndices<int> evaluated_points_by_curve = this->evaluated_points_by_curve();
    const VArray<bool> cyclic = this->cyclic();
    const VArray<int8_t> orders = this->nurbs_orders();
    const VArray<int8_t> knots_modes = this->nurbs_knots_modes();

    nurbs_mask.foreach_segment(GrainSize(64), [&](const IndexMaskSegment segment) {
      Vector<float, 32> knots;
      for (const int curve_index : segment) {
        const IndexRange points = points_by_curve[curve_index];
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];

        const int8_t order = orders[curve_index];
        const bool is_cyclic = cyclic[curve_index];
        const KnotsMode mode = KnotsMode(knots_modes[curve_index]);

        if (!curves::nurbs::check_valid_num_and_order(points.size(), order, is_cyclic, mode)) {
          r_data[curve_index].invalid = true;
          continue;
        }

        knots.reinitialize(curves::nurbs::knots_num(points.size(), order, is_cyclic));
        curves::nurbs::calculate_knots(points.size(), mode, order, is_cyclic, knots);
        curves::nurbs::calculate_basis_cache(
            points.size(), evaluated_points.size(), order, is_cyclic, knots, r_data[curve_index]);
      }
    });
  });
}

Span<float3> CurvesGeometry::evaluated_positions() const
{
  const bke::CurvesGeometryRuntime &runtime = *this->runtime;
  if (this->is_single_type(CURVE_TYPE_POLY)) {
    runtime.evaluated_position_cache.ensure(
        [&](Vector<float3> &r_data) { r_data.clear_and_shrink(); });
    return this->positions();
  }
  this->ensure_nurbs_basis_cache();
  runtime.evaluated_position_cache.ensure([&](Vector<float3> &r_data) {
    r_data.resize(this->evaluated_points_num());
    MutableSpan<float3> evaluated_positions = r_data;

    const OffsetIndices<int> points_by_curve = this->points_by_curve();
    const OffsetIndices<int> evaluated_points_by_curve = this->evaluated_points_by_curve();
    const Span<float3> positions = this->positions();

    auto evaluate_catmull = [&](const IndexMask &selection) {
      const VArray<bool> cyclic = this->cyclic();
      const VArray<int> resolution = this->resolution();
      selection.foreach_index(GrainSize(128), [&](const int curve_index) {
        const IndexRange points = points_by_curve[curve_index];
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];
        curves::catmull_rom::interpolate_to_evaluated(positions.slice(points),
                                                      cyclic[curve_index],
                                                      resolution[curve_index],
                                                      evaluated_positions.slice(evaluated_points));
      });
    };
    auto evaluate_poly = [&](const IndexMask &selection) {
      curves::copy_point_data(
          points_by_curve, evaluated_points_by_curve, selection, positions, evaluated_positions);
    };
    auto evaluate_bezier = [&](const IndexMask &selection) {
      const Span<float3> handle_positions_left = this->handle_positions_left();
      const Span<float3> handle_positions_right = this->handle_positions_right();
      if (handle_positions_left.is_empty() || handle_positions_right.is_empty()) {
        curves::fill_points(evaluated_points_by_curve, selection, float3(0), evaluated_positions);
        return;
      }
      const Span<int> all_bezier_offsets =
          runtime.evaluated_offsets_cache.data().all_bezier_offsets;
      selection.foreach_index(GrainSize(128), [&](const int curve_index) {
        const IndexRange points = points_by_curve[curve_index];
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];
        const IndexRange offsets = curves::per_curve_point_offsets_range(points, curve_index);
        curves::bezier::calculate_evaluated_positions(positions.slice(points),
                                                      handle_positions_left.slice(points),
                                                      handle_positions_right.slice(points),
                                                      all_bezier_offsets.slice(offsets),
                                                      evaluated_positions.slice(evaluated_points));
      });
    };
    auto evaluate_nurbs = [&](const IndexMask &selection) {
      this->ensure_nurbs_basis_cache();
      const VArray<int8_t> nurbs_orders = this->nurbs_orders();
      const Span<float> nurbs_weights = this->nurbs_weights();
      const Span<curves::nurbs::BasisCache> nurbs_basis_cache = runtime.nurbs_basis_cache.data();
      selection.foreach_index(GrainSize(128), [&](const int curve_index) {
        const IndexRange points = points_by_curve[curve_index];
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];
        curves::nurbs::interpolate_to_evaluated(nurbs_basis_cache[curve_index],
                                                nurbs_orders[curve_index],
                                                nurbs_weights.slice_safe(points),
                                                positions.slice(points),
                                                evaluated_positions.slice(evaluated_points));
      });
    };
    curves::foreach_curve_by_type(this->curve_types(),
                                  this->curve_type_counts(),
                                  this->curves_range(),
                                  evaluate_catmull,
                                  evaluate_poly,
                                  evaluate_bezier,
                                  evaluate_nurbs);
  });
  return runtime.evaluated_position_cache.data();
}

Span<float3> CurvesGeometry::evaluated_tangents() const
{
  const bke::CurvesGeometryRuntime &runtime = *this->runtime;
  runtime.evaluated_tangent_cache.ensure([&](Vector<float3> &r_data) {
    const OffsetIndices<int> evaluated_points_by_curve = this->evaluated_points_by_curve();
    const Span<float3> evaluated_positions = this->evaluated_positions();
    const VArray<bool> cyclic = this->cyclic();

    r_data.resize(this->evaluated_points_num());
    MutableSpan<float3> tangents = r_data;

    threading::parallel_for(this->curves_range(), 128, [&](IndexRange curves_range) {
      for (const int curve_index : curves_range) {
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];
        curves::poly::calculate_tangents(evaluated_positions.slice(evaluated_points),
                                         cyclic[curve_index],
                                         tangents.slice(evaluated_points));
      }
    });

    /* Correct the first and last tangents of non-cyclic Bezier curves so that they align with
     * the inner handles. This is a separate loop to avoid the cost when Bezier type curves are
     * not used. */
    IndexMaskMemory memory;
    const IndexMask bezier_mask = this->indices_for_curve_type(CURVE_TYPE_BEZIER, memory);
    if (!bezier_mask.is_empty()) {
      const OffsetIndices<int> points_by_curve = this->points_by_curve();
      const Span<float3> positions = this->positions();
      const Span<float3> handles_left = this->handle_positions_left();
      const Span<float3> handles_right = this->handle_positions_right();

      bezier_mask.foreach_index(GrainSize(1024), [&](const int curve_index) {
        if (cyclic[curve_index]) {
          return;
        }
        const IndexRange points = points_by_curve[curve_index];
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];

        const float epsilon = 1e-6f;
        if (!math::almost_equal_relative(
                handles_right[points.first()], positions[points.first()], epsilon))
        {
          tangents[evaluated_points.first()] = math::normalize(handles_right[points.first()] -
                                                               positions[points.first()]);
        }
        if (!math::almost_equal_relative(
                handles_left[points.last()], positions[points.last()], epsilon)) {
          tangents[evaluated_points.last()] = math::normalize(positions[points.last()] -
                                                              handles_left[points.last()]);
        }
      });
    }
  });
  return runtime.evaluated_tangent_cache.data();
}

static void rotate_directions_around_axes(MutableSpan<float3> directions,
                                          const Span<float3> axes,
                                          const Span<float> angles)
{
  for (const int i : directions.index_range()) {
    directions[i] = math::rotate_direction_around_axis(directions[i], axes[i], angles[i]);
  }
}

static void evaluate_generic_data_for_curve(
    const int curve_index,
    const IndexRange points,
    const VArray<int8_t> &types,
    const VArray<bool> &cyclic,
    const VArray<int> &resolution,
    const Span<int> all_bezier_evaluated_offsets,
    const Span<curves::nurbs::BasisCache> nurbs_basis_cache,
    const VArray<int8_t> &nurbs_orders,
    const Span<float> nurbs_weights,
    const GSpan src,
    GMutableSpan dst)
{
  switch (types[curve_index]) {
    case CURVE_TYPE_CATMULL_ROM:
      curves::catmull_rom::interpolate_to_evaluated(
          src, cyclic[curve_index], resolution[curve_index], dst);
      break;
    case CURVE_TYPE_POLY:
      dst.copy_from(src);
      break;
    case CURVE_TYPE_BEZIER: {
      const IndexRange offsets = curves::per_curve_point_offsets_range(points, curve_index);
      curves::bezier::interpolate_to_evaluated(
          src, all_bezier_evaluated_offsets.slice(offsets), dst);
      break;
    }
    case CURVE_TYPE_NURBS:
      curves::nurbs::interpolate_to_evaluated(nurbs_basis_cache[curve_index],
                                              nurbs_orders[curve_index],
                                              nurbs_weights.slice_safe(points),
                                              src,
                                              dst);
      break;
  }
}

Span<float3> CurvesGeometry::evaluated_normals() const
{
  const bke::CurvesGeometryRuntime &runtime = *this->runtime;
  this->ensure_nurbs_basis_cache();
  runtime.evaluated_normal_cache.ensure([&](Vector<float3> &r_data) {
    const OffsetIndices<int> points_by_curve = this->points_by_curve();
    const OffsetIndices<int> evaluated_points_by_curve = this->evaluated_points_by_curve();
    const VArray<int8_t> types = this->curve_types();
    const VArray<bool> cyclic = this->cyclic();
    const VArray<int8_t> normal_mode = this->normal_mode();
    const VArray<int> resolution = this->resolution();
    const VArray<int8_t> nurbs_orders = this->nurbs_orders();
    const Span<float> nurbs_weights = this->nurbs_weights();
    const Span<int> all_bezier_offsets = runtime.evaluated_offsets_cache.data().all_bezier_offsets;
    const Span<curves::nurbs::BasisCache> nurbs_basis_cache = runtime.nurbs_basis_cache.data();

    const Span<float3> evaluated_tangents = this->evaluated_tangents();
    const VArray<float> tilt = this->tilt();
    VArraySpan<float> tilt_span;
    const bool use_tilt = !(tilt.is_single() && tilt.get_internal_single() == 0.0f);
    if (use_tilt) {
      tilt_span = tilt;
    }

    r_data.resize(this->evaluated_points_num());
    MutableSpan<float3> evaluated_normals = r_data;

    threading::parallel_for(this->curves_range(), 128, [&](IndexRange curves_range) {
      /* Reuse a buffer for the evaluated tilts. */
      Vector<float> evaluated_tilts;

      for (const int curve_index : curves_range) {
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];
        switch (normal_mode[curve_index]) {
          case NORMAL_MODE_Z_UP:
            curves::poly::calculate_normals_z_up(evaluated_tangents.slice(evaluated_points),
                                                 evaluated_normals.slice(evaluated_points));
            break;
          case NORMAL_MODE_MINIMUM_TWIST:
            curves::poly::calculate_normals_minimum(evaluated_tangents.slice(evaluated_points),
                                                    cyclic[curve_index],
                                                    evaluated_normals.slice(evaluated_points));
            break;
        }

        /* If the "tilt" attribute exists, rotate the normals around the tangents by the
         * evaluated angles. We can avoid copying the tilts to evaluate them for poly curves. */
        if (use_tilt) {
          const IndexRange points = points_by_curve[curve_index];
          if (types[curve_index] == CURVE_TYPE_POLY) {
            rotate_directions_around_axes(evaluated_normals.slice(evaluated_points),
                                          evaluated_tangents.slice(evaluated_points),
                                          tilt_span.slice(points));
          }
          else {
            evaluated_tilts.reinitialize(evaluated_points.size());
            evaluate_generic_data_for_curve(curve_index,
                                            points,
                                            types,
                                            cyclic,
                                            resolution,
                                            all_bezier_offsets,
                                            nurbs_basis_cache,
                                            nurbs_orders,
                                            nurbs_weights,
                                            tilt_span.slice(points),
                                            evaluated_tilts.as_mutable_span());
            rotate_directions_around_axes(evaluated_normals.slice(evaluated_points),
                                          evaluated_tangents.slice(evaluated_points),
                                          evaluated_tilts.as_span());
          }
        }
      }
    });
  });
  return this->runtime->evaluated_normal_cache.data();
}

void CurvesGeometry::interpolate_to_evaluated(const int curve_index,
                                              const GSpan src,
                                              GMutableSpan dst) const
{
  const bke::CurvesGeometryRuntime &runtime = *this->runtime;
  const OffsetIndices points_by_curve = this->points_by_curve();
  const IndexRange points = points_by_curve[curve_index];
  BLI_assert(src.size() == points.size());
  BLI_assert(dst.size() == this->evaluated_points_by_curve()[curve_index].size());
  evaluate_generic_data_for_curve(curve_index,
                                  points,
                                  this->curve_types(),
                                  this->cyclic(),
                                  this->resolution(),
                                  runtime.evaluated_offsets_cache.data().all_bezier_offsets,
                                  runtime.nurbs_basis_cache.data(),
                                  this->nurbs_orders(),
                                  this->nurbs_weights(),
                                  src,
                                  dst);
}

void CurvesGeometry::interpolate_to_evaluated(const GSpan src, GMutableSpan dst) const
{
  const bke::CurvesGeometryRuntime &runtime = *this->runtime;
  const OffsetIndices points_by_curve = this->points_by_curve();
  const OffsetIndices evaluated_points_by_curve = this->evaluated_points_by_curve();
  const VArray<int8_t> types = this->curve_types();
  const VArray<int> resolution = this->resolution();
  const VArray<bool> cyclic = this->cyclic();
  const VArray<int8_t> nurbs_orders = this->nurbs_orders();
  const Span<float> nurbs_weights = this->nurbs_weights();
  const Span<int> all_bezier_offsets = runtime.evaluated_offsets_cache.data().all_bezier_offsets;
  const Span<curves::nurbs::BasisCache> nurbs_basis_cache = runtime.nurbs_basis_cache.data();

  threading::parallel_for(this->curves_range(), 512, [&](IndexRange curves_range) {
    for (const int curve_index : curves_range) {
      const IndexRange points = points_by_curve[curve_index];
      const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];
      evaluate_generic_data_for_curve(curve_index,
                                      points,
                                      types,
                                      cyclic,
                                      resolution,
                                      all_bezier_offsets,
                                      nurbs_basis_cache,
                                      nurbs_orders,
                                      nurbs_weights,
                                      src.slice(points),
                                      dst.slice(evaluated_points));
    }
  });
}

void CurvesGeometry::ensure_evaluated_lengths() const
{
  const bke::CurvesGeometryRuntime &runtime = *this->runtime;
  runtime.evaluated_length_cache.ensure([&](Vector<float> &r_data) {
    /* Use an extra length value for the final cyclic segment for a consistent size
     * (see comment on #evaluated_length_cache). */
    const int total_num = this->evaluated_points_num() + this->curves_num();
    r_data.resize(total_num);
    MutableSpan<float> evaluated_lengths = r_data;

    const OffsetIndices<int> evaluated_points_by_curve = this->evaluated_points_by_curve();
    const Span<float3> evaluated_positions = this->evaluated_positions();
    const VArray<bool> curves_cyclic = this->cyclic();

    threading::parallel_for(this->curves_range(), 128, [&](IndexRange curves_range) {
      for (const int curve_index : curves_range) {
        const bool cyclic = curves_cyclic[curve_index];
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];
        const IndexRange lengths_range = this->lengths_range_for_curve(curve_index, cyclic);
        length_parameterize::accumulate_lengths(evaluated_positions.slice(evaluated_points),
                                                cyclic,
                                                evaluated_lengths.slice(lengths_range));
      }
    });
  });
}

void CurvesGeometry::ensure_can_interpolate_to_evaluated() const
{
  this->evaluated_points_by_curve();
  this->ensure_nurbs_basis_cache();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operations
 * \{ */

void CurvesGeometry::resize(const int points_num, const int curves_num)
{
  if (points_num != this->point_num) {
    CustomData_realloc(&this->point_data, this->points_num(), points_num);
    this->point_num = points_num;
  }
  if (curves_num != this->curve_num) {
    CustomData_realloc(&this->curve_data, this->curves_num(), curves_num);
    implicit_sharing::resize_trivial_array(&this->curve_offsets,
                                           &this->runtime->curve_offsets_sharing_info,
                                           this->curve_num == 0 ? 0 : (this->curve_num + 1),
                                           curves_num + 1);
    /* Set common values for convenience. */
    this->curve_offsets[0] = 0;
    this->curve_offsets[curves_num] = this->point_num;
    this->curve_num = curves_num;
  }
  this->tag_topology_changed();
}

void CurvesGeometry::tag_positions_changed()
{
  this->runtime->evaluated_position_cache.tag_dirty();
  this->runtime->evaluated_tangent_cache.tag_dirty();
  this->runtime->evaluated_normal_cache.tag_dirty();
  this->runtime->evaluated_length_cache.tag_dirty();
  this->runtime->bounds_cache.tag_dirty();
}
void CurvesGeometry::tag_topology_changed()
{
  this->tag_positions_changed();
  this->runtime->evaluated_offsets_cache.tag_dirty();
  this->runtime->nurbs_basis_cache.tag_dirty();
}
void CurvesGeometry::tag_normals_changed()
{
  this->runtime->evaluated_normal_cache.tag_dirty();
}
void CurvesGeometry::tag_radii_changed() {}

static void translate_positions(MutableSpan<float3> positions, const float3 &translation)
{
  threading::parallel_for(positions.index_range(), 2048, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position += translation;
    }
  });
}

static void transform_positions(MutableSpan<float3> positions, const float4x4 &matrix)
{
  threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position = math::transform_point(matrix, position);
    }
  });
}

void CurvesGeometry::calculate_bezier_auto_handles()
{
  if (!this->has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return;
  }
  if (this->handle_positions_left().is_empty() || this->handle_positions_right().is_empty()) {
    return;
  }
  const OffsetIndices points_by_curve = this->points_by_curve();
  const VArray<int8_t> types = this->curve_types();
  const VArray<bool> cyclic = this->cyclic();
  const VArraySpan<int8_t> types_left{this->handle_types_left()};
  const VArraySpan<int8_t> types_right{this->handle_types_right()};
  const Span<float3> positions = this->positions();
  MutableSpan<float3> positions_left = this->handle_positions_left_for_write();
  MutableSpan<float3> positions_right = this->handle_positions_right_for_write();

  threading::parallel_for(this->curves_range(), 128, [&](IndexRange range) {
    for (const int i_curve : range) {
      if (types[i_curve] == CURVE_TYPE_BEZIER) {
        const IndexRange points = points_by_curve[i_curve];
        curves::bezier::calculate_auto_handles(cyclic[i_curve],
                                               types_left.slice(points),
                                               types_right.slice(points),
                                               positions.slice(points),
                                               positions_left.slice(points),
                                               positions_right.slice(points));
      }
    }
  });
}

void CurvesGeometry::translate(const float3 &translation)
{
  if (math::is_zero(translation)) {
    return;
  }

  std::optional<Bounds<float3>> bounds;
  if (this->runtime->bounds_cache.is_cached()) {
    bounds = this->runtime->bounds_cache.data();
  }

  translate_positions(this->positions_for_write(), translation);
  if (!this->handle_positions_left().is_empty()) {
    translate_positions(this->handle_positions_left_for_write(), translation);
  }
  if (!this->handle_positions_right().is_empty()) {
    translate_positions(this->handle_positions_right_for_write(), translation);
  }
  this->tag_positions_changed();

  if (bounds) {
    bounds->min += translation;
    bounds->max += translation;
    this->runtime->bounds_cache.ensure([&](blender::Bounds<float3> &r_data) { r_data = *bounds; });
  }
}

void CurvesGeometry::transform(const float4x4 &matrix)
{
  transform_positions(this->positions_for_write(), matrix);
  if (!this->handle_positions_left().is_empty()) {
    transform_positions(this->handle_positions_left_for_write(), matrix);
  }
  if (!this->handle_positions_right().is_empty()) {
    transform_positions(this->handle_positions_right_for_write(), matrix);
  }
  this->tag_positions_changed();
}

bool CurvesGeometry::bounds_min_max(float3 &min, float3 &max) const
{
  if (this->points_num() == 0) {
    return false;
  }

  this->runtime->bounds_cache.ensure(
      [&](Bounds<float3> &r_bounds) { r_bounds = *bounds::min_max(this->evaluated_positions()); });

  const Bounds<float3> &bounds = this->runtime->bounds_cache.data();
  min = math::min(bounds.min, min);
  max = math::max(bounds.max, max);
  return true;
}

CurvesGeometry curves_copy_point_selection(
    const CurvesGeometry &curves,
    const IndexMask &points_to_copy,
    const AnonymousAttributePropagationInfo &propagation_info)
{
  const Array<int> point_to_curve_map = curves.point_to_curve_map();
  Array<int> curve_point_counts(curves.curves_num(), 0);
  points_to_copy.foreach_index(
      [&](const int64_t point_i) { curve_point_counts[point_to_curve_map[point_i]]++; });

  IndexMaskMemory memory;
  const IndexMask curves_to_copy = IndexMask::from_predicate(
      curves.curves_range(), GrainSize(4096), memory, [&](const int64_t i) {
        return curve_point_counts[i] > 0;
      });

  CurvesGeometry dst_curves(points_to_copy.size(), curves_to_copy.size());

  threading::parallel_invoke(
      dst_curves.curves_num() > 1024,
      [&]() {
        MutableSpan<int> new_curve_offsets = dst_curves.offsets_for_write();
        array_utils::gather(
            curve_point_counts.as_span(), curves_to_copy, new_curve_offsets.drop_back(1));
        offset_indices::accumulate_counts_to_offsets(new_curve_offsets);
      },
      [&]() {
        gather_attributes(curves.attributes(),
                          ATTR_DOMAIN_POINT,
                          propagation_info,
                          {},
                          points_to_copy,
                          dst_curves.attributes_for_write());
        gather_attributes(curves.attributes(),
                          ATTR_DOMAIN_CURVE,
                          propagation_info,
                          {},
                          curves_to_copy,
                          dst_curves.attributes_for_write());
      });

  if (dst_curves.curves_num() == curves.curves_num()) {
    dst_curves.runtime->type_counts = curves.runtime->type_counts;
  }
  else {
    dst_curves.remove_attributes_based_on_types();
  }

  return dst_curves;
}

void CurvesGeometry::remove_points(const IndexMask &points_to_delete,
                                   const AnonymousAttributePropagationInfo &propagation_info)
{
  if (points_to_delete.is_empty()) {
    return;
  }
  if (points_to_delete.size() == this->points_num()) {
    *this = {};
  }
  IndexMaskMemory memory;
  const IndexMask points_to_copy = points_to_delete.complement(this->points_range(), memory);
  *this = curves_copy_point_selection(*this, points_to_copy, propagation_info);
}

CurvesGeometry curves_copy_curve_selection(
    const CurvesGeometry &curves,
    const IndexMask &curves_to_copy,
    const AnonymousAttributePropagationInfo &propagation_info)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  CurvesGeometry dst_curves(0, curves_to_copy.size());
  const OffsetIndices dst_points_by_curve = offset_indices::gather_selected_offsets(
      points_by_curve, curves_to_copy, dst_curves.offsets_for_write());
  dst_curves.resize(dst_points_by_curve.total_size(), dst_curves.curves_num());

  const AttributeAccessor src_attributes = curves.attributes();
  MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();

  gather_attributes_group_to_group(src_attributes,
                                   ATTR_DOMAIN_POINT,
                                   propagation_info,
                                   {},
                                   points_by_curve,
                                   dst_points_by_curve,
                                   curves_to_copy,
                                   dst_attributes);

  gather_attributes(
      src_attributes, ATTR_DOMAIN_CURVE, propagation_info, {}, curves_to_copy, dst_attributes);

  dst_curves.remove_attributes_based_on_types();
  dst_curves.update_curve_types();

  return dst_curves;
}

void CurvesGeometry::remove_curves(const IndexMask &curves_to_delete,
                                   const AnonymousAttributePropagationInfo &propagation_info)
{
  if (curves_to_delete.is_empty()) {
    return;
  }
  if (curves_to_delete.size() == this->curves_num()) {
    *this = {};
    return;
  }
  IndexMaskMemory memory;
  const IndexMask curves_to_copy = curves_to_delete.complement(this->curves_range(), memory);
  *this = curves_copy_curve_selection(*this, curves_to_copy, propagation_info);
}

template<typename T>
static void reverse_curve_point_data(const CurvesGeometry &curves,
                                     const IndexMask &curve_selection,
                                     MutableSpan<T> data)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  curve_selection.foreach_index(
      GrainSize(256), [&](const int curve_i) { data.slice(points_by_curve[curve_i]).reverse(); });
}

template<typename T>
static void reverse_swap_curve_point_data(const CurvesGeometry &curves,
                                          const IndexMask &curve_selection,
                                          MutableSpan<T> data_a,
                                          MutableSpan<T> data_b)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  curve_selection.foreach_index(GrainSize(256), [&](const int curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    MutableSpan<T> a = data_a.slice(points);
    MutableSpan<T> b = data_b.slice(points);
    for (const int i : IndexRange(points.size() / 2)) {
      const int end_index = points.size() - 1 - i;
      std::swap(a[end_index], b[i]);
      std::swap(b[end_index], a[i]);
    }
    if (points.size() % 2) {
      const int64_t middle_index = points.size() / 2;
      std::swap(a[middle_index], b[middle_index]);
    }
  });
}

void CurvesGeometry::reverse_curves(const IndexMask &curves_to_reverse)
{
  Set<StringRef> bezier_handle_names{{ATTR_HANDLE_POSITION_LEFT,
                                      ATTR_HANDLE_POSITION_RIGHT,
                                      ATTR_HANDLE_TYPE_LEFT,
                                      ATTR_HANDLE_TYPE_RIGHT}};

  MutableAttributeAccessor attributes = this->attributes_for_write();

  attributes.for_all([&](const AttributeIDRef &id, AttributeMetaData meta_data) {
    if (meta_data.domain != ATTR_DOMAIN_POINT) {
      return true;
    }
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (bezier_handle_names.contains(id.name())) {
      return true;
    }

    GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);
    attribute_math::convert_to_static_type(attribute.span.type(), [&](auto dummy) {
      using T = decltype(dummy);
      reverse_curve_point_data<T>(*this, curves_to_reverse, attribute.span.typed<T>());
    });
    attribute.finish();
    return true;
  });

  /* In order to maintain the shape of Bezier curves, handle attributes must reverse, but also the
   * values for the left and right must swap. Use a utility to swap and reverse at the same time,
   * to avoid loading the attribute twice. Generally we can expect the right layer to exist when
   * the left does, but there's no need to count on it, so check for both attributes. */

  if (attributes.contains(ATTR_HANDLE_POSITION_LEFT) &&
      attributes.contains(ATTR_HANDLE_POSITION_RIGHT))
  {
    reverse_swap_curve_point_data(*this,
                                  curves_to_reverse,
                                  this->handle_positions_left_for_write(),
                                  this->handle_positions_right_for_write());
  }
  if (attributes.contains(ATTR_HANDLE_TYPE_LEFT) && attributes.contains(ATTR_HANDLE_TYPE_RIGHT)) {
    reverse_swap_curve_point_data(*this,
                                  curves_to_reverse,
                                  this->handle_types_left_for_write(),
                                  this->handle_types_right_for_write());
  }

  this->tag_topology_changed();
}

void CurvesGeometry::remove_attributes_based_on_types()
{
  MutableAttributeAccessor attributes = this->attributes_for_write();
  if (!this->has_curve_with_type(CURVE_TYPE_BEZIER)) {
    attributes.remove(ATTR_HANDLE_TYPE_LEFT);
    attributes.remove(ATTR_HANDLE_TYPE_RIGHT);
    attributes.remove(ATTR_HANDLE_POSITION_LEFT);
    attributes.remove(ATTR_HANDLE_POSITION_RIGHT);
  }
  if (!this->has_curve_with_type(CURVE_TYPE_NURBS)) {
    attributes.remove(ATTR_NURBS_WEIGHT);
    attributes.remove(ATTR_NURBS_ORDER);
    attributes.remove(ATTR_NURBS_KNOTS_MODE);
  }
  if (!this->has_curve_with_type({CURVE_TYPE_BEZIER, CURVE_TYPE_CATMULL_ROM, CURVE_TYPE_NURBS})) {
    attributes.remove(ATTR_RESOLUTION);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Domain Interpolation
 * \{ */

/**
 * Mix together all of a curve's control point values.
 *
 * \note Theoretically this interpolation does not need to compute all values at once.
 * However, doing that makes the implementation simpler, and this can be optimized in the future if
 * only some values are required.
 */
template<typename T>
static void adapt_curve_domain_point_to_curve_impl(const CurvesGeometry &curves,
                                                   const VArray<T> &old_values,
                                                   MutableSpan<T> r_values)
{
  attribute_math::DefaultMixer<T> mixer(r_values);

  const OffsetIndices points_by_curve = curves.points_by_curve();
  threading::parallel_for(curves.curves_range(), 128, [&](const IndexRange range) {
    for (const int i_curve : range) {
      for (const int i_point : points_by_curve[i_curve]) {
        mixer.mix_in(i_curve, old_values[i_point]);
      }
    }
    mixer.finalize(range);
  });
}

/**
 * A curve is selected if all of its control points were selected.
 *
 * \note Theoretically this interpolation does not need to compute all values at once.
 * However, doing that makes the implementation simpler, and this can be optimized in the future if
 * only some values are required.
 */
template<>
void adapt_curve_domain_point_to_curve_impl(const CurvesGeometry &curves,
                                            const VArray<bool> &old_values,
                                            MutableSpan<bool> r_values)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  r_values.fill(true);
  for (const int i_curve : IndexRange(curves.curves_num())) {
    for (const int i_point : points_by_curve[i_curve]) {
      if (!old_values[i_point]) {
        r_values[i_curve] = false;
        break;
      }
    }
  }
}

static GVArray adapt_curve_domain_point_to_curve(const CurvesGeometry &curves,
                                                 const GVArray &varray)
{
  GVArray new_varray;
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      Array<T> values(curves.curves_num());
      adapt_curve_domain_point_to_curve_impl<T>(curves, varray.typed<T>(), values);
      new_varray = VArray<T>::ForContainer(std::move(values));
    }
  });
  return new_varray;
}

/**
 * Copy the value from a curve to all of its points.
 *
 * \note Theoretically this interpolation does not need to compute all values at once.
 * However, doing that makes the implementation simpler, and this can be optimized in the future if
 * only some values are required.
 */
template<typename T>
static void adapt_curve_domain_curve_to_point_impl(const CurvesGeometry &curves,
                                                   const VArray<T> &old_values,
                                                   MutableSpan<T> r_values)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  for (const int i_curve : IndexRange(curves.curves_num())) {
    r_values.slice(points_by_curve[i_curve]).fill(old_values[i_curve]);
  }
}

static GVArray adapt_curve_domain_curve_to_point(const CurvesGeometry &curves,
                                                 const GVArray &varray)
{
  GVArray new_varray;
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    Array<T> values(curves.points_num());
    adapt_curve_domain_curve_to_point_impl<T>(curves, varray.typed<T>(), values);
    new_varray = VArray<T>::ForContainer(std::move(values));
  });
  return new_varray;
}

GVArray CurvesGeometry::adapt_domain(const GVArray &varray,
                                     const eAttrDomain from,
                                     const eAttrDomain to) const
{
  if (!varray) {
    return {};
  }
  if (varray.is_empty()) {
    return {};
  }
  if (from == to) {
    return varray;
  }
  if (varray.is_single()) {
    BUFFER_FOR_CPP_TYPE_VALUE(varray.type(), value);
    varray.get_internal_single(value);
    return GVArray::ForSingle(varray.type(), this->attributes().domain_size(to), value);
  }

  if (from == ATTR_DOMAIN_POINT && to == ATTR_DOMAIN_CURVE) {
    return adapt_curve_domain_point_to_curve(*this, varray);
  }
  if (from == ATTR_DOMAIN_CURVE && to == ATTR_DOMAIN_POINT) {
    return adapt_curve_domain_curve_to_point(*this, varray);
  }

  BLI_assert_unreachable();
  return {};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File reading/writing.
 * \{ */

void CurvesGeometry::blend_read(BlendDataReader &reader)
{
  this->runtime = MEM_new<blender::bke::CurvesGeometryRuntime>(__func__);

  CustomData_blend_read(&reader, &this->point_data, this->point_num);
  CustomData_blend_read(&reader, &this->curve_data, this->curve_num);

  if (this->curve_offsets) {
    BLO_read_int32_array(&reader, this->curve_num + 1, &this->curve_offsets);
    this->runtime->curve_offsets_sharing_info = implicit_sharing::info_for_mem_free(
        this->curve_offsets);
  }

  /* Recalculate curve type count cache that isn't saved in files. */
  this->update_curve_types();
}

void CurvesGeometry::blend_write(BlendWriter &writer, ID &id)
{
  Vector<CustomDataLayer, 16> point_layers;
  Vector<CustomDataLayer, 16> curve_layers;
  CustomData_blend_write_prepare(this->point_data, point_layers);
  CustomData_blend_write_prepare(this->curve_data, curve_layers);

  CustomData_blend_write(
      &writer, &this->point_data, point_layers, this->point_num, CD_MASK_ALL, &id);
  CustomData_blend_write(
      &writer, &this->curve_data, curve_layers, this->curve_num, CD_MASK_ALL, &id);

  BLO_write_int32_array(&writer, this->curve_num + 1, this->curve_offsets);
}

/** \} */

}  // namespace blender::bke
