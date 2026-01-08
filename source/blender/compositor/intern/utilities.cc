/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <optional>

#include "BLI_assert.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "NOD_node_declaration.hh"

#include "GPU_compute.hh"
#include "GPU_shader.hh"

#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

bool is_socket_available(const bNodeSocket *socket)
{
  return socket->is_available() && StringRef(socket->idname) != "NodeSocketVirtual";
}

const bNodeSocket *get_output_linked_to_input(const bNodeSocket &input)
{
  if (!input.is_logically_linked()) {
    return nullptr;
  }
  return input.logically_linked_sockets()[0];
}

ResultType socket_data_type_to_result_type(const eNodeSocketDatatype data_type,
                                           const std::optional<int> dimensions)
{
  switch (data_type) {
    case SOCK_FLOAT:
      return ResultType::Float;
    case SOCK_INT:
      return ResultType::Int;
    case SOCK_BOOLEAN:
      return ResultType::Bool;
    case SOCK_VECTOR:
      switch (dimensions.value_or(3)) {
        case 2:
          return ResultType::Float2;
        case 3:
          return ResultType::Float3;
        case 4:
          return ResultType::Float4;
        default:
          BLI_assert_unreachable();
          return ResultType::Float;
      }
    case SOCK_RGBA:
      return ResultType::Color;
    case SOCK_MENU:
      return ResultType::Menu;
    case SOCK_STRING:
      return ResultType::String;
    default:
      BLI_assert_unreachable();
      return ResultType::Float;
  }
}

ResultType get_node_socket_result_type(const bNodeSocket *socket)
{
  /* Gracefully handle undefined sockets, falling back to a float. */
  if (socket->typeinfo == &bke::NodeSocketTypeUndefined) {
    return ResultType::Float;
  }

  const eNodeSocketDatatype socket_type = static_cast<eNodeSocketDatatype>(socket->type);
  if (socket_type == SOCK_VECTOR) {
    return socket_data_type_to_result_type(
        socket_type, socket->default_value_typed<bNodeSocketValueVector>()->dimensions);
  }

  return socket_data_type_to_result_type(socket_type);
}

ResultType get_node_interface_socket_result_type(const bNodeTreeInterfaceSocket &socket)
{
  const eNodeSocketDatatype socket_type = socket.socket_typeinfo()->type;
  if (socket_type == SOCK_VECTOR) {
    return socket_data_type_to_result_type(
        socket_type, static_cast<bNodeSocketValueVector *>(socket.socket_data)->dimensions);
  }

  return socket_data_type_to_result_type(socket_type);
}

bool is_output_linked_to_node_conditioned(const bNodeSocket &output,
                                          FunctionRef<bool(const bNode &)> condition)
{
  for (const bNodeSocket *input : output.logically_linked_sockets()) {
    if (condition(input->owner_node())) {
      return true;
    }
  }
  return false;
}

int number_of_inputs_linked_to_output_conditioned(const bNodeSocket &output,
                                                  FunctionRef<bool(const bNodeSocket &)> condition)
{
  if (!output.is_logically_linked()) {
    return 0;
  }

  int count = 0;
  for (const bNodeSocket *input : output.logically_linked_sockets()) {
    if (condition(*input)) {
      count++;
    }
  }
  return count;
}

bool is_pixel_node(const bNode &node)
{
  BLI_assert(bool(node.typeinfo->gpu_fn) == bool(node.typeinfo->build_multi_function));
  return node.typeinfo->gpu_fn && node.typeinfo->build_multi_function;
}

static ImplicitInput get_implicit_input(const nodes::SocketDeclaration *socket_declaration)
{
  /* We only support implicit textures coordinates, though this can be expanded in the future. */
  if (socket_declaration->input_field_type == nodes::InputSocketFieldType::Implicit) {
    return ImplicitInput::TextureCoordinates;
  }
  return ImplicitInput::None;
}

static int get_domain_priority(const bNodeSocket *input,
                               const nodes::SocketDeclaration *socket_declaration)
{
  /* Negative priority means no priority is set and we fall back to the index, that is, we
   * prioritize inputs according to their order. */
  if (socket_declaration->compositor_domain_priority() < 0) {
    return input->index();
  }
  return socket_declaration->compositor_domain_priority();
}

InputDescriptor input_descriptor_from_input_socket(const bNodeSocket *socket)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = get_node_socket_result_type(socket);

  /* Default to the index of the input as its domain priority in case the node does not have a
   * declaration. */
  input_descriptor.domain_priority = socket->index();

  /* Not every node has a declaration, in which case we assume the default values for the rest of
   * the properties. */
  const nodes::NodeDeclaration *node_declaration = socket->owner_node().declaration();
  if (!node_declaration) {
    return input_descriptor;
  }
  const nodes::SocketDeclaration *socket_declaration = node_declaration->inputs[socket->index()];
  input_descriptor.domain_priority = get_domain_priority(socket, socket_declaration);
  input_descriptor.expects_single_value = socket_declaration->structure_type ==
                                          nodes::StructureType::Single;
  input_descriptor.realization_mode = static_cast<InputRealizationMode>(
      socket_declaration->compositor_realization_mode());
  input_descriptor.implicit_input = get_implicit_input(socket_declaration);

  return input_descriptor;
}

InputDescriptor input_descriptor_from_interface_input(const bNodeTree &node_group,
                                                      const bNodeTreeInterfaceSocket &socket)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = get_node_interface_socket_result_type(socket);
  input_descriptor.domain_priority = node_group.interface_input_index(socket);
  input_descriptor.expects_single_value = socket.structure_type ==
                                          NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_SINGLE;
  input_descriptor.realization_mode = InputRealizationMode::None;
  input_descriptor.implicit_input = socket.default_input == NODE_DEFAULT_INPUT_POSITION_FIELD ?
                                        ImplicitInput::TextureCoordinates :
                                        ImplicitInput::None;

  return input_descriptor;
}

void compute_dispatch_threads_at_least(gpu::Shader *shader, int2 threads_range, int2 local_size)
{
  /* If the threads range is divisible by the local size, dispatch the number of needed groups,
   * which is their division. If it is not divisible, then dispatch an extra group to cover the
   * remaining invocations, which means the actual threads range of the dispatch will be a bit
   * larger than the given one. */
  const int2 groups_to_dispatch = math::divide_ceil(threads_range, local_size);
  GPU_compute_dispatch(shader, groups_to_dispatch.x, groups_to_dispatch.y, 1);
}

bool is_node_preview_needed(const bNode &node)
{
  if (!(node.flag & NODE_PREVIEW)) {
    return false;
  }

  if (node.flag & NODE_COLLAPSED) {
    return false;
  }

  return true;
}

const bNodeSocket *find_preview_output_socket(const bNode &node)
{
  if (!is_node_preview_needed(node)) {
    return nullptr;
  }

  for (const bNodeSocket *output : node.output_sockets()) {
    if (is_socket_available(output) && output->is_logically_linked()) {
      return output;
    }
  }

  return nullptr;
}

}  // namespace blender::compositor
