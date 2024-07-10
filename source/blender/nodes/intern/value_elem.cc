/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_value_elem_eval.hh"

namespace blender::nodes::value_elem {

std::optional<ElemVariant> get_elem_variant_for_socket_type(const eNodeSocketDatatype type)
{
  switch (type) {
    case SOCK_FLOAT:
      return {{FloatElem()}};
    case SOCK_INT:
      return {{IntElem()}};
    case SOCK_BOOLEAN:
      return {{BoolElem()}};
    case SOCK_VECTOR:
      return {{VectorElem()}};
    case SOCK_ROTATION:
      return {{RotationElem()}};
    case SOCK_MATRIX:
      return {{MatrixElem()}};
    default:
      return std::nullopt;
  }
}

std::optional<ElemVariant> convert_socket_elem(const bNodeSocket &old_socket,
                                               const bNodeSocket &new_socket,
                                               const ElemVariant &old_elem)
{
  const eNodeSocketDatatype old_type = eNodeSocketDatatype(old_socket.type);
  const eNodeSocketDatatype new_type = eNodeSocketDatatype(new_socket.type);
  if (old_type == new_type) {
    return old_elem;
  }
  if (ELEM(old_type, SOCK_INT, SOCK_FLOAT, SOCK_BOOLEAN) &&
      ELEM(new_type, SOCK_INT, SOCK_FLOAT, SOCK_BOOLEAN))
  {
    std::optional<ElemVariant> new_elem = get_elem_variant_for_socket_type(new_type);
    if (old_elem) {
      new_elem->set_all();
    }
    return new_elem;
  }
  switch (old_type) {
    case SOCK_MATRIX: {
      const MatrixElem &transform_elem = std::get<MatrixElem>(old_elem.elem);
      if (new_type == SOCK_ROTATION) {
        return ElemVariant{transform_elem.rotation};
      }
      break;
    }
    case SOCK_ROTATION: {
      const RotationElem &rotation_elem = std::get<RotationElem>(old_elem.elem);
      if (new_type == SOCK_MATRIX) {
        MatrixElem matrix_elem;
        matrix_elem.rotation = rotation_elem;
        return ElemVariant{matrix_elem};
      }
      if (new_type == SOCK_VECTOR) {
        return ElemVariant{rotation_elem.euler};
      }
      break;
    }
    case SOCK_VECTOR: {
      const VectorElem &vector_elem = std::get<VectorElem>(old_elem.elem);
      if (new_type == SOCK_ROTATION) {
        RotationElem rotation_elem;
        rotation_elem.euler = vector_elem;
        if (rotation_elem) {
          rotation_elem.angle = FloatElem::all();
          rotation_elem.axis = VectorElem::all();
        }
        return ElemVariant{rotation_elem};
      }
    }
    default:
      break;
  }
  return std::nullopt;
}

ElemEvalParams::ElemEvalParams(const bNode &node,
                               const Map<const bNodeSocket *, ElemVariant> &elem_by_socket,
                               Vector<SocketElem> &output_elems)
    : elem_by_socket_(elem_by_socket), output_elems_(output_elems), node(node)
{
}

InverseElemEvalParams::InverseElemEvalParams(
    const bNode &node,
    const Map<const bNodeSocket *, ElemVariant> &elem_by_socket,
    Vector<SocketElem> &input_elems)
    : elem_by_socket_(elem_by_socket), input_elems_(input_elems), node(node)
{
}

}  // namespace blender::nodes::value_elem
