/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "DNA_curve_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"
#include "BKE_lib_id.hh"

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

CurveComponent::CurveComponent() : GeometryComponent(Type::Curve) {}

CurveComponent::CurveComponent(Curves *curve, GeometryOwnershipType ownership)
    : GeometryComponent(Type::Curve), curves_(curve), ownership_(ownership)
{
}

CurveComponent::~CurveComponent()
{
  this->clear();
}

GeometryComponentPtr CurveComponent::copy() const
{
  CurveComponent *new_component = new CurveComponent();
  if (curves_ != nullptr) {
    new_component->curves_ = BKE_curves_copy_for_eval(curves_);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return GeometryComponentPtr(new_component);
}

void CurveComponent::clear()
{
  BLI_assert(this->is_mutable() || this->is_expired());
  if (curves_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      BKE_id_free(nullptr, curves_);
    }
    if (curve_for_render_ != nullptr) {
      /* The curve created by this component should not have any edit mode data. */
      BLI_assert(curve_for_render_->editfont == nullptr && curve_for_render_->editnurb == nullptr);
      BKE_id_free(nullptr, curve_for_render_);
      curve_for_render_ = nullptr;
    }

    curves_ = nullptr;
  }
}

bool CurveComponent::has_curves() const
{
  return curves_ != nullptr;
}

void CurveComponent::replace(Curves *curves, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  curves_ = curves;
  ownership_ = ownership;
}

Curves *CurveComponent::release()
{
  BLI_assert(this->is_mutable());
  Curves *curves = curves_;
  curves_ = nullptr;
  return curves;
}

const Curves *CurveComponent::get() const
{
  return curves_;
}

Curves *CurveComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    curves_ = BKE_curves_copy_for_eval(curves_);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return curves_;
}

bool CurveComponent::is_empty() const
{
  return curves_ == nullptr;
}

bool CurveComponent::owns_direct_data() const
{
  return ownership_ == GeometryOwnershipType::Owned;
}

void CurveComponent::ensure_owns_direct_data()
{
  BLI_assert(this->is_mutable());
  if (ownership_ != GeometryOwnershipType::Owned) {
    if (curves_) {
      curves_ = BKE_curves_copy_for_eval(curves_);
    }
    ownership_ = GeometryOwnershipType::Owned;
  }
}

void CurveComponent::count_memory(MemoryCounter &memory) const
{
  if (curves_) {
    curves_->geometry.wrap().count_memory(memory);
  }
}

