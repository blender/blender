/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"
#include "BLI_generic_pointer.hh"

struct bNodeTree;
struct bNodeSocket;
struct IDProperty;

namespace blender::nodes::socket_usage_inference {

/**
 * Get a boolean value for each input socket in the given tree that indicates whether that input is
 * used. It is assumed that all output sockets in the tree are used.
 */
Array<bool> infer_all_input_sockets_usage(const bNodeTree &tree);

/**
 * Get a boolean value for each node group input that indicates whether that input is used by the
 * outputs. The result can be used to e.g. gray out or hide individual inputs that are unused.
 *
 * \param group: The node group that is called.
 * \param group_input_values: An optional input value for each node group input. The type is
 *   expected to be `bNodeSocketType::base_cpp_type`. If the input value for a socket is not known
 *   or can't be represented as base type, null has to be passed instead.
 * \param r_input_usages: The destination array where the inferred usages are written.
 */
void infer_group_interface_inputs_usage(const bNodeTree &group,
                                        Span<GPointer> group_input_values,
                                        MutableSpan<bool> r_input_usages);

/**
 * Same as above, but automatically retrieves the input values from the given sockets..
 * This is used for group nodes.
 */
void infer_group_interface_inputs_usage(const bNodeTree &group,
                                        Span<const bNodeSocket *> input_sockets,
                                        MutableSpan<bool> r_input_usages);

/**
 * Same as above, but automatically retrieves the input values from the given properties.
 * This is used with the geometry nodes modifier and node tools.
 */
void infer_group_interface_inputs_usage(const bNodeTree &group,
                                        const IDProperty *properties,
                                        MutableSpan<bool> r_input_usages);

}  // namespace blender::nodes::socket_usage_inference
