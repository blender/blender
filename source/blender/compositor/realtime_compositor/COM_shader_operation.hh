/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include "GPU_material.hh"
#include "GPU_shader.h"

#include "gpu_shader_create_info.hh"

#include "NOD_derived_node_tree.hh"

#include "COM_context.hh"
#include "COM_operation.hh"
#include "COM_scheduler.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

/* A type representing a contiguous subset of the node execution schedule that will be compiled
 * into a Shader Operation. */
using ShaderCompileUnit = VectorSet<DNode>;

/* ------------------------------------------------------------------------------------------------
 * Shader Operation
 *
 * An operation that evaluates a shader compiled from a contiguous subset of the node execution
 * schedule using the GPU material compiler, see GPU_material.hh for more information. The subset
 * of the node execution schedule is called a shader compile unit, see the discussion in
 * COM_compile_state.hh for more information.
 *
 * Consider the following node graph with a node execution schedule denoted by the number on each
 * node. The compiler may decide to compile a subset of the execution schedule into a shader
 * operation, in this case, the nodes from 3 to 5 were compiled together into a shader operation.
 * This subset is called the shader compile unit. See the discussion in COM_evaluator.hh for more
 * information on the compilation process. Each of the nodes inside the compile unit implements a
 * Shader Node which is instantiated, stored in shader_nodes_, and used during compilation. See the
 * discussion in COM_shader_node.hh for more information. Links that are internal to the shader
 * operation are established between the input and outputs of the shader nodes, for instance, the
 * links between nodes 3 and 4 as well as those between nodes 4 and 5. However, links that cross
 * the boundary of the shader operation needs special handling.
 *
 *                                        Shader Operation
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
 * Links from nodes that are not part of the shader operation to nodes that are part of the shader
 * operation are considered inputs of the operation itself and are declared as such. For instance,
 * the link from node 1 to node 3 is declared as an input to the operation, and the same applies
 * for the links from node 2 to nodes 3 and 5. Note, however, that only one input is declared for
 * each distinct output socket, so both links from node 2 share the same input of the operation.
 * An input to the operation is declared for a distinct output socket as follows:
 *
 * - A texture is added to the shader, which will be bound to the result of the output socket
 *   during evaluation.
 * - A GPU attribute is added to the GPU material for that output socket and is linked to the GPU
 *   input stack of the inputs linked to the output socket.
 * - Code is emitted to initialize the values of the attributes by sampling the textures
 *   corresponding to each of the inputs.
 * - The newly added attribute is mapped to the output socket in output_to_material_attribute_map_
 *   to share that same attributes for all inputs linked to the same output socket.
 *
 * Links from nodes that are part of the shader operation to nodes that are not part of the shader
 * operation are considered outputs of the operation itself and are declared as such. For instance,
 * the link from node 5 to node 6 is declared as an output to the operation. An output to the
 * operation is declared for an output socket as follows:
 *
 * - An image is added in the shader where the output value will be written.
 * - A storer GPU material node that stores the value of the output is added and linked to the GPU
 *   output stack of the output. The storer will store the value in the image identified by the
 *   index of the output given to the storer.
 * - The storer functions are generated dynamically to map each index with its appropriate image.
 *
 * The GPU material code generator source is used to construct a compute shader that is then
 * dispatched during operation evaluation after binding the inputs, outputs, and any necessary
 * resources. */
class ShaderOperation : public Operation {
 private:
  /* A reference to the node execution schedule that is being compiled. */
  const Schedule &schedule_;
  /* The compile unit that will be compiled into this shader operation. */
  ShaderCompileUnit compile_unit_;
  /* The GPU material backing the operation. This is created and compiled during construction and
   * freed during destruction. */
  GPUMaterial *material_;
  /* A map that associates each node in the compile unit with an instance of its shader node. */
  Map<DNode, std::unique_ptr<ShaderNode>> shader_nodes_;
  /* A map that associates the identifier of each input of the operation with the output socket it
   * is linked to. This is needed to help the compiler establish links between operations. */
  Map<std::string, DOutputSocket> inputs_to_linked_outputs_map_;
  /* A map that associates the output socket that provides the result of an output of the operation
   * with the identifier of that output. This is needed to help the compiler establish links
   * between operations. */
  Map<DOutputSocket, std::string> output_sockets_to_output_identifiers_map_;
  /* A map that associates the output socket of a node that is not part of the shader operation to
   * the attribute that was created for it. This is used to share the same attribute with all
   * inputs that are linked to the same output socket. */
  Map<DOutputSocket, GPUNodeLink *> output_to_material_attribute_map_;
  /* A vector set that stores all output sockets that are used as previews for nodes inside the
   * shader operation. */
  VectorSet<DOutputSocket> preview_outputs_;

 public:
  /* Construct and compile a GPU material from the given shader compile unit and execution schedule
   * by calling GPU_material_from_callbacks with the appropriate callbacks. */
  ShaderOperation(Context &context, ShaderCompileUnit &compile_unit, const Schedule &schedule);

  /* Free the GPU material. */
  ~ShaderOperation();

  /* Allocate the output results, bind the shader and all its needed resources, then dispatch the
   * shader. */
  void execute() override;

  /* Compute a node preview for all nodes in the shader operations if the node requires a preview.
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

  /* Compute and set the initial reference counts of all the results of the operation. The
   * reference counts of the results are the number of operations that use those results, which is
   * computed as the number of inputs linked to the output corresponding to each of the results of
   * the operation, but only the linked inputs whose node is part of the schedule but not part of
   * the shader operation, since inputs that are part of the shader operations are internal links.
   *
   * Additionally, results that are used as node previews gets an extra reference count because
   * they are referenced and released by the compute_preview method.
   *
   * The node execution schedule is given as an input. */
  void compute_results_reference_counts(const Schedule &schedule);