const Curve *CurveComponent::get_curve_for_render() const
{
  if (curves_ == nullptr) {
    return nullptr;
  }
  if (curve_for_render_ != nullptr) {
    return curve_for_render_;
  }
  std::lock_guard lock{curve_for_render_mutex_};
  if (curve_for_render_ != nullptr) {
    return curve_for_render_;
  }

  curve_for_render_ = BKE_id_new_nomain<Curve>(nullptr);
  curve_for_render_->curve_eval = curves_;

  return curve_for_render_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Normals Access
 * \{ */

static Array<float3> curve_normal_point_domain(const CurvesGeometry &curves)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const OffsetIndices evaluated_points_by_curve = curves.evaluated_points_by_curve();
  const VArray<int8_t> types = curves.curve_types();
  const VArray<int> resolutions = curves.resolution();
  const VArray<bool> curves_cyclic = curves.cyclic();
  const AttributeAccessor attributes = curves.attributes();
  const VArray<float3> custom_normals = *attributes.lookup_or_default<float3>(
      "custom_normal", AttrDomain::Point, float3(0, 0, 1));

  const Span<float3> positions = curves.positions();
  const VArray<int8_t> normal_modes = curves.normal_mode();

  const Span<float3> evaluated_normals = curves.evaluated_normals();

  Array<float3> results(curves.points_num());

  threading::parallel_for(curves.curves_range(), 128, [&](IndexRange range) {
    Vector<float3> nurbs_tangents;

    for (const int i_curve : range) {
      const IndexRange points = points_by_curve[i_curve];
      const IndexRange evaluated_points = evaluated_points_by_curve[i_curve];

      MutableSpan<float3> curve_normals = results.as_mutable_span().slice(points);

      switch (types[i_curve]) {
        case CURVE_TYPE_CATMULL_ROM: {
          const Span<float3> normals = evaluated_normals.slice(evaluated_points);
          const int resolution = resolutions[i_curve];
          for (const int i : IndexRange(points.size())) {
            curve_normals[i] = normals[resolution * i];
          }
          break;
        }
        case CURVE_TYPE_POLY:
          curve_normals.copy_from(evaluated_normals.slice(evaluated_points));
          break;
        case CURVE_TYPE_BEZIER: {
          const Span<float3> normals = evaluated_normals.slice(evaluated_points);
          curve_normals.first() = normals.first();
          const Span<int> offsets = curves.bezier_evaluated_offsets_for_curve(i_curve);
          for (const int i : IndexRange(points.size()).drop_front(1)) {
            curve_normals[i] = normals[offsets[i]];
          }
          break;
        }
        case CURVE_TYPE_NURBS: {
          /* For NURBS curves there is no obvious correspondence between specific evaluated points
           * and control points, so normals are determined by treating them as poly curves. */
          nurbs_tangents.clear();
          nurbs_tangents.resize(points.size());
          const bool cyclic = curves_cyclic[i_curve];
          const Span<float3> curve_positions = positions.slice(points);
          curves::poly::calculate_tangents(curve_positions, cyclic, nurbs_tangents);
          switch (NormalMode(normal_modes[i_curve])) {
            case NORMAL_MODE_Z_UP:
              curves::poly::calculate_normals_z_up(nurbs_tangents, curve_normals);
              break;
            case NORMAL_MODE_MINIMUM_TWIST:
              curves::poly::calculate_normals_minimum(nurbs_tangents, cyclic, curve_normals);
              break;
            case NORMAL_MODE_FREE:
              custom_normals.materialize(points, results);
              break;
          }
          break;
        }
      }
    }
  });
  return results;
}

VArray<float3> curve_normals_varray(const CurvesGeometry &curves, const AttrDomain domain)
{
  const VArray<int8_t> types = curves.curve_types();
  if (curves.is_single_type(CURVE_TYPE_POLY)) {
    return curves.adapt_domain<float3>(
        VArray<float3>::from_span(curves.evaluated_normals()), AttrDomain::Point, domain);
  }

  Array<float3> normals = curve_normal_point_domain(curves);

  if (domain == AttrDomain::Point) {
    return VArray<float3>::from_container(std::move(normals));
  }

  if (domain == AttrDomain::Curve) {
    return curves.adapt_domain<float3>(
        VArray<float3>::from_container(std::move(normals)), AttrDomain::Point, AttrDomain::Curve);
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Length Field Input
 * \{ */

static VArray<float> construct_curve_length_gvarray(const CurvesGeometry &curves,
                                                    const AttrDomain domain)
{
  curves.ensure_evaluated_lengths();

  VArray<bool> cyclic = curves.cyclic();
  VArray<float> lengths = VArray<float>::from_func(
      curves.curves_num(), [&curves, cyclic = std::move(cyclic)](int64_t index) {
        return curves.evaluated_length_total_for_curve(index, cyclic[index]);
      });

  if (domain == AttrDomain::Curve) {
    return lengths;
  }

  if (domain == AttrDomain::Point) {
    return curves.adapt_domain<float>(std::move(lengths), AttrDomain::Curve, AttrDomain::Point);
  }

  return {};
}

CurveLengthFieldInput::CurveLengthFieldInput()
    : CurvesFieldInput(CPPType::get<float>(), "Spline Length node")
{
  category_ = Category::Generated;
}

GVArray CurveLengthFieldInput::get_varray_for_context(const CurvesGeometry &curves,
                                                      const AttrDomain domain,
                                                      const IndexMask & /*mask*/) const
{
  return construct_curve_length_gvarray(curves, domain);
}

uint64_t CurveLengthFieldInput::hash() const
{
  /* Some random constant hash. */
  return 3549623580;
}

bool CurveLengthFieldInput::is_equal_to(const fn::FieldNode &other) const
{
  return dynamic_cast<const CurveLengthFieldInput *>(&other) != nullptr;
}

std::optional<AttrDomain> CurveLengthFieldInput::preferred_domain(
    const CurvesGeometry & /*curves*/) const
{
  return AttrDomain::Curve;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute Access
 * \{ */

std::optional<AttributeAccessor> CurveComponent::attributes() const
{
  return AttributeAccessor(curves_ ? &curves_->geometry : nullptr,
                           curves::get_attribute_accessor_functions());
}

std::optional<MutableAttributeAccessor> CurveComponent::attributes_for_write()
{
  Curves *curves = this->get_for_write();
  return MutableAttributeAccessor(curves ? &curves->geometry : nullptr,
                                  curves::get_attribute_accessor_functions());
}

}  // namespace blender::bke
