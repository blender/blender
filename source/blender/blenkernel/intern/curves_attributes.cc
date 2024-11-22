/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_deform.hh"

#include "FN_multi_function_builder.hh"

#include "attribute_access_intern.hh"

namespace blender::bke::curves {

static void tag_component_topology_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.tag_topology_changed();
}

static void tag_component_curve_types_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.update_curve_types();
  curves.tag_topology_changed();
}

static void tag_component_positions_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.tag_positions_changed();
}

static void tag_component_radii_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.tag_radii_changed();
}

static void tag_component_normals_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.tag_normals_changed();
}

/**
 * This provider makes vertex groups available as float attributes.
 */
class CurvesVertexGroupsAttributeProvider final : public DynamicAttributesProvider {
 public:
  GAttributeReader try_get_for_read(const void *owner, const StringRef attribute_id) const final
  {
    if (bke::attribute_name_is_anonymous(attribute_id)) {
      return {};
    }
    const CurvesGeometry *curves = static_cast<const CurvesGeometry *>(owner);
    if (curves == nullptr) {
      return {};
    }
    const int vertex_group_index = BKE_defgroup_name_index(&curves->vertex_group_names,
                                                           attribute_id);
    if (vertex_group_index < 0) {
      return {};
    }
    const Span<MDeformVert> dverts = curves->deform_verts();
    return this->get_for_vertex_group_index(*curves, dverts, vertex_group_index);
  }

  GAttributeReader get_for_vertex_group_index(const CurvesGeometry &curves,
                                              const Span<MDeformVert> dverts,
                                              const int vertex_group_index) const
  {
    BLI_assert(vertex_group_index >= 0);
    if (dverts.is_empty()) {
      return {VArray<float>::ForSingle(0.0f, curves.points_num()), AttrDomain::Point};
    }
    return {varray_for_deform_verts(dverts, vertex_group_index), AttrDomain::Point};
  }

  GAttributeWriter try_get_for_write(void *owner, const StringRef attribute_id) const final
  {
    if (bke::attribute_name_is_anonymous(attribute_id)) {
      return {};
    }
    CurvesGeometry *curves = static_cast<CurvesGeometry *>(owner);
    if (curves == nullptr) {
      return {};
    }
    const int vertex_group_index = BKE_defgroup_name_index(&curves->vertex_group_names,
                                                           attribute_id);
    if (vertex_group_index < 0) {
      return {};
    }
    MutableSpan<MDeformVert> dverts = curves->deform_verts_for_write();
    return {varray_for_mutable_deform_verts(dverts, vertex_group_index), AttrDomain::Point};
  }

  bool try_delete(void *owner, const StringRef attribute_id) const final
  {
    if (bke::attribute_name_is_anonymous(attribute_id)) {
      return false;
    }
    CurvesGeometry *curves = static_cast<CurvesGeometry *>(owner);
    if (curves == nullptr) {
      return true;
    }
    const std::string name = attribute_id;

    int index;
    bDeformGroup *group;
    if (!BKE_defgroup_listbase_name_find(
            &curves->vertex_group_names, name.c_str(), &index, &group))
    {
      return false;
    }
    BLI_remlink(&curves->vertex_group_names, group);
    MEM_freeN(group);
    if (curves->deform_verts().is_empty()) {
      return true;
    }

    MutableSpan<MDeformVert> dverts = curves->deform_verts_for_write();
    remove_defgroup_index(dverts, index);
    return true;
  }

  bool foreach_attribute(const void *owner,
                         FunctionRef<void(const AttributeIter &)> fn) const final
  {
    const CurvesGeometry *curves = static_cast<const CurvesGeometry *>(owner);
    if (curves == nullptr) {
      return true;
    }
    const Span<MDeformVert> dverts = curves->deform_verts();

    int group_index = 0;
    LISTBASE_FOREACH_INDEX (const bDeformGroup *, group, &curves->vertex_group_names, group_index)
    {
      const auto get_fn = [&]() {
        return this->get_for_vertex_group_index(*curves, dverts, group_index);
      };
      AttributeIter iter{group->name, AttrDomain::Point, CD_PROP_FLOAT, get_fn};
      fn(iter);
      if (iter.is_stopped()) {
        return false;
      }
    }
    return true;
  }

  void foreach_domain(const FunctionRef<void(AttrDomain)> callback) const final
  {
    callback(AttrDomain::Point);
  }
};

/**
 * In this function all the attribute providers for a curves component are created.
 * Most data in this function is statically allocated, because it does not change over time.
 */
