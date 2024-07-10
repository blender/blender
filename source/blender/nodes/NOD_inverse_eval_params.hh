/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"

#include "BKE_node_runtime.hh"
#include "BKE_node_socket_value.hh"

namespace blender::nodes::inverse_eval {

/**
 * Is passed to inverse node evaluation functions to figure out how the inputs have to change
 * exactly to get a specific output value. What is special here is that this gives access to
 * (logged) node inputs and node output values, instead of just either inputs or outputs.
 *
 * This is required because sometimes certain inputs are fixed and need to be known to be able to
 * figure out how another input changes. A typical example of this is the math node, where the
 * second input is fixed and only the first input changes.
 */
class InverseEvalParams {
 private:
  const Map<const bNodeSocket *, bke::SocketValueVariant> &socket_values_;
  Map<const bNodeSocket *, bke::SocketValueVariant> &updated_socket_values_;

 public:
  const bNode &node;

  InverseEvalParams(const bNode &node,
                    const Map<const bNodeSocket *, bke::SocketValueVariant> &socket_values,
                    Map<const bNodeSocket *, bke::SocketValueVariant> &updated_socket_values);

  template<typename T> T get_output(const StringRef identifier) const
  {
    const bNodeSocket &socket = node.output_by_identifier(identifier);
    if (const bke::SocketValueVariant *value = socket_values_.lookup_ptr(&socket)) {
      return value->get<T>();
    }
    return T();
  }

  template<typename T> T get_input(const StringRef identifier) const
  {
    const bNodeSocket &socket = node.input_by_identifier(identifier);
    if (const bke::SocketValueVariant *value = socket_values_.lookup_ptr(&socket)) {
      return value->get<T>();
    }
    return T();
  }

  template<typename T> void set_input(const StringRef identifier, T value)
  {
    const bNodeSocket &socket = node.input_by_identifier(identifier);
    updated_socket_values_.add(&socket, bke::SocketValueVariant(value));
  }
};

}  // namespace blender::nodes::inverse_eval
