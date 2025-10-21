/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <utility>

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_bounds.hh"
#include "BLI_index_mask.hh"
#include "BLI_length_parameterize.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation_legacy.hh"
#include "BLI_memory_counter.hh"
#include "BLI_resource_scope.hh"
#include "BLI_task.hh"

#include "BLO_read_write.hh"

#include "DNA_curves_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_attribute_math.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_attribute_storage_blend_write.hh"
#include "BKE_bake_data_block_id.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"

#include "attribute_storage_access.hh"

namespace blender::bke {

constexpr StringRef ATTR_POSITION = "position";
constexpr StringRef ATTR_RADIUS = "radius";
constexpr StringRef ATTR_TILT = "tilt";
constexpr StringRef ATTR_CURVE_TYPE = "curve_type";
constexpr StringRef ATTR_CYCLIC = "cyclic";
constexpr StringRef ATTR_RESOLUTION = "resolution";
constexpr StringRef ATTR_NORMAL_MODE = "normal_mode";
constexpr StringRef ATTR_HANDLE_TYPE_LEFT = "handle_type_left";
constexpr StringRef ATTR_HANDLE_TYPE_RIGHT = "handle_type_right";
constexpr StringRef ATTR_HANDLE_POSITION_LEFT = "handle_left";
constexpr StringRef ATTR_HANDLE_POSITION_RIGHT = "handle_right";
constexpr StringRef ATTR_NURBS_ORDER = "nurbs_order";
constexpr StringRef ATTR_NURBS_WEIGHT = "nurbs_weight";
constexpr StringRef ATTR_NURBS_KNOTS_MODE = "knots_mode";
constexpr StringRef ATTR_SURFACE_UV_COORDINATE = "surface_uv_coordinate";

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
  CustomData_reset(&this->curve_data_legacy);
  new (&this->attribute_storage.wrap()) blender::bke::AttributeStorage();
  BLI_listbase_clear(&this->vertex_group_names);

  this->attributes_for_write().add<float3>(
      "position", AttrDomain::Point, AttributeInitConstruct());

  this->custom_knots = nullptr;
  this->custom_knot_num = 0;

