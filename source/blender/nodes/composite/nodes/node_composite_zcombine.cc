/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_algorithm_smaa.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "GPU_shader.h"

#include "node_composite_util.hh"

/* **************** Z COMBINE ******************** */

namespace blender::nodes::node_composite_zcombine_cc {

static void cmp_node_zcombine_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Z")
      .default_value(1.0f)
      .min(0.0f)
      .max(10000.0f)
      .compositor_domain_priority(2);
  b.add_input<decl::Color>("Image", "Image_001")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("Z", "Z_001")
      .default_value(1.0f)
      .min(0.0f)
      .max(10000.0f)
      .compositor_domain_priority(3);
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Z");
}

static void node_composit_buts_zcombine(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_alpha", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_antialias_z", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class ZCombineOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (compute_domain().size == int2(1)) {
      execute_single_value();
    }
    else if (use_anti_aliasing()) {
      execute_anti_aliased();
    }
    else {
      execute_simple();
    }
  }

  void execute_single_value()
  {
    const float4 first_color = get_input("Image").get_color_value();
    const float4 second_color = get_input("Image_001").get_color_value();
    const float first_z_value = get_input("Z").get_float_value();
    const float second_z_value = get_input("Z_001").get_float_value();

    /* Mix between the first and second images using a mask such that the image with the object
     * closer to the camera is returned. The mask value is then 1, and thus returns the first image
     * if its Z value is less than that of the second image. Otherwise, its value is 0, and thus
     * returns the second image. Furthermore, if the object in the first image is closer but has a
     * non-opaque alpha, then the alpha is used as a mask, but only if Use Alpha is enabled. */
    const float z_combine_factor = float(first_z_value < second_z_value);
    const float alpha_factor = use_alpha() ? first_color.w : 1.0f;
    const float mix_factor = z_combine_factor * alpha_factor;

    Result &combined = get_result("Image");
    if (combined.should_compute()) {
      float4 combined_color = math::interpolate(second_color, first_color, mix_factor);
      /* Use the more opaque alpha from the two images. */
      combined_color.w = use_alpha() ? math::max(second_color.w, first_color.w) : combined_color.w;

      combined.allocate_single_value();
      combined.set_color_value(combined_color);
    }

    Result &combined_z = get_result("Z");
    if (combined_z.should_compute()) {
      const float combined_z_value = math::interpolate(second_z_value, first_z_value, mix_factor);
      combined_z.allocate_single_value();
      combined_z.set_float_value(combined_z_value);
    }
  }

  void execute_simple()
  {
    GPUShader *shader = shader_manager().get("compositor_z_combine_simple");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "use_alpha", use_alpha());

    const Result &first = get_input("Image");
    first.bind_as_texture(shader, "first_tx");
    const Result &first_z = get_input("Z");
    first_z.bind_as_texture(shader, "first_z_tx");
    const Result &second = get_input("Image_001");
    second.bind_as_texture(shader, "second_tx");
    const Result &second_z = get_input("Z_001");
    second_z.bind_as_texture(shader, "second_z_tx");

    Result &combined = get_result("Image");
    const Domain domain = compute_domain();
    combined.allocate_texture(domain);
    combined.bind_as_image(shader, "combined_img");

    Result &combined_z = get_result("Z");
    combined_z.allocate_texture(domain);
    combined_z.bind_as_image(shader, "combined_z_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    first.unbind_as_texture();
    first_z.unbind_as_texture();
    second.unbind_as_texture();
    second_z.unbind_as_texture();
    combined.unbind_as_image();
    combined_z.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_anti_aliased()
  {
    Result mask = compute_mask();

    GPUShader *shader = shader_manager().get("compositor_z_combine_from_mask");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "use_alpha", use_alpha());

    const Result &first = get_input("Image");
    first.bind_as_texture(shader, "first_tx");
    const Result &first_z = get_input("Z");
    first_z.bind_as_texture(shader, "first_z_tx");
    const Result &second = get_input("Image_001");
    second.bind_as_texture(shader, "second_tx");
    const Result &second_z = get_input("Z_001");
    second_z.bind_as_texture(shader, "second_z_tx");
    mask.bind_as_texture(shader, "mask_tx");

    Result &combined = get_result("Image");
    const Domain domain = compute_domain();
    combined.allocate_texture(domain);
    combined.bind_as_image(shader, "combined_img");

    Result &combined_z = get_result("Z");
    combined_z.allocate_texture(domain);
    combined_z.bind_as_image(shader, "combined_z_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    first.unbind_as_texture();
    first_z.unbind_as_texture();
    second.unbind_as_texture();
    second_z.unbind_as_texture();
    mask.unbind_as_texture();
    combined.unbind_as_image();
    combined_z.unbind_as_image();
    GPU_shader_unbind();

    mask.release();
  }

  Result compute_mask()
  {
    GPUShader *shader = shader_manager().get("compositor_z_combine_compute_mask");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "use_alpha", use_alpha());

    const Result &first = get_input("Image");
    first.bind_as_texture(shader, "first_tx");
    const Result &first_z = get_input("Z");
    first_z.bind_as_texture(shader, "first_z_tx");
    const Result &second_z = get_input("Z_001");
    second_z.bind_as_texture(shader, "second_z_tx");

    const Domain domain = compute_domain();
    Result mask = Result::Temporary(ResultType::Float, texture_pool());
    mask.allocate_texture(domain);
    mask.bind_as_image(shader, "mask_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    first.unbind_as_texture();
    first_z.unbind_as_texture();
    second_z.unbind_as_texture();
    mask.unbind_as_image();
    GPU_shader_unbind();

    Result anti_aliased_mask = Result::Temporary(ResultType::Float, texture_pool());
    smaa(context(), mask, anti_aliased_mask);
    mask.release();

    return anti_aliased_mask;
  }

  bool use_alpha()
  {
    return bnode().custom1 != 0;
  }

  bool use_anti_aliasing()
  {
    return bnode().custom2 == 0;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ZCombineOperation(context, node);
}

}  // namespace blender::nodes::node_composite_zcombine_cc

void register_node_type_cmp_zcombine()
{
  namespace file_ns = blender::nodes::node_composite_zcombine_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ZCOMBINE, "Z Combine", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_zcombine_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_zcombine;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
