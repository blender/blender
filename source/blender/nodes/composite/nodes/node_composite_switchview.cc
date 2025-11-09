/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_listbase.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** SWITCH VIEW ******************** */

namespace blender::nodes::node_composite_switchview_cc {

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
    LISTBASE_FOREACH (SceneRenderView *, srv, &scene->r.views) {
      if (srv->viewflag & SCE_VIEW_DISABLE) {
        continue;
      }
      b.add_input<decl::Color>(srv->name)
          .default_value({0.0f, 0.0f, 0.0f, 1.0f})
          .structure_type(StructureType::Dynamic);
    }
  }
}

static void init_switch_view(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNode *node = (bNode *)ptr->data;

  /* store scene for dynamic declaration */
  node->id = (ID *)scene;
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
    if (context().get_view_name().is_empty()) {
      const Result &input = get_input(node().input(0)->identifier);
      result.share_data(input);
      return;
    }

    const Result &input = get_input(context().get_view_name());
    result.share_data(input);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new SwitchViewOperation(context, node);
}

}  // namespace blender::nodes::node_composite_switchview_cc

static void register_node_type_cmp_switch_view()
{
  namespace file_ns = blender::nodes::node_composite_switchview_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSwitchView", CMP_NODE_SWITCH_VIEW);
  ntype.ui_name = "Switch View";
  ntype.ui_description = "Combine the views (left and right) into a single stereo 3D output";
  ntype.enum_name_legacy = "VIEWSWITCH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::node_declare;
  ntype.initfunc_api = file_ns::init_switch_view;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_switch_view)
