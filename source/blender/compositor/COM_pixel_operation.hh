/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include "NOD_derived_node_tree.hh"

#include "COM_context.hh"
#include "COM_operation.hh"
#include "COM_scheduler.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

/* A type representing a contiguous subset of the node execution schedule that will be compiled
 * into a Pixel Operation. */
using PixelCompileUnit = VectorSet<DNode>;

/* ------------------------------------------------------------------------------------------------
 * Pixel Operation
 *
 * An operation that is evaluated pixel-wise and is compiled from a contiguous subset of the node
 * execution schedule, whose nodes all represent pixel-wise operations. The subset of the node
 * execution schedule is called a Pixel Compile Unit and contains nodes that are called Pixel
 * nodes, see the discussion in COM_compile_state.hh for more information. Since the nodes inside
 * the compile unit are all pixel wise, they can be combined into a single operation that can be
 * evaluated more efficiently. This is an abstract class that should be implemented to compile and
 * evaluate the compile unit as needed.
 *
 * Consider the following node graph with a node execution schedule denoted by the number on each
 * node. The compiler may decide to compile a subset of the execution schedule into a pixel
 * operation if they are all pixel nodes, in this case, the nodes from 3 to 5 were compiled
 * together into a pixel operation. This subset is called the pixel compile unit. See the
 * discussion in COM_evaluator.hh for more information on the compilation process. Links that are
 * internal to the pixel operation are established between the input and outputs of the pixel
 * nodes, for instance, the links between nodes 3 and 4 as well as those between nodes 4 and 5.
 * However, links that cross the boundary of the pixel operation needs special handling.
 *
 *                                        Pixel Operation
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
 * Links from nodes that are not part of the pixel operation to nodes that are part of the pixel
 * operation are considered inputs of the operation itself and are declared as such. For instance,
 * the link from node 1 to node 3 is declared as an input to the operation, and the same applies
 * for the links from node 2 to nodes 3 and 5. Note, however, that only one input is declared for
 * each distinct output socket, so both links from node 2 share the same input of the operation.
 *
 * Links from nodes that are part of the pixel operation to nodes that are not part of the pixel
 * operation are considered outputs of the operation itself and are declared as such. For instance,
 * the link from node 5 to node 6 is declared as an output to the operation. */
class PixelOperation : public Operation {
 protected:
  /* The compile unit that will be compiled into this pixel operation. */
  PixelCompileUnit compile_unit_;
  /* A reference to the node execution schedule that is being compiled. */
  const Schedule &schedule_;
  /* A map that associates the identifier of each input of the operation with the output socket it
   * is linked to. This is needed to help the compiler establish links between operations. */
  Map<std::string, DOutputSocket> inputs_to_linked_outputs_map_;
  /* A map that associates the output socket of a node that is not part of the pixel operation to
   * the identifier of the input of the operation that was declared for it. */
  Map<DOutputSocket, std::string> outputs_to_declared_inputs_map_;
  /* A map that associates each of the needed implicit inputs with the identifiers of the inputs of
   * the operation that were declared for them. */
  Map<ImplicitInput, std::string> implicit_inputs_to_input_identifiers_map_;
  /* A map that associates the identifier of each input of the operation with the number of node
   * inputs that use it, that is, its reference count. This is needed to correct the reference
   * counts of results linked to the inputs of the operation, since the results that provide the
   * inputs aren't aware that multiple of their outgoing links are now part of a single pixel
   * operation. For instance, if an output is linked to both inputs of a Math node, its computed
   * reference count would be 2, but the pixel operation of the Math node would only create a
   * single shared input for it, so from the point of view of the evaluator, the reference count
   * should actually be 1. So the result's reference count should be corrected by decrementing it
   * by the internal reference count computed in this map minus 1. */
  Map<std::string, int> inputs_to_reference_counts_map_;
  /* A map that associates the output socket that provides the result of an output of the operation
   * with the identifier of that output. This is needed to help the compiler establish links
   * between operations. */
  Map<DOutputSocket, std::string> output_sockets_to_output_identifiers_map_;
  /* A vector set that stores all output sockets that are used as previews for nodes inside the
   * pixel operation. */
  VectorSet<DOutputSocket> preview_outputs_;

 public:
  PixelOperation(Context &context, PixelCompileUnit &compile_unit, const Schedule &schedule);

  /* Returns the maximum number of outputs that the PixelOperation can have. Pixel compile units
   * need to be split into smaller units if the numbers of outputs they have is more than the
   * number returned by this method. */
  static int maximum_number_of_outputs(Context &context);

  /* Compute a node preview for all nodes in the pixel operations if the node requires a preview.
   *
   * Previews are computed from results that are populated for outputs that are used to compute
   * previews even if they are internally linked, and those outputs are stored and tracked in the
   * preview_outputs_ vector set, see the populate_results_for_node method for more information. */
  void compute_preview() override;

  /* Get the identifier of the operation output corresponding to the given output socket. This is
   * called by the compiler to identify the operation output that provides the result for an input
   * by providing the output socket that the input is linked to. See
   * output_sockets_to_output_identifiers_map_ for more information. */
  StringRef get_output_identifier_from_output_socket(DOutputSocket output_socket);

  /* Get a reference to the inputs to linked outputs map of the operation. This is called by the
   * compiler to identify the output that each input of the operation is linked to for correct
   * input mapping. See inputs_to_linked_outputs_map_ for more information. */
  Map<std::string, DOutputSocket> &get_inputs_to_linked_outputs_map();

  /* Get a reference to the implicit inputs to input identifiers map of the operation. This is
   * called by the compiler to link the operations inputs with their corresponding implicit input
   * results. See implicit_inputs_to_input_identifiers_map_ for more information. */
  Map<ImplicitInput, std::string> &get_implicit_inputs_to_input_identifiers_map();

  /* Returns the internal reference count of the operation input with the given identifier. See the
   * inputs_to_reference_counts_map_ member for more information. */
  int get_internal_input_reference_count(const StringRef &identifier);

  /* Compute and set the initial reference counts of all the results of the operation. The
   * reference counts of the results are the number of operations that use those results, which is
   * computed as the number of inputs linked to the output corresponding to each of the results of
   * the operation, but only the linked inputs whose node is part of the schedule but not part of
   * the pixel operation, since inputs that are part of the pixel operations are internal links.
   *
   * Additionally, results that are used as node previews gets an extra reference count because
   * they are referenced and released by the compute_preview method.
   *
   * The node execution schedule is given as an input. */
  void compute_results_reference_counts(const Schedule &schedule);
};

}  // namespace blender::compositor
