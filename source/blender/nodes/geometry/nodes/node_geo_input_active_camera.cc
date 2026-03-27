/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DEG_depsgraph_query.hh"

#include "COM_node_operation.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_active_camera_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Object>("Active Camera")
      .description("The camera used for rendering the scene");
}

static void node_exec(GeoNodeExecParams params)
{
  const Scene *scene = DEG_get_evaluated_scene(params.depsgraph());
  Object *camera = DEG_get_evaluated(params.depsgraph(), scene->camera);
  params.set_output("Active Camera"_ustr, camera);
}

using namespace blender::compositor;

class ActiveCameraOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &output = this->get_result("Active Camera");
    output.allocate_single_value();
    output.set_single_value(this->context().get_scene().camera);
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new ActiveCameraOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_cmp_node_type_base(&ntype, "GeometryNodeInputActiveCamera", GEO_NODE_INPUT_ACTIVE_CAMERA);
  ntype.ui_name = "Active Camera";
  ntype.ui_description = "Retrieve the scene's active camera";
  ntype.enum_name_legacy = "INPUT_ACTIVE_CAMERA";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_exec;
  ntype.declare = node_declare;
  ntype.get_compositor_operation = get_compositor_operation;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_active_camera_cc
