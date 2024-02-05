/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_item.h"
#include "node_parser.h"

#include "BLI_assert.h"
#include "BLI_utildefines.h"

namespace blender::nodes::materialx {

NodeItem::NodeItem(MaterialX::GraphElement *graph) : graph_(graph) {}

NodeItem::Type NodeItem::type(const std::string &type_str)
{
  /* Converting only MaterialX supported types */
  if (type_str == "multioutput") {
    return Type::Multioutput;
  }
  if (type_str == "string") {
    return Type::String;
  }
  if (type_str == "filename") {
    return Type::Filename;
  }
  if (type_str == "boolean") {
    return Type::Boolean;
  }
  if (type_str == "integer") {
    return Type::Integer;
  }
  if (type_str == "float") {
    return Type::Float;
  }
  if (type_str == "vector2") {
    return Type::Vector2;
  }
  if (type_str == "vector3") {
    return Type::Vector3;
  }
  if (type_str == "vector4") {
    return Type::Vector4;
  }
  if (type_str == "color3") {
    return Type::Color3;
  }
  if (type_str == "color4") {
    return Type::Color4;
  }
  if (type_str == "BSDF") {
    return Type::BSDF;
  }
  if (type_str == "EDF") {
    return Type::EDF;
  }
  if (type_str == "displacementshader") {
    return Type::DisplacementShader;
  }
  if (type_str == "surfaceshader") {
    return Type::SurfaceShader;
  }
  if (type_str == "material") {
    return Type::Material;
  }
  BLI_assert_unreachable();
  return Type::Empty;
}

std::string NodeItem::type(Type type)
{
  switch (type) {
    case Type::Any:
      return "any";
    case Type::Multioutput:
      return "multioutput";
    case Type::String:
      return "string";
    case Type::Filename:
      return "filename";
    case Type::Boolean:
      return "boolean";
    case Type::Integer:
      return "integer";
    case Type::Float:
      return "float";
    case Type::Vector2:
      return "vector2";
    case Type::Vector3:
      return "vector3";
    case Type::Vector4:
      return "vector4";
    case Type::Color3:
      return "color3";
    case Type::Color4:
      return "color4";
    case Type::BSDF:
      return "BSDF";
    case Type::EDF:
      return "EDF";
    case Type::DisplacementShader:
      return "displacementshader";
    case Type::SurfaceShader:
      return "surfaceshader";
    case Type::Material:
      return "material";
    case Type::SurfaceOpacity:
      return "opacity";
    default:
      BLI_assert_unreachable();
  }
  return "";
}

bool NodeItem::is_arithmetic(Type type)
{
  return type >= Type::Float && type <= Type::Color4;
}

NodeItem::operator bool() const
{
  return value || node || input || output;
}

NodeItem NodeItem::operator+(const NodeItem &other) const
{
  Type type = this->type();
  if (ELEM(type, Type::BSDF, Type::EDF)) {
    /* Special case: add BSDF/EDF shaders */
    NodeItem res = empty();
    if (other.type() == type) {
      res = create_node("add", type, {{"in1", *this}, {"in2", other}});
    }
    else {
      BLI_assert_unreachable();
    }
    return res;
  }

  return arithmetic(other, "add", [](float a, float b) { return a + b; });
}

NodeItem NodeItem::operator-(const NodeItem &other) const
{
  return arithmetic(other, "subtract", [](float a, float b) { return a - b; });
}

NodeItem NodeItem::operator-() const
{
  return val(0.0f) - *this;
}

NodeItem NodeItem::operator*(const NodeItem &other) const
{
  Type type = this->type();
  if (ELEM(type, Type::BSDF, Type::EDF)) {
    /* Special case: multiple BSDF/EDF shader by Float or Color3 */
    NodeItem res = empty();
    Type other_type = other.type();
    if (ELEM(other_type, Type::Float, Type::Color3)) {
      res = create_node("multiply", type, {{"in1", *this}, {"in2", other}});
    }
    else {
      BLI_assert_unreachable();
    }
    return res;
  }

  return arithmetic(other, "multiply", [](float a, float b) { return a * b; });
}

NodeItem NodeItem::operator/(const NodeItem &other) const
{
  return arithmetic(other, "divide", [](float a, float b) { return b == 0.0f ? 0.0f : a / b; });
}

NodeItem NodeItem::operator%(const NodeItem &other) const
{
  return arithmetic(
      other, "modulo", [](float a, float b) { return b == 0.0f ? 0.0f : std::fmod(a, b); });
}

NodeItem NodeItem::operator^(const NodeItem &other) const
{
  return arithmetic(other, "power", [](float a, float b) { return std::pow(a, b); });
}

NodeItem NodeItem::operator[](int index) const
{
  BLI_assert(is_arithmetic(type()));

  if (value) {
    float v = 0.0f;
    switch (type()) {
      case Type::Float:
        v = value->asA<float>();
        break;
      case Type::Vector2:
        v = value->asA<MaterialX::Vector2>()[index];
        break;
      case Type::Vector3:
        v = value->asA<MaterialX::Vector3>()[index];
        break;
      case Type::Vector4:
        v = value->asA<MaterialX::Vector4>()[index];
        break;
      case Type::Color3:
        v = value->asA<MaterialX::Color3>()[index];
        break;
      case Type::Color4:
        v = value->asA<MaterialX::Color4>()[index];
        break;
      default:
        BLI_assert_unreachable();
    }
    return val(v);
  }
  return create_node("extract", Type::Float, {{"in", *this}, {"index", val(index)}});
}

bool NodeItem::operator==(const NodeItem &other) const
{
  if (!*this) {
    return !other;
  }
  if (!other) {
    return !*this;
  }
  if (node && node == other.node) {
    return true;
  }
  if ((node && other.value) || (value && other.node)) {
    return false;
  }

  NodeItem item1 = *this;
  NodeItem item2 = other;
  Type to_type = cast_types(item1, item2);
  if (to_type == Type::Empty) {
    return false;
  }
  return item1.value->getValueString() == item2.value->getValueString();
}

bool NodeItem::operator!=(const NodeItem &other) const
{
  return !(*this == other);
}

NodeItem NodeItem::abs() const
{
  return arithmetic("absval", [](float a) { return std::abs(a); });
}

NodeItem NodeItem::floor() const
{
  return arithmetic("floor", [](float a) { return std::floor(a); });
}

NodeItem NodeItem::ceil() const
{
  return arithmetic("ceil", [](float a) { return std::ceil(a); });
}

NodeItem NodeItem::length() const
{
  if (value) {
    return dotproduct(*this).sqrt();
  }
  return create_node("magnitude", Type::Float, {{"in", to_vector()}});
}

NodeItem NodeItem::normalize() const
{
  if (value) {
    return *this / length();
  }
  return create_node("normalize", Type::Vector3, {{"in", to_vector()}});
}

NodeItem NodeItem::min(const NodeItem &other) const
{
  return arithmetic(other, "min", [](float a, float b) { return std::min(a, b); });
}

NodeItem NodeItem::max(const NodeItem &other) const
{
  return arithmetic(other, "max", [](float a, float b) { return std::max(a, b); });
}

NodeItem NodeItem::dotproduct(const NodeItem &other) const
{
  if (value && other.value) {
    NodeItem d = *this * other;
    float f = 0.0f;
    switch (d.type()) {
      case Type::Float: {
        f = d.value->asA<float>();
        break;
      }
      case Type::Vector2: {
        auto v = d.value->asA<MaterialX::Vector2>();
        f = v[0] + v[1];
        break;
      }
      case Type::Vector3: {
        auto v = d.value->asA<MaterialX::Vector3>();
        f = v[0] + v[1] + v[2];
        break;
      }
      case Type::Vector4: {
        auto v = d.value->asA<MaterialX::Vector4>();
        f = v[0] + v[1] + v[2] + v[3];
        break;
      }
      case Type::Color3: {
        auto v = d.value->asA<MaterialX::Color3>();
        f = v[0] + v[1] + v[2];
        break;
      }
      case Type::Color4: {
        auto v = d.value->asA<MaterialX::Color4>();
        f = v[0] + v[1] + v[2] + v[3];
        break;
      }
      default:
        BLI_assert_unreachable();
    }
    return val(f);
  }

  NodeItem item1 = to_vector();
  NodeItem item2 = other.to_vector();
  cast_types(item1, item2);
  return create_node("dotproduct", Type::Float, {{"in1", item1}, {"in2", item2}});
}

NodeItem NodeItem::mix(const NodeItem &val1, const NodeItem &val2) const
{
  if ((value && val1.value && val2.value) || type() != Type::Float) {
    return (val(1.0f) - *this) * val1 + *this * val2;
  }

  Type type1 = val1.type();
  if (ELEM(type1, Type::BSDF, Type::EDF)) {
    BLI_assert(val2.type() == type1);

    /* Special case: mix BSDF/EDF shaders */
    return create_node("mix", type1, {{"bg", val1}, {"fg", val2}, {"mix", *this}});
  };

  NodeItem item1 = val1;
  NodeItem item2 = val2;
  Type to_type = cast_types(item1, item2);
  return create_node("mix", to_type, {{"bg", item1}, {"fg", item2}, {"mix", *this}});
}

NodeItem NodeItem::clamp(const NodeItem &min_val, const NodeItem &max_val) const
{
  if (value && min_val.value && max_val.value) {
    return min(max_val).max(min_val);
  }

  if (min_val.type() == Type::Float && max_val.type() == Type::Float) {
    return create_node("clamp", type(), {{"in", *this}, {"low", min_val}, {"high", max_val}});
  }

  Type type = this->type();
  return create_node(
      "clamp",
      type,
      {{"in", *this}, {"low", min_val.convert(type)}, {"high", max_val.convert(type)}});
}

NodeItem NodeItem::clamp(float min_val, float max_val) const
{
  return clamp(val(min_val), val(max_val));
}

NodeItem NodeItem::rotate(const NodeItem &angle, const NodeItem &axis)
{
  BLI_assert(type() == Type::Vector3);
  BLI_assert(angle.type() == Type::Float);
  BLI_assert(axis.type() == Type::Vector3);

  return create_node(
      "rotate3d", NodeItem::Type::Vector3, {{"in", *this}, {"amount", angle}, {"axis", axis}});
}

NodeItem NodeItem::rotate(const NodeItem &angle_xyz, bool invert)
{
  NodeItem x = angle_xyz[0];
  NodeItem y = angle_xyz[1];
  NodeItem z = angle_xyz[2];

  NodeItem x_axis = val(MaterialX::Vector3(1.0f, 0.0f, 0.0f));
  NodeItem y_axis = val(MaterialX::Vector3(0.0f, 1.0f, 0.0f));
  NodeItem z_axis = val(MaterialX::Vector3(0.0f, 0.0f, 1.0f));

  if (invert) {
    return rotate(z, z_axis).rotate(y, y_axis).rotate(x, x_axis);
  }
  return rotate(x, x_axis).rotate(y, y_axis).rotate(z, z_axis);
}

NodeItem NodeItem::sin() const
{
  return to_vector().arithmetic("sin", [](float a) { return std::sin(a); });
}

NodeItem NodeItem::cos() const
{
  return to_vector().arithmetic("cos", [](float a) { return std::cos(a); });
}

NodeItem NodeItem::tan() const
{
  return to_vector().arithmetic("tan", [](float a) { return std::tan(a); });
}

NodeItem NodeItem::asin() const
{
  return to_vector().arithmetic("asin", [](float a) { return std::asin(a); });
}

NodeItem NodeItem::acos() const
{
  return to_vector().arithmetic("acos", [](float a) { return std::acos(a); });
}

NodeItem NodeItem::atan() const
{
  return to_vector().arithmetic("atan", [](float a) { return std::atan(a); });
}

NodeItem NodeItem::atan2(const NodeItem &other) const
{
  return to_vector().arithmetic(other, "atan2", [](float a, float b) { return std::atan2(a, b); });
}

NodeItem NodeItem::sinh() const
{
  NodeItem v = to_vector();
  return (v.exp() - (-v).exp()) / val(2.0f);
}

NodeItem NodeItem::cosh() const
{
  NodeItem v = to_vector();
  return (v.exp() + (-v).exp()) / val(2.0f);
}

NodeItem NodeItem::tanh() const
{
  NodeItem v = to_vector();
  NodeItem a = v.exp();
  NodeItem b = (-v).exp();
  return (a - b) / (a + b);
}

NodeItem NodeItem::ln() const
{
  return to_vector().arithmetic("ln", [](float a) { return std::log(a); });
}

NodeItem NodeItem::sqrt() const
{
  return to_vector().arithmetic("sqrt", [](float a) { return std::sqrt(a); });
}

NodeItem NodeItem::sign() const
{
  return arithmetic("sign", [](float a) { return a < 0.0f ? -1.0f : (a == 0.0f ? 0.0f : 1.0f); });
}

NodeItem NodeItem::exp() const
{
  return to_vector().arithmetic("exp", [](float a) { return std::exp(a); });
}

NodeItem NodeItem::convert(Type to_type) const
{
  Type from_type = type();
  if (from_type == Type::Empty || from_type == to_type || to_type == Type::Any) {
    return *this;
  }
  if (!is_arithmetic(from_type) || !is_arithmetic(to_type)) {
    CLOG_WARN(LOG_MATERIALX_SHADER,
              "Cannot convert: %s -> %s",
              type(from_type).c_str(),
              type(to_type).c_str());
    return empty();
  }

  if (to_type == Type::Float) {
    return (*this)[0];
  }

  /* Converting types which requires > 1 iteration */
  switch (from_type) {
    case Type::Vector2:
      switch (to_type) {
        case Type::Vector4:
          return convert(Type::Vector3).convert(Type::Vector4);
        case Type::Color3:
          return convert(Type::Vector3).convert(Type::Color3);
        case Type::Color4:
          return convert(Type::Vector3).convert(Type::Color3).convert(Type::Color4);
        default:
          break;
      }
      break;
    case Type::Vector3:
      switch (to_type) {
        case Type::Color4:
          return convert(Type::Color3).convert(Type::Color4);
        default:
          break;
      }
      break;
    case Type::Vector4:
      switch (to_type) {
        case Type::Vector2:
          return convert(Type::Vector3).convert(Type::Vector2);
        case Type::Color3:
          return convert(Type::Vector3).convert(Type::Color3);
        default:
          break;
      }
      break;
    case Type::Color3:
      switch (to_type) {
        case Type::Vector2:
          return convert(Type::Vector3).convert(Type::Vector2);
        case Type::Vector4:
          return convert(Type::Vector3).convert(Type::Vector4);
        default:
          break;
      }
      break;
    case Type::Color4:
      switch (to_type) {
        case Type::Vector2:
          return convert(Type::Vector4).convert(Type::Vector3).convert(Type::Vector2);
        case Type::Vector3:
          return convert(Type::Vector4).convert(Type::Vector3);
        default:
          break;
      }
      break;
    default:
      break;
  }

  /* Converting 1 iteration types */
  NodeItem res = empty();
  if (value) {
    switch (from_type) {
      case Type::Float: {
        float v = value->asA<float>();
        switch (to_type) {
          case Type::Vector2:
            res.value = MaterialX::Value::createValue<MaterialX::Vector2>({v, v});
            break;
          case Type::Vector3:
            res.value = MaterialX::Value::createValue<MaterialX::Vector3>({v, v, v});
            break;
          case Type::Vector4:
            res.value = MaterialX::Value::createValue<MaterialX::Vector4>({v, v, v, 1.0f});
            break;
          case Type::Color3:
            res.value = MaterialX::Value::createValue<MaterialX::Color3>({v, v, v});
            break;
          case Type::Color4:
            res.value = MaterialX::Value::createValue<MaterialX::Color4>({v, v, v, 1.0f});
            break;
          default:
            BLI_assert_unreachable();
        }
        break;
      }
      case Type::Vector2: {
        auto v = value->asA<MaterialX::Vector2>();
        switch (to_type) {
          case Type::Vector3:
            res.value = MaterialX::Value::createValue<MaterialX::Vector3>({v[0], v[1], 0.0f});
            break;
          default:
            BLI_assert_unreachable();
        }
        break;
      }
      case Type::Vector3: {
        auto v = value->asA<MaterialX::Vector3>();
        switch (to_type) {
          case Type::Vector2:
            res.value = MaterialX::Value::createValue<MaterialX::Vector2>({v[0], v[1]});
            break;
          case Type::Vector4:
            res.value = MaterialX::Value::createValue<MaterialX::Vector4>(
                {v[0], v[1], v[2], 0.0f});
            break;
          case Type::Color3:
            res.value = MaterialX::Value::createValue<MaterialX::Color3>({v[0], v[1], v[2]});
            break;
          default:
            BLI_assert_unreachable();
        }
        break;
      }
      case Type::Vector4: {
        auto v = value->asA<MaterialX::Vector4>();
        switch (to_type) {
          case Type::Vector3:
            res.value = MaterialX::Value::createValue<MaterialX::Vector3>({v[0], v[1], v[2]});
            break;
          case Type::Color4:
            res.value = MaterialX::Value::createValue<MaterialX::Color4>({v[0], v[1], v[2], v[3]});
            break;
          default:
            BLI_assert_unreachable();
        }
        break;
      }
      case Type::Color3: {
        auto v = value->asA<MaterialX::Color3>();
        switch (to_type) {
          case Type::Vector3:
            res.value = MaterialX::Value::createValue<MaterialX::Vector3>({v[0], v[1], v[2]});
            break;
          case Type::Color4:
            res.value = MaterialX::Value::createValue<MaterialX::Color4>({v[0], v[1], v[2], 1.0f});
            break;
          default:
            BLI_assert_unreachable();
        }
        break;
      }
      case Type::Color4: {
        auto v = value->asA<MaterialX::Color4>();
        switch (to_type) {
          case Type::Vector4:
            res.value = MaterialX::Value::createValue<MaterialX::Vector4>(
                {v[0], v[1], v[2], v[3]});
            break;
          case Type::Color3:
            res.value = MaterialX::Value::createValue<MaterialX::Color3>({v[0], v[1], v[2]});
            break;
          default:
            BLI_assert_unreachable();
        }
        break;
      }
      default:
        BLI_assert_unreachable();
    }
  }
  else {
    res = create_node("convert", to_type, {{"in", *this}});
  }
  return res;
}

NodeItem NodeItem::to_vector() const
{
  switch (type()) {
    case Type::Float:
    case Type::Vector2:
    case Type::Vector3:
    case Type::Vector4:
      return *this;

    case Type::Color3:
      return convert(Type::Vector3);

    case Type::Color4:
      return convert(Type::Vector4);

    default:
      BLI_assert_unreachable();
  }
  return empty();
}

NodeItem NodeItem::if_else(CompareOp op,
                           const NodeItem &other,
                           const NodeItem &if_val,
                           const NodeItem &else_val) const
{
  switch (op) {
    case CompareOp::Less:
      return if_else(CompareOp::GreaterEq, other, else_val, if_val);
    case CompareOp::LessEq:
      return if_else(CompareOp::Greater, other, else_val, if_val);
    case CompareOp::NotEq:
      return if_else(CompareOp::Eq, other, else_val, if_val);
    default:
      break;
  }

  NodeItem res = empty();
  if (type() != Type::Float || other.type() != Type::Float) {
    return res;
  }

  auto item1 = if_val;
  auto item2 = else_val;
  Type to_type = cast_types(item1, item2);
  if (to_type == Type::Empty) {
    return res;
  }

  std::function<bool(float, float)> func = nullptr;
  std::string category;
  switch (op) {
    case CompareOp::Greater:
      category = "ifgreater";
      func = [](float a, float b) { return a > b; };
      break;
    case CompareOp::GreaterEq:
      category = "ifgreatereq";
      func = [](float a, float b) { return a >= b; };
      break;
    case CompareOp::Eq:
      category = "ifequal";
      func = [](float a, float b) { return a == b; };
      break;
    default:
      BLI_assert_unreachable();
  }

  if (value && other.value) {
    res = func(value->asA<float>(), other.value->asA<float>()) ? item1 : item2;
  }
  else {
    res = create_node(
        category, to_type, {{"value1", *this}, {"value2", other}, {"in1", item1}, {"in2", item2}});
  }

  return res;
}

NodeItem NodeItem::empty() const
{
  return NodeItem(graph_);
}

NodeItem::Type NodeItem::type() const
{
  if (value) {
    return type(value->getTypeString());
  }
  if (node) {
    return type(node->getType());
  }
  if (output) {
    return type(output->getType());
  }
  return Type::Empty;
}

NodeItem NodeItem::create_node(const std::string &category, Type type) const
{
  std::string type_str = this->type(type);
  CLOG_INFO(LOG_MATERIALX_SHADER, 2, "<%s type=%s>", category.c_str(), type_str.c_str());
  NodeItem res = empty();
  res.node = graph_->addNode(category, MaterialX::EMPTY_STRING, type_str);
  return res;
}

NodeItem NodeItem::create_node(const std::string &category, Type type, const Inputs &inputs) const
{
  NodeItem res = create_node(category, type);
  for (auto &it : inputs) {
    if (it.second) {
      res.set_input(it.first, it.second);
    }
  }
  return res;
}

void NodeItem::set_input(const std::string &in_name, const NodeItem &item)
{
  if (item.value) {
    Type item_type = item.type();
    switch (item_type) {
      case Type::String:
        set_input(in_name, item.value->asA<std::string>(), item_type);
        break;
      case Type::Boolean:
        set_input(in_name, item.value->asA<bool>(), item_type);
        break;
      case Type::Integer:
        set_input(in_name, item.value->asA<int>(), item_type);
        break;
      case Type::Float:
        set_input(in_name, item.value->asA<float>(), item_type);
        break;
      case Type::Vector2:
        set_input(in_name, item.value->asA<MaterialX::Vector2>(), item_type);
        break;
      case Type::Vector3:
        set_input(in_name, item.value->asA<MaterialX::Vector3>(), item_type);
        break;
      case Type::Vector4:
        set_input(in_name, item.value->asA<MaterialX::Vector4>(), item_type);
        break;
      case Type::Color3:
        set_input(in_name, item.value->asA<MaterialX::Color3>(), item_type);
        break;
      case Type::Color4:
        set_input(in_name, item.value->asA<MaterialX::Color4>(), item_type);
        break;
      default:
        BLI_assert_unreachable();
    }
  }
  else if (item.node) {
    node->setConnectedNode(in_name, item.node);
  }
  else if (item.input) {
    node->setAttribute("interfacename", item.input->getName());
  }
  else if (item.output) {
    node->setConnectedOutput(in_name, item.output);
  }
  else {
    CLOG_WARN(LOG_MATERIALX_SHADER, "Empty item to input: %s", in_name.c_str());
  }
}

NodeItem NodeItem::add_output(const std::string &out_name, Type out_type)
{
  NodeItem res = empty();
  res.output = node->addOutput(out_name, type(out_type));
  return res;
}

NodeItem NodeItem::create_input(const std::string &name, const NodeItem &item) const
{
  NodeItem res = empty();
  res.input = graph_->addInput(name);

  Type item_type = item.type();
  if (item.node) {
    res.input->setConnectedNode(item.node);
  }
  else {
    BLI_assert_unreachable();
  }
  res.input->setType(type(item_type));

  return res;
}

NodeItem NodeItem::create_output(const std::string &name, const NodeItem &item) const
{
  NodeItem res = empty();
  res.output = graph_->addOutput(name);

  Type item_type = item.type();
  if (item.node) {
    res.output->setConnectedNode(item.node);
  }
  else if (item.input) {
    res.output->setInterfaceName(item.input->getName());
  }
  else {
    BLI_assert_unreachable();
  }
  res.output->setType(type(item_type));

  return res;
}

NodeItem::Type NodeItem::cast_types(NodeItem &item1, NodeItem &item2)
{
  Type t1 = item1.type();
  Type t2 = item2.type();
  if (t1 == t2) {
    return t1;
  }
  if (!is_arithmetic(t1) || !is_arithmetic(t2)) {
    CLOG_WARN(
        LOG_MATERIALX_SHADER, "Can't adjust types: %s <-> %s", type(t1).c_str(), type(t2).c_str());
    return Type::Empty;
  }
  if (t1 < t2) {
    item1 = item1.convert(t2);
    return t2;
  }
  else {
    item2 = item2.convert(t1);
    return t1;
  }
}

bool NodeItem::is_arithmetic() const
{
  return is_arithmetic(type());
}

NodeItem NodeItem::arithmetic(const std::string &category, std::function<float(float)> func) const
{
  NodeItem res = empty();
  Type type = this->type();
  BLI_assert(is_arithmetic(type));

  if (value) {
    switch (type) {
      case Type::Float: {
        float v = value->asA<float>();
        res.value = MaterialX::Value::createValue<float>(func(v));
        break;
      }
      case Type::Color3: {
        auto v = value->asA<MaterialX::Color3>();
        res.value = MaterialX::Value::createValue<MaterialX::Color3>(
            {func(v[0]), func(v[1]), func(v[2])});
        break;
      }
      case Type::Color4: {
        auto v = value->asA<MaterialX::Color4>();
        res.value = MaterialX::Value::createValue<MaterialX::Color4>(
            {func(v[0]), func(v[1]), func(v[2]), func(v[3])});
        break;
      }
      case Type::Vector2: {
        auto v = value->asA<MaterialX::Vector2>();
        res.value = MaterialX::Value::createValue<MaterialX::Vector2>({func(v[0]), func(v[1])});
        break;
      }
      case Type::Vector3: {
        auto v = value->asA<MaterialX::Vector3>();
        res.value = MaterialX::Value::createValue<MaterialX::Vector3>(
            {func(v[0]), func(v[1]), func(v[2])});
        break;
      }
      case Type::Vector4: {
        auto v = value->asA<MaterialX::Vector4>();
        res.value = MaterialX::Value::createValue<MaterialX::Vector4>(
            {func(v[0]), func(v[1]), func(v[2]), func(v[3])});
        break;
      }
      default:
        BLI_assert_unreachable();
    }
  }
  else {
    res = create_node(category, type, {{"in", *this}});
  }
  return res;
}

NodeItem NodeItem::arithmetic(const NodeItem &other,
                              const std::string &category,
                              std::function<float(float, float)> func,
                              Type to_type) const
{
  NodeItem res = empty();
  NodeItem item1 = *this;
  NodeItem item2 = other;
  to_type = (to_type == Type::Any) ? cast_types(item1, item2) : to_type;
  if (to_type == Type::Empty) {
    return res;
  }

  if (value && other.value) {
    switch (to_type) {
      case Type::Float: {
        float v1 = item1.value->asA<float>();
        float v2 = item2.value->asA<float>();
        res.value = MaterialX::Value::createValue<float>(func(v1, v2));
        break;
      }
      case Type::Color3: {
        auto v1 = item1.value->asA<MaterialX::Color3>();
        auto v2 = item2.value->asA<MaterialX::Color3>();
        res.value = MaterialX::Value::createValue<MaterialX::Color3>(
            {func(v1[0], v2[0]), func(v1[1], v2[1]), func(v1[2], v2[2])});
        break;
      }
      case Type::Color4: {
        auto v1 = item1.value->asA<MaterialX::Color4>();
        auto v2 = item2.value->asA<MaterialX::Color4>();
        res.value = MaterialX::Value::createValue<MaterialX::Color4>(
            {func(v1[0], v2[0]), func(v1[1], v2[1]), func(v1[2], v2[2]), func(v1[3], v2[3])});
        break;
      }
      case Type::Vector2: {
        auto v1 = item1.value->asA<MaterialX::Vector2>();
        auto v2 = item2.value->asA<MaterialX::Vector2>();
        res.value = MaterialX::Value::createValue<MaterialX::Vector2>(
            {func(v1[0], v2[0]), func(v1[1], v2[1])});
        break;
      }
      case Type::Vector3: {
        auto v1 = item1.value->asA<MaterialX::Vector3>();
        auto v2 = item2.value->asA<MaterialX::Vector3>();
        res.value = MaterialX::Value::createValue<MaterialX::Vector3>(
            {func(v1[0], v2[0]), func(v1[1], v2[1]), func(v1[2], v2[2])});
        break;
      }
      case Type::Vector4: {
        auto v1 = item1.value->asA<MaterialX::Vector4>();
        auto v2 = item2.value->asA<MaterialX::Vector4>();
        res.value = MaterialX::Value::createValue<MaterialX::Vector4>(
            {func(v1[0], v2[0]), func(v1[1], v2[1]), func(v1[2], v2[2]), func(v1[3], v2[3])});
        break;
      }
      default:
        BLI_assert_unreachable();
    }
  }
  else {
    res = create_node(category, to_type, {{"in1", item1}, {"in2", item2}});
  }
  return res;
}

}  // namespace blender::nodes::materialx