 private:
  /* Bind the uniform buffer of the GPU material as well as any color band textures needed by the
   * GPU material.  The compiled shader of the material is given as an argument and assumed to be
   * bound. */
  void bind_material_resources(GPUShader *shader);

  /* Bind the input results of the operation to the appropriate textures in the GPU material. The
   * attributes stored in output_to_material_attribute_map_ have names that match the texture
   * samplers in the shader as well as the identifiers of the operation inputs that they correspond
   * to. The compiled shader of the material is given as an argument and assumed to be bound. */
  void bind_inputs(GPUShader *shader);

  /* Bind the output results of the operation to the appropriate images in the GPU material. The
   * name of the images in the shader match the identifier of their corresponding outputs. The
   * compiled shader of the material is given as an argument and assumed to be bound. */
  void bind_outputs(GPUShader *shader);

  /* A static callback method of interface ConstructGPUMaterialFn that is passed to
   * GPU_material_from_callbacks to construct the GPU material graph. The thunk parameter will be a
   * pointer to the instance of ShaderOperation that is being compiled. The method goes over the
   * compile unit and does the following for each node:
   *
   * - Instantiate a ShaderNode from the node and add it to shader_nodes_.
   * - Link the inputs of the node if needed. The inputs are either linked to other nodes in the
   *   GPU material graph or are exposed as inputs to the shader operation itself if they are
   *   linked to nodes that are not part of the shader operation.
   * - Call the compile method of the shader node to actually add and link the GPU material graph
   *   nodes.
   * - If any of the outputs of the node are linked to nodes that are not part of the shader
   *   operation, they are exposed as outputs to the shader operation itself. */
  static void construct_material(void *thunk, GPUMaterial *material);

  /* Link the inputs of the node if needed. Unlinked inputs are ignored as they will be linked by
   * the node compile method. If the input is linked to a node that is not part of the shader
   * operation, the input will be exposed as an input to the shader operation and linked to it.
   * While if the input is linked to a node that is part of the shader operation, then it is linked
   * to that node in the GPU material node graph. */
  void link_node_inputs(DNode node, GPUMaterial *material);

  /* Given the input socket of a node that is part of the shader operation which is linked to the
   * given output socket of a node that is also part of the shader operation, just link the output
   * link of the GPU node stack of the output socket to the input link of the GPU node stack of the
   * input socket. This essentially establishes the needed links in the GPU material node graph. */
  void link_node_input_internal(DInputSocket input_socket, DOutputSocket output_socket);

  /* Given the input socket of a node that is part of the shader operation which is linked to the
   * given output socket of a node that is not part of the shader operation, declare a new
   * operation input and link it to the input link of the GPU node stack of the input socket. An
   * operation input is only declared if no input was already declared for that same output socket
   * before. */
  void link_node_input_external(DInputSocket input_socket,
                                DOutputSocket output_socket,
                                GPUMaterial *material);

  /* Given the input socket of a node that is part of the shader operation which is linked to the
   * given output socket of a node that is not part of the shader operation, declare a new input to
   * the operation that is represented in the GPU material by a newly created GPU attribute. It is
   * assumed that no operation input was declared for this same output socket before. In the
   * generate_code_for_inputs method, a texture will be added in the shader for each of the
   * declared inputs, having the same name as the attribute. Additionally, code will be emitted to
   * initialize the attributes by sampling their corresponding textures. */
  void declare_operation_input(DInputSocket input_socket,
                               DOutputSocket output_socket,
                               GPUMaterial *material);

  /* Populate the output results of the shader operation for output sockets of the given node that
   * are linked to nodes outside of the shader operation or are used to compute a preview for the
   * node. */
  void populate_results_for_node(DNode node, GPUMaterial *material);

  /* Given the output socket of a node that is part of the shader operation which is linked to an
   * input socket of a node that is not part of the shader operation, declare a new output to the
   * operation and link it to an output storer passing in the index of the output. In the
   * generate_code_for_outputs method, an image will be added in the shader for each of the
   * declared outputs. Additionally, code will be emitted to define the storer functions that store
   * the value in the appropriate image identified by the given index. */
  void populate_operation_result(DOutputSocket output_socket, GPUMaterial *material);

  /* A static callback method of interface GPUCodegenCallbackFn that is passed to
   * GPU_material_from_callbacks to create the shader create info of the GPU material. The thunk
   * parameter will be a pointer to the instance of ShaderOperation that is being compiled.
   *
   * This method first generates the necessary code to load the inputs and store the outputs. Then,
   * it creates a compute shader from the generated sources. Finally, it adds the necessary GPU
   * resources to the shader. */
  static void generate_code(void *thunk, GPUMaterial *material, GPUCodegenOutput *code_generator);

  /* Add an image in the shader for each of the declared outputs. Additionally, emit code to define
   * the storer functions that store the given value in the appropriate image identified by the
   * given index. */
  void generate_code_for_outputs(gpu::shader::ShaderCreateInfo &shader_create_info);

  /* Add a texture will in the shader for each of the declared inputs/attributes in the operation,
   * having the same name as the attribute. Additionally, emit code to initialize the attributes by
   * sampling their corresponding textures. */
  void generate_code_for_inputs(GPUMaterial *material,
                                gpu::shader::ShaderCreateInfo &shader_create_info);
};

}  // namespace blender::realtime_compositor
