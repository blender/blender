/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BKE_spline.hh"

#include "BKE_attribute_access.hh"
#include "BKE_attribute_math.hh"
#include "BKE_geometry_set.hh"

#include "attribute_access_intern.hh"

using blender::fn::GMutableSpan;
using blender::fn::GSpan;
using blender::fn::GVArray_For_GSpan;
using blender::fn::GVArray_GSpan;
using blender::fn::GVMutableArray_For_GMutableSpan;

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

CurveComponent::CurveComponent() : GeometryComponent(GEO_COMPONENT_TYPE_CURVE)
{
}

CurveComponent::~CurveComponent()
{
  this->clear();
}

GeometryComponent *CurveComponent::copy() const
{
  CurveComponent *new_component = new CurveComponent();
  if (curve_ != nullptr) {
    new_component->curve_ = new CurveEval(*curve_);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return new_component;
}

void CurveComponent::clear()
{
  BLI_assert(this->is_mutable());
  if (curve_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      delete curve_;
    }
    curve_ = nullptr;
  }
}

bool CurveComponent::has_curve() const
{
  return curve_ != nullptr;
}

/* Clear the component and replace it with the new curve. */
void CurveComponent::replace(CurveEval *curve, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  curve_ = curve;
  ownership_ = ownership;
}

CurveEval *CurveComponent::release()
{
  BLI_assert(this->is_mutable());
  CurveEval *curve = curve_;
  curve_ = nullptr;
  return curve;
}

const CurveEval *CurveComponent::get_for_read() const
{
  return curve_;
}

CurveEval *CurveComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    curve_ = new CurveEval(*curve_);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return curve_;
}

bool CurveComponent::is_empty() const
{
  return curve_ == nullptr;
}

bool CurveComponent::owns_direct_data() const
{
  return ownership_ == GeometryOwnershipType::Owned;
}

