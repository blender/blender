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

class Float : public SocketDeclaration {
 private:
  float default_value_ = 0.0f;
  float soft_min_value_ = -FLT_MAX;
  float soft_max_value_ = FLT_MAX;
  PropertySubType subtype_ = PROP_NONE;

 public:
  Float &min(const float value)
  {
    soft_min_value_ = value;
    return *this;
  }

  Float &max(const float value)
  {
    soft_max_value_ = value;
    return *this;
  }

  Float &default_value(const float value)
  {
    default_value_ = value;
    return *this;
  }

  Float &subtype(PropertySubType subtype)
  {
    subtype_ = subtype;
    return *this;
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
};

class Int : public SocketDeclaration {
 private:
  int default_value_ = 0;
  int soft_min_value_ = INT32_MIN;
  int soft_max_value_ = INT32_MAX;
  PropertySubType subtype_ = PROP_NONE;

 public:
  Int &min(const int value)
  {
    soft_min_value_ = value;
    return *this;
  }

  Int &max(const int value)
  {
    soft_max_value_ = value;
    return *this;
  }

  Int &default_value(const int value)
  {
    default_value_ = value;
    return *this;
  }

  Int &subtype(PropertySubType subtype)
  {
    subtype_ = subtype;
    return *this;
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
};

class Vector : public SocketDeclaration {
 private:
  float3 default_value_ = {0, 0, 0};
  PropertySubType subtype_ = PROP_NONE;

 public:
  Vector &default_value(const float3 value)
  {
    default_value_ = value;
    return *this;
  }

  Vector &subtype(PropertySubType subtype)
  {
    subtype_ = subtype;
    return *this;
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override;
};

class Bool : public SocketDeclaration {
 private:
  bool default_value_ = false;

 public:
  Bool &default_value(const bool value)
  {
    default_value_ = value;
    return *this;
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
};

class Color : public SocketDeclaration {
 private:
  ColorGeometry4f default_value_;

 public:
  Color &default_value(const ColorGeometry4f value)
  {
    default_value_ = value;
    return *this;
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
};

class String : public SocketDeclaration {
 public:
  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
};

namespace detail {
struct CommonIDSocketData {
  const char *idname;
  bool hide_label = false;
};

bNodeSocket &build_id_socket(bNodeTree &ntree,
                             bNode &node,
                             eNodeSocketInOut in_out,
                             const CommonIDSocketData &data,
                             StringRefNull name,
                             StringRefNull identifier);
bool matches_id_socket(const bNodeSocket &socket,
                       const CommonIDSocketData &data,
                       StringRefNull name,
                       StringRefNull identifier);

template<typename Subtype> class IDSocketDeclaration : public SocketDeclaration {
 private:
  CommonIDSocketData data_;

 public:
  IDSocketDeclaration(const char *idname) : data_({idname})
  {
  }

  Subtype &hide_label(bool value)
  {
    data_.hide_label = value;
    return *(Subtype *)this;
  }

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override
  {
    return build_id_socket(ntree, node, in_out, data_, name_, identifier_);
  }

  bool matches(const bNodeSocket &socket) const override
  {
    return matches_id_socket(socket, data_, name_, identifier_);
  }

  bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const override
  {
    if (StringRef(socket.idname) != data_.idname) {
      return this->build(ntree, node, (eNodeSocketInOut)socket.in_out);
    }
    if (data_.hide_label) {
      socket.flag |= SOCK_HIDE_LABEL;
    }
    else {
      socket.flag &= ~SOCK_HIDE_LABEL;
    }
    return socket;
  }
};
}  // namespace detail

class Object : public detail::IDSocketDeclaration<Object> {
 public:
  Object() : detail::IDSocketDeclaration<Object>("NodeSocketObject")
  {
  }
};

class Material : public detail::IDSocketDeclaration<Material> {
 public:
  Material() : detail::IDSocketDeclaration<Material>("NodeSocketMaterial")
  {
  }
};

class Collection : public detail::IDSocketDeclaration<Collection> {
 public:
  Collection() : detail::IDSocketDeclaration<Collection>("NodeSocketCollection")
  {
  }
};

class Texture : public detail::IDSocketDeclaration<Texture> {
 public:
  Texture() : detail::IDSocketDeclaration<Texture>("NodeSocketTexture")
  {
  }
};

class Geometry : public SocketDeclaration {
 public:
  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;
};

}  // namespace blender::nodes::decl
