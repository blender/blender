/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_context.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* Checks if the node group has a File Output node in it or in one of its descendants. */
static bool has_file_output_recursive(const bNodeTree &node_group)
{
  node_group.ensure_topology_cache();
  for (const bNode *node : node_group.nodes_by_type("CompositorNodeOutputFile")) {
    if (!node->is_muted()) {
      return true;
    }
  }

  for (const bNode *group_node : node_group.group_nodes()) {
    if (!group_node->is_muted() && group_node->id) {
      if (has_file_output_recursive(*reinterpret_cast<const bNodeTree *>(group_node->id))) {
        return true;
      }
    }
  }

  return false;
}

/* Checks if the node group with the given instance key has a Viewer node in it or in one of its
 * descendants. Only nodes of node groups whose instance key match that of the given active node
 * group instance key are considered active. */
static bool has_viewer_recursive(const bNodeTree &node_group,
                                 const bNodeInstanceKey instance_key,
                                 const bNodeInstanceKey active_node_group_instance_key)
{
  node_group.ensure_topology_cache();

  /* If this is the active node group, check if a viewer node exists.  */
  if (active_node_group_instance_key == instance_key) {
    for (const bNode *node : node_group.nodes_by_type("CompositorNodeViewer")) {
      if (node->flag & NODE_DO_OUTPUT && !node->is_muted()) {
        return true;
      }
    }
  }

  /* Otherwise, we have to check node groups recursively. */
  for (const bNode *group_node : node_group.group_nodes()) {
    if (group_node->is_muted() || !group_node->id) {
      continue;
    }

    const bNodeTree &child_node_group = *reinterpret_cast<const bNodeTree *>(group_node->id);
    const bNodeInstanceKey child_instance_key = bke::node_instance_key(
        instance_key, &node_group, group_node);
    if (has_viewer_recursive(child_node_group, child_instance_key, active_node_group_instance_key))
    {
      return true;
    }
  }

  return false;
}

/* Add the output nodes whose result should be computed to the given stack. This includes File
 * Output, Group Output, and Viewer nodes. This might also include group nodes that contain File
 * Output or Viewer nodes. */
static void add_output_nodes(const Context &context,
                             const bNodeTree &node_group,
                             NodeGroupOutputTypes needed_outputs_types,
                             const Set<StringRef> &needed_outputs,
                             const bNodeInstanceKey instance_key,
                             const bNodeInstanceKey active_node_group_instance_key,
                             Stack<const bNode *> &node_stack)
{
  node_group.ensure_topology_cache();

  bool viewer_exists = false;
  /* Add group nodes that contain File Output and Viewer nodes. */
  for (const bNode *group_node : node_group.group_nodes()) {
    if (group_node->is_muted() || !group_node->id) {
      continue;
    }

    const bNodeTree &child_tree = *reinterpret_cast<const bNodeTree *>(group_node->id);
    const bNodeInstanceKey child_instance_key = bke::node_instance_key(
        instance_key, &node_group, group_node);
    if (flag_is_set(needed_outputs_types, NodeGroupOutputTypes::ViewerNode) &&
        has_viewer_recursive(child_tree, child_instance_key, active_node_group_instance_key))
    {
      node_stack.push(group_node);
      viewer_exists = true;
      continue;
    }

    if (flag_is_set(needed_outputs_types, NodeGroupOutputTypes::FileOutputNode) &&
        has_file_output_recursive(child_tree))
    {
      node_stack.push(group_node);
    }
  }

  /* Add File Output nodes. */
  if (flag_is_set(needed_outputs_types, NodeGroupOutputTypes::FileOutputNode)) {
    for (const bNode *node : node_group.nodes_by_type("CompositorNodeOutputFile")) {
      if (!node->is_muted()) {
        node_stack.push(node);
      }
    }
  }

  /* Add Viewer node. Only add the node if the node group is active or is a root node group and no
   * viewer node exists in descendants node groups. */
  const bool is_active_node_group = active_node_group_instance_key == instance_key;
  const bool is_root_node_group = instance_key == bke::NODE_INSTANCE_KEY_BASE;
  const bool should_add_viewer = is_active_node_group || (is_root_node_group && !viewer_exists);
  if (flag_is_set(needed_outputs_types, NodeGroupOutputTypes::ViewerNode) && should_add_viewer) {
    for (const bNode *node : node_group.nodes_by_type("CompositorNodeViewer")) {
      if (node->flag & NODE_DO_OUTPUT && !node->is_muted()) {
        node_stack.push(node);
        viewer_exists = true;
        break;
      }
    }
  }

  /* None of the node groups outputs are needed, so no need to add the Group Output node. */
  if (needed_outputs.is_empty()) {
    return;
  }

  /* Add Group Output node. None root node groups should always had a group output node. If the
   * context is treating viewer nodes as group outputs, then the group output should be ignored
   * even if needed. */
  const bool context_ignores_output = context.treat_viewer_as_group_output() && viewer_exists;
  if (!is_root_node_group ||
      (flag_is_set(needed_outputs_types, NodeGroupOutputTypes::GroupOutputNode) &&
       !context_ignores_output))
  {
    const bNode *output_node = node_group.group_output_node();
    if (output_node && !output_node->is_muted()) {
      node_stack.push(output_node);
    }
  }
}

