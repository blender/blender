/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include "NOD_node_declaration.hh"

#include "RNA_types.h"

#include "BLI_color.hh"
#include "BLI_float3.hh"

namespace blender::nodes::decl {

class FloatBuilder;

class Float : public SocketDeclaration {
 private:
  float default_value_ = 0.0f;
  float soft_min_value_ = -FLT_MAX;
  float soft_max_value_ = FLT_MAX;
  PropertySubType subtype_ = PROP_NONE;

  friend FloatBuilder;

 public:
  using Builder = FloatBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
};

class FloatBuilder : public SocketDeclarationBuilder<Float> {
 public:
  FloatBuilder &min(const float value)
  {
    decl_->soft_min_value_ = value;
    return *this;
  }

  FloatBuilder &max(const float value)
  {
    decl_->soft_max_value_ = value;
    return *this;
  }

  FloatBuilder &default_value(const float value)
  {
    decl_->default_value_ = value;
    return *this;
  }

  FloatBuilder &subtype(PropertySubType subtype)
  {
    decl_->subtype_ = subtype;
    return *this;
  }
};

class IntBuilder;

class Int : public SocketDeclaration {
 private:
  int default_value_ = 0;
  int soft_min_value_ = INT32_MIN;
  int soft_max_value_ = INT32_MAX;
  PropertySubType subtype_ = PROP_NONE;

  friend IntBuilder;

 public:
  using Builder = IntBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
};

class IntBuilder : public SocketDeclarationBuilder<Int> {
 public:
  IntBuilder &min(const int value)
  {
    decl_->soft_min_value_ = value;
    return *this;
  }

  IntBuilder &max(const int value)
  {
    decl_->soft_max_value_ = value;
    return *this;
  }

  IntBuilder &default_value(const int value)
  {
    decl_->default_value_ = value;
    return *this;
  }

  IntBuilder &subtype(PropertySubType subtype)
  {
    decl_->subtype_ = subtype;
    return *this;
  }
};

class VectorBuilder;

class Vector : public SocketDeclaration {
 private:
  float3 default_value_ = {0, 0, 0};
  float soft_min_value_ = -FLT_MAX;
  float soft_max_value_ = FLT_MAX;
  PropertySubType subtype_ = PROP_NONE;

  friend VectorBuilder;

 public:
  using Builder = VectorBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
};

class VectorBuilder : public SocketDeclarationBuilder<Vector> {
 public:
  VectorBuilder &default_value(const float3 value)
  {
    decl_->default_value_ = value;
    return *this;
  }

  VectorBuilder &subtype(PropertySubType subtype)
  {
    decl_->subtype_ = subtype;
    return *this;
  }

  VectorBuilder &min(const float min)
  {
    decl_->soft_min_value_ = min;
    return *this;
  }

  VectorBuilder &max(const float max)
  {
    decl_->soft_max_value_ = max;
    return *this;
  }
};

class BoolBuilder;

class Bool : public SocketDeclaration {
 private:
  bool default_value_ = false;
  friend BoolBuilder;

 public:
  using Builder = BoolBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
};

class BoolBuilder : public SocketDeclarationBuilder<Bool> {
 public:
  BoolBuilder &default_value(const bool value)
  {
    decl_->default_value_ = value;
    return *this;
  }
};

class ColorBuilder;

class Color : public SocketDeclaration {
 private:
  ColorGeometry4f default_value_;

  friend ColorBuilder;

 public:
  using Builder = ColorBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
};

class ColorBuilder : public SocketDeclarationBuilder<Color> {
 public:
  ColorBuilder &default_value(const ColorGeometry4f value)
  {
    decl_->default_value_ = value;
    return *this;
  }
};

class String : public SocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<String>;

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
};

class IDSocketDeclaration : public SocketDeclaration {
 private:
  const char *idname_;

 public:
  IDSocketDeclaration(const char *idname) : idname_(idname)
  {
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
};

class Object : public IDSocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<Object>;

  Object() : IDSocketDeclaration("NodeSocketObject")
  {
  }
};

class Material : public IDSocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<Material>;

  Material() : IDSocketDeclaration("NodeSocketMaterial")
  {
  }
};

class Collection : public IDSocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<Collection>;

  Collection() : IDSocketDeclaration("NodeSocketCollection")
  {
  }
};

class Texture : public IDSocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<Texture>;

  Texture() : IDSocketDeclaration("NodeSocketTexture")
  {
  }
};

class Geometry : public SocketDeclaration {
 public:
  using Builder = SocketDeclarationBuilder<Geometry>;

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
};

}  // namespace blender::nodes::decl
