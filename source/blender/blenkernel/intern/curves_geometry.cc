/* SPDX-FileCopyrightText: 2023 Blender Authors
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

#include "BLO_read_write.hh"

#include "DNA_curves_types.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_bake_data_block_id.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"

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
  this->runtime = MEM_new<CurvesGeometryRuntime>(__func__);

  this->point_num = point_num;
  this->curve_num = curve_num;
  CustomData_reset(&this->point_data);
  CustomData_reset(&this->curve_data);
  BLI_listbase_clear(&this->vertex_group_names);

  this->attributes_for_write().add<float3>(
      "position", AttrDomain::Point, AttributeInitConstruct());

  if (curve_num > 0) {
    this->curve_offsets = static_cast<int *>(
        MEM_malloc_arrayN(this->curve_num + 1, sizeof(int), __func__));
    this->runtime->curve_offsets_sharing_info = implicit_sharing::info_for_mem_free(
        this->curve_offsets);
#ifndef NDEBUG
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

CurvesGeometry::CurvesGeometry(const CurvesGeometry &other)
{
  this->curve_offsets = other.curve_offsets;
  if (other.runtime->curve_offsets_sharing_info) {
    other.runtime->curve_offsets_sharing_info->add_user();
  }

  CustomData_copy(&other.point_data, &this->point_data, CD_MASK_ALL, other.point_num);
  CustomData_copy(&other.curve_data, &this->curve_data, CD_MASK_ALL, other.curve_num);

  this->point_num = other.point_num;
  this->curve_num = other.curve_num;

  BKE_defgroup_copy_list(&this->vertex_group_names, &other.vertex_group_names);
  this->vertex_group_active_index = other.vertex_group_active_index;

  this->runtime = MEM_new<CurvesGeometryRuntime>(
      __func__,
      CurvesGeometryRuntime{other.runtime->curve_offsets_sharing_info,
                            other.runtime->type_counts,
                            other.runtime->evaluated_offsets_cache,
                            other.runtime->nurbs_basis_cache,
                            other.runtime->evaluated_position_cache,
                            other.runtime->bounds_cache,
                            other.runtime->evaluated_length_cache,
                            other.runtime->evaluated_tangent_cache,
                            other.runtime->evaluated_normal_cache,
                            {}});

  if (other.runtime->bake_materials) {
    this->runtime->bake_materials = std::make_unique<bake::BakeMaterialsList>(
        *other.runtime->bake_materials);
  }
}

CurvesGeometry &CurvesGeometry::operator=(const CurvesGeometry &other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) CurvesGeometry(other);
  return *this;
}

CurvesGeometry::CurvesGeometry(CurvesGeometry &&other)
{
  this->curve_offsets = other.curve_offsets;
  other.curve_offsets = nullptr;

  this->point_data = other.point_data;
  CustomData_reset(&other.point_data);

  this->curve_data = other.curve_data;
  CustomData_reset(&other.curve_data);

  this->point_num = other.point_num;
  other.point_num = 0;

  this->curve_num = other.curve_num;
  other.curve_num = 0;

  this->vertex_group_names = other.vertex_group_names;
  BLI_listbase_clear(&other.vertex_group_names);

  this->vertex_group_active_index = other.vertex_group_active_index;
  other.vertex_group_active_index = 0;

  this->runtime = other.runtime;
  other.runtime = nullptr;
}

CurvesGeometry &CurvesGeometry::operator=(CurvesGeometry &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) CurvesGeometry(std::move(other));
  return *this;
}

CurvesGeometry::~CurvesGeometry()
{
  CustomData_free(&this->point_data, this->point_num);
  CustomData_free(&this->curve_data, this->curve_num);
  BLI_freelistN(&this->vertex_group_names);
  if (this->runtime) {
    implicit_sharing::free_shared_data(&this->curve_offsets,
                                       &this->runtime->curve_offsets_sharing_info);
    MEM_delete(this->runtime);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Accessors
 * \{ */

