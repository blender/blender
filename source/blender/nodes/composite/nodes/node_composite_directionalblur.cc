/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_matrix.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_directionalblur_cc {

NODE_STORAGE_FUNCS(NodeDBlurData)

static void cmp_node_directional_blur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_dblur(bNodeTree * /*ntree*/, bNode *node)
{
  NodeDBlurData *ndbd = MEM_callocN<NodeDBlurData>(__func__);
  node->storage = ndbd;
  ndbd->iter = 1;
  ndbd->center_x = 0.5;
  ndbd->center_y = 0.5;
}

static void node_composit_buts_dblur(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  uiItemR(layout, ptr, "iterations", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemL(col, IFACE_("Center:"), ICON_NONE);
  uiItemR(col, ptr, "center_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("X"), ICON_NONE);
  uiItemR(col, ptr, "center_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Y"), ICON_NONE);

  uiItemS(layout);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "distance", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(col, ptr, "angle", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

  uiItemS(layout);

  uiItemR(layout, ptr, "spin", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(layout, ptr, "zoom", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

class DirectionalBlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (this->is_identity()) {
      const Result &input = this->get_input("Image");
      Result &output = this->get_result("Image");
      output.share_data(input);
      return;
    }

    if (this->context().use_gpu()) {
      this->execute_gpu();
    }
    else {
      this->execute_cpu();
    }
  }

  void execute_gpu()
  {
    GPUShader *shader = context().get_shader("compositor_directional_blur");
    GPU_shader_bind(shader);

    /* The number of iterations does not cover the original image, that is, the image with no
     * transformation. So add an extra iteration for the original image and put that into
     * consideration in the shader. */
    GPU_shader_uniform_1i(shader, "iterations", get_iterations() + 1);
    GPU_shader_uniform_2fv(shader, "origin", get_origin());
    GPU_shader_uniform_2fv(shader, "translation", get_translation());
    GPU_shader_uniform_1f(shader, "rotation_sin", math::sin(get_rotation()));
    GPU_shader_uniform_1f(shader, "rotation_cos", math::cos(get_rotation()));
    GPU_shader_uniform_1f(shader, "scale", get_scale());

    const Result &input_image = get_input("Image");
    GPU_texture_filter_mode(input_image, true);
    GPU_texture_extend_mode(input_image, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
  }

  void execute_cpu()
  {
    /* The number of iterations does not cover the original image, that is, the image with no
     * transformation. So add an extra iteration for the original image and put that into
     * consideration in the code. */
    const int iterations = this->get_iterations() + 1;
    const float2 origin = this->get_origin();
    const float2 translation = this->get_translation();
    const float rotation_sin = math::sin(this->get_rotation());
    const float rotation_cos = math::cos(this->get_rotation());
    const float scale = this->get_scale();

    const Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const int2 size = domain.size;
    parallel_for(size, [&](const int2 texel) {
      float2 coordinates = float2(texel) + float2(0.5f);

      float current_sin = 0.0f;
      float current_cos = 1.0f;
      float current_scale = 1.0f;
      float2 current_translation = float2(0.0f);

      /* For each iteration, accumulate the input at the transformed coordinates, then increment
       * the transformations for the next iteration. */
      float4 accumulated_color = float4(0.0f);
      for (int i = 0; i < iterations; i++) {
        /* Transform the coordinates by first offsetting the origin, scaling, translating,
         * rotating, then finally restoring the origin. Notice that we do the inverse of each of
         * the transforms, since we are transforming the coordinates, not the image. */
        float2 transformed_coordinates = coordinates;
        transformed_coordinates -= origin;
        transformed_coordinates /= current_scale;
        transformed_coordinates -= current_translation;
        transformed_coordinates = transformed_coordinates *
                                  float2x2(float2(current_cos, current_sin),
                                           float2(-current_sin, current_cos));
        transformed_coordinates += origin;

        accumulated_color += input.sample_bilinear_zero(transformed_coordinates / float2(size));

        current_scale += scale;
        current_translation += translation;

        /* Those are the sine and cosine addition identities. Used to avoid computing sine and
         * cosine at each iteration. */
        float new_sin = current_sin * rotation_cos + current_cos * rotation_sin;
        current_cos = current_cos * rotation_cos - current_sin * rotation_sin;
        current_sin = new_sin;
      }

      output.store_pixel(texel, accumulated_color / iterations);
    });
  }

  /* Get the amount of translation relative to the image size that will be applied on each
   * iteration. The translation is in the negative x direction rotated in the clock-wise
   * direction, hence the negative sign for the rotation and translation vector. */
  float2 get_translation()
  {
    const float2 input_size = float2(get_input("Image").domain().size);
    const float diagonal_length = math::length(input_size);
    const float translation_amount = diagonal_length * node_storage(bnode()).distance;
    const float2x2 rotation = math::from_rotation<float2x2>(
        math::AngleRadian(-node_storage(bnode()).angle));
    const float2 translation = rotation * float2(-translation_amount / get_iterations(), 0.0f);
    return translation;
  }

  /* Get the amount of rotation that will be applied on each iteration. */
  float get_rotation()
  {
    return node_storage(bnode()).spin / get_iterations();
  }

  /* Get the amount of scale that will be applied on each iteration. The scale is identity when
   * the user supplies 0, so we add 1. */
  float get_scale()
  {
    return node_storage(bnode()).zoom / get_iterations();
  }

  float2 get_origin()
  {
    const float2 input_size = float2(get_input("Image").domain().size);
    return float2(node_storage(bnode()).center_x, node_storage(bnode()).center_y) * input_size;
  }

  /* The actual number of iterations is 2 to the power of the user supplied iterations. The power
   * is implemented using a bit shift. But also make sure it doesn't exceed the upper limit which
   * is the number of diagonal pixels. */
  int get_iterations()
  {
    const int iterations = 2 << (node_storage(bnode()).iter - 1);
    const int upper_limit = math::ceil(math::length(float2(get_input("Image").domain().size)));
    return math::min(iterations, upper_limit);
  }

  /* Returns true if the operation does nothing and the input can be passed through. */
  bool is_identity()
  {
    const Result &input = get_input("Image");
    /* Single value inputs can't be blurred and are returned as is. */
    if (input.is_single_value()) {
      return true;
    }

    /* If any of the following options are non-zero, then the operation is not an identity. */
    if (node_storage(bnode()).distance != 0.0f) {
      return false;
    }

    if (node_storage(bnode()).spin != 0.0f) {
      return false;
    }

    if (node_storage(bnode()).zoom != 0.0f) {
      return false;
    }

    return true;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DirectionalBlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_directionalblur_cc

void register_node_type_cmp_dblur()
{
  namespace file_ns = blender::nodes::node_composite_directionalblur_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDBlur", CMP_NODE_DBLUR);
  ntype.ui_name = "Directional Blur";
  ntype.ui_description = "Blur an image along a direction";
  ntype.enum_name_legacy = "DBLUR";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_directional_blur_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_dblur;
  ntype.initfunc = file_ns::node_composit_init_dblur;
  blender::bke::node_type_storage(
      ntype, "NodeDBlurData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
