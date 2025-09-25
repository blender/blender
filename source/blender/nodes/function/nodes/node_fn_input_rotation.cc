/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_euler.hh"

#include "NOD_geometry_nodes_gizmos.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_input_rotation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Rotation>("Rotation").custom_draw([](CustomSocketDrawParams &params) {
    uiLayout &row = params.layout.row(true);
    row.column(true).prop(
        &params.node_ptr, "rotation_euler", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
    if (gizmos::value_node_has_gizmo(params.tree, params.node)) {
      row.prop(&params.socket_ptr, "pin_gizmo", UI_ITEM_NONE, "", ICON_GIZMO);
    }
  });
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  const NodeInputRotation &node_storage = *static_cast<const NodeInputRotation *>(bnode.storage);
  const math::EulerXYZ euler_rotation(node_storage.rotation_euler[0],
                                      node_storage.rotation_euler[1],
                                      node_storage.rotation_euler[2]);
  builder.construct_and_set_matching_fn<mf::CustomMF_Constant<math::Quaternion>>(
      math::to_quaternion(euler_rotation));
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeInputRotation *data = MEM_callocN<NodeInputRotation>(__func__);
  node->storage = data;
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeInputRotation", FN_NODE_INPUT_ROTATION);
  ntype.ui_name = "Rotation";
  ntype.ui_description =
      "Provide a rotation value that can be connected to other nodes in the tree";
  ntype.enum_name_legacy = "INPUT_ROTATION";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeInputRotation", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_input_rotation_cc