  if (curve_num > 0) {
    this->curve_offsets = MEM_malloc_arrayN<int>(size_t(this->curve_num) + 1, __func__);
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

  this->custom_knots = other.custom_knots;
  this->custom_knot_num = other.custom_knot_num;
  if (other.runtime->custom_knots_sharing_info) {
    other.runtime->custom_knots_sharing_info->add_user();
  }

  CustomData_init_from(&other.point_data, &this->point_data, CD_MASK_MDEFORMVERT, other.point_num);

  new (&this->attribute_storage.wrap()) AttributeStorage(other.attribute_storage.wrap());

  this->point_num = other.point_num;
  this->curve_num = other.curve_num;

  BKE_defgroup_copy_list(&this->vertex_group_names, &other.vertex_group_names);
  this->vertex_group_active_index = other.vertex_group_active_index;

  this->attributes_active_index = other.attributes_active_index;

  this->runtime = MEM_new<CurvesGeometryRuntime>(
      __func__,
      CurvesGeometryRuntime{other.runtime->curve_offsets_sharing_info,
                            other.runtime->custom_knots_sharing_info,
                            other.runtime->type_counts,
                            other.runtime->evaluated_offsets_cache,
                            other.runtime->has_cyclic_curve_cache,
                            other.runtime->nurbs_basis_cache,
                            other.runtime->evaluated_position_cache,
                            other.runtime->bounds_cache,
                            other.runtime->bounds_with_radius_cache,
                            other.runtime->evaluated_length_cache,
                            other.runtime->evaluated_tangent_cache,
                            other.runtime->evaluated_normal_cache,
                            other.runtime->max_material_index_cache,
                            other.runtime->custom_knot_offsets_cache,
                            {},
                            true});

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

  this->custom_knots = other.custom_knots;
  other.custom_knots = nullptr;

  this->custom_knot_num = other.custom_knot_num;
  other.custom_knot_num = 0;

  this->point_data = other.point_data;
  CustomData_reset(&other.point_data);

  new (&this->attribute_storage.wrap())
      AttributeStorage(std::move(other.attribute_storage.wrap()));

  this->point_num = other.point_num;
  other.point_num = 0;

  this->curve_num = other.curve_num;
  other.curve_num = 0;

  this->vertex_group_names = other.vertex_group_names;
  BLI_listbase_clear(&other.vertex_group_names);

  this->vertex_group_active_index = other.vertex_group_active_index;
  other.vertex_group_active_index = 0;

  this->attributes_active_index = other.attributes_active_index;
  other.attributes_active_index = 0;

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
  CustomData_free(&this->point_data);
  this->attribute_storage.wrap().~AttributeStorage();
  BLI_freelistN(&this->vertex_group_names);
  if (this->runtime) {
    implicit_sharing::free_shared_data(&this->curve_offsets,
                                       &this->runtime->curve_offsets_sharing_info);
    implicit_sharing::free_shared_data(&this->custom_knots,
                                       &this->runtime->custom_knots_sharing_info);
    MEM_delete(this->runtime);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Accessors
 * \{ */

VArray<int8_t> CurvesGeometry::curve_types() const
{
  return get_varray_attribute<int8_t>(this->attribute_storage.wrap(),
                                      AttrDomain::Curve,
                                      ATTR_CURVE_TYPE,
                                      this->curves_num(),
                                      CURVE_TYPE_CATMULL_ROM);
}

MutableSpan<int8_t> CurvesGeometry::curve_types_for_write()
{
  return get_mutable_attribute<int8_t>(
      this->attribute_storage.wrap(), AttrDomain::Curve, ATTR_CURVE_TYPE, this->curves_num());
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
  return *get_span_attribute<float3>(
      this->attribute_storage.wrap(), AttrDomain::Point, ATTR_POSITION, this->points_num());
}
MutableSpan<float3> CurvesGeometry::positions_for_write()
{
  return get_mutable_attribute<float3>(
      this->attribute_storage.wrap(), AttrDomain::Point, ATTR_POSITION, this->points_num());
}

VArray<float> CurvesGeometry::radius() const
{
  return get_varray_attribute<float>(
      this->attribute_storage.wrap(), AttrDomain::Point, ATTR_RADIUS, this->points_num(), 0.01f);
}
MutableSpan<float> CurvesGeometry::radius_for_write()
{
  return get_mutable_attribute<float>(
      this->attribute_storage.wrap(), AttrDomain::Point, ATTR_RADIUS, this->points_num(), 0.01f);
}

Span<int> CurvesGeometry::offsets() const
{
  if (this->curve_num == 0) {
    return {};
  }
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
  return get_varray_attribute<bool>(
      this->attribute_storage.wrap(), AttrDomain::Curve, ATTR_CYCLIC, this->curves_num(), false);
}
MutableSpan<bool> CurvesGeometry::cyclic_for_write()
{
  return get_mutable_attribute<bool>(
      this->attribute_storage.wrap(), AttrDomain::Curve, ATTR_CYCLIC, this->curves_num(), false);
}

VArray<int> CurvesGeometry::resolution() const
{
  return get_varray_attribute<int>(
      this->attribute_storage.wrap(), AttrDomain::Curve, ATTR_RESOLUTION, this->curves_num(), 12);
}
MutableSpan<int> CurvesGeometry::resolution_for_write()
{
  return get_mutable_attribute<int>(
      this->attribute_storage.wrap(), AttrDomain::Curve, ATTR_RESOLUTION, this->curves_num(), 12);
}

VArray<int8_t> CurvesGeometry::normal_mode() const
{
  return get_varray_attribute<int8_t>(
      this->attribute_storage.wrap(), AttrDomain::Curve, ATTR_NORMAL_MODE, this->curves_num(), 0);
}
MutableSpan<int8_t> CurvesGeometry::normal_mode_for_write()
{
  return get_mutable_attribute<int8_t>(
      this->attribute_storage.wrap(), AttrDomain::Curve, ATTR_NORMAL_MODE, this->curves_num());
}

VArray<float> CurvesGeometry::tilt() const
{
  return get_varray_attribute<float>(
      this->attribute_storage.wrap(), AttrDomain::Point, ATTR_TILT, this->points_num(), 0.0f);
}
MutableSpan<float> CurvesGeometry::tilt_for_write()
{
  return get_mutable_attribute<float>(
      this->attribute_storage.wrap(), AttrDomain::Point, ATTR_TILT, this->points_num());
}

VArray<int8_t> CurvesGeometry::handle_types_left() const
{
  return get_varray_attribute<int8_t>(this->attribute_storage.wrap(),
                                      AttrDomain::Point,
                                      ATTR_HANDLE_TYPE_LEFT,
                                      this->points_num(),
                                      0);
}
MutableSpan<int8_t> CurvesGeometry::handle_types_left_for_write()
{
  return get_mutable_attribute<int8_t>(this->attribute_storage.wrap(),
                                       AttrDomain::Point,
                                       ATTR_HANDLE_TYPE_LEFT,
                                       this->points_num(),
                                       0);
}

VArray<int8_t> CurvesGeometry::handle_types_right() const
{
  return get_varray_attribute<int8_t>(this->attribute_storage.wrap(),
                                      AttrDomain::Point,
                                      ATTR_HANDLE_TYPE_RIGHT,
                                      this->points_num(),
                                      0);
}
MutableSpan<int8_t> CurvesGeometry::handle_types_right_for_write()
{
  return get_mutable_attribute<int8_t>(this->attribute_storage.wrap(),
                                       AttrDomain::Point,
                                       ATTR_HANDLE_TYPE_RIGHT,
                                       this->points_num(),
                                       0);
}

std::optional<Span<float3>> CurvesGeometry::handle_positions_left() const
{
  return get_span_attribute<float3>(this->attribute_storage.wrap(),
                                    AttrDomain::Point,
                                    ATTR_HANDLE_POSITION_LEFT,
                                    this->points_num());
}
MutableSpan<float3> CurvesGeometry::handle_positions_left_for_write()
{
  return get_mutable_attribute<float3>(this->attribute_storage.wrap(),
                                       AttrDomain::Point,
                                       ATTR_HANDLE_POSITION_LEFT,
                                       this->points_num());
}

std::optional<Span<float3>> CurvesGeometry::handle_positions_right() const
{
  return get_span_attribute<float3>(this->attribute_storage.wrap(),
                                    AttrDomain::Point,
                                    ATTR_HANDLE_POSITION_RIGHT,
                                    this->points_num());
}
MutableSpan<float3> CurvesGeometry::handle_positions_right_for_write()
{
  return get_mutable_attribute<float3>(this->attribute_storage.wrap(),
                                       AttrDomain::Point,
                                       ATTR_HANDLE_POSITION_RIGHT,
                                       this->points_num());
}

VArray<int8_t> CurvesGeometry::nurbs_orders() const
{
  return get_varray_attribute<int8_t>(
      this->attribute_storage.wrap(), AttrDomain::Curve, ATTR_NURBS_ORDER, this->curves_num(), 4);
}
MutableSpan<int8_t> CurvesGeometry::nurbs_orders_for_write()
{
  return get_mutable_attribute<int8_t>(
      this->attribute_storage.wrap(), AttrDomain::Curve, ATTR_NURBS_ORDER, this->curves_num(), 4);
}

std::optional<Span<float>> CurvesGeometry::nurbs_weights() const
{
  return get_span_attribute<float>(
      this->attribute_storage.wrap(), AttrDomain::Point, ATTR_NURBS_WEIGHT, this->points_num());
}
MutableSpan<float> CurvesGeometry::nurbs_weights_for_write()
{
  return get_mutable_attribute<float>(this->attribute_storage.wrap(),
                                      AttrDomain::Point,
                                      ATTR_NURBS_WEIGHT,
                                      this->points_num(),
                                      1.0f);
}

VArray<int8_t> CurvesGeometry::nurbs_knots_modes() const
{
  return get_varray_attribute<int8_t>(this->attribute_storage.wrap(),
                                      AttrDomain::Curve,
                                      ATTR_NURBS_KNOTS_MODE,
                                      this->curves_num(),
                                      0);
}
MutableSpan<int8_t> CurvesGeometry::nurbs_knots_modes_for_write()
{
  return get_mutable_attribute<int8_t>(this->attribute_storage.wrap(),
                                       AttrDomain::Curve,
                                       ATTR_NURBS_KNOTS_MODE,
                                       this->curves_num(),
                                       0);
}

std::optional<Span<float2>> CurvesGeometry::surface_uv_coords() const
{
  return get_span_attribute<float2>(this->attribute_storage.wrap(),
                                    AttrDomain::Curve,
                                    ATTR_SURFACE_UV_COORDINATE,
                                    this->curves_num());
}

MutableSpan<float2> CurvesGeometry::surface_uv_coords_for_write()
{
  return get_mutable_attribute<float2>(this->attribute_storage.wrap(),
                                       AttrDomain::Curve,
                                       ATTR_SURFACE_UV_COORDINATE,
                                       this->curves_num());
}

Span<float> CurvesGeometry::nurbs_custom_knots() const
{
  if (this->custom_knot_num == 0) {
    return {};
  }
  return {this->custom_knots, this->custom_knot_num};
}

MutableSpan<float> CurvesGeometry::nurbs_custom_knots_for_write()
{
  if (this->custom_knot_num == 0) {
    return {};
  }
  implicit_sharing::make_trivial_data_mutable(
      &this->custom_knots, &this->runtime->custom_knots_sharing_info, this->custom_knot_num);
  return {this->custom_knots, this->custom_knot_num};
}

IndexMask CurvesGeometry::nurbs_custom_knot_curves(IndexMaskMemory &memory) const
{
  const VArray<int8_t> curve_types = this->curve_types();
  const VArray<int8_t> knot_modes = this->nurbs_knots_modes();
  return IndexMask::from_predicate(
      this->curves_range(), GrainSize(4096), memory, [&](const int64_t curve) {
        return curve_types[curve] == CURVE_TYPE_NURBS &&
               knot_modes[curve] == NURBS_KNOT_MODE_CUSTOM;
      });
}

OffsetIndices<int> CurvesGeometry::nurbs_custom_knots_by_curve() const
{
  const CurvesGeometryRuntime &runtime = *this->runtime;
  if (!this->has_curve_with_type(CURVE_TYPE_NURBS)) {
    return {};
  }
  runtime.custom_knot_offsets_cache.ensure([&](Vector<int> &r_data) {
    r_data.resize(this->curve_num + 1);

    const OffsetIndices<int> points_by_curve = this->points_by_curve();
    const VArray<int8_t> curve_types = this->curve_types();
    const VArray<int8_t> knot_modes = this->nurbs_knots_modes();
    const VArray<int8_t> orders = this->nurbs_orders();

    threading::parallel_for(this->curves_range(), 1024, [&](const IndexRange range) {
      for (const int curve : range) {
        if (curve_types[curve] != CURVE_TYPE_NURBS) {
          r_data[curve] = 0;
          continue;
        }
        if (knot_modes[curve] != NURBS_KNOT_MODE_CUSTOM) {
          r_data[curve] = 0;
          continue;
        }
        r_data[curve] = points_by_curve[curve].size() + orders[curve];
      }
    });
    offset_indices::accumulate_counts_to_offsets(r_data.as_mutable_span());
  });
  return OffsetIndices<int>(runtime.custom_knot_offsets_cache.data());
}

void CurvesGeometry::nurbs_custom_knots_update_size()
{
  this->runtime->custom_knot_offsets_cache.tag_dirty();
  const OffsetIndices<int> knots_by_curve = this->nurbs_custom_knots_by_curve();
  const int knots_num = knots_by_curve.total_size();
  if (this->custom_knot_num != knots_num) {
    implicit_sharing::resize_trivial_array(&this->custom_knots,
                                           &this->runtime->custom_knots_sharing_info,
                                           this->custom_knot_num,
                                           knots_num);
    this->custom_knot_num = knots_num;
  }
}

void CurvesGeometry::nurbs_custom_knots_resize(int knots_num)
{
  implicit_sharing::resize_trivial_array(&this->custom_knots,
                                         &this->runtime->custom_knots_sharing_info,
                                         this->custom_knot_num,
                                         knots_num);
  this->custom_knot_num = knots_num;
  this->runtime->custom_knot_offsets_cache.tag_dirty();
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
  const OffsetIndices<int> custom_knots_by_curve = curves.nurbs_custom_knots_by_curve();
  const Span<float> all_custom_knots = curves.nurbs_custom_knots();

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
        const bool is_cyclic = cyclic[curve_index];
        const int8_t order = nurbs_orders[curve_index];
        const KnotsMode knots_mode = KnotsMode(nurbs_knots_modes[curve_index]);
        const IndexRange custom_knots_range = custom_knots_by_curve[curve_index];
        const Span<float> custom_knots = knots_mode == NURBS_KNOT_MODE_CUSTOM &&
                                                 !all_custom_knots.is_empty() &&
                                                 !custom_knots_range.is_empty() ?
                                             all_custom_knots.slice(custom_knots_range) :
                                             Span<float>();
        return curves::nurbs::calculate_evaluated_num(
            points.size(), order, is_cyclic, resolution[curve_index], knots_mode, custom_knots);
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

bool CurvesGeometry::has_cyclic_curve() const
{
  this->runtime->has_cyclic_curve_cache.ensure([&](bool &r_data) {
    r_data = array_utils::contains(this->cyclic(), this->curves_range(), true);
  });
  return this->runtime->has_cyclic_curve_cache.data();
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
    const OffsetIndices<int> custom_knots_by_curve = this->nurbs_custom_knots_by_curve();
    const VArray<bool> cyclic = this->cyclic();
    const VArray<int8_t> orders = this->nurbs_orders();
    const VArray<int> resolutions = this->resolution();
    const VArray<int8_t> knots_modes = this->nurbs_knots_modes();
    const Span<float> custom_knots = this->nurbs_custom_knots();

    nurbs_mask.foreach_segment(GrainSize(64), [&](const IndexMaskSegment segment) {
      Vector<float, 32> knots;
      for (const int curve_index : segment) {
        const IndexRange points = points_by_curve[curve_index];
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];

        const int8_t order = orders[curve_index];
        const int resolution = resolutions[curve_index];
        const bool is_cyclic = cyclic[curve_index];
        const KnotsMode mode = KnotsMode(knots_modes[curve_index]);

        if (!curves::nurbs::check_valid_eval_params(
                points.size(), order, is_cyclic, mode, resolution))
        {
          r_data[curve_index].invalid = true;
          continue;
        }
        const int knots_num = curves::nurbs::knots_num(points.size(), order, is_cyclic);
        knots.reinitialize(knots_num);
        curves::nurbs::load_curve_knots(mode,
                                        points.size(),
                                        order,
                                        is_cyclic,
                                        custom_knots_by_curve[curve_index],
                                        custom_knots,
                                        knots);

        curves::nurbs::calculate_basis_cache(points.size(),
                                             evaluated_points.size(),
                                             order,
                                             resolution,
                                             is_cyclic,
                                             mode,
                                             knots,
                                             r_data[curve_index]);
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
      const std::optional<Span<float3>> handle_positions_left = this->handle_positions_left();
      const std::optional<Span<float3>> handle_positions_right = this->handle_positions_right();
      if (!handle_positions_left || !handle_positions_right) {
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
                                                      handle_positions_left->slice(points),
                                                      handle_positions_right->slice(points),
                                                      all_bezier_offsets.slice(offsets),
                                                      evaluated_positions.slice(evaluated_points));
      });
    };
    auto evaluate_nurbs = [&](const IndexMask &selection) {
      this->ensure_nurbs_basis_cache();
      const VArray<int8_t> nurbs_orders = this->nurbs_orders();
      const std::optional<Span<float>> nurbs_weights = this->nurbs_weights();
      const Span<curves::nurbs::BasisCache> nurbs_basis_cache = runtime.nurbs_basis_cache.data();
      selection.foreach_index(GrainSize(128), [&](const int curve_index) {
        const IndexRange points = points_by_curve[curve_index];
        const IndexRange evaluated_points = evaluated_points_by_curve[curve_index];
        curves::nurbs::interpolate_to_evaluated(nurbs_basis_cache[curve_index],
                                                nurbs_orders[curve_index],
                                                nurbs_weights ? nurbs_weights->slice(points) :
                                                                Span<float>(),
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
      const Span<float3> handles_left = *this->handle_positions_left();
      const Span<float3> handles_right = *this->handle_positions_right();

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
    const float3 axis = axes[i];
    if (UNLIKELY(math::is_zero(axis))) {
      continue;
    }
    directions[i] = math::rotate_direction_around_axis(directions[i], axis, angles[i]);
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
  const std::optional<Span<float>> nurbs_weights;
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
      curves::nurbs::interpolate_to_evaluated(
          eval_data.nurbs_basis_cache[curve_index],
          eval_data.nurbs_orders[curve_index],
          eval_data.nurbs_weights ? eval_data.nurbs_weights->slice(points) : Span<float>(),
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
  BLI_assert(curves_num >= 0 && points_num >= 0);
  if (points_num != this->point_num) {
    this->attribute_storage.wrap().resize(AttrDomain::Point, points_num);
    CustomData_realloc(&this->point_data, this->points_num(), points_num);
    this->point_num = points_num;
  }
  if (curves_num != this->curve_num) {
    this->attribute_storage.wrap().resize(AttrDomain::Curve, curves_num);
    implicit_sharing::resize_trivial_array(&this->curve_offsets,
                                           &this->runtime->curve_offsets_sharing_info,
                                           this->curve_num == 0 ? 0 : (this->curve_num + 1),
                                           curves_num == 0 ? 0 : (curves_num + 1));
    if (curves_num > 0) {
      /* Set common values for convenience. */
      this->curve_offsets[0] = 0;
      this->curve_offsets[curves_num] = this->point_num;
    }
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
  this->runtime->bounds_with_radius_cache.tag_dirty();
}
void CurvesGeometry::tag_topology_changed()
{
  this->runtime->custom_knot_offsets_cache.tag_dirty();
  this->tag_positions_changed();
  this->runtime->evaluated_offsets_cache.tag_dirty();
  this->runtime->has_cyclic_curve_cache.tag_dirty();
  this->runtime->nurbs_basis_cache.tag_dirty();
  this->runtime->max_material_index_cache.tag_dirty();
  this->runtime->check_type_counts = true;
}
void CurvesGeometry::tag_normals_changed()
{
  this->runtime->evaluated_normal_cache.tag_dirty();
}
void CurvesGeometry::tag_radii_changed()
{
  this->runtime->bounds_with_radius_cache.tag_dirty();
}
void CurvesGeometry::tag_material_index_changed()
{
  this->runtime->max_material_index_cache.tag_dirty();
}

static void translate_positions(MutableSpan<float3> positions, const float3 &translation)
{
  threading::parallel_for(positions.index_range(), 2048, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position += translation;
    }
  });
}

void CurvesGeometry::calculate_bezier_auto_handles()
{
  if (!this->has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return;
  }
  if (!this->handle_positions_left() || !this->handle_positions_right()) {
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
  if (this->handle_positions_left()) {
    translate_positions(this->handle_positions_left_for_write(), translation);
  }
  if (this->handle_positions_right()) {
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
  math::transform_points(matrix, this->positions_for_write());
  if (this->handle_positions_left()) {
    math::transform_points(matrix, this->handle_positions_left_for_write());
  }
  if (this->handle_positions_right()) {
    math::transform_points(matrix, this->handle_positions_right_for_write());
  }
  MutableAttributeAccessor attributes = this->attributes_for_write();
  transform_custom_normal_attribute(matrix, attributes);
  this->tag_positions_changed();
}

std::optional<Bounds<float3>> CurvesGeometry::bounds_min_max(const bool use_radius) const
{
  if (this->is_empty()) {
    return std::nullopt;
  }
  if (use_radius) {
    this->runtime->bounds_with_radius_cache.ensure([&](Bounds<float3> &r_bounds) {
      const VArray<float> radius = this->radius();
      if (const std::optional radius_single = radius.get_if_single()) {
        r_bounds = *this->bounds_min_max(false);
        r_bounds.pad(*radius_single);
        return;
      }
      const Span radius_span = radius.get_internal_span();
      if (this->is_single_type(CURVE_TYPE_POLY)) {
        r_bounds = *bounds::min_max_with_radii(this->positions(), radius_span);
        return;
      }
      Array<float> radii_eval(this->evaluated_points_num());
      this->ensure_can_interpolate_to_evaluated();
      this->interpolate_to_evaluated(radius_span, radii_eval.as_mutable_span());
      r_bounds = *bounds::min_max_with_radii(this->evaluated_positions(), radii_eval.as_span());
    });
  }
  else {
    this->runtime->bounds_cache.ensure([&](Bounds<float3> &r_bounds) {
      r_bounds = *bounds::min_max(this->evaluated_positions());
    });
  }
  return use_radius ? this->runtime->bounds_with_radius_cache.data() :
                      this->runtime->bounds_cache.data();
}

std::optional<int> CurvesGeometry::material_index_max() const
{
  this->runtime->max_material_index_cache.ensure([&](std::optional<int> &r_max_material_index) {
    r_max_material_index = blender::bounds::max<int>(
        this->attributes()
            .lookup_or_default<int>("material_index", blender::bke::AttrDomain::Curve, 0)
            .varray);
    if (r_max_material_index.has_value()) {
      r_max_material_index = std::clamp(*r_max_material_index, 0, MAXMAT);
    }
  });
  return this->runtime->max_material_index_cache.data();
}

void CurvesGeometry::count_memory(MemoryCounter &memory) const
{
  memory.add_shared(this->runtime->curve_offsets_sharing_info, this->offsets().size_in_bytes());
  memory.add_shared(this->runtime->custom_knots_sharing_info,
                    this->nurbs_custom_knots().size_in_bytes());
  this->attribute_storage.wrap().count_memory(memory);
  CustomData_count_memory(this->point_data, this->point_num, memory);
}

static void copy_point_selection_custom_knots(const CurvesGeometry &curves,
                                              const IndexMask &points_to_copy,
                                              const Span<int> curve_point_counts,
                                              CurvesGeometry &dst_curves)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<int8_t> orders = curves.nurbs_orders();
  const VArray<bool> cyclic = curves.cyclic();

  IndexMaskMemory memory;
  const IndexMask custom_knot_curves = curves.nurbs_custom_knot_curves(memory);
  const IndexMask custom_knot_points = bke::curves::curve_to_point_selection(
      points_by_curve, custom_knot_curves, memory);
  const IndexMask custom_knot_points_to_copy = IndexMask::from_intersection(
      points_to_copy, custom_knot_points, memory);

  int dst_knot_count = 0;
  custom_knot_curves.foreach_index([&](const int64_t curve) {
    dst_knot_count += curves::nurbs::knots_num(
        curve_point_counts[curve], orders[curve], cyclic[curve]);
  });
  const OffsetIndices<int> src_knots_by_curve = curves.nurbs_custom_knots_by_curve();
  const Span<float> src_knots = curves.nurbs_custom_knots();

  Vector<float> new_knots;
  new_knots.reserve(dst_knot_count);

  curves::foreach_selected_point_ranges_per_curve(
      custom_knot_points_to_copy,
      points_by_curve,
      [&](int curve, IndexRange points, Span<IndexRange> ranges_to_copy) {
        const IndexRange src_range = src_knots_by_curve[curve];
        const int order = orders[curve];
        const int leading_spans = order / 2;
        const int point_to_knot = -points.start() + src_range.start();
        const int point_to_span = point_to_knot + leading_spans;

        const int first_knot = ranges_to_copy.first().start() + point_to_knot;
        new_knots.extend(
            src_knots.slice(IndexRange::from_begin_size(first_knot, leading_spans + 1)));
        float last_knot = new_knots.last();
        for (IndexRange range : ranges_to_copy) {
          for (const int spans_left_knot : range.shift(point_to_span)) {
            last_knot += src_knots[spans_left_knot + 1] - src_knots[spans_left_knot];
            new_knots.append(last_knot);
          }
        }
        const int last_spans_left_knot = ranges_to_copy.last().last() + point_to_span + 1;
        last_knot += src_knots[last_spans_left_knot + 1] - src_knots[last_spans_left_knot];
        new_knots.append(last_knot);
      });
  dst_curves.nurbs_custom_knots_update_size();
  dst_curves.nurbs_custom_knots_for_write().copy_from(new_knots);
}

CurvesGeometry curves_copy_point_selection(const CurvesGeometry &curves,
                                           const IndexMask &points_to_copy,
                                           const AttributeFilter &attribute_filter)
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
        if (curves_to_copy.is_empty()) {
          return;
        }
        MutableSpan<int> new_curve_offsets = dst_curves.offsets_for_write();
        array_utils::gather(
            curve_point_counts.as_span(), curves_to_copy, new_curve_offsets.drop_back(1));
        offset_indices::accumulate_counts_to_offsets(new_curve_offsets);
      },
      [&]() {
        gather_attributes(curves.attributes(),
                          AttrDomain::Point,
                          AttrDomain::Point,
                          attribute_filter,
                          points_to_copy,
                          dst_curves.attributes_for_write());
        gather_attributes(curves.attributes(),
                          AttrDomain::Curve,
                          AttrDomain::Curve,
                          attribute_filter,
                          curves_to_copy,
                          dst_curves.attributes_for_write());
      });

  if (curves.nurbs_has_custom_knots()) {
    copy_point_selection_custom_knots(curves, points_to_copy, curve_point_counts, dst_curves);
  }

  if (dst_curves.curves_num() == curves.curves_num()) {
    dst_curves.runtime->type_counts = curves.runtime->type_counts;
  }
  else {
    dst_curves.remove_attributes_based_on_types();
  }

  return dst_curves;
}

void CurvesGeometry::remove_points(const IndexMask &points_to_delete,
                                   const AttributeFilter &attribute_filter)
{
  if (points_to_delete.is_empty()) {
    return;
  }
  if (points_to_delete.size() == this->points_num()) {
    this->resize(0, 0);
    this->update_curve_types();
    return;
  }
  IndexMaskMemory memory;
  const IndexMask points_to_copy = points_to_delete.complement(this->points_range(), memory);
  *this = curves_copy_point_selection(*this, points_to_copy, attribute_filter);
}

static void copy_curve_selection_custom_knots(const CurvesGeometry &curves,
                                              const IndexMask &curves_to_copy,
                                              CurvesGeometry &dst_curves)
{
  IndexMaskMemory memory;
  const IndexMask custom_knot_curves = curves.nurbs_custom_knot_curves(memory);
  const IndexMask custom_knot_curves_to_copy = IndexMask::from_intersection(
      curves_to_copy, custom_knot_curves, memory);

  Array<int> dst_knot_offsets_data(custom_knot_curves_to_copy.size() + 1, 0);

  const OffsetIndices<int> src_knots_by_curve = curves.nurbs_custom_knots_by_curve();
  const OffsetIndices<int> dst_knots_by_curve = offset_indices::gather_selected_offsets(
      src_knots_by_curve, custom_knot_curves_to_copy, dst_knot_offsets_data);

  dst_curves.nurbs_custom_knots_update_size();
  array_utils::gather_group_to_group(src_knots_by_curve,
                                     dst_knots_by_curve,
                                     custom_knot_curves_to_copy,
                                     curves.nurbs_custom_knots(),
                                     dst_curves.nurbs_custom_knots_for_write());
}

CurvesGeometry curves_copy_curve_selection(const CurvesGeometry &curves,
                                           const IndexMask &curves_to_copy,
                                           const AttributeFilter &attribute_filter)
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
                                   AttrDomain::Point,
                                   attribute_filter,
                                   points_by_curve,
                                   dst_points_by_curve,
                                   curves_to_copy,
                                   dst_attributes);

  gather_attributes(src_attributes,
                    AttrDomain::Curve,
                    AttrDomain::Curve,
                    attribute_filter,
                    curves_to_copy,
                    dst_attributes);

  if (curves.nurbs_has_custom_knots()) {
    copy_curve_selection_custom_knots(curves, curves_to_copy, dst_curves);
  }

  dst_curves.update_curve_types();
  dst_curves.remove_attributes_based_on_types();

  return dst_curves;
}

void CurvesGeometry::remove_curves(const IndexMask &curves_to_delete,
                                   const AttributeFilter &attribute_filter)
{
  if (curves_to_delete.is_empty()) {
    return;
  }
  if (curves_to_delete.size() == this->curves_num()) {
    this->resize(0, 0);
    this->update_curve_types();
    return;
  }
  IndexMaskMemory memory;
  const IndexMask curves_to_copy = curves_to_delete.complement(this->curves_range(), memory);
  *this = curves_copy_curve_selection(*this, curves_to_copy, attribute_filter);
}

static void reverse_custom_knots(MutableSpan<float> custom_knots)
{
  const float last = custom_knots.last();
  custom_knots.reverse();
  for (float &knot_value : custom_knots) {
    knot_value = last - knot_value;
  }
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

  attributes.foreach_attribute([&](const AttributeIter &iter) {
    if (iter.domain != AttrDomain::Point) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    if (bezier_handle_names.contains(iter.name)) {
      return;
    }

    GSpanAttributeWriter attribute = attributes.lookup_for_write_span(iter.name);
    attribute_math::convert_to_static_type(attribute.span.type(), [&](auto dummy) {
      using T = decltype(dummy);
      reverse_curve_point_data<T>(*this, curves_to_reverse, attribute.span.typed<T>());
    });
    attribute.finish();
    return;
  });

  if (this->nurbs_has_custom_knots()) {
    const OffsetIndices custom_knots_by_curve = this->nurbs_custom_knots_by_curve();
    MutableSpan<float> custom_knots = this->nurbs_custom_knots_for_write();
    curves_to_reverse.foreach_index(GrainSize(256), [&](const int64_t curve) {
      const IndexRange curve_knots = custom_knots_by_curve[curve];
      if (!custom_knots.is_empty()) {
        reverse_custom_knots(custom_knots.slice(curve_knots));
      }
    });
  }

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

CurvesGeometry curves_new_no_attributes(int point_num, int curve_num)
{
  CurvesGeometry curves(0, curve_num);
  curves.point_num = point_num;
  curves.attribute_storage.wrap().remove("position");
  return curves;
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
      new_varray = VArray<T>::from_container(std::move(values));
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
    new_varray = VArray<T>::from_container(std::move(values));
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
    return GVArray::from_single(varray.type(), this->attributes().domain_size(to), value);
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

AttributeAccessor CurvesGeometry::attributes() const
{
  return AttributeAccessor(this, curves::get_attribute_accessor_functions());
}

MutableAttributeAccessor CurvesGeometry::attributes_for_write()
{
  return MutableAttributeAccessor(this, curves::get_attribute_accessor_functions());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File reading/writing.
 * \{ */

void CurvesGeometry::blend_read(BlendDataReader &reader)
{
  this->runtime = MEM_new<blender::bke::CurvesGeometryRuntime>(__func__);

  CustomData_blend_read(&reader, &this->point_data, this->point_num);
  CustomData_blend_read(&reader, &this->curve_data_legacy, this->curve_num);
  this->attribute_storage.wrap().blend_read(reader);

  if (this->curve_offsets) {
    this->runtime->curve_offsets_sharing_info = BLO_read_shared(
        &reader, &this->curve_offsets, [&]() {
          BLO_read_int32_array(&reader, this->curve_num + 1, &this->curve_offsets);
          return implicit_sharing::info_for_mem_free(this->curve_offsets);
        });
  }

  BLO_read_struct_list(&reader, bDeformGroup, &this->vertex_group_names);

  if (this->custom_knot_num) {
    this->runtime->custom_knots_sharing_info = BLO_read_shared(
        &reader, &this->custom_knots, [&]() {
          BLO_read_float_array(&reader, this->custom_knot_num, &this->custom_knots);
          return implicit_sharing::info_for_mem_free(this->custom_knots);
        });
  }

  /* Recalculate curve type count cache that isn't saved in files. */
  this->update_curve_types();
}

CurvesGeometry::BlendWriteData::BlendWriteData(ResourceScope &scope)
    : scope(scope),
      point_layers(scope.construct<Vector<CustomDataLayer, 16>>()),
      curve_layers(scope.construct<Vector<CustomDataLayer, 16>>()),
      attribute_data(scope)
{
}

void CurvesGeometry::blend_write_prepare(CurvesGeometry::BlendWriteData &write_data)
{
  CustomData_reset(&this->curve_data_legacy);
  attribute_storage_blend_write_prepare(this->attribute_storage.wrap(), write_data.attribute_data);
  CustomData_blend_write_prepare(this->point_data,
                                 AttrDomain::Point,
                                 this->points_num(),
                                 write_data.point_layers,
                                 write_data.attribute_data);
  if (write_data.attribute_data.attributes.is_empty()) {
    this->attribute_storage.dna_attributes = nullptr;
    this->attribute_storage.dna_attributes_num = 0;
  }
  else {
    this->attribute_storage.dna_attributes = write_data.attribute_data.attributes.data();
    this->attribute_storage.dna_attributes_num = write_data.attribute_data.attributes.size();
  }
}

void CurvesGeometry::blend_write(BlendWriter &writer,
                                 ID &id,
                                 const CurvesGeometry::BlendWriteData &write_data)
{
  CustomData_blend_write(
      &writer, &this->point_data, write_data.point_layers, this->point_num, CD_MASK_ALL, &id);
  this->attribute_storage.wrap().blend_write(writer, write_data.attribute_data);

  if (this->curve_offsets) {
    BLO_write_shared(
        &writer,
        this->curve_offsets,
        sizeof(int) * (this->curve_num + 1),
        this->runtime->curve_offsets_sharing_info,
        [&]() { BLO_write_int32_array(&writer, this->curve_num + 1, this->curve_offsets); });
  }

  BKE_defbase_blend_write(&writer, &this->vertex_group_names);

  if (this->custom_knot_num) {
    BLO_write_shared(
        &writer,
        this->custom_knots,
        sizeof(float) * this->custom_knot_num,
        this->runtime->custom_knots_sharing_info,
        [&]() { BLO_write_float_array(&writer, this->custom_knot_num, this->custom_knots); });
  }
}

/** \} */

}  // namespace blender::bke