static GeometryAttributeProviders create_attribute_providers_for_curve()
{
  static CustomDataAccessInfo curve_access = {
      [](void *owner) -> CustomData * {
        CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
        return &curves.curve_data;
      },
      [](const void *owner) -> const CustomData * {
        const CurvesGeometry &curves = *static_cast<const CurvesGeometry *>(owner);
        return &curves.curve_data;
      },
      [](const void *owner) -> int {
        const CurvesGeometry &curves = *static_cast<const CurvesGeometry *>(owner);
        return curves.curves_num();
      }};
  static CustomDataAccessInfo point_access = {
      [](void *owner) -> CustomData * {
        CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
        return &curves.point_data;
      },
      [](const void *owner) -> const CustomData * {
        const CurvesGeometry &curves = *static_cast<const CurvesGeometry *>(owner);
        return &curves.point_data;
      },
      [](const void *owner) -> int {
        const CurvesGeometry &curves = *static_cast<const CurvesGeometry *>(owner);
        return curves.points_num();
      }};

  static BuiltinCustomDataLayerProvider position("position",
                                                 AttrDomain::Point,
                                                 CD_PROP_FLOAT3,
                                                 BuiltinAttributeProvider::NonDeletable,
                                                 point_access,
                                                 tag_component_positions_changed);

  static BuiltinCustomDataLayerProvider radius("radius",
                                               AttrDomain::Point,
                                               CD_PROP_FLOAT,
                                               BuiltinAttributeProvider::Deletable,
                                               point_access,
                                               tag_component_radii_changed);

  static BuiltinCustomDataLayerProvider id("id",
                                           AttrDomain::Point,
                                           CD_PROP_INT32,
                                           BuiltinAttributeProvider::Deletable,
                                           point_access,
                                           nullptr);

  static BuiltinCustomDataLayerProvider tilt("tilt",
                                             AttrDomain::Point,
                                             CD_PROP_FLOAT,
                                             BuiltinAttributeProvider::Deletable,
                                             point_access,
                                             tag_component_normals_changed);

  static BuiltinCustomDataLayerProvider handle_right("handle_right",
                                                     AttrDomain::Point,
                                                     CD_PROP_FLOAT3,
                                                     BuiltinAttributeProvider::Deletable,
                                                     point_access,
                                                     tag_component_positions_changed);

  static BuiltinCustomDataLayerProvider handle_left("handle_left",
                                                    AttrDomain::Point,
                                                    CD_PROP_FLOAT3,
                                                    BuiltinAttributeProvider::Deletable,
                                                    point_access,
                                                    tag_component_positions_changed);

  static auto handle_type_clamp = mf::build::SI1_SO<int8_t, int8_t>(
      "Handle Type Validate",
      [](int8_t value) {
        return std::clamp<int8_t>(value, BEZIER_HANDLE_FREE, BEZIER_HANDLE_ALIGN);
      },
      mf::build::exec_presets::AllSpanOrSingle());
  static BuiltinCustomDataLayerProvider handle_type_right("handle_type_right",
                                                          AttrDomain::Point,
                                                          CD_PROP_INT8,
                                                          BuiltinAttributeProvider::Deletable,
                                                          point_access,
                                                          tag_component_topology_changed,
                                                          AttributeValidator{&handle_type_clamp});

  static BuiltinCustomDataLayerProvider handle_type_left("handle_type_left",
                                                         AttrDomain::Point,
                                                         CD_PROP_INT8,
                                                         BuiltinAttributeProvider::Deletable,
                                                         point_access,
                                                         tag_component_topology_changed,
                                                         AttributeValidator{&handle_type_clamp});

  static BuiltinCustomDataLayerProvider nurbs_weight("nurbs_weight",
                                                     AttrDomain::Point,
                                                     CD_PROP_FLOAT,
                                                     BuiltinAttributeProvider::Deletable,
                                                     point_access,
                                                     tag_component_positions_changed);

  static const auto nurbs_order_clamp = mf::build::SI1_SO<int8_t, int8_t>(
      "NURBS Order Validate",
      [](int8_t value) { return std::max<int8_t>(value, 1); },
      mf::build::exec_presets::AllSpanOrSingle());
  static int nurbs_order_default = 4;
  static BuiltinCustomDataLayerProvider nurbs_order("nurbs_order",
                                                    AttrDomain::Curve,
                                                    CD_PROP_INT8,
                                                    BuiltinAttributeProvider::Deletable,
                                                    curve_access,
                                                    tag_component_topology_changed,
                                                    AttributeValidator{&nurbs_order_clamp},
                                                    &nurbs_order_default);

  static const auto normal_mode_clamp = mf::build::SI1_SO<int8_t, int8_t>(
      "Normal Mode Validate",
      [](int8_t value) {
        return std::clamp<int8_t>(value, NORMAL_MODE_MINIMUM_TWIST, NORMAL_MODE_FREE);
      },
      mf::build::exec_presets::AllSpanOrSingle());
  static BuiltinCustomDataLayerProvider normal_mode("normal_mode",
                                                    AttrDomain::Curve,
                                                    CD_PROP_INT8,
                                                    BuiltinAttributeProvider::Deletable,
                                                    curve_access,
                                                    tag_component_normals_changed,
                                                    AttributeValidator{&normal_mode_clamp});

  static BuiltinCustomDataLayerProvider custom_normal("custom_normal",
                                                      AttrDomain::Point,
                                                      CD_PROP_FLOAT3,
                                                      BuiltinAttributeProvider::Deletable,
                                                      point_access,
                                                      tag_component_normals_changed);

  static const auto knots_mode_clamp = mf::build::SI1_SO<int8_t, int8_t>(
      "Knots Mode Validate",
      [](int8_t value) {
        return std::clamp<int8_t>(value, NURBS_KNOT_MODE_NORMAL, NURBS_KNOT_MODE_ENDPOINT_BEZIER);
      },
      mf::build::exec_presets::AllSpanOrSingle());
  static BuiltinCustomDataLayerProvider nurbs_knots_mode("knots_mode",
                                                         AttrDomain::Curve,
                                                         CD_PROP_INT8,
                                                         BuiltinAttributeProvider::Deletable,
                                                         curve_access,
                                                         tag_component_topology_changed,
                                                         AttributeValidator{&knots_mode_clamp});

  static const auto curve_type_clamp = mf::build::SI1_SO<int8_t, int8_t>(
      "Curve Type Validate",
      [](int8_t value) {
        return std::clamp<int8_t>(value, CURVE_TYPE_CATMULL_ROM, CURVE_TYPES_NUM);
      },
      mf::build::exec_presets::AllSpanOrSingle());
  static BuiltinCustomDataLayerProvider curve_type("curve_type",
                                                   AttrDomain::Curve,
                                                   CD_PROP_INT8,
                                                   BuiltinAttributeProvider::Deletable,
                                                   curve_access,
                                                   tag_component_curve_types_changed,
                                                   AttributeValidator{&curve_type_clamp});

  static const auto resolution_clamp = mf::build::SI1_SO<int, int>(
      "Resolution Validate",
      [](int value) { return std::max<int>(value, 1); },
      mf::build::exec_presets::AllSpanOrSingle());
  static int resolution_default = 12;
  static BuiltinCustomDataLayerProvider resolution("resolution",
                                                   AttrDomain::Curve,
                                                   CD_PROP_INT32,
                                                   BuiltinAttributeProvider::Deletable,
                                                   curve_access,
                                                   tag_component_topology_changed,
                                                   AttributeValidator{&resolution_clamp},
                                                   &resolution_default);

  static BuiltinCustomDataLayerProvider cyclic("cyclic",
                                               AttrDomain::Curve,
                                               CD_PROP_BOOL,
                                               BuiltinAttributeProvider::Deletable,
                                               curve_access,
                                               tag_component_topology_changed);

  static CurvesVertexGroupsAttributeProvider vertex_groups;
  static CustomDataAttributeProvider curve_custom_data(AttrDomain::Curve, curve_access);
  static CustomDataAttributeProvider point_custom_data(AttrDomain::Point, point_access);

  return GeometryAttributeProviders({&position,
                                     &radius,
                                     &id,
                                     &tilt,
                                     &handle_right,
                                     &handle_left,
                                     &handle_type_right,
                                     &handle_type_left,
                                     &normal_mode,
                                     &custom_normal,
                                     &nurbs_order,
                                     &nurbs_knots_mode,
                                     &nurbs_weight,
                                     &curve_type,
                                     &resolution,
                                     &cyclic},
                                    {&vertex_groups, &curve_custom_data, &point_custom_data});
}

