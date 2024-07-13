/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * A #ValueElem is an abstract element or part of a value. It does not store the actual value of
 * the type but which parts of it are affected. For example, #VectorElem does not store the actual
 * vector values but just a boolean for each component.
 *
 * Some nodes implement special #node_eval_elem and #node_eval_inverse_elem methods which allow
 * analyzing the potential impact of changing part of a value in one place of a node tree.
 *
 * The types are generally quite small and trivially copyable and destructible.
 * They just contain some booleans.
 */

#include <optional>
#include <variant>

#include "BLI_hash.hh"
#include "BLI_struct_equality_utils.hh"

#include "DNA_node_types.h"

namespace blender::nodes::value_elem {

/**
 * Common base type value primitive types that can't be subdivided further.
 */
struct PrimitiveValueElem {
  bool affected = false;

  operator bool() const
  {
    return this->affected;
  }

  BLI_STRUCT_EQUALITY_OPERATORS_1(PrimitiveValueElem, affected)

  void merge(const PrimitiveValueElem &other)
  {
    this->affected |= other.affected;
  }

  void intersect(const PrimitiveValueElem &other)
  {
    this->affected &= other.affected;
  }

  uint64_t hash() const
  {
    return get_default_hash(this->affected);
  }
};

struct BoolElem : public PrimitiveValueElem {
  static BoolElem all()
  {
    return {true};
  }
};

struct FloatElem : public PrimitiveValueElem {
  static FloatElem all()
  {
    return {true};
  }
};

struct IntElem : public PrimitiveValueElem {
  static IntElem all()
  {
    return {true};
  }
};

struct VectorElem {
  /** Members indicate which components of the vector are affected. */
  FloatElem x;
  FloatElem y;
  FloatElem z;

  operator bool() const
  {
    return this->x || this->y || this->z;
  }

  BLI_STRUCT_EQUALITY_OPERATORS_3(VectorElem, x, y, z)

  uint64_t hash() const
  {
    return get_default_hash(this->x, this->y, this->z);
  }

  void merge(const VectorElem &other)
  {
    this->x.merge(other.x);
    this->y.merge(other.y);
    this->z.merge(other.z);
  }

  void intersect(const VectorElem &other)
  {
    this->x.intersect(other.x);
    this->y.intersect(other.y);
    this->z.intersect(other.z);
  }

  static VectorElem all()
  {
    return {{true}, {true}, {true}};
  }
};

struct RotationElem {
  /**
   * The euler and axis-angle components have overlap. All components that can be affected need to
   * be tagged. For example if a node affects the euler angles, it indirectly also affects the
   * axis-angle.
   */
  VectorElem euler;
  VectorElem axis;
  FloatElem angle;

  operator bool() const
  {
    return this->euler || this->axis || this->angle;
  }

  BLI_STRUCT_EQUALITY_OPERATORS_3(RotationElem, euler, axis, angle)

  uint64_t hash() const
  {
    return get_default_hash(this->euler, this->axis, this->angle);
  }

  void merge(const RotationElem &other)
  {
    this->euler.merge(other.euler);
    this->axis.merge(other.axis);
    this->angle.merge(other.angle);
  }

  void intersect(const RotationElem &other)
  {
    this->euler.intersect(other.euler);
    this->axis.intersect(other.axis);
    this->angle.intersect(other.angle);
  }

  static RotationElem all()
  {
    return {VectorElem::all(), VectorElem::all(), {true}};
  }
};

struct MatrixElem {
  VectorElem translation;
  RotationElem rotation;
  VectorElem scale;
  /** For 4x4 matrices this describes whether any entry of the last row is affected. */
  FloatElem any_non_transform;

  operator bool() const
  {
    return this->translation || this->rotation || this->scale || this->any_non_transform;
  }

  BLI_STRUCT_EQUALITY_OPERATORS_4(MatrixElem, translation, rotation, scale, any_non_transform)

  uint64_t hash() const
  {
    return get_default_hash(
        this->translation, this->rotation, this->scale, this->any_non_transform);
  }

  void merge(const MatrixElem &other)
  {
    this->translation.merge(other.translation);
    this->rotation.merge(other.rotation);
    this->scale.merge(other.scale);
    this->any_non_transform.merge(other.any_non_transform);
  }

  void intersect(const MatrixElem &other)
  {
    this->translation.intersect(other.translation);
    this->rotation.intersect(other.rotation);
    this->scale.intersect(other.scale);
    this->any_non_transform.intersect(other.any_non_transform);
  }

  static MatrixElem all()
  {
    return {VectorElem::all(), RotationElem::all(), VectorElem::all(), FloatElem::all()};
  }
};

/**
 * A generic type that can hold the value element for any of the above types and has the same
 * interface. This should be used when the data type is not known at compile time.
 */
struct ElemVariant {
  std::variant<BoolElem, FloatElem, IntElem, VectorElem, RotationElem, MatrixElem> elem;

  operator bool() const
  {
    return std::visit([](const auto &value) { return bool(value); }, this->elem);
  }

  uint64_t hash() const
  {
    return std::visit([](auto &value) { return value.hash(); }, this->elem);
  }

  void merge(const ElemVariant &other)
  {
    BLI_assert(this->elem.index() == other.elem.index());
    std::visit(
        [&](auto &value) {
          using T = std::decay_t<decltype(value)>;
          value.merge(std::get<T>(other.elem));
        },
        this->elem);
  }

  void intersect(const ElemVariant &other)
  {
    BLI_assert(this->elem.index() == other.elem.index());
    std::visit(
        [&](auto &value) {
          using T = std::decay_t<decltype(value)>;
          value.intersect(std::get<T>(other.elem));
        },
        this->elem);
  }

  void set_all()
  {
    std::visit(
        [](auto &value) {
          using T = std::decay_t<decltype(value)>;
          value = T::all();
        },
        this->elem);
  }

  void clear_all()
  {
    std::visit(
        [](auto &value) {
          using T = std::decay_t<decltype(value)>;
          value = T();
        },
        this->elem);
  }

  BLI_STRUCT_EQUALITY_OPERATORS_1(ElemVariant, elem)
};

/** Utility struct to pair a socket with a value element. */
struct SocketElem {
  const bNodeSocket *socket = nullptr;
  ElemVariant elem;

  uint64_t hash() const
  {
    return get_default_hash(this->socket, this->elem);
  }

  BLI_STRUCT_EQUALITY_OPERATORS_2(SocketElem, socket, elem)
};

/** Utility struct to pair a group input index with a value element. */
struct GroupInputElem {
  int group_input_index = 0;
  ElemVariant elem;

  uint64_t hash() const
  {
    return get_default_hash(this->group_input_index, this->elem);
  }

  BLI_STRUCT_EQUALITY_OPERATORS_2(GroupInputElem, group_input_index, elem)
};

/** Utility struct to pair a value node with a value element. */
struct ValueNodeElem {
  const bNode *node = nullptr;
  ElemVariant elem;

  uint64_t hash() const
  {
    return get_default_hash(this->node, this->elem);
  }

  BLI_STRUCT_EQUALITY_OPERATORS_2(ValueNodeElem, node, elem)
};

/**
 * Get the default value element for the given socket type if it exists.
 */
std::optional<ElemVariant> get_elem_variant_for_socket_type(eNodeSocketDatatype type);

/** Converts the type of a value element if possible. */
std::optional<ElemVariant> convert_socket_elem(const bNodeSocket &old_socket,
                                               const bNodeSocket &new_socket,
                                               const ElemVariant &old_elem);

}  // namespace blender::nodes::value_elem
