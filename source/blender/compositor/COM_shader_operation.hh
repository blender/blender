/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_map.hh"

#include "GPU_material.hh"
#include "GPU_shader.hh"

#include "gpu_shader_create_info.hh"

#include "NOD_derived_node_tree.hh"

#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_pixel_operation.hh"
#include "COM_scheduler.hh"
#include "COM_shader_node.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

/* ------------------------------------------------------------------------------------------------
 * Shader Operation
 *
 * A pixel operation that evaluates a shader compiled from the pixel compile unit using the GPU
 * material compiler, see GPU_material.hh for more information. Also see the PixelOperation class
 * for more information on pixel operations.
 *
 * An input to the pixel operation is declared for a distinct output socket as follows:
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
 * An output to the pixel operation is declared for an output socket as follows:
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
class ShaderOperation : public PixelOperation {
 private:
  /* The GPU material backing the operation. This is created and compiled during construction and
   * freed during destruction. */
  GPUMaterial *material_;
  /* A map that associates each node in the compile unit with an instance of its shader node. */
  Map<DNode, std::unique_ptr<ShaderNode>> shader_nodes_;
  /* A map that associates the output socket of a node that is not part of the shader operation to
   * the attribute that was created for it. This is used to share the same attribute with all
   * inputs that are linked to the same output socket. */
  Map<DOutputSocket, GPUNodeLink *> output_to_material_attribute_map_;
  /* A map that associates implicit inputs to the attributes that were created for them. */
  Map<ImplicitInput, GPUNodeLink *> implicit_input_to_material_attribute_map_;

 public:
  /* Construct and compile a GPU material from the given shader compile unit and execution schedule
   * by calling GPU_material_from_callbacks with the appropriate callbacks. */
  ShaderOperation(Context &context, PixelCompileUnit &compile_unit, const Schedule &schedule);

  /* Free the GPU material. */
  ~ShaderOperation() override;

  /* Allocate the output results, bind the shader and all its needed resources, then dispatch the
   * shader. */
  void execute() override;

 private:
  /* Bind the uniform buffer of the GPU material as well as any color band textures needed by the
   * GPU material.  The compiled shader of the material is given as an argument and assumed to be
   * bound. */
  void bind_material_resources(gpu::Shader *shader);

  /* Bind the input results of the operation to the appropriate textures in the GPU material. The
   * attributes stored in output_to_material_attribute_map_ have names that match the texture
   * samplers in the shader as well as the identifiers of the operation inputs that they correspond
   * to. The compiled shader of the material is given as an argument and assumed to be bound. */
  void bind_inputs(gpu::Shader *shader);

  /* Bind the output results of the operation to the appropriate images in the GPU material. The
   * name of the images in the shader match the identifier of their corresponding outputs. The
   * compiled shader of the material is given as an argument and assumed to be bound. */
  void bind_outputs(gpu::Shader *shader);

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

  /* Link the inputs of the node if needed. Unlinked inputs will be linked to constant values. If
   * the input is linked to a node that is not part of the shader operation, the input will be
   * exposed as an input to the shader operation and linked to it. While if the input is linked to
   * a node that is part of the shader operation, then it is linked to that node in the GPU
   * material node graph. */
  void link_node_inputs(DNode node);

  /* Link the GPU stack of the given unavailable input to a constant zero value setter GPU node.
   * The value is ignored since the socket is unavailable, but the GPU Material compiler expects
   * all inputs to be linked, even unavailable ones. */
  void link_node_input_unavailable(const DInputSocket input);

  /* Link the GPU stack of the given unlinked input to a constant value setter GPU node that
   * supplies the value of the unlinked input. The value is taken from the given origin input,
   * which will be equal to the input in most cases, but can also be an unlinked input of a group
   * node. */
  void link_node_input_constant(const DInputSocket input, const DInputSocket origin);

  /* Given an unlinked input with an implicit input. Declare a new input to the operation for that
   * implicit input if not done already and link it to the input link of the GPU node stack of the
   * input socket. The implicit input and type are taken from the given origin input, which will
   * be equal to the input in most cases, but can also be an unlinked input of a group node */
  void link_node_input_implicit(const DInputSocket input, const DInputSocket origin);

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
  void link_node_input_external(DInputSocket input_socket, DOutputSocket output_socket);

  /* Given the input socket of a node that is part of the shader operation which is linked to the
   * given output socket of a node that is not part of the shader operation, declare a new input to
   * the operation that is represented in the GPU material by a newly created GPU attribute. It is
   * assumed that no operation input was declared for this same output socket before. In the
   * generate_code_for_inputs method, a texture will be added in the shader for each of the
   * declared inputs, having the same name as the attribute. Additionally, code will be emitted to
   * initialize the attributes by sampling their corresponding textures. */
  void declare_operation_input(DInputSocket input_socket, DOutputSocket output_socket);

  /* Populate the output results of the shader operation for output sockets of the given node that
   * are linked to nodes outside of the shader operation or are used to compute a preview for the
   * node. */
  void populate_results_for_node(DNode node);

  /* Given the output socket of a node that is part of the shader operation which is linked to an
   * input socket of a node that is not part of the shader operation, declare a new output to the
   * operation and link it to an output storer passing in the index of the output. In the
   * generate_code_for_outputs method, an image will be added in the shader for each of the
   * declared outputs. Additionally, code will be emitted to define the storer functions that store
   * the value in the appropriate image identified by the given index. */
  void populate_operation_result(DOutputSocket output_socket);

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
  std::string generate_code_for_outputs(gpu::shader::ShaderCreateInfo &shader_create_info);

  /* Add a texture will in the shader for each of the declared inputs/attributes in the operation,
   * having the same name as the attribute. Additionally, emit code to initialize the attributes by
   * sampling their corresponding textures. */
  std::string generate_code_for_inputs(GPUMaterial *material,
                                       gpu::shader::ShaderCreateInfo &shader_create_info);
};

}  // namespace blender::compositor
