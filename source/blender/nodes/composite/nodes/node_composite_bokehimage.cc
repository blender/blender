/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.h"
#include "BLI_math_vec_types.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "GPU_shader.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Bokeh image Tools  ******************** */

namespace blender::nodes::node_composite_bokehimage_cc {

static void cmp_node_bokehimage_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_bokehimage(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeBokehImage *data = MEM_cnew<NodeBokehImage>(__func__);
  data->angle = 0.0f;
  data->flaps = 5;
  data->rounding = 0.0f;
  data->catadioptric = 0.0f;
  data->lensshift = 0.0f;
  node->storage = data;
}

static void node_composit_buts_bokehimage(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "flaps", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "angle", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(
      layout, ptr, "rounding", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(layout,
          ptr,
          "catadioptric",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(layout, ptr, "shift", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class BokehImageOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    GPUShader *shader = shader_manager().get("compositor_bokeh_image");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "exterior_angle", get_exterior_angle());
    GPU_shader_uniform_1f(shader, "rotation", get_rotation());
    GPU_shader_uniform_1f(shader, "roundness", get_node_bokeh_image().rounding);
    GPU_shader_uniform_1f(shader, "catadioptric", get_node_bokeh_image().catadioptric);
    GPU_shader_uniform_1f(shader, "lens_shift", get_node_bokeh_image().lensshift);

    Result &output = get_result("Image");
    const Domain domain = compute_domain();
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    output.unbind_as_image();
    GPU_shader_unbind();
  }

  Domain compute_domain() override
  {
    return Domain(int2(512));
  }

  NodeBokehImage &get_node_bokeh_image()
  {
    return *static_cast<NodeBokehImage *>(bnode().storage);
  }

  /* The exterior angle is the angle between each two consective vertices of the regular polygon
   * from its center. */
  float get_exterior_angle()
  {
    return (M_PI * 2.0f) / get_node_bokeh_image().flaps;
  }

  float get_rotation()
  {
    /* Offset the rotation such that the second vertex of the regular polygon lies on the positive
     * y axis, which is 90 degrees minus the angle that it makes with the positive x axis assuming
     * the first vertex lies on the positive x axis. */
    const float offset = M_PI_2 - get_exterior_angle();
    return get_node_bokeh_image().angle - offset;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new BokehImageOperation(context, node);
}

}  // namespace blender::nodes::node_composite_bokehimage_cc

void register_node_type_cmp_bokehimage()
{
  namespace file_ns = blender::nodes::node_composite_bokehimage_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BOKEHIMAGE, "Bokeh Image", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_bokehimage_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_bokehimage;
  ntype.flag |= NODE_PREVIEW;
  node_type_init(&ntype, file_ns::node_composit_init_bokehimage);
  node_type_storage(
      &ntype, "NodeBokehImage", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
