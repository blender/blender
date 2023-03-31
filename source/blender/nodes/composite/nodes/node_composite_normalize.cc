/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

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
  b.add_input<decl::Float>(N_("Value"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(0);
  b.add_output<decl::Float>(N_("Value"));
}

using namespace blender::realtime_compositor;

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
    Result &input_image = get_input("Value");
    Result &output_image = get_result("Value");
    if (input_image.is_single_value()) {
      input_image.pass_through(output_image);
      return;
    }

    const float maximum = maximum_float_in_range(
        context(), input_image.texture(), -range_, range_);
    const float minimum = minimum_float_in_range(
        context(), input_image.texture(), -range_, range_);
    const float scale = (maximum != minimum) ? (1.0f / (maximum - minimum)) : 0.0f;

    GPUShader *shader = shader_manager().get("compositor_normalize");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "minimum", minimum);
    GPU_shader_uniform_1f(shader, "scale", scale);

    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new NormalizeOperation(context, node);
}

}  // namespace blender::nodes::node_composite_normalize_cc

void register_node_type_cmp_normalize()
{
  namespace file_ns = blender::nodes::node_composite_normalize_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_NORMALIZE, "Normalize", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::cmp_node_normalize_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
