/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "DNA_node_types.h"

#include "GPU_shader.hh"

#include "COM_input_descriptor.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* Returns true if the socket is available and not virtual. Returns false otherwise. */
bool is_socket_available(const bNodeSocket *socket);

/* Get the output socket linked to the given node input. If the input is not linked to an output,
 * a null output is returned. */
const bNodeSocket *get_output_linked_to_input(const bNodeSocket &input);

/* Get the result type that corresponds to the given socket data type. For vector sockets, the
 * dimensions of the socket can be provided, but if not provided, 3 will be assumed. */
ResultType socket_data_type_to_result_type(const eNodeSocketDatatype data_type,
                                           const std::optional<int> dimensions = std::nullopt);

/* Get the result type that corresponds to the type of the given socket. */
ResultType get_node_socket_result_type(const bNodeSocket *socket);

/* Get the result type that corresponds to the type of the given interface socket. */
ResultType get_node_interface_socket_result_type(const bNodeTreeInterfaceSocket &socket);

/* Returns true if any of the nodes linked to the given output satisfies the given condition,
 * and false otherwise. */
bool is_output_linked_to_node_conditioned(const bNodeSocket &output,
                                          FunctionRef<bool(const bNode &)> condition);

/* Returns the number of inputs linked to the given output that satisfy the given condition. */
int number_of_inputs_linked_to_output_conditioned(
    const bNodeSocket &output, FunctionRef<bool(const bNodeSocket &)> condition);

/* A node is a pixel node if it defines a method to get a pixel node operation. */
bool is_pixel_node(const bNode &node);

/* Get the input descriptor of the given input socket. */
InputDescriptor input_descriptor_from_input_socket(const bNodeSocket *socket);

/* Get the input descriptor of the given interface input of the given node group. */
InputDescriptor input_descriptor_from_interface_input(const bNodeTree &node_group,
                                                      const bNodeTreeInterfaceSocket &socket);

/* Dispatch the given compute shader in a 2D compute space such that the number of threads in both
 * dimensions is as small as possible but at least covers the entirety of threads_range assuming
 * the shader has a local group size given by local_size. That means that the number of threads
 * might be a bit larger than threads_range, so shaders has to put that into consideration. A
 * default local size of 16x16 is assumed, which is the optimal local size for many image
 * processing shaders. */
void compute_dispatch_threads_at_least(gpu::Shader *shader,
                                       int2 threads_range,
                                       int2 local_size = int2(16));

/* Returns true if a node preview needs to be computed for the give node. */
bool is_node_preview_needed(const bNode &node);

/* Returns the node output that will be used to generate previews. */
const bNodeSocket *find_preview_output_socket(const bNode &node);

/* -------------------------------------------------------------------- */
/* Inline Functions.
 */

/* Executes the given function in parallel over the given 2D range. The given function gets the
 * texel coordinates of the element of the range as an argument. */
template<typename Function> inline void parallel_for(const int2 range, const Function &function)
{
  threading::parallel_for(IndexRange(range.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(range.x)) {
        function(int2(x, y));
      }
    }
  });
}

}  // namespace blender::compositor
