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

#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "BKE_node.h"

#include "BLI_math_vector.h"

namespace blender::nodes::decl {

/**
 * \note This function only deals with declarations, not the field status of existing nodes. If the
 * field status of existing nodes was stored on the sockets, an improvement would be to check the
 * existing socket's current status instead of the declaration.
 */
static bool field_types_are_compatible(const SocketDeclaration &input,
                                       const SocketDeclaration &output)
{
  if (output.output_field_dependency().field_type() == OutputSocketFieldType::FieldSource) {
    if (input.input_field_type() == InputSocketFieldType::None) {
      return false;
    }
  }
  return true;
}

static bool sockets_can_connect(const SocketDeclaration &socket_decl,
                                const bNodeSocket &other_socket)
{
  /* Input sockets cannot connect to input sockets, outputs cannot connect to outputs. */
  if (socket_decl.in_out() == other_socket.in_out) {
    return false;
  }

  if (other_socket.declaration) {
    if (socket_decl.in_out() == SOCK_IN) {
      if (!field_types_are_compatible(socket_decl, *other_socket.declaration)) {
        return false;
      }
    }
    else {
      if (!field_types_are_compatible(*other_socket.declaration, socket_decl)) {
        return false;
      }
    }
  }

  return true;
}

static bool basic_types_can_connect(const SocketDeclaration &UNUSED(socket_decl),
                                    const bNodeSocket &other_socket)
{
  return ELEM(other_socket.type, SOCK_FLOAT, SOCK_INT, SOCK_BOOLEAN, SOCK_VECTOR, SOCK_RGBA);
}

static void modify_subtype_except_for_storage(bNodeSocket &socket, int new_subtype)
{
  const char *idname = nodeStaticSocketType(socket.type, new_subtype);
  BLI_strncpy(socket.idname, idname, sizeof(socket.idname));
  bNodeSocketType *socktype = nodeSocketTypeFind(idname);
  socket.typeinfo = socktype;
}

/* -------------------------------------------------------------------- */
/** \name #Float
 * \{ */

bNodeSocket &Float::build(bNodeTree &ntree, bNode &node) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out_, SOCK_FLOAT, subtype_, identifier_.c_str(), name_.c_str());
  this->set_common_flags(socket);
  bNodeSocketValueFloat &value = *(bNodeSocketValueFloat *)socket.default_value;
  value.min = soft_min_value_;
  value.max = soft_max_value_;
  value.value = default_value_;
  return socket;
}

bool Float::matches(const bNodeSocket &socket) const
{
  if (!this->matches_common_data(socket)) {
    return false;
  }
  if (socket.type != SOCK_FLOAT) {
    return false;
  }
  if (socket.typeinfo->subtype != subtype_) {
    return false;
  }
  bNodeSocketValueFloat &value = *(bNodeSocketValueFloat *)socket.default_value;
  if (value.min != soft_min_value_) {
    return false;
  }
  if (value.max != soft_max_value_) {
    return false;
  }
  return true;
}

bool Float::can_connect(const bNodeSocket &socket) const
{
  if (!sockets_can_connect(*this, socket)) {
    return false;
  }
  return basic_types_can_connect(*this, socket);
}

bNodeSocket &Float::update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const
{
  if (socket.type != SOCK_FLOAT) {
    BLI_assert(socket.in_out == in_out_);
    return this->build(ntree, node);
  }
  if (socket.typeinfo->subtype != subtype_) {
    modify_subtype_except_for_storage(socket, subtype_);
  }
  this->set_common_flags(socket);
  bNodeSocketValueFloat &value = *(bNodeSocketValueFloat *)socket.default_value;
  value.min = soft_min_value_;
  value.max = soft_max_value_;
  value.subtype = subtype_;
  return socket;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Int
 * \{ */

bNodeSocket &Int::build(bNodeTree &ntree, bNode &node) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out_, SOCK_INT, subtype_, identifier_.c_str(), name_.c_str());
  this->set_common_flags(socket);
  bNodeSocketValueInt &value = *(bNodeSocketValueInt *)socket.default_value;
  value.min = soft_min_value_;
  value.max = soft_max_value_;
  value.value = default_value_;
  return socket;
}

