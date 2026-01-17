/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"

#include "COM_context.hh"
#include "COM_operation.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Node Operation
 *
 * A node operation is a subclass of operation that nodes should implement and instantiate in the
 * get_compositor_operation function of bNodeType, passing the inputs given to that function to the
 * constructor. This class essentially just implements a default constructor that populates output
 * results for all outputs of the node as well as input descriptors for all inputs of the nodes
 * based on their socket declaration. The class also provides some utility methods for easier
 * implementation of nodes. */
class NodeOperation : public Operation {
 private:
  /* The node that this operation represents. */
  const bNode &node_;
  /* A node instance key that identifies the node instance in the nested node groups path. */
  bNodeInstanceKey instance_key_ = bke::NODE_INSTANCE_KEY_NONE;
  /* A map that associates each node instance identified by its node instance key to its node
   * preview. This could be nullptr if node previews are not needed. */
  Map<bNodeInstanceKey, bke::bNodePreview> *node_previews_ = nullptr;

 public:
  /* Populate the output results based on the node outputs and populate the input descriptors based
   * on the node inputs. */
  NodeOperation(Context &context, const bNode &node);

  /* Calls the evaluate method of the operation, but also measures the execution time and stores it
   * in the context's profile data. */
  void evaluate() override;

  /* Compute and set the initial reference counts of all the results of the operation. The
   * reference counts of the results are the number of operations that use those results, which is
   * computed as the number of inputs whose node is part of the schedule and is linked to the
   * output corresponding to each result. The node execution schedule is given as an input. */
  void compute_results_reference_counts(const VectorSet<const bNode *> &schedule);

  /* Setter and getter for instance_key_. */
  void set_instance_key(const bNodeInstanceKey &instance_key);
  const bNodeInstanceKey &get_instance_key() const;

  /* Setter and getter for node_previews_. */
  void set_node_previews(Map<bNodeInstanceKey, bke::bNodePreview> *node_previews);
  Map<bNodeInstanceKey, bke::bNodePreview> *get_node_previews();

 protected:
  /* Compute a node preview using the result returned from the get_preview_result method. */
  void compute_preview() override;

  /* Returns a reference to the node that this operation represents. */
  const bNode &node() const;

 private:
  /* Get the result which will be previewed in the node, this is chosen as the first linked output
   * of the node, if no outputs exist, then the first allocated input will be chosen. Returns
   * nullptr if no result is viewable. */
  Result *get_preview_result();
};

}  // namespace blender::compositor
