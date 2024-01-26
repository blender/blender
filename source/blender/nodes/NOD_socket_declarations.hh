/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "NOD_node_declaration.hh"

#include "RNA_types.hh"

#include "BLI_color.hh"
#include "BLI_math_euler_types.hh"
#include "BLI_math_vector_types.hh"

namespace blender::nodes::decl {

class FloatBuilder;

class Float : public SocketDeclaration {
 public:
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
  float3 default_value = {0, 0, 0};
  float soft_min_value = -FLT_MAX;
  float soft_max_value = FLT_MAX;
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
  VectorBuilder &default_value(const float3 value);
  VectorBuilder &subtype(PropertySubType subtype);
  VectorBuilder &min(float min);
  VectorBuilder &max(float max);
  VectorBuilder &compact();
};

class BoolBuilder;

class Bool : public SocketDeclaration {
 public:
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

class StringBuilder;

class String : public SocketDeclaration {
 public:
  std::string default_value;

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
};

class MenuBuilder;

class Menu : public SocketDeclaration {
 public:
  int32_t default_value;

  friend MenuBuilder;

  using Builder = MenuBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class MenuBuilder : public SocketDeclarationBuilder<Menu> {
 public:
  MenuBuilder &default_value(int32_t value);
};

class IDSocketDeclaration : public SocketDeclaration {
 public:
  const char *idname;
  /**
   * Get the default ID pointer for this socket. This is a function to avoid dangling pointers,
   * since bNode::id pointers are remapped as ID pointers change, but pointers in socket
   * declarations are not managed the same way.
   */
  std::function<ID *(const bNode &node)> default_value_fn;

 public:
  IDSocketDeclaration(const char *idname);

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class Object : public IDSocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<Object>;

  Object();
};

class Material : public IDSocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<Material>;

  Material();
};

class Collection : public IDSocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<Collection>;

  Collection();
};

class Texture : public IDSocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<Texture>;

  Texture();
};

class Image : public IDSocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<Image>;

  Image();
};

class ShaderBuilder;

class Shader : public SocketDeclaration {
 public:
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
  using Builder = ExtendBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

class ExtendBuilder : public SocketDeclarationBuilder<Extend> {};

class Custom : public SocketDeclaration {
 public:
  const char *idname_;
  std::function<void(bNode &node, bNodeSocket &socket, const char *data_path)> init_socket_fn;

  bNodeSocket &build(bNodeTree &ntree, bNode &node) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
  bool can_connect(const bNodeSocket &socket) const override;
};

/* -------------------------------------------------------------------- */
/** \name #FloatBuilder Inline Methods
 * \{ */

inline FloatBuilder &FloatBuilder::min(const float value)
{
  if (decl_in_) {
    decl_in_->soft_min_value = value;
  }
  if (decl_out_) {
    decl_out_->soft_min_value = value;
  }
  return *this;
}

inline FloatBuilder &FloatBuilder::max(const float value)
{
  if (decl_in_) {
    decl_in_->soft_max_value = value;
  }
  if (decl_out_) {
    decl_out_->soft_max_value = value;
  }
  return *this;
}

inline FloatBuilder &FloatBuilder::default_value(const float value)
{
  if (decl_in_) {
    decl_in_->default_value = value;
  }
  if (decl_out_) {
    decl_out_->default_value = value;
  }
  return *this;
}

inline FloatBuilder &FloatBuilder::subtype(PropertySubType subtype)
{
  if (decl_in_) {
    decl_in_->subtype = subtype;
  }
  if (decl_out_) {
    decl_out_->subtype = subtype;
  }
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #IntBuilder Inline Methods
 * \{ */

inline IntBuilder &IntBuilder::min(const int value)
{
  if (decl_in_) {
    decl_in_->soft_min_value = value;
  }
  if (decl_out_) {
    decl_out_->soft_min_value = value;
  }
  return *this;
}

inline IntBuilder &IntBuilder::max(const int value)
{
  if (decl_in_) {
    decl_in_->soft_max_value = value;
  }
  if (decl_out_) {
    decl_out_->soft_max_value = value;
  }
  return *this;
}

inline IntBuilder &IntBuilder::default_value(const int value)
{
  if (decl_in_) {
    decl_in_->default_value = value;
  }
  if (decl_out_) {
    decl_out_->default_value = value;
  }
  return *this;
}

inline IntBuilder &IntBuilder::subtype(PropertySubType subtype)
{
  if (decl_in_) {
    decl_in_->subtype = subtype;
  }
  if (decl_out_) {
    decl_out_->subtype = subtype;
  }
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #VectorBuilder Inline Methods
 * \{ */

inline VectorBuilder &VectorBuilder::default_value(const float3 value)
{
  if (decl_in_) {
    decl_in_->default_value = value;
  }
  if (decl_out_) {
    decl_out_->default_value = value;
  }
  return *this;
}

inline VectorBuilder &VectorBuilder::subtype(PropertySubType subtype)
{
  if (decl_in_) {
    decl_in_->subtype = subtype;
  }
  if (decl_out_) {
    decl_out_->subtype = subtype;
  }
  return *this;
}

inline VectorBuilder &VectorBuilder::min(const float min)
{
  if (decl_in_) {
    decl_in_->soft_min_value = min;
  }
  if (decl_out_) {
    decl_out_->soft_min_value = min;
  }
  return *this;
}

inline VectorBuilder &VectorBuilder::max(const float max)
{
  if (decl_in_) {
    decl_in_->soft_max_value = max;
  }
  if (decl_out_) {
    decl_out_->soft_max_value = max;
  }
  return *this;
}

inline VectorBuilder &VectorBuilder::compact()
{
  if (decl_in_) {
    decl_in_->compact = true;
  }
  if (decl_out_) {
    decl_out_->compact = true;
  }
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #BoolBuilder Inline Methods
 * \{ */

inline BoolBuilder &BoolBuilder::default_value(const bool value)
{
  if (decl_in_) {
    decl_in_->default_value = value;
  }
  if (decl_out_) {
    decl_out_->default_value = value;
  }
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #ColorBuilder Inline Methods
 * \{ */

inline ColorBuilder &ColorBuilder::default_value(const ColorGeometry4f value)
{
  if (decl_in_) {
    decl_in_->default_value = value;
  }
  if (decl_out_) {
    decl_out_->default_value = value;
  }
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #StringBuilder Inline Methods
 * \{ */

inline StringBuilder &StringBuilder::default_value(std::string value)
{
  if (decl_in_) {
    decl_in_->default_value = std::move(value);
  }
  if (decl_out_) {
    decl_out_->default_value = std::move(value);
  }
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MenuBuilder Inline Methods
 * \{ */

inline MenuBuilder &MenuBuilder::default_value(const int32_t value)
{
  if (decl_in_) {
    decl_in_->default_value = value;
  }
  if (decl_out_) {
    decl_out_->default_value = value;
  }
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #RotationBuilder Inline Methods
 * \{ */

inline RotationBuilder &RotationBuilder::default_value(const math::EulerXYZ &value)
{
  if (decl_in_) {
    decl_in_->default_value = value;
  }
  if (decl_out_) {
    decl_out_->default_value = value;
  }
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

SocketDeclarationPtr create_extend_declaration(const eNodeSocketInOut in_out);

}  // namespace blender::nodes::decl
