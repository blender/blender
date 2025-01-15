/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_algorithm_smaa.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "GPU_shader.hh"

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
  uiItemR(col, ptr, "use_alpha", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(col, ptr, "use_antialias_z", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

class ZCombineOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (this->get_input("Image").is_single_value() &&
        this->get_input("Image_001").is_single_value() && this->get_input("Z").is_single_value() &&
        this->get_input("Z_001").is_single_value())
    {
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
    const float4 first_color = get_input("Image").get_single_value<float4>();
    const float4 second_color = get_input("Image_001").get_single_value<float4>();
    const float first_z_value = get_input("Z").get_single_value<float>();
    const float second_z_value = get_input("Z_001").get_single_value<float>();

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
      combined.set_single_value(combined_color);
    }

    Result &combined_z = get_result("Z");
    if (combined_z.should_compute()) {
      const float combined_z_value = math::interpolate(second_z_value, first_z_value, mix_factor);
      combined_z.allocate_single_value();
      combined_z.set_single_value(combined_z_value);
    }
  }

  void execute_simple()
  {
    if (this->context().use_gpu()) {
      this->execute_simple_gpu();
    }
    else {
      this->execute_simple_cpu();
    }
  }

  void execute_simple_gpu()
  {
    GPUShader *shader = context().get_shader("compositor_z_combine_simple");
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

  void execute_simple_cpu()
  {
    const bool use_alpha = this->use_alpha();

    const Result &first = this->get_input("Image");
    const Result &first_z = this->get_input("Z");
    const Result &second = this->get_input("Image_001");
    const Result &second_z = this->get_input("Z_001");

    const Domain domain = this->compute_domain();
    Result &combined = this->get_result("Image");
    if (combined.should_compute()) {
      combined.allocate_texture(domain);
      parallel_for(domain.size, [&](const int2 texel) {
        float4 first_color = first.load_pixel<float4, true>(texel);
        float4 second_color = second.load_pixel<float4, true>(texel);
        float first_z_value = first_z.load_pixel<float, true>(texel);
        float second_z_value = second_z.load_pixel<float, true>(texel);

        /* Choose the closer pixel as the foreground, that is, the pixel with the lower z value. If
         * Use Alpha is disabled, return the foreground, otherwise, mix between the foreground and
         * background using the alpha of the foreground. */
        float4 foreground_color = first_z_value < second_z_value ? first_color : second_color;
        float4 background_color = first_z_value < second_z_value ? second_color : first_color;
        float mix_factor = use_alpha ? foreground_color.w : 1.0f;
        float4 combined_color = math::interpolate(background_color, foreground_color, mix_factor);

        /* Use the more opaque alpha from the two images. */
        combined_color.w = use_alpha ? math::max(second_color.w, first_color.w) : combined_color.w;
        combined.store_pixel(texel, combined_color);
      });
    }

    Result &combined_z_output = this->get_result("Z");
    if (combined_z_output.should_compute()) {
      combined_z_output.allocate_texture(domain);
      parallel_for(domain.size, [&](const int2 texel) {
        float first_z_value = first_z.load_pixel<float, true>(texel);
        float second_z_value = second_z.load_pixel<float, true>(texel);
        float combined_z = math::min(first_z_value, second_z_value);
        combined_z_output.store_pixel(texel, combined_z);
      });
    }
  }

  void execute_anti_aliased()
  {
    Result mask = compute_mask();

    Result anti_aliased_mask = this->context().create_result(ResultType::Float);
    smaa(this->context(), mask, anti_aliased_mask);
    mask.release();

    if (this->context().use_gpu()) {
      this->execute_anti_aliased_gpu(anti_aliased_mask);
    }
    else {
      this->execute_anti_aliased_cpu(anti_aliased_mask);
    }

    anti_aliased_mask.release();
  }

  void execute_anti_aliased_gpu(const Result &mask)
  {
    GPUShader *shader = context().get_shader("compositor_z_combine_from_mask");
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
  }

  void execute_anti_aliased_cpu(const Result &mask)
  {
    const bool use_alpha = this->use_alpha();

    const Result &first = this->get_input("Image");
    const Result &first_z = this->get_input("Z");
    const Result &second = this->get_input("Image_001");
    const Result &second_z = this->get_input("Z_001");

    const Domain domain = this->compute_domain();
    Result &combined = this->get_result("Image");
    if (combined.should_compute()) {
      combined.allocate_texture(domain);
      parallel_for(domain.size, [&](const int2 texel) {
        float4 first_color = first.load_pixel<float4, true>(texel);
        float4 second_color = second.load_pixel<float4, true>(texel);
        float mask_value = mask.load_pixel<float>(texel);

        /* Choose the closer pixel as the foreground, that is, the masked pixel with the lower z
         * value. If Use Alpha is disabled, return the foreground, otherwise, mix between the
         * foreground and background using the alpha of the foreground. */
        float4 foreground_color = math::interpolate(second_color, first_color, mask_value);
        float4 background_color = math::interpolate(first_color, second_color, mask_value);
        float mix_factor = use_alpha ? foreground_color.w : 1.0f;
        float4 combined_color = math::interpolate(background_color, foreground_color, mix_factor);

        /* Use the more opaque alpha from the two images. */
        combined_color.w = use_alpha ? math::max(second_color.w, first_color.w) : combined_color.w;
        combined.store_pixel(texel, combined_color);
      });
    }

    Result &combined_z_output = this->get_result("Z");
    if (combined_z_output.should_compute()) {
      combined_z_output.allocate_texture(domain);
      parallel_for(domain.size, [&](const int2 texel) {
        float first_z_value = first_z.load_pixel<float, true>(texel);
        float second_z_value = second_z.load_pixel<float, true>(texel);
        float combined_z = math::min(first_z_value, second_z_value);
        combined_z_output.store_pixel(texel, combined_z);
      });
    }
  }

  Result compute_mask()
  {
    if (this->context().use_gpu()) {
      return this->compute_mask_gpu();
    }

    return this->compute_mask_cpu();
  }

  Result compute_mask_gpu()
  {
    GPUShader *shader = context().get_shader("compositor_z_combine_compute_mask");
    GPU_shader_bind(shader);

    const Result &first_z = get_input("Z");
    first_z.bind_as_texture(shader, "first_z_tx");
    const Result &second_z = get_input("Z_001");
    second_z.bind_as_texture(shader, "second_z_tx");

    const Domain domain = compute_domain();
    Result mask = context().create_result(ResultType::Float);
    mask.allocate_texture(domain);
    mask.bind_as_image(shader, "mask_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    first_z.unbind_as_texture();
    second_z.unbind_as_texture();
    mask.unbind_as_image();
    GPU_shader_unbind();

    return mask;
  }

  Result compute_mask_cpu()
  {
    const Result &first_z = this->get_input("Z");
    const Result &second_z = this->get_input("Z_001");

    const Domain domain = this->compute_domain();
    Result mask = this->context().create_result(ResultType::Float);
    mask.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float first_z_value = first_z.load_pixel<float, true>(texel);
      float second_z_value = second_z.load_pixel<float, true>(texel);
      float z_combine_factor = float(first_z_value < second_z_value);
      mask.store_pixel(texel, z_combine_factor);
    });

    return mask;
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

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeZcombine", CMP_NODE_ZCOMBINE);
  ntype.ui_name = "Z Combine";
  ntype.ui_description = "Combine two images using depth maps";
  ntype.enum_name_legacy = "ZCOMBINE";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_zcombine_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_zcombine;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
