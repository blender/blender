/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_assert.h"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_group_node_operation.hh"
#include "COM_node_group_operation.hh"
#include "COM_node_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* A node operation representing a group node. This is a thin wrapper around a NodeGroupOperation
 * mapping its own inputs to the inputs of the node group operation and sharing its results with
 * the results of the node group operation. */
class GroupNodeOperation : public NodeOperation {
 private:
  /* The node group outputs needed by the caller. */
  const NodeGroupOutputTypes needed_outputs_;
  /* The node instance key of the active group node. */
  const bNodeInstanceKey active_node_group_instance_key_ = bke::NODE_INSTANCE_KEY_BASE;

 public:
  GroupNodeOperation(Context &context,
                     const bNode &node,
                     const NodeGroupOutputTypes needed_outputs,
                     const bNodeInstanceKey active_node_group_instance_key)
      : NodeOperation(context, node),
        needed_outputs_(needed_outputs),
        active_node_group_instance_key_(active_node_group_instance_key)
  {
    for (const bNodeSocket *input : node.input_sockets()) {
      if (!is_socket_available(input)) {
        continue;
      }

      InputDescriptor &descriptor = this->get_input_descriptor(input->identifier);
      /* The structure type of the inputs of Group nodes are inferred, so we need to make sure this
       * is not wrongly expecting single values. */
      descriptor.expects_single_value = false;
      /* Groups nodes should not force realization since it is defined by the user, and there is
       * currently no way for the user to define that through the UI. */
      descriptor.realization_mode = InputRealizationMode::None;
    }
  }

  void execute() override
  {
    const bNodeTree *node_group = this->get_node_group();
    if (!node_group) {
      this->execute_invalid();
      return;
    }

    NodeGroupOperation operation(this->context(),
                                 *node_group,
                                 needed_outputs_,
                                 this->get_node_previews(),
                                 active_node_group_instance_key_,
                                 this->get_instance_key());

    this->set_reference_counts(operation);
    Vector<std::unique_ptr<Result>> temporary_inputs = this->map_inputs(operation);
    operation.evaluate();
    this->write_outputs(operation);
  }

  /* Sets the reference counts of the node group operation according to the needed status of the
   * outputs of the group node. */
  void set_reference_counts(Operation &operation)
  {
    const bNodeTree *node_group = this->get_node_group();
    node_group->ensure_interface_cache();
    for (const bNodeTreeInterfaceSocket *output_socket : node_group->interface_outputs()) {
      Result &node_group_result = operation.get_result(output_socket->identifier);
      Result &group_node_result = this->get_result(output_socket->identifier);
      node_group_result.set_reference_count(group_node_result.should_compute() ? 1 : 0);
    }
  }

  /* Maps the input results of the node group operation to this group node's inputs through
   * temporary results that share the data of the this group's inputs. */
  Vector<std::unique_ptr<Result>> map_inputs(Operation &operation)
  {
    const bNodeTree *node_group = this->get_node_group();
    Vector<std::unique_ptr<Result>> temporary_inputs;
    node_group->ensure_interface_cache();
    for (const bNodeTreeInterfaceSocket *input_socket : node_group->interface_inputs()) {
      const Result &input_result = this->get_input(input_socket->identifier);
      Result temporary_input = this->context().create_result(input_result.type(),
                                                             input_result.precision());
      temporary_input.share_data(input_result);
      temporary_inputs.append(std::make_unique<Result>(temporary_input));
      operation.map_input_to_result(input_socket->identifier, temporary_inputs.last().get());
    }
    return temporary_inputs;
  }

  /* Writes the output results of the node group operation to this group node operation by sharing
   * its data and freeing the results. */
  void write_outputs(Operation &operation)
  {
    const bNodeTree *node_group = this->get_node_group();
    node_group->ensure_interface_cache();
    for (const bNodeTreeInterfaceSocket *output_socket : node_group->interface_outputs()) {
      Result &node_group_result = operation.get_result(output_socket->identifier);
      Result &group_node_result = this->get_result(output_socket->identifier);
      if (group_node_result.should_compute()) {
        group_node_result.share_data(node_group_result);
        node_group_result.release();
      }
    }
  }

  void execute_invalid()
  {
    const bNodeTree *node_group = this->get_node_group();
    node_group->ensure_interface_cache();
    for (const bNodeTreeInterfaceSocket *output : node_group->interface_outputs()) {
      Result &group_node_result = this->get_result(output->identifier);
      if (group_node_result.should_compute()) {
        group_node_result.allocate_invalid();
      }
    }
  }

  const bNodeTree *get_node_group()
  {
    BLI_assert(this->node().is_group());
    return reinterpret_cast<const bNodeTree *>(this->node().id);
  }
};

NodeOperation *get_group_node_operation(Context &context,
                                        const bNode &node,
                                        const NodeGroupOutputTypes &needed_outputs,
                                        const bNodeInstanceKey active_node_group_instance_key)
{
  return new GroupNodeOperation(context, node, needed_outputs, active_node_group_instance_key);
}

}  // namespace blender::compositor
