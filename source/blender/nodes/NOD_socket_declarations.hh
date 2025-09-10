/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cfloat>

#include "NOD_menu_value.hh"
#include "NOD_node_declaration.hh"

#include "RNA_types.hh"

#include "BKE_node_enum.hh"

#include "BLI_color_types.hh"
#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_math_euler_types.hh"
#include "BLI_math_vector_types.hh"

namespace blender::nodes::decl {

class FloatBuilder;

class Float : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_FLOAT;

  float default_value = 0.0f;
  float soft_min_value = -FLT_MAX;
  float soft_max_value = FLT_MAX;
  PropertySubType subtype = PROP_NONE;

  friend FloatBuilder;

  using Builder = FloatBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class FloatBuilder : public SocketDeclarationBuilder<Float> {
 public:
  FloatBuilder &min(float value);
  FloatBuilder &max(float value);
  FloatBuilder &default_value(float value);
  FloatBuilder &subtype(PropertySubType subtype);
};

class IntBuilder;

class Int : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_INT;

  int default_value = 0;
  int soft_min_value = INT32_MIN;
  int soft_max_value = INT32_MAX;
  PropertySubType subtype = PROP_NONE;

  friend IntBuilder;

  using Builder = IntBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class IntBuilder : public SocketDeclarationBuilder<Int> {
 public:
  IntBuilder &min(int value);
  IntBuilder &max(int value);
  IntBuilder &default_value(int value);
  IntBuilder &subtype(PropertySubType subtype);
};

class VectorBuilder;

class Vector : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_VECTOR;

  float4 default_value = {0, 0, 0, 0};
  float soft_min_value = -FLT_MAX;
  float soft_max_value = FLT_MAX;
  int dimensions = 3;
  PropertySubType subtype = PROP_NONE;

  friend VectorBuilder;

  using Builder = VectorBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class VectorBuilder : public SocketDeclarationBuilder<Vector> {
 public:
  VectorBuilder &default_value(const float2 value);
  VectorBuilder &default_value(const float3 value);
  VectorBuilder &default_value(const float4 value);
  VectorBuilder &subtype(PropertySubType subtype);
  VectorBuilder &dimensions(int dimensions);
  VectorBuilder &min(float min);
  VectorBuilder &max(float max);
  VectorBuilder &compact();
};

class BoolBuilder;

class Bool : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_BOOLEAN;

  bool default_value = false;
  friend BoolBuilder;

  using Builder = BoolBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class BoolBuilder : public SocketDeclarationBuilder<Bool> {
 public:
  BoolBuilder &default_value(bool value);
};

class ColorBuilder;

class Color : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_RGBA;

  ColorGeometry4f default_value{0.8f, 0.8f, 0.8f, 1.0f};

  friend ColorBuilder;

  using Builder = ColorBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class ColorBuilder : public SocketDeclarationBuilder<Color> {
 public:
  ColorBuilder &default_value(const ColorGeometry4f value);
};

class RotationBuilder;

class Rotation : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_ROTATION;

  math::EulerXYZ default_value;

  friend RotationBuilder;

  using Builder = RotationBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class RotationBuilder : public SocketDeclarationBuilder<Rotation> {
 public:
  RotationBuilder &default_value(const math::EulerXYZ &value);
};

class MatrixBuilder;

class Matrix : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_MATRIX;

  friend MatrixBuilder;

  using Builder = MatrixBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class MatrixBuilder : public SocketDeclarationBuilder<Matrix> {};

class StringBuilder;

class String : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_STRING;

  std::string default_value;
  PropertySubType subtype = PROP_NONE;
  std::optional<std::string> path_filter;

  friend StringBuilder;

  using Builder = StringBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class StringBuilder : public SocketDeclarationBuilder<String> {
 public:
  StringBuilder &default_value(const std::string value);
  StringBuilder &subtype(PropertySubType subtype);
  StringBuilder &path_filter(std::optional<std::string> filter);
};

class MenuBuilder;

class Menu : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_MENU;

  MenuValue default_value;
  bool is_expanded = false;
  ImplicitSharingPtr<bke::RuntimeNodeEnumItems> items;