/** \} */

static AttributeAccessorFunctions get_curves_accessor_functions()
{
  static const GeometryAttributeProviders providers = create_attribute_providers_for_curve();
  AttributeAccessorFunctions fn =
      attribute_accessor_functions::accessor_functions_for_providers<providers>();
  fn.domain_size = [](const void *owner, const AttrDomain domain) {
    if (owner == nullptr) {
      return 0;
    }
    const CurvesGeometry &curves = *static_cast<const CurvesGeometry *>(owner);
    switch (domain) {
      case AttrDomain::Point:
        return curves.points_num();
      case AttrDomain::Curve:
        return curves.curves_num();
      default:
        return 0;
    }
  };
  fn.domain_supported = [](const void * /*owner*/, const AttrDomain domain) {
    return ELEM(domain, AttrDomain::Point, AttrDomain::Curve);
  };
  fn.adapt_domain = [](const void *owner,
                       const GVArray &varray,
                       const AttrDomain from_domain,
                       const AttrDomain to_domain) -> GVArray {
    if (owner == nullptr) {
      return {};
    }
    const CurvesGeometry &curves = *static_cast<const CurvesGeometry *>(owner);
    return curves.adapt_domain(varray, from_domain, to_domain);
  };
  return fn;
}

const AttributeAccessorFunctions &get_attribute_accessor_functions()
{
  static const AttributeAccessorFunctions fn = get_curves_accessor_functions();
  return fn;
}

}  // namespace blender::bke::curves