static int domain_num(const CurvesGeometry &curves, const AttrDomain domain)
{
  return domain == AttrDomain::Point ? curves.points_num() : curves.curves_num();
}

static CustomData &domain_custom_data(CurvesGeometry &curves, const AttrDomain domain)
{
  return domain == AttrDomain::Point ? curves.point_data : curves.curve_data;
}

static const CustomData &domain_custom_data(const CurvesGeometry &curves, const AttrDomain domain)
{
  return domain == AttrDomain::Point ? curves.point_data : curves.curve_data;
}

template<typename T>
static VArray<T> get_varray_attribute(const CurvesGeometry &curves,
                                      const AttrDomain domain,
                                      const StringRef name,
                                      const T default_value)
{
  const int num = domain_num(curves, domain);
  const eCustomDataType type = cpp_type_to_custom_data_type(CPPType::get<T>());
  const CustomData &custom_data = domain_custom_data(curves, domain);

  const T *data = (const T *)CustomData_get_layer_named(&custom_data, type, name);
  if (data != nullptr) {
    return VArray<T>::ForSpan(Span<T>(data, num));
  }
  return VArray<T>::ForSingle(default_value, num);
}

template<typename T>
static Span<T> get_span_attribute(const CurvesGeometry &curves,
                                  const AttrDomain domain,
                                  const StringRef name)
{
  const int num = domain_num(curves, domain);
  const CustomData &custom_data = domain_custom_data(curves, domain);
  const eCustomDataType type = cpp_type_to_custom_data_type(CPPType::get<T>());

  T *data = (T *)CustomData_get_layer_named(&custom_data, type, name);
  if (data == nullptr) {
    return {};
  }
  return {data, num};
}

template<typename T>
static MutableSpan<T> get_mutable_attribute(CurvesGeometry &curves,
                                            const AttrDomain domain,
                                            const StringRef name,
                                            const T default_value = T())
{
  const int num = domain_num(curves, domain);
  if (num <= 0) {
    return {};
  }
  const eCustomDataType type = cpp_type_to_custom_data_type(CPPType::get<T>());
  CustomData &custom_data = domain_custom_data(curves, domain);

  T *data = (T *)CustomData_get_layer_named_for_write(&custom_data, type, name, num);
  if (data != nullptr) {
    return {data, num};
  }
  data = (T *)CustomData_add_layer_named(&custom_data, type, CD_SET_DEFAULT, num, name);
  MutableSpan<T> span = {data, num};
  if (num > 0 && span.first() != default_value) {
    span.fill(default_value);
  }
  return span;
}

VArray<int8_t> CurvesGeometry::curve_types() const
{
  return get_varray_attribute<int8_t>(
      *this, AttrDomain::Curve, ATTR_CURVE_TYPE, CURVE_TYPE_CATMULL_ROM);
}

MutableSpan<int8_t> CurvesGeometry::curve_types_for_write()
{
  return get_mutable_attribute<int8_t>(*this, AttrDomain::Curve, ATTR_CURVE_TYPE);
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
  return get_span_attribute<float3>(*this, AttrDomain::Point, ATTR_POSITION);
}
MutableSpan<float3> CurvesGeometry::positions_for_write()
{
  return get_mutable_attribute<float3>(*this, AttrDomain::Point, ATTR_POSITION);
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
  return get_varray_attribute<bool>(*this, AttrDomain::Curve, ATTR_CYCLIC, false);
}
MutableSpan<bool> CurvesGeometry::cyclic_for_write()
{
  return get_mutable_attribute<bool>(*this, AttrDomain::Curve, ATTR_CYCLIC, false);
}

VArray<int> CurvesGeometry::resolution() const
{
  return get_varray_attribute<int>(*this, AttrDomain::Curve, ATTR_RESOLUTION, 12);
}
MutableSpan<int> CurvesGeometry::resolution_for_write()
{
  return get_mutable_attribute<int>(*this, AttrDomain::Curve, ATTR_RESOLUTION, 12);
}