bool Int::matches(const bNodeSocket &socket) const
{
  if (!this->matches_common_data(socket)) {
    return false;
  }
  if (socket.type != SOCK_INT) {
    return false;
  }
  if (socket.typeinfo->subtype != subtype_) {
    return false;
  }
  bNodeSocketValueInt &value = *(bNodeSocketValueInt *)socket.default_value;
  if (value.min != soft_min_value_) {
    return false;
  }
  if (value.max != soft_max_value_) {
    return false;
  }
  return true;
}

bool Int::can_connect(const bNodeSocket &socket) const
{
  if (!sockets_can_connect(*this, socket)) {
    return false;
  }
  return basic_types_can_connect(*this, socket);
}

bNodeSocket &Int::update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const
{
  if (socket.type != SOCK_INT) {
    BLI_assert(socket.in_out == in_out_);
    return this->build(ntree, node);
  }
  if (socket.typeinfo->subtype != subtype_) {
    modify_subtype_except_for_storage(socket, subtype_);
  }
  this->set_common_flags(socket);
  bNodeSocketValueInt &value = *(bNodeSocketValueInt *)socket.default_value;
  value.min = soft_min_value_;
  value.max = soft_max_value_;
  value.subtype = subtype_;
  return socket;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Vector
 * \{ */

bNodeSocket &Vector::build(bNodeTree &ntree, bNode &node) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out_, SOCK_VECTOR, subtype_, identifier_.c_str(), name_.c_str());
  this->set_common_flags(socket);
  bNodeSocketValueVector &value = *(bNodeSocketValueVector *)socket.default_value;
  copy_v3_v3(value.value, default_value_);
  value.min = soft_min_value_;
  value.max = soft_max_value_;
  return socket;
}

bool Vector::matches(const bNodeSocket &socket) const
{
  if (!this->matches_common_data(socket)) {
    return false;
  }
  if (socket.type != SOCK_VECTOR) {
    return false;
  }
  if (socket.typeinfo->subtype != subtype_) {
    return false;
  }
  return true;
}

bool Vector::can_connect(const bNodeSocket &socket) const
{
  if (!sockets_can_connect(*this, socket)) {
    return false;
  }
  return basic_types_can_connect(*this, socket);
}

bNodeSocket &Vector::update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const
{
  if (socket.type != SOCK_VECTOR) {
    BLI_assert(socket.in_out == in_out_);
    return this->build(ntree, node);
  }
  if (socket.typeinfo->subtype != subtype_) {
    modify_subtype_except_for_storage(socket, subtype_);
  }
  this->set_common_flags(socket);
  bNodeSocketValueVector &value = *(bNodeSocketValueVector *)socket.default_value;
  value.subtype = subtype_;
  STRNCPY(socket.name, name_.c_str());
  return socket;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Bool
 * \{ */

bNodeSocket &Bool::build(bNodeTree &ntree, bNode &node) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out_, SOCK_BOOLEAN, PROP_NONE, identifier_.c_str(), name_.c_str());
  this->set_common_flags(socket);
  bNodeSocketValueBoolean &value = *(bNodeSocketValueBoolean *)socket.default_value;
  value.value = default_value_;
  return socket;
}

bool Bool::matches(const bNodeSocket &socket) const
{
  if (!this->matches_common_data(socket)) {
    return false;
  }
  if (socket.type != SOCK_BOOLEAN) {
    return false;
  }
  return true;
}

bool Bool::can_connect(const bNodeSocket &socket) const
{
  if (!sockets_can_connect(*this, socket)) {
    return false;
  }
  return basic_types_can_connect(*this, socket);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Color
 * \{ */

bNodeSocket &Color::build(bNodeTree &ntree, bNode &node) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out_, SOCK_RGBA, PROP_NONE, identifier_.c_str(), name_.c_str());
  this->set_common_flags(socket);
  bNodeSocketValueRGBA &value = *(bNodeSocketValueRGBA *)socket.default_value;
  copy_v4_v4(value.value, default_value_);
  return socket;
}

bool Color::matches(const bNodeSocket &socket) const
{
  if (!this->matches_common_data(socket)) {
    if (socket.name != name_) {
      return false;
    }
    if (socket.identifier != identifier_) {
      return false;
    }
  }
  if (socket.type != SOCK_RGBA) {
    return false;
  }
  return true;
}

bool Color::can_connect(const bNodeSocket &socket) const
{
  if (!sockets_can_connect(*this, socket)) {
    return false;
  }
  return basic_types_can_connect(*this, socket);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #String
 * \{ */

bNodeSocket &String::build(bNodeTree &ntree, bNode &node) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out_, SOCK_STRING, PROP_NONE, identifier_.c_str(), name_.c_str());
  STRNCPY(((bNodeSocketValueString *)socket.default_value)->value, default_value_.c_str());
  this->set_common_flags(socket);
  return socket;
}

