/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"

#include "DNA_object_types.h"

#include "BKE_attribute_storage.hh"
#include "BKE_curves.hh"
#include "BKE_deform.hh"

#include "FN_multi_function_builder.hh"

#include "attribute_storage_access.hh"

namespace blender::bke::curves {

static void tag_topology_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.tag_topology_changed();
}

static void tag_curve_types_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.update_curve_types();
  curves.tag_topology_changed();
}

static void tag_positions_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.tag_positions_changed();
}

static void tag_radii_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.tag_radii_changed();
}

static void tag_normals_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.tag_normals_changed();
}

static void tag_material_index_changed(void *owner)
{
  CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
  curves.tag_material_index_changed();
}

static const auto &changed_tags()
{
  static Map<StringRef, AttrUpdateOnChange> attributes{
      {"position", tag_positions_changed},
      {"radius", tag_radii_changed},
      {"tilt", tag_normals_changed},
      {"handle_left", tag_positions_changed},
      {"handle_right", tag_positions_changed},
      {"handle_type_left", tag_topology_changed},
      {"handle_type_right", tag_topology_changed},
      {"nurbs_weight", tag_positions_changed},
      {"nurbs_order", tag_topology_changed},
      {"normal_mode", tag_normals_changed},
      {"custom_normal", tag_normals_changed},
      {"curve_type", tag_curve_types_changed},
      {"resolution", tag_topology_changed},
      {"cyclic", tag_topology_changed},
      {"material_index", tag_material_index_changed},
  };
  return attributes;
}

static int get_domain_size(const void *owner, const AttrDomain domain)
{
  const CurvesGeometry &curves = *static_cast<const CurvesGeometry *>(owner);
  switch (domain) {
    case AttrDomain::Point:
      return curves.points_num();
    case AttrDomain::Curve:
      return curves.curves_num();
    default:
      return 0;
  }
}

static GAttributeReader reader_for_vertex_group_index(const CurvesGeometry &curves,
                                                      const Span<MDeformVert> dverts,
                                                      const int vertex_group_index)
{
  BLI_assert(vertex_group_index >= 0);
  if (dverts.is_empty()) {
    return {VArray<float>::from_single(0.0f, curves.points_num()), AttrDomain::Point};
  }
  return {varray_for_deform_verts(dverts, vertex_group_index), AttrDomain::Point};
}

static GAttributeReader try_get_vertex_group(const void *owner, const StringRef attribute_id)
{
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
  return reader_for_vertex_group_index(*curves, dverts, vertex_group_index);
}