  friend MenuBuilder;

  using Builder = MenuBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class MenuBuilder : public SocketDeclarationBuilder<Menu> {
 public:
  MenuBuilder &default_value(MenuValue value);

  /** Draw the menu items next to each other instead of as a drop-down menu. */
  MenuBuilder &expanded(bool value = true);

  /** Set the available items in the menu. The items array must have static lifetime. */
  MenuBuilder &static_items(const EnumPropertyItem *items);
};

class BundleBuilder;

class Bundle : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_BUNDLE;
  /**
   * Index of a corresponding input socket. If set, the output is assumed to have the same bundle
   * structure as the input.
   */
  std::optional<int> pass_through_input_index;

  friend BundleBuilder;

  using Builder = BundleBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class BundleBuilder : public SocketDeclarationBuilder<Bundle> {
 public:
  /** On output sockets, indicate that the bundle structure is passed through from an input. */
  BundleBuilder &pass_through_input_index(std::optional<int> index);
};

class ClosureBuilder;

class Closure : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_CLOSURE;

  friend ClosureBuilder;

  using Builder = ClosureBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class ClosureBuilder : public SocketDeclarationBuilder<Closure> {};

class IDSocketDeclaration : public SocketDeclaration {
 public:
  const char *idname;
  /**
   * Get the default ID pointer for this socket. This is a function to avoid dangling pointers,
   * since bNode::id pointers are remapped as ID pointers change, but pointers in socket
   * declarations are not managed the same way.
   */
  std::function<ID *(const bNode &node)> default_value_fn;

  IDSocketDeclaration(const char *idname);

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

template<typename T> class IDSocketDeclarationBuilder : public SocketDeclarationBuilder<T> {
 public:
  IDSocketDeclarationBuilder &default_value_fn(std::function<ID *(const bNode &node)> fn)
  {
    this->decl_->default_value_fn = std::move(fn);
    return *this;
  }
};

class Object : public IDSocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_OBJECT;

  using Builder = IDSocketDeclarationBuilder<Object>;

  Object();
};

class Material : public IDSocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_MATERIAL;

  using Builder = IDSocketDeclarationBuilder<Material>;

  Material();
};

class Collection : public IDSocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_COLLECTION;

  using Builder = IDSocketDeclarationBuilder<Collection>;

  Collection();
};

class Texture : public IDSocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_TEXTURE;

  using Builder = IDSocketDeclarationBuilder<Texture>;

  Texture();
};

class Image : public IDSocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_IMAGE;

  using Builder = IDSocketDeclarationBuilder<Image>;

  Image();
};

class ShaderBuilder;

class Shader : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_SHADER;

  friend ShaderBuilder;

  using Builder = ShaderBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class ShaderBuilder : public SocketDeclarationBuilder<Shader> {};

class ExtendBuilder;

class Extend : public SocketDeclaration {
 private:
  friend ExtendBuilder;

 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_CUSTOM;

  using Builder = ExtendBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class ExtendBuilder : public SocketDeclarationBuilder<Extend> {};

class CustomTypeBuilder;

class Custom : public SocketDeclaration {
 public:
  static constexpr eNodeSocketDatatype static_socket_type = SOCK_CUSTOM;

  friend CustomTypeBuilder;

  using Builder = CustomTypeBuilder;