/* A type representing a mapping that associates each node with a heuristic estimation of the
 * number of intermediate buffers needed to compute it and all of its dependencies. See the
 * compute_number_of_needed_buffers function for more information. */
using NeededBuffers = Map<const bNode *, int>;

/* Compute a heuristic estimation of the number of intermediate buffers needed to compute each node
 * and all of its dependencies for all nodes that the given node depends on. The output is a map
 * that maps each node with the number of intermediate buffers needed to compute it and all of its
 * dependencies.
 *
 * Consider a node that takes n number of buffers as an input from a number of node dependencies,
 * which we shall call the input nodes. The node also computes and outputs m number of buffers.
 * In order for the node to compute its output, a number of intermediate buffers will be needed.
 * Since the node takes n buffers and outputs m buffers, then the number of buffers directly
 * needed by the node is (n + m). But each of the input buffers are computed by a node that, in
 * turn, needs a number of buffers to compute its output. So the total number of buffers needed
 * to compute the output of the node is max(n + m, d) where d is the number of buffers needed by
 * the input node that needs the largest number of buffers. We only consider the input node that
 * needs the largest number of buffers, because those buffers can be reused by any input node
 * that needs a lesser number of buffers.
 *
 * Pixel nodes, however, are a special case because links between two pixel nodes inside the same
 * pixel operation don't pass a buffer, but a single value in the pixel processor. So for pixel
 * nodes, only inputs and outputs linked to nodes that are not pixel nodes should be considered.
 * Note that this might not actually be true, because the compiler may decide to split a pixel
 * operation into multiples ones that will pass buffers, but this is not something that can be
 * known at scheduling-time. See the discussion in COM_compile_state.hh, COM_evaluator.hh, and
 * COM_shader_operation.hh for more information. In the node tree shown below, node 4 will have
 * exactly the same number of needed buffers by node 3, because its inputs and outputs are all
 * internally linked in the pixel operation.
 *
 *                                      Pixel Operation
 *                   +------------------------------------------------------+
 * .------------.    |  .------------.  .------------.      .------------.  |  .------------.
 * |   Node 1   |    |  |   Node 3   |  |   Node 4   |      |   Node 5   |  |  |   Node 6   |
 * |            |----|--|            |--|            |------|            |--|--|            |
 * |            |  .-|--|            |  |            |  .---|            |  |  |            |
 * '------------'  | |  '------------'  '------------'  |   '------------'  |  '------------'
 *                 | +----------------------------------|-------------------+
 * .------------.  |                                    |
 * |   Node 2   |  |                                    |
 * |            |--'------------------------------------'
 * |            |
 * '------------'
 *
 * Note that the computed output is not guaranteed to be accurate, and will not be in most cases.
 * The computation is merely a heuristic estimation that works well in most cases. This is due to a
 * number of reasons:
 * - The node tree is actually a graph that allows output sharing, which is not something that was
 *   taken into consideration in this implementation because it is difficult to correctly consider.
 * - Each node may allocate any number of internal buffers, which is not taken into account in this
 *   implementation because it rarely affects the output and is done by very few nodes.
 * - The compiler may decide to compiler the schedule differently depending on runtime information
 *   which we can merely speculate at scheduling-time as described above. */
