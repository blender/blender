/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include <cmath>

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "GPU_shader.h"

#include "COM_algorithm_smaa.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** ID Mask  ******************** */

namespace blender::nodes::node_composite_id_mask_cc {

static void cmp_node_idmask_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("ID value"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(0);
  b.add_output<decl::Float>(N_("Alpha"));
}

static void node_composit_buts_id_mask(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "index", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_antialiasing", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class IDMaskOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_mask = get_input("ID value");
    if (input_mask.is_single_value()) {
      execute_single_value();
      return;
    }

    GPUShader *shader = shader_manager().get("compositor_id_mask");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "index", get_index());

    input_mask.bind_as_texture(shader, "input_mask_tx");

    /* If anti-aliasing is disabled, write to the output directly, otherwise, write to a temporary
     * result to later perform anti-aliasing. */
    Result non_anti_aliased_mask = Result::Temporary(ResultType::Float, texture_pool());
    Result &output_mask = use_anti_aliasing() ? non_anti_aliased_mask : get_result("Alpha");

    const Domain domain = compute_domain();
    output_mask.allocate_texture(domain);
    output_mask.bind_as_image(shader, "output_mask_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_mask.unbind_as_texture();
    output_mask.unbind_as_image();
    GPU_shader_unbind();

    if (use_anti_aliasing()) {
      smaa(context(), non_anti_aliased_mask, get_result("Alpha"));
      non_anti_aliased_mask.release();
    }
  }

  void execute_single_value()
  {
    const float input_mask_value = get_input("ID value").get_float_value();
    const float mask = int(round(input_mask_value)) == get_index() ? 1.0f : 0.0f;
    get_result("Alpha").allocate_single_value();
    get_result("Alpha").set_float_value(mask);
  }

  int get_index()
  {
    return bnode().custom1;
  }

  bool use_anti_aliasing()
  {
    return bnode().custom2 != 0;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new IDMaskOperation(context, node);
}

}  // namespace blender::nodes::node_composite_id_mask_cc

void register_node_type_cmp_idmask()
{
  namespace file_ns = blender::nodes::node_composite_id_mask_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ID_MASK, "ID Mask", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_idmask_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_id_mask;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
