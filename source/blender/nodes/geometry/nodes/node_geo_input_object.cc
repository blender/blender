/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_object_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Object>("Object").custom_draw([](CustomSocketDrawParams &params) {
    params.layout.alignment_set(ui::LayoutAlign::Expand);
    params.layout.prop(&params.node_ptr, "object", ui::ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Object *object = reinterpret_cast<Object *>(params.node().id);
  params.set_output("Object"_ustr, object);
}

using namespace blender::compositor;

class InputObjectOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Object *object = reinterpret_cast<Object *>(this->node().id);
    Result &result = this->get_result("Object");
    result.allocate_single_value();
    result.set_single_value(object);
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new InputObjectOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_cmp_node_type_base(&ntype, "GeometryNodeInputObject", GEO_NODE_INPUT_OBJECT);
  ntype.ui_name = "Object";
  ntype.ui_description = "Output a single object";
  ntype.enum_name_legacy = "INPUT_OBJECT";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.get_compositor_operation = get_compositor_operation;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_object_cc
