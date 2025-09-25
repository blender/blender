/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.h"
#include "BLI_string_ref.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"
#include "COM_utilities.hh"
#include "COM_utilities_gpu_material.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

ShaderNode::ShaderNode(DNode node) : node_(node)
{
  this->populate_inputs();
  this->populate_outputs();
}

void ShaderNode::compile(GPUMaterial *material)
{
  node_->typeinfo->gpu_fn(
      material, const_cast<bNode *>(node_.bnode()), nullptr, inputs_.data(), outputs_.data());
}

GPUNodeStack &ShaderNode::get_input(const StringRef identifier)
{
  return get_shader_node_input(*node_, inputs_.data(), identifier);
}

GPUNodeStack &ShaderNode::get_output(const StringRef identifier)
{
  return get_shader_node_output(*node_, outputs_.data(), identifier);
}

static GPUType gpu_type_from_socket(DSocket socket)
{
  switch (eNodeSocketDatatype(socket->type)) {
    case SOCK_FLOAT:
      return GPU_FLOAT;
    case SOCK_INT:
      /* GPUMaterial doesn't support int, so it is passed as a float. */
      return GPU_FLOAT;
    case SOCK_BOOLEAN:
      /* GPUMaterial doesn't support boolean, so it is passed as a float. */
      return GPU_FLOAT;
    case SOCK_VECTOR:
      switch (socket->default_value_typed<bNodeSocketValueVector>()->dimensions) {
        case 2:
          return GPU_VEC2;
        case 3:
          return GPU_VEC3;
        case 4:
          return GPU_VEC4;
        default:
          BLI_assert_unreachable();
          return GPU_NONE;
      }
    case SOCK_RGBA:
      return GPU_VEC4;
    case SOCK_MENU:
      /* GPUMaterial doesn't support int, so it is passed as a float. */
      return GPU_FLOAT;
    case SOCK_STRING:
      /* Single only types do not support GPU code path. */
      BLI_assert(Result::is_single_value_only_type(get_node_socket_result_type(socket.bsocket())));
      BLI_assert_unreachable();
      return GPU_NONE;
    default:
      /* The GPU material compiler will skip unsupported sockets if GPU_NONE is provided. So this
       * is an appropriate and a valid type for unsupported sockets. */
      return GPU_NONE;
  }
}

static void populate_gpu_node_stack(DSocket socket, GPUNodeStack &stack)
{
  /* Make sure this stack is not marked as the end of the stack array. */
  stack.end = false;
  /* This will be initialized later by the GPU material compiler or the compile method. */
  stack.link = nullptr;
  /* This will be initialized by the GPU material compiler if needed. */
  zero_v4(stack.vec);

  stack.sockettype = socket->type;
  stack.type = gpu_type_from_socket(socket);

  stack.hasinput = socket->is_logically_linked();
  stack.hasoutput = socket->is_logically_linked();
}

void ShaderNode::populate_inputs()
{
  /* Reserve a stack for each input in addition to an extra stack at the end to mark the end of the
   * array, as this is what the GPU module functions expect. */
  const int num_input_sockets = node_->input_sockets().size();
  inputs_.resize(num_input_sockets + 1);
  inputs_.last().end = true;

  for (int i = 0; i < num_input_sockets; i++) {
    populate_gpu_node_stack(node_.input(i), inputs_[i]);
  }
}

void ShaderNode::populate_outputs()
{
  /* Reserve a stack for each output in addition to an extra stack at the end to mark the end of
   * the array, as this is what the GPU module functions expect. */
  const int num_output_sockets = node_->output_sockets().size();
  outputs_.resize(num_output_sockets + 1);
  outputs_.last().end = true;

  for (int i = 0; i < num_output_sockets; i++) {
    populate_gpu_node_stack(node_.output(i), outputs_[i]);
  }
}

}  // namespace blender::compositor
