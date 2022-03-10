/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_bounds.hh"

#include "DNA_curves_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"

namespace blender::bke {

static const std::string ATTR_POSITION = "position";
static const std::string ATTR_RADIUS = "radius";
static const std::string ATTR_CURVE_TYPE = "curve_type";
static const std::string ATTR_CYCLIC = "cyclic";

/* -------------------------------------------------------------------- */
/** \name Constructors/Destructor
 * \{ */

CurvesGeometry::CurvesGeometry() : CurvesGeometry(0, 0)
{
}

CurvesGeometry::CurvesGeometry(const int point_size, const int curve_size)
{
  this->point_size = point_size;
  this->curve_size = curve_size;
  CustomData_reset(&this->point_data);
  CustomData_reset(&this->curve_data);

  CustomData_add_layer_named(&this->point_data,
                             CD_PROP_FLOAT3,
                             CD_DEFAULT,
                             nullptr,
                             this->point_size,
                             ATTR_POSITION.c_str());

  this->curve_offsets = (int *)MEM_calloc_arrayN(this->curve_size + 1, sizeof(int), __func__);

  this->update_customdata_pointers();

  this->runtime = MEM_new<CurvesGeometryRuntime>(__func__);
}

/**
 * \note Expects `dst` to be initialized, since the original attributes must be freed.
 */
static void copy_curves_geometry(CurvesGeometry &dst, const CurvesGeometry &src)
{
  CustomData_free(&dst.point_data, dst.point_size);
  CustomData_free(&dst.curve_data, dst.curve_size);
  dst.point_size = src.point_size;
  dst.curve_size = src.curve_size;
  CustomData_copy(&src.point_data, &dst.point_data, CD_MASK_ALL, CD_DUPLICATE, dst.point_size);
  CustomData_copy(&src.curve_data, &dst.curve_data, CD_MASK_ALL, CD_DUPLICATE, dst.curve_size);

  MEM_SAFE_FREE(dst.curve_offsets);
  dst.curve_offsets = (int *)MEM_calloc_arrayN(dst.point_size + 1, sizeof(int), __func__);
  dst.offsets().copy_from(src.offsets());

  dst.tag_topology_changed();

  dst.update_customdata_pointers();
}

CurvesGeometry::CurvesGeometry(const CurvesGeometry &other)
    : CurvesGeometry(other.point_size, other.curve_size)
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

CurvesGeometry::~CurvesGeometry()
{
  CustomData_free(&this->point_data, this->point_size);
  CustomData_free(&this->curve_data, this->curve_size);
  MEM_SAFE_FREE(this->curve_offsets);
  MEM_delete(this->runtime);
  this->runtime = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Accessors
 * \{ */

int CurvesGeometry::points_size() const
{
  return this->point_size;
}
int CurvesGeometry::curves_size() const
{
  return this->curve_size;
}
IndexRange CurvesGeometry::points_range() const
{
  return IndexRange(this->points_size());
}
IndexRange CurvesGeometry::curves_range() const
{
  return IndexRange(this->curves_size());
}

int CurvesGeometry::evaluated_points_size() const
{
  /* TODO: Implement when there are evaluated points. */
  return 0;
}

IndexRange CurvesGeometry::range_for_curve(const int index) const
{
  const int offset = this->curve_offsets[index];
  const int offset_next = this->curve_offsets[index + 1];
  return {offset, offset_next - offset};
}

static int domain_size(const CurvesGeometry &curves, const AttributeDomain domain)
{
  return domain == ATTR_DOMAIN_POINT ? curves.points_size() : curves.curves_size();
}

static CustomData &domain_custom_data(CurvesGeometry &curves, const AttributeDomain domain)
{
  return domain == ATTR_DOMAIN_POINT ? curves.point_data : curves.curve_data;
}

static const CustomData &domain_custom_data(const CurvesGeometry &curves,
                                            const AttributeDomain domain)
{
  return domain == ATTR_DOMAIN_POINT ? curves.point_data : curves.curve_data;
}

template<typename T>
static VArray<T> get_varray_attribute(const CurvesGeometry &curves,
                                      const AttributeDomain domain,
                                      const StringRefNull name,
                                      const T default_value)
{
  const int size = domain_size(curves, domain);
  const CustomDataType type = cpp_type_to_custom_data_type(CPPType::get<T>());
  const CustomData &custom_data = domain_custom_data(curves, domain);

  const T *data = (const T *)CustomData_get_layer_named(&custom_data, type, name.c_str());
  if (data != nullptr) {
    return VArray<T>::ForSpan(Span<T>(data, size));
  }
  return VArray<T>::ForSingle(default_value, size);
}

template<typename T>
static MutableSpan<T> get_mutable_attribute(CurvesGeometry &curves,
                                            const AttributeDomain domain,
                                            const StringRefNull name)
{
  const int size = domain_size(curves, domain);
  const CustomDataType type = cpp_type_to_custom_data_type(CPPType::get<T>());
  CustomData &custom_data = domain_custom_data(curves, domain);

  T *data = (T *)CustomData_duplicate_referenced_layer_named(
      &custom_data, type, name.c_str(), size);
  if (data != nullptr) {
    return {data, size};
  }
  data = (T *)CustomData_add_layer_named(
      &custom_data, type, CD_CALLOC, nullptr, size, name.c_str());
  return {data, size};
}

VArray<int8_t> CurvesGeometry::curve_types() const
{
  return get_varray_attribute<int8_t>(
      *this, ATTR_DOMAIN_CURVE, ATTR_CURVE_TYPE, CURVE_TYPE_CATMULL_ROM);
}

MutableSpan<int8_t> CurvesGeometry::curve_types()
{
  return get_mutable_attribute<int8_t>(*this, ATTR_DOMAIN_CURVE, ATTR_CURVE_TYPE);
}

MutableSpan<float3> CurvesGeometry::positions()
{
  this->position = (float(*)[3])CustomData_duplicate_referenced_layer_named(
      &this->point_data, CD_PROP_FLOAT3, ATTR_POSITION.c_str(), this->point_size);
  return {(float3 *)this->position, this->point_size};
}
Span<float3> CurvesGeometry::positions() const
{
  return {(const float3 *)this->position, this->point_size};
}

MutableSpan<int> CurvesGeometry::offsets()
{
  return {this->curve_offsets, this->curve_size + 1};
}
Span<int> CurvesGeometry::offsets() const
{
  return {this->curve_offsets, this->curve_size + 1};
}

VArray<bool> CurvesGeometry::cyclic() const
{
  return get_varray_attribute<bool>(*this, ATTR_DOMAIN_CURVE, ATTR_CYCLIC, false);
}

MutableSpan<bool> CurvesGeometry::cyclic()
{
  return get_mutable_attribute<bool>(*this, ATTR_DOMAIN_CURVE, ATTR_CYCLIC);
}

void CurvesGeometry::resize(const int point_size, const int curve_size)
{
  if (point_size != this->point_size) {
    CustomData_realloc(&this->point_data, point_size);
    this->point_size = point_size;
  }
  if (curve_size != this->curve_size) {
    CustomData_realloc(&this->curve_data, curve_size);
    this->curve_size = curve_size;
    this->curve_offsets = (int *)MEM_reallocN(this->curve_offsets, sizeof(int) * (curve_size + 1));
  }
  this->tag_topology_changed();
  this->update_customdata_pointers();
}

void CurvesGeometry::tag_positions_changed()
{
  this->runtime->position_cache_dirty = true;
  this->runtime->tangent_cache_dirty = true;
  this->runtime->normal_cache_dirty = true;
}
void CurvesGeometry::tag_topology_changed()
{
  this->runtime->position_cache_dirty = true;
  this->runtime->tangent_cache_dirty = true;
  this->runtime->normal_cache_dirty = true;
}
void CurvesGeometry::tag_normals_changed()
{
  this->runtime->normal_cache_dirty = true;
}

void CurvesGeometry::translate(const float3 &translation)
{
  MutableSpan<float3> positions = this->positions();
  threading::parallel_for(positions.index_range(), 2048, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position += translation;
    }
  });
}

void CurvesGeometry::transform(const float4x4 &matrix)
{
  MutableSpan<float3> positions = this->positions();
  threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position = matrix * position;
    }
  });
}

