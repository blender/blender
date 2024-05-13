/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include <fmt/format.h>

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_remove_attribute_cc {

enum class PatternMode {
  Exact,
  Wildcard,
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::String>("Name").is_attribute_name();
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "pattern_mode", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const std::string pattern = params.extract_input<std::string>("Name");
  if (pattern.empty()) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }
  const bNode &node = params.node();
  PatternMode pattern_mode = PatternMode(node.custom1);
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

  StringRef wildcard_prefix;
  StringRef wildcard_suffix;
  if (pattern_mode == PatternMode::Wildcard) {
    const int wildcard_index = pattern.find('*');
    wildcard_prefix = StringRef(pattern).substr(0, wildcard_index);
    wildcard_suffix = StringRef(pattern).substr(wildcard_index + 1);
  }

  Set<std::string> removed_attributes;
  Set<std::string> failed_attributes;

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                               GeometryComponent::Type::PointCloud,
                                               GeometryComponent::Type::Curve,
                                               GeometryComponent::Type::Instance})
    {
      if (!geometry_set.has(type)) {
        continue;
      }
      /* First check if the attribute exists before getting write access,
       * to avoid potentially expensive unnecessary copies. */
      const GeometryComponent &read_only_component = *geometry_set.get_component(type);
      Vector<std::string> attributes_to_remove;
      switch (pattern_mode) {
        case PatternMode::Exact: {
          if (read_only_component.attributes()->contains(pattern)) {
            attributes_to_remove.append(pattern);
          }
          break;
        }
        case PatternMode::Wildcard: {
          read_only_component.attributes()->for_all(
              [&](const blender::bke::AttributeIDRef &id,
                  const blender::bke::AttributeMetaData /*meta_data*/) {
                if (id.is_anonymous()) {
                  return true;
                }
                const StringRef attribute_name = id.name();
                if (attribute_name.startswith(wildcard_prefix) &&
                    attribute_name.endswith(wildcard_suffix))
                {
                  attributes_to_remove.append(attribute_name);
                }
                return true;
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
          removed_attributes.add(attribute_name);
        }
        else {
          failed_attributes.add(attribute_name);
        }
      }
    }
  });

  for (const StringRef attribute_name : removed_attributes) {
    params.used_named_attribute(attribute_name, NamedAttributeUsage::Remove);
  }

  if (!failed_attributes.is_empty()) {
    Vector<std::string> quoted_attribute_names;
    for (const StringRef attribute_name : failed_attributes) {
      quoted_attribute_names.append(fmt::format("\"{}\"", attribute_name));
    }
    const std::string message = fmt::format(TIP_("Cannot remove built-in attributes: {}"),
                                            fmt::join(quoted_attribute_names, ", "));
    params.error_message_add(NodeWarningType::Warning, message);
  }
  else if (removed_attributes.is_empty() && pattern_mode == PatternMode::Exact) {
    const std::string message = fmt::format(TIP_("Attribute does not exist: \"{}\""), pattern);
    params.error_message_add(NodeWarningType::Warning, message);
  }

  params.set_output("Geometry", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem pattern_mode_items[] = {
      {int(PatternMode::Exact),
       "EXACT",
       0,
       "Exact",
       "Remove the one attribute with the given name"},
      {int(PatternMode::Wildcard),
       "WILDCARD",
       0,
       "Wildcard",
       "Remove all attributes that match the pattern which is allowed to contain a single "
       "wildcard (*)"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  RNA_def_node_enum(srna,
                    "pattern_mode",
                    "Pattern Mode",
                    "How the attributes to remove are chosen",
                    pattern_mode_items,
                    NOD_inline_enum_accessors(custom1));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_REMOVE_ATTRIBUTE, "Remove Named Attribute", NODE_CLASS_ATTRIBUTE);
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  bke::node_type_size(&ntype, 170, 100, 700);
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_remove_attribute_cc