static NeededBuffers compute_number_of_needed_buffers(Stack<const bNode *> &output_nodes,
                                                      const Set<StringRef> &needed_outputs)
{
  NeededBuffers needed_buffers;

  /* A stack of nodes used to traverse the node group starting from the output nodes. */
  Stack<const bNode *> node_stack = output_nodes;

  /* Traverse the node group in a post order depth first manner and compute the number of needed
   * buffers for each node. Post order traversal guarantee that all the node dependencies of each
   * node are computed before it. This is done by pushing all the uncomputed node dependencies to
   * the node stack first and only popping and computing the node when all its node dependencies
   * were computed. */
  while (!node_stack.is_empty()) {
    /* Do not pop the node immediately, as it may turn out that we can't compute its number of
     * needed buffers just yet because its dependencies weren't computed, it will be popped later
     * when needed. */
    const bNode &node = *node_stack.peek();

    /* Go over the node dependencies connected to the inputs of the node and push them to the node
     * stack if they were not computed already. */
    Set<const bNode *> pushed_nodes;
    for (const bNodeSocket *input : node.input_sockets()) {
      if (!is_socket_available(input)) {
        continue;
      }

      if (node.is_group_output() && !needed_outputs.contains(input->identifier)) {
        continue;
      }

      /* Get the output linked to the input. If it is null, that means the input is unlinked and
       * has no dependency node. */
      const bNodeSocket *output = get_output_linked_to_input(*input);
      if (!output) {
        continue;
      }

      /* The node dependency was already computed or pushed before, so skip it. */
      if (needed_buffers.contains(&output->owner_node()) ||
          pushed_nodes.contains(&output->owner_node()))
      {
        continue;
      }

      /* The output node needs to be computed, push the node dependency to the node stack and
       * indicate that it was pushed. */
      node_stack.push(&output->owner_node());
      pushed_nodes.add_new(&output->owner_node());
    }

    /* If any of the node dependencies were pushed, that means that not all of them were computed
     * and consequently we can't compute the number of needed buffers for this node just yet. */
    if (!pushed_nodes.is_empty()) {
      continue;
    }

    /* We don't need to store the result of the pop because we already peeked at it before. */
    node_stack.pop();

    /* Compute the number of buffers that the node takes as an input as well as the number of
     * buffers needed to compute the most demanding of the node dependencies. */
    int number_of_input_buffers = 0;
    int buffers_needed_by_dependencies = 0;
    for (const bNodeSocket *input : node.input_sockets()) {
      if (!is_socket_available(input)) {
        continue;
      }

      if (node.is_group_output() && !needed_outputs.contains(input->identifier)) {
        continue;
      }

      /* Get the output linked to the input. If it is null, that means the input is unlinked.
       * Unlinked inputs do not take a buffer, so skip those inputs. */
      const bNodeSocket *output = get_output_linked_to_input(*input);
      if (!output) {
        continue;
      }

      /* Since this input is linked, if the link is not between two pixel nodes, it means that the
       * node takes a buffer through this input and so we increment the number of input buffers. */
      if (!is_pixel_node(node) || !is_pixel_node(output->owner_node())) {
        number_of_input_buffers++;
      }

      /* If the number of buffers needed by the node dependency is more than the total number of
       * buffers needed by the dependencies, then update the latter to be the former. This is
       * computing the "d" in the aforementioned equation "max(n + m, d)". */
      const int buffers_needed_by_dependency = needed_buffers.lookup(&output->owner_node());
      buffers_needed_by_dependencies = std::max(buffers_needed_by_dependency,
                                                buffers_needed_by_dependencies);
    }

    /* Compute the number of buffers that will be computed/output by this node. */
    int number_of_output_buffers = 0;
    for (const bNodeSocket *output : node.output_sockets()) {
      if (!is_socket_available(output)) {
        continue;
      }

      /* The output is not linked, it outputs no buffer. */
      if (!output->is_logically_linked()) {
        continue;
      }

      /* If any of the links is not between two pixel nodes, it means that the node outputs
       * a buffer through this output and so we increment the number of output buffers. */
      if (!is_output_linked_to_node_conditioned(*output, is_pixel_node) || !is_pixel_node(node)) {
        number_of_output_buffers++;
      }
    }

    /* Compute the heuristic estimation of the number of needed intermediate buffers to compute
     * this node and all of its dependencies. This is computing the aforementioned equation
     * "max(n + m, d)". */
    const int total_buffers = std::max(number_of_input_buffers + number_of_output_buffers,
                                       buffers_needed_by_dependencies);
    needed_buffers.add(&node, total_buffers);
  }

  return needed_buffers;
}

/* There are multiple different possible orders of evaluating a node graph, each of which needs
 * to allocate a number of intermediate buffers to store its intermediate results. It follows
 * that we need to find the evaluation order which uses the least amount of intermediate buffers.
 * For instance, consider a node that takes two input buffers A and B. Each of those buffers is
 * computed through a number of nodes constituting a sub-graph whose root is the node that
 * outputs that buffer. Suppose the number of intermediate buffers needed to compute A and B are
 * N(A) and N(B) respectively and N(A) > N(B). Then evaluating the sub-graph computing A would be
 * a better option than that of B, because had B was computed first, its outputs will need to be
 * stored in extra buffers in addition to the buffers needed by A. The number of buffers needed by
 * each node is estimated as described in the compute_number_of_needed_buffers function.
 *
 * This is a heuristic generalization of the Sethi-Ullman algorithm, a generalization that
 * doesn't always guarantee an optimal evaluation order, as the optimal evaluation order is very
 * difficult to compute, however, this method works well in most cases. Moreover it assumes that
 * all buffers will have roughly the same size, which may not always be the case. */
