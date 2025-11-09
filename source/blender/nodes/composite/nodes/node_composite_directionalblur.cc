/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_matrix.hh"

#include "UI_resources.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_directionalblur_cc {

static void cmp_node_directional_blur_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Int>("Samples").default_value(1).min(1).max(32).description(
      "The number of samples used to compute the blur. The more samples the smoother the "
      "result, but at the expense of more compute time. The actual number of samples is two "
      "to the power of this input, so it increases exponentially");
  b.add_input<decl::Vector>("Center")
      .subtype(PROP_FACTOR)
      .dimensions(2)
      .default_value({0.5f, 0.5f})
      .min(0.0f)
      .max(1.0f)
      .description(
          "The position at which the transformations pivot around. Defined in normalized "
          "coordinates, so 0 means lower left corner and 1 means upper right corner of the image");

  b.add_input<decl::Float>("Rotation")
      .default_value(0.0f)
      .subtype(PROP_ANGLE)
      .description("The amount of rotation that the blur spans");
  b.add_input<decl::Float>("Scale").default_value(1.0f).min(0.0f).description(
      "The amount of scaling that the blur spans");

  PanelDeclarationBuilder &translation_panel = b.add_panel("Translation").default_closed(false);
  translation_panel.add_input<decl::Float>("Amount", "Translation Amount")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(-1.0f)
      .max(1.0f)
      .description(
          "The amount of translation that the blur spans in the specified direction relative to "
          "the size of the image. Negative values indicate translation in the opposite direction");
  translation_panel.add_input<decl::Float>("Direction", "Translation Direction")
      .default_value(0.0f)
      .subtype(PROP_ANGLE)
      .description("The angle that defines the direction of the translation");
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
    gpu::Shader *shader = context().get_shader("compositor_directional_blur");
    GPU_shader_bind(shader);

    /* The number of iterations does not cover the original image, that is, the image with no
     * transformation. So add an extra iteration for the original image and put that into
     * consideration in the shader. */
    GPU_shader_uniform_1i(shader, "iterations", this->get_iterations() + 1);
    GPU_shader_uniform_2fv(shader, "origin", this->get_origin());
    GPU_shader_uniform_2fv(shader, "delta_translation", this->get_delta_translation());
    GPU_shader_uniform_1f(shader, "delta_rotation_sin", math::sin(this->get_delta_rotation()));
    GPU_shader_uniform_1f(shader, "delta_rotation_cos", math::cos(this->get_delta_rotation()));
    GPU_shader_uniform_1f(shader, "delta_scale", this->get_delta_scale());

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
    const float2 delta_translation = this->get_delta_translation();
    const float delta_rotation_sin = math::sin(this->get_delta_rotation());
    const float delta_rotation_cos = math::cos(this->get_delta_rotation());
    const float delta_scale = this->get_delta_scale();

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

        current_scale += delta_scale;
        current_translation += delta_translation;

        /* Those are the sine and cosine addition identities. Used to avoid computing sine and
         * cosine at each iteration. */
        float new_sin = current_sin * delta_rotation_cos + current_cos * delta_rotation_sin;
        current_cos = current_cos * delta_rotation_cos - current_sin * delta_rotation_sin;
        current_sin = new_sin;
      }

      output.store_pixel(texel, Color(accumulated_color / iterations));
    });
  }

  /* Get the delta that will be added to the translation on each iteration. The translation is in
   * the negative x direction rotated in the clock-wise direction, hence the negative sign for the
   * rotation and translation vector. */
  float2 get_delta_translation()
  {
    const float2 input_size = float2(get_input("Image").domain().size);
    const float diagonal_length = math::length(input_size);
    const float translation_amount = diagonal_length * this->get_translation_amount();
    const float2x2 rotation = math::from_rotation<float2x2>(
        math::AngleRadian(-this->get_translation_direction()));
    const float2 translation = rotation *
                               float2(-translation_amount / this->get_iterations(), 0.0f);
    return translation;
  }

  /* Get the delta that will be added to the rotation on each iteration. */
  float get_delta_rotation()
  {
    return this->get_rotation() / this->get_iterations();
  }

  /* Get the delta that will be added to the scale on each iteration. */
  float get_delta_scale()
  {
    return (this->get_scale() - 1.0f) / this->get_iterations();
  }

  float2 get_origin()
  {
    const float2 input_size = float2(get_input("Image").domain().size);
    return this->get_center() * input_size;
  }

  /* The actual number of iterations is 2 to the power of the user supplied iterations. The power
   * is implemented using a bit shift. But also make sure it doesn't exceed the upper limit which
   * is the number of diagonal pixels. */
  int get_iterations()
  {
    const int iterations = 2 << (this->get_samples() - 1);
    const int upper_limit = math::ceil(math::length(float2(get_input("Image").domain().size)));
    return math::min(iterations, upper_limit);
  }

  bool is_identity()
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value()) {
      return true;
    }

    if (this->get_translation_amount() != 0.0f) {
      return false;
    }

    if (this->get_rotation() != 0.0f) {
      return false;
    }

    if (this->get_scale() != 1.0f) {
      return false;
    }

    return true;
  }

  int get_samples()
  {
    return math::clamp(this->get_input("Samples").get_single_value_default(1), 1, 32);
  }

  float2 get_center()
  {
    return math::clamp(this->get_input("Center").get_single_value_default(float2(0.5f)),
                       float2(0.0f),
                       float2(1.0f));
  }

  float get_translation_amount()
  {
    return math::clamp(
        this->get_input("Translation Amount").get_single_value_default(0.0f), -1.0f, 1.0f);
  }

  float get_translation_direction()
  {
    return this->get_input("Translation Direction").get_single_value_default(0.0f);
  }

  float get_rotation()
  {
    return this->get_input("Rotation").get_single_value_default(0.0f);
  }

  float get_scale()
  {
    return math::max(10e-6f, this->get_input("Scale").get_single_value_default(1.0f));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DirectionalBlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_directionalblur_cc

static void register_node_type_cmp_dblur()
{
  namespace file_ns = blender::nodes::node_composite_directionalblur_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDBlur", CMP_NODE_DBLUR);
  ntype.ui_name = "Directional Blur";
  ntype.ui_description = "Blur an image along a direction";
  ntype.enum_name_legacy = "DBLUR";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_directional_blur_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_dblur)
