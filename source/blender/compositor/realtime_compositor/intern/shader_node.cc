/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_vector.h"
#include "BLI_string_ref.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

ShaderNode::ShaderNode(DNode node) : node_(node)
{
  populate_inputs();
  populate_outputs();
}

GPUNodeStack *ShaderNode::get_inputs_array()
{
  return inputs_.data();
}

GPUNodeStack *ShaderNode::get_outputs_array()
{
  return outputs_.data();
}

GPUNodeStack &ShaderNode::get_input(StringRef identifier)
{
  return inputs_[node_.input_by_identifier(identifier)->index()];
}

GPUNodeStack &ShaderNode::get_output(StringRef identifier)
{
  return outputs_[node_.output_by_identifier(identifier)->index()];
}

GPUNodeLink *ShaderNode::get_input_link(StringRef identifier)
{
  GPUNodeStack &input = get_input(identifier);
  if (input.link) {
    return input.link;
  }
  return GPU_uniform(input.vec);
}

const DNode &ShaderNode::node() const
{
  return node_;
}

const bNode &ShaderNode::bnode() const
{
  return *node_;
}

static eGPUType gpu_type_from_socket_type(eNodeSocketDatatype type)
{
  switch (type) {
    case SOCK_FLOAT:
      return GPU_FLOAT;
    case SOCK_VECTOR:
      return GPU_VEC3;
    case SOCK_RGBA:
      return GPU_VEC4;
    default:
      BLI_assert_unreachable();
      return GPU_NONE;
  }
}

/* Initializes the vector value of the given GPU node stack from the default value of the given
 * socket. Note that the type of the stack may not match that of the socket, so perform implicit
 * conversion if needed. */
static void gpu_stack_vector_from_socket(GPUNodeStack &stack, const bNodeSocket *socket)
{
  switch (socket->type) {
    case SOCK_FLOAT: {
      const float value = socket->default_value_typed<bNodeSocketValueFloat>()->value;
      switch (stack.sockettype) {
        case SOCK_FLOAT:
          stack.vec[0] = value;
          return;
        case SOCK_VECTOR:
          copy_v3_fl(stack.vec, value);
          return;
        case SOCK_RGBA:
          copy_v4_fl(stack.vec, value);
          stack.vec[3] = 1.0f;
          return;
        default:
          BLI_assert_unreachable();
          return;
      }
    }
    case SOCK_VECTOR: {
      const float *value = socket->default_value_typed<bNodeSocketValueVector>()->value;
      switch (stack.sockettype) {
        case SOCK_FLOAT:
          stack.vec[0] = (value[0] + value[1] + value[2]) / 3.0f;
          return;
        case SOCK_VECTOR:
          copy_v3_v3(stack.vec, value);
          return;
        case SOCK_RGBA:
          copy_v3_v3(stack.vec, value);
          stack.vec[3] = 1.0f;
          return;
        default:
          BLI_assert_unreachable();
          return;
      }
    }
    case SOCK_RGBA: {
      const float *value = socket->default_value_typed<bNodeSocketValueRGBA>()->value;
      switch (stack.sockettype) {
        case SOCK_FLOAT:
          stack.vec[0] = (value[0] + value[1] + value[2]) / 3.0f;
          return;
        case SOCK_VECTOR:
          copy_v3_v3(stack.vec, value);
          return;
        case SOCK_RGBA:
          copy_v4_v4(stack.vec, value);
          return;
        default:
          BLI_assert_unreachable();
          return;
      }
    }
    default:
      BLI_assert_unreachable();
  }
}

static void populate_gpu_node_stack(DSocket socket, GPUNodeStack &stack)
{
  /* Make sure this stack is not marked as the end of the stack array. */
  stack.end = false;
  /* This will be initialized later by the GPU material compiler or the compile method. */
  stack.link = nullptr;

  stack.sockettype = socket->type;
  stack.type = gpu_type_from_socket_type((eNodeSocketDatatype)socket->type);

  if (socket->is_input()) {
    const DInputSocket input(socket);

    DSocket origin = get_input_origin_socket(input);

    /* The input is linked if the origin socket is an output socket. Had it been an input socket,
     * then it is an unlinked input of a group input node. */
    stack.hasinput = origin->is_output();

    /* Get the socket value from the origin if it is an input, because then it would either be an
     * unlinked input or an unlinked input of a group input node that the socket is linked to,
     * otherwise, get the value from the socket itself. */
    if (origin->is_input()) {
      gpu_stack_vector_from_socket(stack, origin.bsocket());
    }
    else {
      gpu_stack_vector_from_socket(stack, socket.bsocket());
    }
  }
  else {
    stack.hasoutput = socket->is_logically_linked();
  }
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

}  // namespace blender::realtime_compositor
