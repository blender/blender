/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <map>

#include <MaterialXCore/Node.h>

namespace blender::nodes::materialx {

/**
 * This class serves as abstraction from MateralX API. It implements arithmetic operations,
 * conversions between different types, adding new nodes, setting inputs, etc.
 * All work should be done via this class instead of using MaterialX API directly.
 */
class NodeItem {
 public:
  using Inputs = std::vector<std::pair<std::string, NodeItem>>;

  enum class Type {
    Any = 0,
    Empty,
    Multioutput,

    /* Value types */
    String,
    Filename,
    Boolean,
    Integer,

    /* Arithmetic types. NOTE: Ordered by type cast */
    Float,
    Vector2,
    Vector3,
    Color3,
    Vector4,
    Color4,

    /* Shader types. NOTE: There are only supported types */
    BSDF,
    EDF,
    DisplacementShader,
    SurfaceShader,
    Material,

    /* Special type to retrieve opacity for <surface> */
    SurfaceOpacity,
  };
  enum class CompareOp { Less = 0, LessEq, Eq, GreaterEq, Greater, NotEq };

 public:
  MaterialX::ValuePtr value;
  MaterialX::NodePtr node;
  MaterialX::InputPtr input;
  MaterialX::OutputPtr output;

 private:
  MaterialX::GraphElement *graph_ = nullptr;

 public:
  /* NOTE: Default constructor added to allow easy work with std::map.
   * Don't use this constructor to create NodeItem. */
  NodeItem() = default;
  NodeItem(MaterialX::GraphElement *graph);
  ~NodeItem() = default;

  static Type type(const std::string &type_str);
  static std::string type(Type type);
  static bool is_arithmetic(Type type);

  /* Operators */
  operator bool() const;
  NodeItem operator+(const NodeItem &other) const;
  NodeItem operator-(const NodeItem &other) const;
  NodeItem operator-() const;
  NodeItem operator*(const NodeItem &other) const;
  NodeItem operator/(const NodeItem &other) const;
  NodeItem operator%(const NodeItem &other) const;
  NodeItem operator^(const NodeItem &other) const;
  NodeItem operator[](int index) const;
  bool operator==(const NodeItem &other) const;
  bool operator!=(const NodeItem &other) const;

  /* Math functions */
  NodeItem abs() const;
  NodeItem floor() const;
  NodeItem ceil() const;
  NodeItem length() const;
  NodeItem normalize() const;
  NodeItem min(const NodeItem &other) const;
  NodeItem max(const NodeItem &other) const;
  NodeItem dotproduct(const NodeItem &other) const;
  NodeItem mix(const NodeItem &val1, const NodeItem &val2) const;
  NodeItem clamp(const NodeItem &min_val, const NodeItem &max_val) const;
  NodeItem clamp(float min_val = 0.0f, float max_val = 1.0f) const;
  NodeItem rotate(const NodeItem &angle, const NodeItem &axis);    /* angle in degrees */
  NodeItem rotate(const NodeItem &angle_xyz, bool invert = false); /* angle in degrees */
  NodeItem sin() const;
  NodeItem cos() const;
  NodeItem tan() const;
  NodeItem asin() const;
  NodeItem acos() const;
  NodeItem atan() const;
  NodeItem atan2(const NodeItem &other) const;
  NodeItem sinh() const;
  NodeItem cosh() const;
  NodeItem tanh() const;
  NodeItem ln() const;
  NodeItem sqrt() const;
  NodeItem sign() const;
  NodeItem exp() const;
  NodeItem convert(Type to_type) const;
  NodeItem to_vector() const;
  NodeItem if_else(CompareOp op,
                   const NodeItem &other,
                   const NodeItem &if_val,
                   const NodeItem &else_val) const;

  /* Useful functions */
  NodeItem empty() const;
  template<class T> NodeItem val(const T &data) const;
  Type type() const;

  /* Node functions */
  NodeItem create_node(const std::string &category, Type type) const;
  NodeItem create_node(const std::string &category, Type type, const Inputs &inputs) const;
  template<class T> void set_input(const std::string &in_name, const T &value, Type in_type);
  void set_input(const std::string &in_name, const NodeItem &item);
  NodeItem add_output(const std::string &out_name, Type out_type);

  /* Output functions */
  NodeItem create_input(const std::string &name, const NodeItem &item) const;
  NodeItem create_output(const std::string &name, const NodeItem &item) const;

 private:
  static Type cast_types(NodeItem &item1, NodeItem &item2);

  bool is_arithmetic() const;
  NodeItem arithmetic(const std::string &category, std::function<float(float)> func) const;
  NodeItem arithmetic(const NodeItem &other,
                      const std::string &category,
                      std::function<float(float, float)> func,
                      Type to_type = Type::Any) const;
};

template<class T> NodeItem NodeItem::val(const T &data) const
{
  NodeItem res(graph_);
  res.value = MaterialX::Value::createValue<T>(data);
  return res;
}

template<class T>
void NodeItem::set_input(const std::string &in_name, const T &value, Type in_type)
{
  node->setInputValue(in_name, value, type(in_type));
}

}  // namespace blender::nodes::materialx
