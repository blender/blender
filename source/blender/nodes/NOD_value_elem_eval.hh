/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "NOD_value_elem.hh"

#include "BLI_map.hh"

namespace blender::nodes::value_elem {

/**
 * Is passed to the node evaluation function to figure which outputs change when parts of the
 * inputs change.
 */
class ElemEvalParams {
 private:
  const Map<const bNodeSocket *, ElemVariant> &elem_by_socket_;
  Vector<SocketElem> &output_elems_;

 public:
  const bNode &node;

  ElemEvalParams(const bNode &node,
                 const Map<const bNodeSocket *, ElemVariant> &elem_by_socket,
                 Vector<SocketElem> &output_elems);

  template<typename T> T get_input_elem(const StringRef identifier) const
  {
    const bNodeSocket &socket = node.input_by_identifier(identifier);
    if (const ElemVariant *elem = this->elem_by_socket_.lookup_ptr(&socket)) {
      return std::get<T>(elem->elem);
    }
    return T();
  }

  template<typename T> void set_output_elem(const StringRef identifier, T elem)
  {
    const bNodeSocket &socket = node.output_by_identifier(identifier);
    output_elems_.append({&socket, ElemVariant{elem}});
  }
};

/**
 * Same as above but for inverse evaluation, i.e. to figure out which inputs need to change when
 * specific parts of the output change.
 */
class InverseElemEvalParams {
 private:
  const Map<const bNodeSocket *, ElemVariant> &elem_by_socket_;
  Vector<SocketElem> &input_elems_;

 public:
  const bNode &node;

  InverseElemEvalParams(const bNode &node,
                        const Map<const bNodeSocket *, ElemVariant> &elem_by_socket,
                        Vector<SocketElem> &input_elems);

  template<typename T> T get_output_elem(const StringRef identifier) const
  {
    const bNodeSocket &socket = node.output_by_identifier(identifier);
    if (const ElemVariant *elem = this->elem_by_socket_.lookup_ptr(&socket)) {
      return std::get<T>(elem->elem);
    }
    return T();
  }

  template<typename T> void set_input_elem(const StringRef identifier, T elem)
  {
    const bNodeSocket &socket = node.input_by_identifier(identifier);
    input_elems_.append({&socket, ElemVariant{elem}});
  }
};

}  // namespace blender::nodes::value_elem
