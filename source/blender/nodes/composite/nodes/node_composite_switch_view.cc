/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"

#include "DNA_node_types.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_switch_view_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>(N_("Image")).structure_type(StructureType::Dynamic);

  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }

  Scene *scene = reinterpret_cast<Scene *>(node->id);

  if (scene != nullptr) {
    /* add the new views */
    for (SceneRenderView &srv : scene->r.views) {
      if (srv.viewflag & SCE_VIEW_DISABLE) {
        continue;
      }
      b.add_input<decl::Color>(srv.name)
          .default_value({0.0f, 0.0f, 0.0f, 1.0f})
          .structure_type(StructureType::Dynamic);
    }
  }
}

static void node_init(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNode *node = (bNode *)ptr->data;

  /* store scene for dynamic declaration */
  node->id = reinterpret_cast<ID *>(scene);
  id_us_plus(node->id);
}

using namespace blender::compositor;

class SwitchViewOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &result = get_result("Image");

    /* A context that is not multi view, pass the first input through as a fallback. */
    if (this->context().get_view_name().is_empty()) {
      const Result &input = this->get_input(this->node().input_socket(0).identifier);
      result.share_data(input);
      return;
    }

    const Result &input = this->get_input(this->context().get_view_name());
    result.share_data(input);
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new SwitchViewOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSwitchView", CMP_NODE_SWITCH_VIEW);
  ntype.ui_name = "Switch View";
  ntype.ui_description = "Combine the views (left and right) into a single stereo 3D output";
  ntype.enum_name_legacy = "VIEWSWITCH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc_api = node_init;
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_switch_view_cc
