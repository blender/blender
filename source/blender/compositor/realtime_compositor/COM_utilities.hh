/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_math_vector_types.hh"

#include "NOD_derived_node_tree.hh"

#include "GPU_shader.hh"

#include "COM_input_descriptor.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

/**
 * Get the origin socket of the given node input. If the input is not linked, the socket itself is
 * returned. If the input is linked, the socket that is linked to it is returned, which could
 * either be an input or an output. An input socket is returned when the given input is connected
 * to an unlinked input of a group input node.
 */
DSocket get_input_origin_socket(DInputSocket input);

/**
 * Get the output socket linked to the given node input. If the input is not linked to an output,
 * a null output is returned.
 */
DOutputSocket get_output_linked_to_input(DInputSocket input);

/** Get the result type that corresponds to the type of the given socket. */
ResultType get_node_socket_result_type(const bNodeSocket *socket);

/**
 * Returns true if any of the nodes linked to the given output satisfies the given condition,
 * and false otherwise.
 */
bool is_output_linked_to_node_conditioned(DOutputSocket output,
                                          FunctionRef<bool(DNode)> condition);

/** Returns the number of inputs linked to the given output that satisfy the given condition. */
int number_of_inputs_linked_to_output_conditioned(DOutputSocket output,
                                                  FunctionRef<bool(DInputSocket)> condition);

/** A node is a shader node if it defines a method to get a shader node operation. */
bool is_shader_node(DNode node);

/**
 * Returns true if the given node is supported, that is, have an implementation.
 * Returns false otherwise.
 */
bool is_node_supported(DNode node);

/** Get the input descriptor of the given input socket. */
InputDescriptor input_descriptor_from_input_socket(const bNodeSocket *socket);

/**
 * Dispatch the given compute shader in a 2D compute space such that the number of threads in both
 * dimensions is as small as possible but at least covers the entirety of threads_range assuming
 * the shader has a local group size given by local_size. That means that the number of threads
 * might be a bit larger than threads_range, so shaders has to put that into consideration. A
 * default local size of 16x16 is assumed, which is the optimal local size for many image
 * processing shaders.
 */
void compute_dispatch_threads_at_least(GPUShader *shader,
                                       int2 threads_range,
                                       int2 local_size = int2(16));

/* Returns true if a node preview needs to be computed for the give node. */
bool is_node_preview_needed(const DNode &node);

/* Returns the node output that will be used to generate previews. */
DOutputSocket find_preview_output_socket(const DNode &node);

/* Computes a lower resolution version of the given result and sets it as a preview for the given
 * node after applying the appropriate color management specified in the given context. */
void compute_preview_from_result(Context &context, const DNode &node, Result &input_result);

}  // namespace blender::realtime_compositor