void CurveComponent::ensure_owns_direct_data()
{
  BLI_assert(this->is_mutable());
  if (ownership_ != GeometryOwnershipType::Owned) {
    curve_ = new CurveEval(*curve_);
    ownership_ = GeometryOwnershipType::Owned;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute Access Helper Functions
 * \{ */

int CurveComponent::attribute_domain_size(const AttributeDomain domain) const
{
  if (curve_ == nullptr) {
    return 0;
  }
  if (domain == ATTR_DOMAIN_POINT) {
    int total = 0;
    for (const SplinePtr &spline : curve_->splines()) {
      total += spline->size();
    }
    return total;
  }
  if (domain == ATTR_DOMAIN_CURVE) {
    return curve_->splines().size();
  }
  return 0;
}

static CurveEval *get_curve_from_component_for_write(GeometryComponent &component)
{
  BLI_assert(component.type() == GEO_COMPONENT_TYPE_CURVE);
  CurveComponent &curve_component = static_cast<CurveComponent &>(component);
  return curve_component.get_for_write();
}

static const CurveEval *get_curve_from_component_for_read(const GeometryComponent &component)
{
  BLI_assert(component.type() == GEO_COMPONENT_TYPE_CURVE);
  const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
  return curve_component.get_for_read();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Builtin Spline Attributes
 *
 * Attributes with a value for every spline, stored contiguously or in every spline separately.
 * \{ */

namespace blender::bke {

class BuiltinSplineAttributeProvider final : public BuiltinAttributeProvider {
  using AsReadAttribute = GVArrayPtr (*)(const CurveEval &data);
  using AsWriteAttribute = GVMutableArrayPtr (*)(CurveEval &data);
  const AsReadAttribute as_read_attribute_;
  const AsWriteAttribute as_write_attribute_;

 public:
  BuiltinSplineAttributeProvider(std::string attribute_name,
                                 const CustomDataType attribute_type,
                                 const WritableEnum writable,
                                 const AsReadAttribute as_read_attribute,
                                 const AsWriteAttribute as_write_attribute)
      : BuiltinAttributeProvider(std::move(attribute_name),
                                 ATTR_DOMAIN_CURVE,
                                 attribute_type,
                                 BuiltinAttributeProvider::NonCreatable,
                                 writable,
                                 BuiltinAttributeProvider::NonDeletable),
        as_read_attribute_(as_read_attribute),
        as_write_attribute_(as_write_attribute)
  {
  }

  GVArrayPtr try_get_for_read(const GeometryComponent &component) const final
  {
    const CurveEval *curve = get_curve_from_component_for_read(component);
    if (curve == nullptr) {
      return {};
    }
    return as_read_attribute_(*curve);
  }

  GVMutableArrayPtr try_get_for_write(GeometryComponent &component) const final
  {
    if (writable_ != Writable) {
      return {};
    }
    CurveEval *curve = get_curve_from_component_for_write(component);
    if (curve == nullptr) {
      return {};
    }
    return as_write_attribute_(*curve);
  }

  bool try_delete(GeometryComponent &UNUSED(component)) const final
  {
    return false;
  }

  bool try_create(GeometryComponent &UNUSED(component),
                  const AttributeInit &UNUSED(initializer)) const final
  {
    return false;
  }

  bool exists(const GeometryComponent &component) const final
  {
    return component.attribute_domain_size(ATTR_DOMAIN_CURVE) != 0;
  }
};

static int get_spline_resolution(const SplinePtr &spline)
{
  if (const BezierSpline *bezier_spline = dynamic_cast<const BezierSpline *>(spline.get())) {
    return bezier_spline->resolution();
  }
  if (const NURBSpline *nurb_spline = dynamic_cast<const NURBSpline *>(spline.get())) {
    return nurb_spline->resolution();
  }
  return 1;
}

static void set_spline_resolution(SplinePtr &spline, const int resolution)
{
  if (BezierSpline *bezier_spline = dynamic_cast<BezierSpline *>(spline.get())) {
    bezier_spline->set_resolution(std::max(resolution, 1));
  }
  if (NURBSpline *nurb_spline = dynamic_cast<NURBSpline *>(spline.get())) {
    nurb_spline->set_resolution(std::max(resolution, 1));
  }
}

static GVArrayPtr make_resolution_read_attribute(const CurveEval &curve)
{
  return std::make_unique<fn::GVArray_For_DerivedSpan<SplinePtr, int, get_spline_resolution>>(
      curve.splines());
}

static GVMutableArrayPtr make_resolution_write_attribute(CurveEval &curve)
{
  return std::make_unique<fn::GVMutableArray_For_DerivedSpan<SplinePtr,
                                                             int,
                                                             get_spline_resolution,
                                                             set_spline_resolution>>(
      curve.splines());
}

static bool get_cyclic_value(const SplinePtr &spline)
{
  return spline->is_cyclic();
}

static void set_cyclic_value(SplinePtr &spline, const bool value)
{
  if (spline->is_cyclic() != value) {
    spline->set_cyclic(value);
    spline->mark_cache_invalid();
  }
}

static GVArrayPtr make_cyclic_read_attribute(const CurveEval &curve)
{
  return std::make_unique<fn::GVArray_For_DerivedSpan<SplinePtr, bool, get_cyclic_value>>(
      curve.splines());
}

static GVMutableArrayPtr make_cyclic_write_attribute(CurveEval &curve)
{
  return std::make_unique<
      fn::GVMutableArray_For_DerivedSpan<SplinePtr, bool, get_cyclic_value, set_cyclic_value>>(
      curve.splines());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Builtin Control Point Attributes
 *
 * Attributes with a value for every control point. Most of the complexity here is due to the fact
 * that we must provide access to the attribute data as if it was a contiguous array when it is
 * really stored separately on each spline. That will be inherently rather slow, but these virtual
 * array implementations try to make it workable in common situations.
 * \{ */

namespace {
struct PointIndices {
  int spline_index;
  int point_index;
};
}  // namespace
static PointIndices lookup_point_indices(Span<int> offsets, const int index)
{
  const int spline_index = std::upper_bound(offsets.begin(), offsets.end(), index) -
                           offsets.begin() - 1;
  const int index_in_spline = index - offsets[spline_index];
  return {spline_index, index_in_spline};
}

template<typename T>
static void point_attribute_materialize(Span<Span<T>> data,
                                        Span<int> offsets,
                                        const IndexMask mask,
                                        MutableSpan<T> r_span)
{
  const int total_size = offsets.last();
  if (mask.is_range() && mask.as_range() == IndexRange(total_size)) {
    for (const int spline_index : data.index_range()) {
      const int offset = offsets[spline_index];
      const int next_offset = offsets[spline_index + 1];
      initialized_copy_n(data[spline_index].data(), next_offset - offset, r_span.data() + offset);
    }
  }
  else {
    int spline_index = 0;
    for (const int i : r_span.index_range()) {
      const int dst_index = mask[i];

      while (offsets[spline_index] < dst_index) {
        spline_index++;
      }

      const int index_in_spline = dst_index - offsets[spline_index];
      r_span[dst_index] = data[spline_index][index_in_spline];
    }
  }
}

template<typename T>
static void point_attribute_materialize_to_uninitialized(Span<Span<T>> data,
                                                         Span<int> offsets,
                                                         const IndexMask mask,
                                                         MutableSpan<T> r_span)
{
  T *dst = r_span.data();
  const int total_size = offsets.last();
  if (mask.is_range() && mask.as_range() == IndexRange(total_size)) {
    for (const int spline_index : data.index_range()) {
      const int offset = offsets[spline_index];
      const int next_offset = offsets[spline_index + 1];
      uninitialized_copy_n(data[spline_index].data(), next_offset - offset, dst + offset);
    }
  }
  else {
    int spline_index = 0;
    for (const int i : r_span.index_range()) {
      const int dst_index = mask[i];

      while (offsets[spline_index] < dst_index) {
        spline_index++;
      }

      const int index_in_spline = dst_index - offsets[spline_index];
      new (dst + dst_index) T(data[spline_index][index_in_spline]);
    }
  }
}

/**
 * Virtual array for any control point data accessed with spans and an offset array.
 */
template<typename T> class VArray_For_SplinePoints : public VArray<T> {
 private:
  const Array<Span<T>> data_;
  Array<int> offsets_;

 public:
  VArray_For_SplinePoints(Array<Span<T>> data, Array<int> offsets)
      : VArray<T>(offsets.last()), data_(std::move(data)), offsets_(std::move(offsets))
  {
  }

  T get_impl(const int64_t index) const final
  {
    const PointIndices indices = lookup_point_indices(offsets_, index);
    return data_[indices.spline_index][indices.point_index];
  }

  void materialize_impl(const IndexMask mask, MutableSpan<T> r_span) const final
  {
    point_attribute_materialize(data_.as_span(), offsets_, mask, r_span);
  }

  void materialize_to_uninitialized_impl(const IndexMask mask, MutableSpan<T> r_span) const final
  {
    point_attribute_materialize_to_uninitialized(data_.as_span(), offsets_, mask, r_span);
  }
};

/**
 * Mutable virtual array for any control point data accessed with spans and an offset array.
 */
template<typename T> class VMutableArray_For_SplinePoints final : public VMutableArray<T> {
 private:
  Array<MutableSpan<T>> data_;
  Array<int> offsets_;

 public:
  VMutableArray_For_SplinePoints(Array<MutableSpan<T>> data, Array<int> offsets)
      : VMutableArray<T>(offsets.last()), data_(std::move(data)), offsets_(std::move(offsets))
  {
  }

  T get_impl(const int64_t index) const final
  {
    const PointIndices indices = lookup_point_indices(offsets_, index);
    return data_[indices.spline_index][indices.point_index];
  }

  void set_impl(const int64_t index, T value) final
  {
    const PointIndices indices = lookup_point_indices(offsets_, index);
    data_[indices.spline_index][indices.point_index] = value;
  }

  void set_all_impl(Span<T> src) final
  {
    for (const int spline_index : data_.index_range()) {
      const int offset = offsets_[spline_index];
      const int next_offsets = offsets_[spline_index + 1];
      data_[spline_index].copy_from(src.slice(offset, next_offsets - offset));
    }
  }

  void materialize_impl(const IndexMask mask, MutableSpan<T> r_span) const final
  {
    point_attribute_materialize({(Span<T> *)data_.data(), data_.size()}, offsets_, mask, r_span);
  }

  void materialize_to_uninitialized_impl(const IndexMask mask, MutableSpan<T> r_span) const final
  {
    point_attribute_materialize_to_uninitialized(
        {(Span<T> *)data_.data(), data_.size()}, offsets_, mask, r_span);
  }
};

template<typename T> GVArrayPtr point_data_gvarray(Array<Span<T>> spans, Array<int> offsets)
{
  return std::make_unique<fn::GVArray_For_EmbeddedVArray<T, VArray_For_SplinePoints<T>>>(
      offsets.last(), std::move(spans), std::move(offsets));
}

template<typename T>
GVMutableArrayPtr point_data_gvarray(Array<MutableSpan<T>> spans, Array<int> offsets)
{
  return std::make_unique<
      fn::GVMutableArray_For_EmbeddedVMutableArray<T, VMutableArray_For_SplinePoints<T>>>(
      offsets.last(), std::move(spans), std::move(offsets));
}

/**
 * Virtual array implementation specifically for control point positions. This is only needed for
 * Bezier splines, where adjusting the position also requires adjusting handle positions depending
 * on handle types. We pay a small price for this when other spline types are mixed with Bezier.
 *
 * \note There is no need to check the handle type to avoid changing auto handles, since
 * retrieving write access to the position data will mark them for recomputation anyway.
 */
class VMutableArray_For_SplinePosition final : public VMutableArray<float3> {
 private:
  MutableSpan<SplinePtr> splines_;
  Array<int> offsets_;

 public:
  VMutableArray_For_SplinePosition(MutableSpan<SplinePtr> splines, Array<int> offsets)
      : VMutableArray<float3>(offsets.last()), splines_(splines), offsets_(std::move(offsets))
  {
  }

  float3 get_impl(const int64_t index) const final
  {
    const PointIndices indices = lookup_point_indices(offsets_, index);
    return splines_[indices.spline_index]->positions()[indices.point_index];
  }

  void set_impl(const int64_t index, float3 value) final
  {
    const PointIndices indices = lookup_point_indices(offsets_, index);
    Spline &spline = *splines_[indices.spline_index];
    if (BezierSpline *bezier_spline = dynamic_cast<BezierSpline *>(&spline)) {
      const float3 delta = value - bezier_spline->positions()[indices.point_index];
      bezier_spline->handle_positions_left()[indices.point_index] += delta;
      bezier_spline->handle_positions_right()[indices.point_index] += delta;
      bezier_spline->positions()[indices.point_index] = value;
    }
    else {
      spline.positions()[indices.point_index] = value;
    }
  }

  void set_all_impl(Span<float3> src) final
  {
    for (const int spline_index : splines_.index_range()) {
      Spline &spline = *splines_[spline_index];
      const int offset = offsets_[spline_index];
      const int next_offset = offsets_[spline_index + 1];
      if (BezierSpline *bezier_spline = dynamic_cast<BezierSpline *>(&spline)) {
        MutableSpan<float3> positions = bezier_spline->positions();
        MutableSpan<float3> handle_positions_left = bezier_spline->handle_positions_left();
        MutableSpan<float3> handle_positions_right = bezier_spline->handle_positions_right();
        for (const int i : IndexRange(next_offset - offset)) {
          const float3 delta = src[offset + i] - positions[i];
          handle_positions_left[i] += delta;
          handle_positions_right[i] += delta;
          positions[i] = src[offset + i];
        }
      }
      else {
        spline.positions().copy_from(src.slice(offset, next_offset - offset));
      }
    }
  }

  /** Utility so we can pass positions to the materialize functions above. */
  Array<Span<float3>> get_position_spans() const
  {
    Array<Span<float3>> spans(splines_.size());
    for (const int i : spans.index_range()) {
      spans[i] = splines_[i]->positions();
    }
    return spans;
  }

  void materialize_impl(const IndexMask mask, MutableSpan<float3> r_span) const final
  {
    Array<Span<float3>> spans = this->get_position_spans();
    point_attribute_materialize(spans.as_span(), offsets_, mask, r_span);
  }

  void materialize_to_uninitialized_impl(const IndexMask mask,
                                         MutableSpan<float3> r_span) const final
  {
    Array<Span<float3>> spans = this->get_position_spans();
    point_attribute_materialize_to_uninitialized(spans.as_span(), offsets_, mask, r_span);
  }
};

/**
 * Provider for any builtin control point attribute that doesn't need
 * special handling like access to other arrays in the spline.
 */
template<typename T> class BuiltinPointAttributeProvider : public BuiltinAttributeProvider {
 protected:
  using GetSpan = Span<T> (*)(const Spline &spline);
  using GetMutableSpan = MutableSpan<T> (*)(Spline &spline);
  using UpdateOnWrite = void (*)(Spline &spline);
  const GetSpan get_span_;
  const GetMutableSpan get_mutable_span_;
  const UpdateOnWrite update_on_write_;

 public:
  BuiltinPointAttributeProvider(std::string attribute_name,
                                const WritableEnum writable,
                                const GetSpan get_span,
                                const GetMutableSpan get_mutable_span,
                                const UpdateOnWrite update_on_write)
      : BuiltinAttributeProvider(std::move(attribute_name),
                                 ATTR_DOMAIN_POINT,
                                 bke::cpp_type_to_custom_data_type(CPPType::get<T>()),
                                 BuiltinAttributeProvider::NonCreatable,
                                 writable,
                                 BuiltinAttributeProvider::NonDeletable),
        get_span_(get_span),
        get_mutable_span_(get_mutable_span),
        update_on_write_(update_on_write)
  {
  }

  GVArrayPtr try_get_for_read(const GeometryComponent &component) const override
  {
    const CurveEval *curve = get_curve_from_component_for_read(component);
    if (curve == nullptr) {
      return {};
    }

    Span<SplinePtr> splines = curve->splines();
    if (splines.size() == 1) {
      return std::make_unique<fn::GVArray_For_GSpan>(get_span_(*splines.first()));
    }

    Array<int> offsets = curve->control_point_offsets();
    Array<Span<T>> spans(splines.size());
    for (const int i : splines.index_range()) {
      spans[i] = get_span_(*splines[i]);
    }

    return point_data_gvarray(spans, offsets);
  }

  GVMutableArrayPtr try_get_for_write(GeometryComponent &component) const override
  {
    CurveEval *curve = get_curve_from_component_for_write(component);
    if (curve == nullptr) {
      return {};
    }

    MutableSpan<SplinePtr> splines = curve->splines();
    if (splines.size() == 1) {
      return std::make_unique<fn::GVMutableArray_For_GMutableSpan>(
          get_mutable_span_(*splines.first()));
    }

    Array<int> offsets = curve->control_point_offsets();
    Array<MutableSpan<T>> spans(splines.size());
    for (const int i : splines.index_range()) {
      spans[i] = get_mutable_span_(*splines[i]);
      if (update_on_write_) {
        update_on_write_(*splines[i]);
      }
    }

    return point_data_gvarray(spans, offsets);
  }

  bool try_delete(GeometryComponent &UNUSED(component)) const final
  {
    return false;
  }

  bool try_create(GeometryComponent &UNUSED(component),
                  const AttributeInit &UNUSED(initializer)) const final
  {
    return false;
  }

  bool exists(const GeometryComponent &component) const final
  {
    return component.attribute_domain_size(ATTR_DOMAIN_POINT) != 0;
  }
};

/**
 * Special attribute provider for the position attribute. Keeping this separate means we don't
 * need to make #BuiltinPointAttributeProvider overly generic, and the special handling for the
 * positions is more clear.
 */
class PositionAttributeProvider final : public BuiltinPointAttributeProvider<float3> {
 public:
  PositionAttributeProvider()
      : BuiltinPointAttributeProvider(
            "position",
            BuiltinAttributeProvider::Writable,
            [](const Spline &spline) { return spline.positions(); },
            [](Spline &spline) { return spline.positions(); },
            [](Spline &spline) { spline.mark_cache_invalid(); })
  {
  }

  GVMutableArrayPtr try_get_for_write(GeometryComponent &component) const final
  {
    CurveEval *curve = get_curve_from_component_for_write(component);
    if (curve == nullptr) {
      return {};
    }

    bool curve_has_bezier_spline = false;
    for (SplinePtr &spline : curve->splines()) {
      if (spline->type() == Spline::Type::Bezier) {
        curve_has_bezier_spline = true;
        break;
      }
    }

    /* Use the regular position virtual array when there aren't any Bezier splines
     * to avoid the overhead of checking the spline type for every point. */
    if (!curve_has_bezier_spline) {
      return BuiltinPointAttributeProvider<float3>::try_get_for_write(component);
    }

    /* Changing the positions requires recalculation of cached evaluated data in many cases.
     * This could set more specific flags in the future to avoid unnecessary recomputation. */
    for (SplinePtr &spline : curve->splines()) {
      spline->mark_cache_invalid();
    }

    Array<int> offsets = curve->control_point_offsets();
    return std::make_unique<
        fn::GVMutableArray_For_EmbeddedVMutableArray<float3, VMutableArray_For_SplinePosition>>(
        offsets.last(), curve->splines(), std::move(offsets));
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dynamic Control Point Attributes
 *
 * The dynamic control point attribute implementation is very similar to the builtin attribute
 * implementation-- it uses the same virtual array types. In order to work, this code depends on
 * the fact that all a curve's splines will have the same attributes and they all have the same
 * type.
 * \{ */

class DynamicPointAttributeProvider final : public DynamicAttributesProvider {
 private:
  static constexpr uint64_t supported_types_mask = CD_MASK_PROP_FLOAT | CD_MASK_PROP_FLOAT2 |
                                                   CD_MASK_PROP_FLOAT3 | CD_MASK_PROP_INT32 |
                                                   CD_MASK_PROP_COLOR | CD_MASK_PROP_BOOL;

 public:
  ReadAttributeLookup try_get_for_read(const GeometryComponent &component,
                                       const StringRef attribute_name) const final
  {
    const CurveEval *curve = get_curve_from_component_for_read(component);
    if (curve == nullptr || curve->splines().size() == 0) {
      return {};
    }

    Span<SplinePtr> splines = curve->splines();
    Vector<GSpan> spans; /* GSpan has no default constructor. */
    spans.reserve(splines.size());
    std::optional<GSpan> first_span = splines[0]->attributes.get_for_read(attribute_name);
    if (!first_span) {
      return {};
    }
    spans.append(*first_span);
    for (const int i : IndexRange(1, splines.size() - 1)) {
      std::optional<GSpan> span = splines[i]->attributes.get_for_read(attribute_name);
      if (!span) {
        /* All splines should have the same set of data layers. It would be possible to recover
         * here and return partial data instead, but that would add a lot of complexity for a
         * situation we don't even expect to encounter. */
        BLI_assert_unreachable();
        return {};
      }
      if (span->type() != spans.last().type()) {
        /* Data layer types on separate splines do not match. */
        BLI_assert_unreachable();
        return {};
      }
      spans.append(*span);
    }

    /* First check for the simpler situation when we can return a simpler span virtual array. */
    if (spans.size() == 1) {
      return {std::make_unique<GVArray_For_GSpan>(spans.first()), ATTR_DOMAIN_POINT};
    }

    ReadAttributeLookup attribute = {};
    Array<int> offsets = curve->control_point_offsets();
    attribute_math::convert_to_static_type(spans[0].type(), [&](auto dummy) {
      using T = decltype(dummy);
      Array<Span<T>> data(splines.size());
      for (const int i : splines.index_range()) {
        data[i] = spans[i].typed<T>();
        BLI_assert(data[i].data() != nullptr);
      }
      attribute = {point_data_gvarray(data, offsets), ATTR_DOMAIN_POINT};
    });
    return attribute;
  }

  /* This function is almost the same as #try_get_for_read, but without const. */
  WriteAttributeLookup try_get_for_write(GeometryComponent &component,
                                         const StringRef attribute_name) const final
  {
    CurveEval *curve = get_curve_from_component_for_write(component);
    if (curve == nullptr || curve->splines().size() == 0) {
      return {};
    }

    MutableSpan<SplinePtr> splines = curve->splines();
    Vector<GMutableSpan> spans; /* GMutableSpan has no default constructor. */
    spans.reserve(splines.size());
    std::optional<GMutableSpan> first_span = splines[0]->attributes.get_for_write(attribute_name);
    if (!first_span) {
      return {};
    }
    spans.append(*first_span);
    for (const int i : IndexRange(1, splines.size() - 1)) {
      std::optional<GMutableSpan> span = splines[i]->attributes.get_for_write(attribute_name);
      if (!span) {
        /* All splines should have the same set of data layers. It would be possible to recover
         * here and return partial data instead, but that would add a lot of complexity for a
         * situation we don't even expect to encounter. */
        BLI_assert_unreachable();
        return {};
      }
      if (span->type() != spans.last().type()) {
        /* Data layer types on separate splines do not match. */
        BLI_assert_unreachable();
        return {};
      }
      spans.append(*span);
    }

    /* First check for the simpler situation when we can return a simpler span virtual array. */
    if (spans.size() == 1) {
      return {std::make_unique<GVMutableArray_For_GMutableSpan>(spans.first()), ATTR_DOMAIN_POINT};
    }

    WriteAttributeLookup attribute = {};
    Array<int> offsets = curve->control_point_offsets();
    attribute_math::convert_to_static_type(spans[0].type(), [&](auto dummy) {
      using T = decltype(dummy);
      Array<MutableSpan<T>> data(splines.size());
      for (const int i : splines.index_range()) {
        data[i] = spans[i].typed<T>();
        BLI_assert(data[i].data() != nullptr);
      }
      attribute = {point_data_gvarray(data, offsets), ATTR_DOMAIN_POINT};
    });
    return attribute;
  }

  bool try_delete(GeometryComponent &component, const StringRef attribute_name) const final
  {
    CurveEval *curve = get_curve_from_component_for_write(component);
    if (curve == nullptr) {
      return false;
    }

    bool layer_freed = false;
    for (SplinePtr &spline : curve->splines()) {
      spline->attributes.remove(attribute_name);
    }
    return layer_freed;
  }

  static GVArrayPtr varray_from_initializer(const AttributeInit &initializer,
                                            const CustomDataType data_type,
                                            const int total_size)
  {
    switch (initializer.type) {
      case AttributeInit::Type::Default:
        /* This function shouldn't be called in this case, since there
         * is no need to copy anything to the new custom data array. */
        BLI_assert_unreachable();
        return {};
      case AttributeInit::Type::VArray:
        return static_cast<const AttributeInitVArray &>(initializer).varray->shallow_copy();
      case AttributeInit::Type::MoveArray:
        return std::make_unique<fn::GVArray_For_GSpan>(
            GSpan(*bke::custom_data_type_to_cpp_type(data_type),
                  static_cast<const AttributeInitMove &>(initializer).data,
                  total_size));
    }
    BLI_assert_unreachable();
    return {};
  }

  bool try_create(GeometryComponent &component,
                  const StringRef attribute_name,
                  const AttributeDomain domain,
                  const CustomDataType data_type,
                  const AttributeInit &initializer) const final
  {
    BLI_assert(this->type_is_supported(data_type));
    if (domain != ATTR_DOMAIN_POINT) {
      return false;
    }
    CurveEval *curve = get_curve_from_component_for_write(component);
    if (curve == nullptr || curve->splines().size() == 0) {
      return false;
    }

    MutableSpan<SplinePtr> splines = curve->splines();

    /* First check the one case that allows us to avoid copying the input data. */
    if (splines.size() == 1 && initializer.type == AttributeInit::Type::MoveArray) {
      void *source_data = static_cast<const AttributeInitMove &>(initializer).data;
      if (!splines[0]->attributes.create_by_move(attribute_name, data_type, source_data)) {
        MEM_freeN(source_data);
        return false;
      }
      return true;
    }

    /* Otherwise just create a custom data layer on each of the splines. */
    for (const int i : splines.index_range()) {
      if (!splines[i]->attributes.create(attribute_name, data_type)) {
        /* If attribute creation fails on one of the splines, we cannot leave the custom data
         * layers in the previous splines around, so delete them before returning. However,
         * this is not an expected case. */
        BLI_assert_unreachable();
        return false;
      }
    }

    /* With a default initializer type, we can keep the values at their initial values. */
    if (initializer.type == AttributeInit::Type::Default) {
      return true;
    }

    WriteAttributeLookup write_attribute = this->try_get_for_write(component, attribute_name);
    /* We just created the attribute, it should exist. */
    BLI_assert(write_attribute);

    const int total_size = curve->control_point_offsets().last();
    GVArrayPtr source_varray = varray_from_initializer(initializer, data_type, total_size);
    /* TODO: When we can call a variant of #set_all with a virtual array argument,
     * this theoretically unnecessary materialize step could be removed. */
    GVArray_GSpan source_varray_span{*source_varray};
    write_attribute.varray->set_all(source_varray_span.data());

    if (initializer.type == AttributeInit::Type::MoveArray) {
      MEM_freeN(static_cast<const AttributeInitMove &>(initializer).data);
    }

    return true;
  }

  bool foreach_attribute(const GeometryComponent &component,
                         const AttributeForeachCallback callback) const final
  {
    const CurveEval *curve = get_curve_from_component_for_read(component);
    if (curve == nullptr || curve->splines().size() == 0) {
      return false;
    }

    Span<SplinePtr> splines = curve->splines();

    /* In a debug build, check that all corresponding custom data layers have the same type. */
    curve->assert_valid_point_attributes();

    /* Use the first spline as a representative for all the others. */
    splines.first()->attributes.foreach_attribute(callback, ATTR_DOMAIN_POINT);

    return true;
  }

  void foreach_domain(const FunctionRef<void(AttributeDomain)> callback) const final
  {
    callback(ATTR_DOMAIN_POINT);
  }

  bool type_is_supported(CustomDataType data_type) const
  {
    return ((1ULL << data_type) & supported_types_mask) != 0;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute Provider Declaration
 * \{ */

/**
 * In this function all the attribute providers for a curve component are created.
 * Most data in this function is statically allocated, because it does not change over time.
 */
static ComponentAttributeProviders create_attribute_providers_for_curve()
{
  static BuiltinSplineAttributeProvider resolution("resolution",
                                                   CD_PROP_INT32,
                                                   BuiltinAttributeProvider::Writable,
                                                   make_resolution_read_attribute,
                                                   make_resolution_write_attribute);

  static BuiltinSplineAttributeProvider cyclic("cyclic",
                                               CD_PROP_BOOL,
                                               BuiltinAttributeProvider::Writable,
                                               make_cyclic_read_attribute,
                                               make_cyclic_write_attribute);

  static CustomDataAccessInfo spline_custom_data_access = {
      [](GeometryComponent &component) -> CustomData * {
        CurveEval *curve = get_curve_from_component_for_write(component);
        return curve ? &curve->attributes.data : nullptr;
      },
      [](const GeometryComponent &component) -> const CustomData * {
        const CurveEval *curve = get_curve_from_component_for_read(component);
        return curve ? &curve->attributes.data : nullptr;
      },
      nullptr};

  static CustomDataAttributeProvider spline_custom_data(ATTR_DOMAIN_CURVE,
                                                        spline_custom_data_access);

  static PositionAttributeProvider position;

  static BuiltinPointAttributeProvider<float> radius(
      "radius",
      BuiltinAttributeProvider::Writable,
      [](const Spline &spline) { return spline.radii(); },
      [](Spline &spline) { return spline.radii(); },
      nullptr);

  static BuiltinPointAttributeProvider<float> tilt(
      "tilt",
      BuiltinAttributeProvider::Writable,
      [](const Spline &spline) { return spline.tilts(); },
      [](Spline &spline) { return spline.tilts(); },
      [](Spline &spline) { spline.mark_cache_invalid(); });

  static DynamicPointAttributeProvider point_custom_data;

  return ComponentAttributeProviders({&position, &radius, &tilt, &resolution, &cyclic},
                                     {&spline_custom_data, &point_custom_data});
}

}  // namespace blender::bke

const blender::bke::ComponentAttributeProviders *CurveComponent::get_attribute_providers() const
{
  static blender::bke::ComponentAttributeProviders providers =
      blender::bke::create_attribute_providers_for_curve();
  return &providers;
}

/** \} */