static GAttributeWriter try_get_vertex_group_for_write(void *owner, const StringRef attribute_id)
{
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

static bool try_delete_vertex_group(void *owner, const StringRef name)
{
  CurvesGeometry *curves = static_cast<CurvesGeometry *>(owner);
  if (curves == nullptr) {
    return true;
  }

  int index;
  bDeformGroup *group;
  if (!BKE_defgroup_listbase_name_find(&curves->vertex_group_names, name, &index, &group)) {
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

static bool foreach_vertex_group(const void *owner, FunctionRef<void(const AttributeIter &)> fn)
{
  const CurvesGeometry *curves = static_cast<const CurvesGeometry *>(owner);
  if (curves == nullptr) {
    return true;
  }
  const AttributeAccessor accessor = curves->attributes();
  const Span<MDeformVert> dverts = curves->deform_verts();
  int group_index = 0;
  LISTBASE_FOREACH_INDEX (const bDeformGroup *, group, &curves->vertex_group_names, group_index) {
    const auto get_fn = [&]() {
      return reader_for_vertex_group_index(*curves, dverts, group_index);
    };
    AttributeIter iter{group->name, AttrDomain::Point, bke::AttrType::Float, get_fn};
    iter.is_builtin = false;
    iter.accessor = &accessor;
    fn(iter);
    if (iter.is_stopped()) {
      return false;
    }
  }
  return true;
}

static const auto &builtin_attributes()
{
  static auto attributes = []() {
    Map<StringRef, AttrBuiltinInfo> map;

    AttrBuiltinInfo position(AttrDomain::Point, AttrType::Float3);
    position.deletable = false;
    map.add_new("position", std::move(position));

    AttrBuiltinInfo radius(AttrDomain::Point, AttrType::Float);
    map.add_new("radius", std::move(radius));

    AttrBuiltinInfo tilt(AttrDomain::Point, AttrType::Float);
    map.add_new("tilt", std::move(tilt));

    AttrBuiltinInfo handle_left(AttrDomain::Point, AttrType::Float3);
    map.add_new("handle_left", std::move(handle_left));

    AttrBuiltinInfo handle_right(AttrDomain::Point, AttrType::Float3);
    map.add_new("handle_right", std::move(handle_right));

    static auto handle_type_clamp = mf::build::SI1_SO<int8_t, int8_t>(
        "Handle Type Validate",
        [](int8_t value) {
          return std::clamp<int8_t>(value, BEZIER_HANDLE_FREE, BEZIER_HANDLE_ALIGN);
        },
        mf::build::exec_presets::AllSpanOrSingle());

    AttrBuiltinInfo handle_type_left(AttrDomain::Point, AttrType::Int8);
    handle_type_left.validator = AttributeValidator{&handle_type_clamp};
    map.add_new("handle_type_left", std::move(handle_type_left));

    AttrBuiltinInfo handle_type_right(AttrDomain::Point, AttrType::Int8);
    handle_type_right.validator = AttributeValidator{&handle_type_clamp};
    map.add_new("handle_type_right", std::move(handle_type_right));

    static float default_nurbs_weight = 1.0f;
    AttrBuiltinInfo nurbs_weight(AttrDomain::Point, AttrType::Float);
    nurbs_weight.default_value = &default_nurbs_weight;
    map.add_new("nurbs_weight", std::move(nurbs_weight));

    static const auto nurbs_order_clamp = mf::build::SI1_SO<int8_t, int8_t>(
        "NURBS Order Validate",
        [](int8_t value) { return std::max<int8_t>(value, 1); },
        mf::build::exec_presets::AllSpanOrSingle());
    static int nurbs_order_default = 4;
    AttrBuiltinInfo nurbs_order(AttrDomain::Curve, AttrType::Int8);
    nurbs_order.default_value = &nurbs_order_default;
    nurbs_order.validator = AttributeValidator{&nurbs_order_clamp};
    map.add_new("nurbs_order", std::move(nurbs_order));

    static const auto normal_mode_clamp = mf::build::SI1_SO<int8_t, int8_t>(
        "Normal Mode Validate",
        [](int8_t value) {
          return std::clamp<int8_t>(value, NORMAL_MODE_MINIMUM_TWIST, NORMAL_MODE_FREE);
        },
        mf::build::exec_presets::AllSpanOrSingle());
    AttrBuiltinInfo normal_mode(AttrDomain::Curve, AttrType::Int8);
    normal_mode.validator = AttributeValidator{&normal_mode_clamp};
    map.add_new("normal_mode", std::move(normal_mode));

    AttrBuiltinInfo custom_normal(AttrDomain::Point, AttrType::Float3);
    map.add_new("custom_normal", std::move(custom_normal));

    static const auto knots_mode_clamp = mf::build::SI1_SO<int8_t, int8_t>(
        "Knots Mode Validate",
        [](int8_t value) {
          return std::clamp<int8_t>(
              value, NURBS_KNOT_MODE_NORMAL, NURBS_KNOT_MODE_ENDPOINT_BEZIER);
        },
        mf::build::exec_presets::AllSpanOrSingle());
    AttrBuiltinInfo knots_mode(AttrDomain::Curve, AttrType::Int8);
    knots_mode.validator = AttributeValidator{&knots_mode_clamp};
    map.add_new("knots_mode", std::move(knots_mode));

    static const auto curve_type_clamp = mf::build::SI1_SO<int8_t, int8_t>(
        "Curve Type Validate",
        [](int8_t value) {
          return std::clamp<int8_t>(value, CURVE_TYPE_CATMULL_ROM, CURVE_TYPES_NUM);
        },
        mf::build::exec_presets::AllSpanOrSingle());
    AttrBuiltinInfo curve_type(AttrDomain::Curve, AttrType::Int8);
    curve_type.validator = AttributeValidator{&curve_type_clamp};
    map.add_new("curve_type", std::move(curve_type));

    static const auto resolution_clamp = mf::build::SI1_SO<int, int>(
        "Resolution Validate",
        [](int value) { return std::max<int>(value, 1); },
        mf::build::exec_presets::AllSpanOrSingle());
    static int resolution_default = 12;
    AttrBuiltinInfo resolution(AttrDomain::Curve, AttrType::Int32);
    resolution.default_value = &resolution_default;
    resolution.validator = AttributeValidator{&resolution_clamp};
    map.add_new("resolution", std::move(resolution));

    AttrBuiltinInfo cyclic(AttrDomain::Curve, AttrType::Bool);
    map.add_new("cyclic", std::move(cyclic));

    static const auto material_index_clamp = mf::build::SI1_SO<int, int>(
        "Material Index Validate",
        [](int value) {
          /* Use #short for the maximum since many areas still use that type for indices. */
          return std::clamp<int>(value, 0, std::numeric_limits<short>::max());
        },
        mf::build::exec_presets::AllSpanOrSingle());
    AttrBuiltinInfo material_index(AttrDomain::Curve, AttrType::Int32);
    material_index.validator = AttributeValidator{&material_index_clamp};
    map.add_new("material_index", std::move(material_index));

    return map;
  }();
  return attributes;
}

/** \} */

static AttributeAccessorFunctions get_curves_accessor_functions()
{
  AttributeAccessorFunctions fn{};
  fn.domain_supported = [](const void * /*owner*/, const AttrDomain domain) {
    return ELEM(domain, AttrDomain::Point, AttrDomain::Curve);
  };
  fn.domain_size = get_domain_size;
  fn.builtin_domain_and_type = [](const void * /*owner*/,
                                  const StringRef name) -> std::optional<AttributeDomainAndType> {
    const AttrBuiltinInfo *info = builtin_attributes().lookup_ptr(name);
    if (!info) {
      return std::nullopt;
    }
    return AttributeDomainAndType{info->domain, info->type};
  };
  fn.get_builtin_default = [](const void * /*owner*/, StringRef name) -> GPointer {
    const AttrBuiltinInfo &info = builtin_attributes().lookup(name);
    return info.default_value;
  };
  fn.lookup = [](const void *owner, const StringRef name) -> GAttributeReader {
    const CurvesGeometry &curves = *static_cast<const CurvesGeometry *>(owner);

    if (GAttributeReader vertex_group = try_get_vertex_group(owner, name)) {
      return vertex_group;
    }

    const AttributeStorage &storage = curves.attribute_storage.wrap();
    const Attribute *attr = storage.lookup(name);
    if (!attr) {
      return {};
    }
    const int domain_size = get_domain_size(owner, attr->domain());
    return attribute_to_reader(*attr, attr->domain(), domain_size);
  };
  fn.adapt_domain = [](const void *owner,
                       const GVArray &varray,
                       const AttrDomain from_domain,
                       const AttrDomain to_domain) -> GVArray {
    const CurvesGeometry &curves = *static_cast<const CurvesGeometry *>(owner);
    return curves.adapt_domain(varray, from_domain, to_domain);
  };
  fn.foreach_attribute = [](const void *owner,
                            const FunctionRef<void(const AttributeIter &)> fn,
                            const AttributeAccessor &accessor) {
    const CurvesGeometry &curves = *static_cast<const CurvesGeometry *>(owner);

    const bool should_continue = foreach_vertex_group(
        owner, [&](const AttributeIter &iter) { fn(iter); });
    if (!should_continue) {
      return;
    }

    const AttributeStorage &storage = curves.attribute_storage.wrap();
    storage.foreach_with_stop([&](const Attribute &attr) {
      const auto get_fn = [&]() {
        const int domain_size = get_domain_size(owner, attr.domain());
        return attribute_to_reader(attr, attr.domain(), domain_size);
      };
      AttributeIter iter(attr.name(), attr.domain(), attr.data_type(), get_fn);
      iter.is_builtin = builtin_attributes().contains(attr.name());
      iter.accessor = &accessor;
      fn(iter);
      return !iter.is_stopped();
    });
  };
  fn.lookup_validator = [](const void * /*owner*/, const StringRef name) -> AttributeValidator {
    const AttrBuiltinInfo *info = builtin_attributes().lookup_ptr(name);
    if (!info) {
      return {};
    }
    return info->validator;
  };
  fn.lookup_for_write = [](void *owner, const StringRef name) -> GAttributeWriter {
    CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);

    if (GAttributeWriter vertex_group = try_get_vertex_group_for_write(owner, name)) {
      return vertex_group;
    }

    AttributeStorage &storage = curves.attribute_storage.wrap();
    Attribute *attr = storage.lookup(name);
    if (!attr) {
      return {};
    }
    const int domain_size = get_domain_size(owner, attr->domain());
    return attribute_to_writer(&curves, changed_tags(), domain_size, *attr);
  };
  fn.remove = [](void *owner, const StringRef name) -> bool {
    CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);

    if (try_delete_vertex_group(owner, name)) {
      return true;
    }

    AttributeStorage &storage = curves.attribute_storage.wrap();
    if (const AttrBuiltinInfo *info = builtin_attributes().lookup_ptr(name)) {
      if (!info->deletable) {
        return false;
      }
    }
    const std::optional<AttrUpdateOnChange> fn = changed_tags().lookup_try(name);
    const bool removed = storage.remove(name);
    if (!removed) {
      return false;
    }
    if (fn) {
      (*fn)(owner);
    }
    return true;
  };
  fn.add = [](void *owner,
              const StringRef name,
              const AttrDomain domain,
              const AttrType type,
              const AttributeInit &initializer) {
    CurvesGeometry &curves = *static_cast<CurvesGeometry *>(owner);
    const int domain_size = get_domain_size(owner, domain);
    AttributeStorage &storage = curves.attribute_storage.wrap();
    if (const AttrBuiltinInfo *info = builtin_attributes().lookup_ptr(name)) {
      if (info->domain != domain || info->type != type) {
        return false;
      }
    }
    if (storage.lookup(name)) {
      return false;
    }
    storage.add(name, domain, type, attribute_init_to_data(type, domain_size, initializer));
    if (initializer.type != AttributeInit::Type::Construct) {
      if (const std::optional<AttrUpdateOnChange> fn = changed_tags().lookup_try(name)) {
        (*fn)(owner);
      }
    }
    return true;
  };

  return fn;
}

const AttributeAccessorFunctions &get_attribute_accessor_functions()
{
  static const AttributeAccessorFunctions fn = get_curves_accessor_functions();
  return fn;
}

}  // namespace blender::bke::curves