VectorSet<const bNode *> compute_schedule(const Context &context,
                                          const bNodeTree &node_group,
                                          NodeGroupOutputTypes needed_outputs_types,
                                          const Set<StringRef> &needed_outputs,
                                          const bNodeInstanceKey instance_key,
                                          const bNodeInstanceKey active_node_group_instance_key)
{
  VectorSet<const bNode *> schedule;

  /* Validate node group. */
  node_group.ensure_topology_cache();
  if (node_group.has_available_link_cycle()) {
    context.set_info_message("Compositor node group has cyclic links.");
    return schedule;
  }

  /* A stack of nodes used to traverse the node group starting from the output nodes. */
  Stack<const bNode *> node_stack;

  /* Add the output nodes whose result should be computed to the stack. */
  add_output_nodes(context,
                   node_group,
                   needed_outputs_types,
                   needed_outputs,
                   instance_key,
                   active_node_group_instance_key,
                   node_stack);

  /* No output nodes, the node group has no effect, return an empty schedule. */
  if (node_stack.is_empty()) {
    return schedule;
  }

  /* Compute the number of buffers needed by each node connected to the outputs. */
  const NeededBuffers needed_buffers = compute_number_of_needed_buffers(node_stack,
                                                                        needed_outputs);

  /* Traverse the node group in a post order depth first manner, scheduling the nodes in an order
   * informed by the number of buffers needed by each node. Post order traversal guarantee that all
   * the node dependencies of each node are scheduled before it. This is done by pushing all the
   * unscheduled node dependencies to the node stack first and only popping and scheduling the node
   * when all its node dependencies were scheduled. */
  while (!node_stack.is_empty()) {
    /* Do not pop the node immediately, as it may turn out that we can't schedule it just yet
     * because its dependencies weren't scheduled, it will be popped later when needed. */
    const bNode &node = *node_stack.peek();

    /* Compute the nodes directly connected to the node inputs sorted by their needed buffers such
     * that the node with the lowest number of needed buffers comes first. Note that we actually
     * want the node with the highest number of needed buffers to be schedule first, but since
     * those are pushed to the traversal stack, we need to push them in reverse order. */
    Vector<const bNode *> sorted_dependency_nodes;
    for (const bNodeSocket *input : node.input_sockets()) {
      if (!is_socket_available(input)) {
        continue;
      }

      if (node.is_group_output() && !needed_outputs.contains(input->identifier)) {
        continue;
      }

      /* Get the output linked to the input. If it is null, that means the input is unlinked and
       * has no dependency node, so skip it. */
      const bNodeSocket *output = get_output_linked_to_input(*input);
      if (!output) {
        continue;
      }

      /* The dependency node was added before, so skip it. The number of dependency nodes is very
       * small, typically less than 3, so a linear search is okay. */
      if (sorted_dependency_nodes.contains(&output->owner_node())) {
        continue;
      }

      /* The dependency node was already schedule, so skip it. */
      if (schedule.contains(&output->owner_node())) {
        continue;
      }

      /* Sort in ascending order on insertion, the number of dependency nodes is very small,
       * typically less than 3, so insertion sort is okay. */
      int insertion_position = 0;
      for (int i = 0; i < sorted_dependency_nodes.size(); i++) {
        if (needed_buffers.lookup(&output->owner_node()) >
            needed_buffers.lookup(sorted_dependency_nodes[i]))
        {
          insertion_position++;
        }
        else {
          break;
        }
      }
      sorted_dependency_nodes.insert(insertion_position, &output->owner_node());
    }

    /* Push the sorted dependency nodes to the node stack in order. */
    for (const bNode *dependency_node : sorted_dependency_nodes) {
      node_stack.push(dependency_node);
    }

    /* If there are no sorted dependency nodes, that means they were all already scheduled or that
     * none exists in the first place, so we can pop and schedule the node now. */
    if (sorted_dependency_nodes.is_empty()) {
      /* The node might have already been scheduled, so we don't use add_new here and simply don't
       * add it if it was already scheduled. */
      schedule.add(node_stack.pop());
    }
  }

  return schedule;
}

}  // namespace blender::compositor
