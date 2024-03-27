/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** COMPOSITE ******************** */

namespace blender::nodes::node_composite_composite_cc {

static void cmp_node_composite_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image").default_value({0.0f, 0.0f, 0.0f, 1.0f});
  b.add_input<decl::Float>("Alpha").default_value(1.0f).min(0.0f).max(1.0f);
}

static void node_composit_buts_composite(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_alpha", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class CompositeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (!context().is_valid_compositing_region()) {
      return;
    }

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
    GPUShader *shader = context().get_shader("compositor_write_output_opaque",
                                             ResultPrecision::Half);
    GPU_shader_bind(shader);

    /* The compositing space might be limited to a subset of the output texture, so only write into
     * that compositing region. */
    const rcti compositing_region = context().get_compositing_region();
    const int2 lower_bound = int2(compositing_region.xmin, compositing_region.ymin);
    const int2 upper_bound = int2(compositing_region.xmax, compositing_region.ymax);
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);
    GPU_shader_uniform_2iv(shader, "upper_bound", upper_bound);

    const Result &image = get_input("Image");
    image.bind_as_texture(shader, "input_tx");

    GPUTexture *output_texture = context().get_output_texture();
    const int image_unit = GPU_shader_get_sampler_binding(shader, "output_img");
    GPU_texture_image_bind(output_texture, image_unit);

    const int2 compositing_region_size = context().get_compositing_region_size();
    compute_dispatch_threads_at_least(shader, compositing_region_size);

    image.unbind_as_texture();
    GPU_texture_image_unbind(output_texture);
    GPU_shader_unbind();
  }

  /* Executes when the image texture is written with no adjustments and can thus be copied directly
   * to the output texture. */
  void execute_copy()
  {
    GPUShader *shader = context().get_shader("compositor_write_output", ResultPrecision::Half);
    GPU_shader_bind(shader);

    /* The compositing space might be limited to a subset of the output texture, so only write into
     * that compositing region. */
    const rcti compositing_region = context().get_compositing_region();
    const int2 lower_bound = int2(compositing_region.xmin, compositing_region.ymin);
    const int2 upper_bound = int2(compositing_region.xmax, compositing_region.ymax);
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);
    GPU_shader_uniform_2iv(shader, "upper_bound", upper_bound);

    const Result &image = get_input("Image");
    image.bind_as_texture(shader, "input_tx");

    GPUTexture *output_texture = context().get_output_texture();
    const int image_unit = GPU_shader_get_sampler_binding(shader, "output_img");
    GPU_texture_image_bind(output_texture, image_unit);

    const int2 compositing_region_size = context().get_compositing_region_size();
    compute_dispatch_threads_at_least(shader, compositing_region_size);

    image.unbind_as_texture();
    GPU_texture_image_unbind(output_texture);
    GPU_shader_unbind();
  }

  /* Executes when the alpha channel of the image is set as the value of the input alpha. */
  void execute_set_alpha()
  {
    GPUShader *shader = context().get_shader("compositor_write_output_alpha",
                                             ResultPrecision::Half);
    GPU_shader_bind(shader);

    /* The compositing space might be limited to a subset of the output texture, so only write into
     * that compositing region. */
    const rcti compositing_region = context().get_compositing_region();
    const int2 lower_bound = int2(compositing_region.xmin, compositing_region.ymin);
    const int2 upper_bound = int2(compositing_region.xmax, compositing_region.ymax);
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);
    GPU_shader_uniform_2iv(shader, "upper_bound", upper_bound);

    const Result &image = get_input("Image");
    image.bind_as_texture(shader, "input_tx");

    const Result &alpha = get_input("Alpha");
    alpha.bind_as_texture(shader, "alpha_tx");

    GPUTexture *output_texture = context().get_output_texture();
    const int image_unit = GPU_shader_get_sampler_binding(shader, "output_img");
    GPU_texture_image_bind(output_texture, image_unit);

    const int2 compositing_region_size = context().get_compositing_region_size();
    compute_dispatch_threads_at_least(shader, compositing_region_size);

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

  /* The operation domain has the same size as the compositing region without any transformations
   * applied. */
  Domain compute_domain() override
  {
    return Domain(context().get_compositing_region_size());
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