VArray<int8_t> CurvesGeometry::normal_mode() const
{
  return get_varray_attribute<int8_t>(*this, AttrDomain::Curve, ATTR_NORMAL_MODE, 0);
}
MutableSpan<int8_t> CurvesGeometry::normal_mode_for_write()
{
  return get_mutable_attribute<int8_t>(*this, AttrDomain::Curve, ATTR_NORMAL_MODE);
}

VArray<float> CurvesGeometry::tilt() const
{
  return get_varray_attribute<float>(*this, AttrDomain::Point, ATTR_TILT, 0.0f);
}
MutableSpan<float> CurvesGeometry::tilt_for_write()
{
  return get_mutable_attribute<float>(*this, AttrDomain::Point, ATTR_TILT);
}

VArray<int8_t> CurvesGeometry::handle_types_left() const
{
  return get_varray_attribute<int8_t>(*this, AttrDomain::Point, ATTR_HANDLE_TYPE_LEFT, 0);
}
MutableSpan<int8_t> CurvesGeometry::handle_types_left_for_write()
{
  return get_mutable_attribute<int8_t>(*this, AttrDomain::Point, ATTR_HANDLE_TYPE_LEFT, 0);
}

VArray<int8_t> CurvesGeometry::handle_types_right() const
{
  return get_varray_attribute<int8_t>(*this, AttrDomain::Point, ATTR_HANDLE_TYPE_RIGHT, 0);
}
MutableSpan<int8_t> CurvesGeometry::handle_types_right_for_write()
{
  return get_mutable_attribute<int8_t>(*this, AttrDomain::Point, ATTR_HANDLE_TYPE_RIGHT, 0);
}

Span<float3> CurvesGeometry::handle_positions_left() const
{
  return get_span_attribute<float3>(*this, AttrDomain::Point, ATTR_HANDLE_POSITION_LEFT);
}
MutableSpan<float3> CurvesGeometry::handle_positions_left_for_write()
{
  return get_mutable_attribute<float3>(*this, AttrDomain::Point, ATTR_HANDLE_POSITION_LEFT);
}

Span<float3> CurvesGeometry::handle_positions_right() const
{
  return get_span_attribute<float3>(*this, AttrDomain::Point, ATTR_HANDLE_POSITION_RIGHT);
}
MutableSpan<float3> CurvesGeometry::handle_positions_right_for_write()
{
  return get_mutable_attribute<float3>(*this, AttrDomain::Point, ATTR_HANDLE_POSITION_RIGHT);
}

VArray<int8_t> CurvesGeometry::nurbs_orders() const
{
  return get_varray_attribute<int8_t>(*this, AttrDomain::Curve, ATTR_NURBS_ORDER, 4);
}
MutableSpan<int8_t> CurvesGeometry::nurbs_orders_for_write()
{
  return get_mutable_attribute<int8_t>(*this, AttrDomain::Curve, ATTR_NURBS_ORDER, 4);
}

Span<float> CurvesGeometry::nurbs_weights() const
{
  return get_span_attribute<float>(*this, AttrDomain::Point, ATTR_NURBS_WEIGHT);
}
MutableSpan<float> CurvesGeometry::nurbs_weights_for_write()
{
  return get_mutable_attribute<float>(*this, AttrDomain::Point, ATTR_NURBS_WEIGHT);
}

VArray<int8_t> CurvesGeometry::nurbs_knots_modes() const
{
  return get_varray_attribute<int8_t>(*this, AttrDomain::Curve, ATTR_NURBS_KNOTS_MODE, 0);
}
MutableSpan<int8_t> CurvesGeometry::nurbs_knots_modes_for_write()
{
  return get_mutable_attribute<int8_t>(*this, AttrDomain::Curve, ATTR_NURBS_KNOTS_MODE, 0);
}

Span<float2> CurvesGeometry::surface_uv_coords() const
{
  return get_span_attribute<float2>(*this, AttrDomain::Curve, ATTR_SURFACE_UV_COORDINATE);
}

MutableSpan<float2> CurvesGeometry::surface_uv_coords_for_write()
{
  return get_mutable_attribute<float2>(*this, AttrDomain::Curve, ATTR_SURFACE_UV_COORDINATE);
}

