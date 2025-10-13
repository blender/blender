/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "COM_algorithm_parallel_reduction.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** NORMALIZE single channel, useful for Z buffer ******************** */

namespace blender::nodes::node_composite_normalize_cc {

static void cmp_node_normalize_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Float>("Value").default_value(1.0f).min(0.0f).max(1.0f).structure_type(
      StructureType::Dynamic);
  b.add_output<decl::Float>("Value").structure_type(StructureType::Dynamic).align_with_previous();
}

using namespace blender::compositor;

class NormalizeOperation : public NodeOperation {
 private:
  /* The normalize operation is specifically designed to normalize Z Depth information. But since Z
   * Depth can contain near infinite values, normalization is limited to [-range_, range], meaning
   * that values outside of that range will be ignored when computing the maximum and minimum for
   * normalization and will eventually be 0 or 1 if they are less than or larger than the range
   * respectively. */
  constexpr static float range_ = 10000.0f;

 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = this->get_input("Value");
    if (input_image.is_single_value()) {
      Result &output_image = this->get_result("Value");
      output_image.share_data(input_image);
      return;
    }

    const float maximum = maximum_float_in_range(context(), input_image, -range_, range_);
    const float minimum = minimum_float_in_range(context(), input_image, -range_, range_);
    const float scale = (maximum != minimum) ? (1.0f / (maximum - minimum)) : 0.0f;

    if (context().use_gpu()) {
      this->execute_gpu(minimum, scale);
    }
    else {
      this->execute_cpu(minimum, scale);
    }
  }

  void execute_gpu(const float minimum, const float scale)
  {
    gpu::Shader *shader = this->context().get_shader("compositor_normalize");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "minimum", minimum);
    GPU_shader_uniform_1f(shader, "scale", scale);

    Result &input_image = this->get_input("Value");
    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = this->compute_domain();
    Result &output_image = this->get_result("Value");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
  }

  void execute_cpu(const float minimum, const float scale)
  {
    Result &image = this->get_input("Value");

    const Domain domain = this->compute_domain();
    Result &output = this->get_result("Value");
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      const float value = image.load_pixel<float>(texel);
      const float normalized_value = (value - minimum) * scale;
      const float clamped_value = math::clamp(normalized_value, 0.0f, 1.0f);
      output.store_pixel(texel, clamped_value);
    });
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new NormalizeOperation(context, node);
}

}  // namespace blender::nodes::node_composite_normalize_cc

static void register_node_type_cmp_normalize()
{
  namespace file_ns = blender::nodes::node_composite_normalize_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeNormalize", CMP_NODE_NORMALIZE);
  ntype.ui_name = "Normalize";
  ntype.ui_description =
      "Map values to 0 to 1 range, based on the minimum and maximum pixel values";
  ntype.enum_name_legacy = "NORMALIZE";
  ntype.nclass = NODE_CLASS_OP_VECTOR;
  ntype.declare = file_ns::cmp_node_normalize_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_normalize)
