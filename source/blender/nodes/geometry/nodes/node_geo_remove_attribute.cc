/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_instances.hh"

#include "NOD_string_pattern.hh"

#include "node_geometry_util.hh"

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace blender::nodes::node_geo_remove_attribute_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Geometry"_ustr).description("Geometry to remove attributes from");
  b.add_output<decl::Geometry>("Geometry"_ustr).propagate_all_geometry().align_with_previous();
  b.add_input<decl::Menu>("Pattern Mode"_ustr)
      .static_items(string_pattern_mode_items)
      .optional_label()
      .description("How the attributes to remove are chosen");
  b.add_input<decl::String>("Name"_ustr).is_attribute_name().optional_label();
}

struct RemoveAttributeParams {
  StringPattern pattern;

  Set<std::string> removed_attributes;
  Set<std::string> failed_attributes;
};

static void remove_attributes_recursive(GeometrySet &geometry_set, RemoveAttributeParams &params)
{
  for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                             GeometryComponent::Type::PointCloud,
                                             GeometryComponent::Type::Curve,
                                             GeometryComponent::Type::Instance,
                                             GeometryComponent::Type::GreasePencil})
  {
    if (!geometry_set.has(type)) {
      continue;
    }
    /* First check if the attribute exists before getting write access,
     * to avoid potentially expensive unnecessary copies. */
    const GeometryComponent &read_only_component = *geometry_set.get_component(type);
    Vector<std::string> attributes_to_remove;
    if (const std::optional<StringRef> exact_pattern = params.pattern.exact_pattern()) {
      if (read_only_component.attributes()->contains(*exact_pattern)) {
        attributes_to_remove.append(*exact_pattern);
      }
    }
    else {
      read_only_component.attributes()->foreach_attribute([&](const bke::AttributeIter &iter) {
        const StringRef attribute_name = iter.name;
        if (bke::attribute_name_is_anonymous(attribute_name)) {
          return;
        }
        if (params.pattern.match(attribute_name)) {
          attributes_to_remove.append(attribute_name);
        }
      });
    }

    if (attributes_to_remove.is_empty()) {
      continue;
    }

    GeometryComponent &component = geometry_set.get_component_for_write(type);
    for (const StringRef attribute_name : attributes_to_remove) {
      if (!bke::allow_procedural_attribute_access(attribute_name)) {
        continue;
      }
      if (component.attributes_for_write()->remove(attribute_name)) {
        params.removed_attributes.add(attribute_name);
      }
      else {
        params.failed_attributes.add(attribute_name);
      }
    }
  }

  if (bke::Instances *instances = geometry_set.get_instances_for_write()) {
    instances->ensure_geometry_instances();
    for (bke::InstanceReference &reference : instances->references_for_write()) {
      if (reference.type() == bke::InstanceReference::Type::GeometrySet) {
        remove_attributes_recursive(reference.geometry_set(), params);
      }
    }
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry"_ustr);
  const std::string pattern = params.extract_input<std::string>("Name"_ustr);
  if (pattern.empty()) {
    params.set_output("Geometry"_ustr, std::move(geometry_set));
    return;
  }

  std::string pattern_error;
  std::optional<StringPattern> pattern_fn = StringPattern::from_str(
      params.get_input<StringPatternMode>("Pattern Mode"_ustr), pattern, pattern_error);
  if (!pattern_fn) {
    params.error_message_add(NodeWarningType::Error, pattern_error);
    params.set_output("Geometry"_ustr, std::move(geometry_set));
    return;
  }

  RemoveAttributeParams removal_params{*pattern_fn};
  remove_attributes_recursive(geometry_set, removal_params);

  for (const StringRef attribute_name : removal_params.removed_attributes) {
    params.used_named_attribute(attribute_name, NamedAttributeUsage::Remove);
  }

  if (!removal_params.failed_attributes.is_empty()) {
    Vector<std::string> quoted_attribute_names;
    for (const StringRef attribute_name : removal_params.failed_attributes) {
      quoted_attribute_names.append(fmt::format("\"{}\"", attribute_name));
    }
    const std::string message = fmt::format(
        fmt::runtime(TIP_("Cannot remove built-in attributes: {}")),
        fmt::join(quoted_attribute_names, ", "));
    params.error_message_add(NodeWarningType::Warning, message);
  }
  else if (removal_params.removed_attributes.is_empty()) {
    if (const std::optional<StringRef> exact_pattern = removal_params.pattern.exact_pattern()) {
      const std::string message = fmt::format(
          fmt::runtime(TIP_("Attribute does not exist: \"{}\"")), *exact_pattern);
      params.error_message_add(NodeWarningType::Warning, message);
    }
  }

  params.set_output("Geometry"_ustr, std::move(geometry_set));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeRemoveAttribute"_ustr, GEO_NODE_REMOVE_ATTRIBUTE);
  ntype.ui_name = "Remove Named Attribute";
  ntype.ui_description =
      "Delete an attribute with a specified name from a geometry. Typically used to optimize "
      "performance";
  ntype.enum_name_legacy = "REMOVE_ATTRIBUTE";
  ntype.nclass = NODE_CLASS_ATTRIBUTE;
  ntype.declare = node_declare;
  ntype.default_width = bke::NodeWidth::_180;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_remove_attribute_cc
