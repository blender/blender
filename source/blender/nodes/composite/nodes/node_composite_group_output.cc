/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLT_translation.hh"

#include "UI_resources.hh"

#include "DNA_space_types.h"

#include "BKE_context.hh"

#include "NOD_composite.hh"
#include "NOD_node_extra_info.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

namespace blender::nodes::node_composite_group_output_cc {

using namespace blender::compositor;

class GroupOutputOperation : public NodeOperation {
 public:
  GroupOutputOperation(Context &context, DNode node) : NodeOperation(context, node)
  {
    for (const bNodeSocket *input : node->input_sockets()) {
      if (!is_socket_available(input)) {
        continue;
      }

      /* The structure type of the inputs of Group Output nodes are inferred, so we need to
       * manually specify this here. */
      InputDescriptor &descriptor = this->get_input_descriptor(input->identifier);
      descriptor.expects_single_value = false;
    }
  }

  void execute() override
  {
    /* Get the first input to be written to the output. The rest of the inputs are ignored. Only
     * color sockets are supported. */
    const bNodeSocket *input_socket = this->node().input_sockets()[0];
    if (input_socket->type != SOCK_RGBA) {
      return;
    }

    const Result &image = this->get_input(input_socket->identifier);
    this->context().write_output(image);
  }

  Domain compute_domain() override
  {
    if (this->context().use_compositing_domain_for_input_output()) {
      return this->context().get_compositing_domain();
    }
    return NodeOperation::compute_domain();
  }
};

}  // namespace blender::nodes::node_composite_group_output_cc

namespace blender::nodes {

compositor::NodeOperation *get_group_output_compositor_operation(compositor::Context &context,
                                                                 DNode node)
{
  return new node_composite_group_output_cc::GroupOutputOperation(context, node);
}

void get_compositor_group_output_extra_info(blender::nodes::NodeExtraInfoParams &parameters)
{
  if (parameters.tree.type != NTREE_COMPOSIT) {
    return;
  }

  SpaceNode *space_node = CTX_wm_space_node(&parameters.C);
  if (space_node->edittree != space_node->nodetree) {
    return;
  }

  Span<const bNodeSocket *> group_outputs = parameters.node.input_sockets().drop_back(1);
  if (group_outputs.is_empty()) {
    blender::nodes::NodeExtraInfoRow row;
    row.text = IFACE_("No Output");
    row.icon = ICON_ERROR;
    row.tooltip = TIP_("Node group must have a Color output socket");
    parameters.rows.append(std::move(row));
    return;
  }

  if (group_outputs[0]->type != SOCK_RGBA) {
    blender::nodes::NodeExtraInfoRow row;
    row.text = IFACE_("Wrong Output Type");
    row.icon = ICON_ERROR;
    row.tooltip = TIP_("Node group's first output must be a color output");
    parameters.rows.append(std::move(row));
    return;
  }

  if (group_outputs.size() > 1) {
    blender::nodes::NodeExtraInfoRow row;
    row.text = IFACE_("Ignored Outputs");
    row.icon = ICON_WARNING_LARGE;
    row.tooltip = TIP_("Only the first output is considered while the rest are ignored");
    parameters.rows.append(std::move(row));
    return;
  }
}

}  // namespace blender::nodes
