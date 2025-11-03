/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_numbers.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** SPLIT NODE ******************** */

namespace blender::nodes::node_composite_split_cc {

static void cmp_node_split_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Position")
      .dimensions(2)
      .subtype(PROP_FACTOR)
      .default_value({0.5f, 0.5f})
      .min(0.0f)
      .max(1.0f)
      .description("Line position where the image should be split");
  b.add_input<decl::Float>("Rotation")
      .default_value(math::numbers::pi_v<float> / 4.0f)
      .subtype(PROP_ANGLE)
      .description("Line angle where the image should be split");

  b.add_input<decl::Color>("Image").structure_type(StructureType::Dynamic);
  b.add_input<decl::Color>("Image", "Image_001").structure_type(StructureType::Dynamic);

  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
}

using namespace blender::compositor;

class SplitOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (this->context().use_gpu()) {
      this->execute_gpu();
    }
    else {
      this->execute_cpu();
    }
  }

  void execute_gpu()
  {
    gpu::Shader *shader = this->context().get_shader("compositor_split");
    GPU_shader_bind(shader);

    const Domain domain = this->compute_domain();

    GPU_shader_uniform_2fv(shader, "position", this->get_position(domain));

    const float2 normal = {-math::sin(this->get_rotation()), math::cos(this->get_rotation())};
    GPU_shader_uniform_2fv(shader, "normal", normal);

    const Result &first_image = this->get_input("Image");
    first_image.bind_as_texture(shader, "first_image_tx");
    const Result &second_image = this->get_input("Image_001");
    second_image.bind_as_texture(shader, "second_image_tx");

    Result &output_image = this->get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    first_image.unbind_as_texture();
    second_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_cpu()
  {
    const Result &first_image = this->get_input("Image");
    const Result &second_image = this->get_input("Image_001");

    const Domain domain = this->compute_domain();
    Result &output_image = this->get_result("Image");
    output_image.allocate_texture(domain);

    const math::AngleRadian rotation = this->get_rotation();
    const float2 normal = {-math::sin(rotation), math::cos(rotation)};
    const float2 line_point = this->get_position(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      const float2 direction_to_line_point = line_point - float2(texel);
      const float projection = math::dot(normal, direction_to_line_point);
      const bool is_below_line = projection <= 0;
      output_image.store_pixel(texel,
                               is_below_line ? first_image.load_pixel<Color, true>(texel) :
                                               second_image.load_pixel<Color, true>(texel));
    });
  }

  float2 get_position(const Domain &domain)
  {
    const float2 relative_position =
        this->get_input("Position").get_single_value_default(float2(0.5f, 0.5f));
    return float2(domain.size) * relative_position;
  }

  math::AngleRadian get_rotation()
  {
    return this->get_input("Rotation").get_single_value_default(0.0f);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new SplitOperation(context, node);
}

}  // namespace blender::nodes::node_composite_split_cc

static void register_node_type_cmp_split()
{
  namespace file_ns = blender::nodes::node_composite_split_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSplit", CMP_NODE_SPLIT);
  ntype.ui_name = "Split";
  ntype.ui_description =
      "Combine two images for side-by-side display. Typically used in combination with a Viewer "
      "node";
  ntype.enum_name_legacy = "SPLIT";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_split_declare;
  ntype.flag |= NODE_PREVIEW;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_split)
