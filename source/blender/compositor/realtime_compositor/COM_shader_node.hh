/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "GPU_material.h"

#include "NOD_derived_node_tree.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

/* ------------------------------------------------------------------------------------------------
 * Shader Node
 *
 * A shader node encapsulates a compositor node tree that is capable of being used together with
 * other shader nodes to construct a Shader Operation using the GPU material compiler. A GPU node
 * stack for each of the node inputs and outputs is stored and populated during construction in
 * order to represent the node as a GPU node inside the GPU material graph, see GPU_material.h for
 * more information. Derived classes should implement the compile method to add the node and link
 * it to the GPU material given to the method. The compiler is expected to initialize the input
 * links of the node before invoking the compile method. See the discussion in
 * COM_shader_operation.hh for more information. */
class ShaderNode {
 private:
  /* The node that this operation represents. */
  DNode node_;
  /* The GPU node stacks of the inputs of the node. Those are populated during construction in the
   * populate_inputs method. The links of the inputs are initialized by the GPU material compiler
   * prior to calling the compile method. There is an extra stack at the end to mark the end of the
   * array, as this is what the GPU module functions expect. */
  Vector<GPUNodeStack> inputs_;
  /* The GPU node stacks of the outputs of the node. Those are populated during construction in the
   * populate_outputs method. There is an extra stack at the end to mark the end of the array, as
   * this is what the GPU module functions expect. */
  Vector<GPUNodeStack> outputs_;

 public:
  /* Construct the node by populating both its inputs and outputs. */
  ShaderNode(DNode node);

  virtual ~ShaderNode() = default;

  /* Compile the node by adding the appropriate GPU material graph nodes and linking the
   * appropriate resources. */
  virtual void compile(GPUMaterial *material) = 0;

  /* Returns a contiguous array containing the GPU node stacks of each input. */
  GPUNodeStack *get_inputs_array();

  /* Returns a contiguous array containing the GPU node stacks of each output. */
  GPUNodeStack *get_outputs_array();

  /* Returns the GPU node stack of the input with the given identifier. */
  GPUNodeStack &get_input(StringRef identifier);

  /* Returns the GPU node stack of the output with the given identifier. */
  GPUNodeStack &get_output(StringRef identifier);

  /* Returns the GPU node link of the input with the given identifier, if the input is not linked,
   * a uniform link carrying the value of the input will be created a returned. It is expected that
   * the caller will use the returned link in a GPU material, otherwise, the link may not be
   * properly freed. */
  GPUNodeLink *get_input_link(StringRef identifier);

 protected:
  /* Returns a reference to the derived node that this operation represents. */
  const DNode &node() const;

  /* Returns a reference to the node this operations represents. */
  const bNode &bnode() const;

 private:
  /* Populate the inputs of the node. The input link is set to nullptr and is expected to be
   * initialized by the GPU material compiler before calling the compile method. */
  void populate_inputs();
  /* Populate the outputs of the node. The output link is set to nullptr and is expected to be
   * initialized by the compile method. */
  void populate_outputs();
};

}  // namespace blender::realtime_compositor