bool String::matches(const bNodeSocket &socket) const
{
  if (!this->matches_common_data(socket)) {
    return false;
  }
  if (socket.type != SOCK_STRING) {
    return false;
  }
  return true;
}

bool String::can_connect(const bNodeSocket &socket) const
{
  return sockets_can_connect(*this, socket) && socket.type == SOCK_STRING;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #IDSocketDeclaration
 * \{ */

bNodeSocket &IDSocketDeclaration::build(bNodeTree &ntree, bNode &node) const
{
  bNodeSocket &socket = *nodeAddSocket(
      &ntree, &node, in_out_, idname_, identifier_.c_str(), name_.c_str());
  this->set_common_flags(socket);
  return socket;
}

bool IDSocketDeclaration::matches(const bNodeSocket &socket) const
{
  if (!this->matches_common_data(socket)) {
    return false;
  }
  if (!STREQ(socket.idname, idname_)) {
    return false;
  }
  return true;
}

bool IDSocketDeclaration::can_connect(const bNodeSocket &socket) const
{
  return sockets_can_connect(*this, socket) && STREQ(socket.idname, idname_);
}

bNodeSocket &IDSocketDeclaration::update_or_build(bNodeTree &ntree,
                                                  bNode &node,
                                                  bNodeSocket &socket) const
{
  if (StringRef(socket.idname) != idname_) {
    BLI_assert(socket.in_out == in_out_);
    return this->build(ntree, node);
  }
  this->set_common_flags(socket);
  return socket;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Geometry
 * \{ */

bNodeSocket &Geometry::build(bNodeTree &ntree, bNode &node) const
{
  bNodeSocket &socket = *nodeAddSocket(
      &ntree, &node, in_out_, "NodeSocketGeometry", identifier_.c_str(), name_.c_str());
  this->set_common_flags(socket);
  return socket;
}

bool Geometry::matches(const bNodeSocket &socket) const
{
  if (!this->matches_common_data(socket)) {
    return false;
  }
  if (socket.type != SOCK_GEOMETRY) {
    return false;
  }
  return true;
}

bool Geometry::can_connect(const bNodeSocket &socket) const
{
  return sockets_can_connect(*this, socket) && socket.type == SOCK_GEOMETRY;
}

Span<GeometryComponentType> Geometry::supported_types() const
{
  return supported_types_;
}

bool Geometry::only_realized_data() const
{
  return only_realized_data_;
}

bool Geometry::only_instances() const
{
  return only_instances_;
}

GeometryBuilder &GeometryBuilder::supported_type(GeometryComponentType supported_type)
{
  decl_->supported_types_ = {supported_type};
  return *this;
}

GeometryBuilder &GeometryBuilder::supported_type(
    blender::Vector<GeometryComponentType> supported_types)
{
  decl_->supported_types_ = std::move(supported_types);
  return *this;
}

GeometryBuilder &GeometryBuilder::only_realized_data(bool value)
{
  decl_->only_realized_data_ = value;
  return *this;
}

GeometryBuilder &GeometryBuilder::only_instances(bool value)
{
  decl_->only_instances_ = value;
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Shader
 * \{ */

bNodeSocket &Shader::build(bNodeTree &ntree, bNode &node) const
{
  bNodeSocket &socket = *nodeAddSocket(
      &ntree, &node, in_out_, "NodeSocketShader", identifier_.c_str(), name_.c_str());
  this->set_common_flags(socket);
  return socket;
}

bool Shader::matches(const bNodeSocket &socket) const
{
  if (!this->matches_common_data(socket)) {
    return false;
  }
  if (socket.type != SOCK_SHADER) {
    return false;
  }
  return true;
}

bool Shader::can_connect(const bNodeSocket &socket) const
{
  if (!sockets_can_connect(*this, socket)) {
    return false;
  }
  /* Basic types can convert to shaders, but not the other way around. */
  if (in_out_ == SOCK_IN) {
    return ELEM(
        socket.type, SOCK_VECTOR, SOCK_RGBA, SOCK_FLOAT, SOCK_INT, SOCK_BOOLEAN, SOCK_SHADER);
  }
  return socket.type == SOCK_SHADER;
}

/** \} */

}  // namespace blender::nodes::decl
