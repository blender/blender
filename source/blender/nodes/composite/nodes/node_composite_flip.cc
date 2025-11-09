/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Flip  ******************** */

namespace blender::nodes::node_composite_flip_cc {

static void cmp_node_flip_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Bool>("Flip X").default_value(false);
  b.add_input<decl::Bool>("Flip Y").default_value(false);
}

using namespace blender::compositor;

class FlipOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value() || (!this->get_flip_x() && !this->get_flip_y())) {
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
    gpu::Shader *shader = context().get_shader("compositor_flip");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "flip_x", this->get_flip_x());
    GPU_shader_uniform_1b(shader, "flip_y", this->get_flip_y());

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result &result = get_result("Image");
    result.allocate_texture(domain);
    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    result.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_cpu()
  {
    const bool flip_x = this->get_flip_x();
    const bool flip_y = this->get_flip_y();

    Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const int2 size = domain.size;
    parallel_for(domain.size, [&](const int2 texel) {
      int2 flipped_texel = texel;
      if (flip_x) {
        flipped_texel.x = size.x - texel.x - 1;
      }
      if (flip_y) {
        flipped_texel.y = size.y - texel.y - 1;
      }
      output.store_pixel(texel, input.load_pixel<Color>(flipped_texel));
    });
  }

  bool get_flip_x()
  {
    return this->get_input("Flip X").get_single_value_default(false);
  }

  bool get_flip_y()
  {
    return this->get_input("Flip Y").get_single_value_default(false);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new FlipOperation(context, node);
}

}  // namespace blender::nodes::node_composite_flip_cc

static void register_node_type_cmp_flip()
{
  namespace file_ns = blender::nodes::node_composite_flip_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeFlip", CMP_NODE_FLIP);
  ntype.ui_name = "Flip";
  ntype.ui_description = "Flip an image along a defined axis";
  ntype.enum_name_legacy = "FLIP";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_flip_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_flip)
