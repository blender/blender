/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vec_types.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "GPU_shader.h"
#include "GPU_state.h"
#include "GPU_texture.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** COMPOSITE ******************** */

namespace blender::nodes::node_composite_composite_cc {

static void cmp_node_composite_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({0.0f, 0.0f, 0.0f, 1.0f});
  b.add_input<decl::Float>(N_("Alpha")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("Z")).default_value(1.0f).min(0.0f).max(1.0f);
}

static void node_composit_buts_composite(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_alpha", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class CompositeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &image = get_input("Image");
    const Result &alpha = get_input("Alpha");

    if (image.is_single_value() && alpha.is_single_value()) {
      execute_clear();
    }
    else if (ignore_alpha()) {
      execute_ignore_alpha();
    }
    else if (!node().input_by_identifier("Alpha")->is_logically_linked()) {
      execute_copy();
    }
    else {
      execute_set_alpha();
    }
  }

  /* Executes when all inputs are single values, in which case, the output texture can just be
   * cleared to the appropriate color. */
  void execute_clear()
  {
    const Result &image = get_input("Image");
    const Result &alpha = get_input("Alpha");

    float4 color = image.get_color_value();
    if (ignore_alpha()) {
      color.w = 1.0f;
    }
    else if (node().input_by_identifier("Alpha")->is_logically_linked()) {
      color.w = alpha.get_float_value();
    }

    GPU_texture_clear(context().get_output_texture(), GPU_DATA_FLOAT, color);
  }

  /* Executes when the alpha channel of the image is ignored. */
  void execute_ignore_alpha()
  {
    GPUShader *shader = shader_manager().get("compositor_convert_color_to_opaque");
    GPU_shader_bind(shader);

    const Result &image = get_input("Image");
    image.bind_as_texture(shader, "input_tx");

    GPUTexture *output_texture = context().get_output_texture();
    const int image_unit = GPU_shader_get_texture_binding(shader, "output_img");
    GPU_texture_image_bind(output_texture, image_unit);

    compute_dispatch_threads_at_least(shader, compute_domain().size);

    image.unbind_as_texture();
    GPU_texture_image_unbind(output_texture);
    GPU_shader_unbind();
  }

  /* Executes when the image texture is written with no adjustments and can thus be copied directly
   * to the output texture. */
  void execute_copy()
  {
    const Result &image = get_input("Image");

    /* Make sure any prior writes to the texture are reflected before copying it. */
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

    GPU_texture_copy(context().get_output_texture(), image.texture());
  }

  /* Executes when the alpha channel of the image is set as the value of the input alpha. */
  void execute_set_alpha()
  {
    GPUShader *shader = shader_manager().get("compositor_set_alpha");
    GPU_shader_bind(shader);

    const Result &image = get_input("Image");
    image.bind_as_texture(shader, "image_tx");

    const Result &alpha = get_input("Alpha");
    alpha.bind_as_texture(shader, "alpha_tx");

    GPUTexture *output_texture = context().get_output_texture();
    const int image_unit = GPU_shader_get_texture_binding(shader, "output_img");
    GPU_texture_image_bind(output_texture, image_unit);

    compute_dispatch_threads_at_least(shader, compute_domain().size);

    image.unbind_as_texture();
    alpha.unbind_as_texture();
    GPU_texture_image_unbind(output_texture);
    GPU_shader_unbind();
  }

  /* If true, the alpha channel of the image is set to 1, that is, it becomes opaque. If false, the
   * alpha channel of the image is retained, but only if the alpha input is not linked. If the
   * alpha input is linked, it the value of that input will be used as the alpha of the image. */
  bool ignore_alpha()
  {
    return bnode().custom2 & CMP_NODE_OUTPUT_IGNORE_ALPHA;
  }

  /* The operation domain have the same dimensions of the output without any transformations. */
  Domain compute_domain() override
  {
    return Domain(context().get_output_size());
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CompositeOperation(context, node);
}

}  // namespace blender::nodes::node_composite_composite_cc

void register_node_type_cmp_composite()
{
  namespace file_ns = blender::nodes::node_composite_composite_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COMPOSITE, "Composite", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::cmp_node_composite_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_composite;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.flag |= NODE_PREVIEW;
  ntype.no_muting = true;

  nodeRegisterType(&ntype);
}