static std::optional<bounds::MinMaxResult<float3>> curves_bounds(const CurvesGeometry &curves)
{
  Span<float3> positions = curves.positions();
  if (curves.radius) {
    Span<float> radii{curves.radius, curves.points_size()};
    return bounds::min_max_with_radii(positions, radii);
  }
  return bounds::min_max(positions);
}

bool CurvesGeometry::bounds_min_max(float3 &min, float3 &max) const
{
  const std::optional<bounds::MinMaxResult<float3>> bounds = curves_bounds(*this);
  if (!bounds) {
    return false;
  }
  min = math::min(bounds->min, min);
  max = math::max(bounds->max, max);
  return true;
}

void CurvesGeometry::update_customdata_pointers()
{
  this->position = (float(*)[3])CustomData_get_layer_named(
      &this->point_data, CD_PROP_FLOAT3, ATTR_POSITION.c_str());
  this->radius = (float *)CustomData_get_layer_named(
      &this->point_data, CD_PROP_FLOAT, ATTR_RADIUS.c_str());
  this->curve_type = (int8_t *)CustomData_get_layer_named(
      &this->point_data, CD_PROP_INT8, ATTR_CURVE_TYPE.c_str());
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
  for (const int i_curve : IndexRange(curves.curves_size())) {
    for (const int i_point : curves.range_for_curve(i_curve)) {
      mixer.mix_in(i_curve, old_values[i_point]);
    }
  }
  mixer.finalize();
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
  r_values.fill(true);
  for (const int i_curve : IndexRange(curves.curves_size())) {
    for (const int i_point : curves.range_for_curve(i_curve)) {
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
      Array<T> values(curves.curves_size());
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
  for (const int i_curve : IndexRange(curves.curves_size())) {
    r_values.slice(curves.range_for_curve(i_curve)).fill(old_values[i_curve]);
  }
}

static GVArray adapt_curve_domain_curve_to_point(const CurvesGeometry &curves,
                                                 const GVArray &varray)
{
  GVArray new_varray;
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    Array<T> values(curves.points_size());
    adapt_curve_domain_curve_to_point_impl<T>(curves, varray.typed<T>(), values);
    new_varray = VArray<T>::ForContainer(std::move(values));
  });
  return new_varray;
}

fn::GVArray CurvesGeometry::adapt_domain(const fn::GVArray &varray,
                                         const AttributeDomain from,
                                         const AttributeDomain to) const
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

}  // namespace blender::bke
