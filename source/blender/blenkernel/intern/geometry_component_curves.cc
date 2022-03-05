/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "DNA_ID_enums.h"
#include "DNA_curve_types.h"

#include "BKE_attribute_access.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curve.h"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_spline.hh"

#include "attribute_access_intern.hh"

using blender::fn::GVArray;

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
  if (curves_ != nullptr) {
    new_component->curves_ = BKE_curves_copy_for_eval(curves_, false);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return new_component;
}

void CurveComponent::clear()
{
  BLI_assert(this->is_mutable());
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

const Curves *CurveComponent::get_for_read() const
{
  return curves_;
}

Curves *CurveComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    curves_ = BKE_curves_copy_for_eval(curves_, false);
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
    curves_ = BKE_curves_copy_for_eval(curves_, false);
    ownership_ = GeometryOwnershipType::Owned;
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

  curve_for_render_ = (Curve *)BKE_id_new_nomain(ID_CU_LEGACY, nullptr);
  curve_for_render_->curve_eval = curves_to_curve_eval(*curves_).release();

  return curve_for_render_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Normals Access
 * \{ */

namespace blender::bke {

static void calculate_bezier_normals(const BezierSpline &spline, MutableSpan<float3> normals)
{
  Span<int> offsets = spline.control_point_offsets();
  Span<float3> evaluated_normals = spline.evaluated_normals();
  for (const int i : IndexRange(spline.size())) {
    normals[i] = evaluated_normals[offsets[i]];
  }
}

static void calculate_poly_normals(const PolySpline &spline, MutableSpan<float3> normals)
{
  normals.copy_from(spline.evaluated_normals());
}

/**
 * Because NURBS control points are not necessarily on the path, the normal at the control points
 * is not well defined, so create a temporary poly spline to find the normals. This requires extra
 * copying currently, but may be more efficient in the future if attributes have some form of CoW.
 */
static void calculate_nurbs_normals(const NURBSpline &spline, MutableSpan<float3> normals)
{
  PolySpline poly_spline;
  poly_spline.resize(spline.size());
  poly_spline.positions().copy_from(spline.positions());
  poly_spline.tilts().copy_from(spline.tilts());
  normals.copy_from(poly_spline.evaluated_normals());
}

static Array<float3> curve_normal_point_domain(const CurveEval &curve)
{
  Span<SplinePtr> splines = curve.splines();
  Array<int> offsets = curve.control_point_offsets();
  const int total_size = offsets.last();
  Array<float3> normals(total_size);

  threading::parallel_for(splines.index_range(), 128, [&](IndexRange range) {
    for (const int i : range) {
      const Spline &spline = *splines[i];
      MutableSpan spline_normals{normals.as_mutable_span().slice(offsets[i], spline.size())};
      switch (splines[i]->type()) {
        case CURVE_TYPE_BEZIER:
          calculate_bezier_normals(static_cast<const BezierSpline &>(spline), spline_normals);
          break;
        case CURVE_TYPE_POLY:
          calculate_poly_normals(static_cast<const PolySpline &>(spline), spline_normals);
          break;
        case CURVE_TYPE_NURBS:
          calculate_nurbs_normals(static_cast<const NURBSpline &>(spline), spline_normals);
          break;
        case CURVE_TYPE_CATMULL_ROM:
          BLI_assert_unreachable();
          break;
      }
    }
  });
  return normals;
}

VArray<float3> curve_normals_varray(const CurveComponent &component, const AttributeDomain domain)
{
  if (component.is_empty()) {
    return nullptr;
  }
  const std::unique_ptr<CurveEval> curve = curves_to_curve_eval(*component.get_for_read());

  if (domain == ATTR_DOMAIN_POINT) {
    Array<float3> normals = curve_normal_point_domain(*curve);
    return VArray<float3>::ForContainer(std::move(normals));
  }

  if (domain == ATTR_DOMAIN_CURVE) {
    Array<float3> point_normals = curve_normal_point_domain(*curve);
    VArray<float3> varray = VArray<float3>::ForContainer(std::move(point_normals));
    return component.attribute_try_adapt_domain<float3>(
        std::move(varray), ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE);
  }

  return nullptr;
}

}  // namespace blender::bke

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute Access Helper Functions
 * \{ */

int CurveComponent::attribute_domain_size(const AttributeDomain domain) const
{
  if (curves_ == nullptr) {
    return 0;
  }
  const blender::bke::CurvesGeometry &geometry = blender::bke::CurvesGeometry::wrap(
      curves_->geometry);
  if (domain == ATTR_DOMAIN_POINT) {
    return geometry.points_size();
  }
  if (domain == ATTR_DOMAIN_CURVE) {
    return geometry.curves_size();
  }
  return 0;
}

GVArray CurveComponent::attribute_try_adapt_domain_impl(const GVArray &varray,
                                                        const AttributeDomain from_domain,
                                                        const AttributeDomain to_domain) const
{
  return blender::bke::CurvesGeometry::wrap(curves_->geometry)
      .adapt_domain(varray, from_domain, to_domain);
}

static Curves *get_curves_from_component_for_write(GeometryComponent &component)
{
  BLI_assert(component.type() == GEO_COMPONENT_TYPE_CURVE);
  CurveComponent &curve_component = static_cast<CurveComponent &>(component);
  return curve_component.get_for_write();
}

static const Curves *get_curves_from_component_for_read(const GeometryComponent &component)
{
  BLI_assert(component.type() == GEO_COMPONENT_TYPE_CURVE);
  const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
  return curve_component.get_for_read();
}

static void tag_component_topology_changed(GeometryComponent &component)
{
  Curves *curves = get_curves_from_component_for_write(component);
  if (curves) {
    blender::bke::CurvesGeometry::wrap(curves->geometry).tag_topology_changed();
  }
}

static void tag_component_positions_changed(GeometryComponent &component)
{
  Curves *curves = get_curves_from_component_for_write(component);
  if (curves) {
    blender::bke::CurvesGeometry::wrap(curves->geometry).tag_positions_changed();
  }
}

static void tag_component_normals_changed(GeometryComponent &component)
{
  Curves *curves = get_curves_from_component_for_write(component);
  if (curves) {
    blender::bke::CurvesGeometry::wrap(curves->geometry).tag_normals_changed();
  }
}

/** \} */

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Attribute Provider Declaration
 * \{ */

/**
 * In this function all the attribute providers for a curves component are created.
 * Most data in this function is statically allocated, because it does not change over time.
 */
static ComponentAttributeProviders create_attribute_providers_for_curve()
{
  static CustomDataAccessInfo curve_access = {
      [](GeometryComponent &component) -> CustomData * {
        Curves *curves = get_curves_from_component_for_write(component);
        return curves ? &curves->geometry.curve_data : nullptr;
      },
      [](const GeometryComponent &component) -> const CustomData * {
        const Curves *curves = get_curves_from_component_for_read(component);
        return curves ? &curves->geometry.curve_data : nullptr;
      },
      [](GeometryComponent &component) {
        Curves *curves = get_curves_from_component_for_write(component);
        if (curves) {
          blender::bke::CurvesGeometry::wrap(curves->geometry).update_customdata_pointers();
        }
      }};
  static CustomDataAccessInfo point_access = {
      [](GeometryComponent &component) -> CustomData * {
        Curves *curves = get_curves_from_component_for_write(component);
        return curves ? &curves->geometry.point_data : nullptr;
      },
      [](const GeometryComponent &component) -> const CustomData * {
        const Curves *curves = get_curves_from_component_for_read(component);
        return curves ? &curves->geometry.point_data : nullptr;
      },
      [](GeometryComponent &component) {
        Curves *curves = get_curves_from_component_for_write(component);
        if (curves) {
          blender::bke::CurvesGeometry::wrap(curves->geometry).update_customdata_pointers();
        }
      }};

  static BuiltinCustomDataLayerProvider position("position",
                                                 ATTR_DOMAIN_POINT,
                                                 CD_PROP_FLOAT3,
                                                 CD_PROP_FLOAT3,
                                                 BuiltinAttributeProvider::NonCreatable,
                                                 BuiltinAttributeProvider::Writable,
                                                 BuiltinAttributeProvider::NonDeletable,
                                                 point_access,
                                                 make_array_read_attribute<float3>,
                                                 make_array_write_attribute<float3>,
                                                 tag_component_positions_changed);

  static BuiltinCustomDataLayerProvider radius("radius",
                                               ATTR_DOMAIN_POINT,
                                               CD_PROP_FLOAT,
                                               CD_PROP_FLOAT,
                                               BuiltinAttributeProvider::Creatable,
                                               BuiltinAttributeProvider::Writable,
                                               BuiltinAttributeProvider::Deletable,
                                               point_access,
                                               make_array_read_attribute<float>,
                                               make_array_write_attribute<float>,
                                               nullptr);

  static BuiltinCustomDataLayerProvider id("id",
                                           ATTR_DOMAIN_POINT,
                                           CD_PROP_INT32,
                                           CD_PROP_INT32,
                                           BuiltinAttributeProvider::Creatable,
                                           BuiltinAttributeProvider::Writable,
                                           BuiltinAttributeProvider::Deletable,
                                           point_access,
                                           make_array_read_attribute<int>,
                                           make_array_write_attribute<int>,
                                           nullptr);

  static BuiltinCustomDataLayerProvider tilt("tilt",
                                             ATTR_DOMAIN_POINT,
                                             CD_PROP_FLOAT,
                                             CD_PROP_FLOAT,
                                             BuiltinAttributeProvider::Creatable,
                                             BuiltinAttributeProvider::Writable,
                                             BuiltinAttributeProvider::Deletable,
                                             point_access,
                                             make_array_read_attribute<float>,
                                             make_array_write_attribute<float>,
                                             tag_component_normals_changed);

  static BuiltinCustomDataLayerProvider handle_right("handle_right",
                                                     ATTR_DOMAIN_POINT,
                                                     CD_PROP_FLOAT3,
                                                     CD_PROP_FLOAT3,
                                                     BuiltinAttributeProvider::Creatable,
                                                     BuiltinAttributeProvider::Writable,
                                                     BuiltinAttributeProvider::Deletable,
                                                     point_access,
                                                     make_array_read_attribute<float3>,
                                                     make_array_write_attribute<float3>,
                                                     tag_component_positions_changed);

  static BuiltinCustomDataLayerProvider handle_left("handle_left",
                                                    ATTR_DOMAIN_POINT,
                                                    CD_PROP_FLOAT3,
                                                    CD_PROP_FLOAT3,
                                                    BuiltinAttributeProvider::Creatable,
                                                    BuiltinAttributeProvider::Writable,
                                                    BuiltinAttributeProvider::Deletable,
                                                    point_access,
                                                    make_array_read_attribute<float3>,
                                                    make_array_write_attribute<float3>,
                                                    tag_component_positions_changed);

  static BuiltinCustomDataLayerProvider handle_type_right("handle_type_right",
                                                          ATTR_DOMAIN_POINT,
                                                          CD_PROP_INT8,
                                                          CD_PROP_INT8,
                                                          BuiltinAttributeProvider::Creatable,
                                                          BuiltinAttributeProvider::Writable,
                                                          BuiltinAttributeProvider::Deletable,
                                                          point_access,
                                                          make_array_read_attribute<int8_t>,
                                                          make_array_write_attribute<int8_t>,
                                                          tag_component_topology_changed);

  static BuiltinCustomDataLayerProvider handle_type_left("handle_type_left",
                                                         ATTR_DOMAIN_POINT,
                                                         CD_PROP_INT8,
                                                         CD_PROP_INT8,
                                                         BuiltinAttributeProvider::Creatable,
                                                         BuiltinAttributeProvider::Writable,
                                                         BuiltinAttributeProvider::Deletable,
                                                         point_access,
                                                         make_array_read_attribute<int8_t>,
                                                         make_array_write_attribute<int8_t>,
                                                         tag_component_topology_changed);

  static BuiltinCustomDataLayerProvider nurbs_weight("nurbs_weight",
                                                     ATTR_DOMAIN_POINT,
                                                     CD_PROP_FLOAT,
                                                     CD_PROP_FLOAT,
                                                     BuiltinAttributeProvider::Creatable,
                                                     BuiltinAttributeProvider::Writable,
                                                     BuiltinAttributeProvider::Deletable,
                                                     point_access,
                                                     make_array_read_attribute<float>,
                                                     make_array_write_attribute<float>,
                                                     tag_component_positions_changed);

  static BuiltinCustomDataLayerProvider nurbs_order("nurbs_order",
                                                    ATTR_DOMAIN_CURVE,
                                                    CD_PROP_INT32,
                                                    CD_PROP_INT32,
                                                    BuiltinAttributeProvider::Creatable,
                                                    BuiltinAttributeProvider::Writable,
                                                    BuiltinAttributeProvider::Deletable,
                                                    curve_access,
                                                    make_array_read_attribute<int>,
                                                    make_array_write_attribute<int>,
                                                    tag_component_topology_changed);

  static BuiltinCustomDataLayerProvider nurbs_knots_mode("knots_mode",
                                                         ATTR_DOMAIN_CURVE,
                                                         CD_PROP_INT8,
                                                         CD_PROP_INT8,
                                                         BuiltinAttributeProvider::Creatable,
                                                         BuiltinAttributeProvider::Writable,
                                                         BuiltinAttributeProvider::Deletable,
                                                         curve_access,
                                                         make_array_read_attribute<int8_t>,
                                                         make_array_write_attribute<int8_t>,
                                                         tag_component_topology_changed);

  static BuiltinCustomDataLayerProvider curve_type("curve_type",
                                                   ATTR_DOMAIN_CURVE,
                                                   CD_PROP_INT8,
                                                   CD_PROP_INT8,
                                                   BuiltinAttributeProvider::Creatable,
                                                   BuiltinAttributeProvider::Writable,
                                                   BuiltinAttributeProvider::Deletable,
                                                   curve_access,
                                                   make_array_read_attribute<int8_t>,
                                                   make_array_write_attribute<int8_t>,
                                                   tag_component_topology_changed);

  static BuiltinCustomDataLayerProvider resolution("resolution",
                                                   ATTR_DOMAIN_CURVE,
                                                   CD_PROP_INT32,
                                                   CD_PROP_INT32,
                                                   BuiltinAttributeProvider::Creatable,
                                                   BuiltinAttributeProvider::Writable,
                                                   BuiltinAttributeProvider::Deletable,
                                                   curve_access,
                                                   make_array_read_attribute<int>,
                                                   make_array_write_attribute<int>,
                                                   tag_component_positions_changed);

  static BuiltinCustomDataLayerProvider cyclic("cyclic",
                                               ATTR_DOMAIN_CURVE,
                                               CD_PROP_BOOL,
                                               CD_PROP_BOOL,
                                               BuiltinAttributeProvider::Creatable,
                                               BuiltinAttributeProvider::Writable,
                                               BuiltinAttributeProvider::Deletable,
                                               curve_access,
                                               make_array_read_attribute<bool>,
                                               make_array_write_attribute<bool>,
                                               tag_component_topology_changed);

  static CustomDataAttributeProvider curve_custom_data(ATTR_DOMAIN_CURVE, curve_access);
  static CustomDataAttributeProvider point_custom_data(ATTR_DOMAIN_POINT, point_access);

  return ComponentAttributeProviders({&position,
                                      &radius,
                                      &id,
                                      &tilt,
                                      &handle_right,
                                      &handle_left,
                                      &handle_type_right,
                                      &handle_type_left,
                                      &nurbs_order,
                                      &nurbs_weight,
                                      &curve_type,
                                      &resolution,
                                      &cyclic},
                                     {&curve_custom_data, &point_custom_data});
}

/** \} */

}  // namespace blender::bke

const blender::bke::ComponentAttributeProviders *CurveComponent::get_attribute_providers() const
{
  static blender::bke::ComponentAttributeProviders providers =
      blender::bke::create_attribute_providers_for_curve();
  return &providers;
}
