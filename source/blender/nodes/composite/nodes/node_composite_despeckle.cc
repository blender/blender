/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "GPU_shader.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** FILTER  ******************** */

namespace blender::nodes::node_composite_despeckle_cc {

static void cmp_node_despeckle_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_despeckle(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom3 = 0.5f;
  node->custom4 = 0.5f;
}

static void node_composit_buts_despeckle(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "threshold", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "threshold_neighbor", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class DespeckleOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = get_input("Image");
    /* Single value inputs can't be despeckled and are returned as is. */
    if (input_image.is_single_value()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    GPUShader *shader = shader_manager().get("compositor_despeckle");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "threshold", get_threshold());
    GPU_shader_uniform_1f(shader, "neighbor_threshold", get_neighbor_threshold());

    input_image.bind_as_texture(shader, "input_tx");

    const Result &factor_image = get_input("Fac");
    factor_image.bind_as_texture(shader, "factor_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    factor_image.unbind_as_texture();
  }

  float get_threshold()
  {
    return bnode().custom3;
  }

  float get_neighbor_threshold()
  {
    return bnode().custom4;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DespeckleOperation(context, node);
}

}  // namespace blender::nodes::node_composite_despeckle_cc

void register_node_type_cmp_despeckle()
{
  namespace file_ns = blender::nodes::node_composite_despeckle_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DESPECKLE, "Despeckle", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_despeckle_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_despeckle;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_despeckle;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
