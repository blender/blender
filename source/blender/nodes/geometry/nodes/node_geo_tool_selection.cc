/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_tool_selection_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Selection").field_source();
}

class ToolSelectionFieldInput final : public bke::GeometryFieldInput {
 public:
  ToolSelectionFieldInput() : bke::GeometryFieldInput(CPPType::get<bool>(), "Operator Selection")
  {
    category_ = Category::NamedAttribute;
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask & /*mask*/) const final
  {
    const eAttrDomain domain = context.domain();
    const AttributeAccessor attributes = *context.attributes();
    switch (context.type()) {
      case GeometryComponent::Type::Curve:
      case GeometryComponent::Type::PointCloud:
        return *attributes.lookup_or_default<bool>(".selection", domain, true);
      case GeometryComponent::Type::Mesh:
        switch (domain) {
          case ATTR_DOMAIN_POINT:
            return *attributes.lookup_or_default<bool>(".select_vert", domain, false);
          case ATTR_DOMAIN_EDGE:
            return *attributes.lookup_or_default<bool>(".select_edge", domain, false);
          case ATTR_DOMAIN_FACE:
          case ATTR_DOMAIN_CORNER:
            return *attributes.lookup_or_default<bool>(".select_poly", domain, false);
          default:
            BLI_assert_unreachable();
            return {};
        }
      default:
        return {};
    }
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!check_tool_context_and_error(params)) {
    return;
  }
  params.set_output("Selection", Field<bool>(std::make_shared<ToolSelectionFieldInput>()));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_TOOL_SELECTION, "Selection", NODE_CLASS_INPUT);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.gather_add_node_search_ops = search_link_ops_for_for_tool_node;
  ntype.gather_link_search_ops = search_link_ops_for_tool_node;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_tool_selection_cc
