/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.hh"

#include "RNA_enum_types.hh"

#include "BKE_mesh.hh"
#include "BKE_node.hh"

#include "DNA_meshdata_types.h"

#include "NOD_rna_define.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_tool_active_element_cc {

NODE_STORAGE_FUNCS(NodeGeometryToolActiveElement)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Index").description(
      "Index of the active element in the specified domain");
  b.add_output<decl::Bool>("Exists").description(
      "True if an active element exists in the mesh, false otherwise");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryToolActiveElement *data = MEM_cnew<NodeGeometryToolActiveElement>(__func__);
  data->domain = int16_t(AttrDomain::Point);
  node->storage = data;
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "",
                    rna_enum_attribute_domain_only_mesh_no_corner_items,
                    NOD_storage_enum_accessors(domain),
                    int(AttrDomain::Point));
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_exec(GeoNodeExecParams params)
{
  if (!check_tool_context_and_error(params)) {
    return;
  }

  if (params.user_data()->call_data->operator_data->mode != OB_MODE_EDIT) {
    params.set_default_remaining_outputs();
    return;
  }

  const GeoNodesOperatorData *operator_data = params.user_data()->call_data->operator_data;

  switch (static_cast<AttrDomain>(node_storage(params.node()).domain)) {
    case AttrDomain::Point:
      params.set_output("Exists", operator_data->active_point_index >= 0);
      params.set_output("Index", std::max(0, operator_data->active_point_index));
      break;
    case AttrDomain::Edge:
      params.set_output("Exists", operator_data->active_edge_index >= 0);
      params.set_output("Index", std::max(0, operator_data->active_edge_index));
      break;
    case AttrDomain::Face:
      params.set_output("Exists", operator_data->active_face_index >= 0);
      params.set_output("Index", std::max(0, operator_data->active_face_index));
      break;
    default:
      params.set_default_remaining_outputs();
      BLI_assert_unreachable();
      break;
  }
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_TOOL_ACTIVE_ELEMENT, "Active Element", NODE_CLASS_INPUT);
  node_type_storage(&ntype,
                    "NodeGeometryToolActiveElement",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_exec;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = search_link_ops_for_tool_node;
  ntype.draw_buttons = node_layout;
  blender::bke::nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_tool_active_element_cc
