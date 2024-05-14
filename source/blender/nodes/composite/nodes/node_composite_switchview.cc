/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BKE_context.hh"
#include "BKE_lib_id.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** SWITCH VIEW ******************** */

namespace blender::nodes::node_composite_switchview_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>(N_("Image"));

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
      b.add_input<decl::Color>(N_(srv->name)).default_value({0.0f, 0.0f, 0.0f, 1.0f});
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

static void node_composit_buts_switch_view_ex(uiLayout *layout,
                                              bContext * /*C*/,
                                              PointerRNA * /*ptr*/)
{
  uiItemFullO(layout,
              "NODE_OT_switch_view_update",
              "Update Views",
              ICON_FILE_REFRESH,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              UI_ITEM_NONE,
              nullptr);
}

using namespace blender::realtime_compositor;

class SwitchViewOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &result = get_result("Image");

    /* A context that is not multi view, pass the first input through as a fallback. */
    if (context().get_view_name().is_empty()) {
      Result &input = get_input(node().input(0)->identifier);
      input.pass_through(result);
      return;
    }

    Result &input = get_input(context().get_view_name());
    input.pass_through(result);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new SwitchViewOperation(context, node);
}

}  // namespace blender::nodes::node_composite_switchview_cc

void register_node_type_cmp_switch_view()
{
  namespace file_ns = blender::nodes::node_composite_switchview_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SWITCH_VIEW, "Switch View", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons_ex = file_ns::node_composit_buts_switch_view_ex;
  ntype.initfunc_api = file_ns::init_switch_view;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::nodeRegisterType(&ntype);
}
