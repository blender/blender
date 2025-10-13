/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_instances.hh"
#include "node_geometry_util.hh"

#include <fmt/format.h>

namespace blender::nodes::node_geo_remove_attribute_cc {

enum class PatternMode {
  Exact,
  Wildcard,
};

static const EnumPropertyItem pattern_mode_items[] = {
    {int(PatternMode::Exact),
     "EXACT",
     0,
     N_("Exact"),
     N_("Remove the one attribute with the given name")},
    {int(PatternMode::Wildcard),
     "WILDCARD",
     0,
     N_("Wildcard"),
     N_("Remove all attributes that match the pattern which is allowed to contain a single "
        "wildcard (*)")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Geometry").description("Geometry to remove attributes from");
  b.add_output<decl::Geometry>("Geometry").propagate_all().align_with_previous();
  b.add_input<decl::Menu>("Pattern Mode")
      .static_items(pattern_mode_items)
      .optional_label()
      .description("How the attributes to remove are chosen");
  b.add_input<decl::String>("Name").is_attribute_name().optional_label();
}

struct RemoveAttributeParams {
  PatternMode pattern_mode;
  std::string pattern;
  std::string wildcard_prefix;
  std::string wildcard_suffix;

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
    switch (params.pattern_mode) {
      case PatternMode::Exact: {
        if (read_only_component.attributes()->contains(params.pattern)) {
          attributes_to_remove.append(params.pattern);
        }
        break;
      }
      case PatternMode::Wildcard: {
        read_only_component.attributes()->foreach_attribute([&](const bke::AttributeIter &iter) {
          const StringRef attribute_name = iter.name;
          if (bke::attribute_name_is_anonymous(attribute_name)) {
            return;
          }
          if (attribute_name.startswith(params.wildcard_prefix) &&
              attribute_name.endswith(params.wildcard_suffix))
          {
            attributes_to_remove.append(attribute_name);
          }
        });

        break;
      }
    }
    if (attributes_to_remove.is_empty()) {
      break;
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
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const std::string pattern = params.extract_input<std::string>("Name");
  if (pattern.empty()) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  PatternMode pattern_mode = params.get_input<PatternMode>("Pattern Mode");
  if (pattern_mode == PatternMode::Wildcard) {
    const int wildcard_count = Span(pattern.c_str(), pattern.size()).count('*');
    if (wildcard_count == 0) {
      pattern_mode = PatternMode::Exact;
    }
    else if (wildcard_count >= 2) {
      params.error_message_add(NodeWarningType::Info,
                               TIP_("Only one * is supported in the pattern"));
      params.set_output("Geometry", std::move(geometry_set));
      return;
    }
  }

  RemoveAttributeParams removal_params;
  removal_params.pattern_mode = pattern_mode;
  removal_params.pattern = pattern;
  if (pattern_mode == PatternMode::Wildcard) {
    const int wildcard_index = pattern.find('*');
    removal_params.wildcard_prefix = StringRef(pattern).substr(0, wildcard_index);
    removal_params.wildcard_suffix = StringRef(pattern).substr(wildcard_index + 1);
  }

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
  else if (removal_params.removed_attributes.is_empty() && pattern_mode == PatternMode::Exact) {
    const std::string message = fmt::format(fmt::runtime(TIP_("Attribute does not exist: \"{}\"")),
                                            pattern);
    params.error_message_add(NodeWarningType::Warning, message);
  }

  params.set_output("Geometry", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeRemoveAttribute", GEO_NODE_REMOVE_ATTRIBUTE);
  ntype.ui_name = "Remove Named Attribute";
  ntype.ui_description =
      "Delete an attribute with a specified name from a geometry. Typically used to optimize "
      "performance";
  ntype.enum_name_legacy = "REMOVE_ATTRIBUTE";
  ntype.nclass = NODE_CLASS_ATTRIBUTE;
  ntype.declare = node_declare;
  bke::node_type_size(ntype, 170, 100, 700);
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_remove_attribute_cc
