/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Displace  ******************** */

namespace blender::nodes::node_composite_displace_cc {

static void cmp_node_displace_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Vector>("Vector")
      .default_value({1.0f, 1.0f, 1.0f})
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_TRANSLATION)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("X Scale")
      .default_value(0.0f)
      .min(-1000.0f)
      .max(1000.0f)
      .compositor_domain_priority(2);
  b.add_input<decl::Float>("Y Scale")
      .default_value(0.0f)
      .min(-1000.0f)
      .max(1000.0f)
      .compositor_domain_priority(3);
  b.add_output<decl::Color>("Image");
}

using namespace blender::realtime_compositor;

class DisplaceOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    GPUShader *shader = context().get_shader("compositor_displace");
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Image");
    GPU_texture_mipmap_mode(input_image.texture(), true, true);
    GPU_texture_anisotropic_filter(input_image.texture(), true);
    GPU_texture_extend_mode(input_image.texture(), GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    const Result &input_displacement = get_input("Vector");
    input_displacement.bind_as_texture(shader, "displacement_tx");
    const Result &input_x_scale = get_input("X Scale");
    input_x_scale.bind_as_texture(shader, "x_scale_tx");
    const Result &input_y_scale = get_input("Y Scale");
    input_y_scale.bind_as_texture(shader, "y_scale_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    input_displacement.unbind_as_texture();
    input_x_scale.unbind_as_texture();
    input_y_scale.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  bool is_identity()
  {
    const Result &input_image = get_input("Image");
    if (input_image.is_single_value()) {
      return true;
    }

    const Result &input_displacement = get_input("Vector");
    if (input_displacement.is_single_value() &&
        math::is_zero(input_displacement.get_vector_value()))
    {
      return true;
    }

    const Result &input_x_scale = get_input("X Scale");
    const Result &input_y_scale = get_input("Y Scale");
    if (input_x_scale.is_single_value() && input_x_scale.get_float_value() == 0.0f &&
        input_y_scale.is_single_value() && input_y_scale.get_float_value() == 0.0f)
    {
      return true;
    }

    return false;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DisplaceOperation(context, node);
}

}  // namespace blender::nodes::node_composite_displace_cc

void register_node_type_cmp_displace()
{
  namespace file_ns = blender::nodes::node_composite_displace_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DISPLACE, "Displace", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_displace_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::nodeRegisterType(&ntype);
}