  const char *idname_;
  std::function<void(bNode &node, bNodeSocket &socket, const char *data_path)> init_socket_fn;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class CustomTypeBuilder : public SocketDeclarationBuilder<Custom> {
 public:
  CustomTypeBuilder &idname(const char *idname);

  CustomTypeBuilder &init_socket_fn(
      std::function<void(bNode &node, bNodeSocket &socket, const char *data_path)> fn)
  {
    decl_->init_socket_fn = std::move(fn);
    return *this;
  }
};

/* -------------------------------------------------------------------- */
/** \name #FloatBuilder Inline Methods
 * \{ */

inline FloatBuilder &FloatBuilder::min(const float value)
{
  decl_->soft_min_value = value;
  return *this;
}

inline FloatBuilder &FloatBuilder::max(const float value)
{
  decl_->soft_max_value = value;
  return *this;
}

inline FloatBuilder &FloatBuilder::default_value(const float value)
{
  decl_->default_value = value;
  return *this;
}

inline FloatBuilder &FloatBuilder::subtype(PropertySubType subtype)
{
  decl_->subtype = subtype;
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #IntBuilder Inline Methods
 * \{ */

inline IntBuilder &IntBuilder::min(const int value)
{
  decl_->soft_min_value = value;
  return *this;
}

inline IntBuilder &IntBuilder::max(const int value)
{
  decl_->soft_max_value = value;
  return *this;
}

inline IntBuilder &IntBuilder::default_value(const int value)
{
  decl_->default_value = value;
  return *this;
}

inline IntBuilder &IntBuilder::subtype(PropertySubType subtype)
{
  decl_->subtype = subtype;
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #VectorBuilder Inline Methods
 * \{ */

inline VectorBuilder &VectorBuilder::default_value(const float2 value)
{
  decl_->default_value = float4(value, 0.0f, 0.0f);
  return *this;
}

inline VectorBuilder &VectorBuilder::default_value(const float3 value)
{
  decl_->default_value = float4(value, 0.0f);
  return *this;
}

inline VectorBuilder &VectorBuilder::default_value(const float4 value)
{
  decl_->default_value = value;
  return *this;
}

inline VectorBuilder &VectorBuilder::subtype(PropertySubType subtype)
{
  decl_->subtype = subtype;
  return *this;
}

inline VectorBuilder &VectorBuilder::dimensions(int dimensions)
{
  BLI_assert(dimensions >= 2 && dimensions <= 4);
  decl_->dimensions = dimensions;
  return *this;
}

inline VectorBuilder &VectorBuilder::min(const float min)
{
  decl_->soft_min_value = min;
  return *this;
}

inline VectorBuilder &VectorBuilder::max(const float max)
{
  decl_->soft_max_value = max;
  return *this;
}

inline VectorBuilder &VectorBuilder::compact()
{
  decl_->compact = true;
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #BoolBuilder Inline Methods
 * \{ */

inline BoolBuilder &BoolBuilder::default_value(const bool value)
{
  decl_->default_value = value;
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #ColorBuilder Inline Methods
 * \{ */

inline ColorBuilder &ColorBuilder::default_value(const ColorGeometry4f value)
{
  decl_->default_value = value;
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #StringBuilder Inline Methods
 * \{ */

inline StringBuilder &StringBuilder::default_value(std::string value)
{
  decl_->default_value = std::move(value);
  return *this;
}

inline StringBuilder &StringBuilder::subtype(PropertySubType subtype)
{
  decl_->subtype = subtype;
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MenuBuilder Inline Methods
 * \{ */

inline MenuBuilder &MenuBuilder::default_value(const MenuValue value)
{
  decl_->default_value = value;
  return *this;
}

inline MenuBuilder &MenuBuilder::expanded(const bool value)
{
  decl_->is_expanded = value;
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #RotationBuilder Inline Methods
 * \{ */

inline RotationBuilder &RotationBuilder::default_value(const math::EulerXYZ &value)
{
  decl_->default_value = value;
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #IDSocketDeclaration and Children Inline Methods
 * \{ */

inline IDSocketDeclaration::IDSocketDeclaration(const char *idname) : idname(idname) {}

inline Object::Object() : IDSocketDeclaration("NodeSocketObject") {}

inline Material::Material() : IDSocketDeclaration("NodeSocketMaterial") {}

inline Collection::Collection() : IDSocketDeclaration("NodeSocketCollection") {}

inline Texture::Texture() : IDSocketDeclaration("NodeSocketTexture") {}

inline Image::Image() : IDSocketDeclaration("NodeSocketImage") {}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #CustomTypeBuilder Inline Methods
 * \{ */

inline CustomTypeBuilder &CustomTypeBuilder::idname(const char *idname)
{
  decl_->idname_ = idname;
  return *this;
}

/** \} */

}  // namespace blender::nodes::decl
