/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "UI_resources.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_sunbeams_cc {

static void cmp_node_sunbeams_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Vector>("Source")
      .subtype(PROP_FACTOR)
      .dimensions(2)
      .default_value({0.5f, 0.5f})
      .min(0.0f)
      .max(1.0f)
      .description(
          "The position of the source of the rays in normalized coordinates. 0 means lower left "
          "corner and 1 means upper right corner");
  b.add_input<decl::Float>("Length")
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .default_value(0.2f)
      .description(
          "The length of rays relative to the size of the image. 0 means no rays and 1 means the "
          "rays cover the full extent of the image");

  b.add_output<decl::Color>("Image");
}

using namespace blender::compositor;

class SunBeamsOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = this->get_input("Image");

    const int2 input_size = input_image.domain().size;
    const int max_steps = int(this->get_length() * math::length(input_size));
    if (max_steps == 0) {
      Result &output_image = this->get_result("Image");
      output_image.share_data(input_image);
      return;
    }

    if (this->context().use_gpu()) {
      this->execute_gpu(max_steps);
    }
    else {
      this->execute_cpu(max_steps);
    }
  }

  void execute_gpu(const int max_steps)
  {
    GPUShader *shader = context().get_shader("compositor_sun_beams");
    GPU_shader_bind(shader);

    GPU_shader_uniform_2fv(shader, "source", this->get_source());
    GPU_shader_uniform_1i(shader, "max_steps", max_steps);

    Result &input_image = get_input("Image");
    GPU_texture_filter_mode(input_image, true);
    GPU_texture_extend_mode(input_image, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    Result &output_image = get_result("Image");
    const Domain domain = compute_domain();
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
  }

  void execute_cpu(const int max_steps)
  {
    const float2 source = this->get_source();

    Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const int2 input_size = domain.size;
    parallel_for(input_size, [&](const int2 texel) {
      /* The number of steps is the distance in pixels from the source to the current texel. With
       * at least a single step and at most the user specified maximum ray length, which is
       * proportional to the diagonal pixel count. */
      float unbounded_steps = math::max(
          1.0f, math::distance(float2(texel), source * float2(input_size)));
      int steps = math::min(max_steps, int(unbounded_steps));

      /* We integrate from the current pixel to the source pixel, so compute the start coordinates
       * and step vector in the direction to source. Notice that the step vector is still computed
       * from the unbounded steps, such that the total integration length becomes limited by the
       * bounded steps, and thus by the maximum ray length. */
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(input_size);
      float2 vector_to_source = source - coordinates;
      float2 step_vector = vector_to_source / unbounded_steps;

      float accumulated_weight = 0.0f;
      float4 accumulated_color = float4(0.0f);
      for (int i = 0; i <= steps; i++) {
        float2 position = coordinates + i * step_vector;

        /* We are already past the image boundaries, and any future steps are also past the image
         * boundaries, so break. */
        if (position.x < 0.0f || position.y < 0.0f || position.x > 1.0f || position.y > 1.0f) {
          break;
        }

        float4 sample_color = input.sample_bilinear_zero(position);

        /* Attenuate the contributions of pixels that are further away from the source using a
         * quadratic falloff. */
        float weight = math::square(1.0f - i / float(steps));

        accumulated_weight += weight;
        accumulated_color += sample_color * weight;
      }

      accumulated_color /= accumulated_weight != 0.0f ? accumulated_weight : 1.0f;
      output.store_pixel(texel, accumulated_color);
    });
  }

  float2 get_source()
  {
    return this->get_input("Source").get_single_value_default(float2(0.5f));
  }

  float get_length()
  {
    return math::clamp(this->get_input("Length").get_single_value_default(0.2f), 0.0f, 1.0f);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new SunBeamsOperation(context, node);
}

}  // namespace blender::nodes::node_composite_sunbeams_cc

static void register_node_type_cmp_sunbeams()
{
  namespace file_ns = blender::nodes::node_composite_sunbeams_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSunBeams", CMP_NODE_SUNBEAMS);
  ntype.ui_name = "Sun Beams";
  ntype.ui_description = "Create sun beams based on image brightness";
  ntype.enum_name_legacy = "SUNBEAMS";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_sunbeams_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_sunbeams)