Span<MDeformVert> CurvesGeometry::deform_verts() const
{
  const MDeformVert *dverts = static_cast<const MDeformVert *>(
      CustomData_get_layer(&this->point_data, CD_MDEFORMVERT));
  if (dverts == nullptr) {
    return {};
  }
  return {dverts, this->point_num};
}

MutableSpan<MDeformVert> CurvesGeometry::deform_verts_for_write()
{
  MDeformVert *dvert = static_cast<MDeformVert *>(
      CustomData_get_layer_for_write(&this->point_data, CD_MDEFORMVERT, this->point_num));
  if (dvert != nullptr) {
    return {dvert, this->point_num};
  }
  return {static_cast<MDeformVert *>(CustomData_add_layer(
              &this->point_data, CD_MDEFORMVERT, CD_SET_DEFAULT, this->point_num)),
          this->point_num};
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
  const CurvesGeometryRuntime &runtime = *this->runtime;
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
  const CurvesGeometryRuntime &runtime = *this->runtime;
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
  const CurvesGeometryRuntime &runtime = *this->runtime;
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
      array_utils::copy_group_to_group(
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
  const CurvesGeometryRuntime &runtime = *this->runtime;
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
                handles_left[points.last()], positions[points.last()], epsilon))
        {
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

static void normalize_span(MutableSpan<float3> data)
{
  for (const int i : data.index_range()) {
    data[i] = math::normalize(data[i]);
  }
}

/** Data needed to interpolate generic data from control points to evaluated points. */
struct EvalData {
  const OffsetIndices<int> points_by_curve;
  const VArray<int8_t> &types;
  const VArray<bool> &cyclic;
  const VArray<int> &resolution;
  const Span<int> all_bezier_evaluated_offsets;
  const Span<curves::nurbs::BasisCache> nurbs_basis_cache;
  const VArray<int8_t> &nurbs_orders;
  const Span<float> nurbs_weights;
};

static void evaluate_generic_data_for_curve(const EvalData &eval_data,
                                            const int curve_index,
                                            const GSpan src,
                                            GMutableSpan dst)
{
  const IndexRange points = eval_data.points_by_curve[curve_index];
  switch (eval_data.types[curve_index]) {
    case CURVE_TYPE_CATMULL_ROM:
      curves::catmull_rom::interpolate_to_evaluated(
          src, eval_data.cyclic[curve_index], eval_data.resolution[curve_index], dst);
      break;
    case CURVE_TYPE_POLY:
      dst.copy_from(src);
      break;
    case CURVE_TYPE_BEZIER: {
      const IndexRange offsets = curves::per_curve_point_offsets_range(points, curve_index);
      curves::bezier::interpolate_to_evaluated(
          src, eval_data.all_bezier_evaluated_offsets.slice(offsets), dst);
      break;
    }
    case CURVE_TYPE_NURBS:
      curves::nurbs::interpolate_to_evaluated(eval_data.nurbs_basis_cache[curve_index],
                                              eval_data.nurbs_orders[curve_index],
                                              eval_data.nurbs_weights.slice_safe(points),
                                              src,
                                              dst);
      break;
  }
}

Span<float3> CurvesGeometry::evaluated_normals() const
{
  const CurvesGeometryRuntime &runtime = *this->runtime;
  this->ensure_nurbs_basis_cache();
  runtime.evaluated_normal_cache.ensure([&](Vector<float3> &r_data) {
    const OffsetIndices<int> points_by_curve = this->points_by_curve();
    const OffsetIndices<int> evaluated_points_by_curve = this->evaluated_points_by_curve();
    const VArray<int8_t> types = this->curve_types();
    const VArray<bool> cyclic = this->cyclic();
    const VArray<int8_t> normal_mode = this->normal_mode();
    const Span<float3> evaluated_tangents = this->evaluated_tangents();
    const AttributeAccessor attributes = this->attributes();
    const EvalData eval_data{
        points_by_curve,
        types,
        cyclic,
        this->resolution(),
        runtime.evaluated_offsets_cache.data().all_bezier_offsets,
        runtime.nurbs_basis_cache.data(),
        this->nurbs_orders(),
        this->nurbs_weights(),
    };
    const VArray<float> tilt = this->tilt();
    VArraySpan<float> tilt_span;
    const bool use_tilt = !(tilt.is_single() && tilt.get_internal_single() == 0.0f);
    if (use_tilt) {
      tilt_span = tilt;
    }
    VArraySpan<float3> custom_normal_span;
    if (const VArray<float3> custom_normal = *attributes.lookup<float3>("custom_normal",
                                                                        AttrDomain::Point))
    {
      custom_normal_span = custom_normal;
    }

    r_data.resize(this->evaluated_points_num());
    MutableSpan<float3> evaluated_normals = r_data;

    threading::parallel_for(this->curves_range(), 128, [&](IndexRange curves_range) {
      /* Reuse a buffer for the evaluated tilts. */
      Vector<float> evaluated_tilts;

      for (const int curve_index : curves_range) {
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];
        switch (NormalMode(normal_mode[curve_index])) {
          case NORMAL_MODE_Z_UP:
            curves::poly::calculate_normals_z_up(evaluated_tangents.slice(evaluated_points),
                                                 evaluated_normals.slice(evaluated_points));
            break;
          case NORMAL_MODE_MINIMUM_TWIST:
            curves::poly::calculate_normals_minimum(evaluated_tangents.slice(evaluated_points),
                                                    cyclic[curve_index],
                                                    evaluated_normals.slice(evaluated_points));
            break;
          case NORMAL_MODE_FREE:
            if (custom_normal_span.is_empty()) {
              curves::poly::calculate_normals_z_up(evaluated_tangents.slice(evaluated_points),
                                                   evaluated_normals.slice(evaluated_points));
            }
            else {
              const Span<float3> src = custom_normal_span.slice(points_by_curve[curve_index]);
              MutableSpan<float3> dst = evaluated_normals.slice(
                  evaluated_points_by_curve[curve_index]);
              evaluate_generic_data_for_curve(eval_data, curve_index, src, dst);
              normalize_span(dst);
            }
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
            evaluate_generic_data_for_curve(eval_data,
                                            curve_index,
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
  const CurvesGeometryRuntime &runtime = *this->runtime;
  const EvalData eval_data{
      this->points_by_curve(),
      this->curve_types(),
      this->cyclic(),
      this->resolution(),
      runtime.evaluated_offsets_cache.data().all_bezier_offsets,
      runtime.nurbs_basis_cache.data(),
      this->nurbs_orders(),
      this->nurbs_weights(),
  };
  BLI_assert(src.size() == this->points_by_curve()[curve_index].size());
  BLI_assert(dst.size() == this->evaluated_points_by_curve()[curve_index].size());
  evaluate_generic_data_for_curve(eval_data, curve_index, src, dst);
}

void CurvesGeometry::interpolate_to_evaluated(const GSpan src, GMutableSpan dst) const
{
  const CurvesGeometryRuntime &runtime = *this->runtime;
  const OffsetIndices points_by_curve = this->points_by_curve();
  const EvalData eval_data{
      points_by_curve,
      this->curve_types(),
      this->cyclic(),
      this->resolution(),
      runtime.evaluated_offsets_cache.data().all_bezier_offsets,
      runtime.nurbs_basis_cache.data(),
      this->nurbs_orders(),
      this->nurbs_weights(),
  };
  const OffsetIndices evaluated_points_by_curve = this->evaluated_points_by_curve();

  threading::parallel_for(this->curves_range(), 512, [&](IndexRange curves_range) {
    for (const int curve_index : curves_range) {
      const IndexRange points = points_by_curve[curve_index];
      const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];
      evaluate_generic_data_for_curve(
          eval_data, curve_index, src.slice(points), dst.slice(evaluated_points));
    }
  });
}

void CurvesGeometry::ensure_evaluated_lengths() const
{
  const CurvesGeometryRuntime &runtime = *this->runtime;
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

static void transform_normals(MutableSpan<float3> normals, const float4x4 &matrix)
{
  const float3x3 normal_transform = math::transpose(math::invert(float3x3(matrix)));
  threading::parallel_for(normals.index_range(), 1024, [&](const IndexRange range) {
    for (float3 &normal : normals.slice(range)) {
      normal = normal_transform * normal;
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
  MutableAttributeAccessor attributes = this->attributes_for_write();
  if (SpanAttributeWriter normals = attributes.lookup_for_write_span<float3>("custom_normal")) {
    transform_normals(normals.span, matrix);
    normals.finish();
  }
  this->tag_positions_changed();
}

std::optional<Bounds<float3>> CurvesGeometry::bounds_min_max() const
{
  if (this->points_num() == 0) {
    return std::nullopt;
  }
  this->runtime->bounds_cache.ensure(
      [&](Bounds<float3> &r_bounds) { r_bounds = *bounds::min_max(this->evaluated_positions()); });
  return this->runtime->bounds_cache.data();
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

  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &curves.vertex_group_names);

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
                          AttrDomain::Point,
                          propagation_info,
                          {},
                          points_to_copy,
                          dst_curves.attributes_for_write());
        gather_attributes(curves.attributes(),
                          AttrDomain::Curve,
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
    return;
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

  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &curves.vertex_group_names);

  const AttributeAccessor src_attributes = curves.attributes();
  MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();

  gather_attributes_group_to_group(src_attributes,
                                   AttrDomain::Point,
                                   propagation_info,
                                   {},
                                   points_by_curve,
                                   dst_points_by_curve,
                                   curves_to_copy,
                                   dst_attributes);

  gather_attributes(
      src_attributes, AttrDomain::Curve, propagation_info, {}, curves_to_copy, dst_attributes);

  dst_curves.update_curve_types();
  dst_curves.remove_attributes_based_on_types();

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
    if (meta_data.domain != AttrDomain::Point) {
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
                                     const AttrDomain from,
                                     const AttrDomain to) const
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

  if (from == AttrDomain::Point && to == AttrDomain::Curve) {
    return adapt_curve_domain_point_to_curve(*this, varray);
  }
  if (from == AttrDomain::Curve && to == AttrDomain::Point) {
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
    this->runtime->curve_offsets_sharing_info = BLO_read_shared(
        &reader, &this->curve_offsets, [&]() {
          BLO_read_int32_array(&reader, this->curve_num + 1, &this->curve_offsets);
          return implicit_sharing::info_for_mem_free(this->curve_offsets);
        });
  }

  BLO_read_list(&reader, &this->vertex_group_names);

  /* Recalculate curve type count cache that isn't saved in files. */
  this->update_curve_types();
}

CurvesGeometry::BlendWriteData CurvesGeometry::blend_write_prepare()
{
  CurvesGeometry::BlendWriteData write_data;
  CustomData_blend_write_prepare(this->point_data, write_data.point_layers);
  CustomData_blend_write_prepare(this->curve_data, write_data.curve_layers);
  return write_data;
}

void CurvesGeometry::blend_write(BlendWriter &writer,
                                 ID &id,
                                 const CurvesGeometry::BlendWriteData &write_data)
{
  CustomData_blend_write(
      &writer, &this->point_data, write_data.point_layers, this->point_num, CD_MASK_ALL, &id);
  CustomData_blend_write(
      &writer, &this->curve_data, write_data.curve_layers, this->curve_num, CD_MASK_ALL, &id);

  if (this->curve_offsets) {
    BLO_write_shared(
        &writer,
        this->curve_offsets,
        sizeof(int) * (this->curve_num + 1),
        this->runtime->curve_offsets_sharing_info,
        [&]() { BLO_write_int32_array(&writer, this->curve_num + 1, this->curve_offsets); });
  }

  BKE_defbase_blend_write(&writer, &this->vertex_group_names);
}

/** \} */

}  // namespace blender::bke
