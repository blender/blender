/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"

#include "GPU_shader.hh"

#include "NOD_composite.hh" /* Own include. */

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

namespace blender::nodes::node_composite_group_input_cc {

using namespace blender::compositor;

class GroupInputOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Scene &scene = this->context().get_scene();
    for (const bNodeSocket *output : this->node()->output_sockets()) {
      if (!is_socket_available(output)) {
        continue;
      }

      Result &result = this->get_result(output->identifier);
      if (!result.should_compute()) {
        continue;
      }

      this->context().populate_meta_data_for_pass(&scene, 0, output->name, result.meta_data);

      const Result pass = this->context().get_input(&scene, 0, output->name);
      this->execute_pass(pass, result);
    }
  }

  void execute_pass(const Result &pass, Result &result)
  {
    if (!pass.is_allocated()) {
      /* Pass not rendered yet, or not supported by viewport. */
      result.allocate_invalid();
      this->context().set_info_message("Viewport compositor setup not fully supported");
      return;
    }

    if (!this->context().is_valid_compositing_region()) {
      result.allocate_invalid();
      return;
    }

    result.set_type(pass.type());
    result.set_precision(pass.precision());

    if (this->context().use_gpu()) {
      this->execute_pass_gpu(pass, result);
    }
    else {
      this->execute_pass_cpu(pass, result);
    }
  }

  void execute_pass_gpu(const Result &pass, Result &result)
  {
    GPUShader *shader = this->context().get_shader(this->get_shader_name(pass),
                                                   result.precision());
    GPU_shader_bind(shader);

    /* The compositing space might be limited to a subset of the pass texture, so only read that
     * compositing region into an appropriately sized result. */
    const int2 lower_bound = this->context().get_compositing_region().min;
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);

    pass.bind_as_texture(shader, "input_tx");

    result.allocate_texture(Domain(this->context().get_compositing_region_size()));
    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, result.domain().size);

    GPU_shader_unbind();
    pass.unbind_as_texture();
    result.unbind_as_image();
  }

  const char *get_shader_name(const Result &pass)
  {
    switch (pass.type()) {
      case ResultType::Float:
        return "compositor_read_input_float";
      case ResultType::Float3:
      case ResultType::Color:
      case ResultType::Float4:
        return "compositor_read_input_float4";
      case ResultType::Int:
      case ResultType::Int2:
      case ResultType::Float2:
      case ResultType::Bool:
        /* Not supported. */
        break;
      case ResultType::Menu:
        /* Single only types do not support GPU code path. */
        BLI_assert(Result::is_single_value_only_type(pass.type()));
        BLI_assert_unreachable();
        break;
    }

    BLI_assert_unreachable();
    return nullptr;
  }

  void execute_pass_cpu(const Result &pass, Result &result)
  {
    /* The compositing space might be limited to a subset of the pass texture, so only read that
     * compositing region into an appropriately sized result. */
    const int2 lower_bound = this->context().get_compositing_region().min;

    result.allocate_texture(Domain(this->context().get_compositing_region_size()));

    parallel_for(result.domain().size, [&](const int2 texel) {
      result.store_pixel_generic_type(texel, pass.load_pixel_generic_type(texel + lower_bound));
    });
  }
};

}  // namespace blender::nodes::node_composite_group_input_cc

namespace blender::nodes {

compositor::NodeOperation *get_group_input_compositor_operation(compositor::Context &context,
                                                                DNode node)
{
  return new node_composite_group_input_cc::GroupInputOperation(context, node);
}

}  // namespace blender::nodes
