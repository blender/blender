/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"

#include "BLI_sort.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_get_attribute_names_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_output<decl::String>("Names"_ustr).structure_type(StructureType::List);

  b.add_input<decl::Geometry>("Geometry"_ustr);

  {
    auto &p = b.add_panel("Filter Data Type"_ustr);
    p.add_input<decl::Bool>("Filter Data Type"_ustr).panel_toggle();
    p.add_input<decl::Menu>("Data Type"_ustr)
        .static_items(rna_enum_attrtype_items)
        .default_value(bke::AttrType::Float)
        .optional_label()
        .usage_by_panel_toggle();
  }
  {
    auto &p = b.add_panel("Filter Domain"_ustr);
    p.add_input<decl::Bool>("Filter Domain"_ustr).panel_toggle();
    p.add_input<decl::Menu>("Domain"_ustr)
        .static_items(rna_enum_attribute_domain_items)
        .default_value(AttrDomain::Point)
        .optional_label()
        .usage_by_panel_toggle();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry"_ustr);
  const bool filter_data_type = params.extract_input<bool>("Filter Data Type"_ustr);
  const bool filter_domain = params.extract_input<bool>("Filter Domain"_ustr);

  const bke::AttrType data_type = params.extract_input<bke::AttrType>("Data Type"_ustr);
  const AttrDomain domain = params.extract_input<AttrDomain>("Domain"_ustr);

  VectorSet<std::string> names_set;

  for (const GeometryComponent *component : geometry_set.get_components()) {
    const std::optional<AttributeAccessor> attributes = component->attributes();
    if (!attributes) {
      continue;
    }
    attributes->foreach_attribute([&](const AttributeIter &iter) {
      if (filter_data_type) {
        if (iter.data_type != data_type) {
          return;
        }
      }
      if (filter_domain) {
        if (iter.domain != domain) {
          return;
        }
      }
      if (iter.name.startswith(".")) {
        return;
      }
      names_set.add(iter.name);
    });
  }

  Vector<std::string> names = names_set.extract_vector();
  parallel_sort(
      names.begin(), names.end(), [](const StringRef a, const StringRef b) { return a < b; });
  params.set_output("Names"_ustr, GList::from_container(names));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGetAttributeNames"_ustr);
  ntype.ui_name = "Get Attribute Names";
  ntype.ui_description = "Retrieves attribute names as a list of strings";
  ntype.nclass = NODE_CLASS_ATTRIBUTE;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_type_size(ntype, 160, 100, NODE_DEFAULT_MAX_WIDTH);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_get_attribute_names_cc
