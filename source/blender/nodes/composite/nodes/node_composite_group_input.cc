/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"

#include "GPU_shader.hh"

#include "BLT_translation.hh"

#include "UI_resources.hh"

#include "DNA_space_types.h"

#include "BKE_context.hh"

#include "NOD_composite.hh"
#include "NOD_node_extra_info.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

namespace blender::nodes::node_composite_group_input_cc {

using namespace blender::compositor;

class GroupInputOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    for (const bNodeSocket *output : this->node()->output_sockets()) {
      if (!is_socket_available(output)) {
        continue;
      }

      Result &result = this->get_result(output->identifier);
      if (!result.should_compute()) {
        continue;
      }

      const Result pass = this->context().get_input(output->name);
      this->execute_pass(pass, result);
    }
  }

  void execute_pass(const Result &pass, Result &result)
  {
    if (!pass.is_allocated()) {
      /* Pass not rendered yet, or not supported by viewport. */
      result.allocate_invalid();
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
    result.set_transformation(pass.domain().transformation);
  }

  void execute_pass_gpu(const Result &pass, Result &result)
  {
    gpu::Shader *shader = this->context().get_shader(this->get_shader_name(pass),
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
      case ResultType::Menu:
        /* Not supported. */
        break;
      case ResultType::String:
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

    const int2 size = this->context().use_context_bounds_for_input_output() ?
                          this->context().get_compositing_region_size() :
                          pass.domain().size;
    result.allocate_texture(size);

    parallel_for(size, [&](const int2 texel) {
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

void get_compositor_group_input_extra_info(blender::nodes::NodeExtraInfoParams &parameters)
{
  if (parameters.tree.type != NTREE_COMPOSIT) {
    return;
  }

  SpaceNode *space_node = CTX_wm_space_node(&parameters.C);
  if (space_node->edittree != space_node->nodetree) {
    return;
  }

  if (space_node->node_tree_sub_type != SNODE_COMPOSITOR_SEQUENCER) {
    return;
  }

  Span<const bNodeSocket *> group_inputs = parameters.node.output_sockets().drop_back(1);
  bool added_warning_for_unsupported_inputs = false;
  for (const bNodeSocket *input : group_inputs) {
    if (StringRef(input->name) == "Image") {
      if (input->type != SOCK_RGBA) {
        blender::nodes::NodeExtraInfoRow row;
        row.text = IFACE_("Wrong Image Input Type");
        row.icon = ICON_ERROR;
        row.tooltip = TIP_("Node group's main Image input should be of type Color");
        parameters.rows.append(std::move(row));
      }
    }
    else if (StringRef(input->name) == "Mask") {
      if (input->type != SOCK_RGBA) {
        blender::nodes::NodeExtraInfoRow row;
        row.text = IFACE_("Wrong Mask Input Type");
        row.icon = ICON_ERROR;
        row.tooltip = TIP_("Node group's Mask input should be of type Color");
        parameters.rows.append(std::move(row));
      }
    }
    else {
      if (added_warning_for_unsupported_inputs) {
        continue;
      }
      blender::nodes::NodeExtraInfoRow row;
      row.text = IFACE_("Unsupported Inputs");
      row.icon = ICON_WARNING_LARGE;
      row.tooltip = TIP_(
          "Only a main Image and Mask inputs are supported, the rest are unsupported and will "
          "return zero");
      parameters.rows.append(std::move(row));
      added_warning_for_unsupported_inputs = true;
    }
  }
}

}  // namespace blender::nodes
