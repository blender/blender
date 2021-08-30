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

#include "BKE_node.h"

#include "BLI_math_vector.h"

namespace blender::nodes::decl {

static void modify_subtype_except_for_storage(bNodeSocket &socket, int new_subtype)
{
  const char *idname = nodeStaticSocketType(socket.type, new_subtype);
  BLI_strncpy(socket.idname, idname, sizeof(socket.idname));
  bNodeSocketType *socktype = nodeSocketTypeFind(idname);
  socket.typeinfo = socktype;
}

/* --------------------------------------------------------------------
 * Float.
 */

bNodeSocket &Float::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out, SOCK_FLOAT, subtype_, identifier_.c_str(), name_.c_str());
  bNodeSocketValueFloat &value = *(bNodeSocketValueFloat *)socket.default_value;
  value.min = soft_min_value_;
  value.max = soft_max_value_;
  value.value = default_value_;
  return socket;
}

bool Float::matches(const bNodeSocket &socket) const
{
  if (socket.type != SOCK_FLOAT) {
    return false;
  }
  if (socket.typeinfo->subtype != subtype_) {
    return false;
  }
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != identifier_) {
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

bNodeSocket &Float::update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const
{
  if (socket.type != SOCK_FLOAT) {
    return this->build(ntree, node, (eNodeSocketInOut)socket.in_out);
  }
  if (socket.typeinfo->subtype != subtype_) {
    modify_subtype_except_for_storage(socket, subtype_);
  }
  bNodeSocketValueFloat &value = *(bNodeSocketValueFloat *)socket.default_value;
  value.min = soft_min_value_;
  value.max = soft_max_value_;
  value.subtype = subtype_;
  return socket;
}

/* --------------------------------------------------------------------
 * Int.
 */

bNodeSocket &Int::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out, SOCK_INT, subtype_, identifier_.c_str(), name_.c_str());
  bNodeSocketValueInt &value = *(bNodeSocketValueInt *)socket.default_value;
  value.min = soft_min_value_;
  value.max = soft_max_value_;
  value.value = default_value_;
  return socket;
}

bool Int::matches(const bNodeSocket &socket) const
{
  if (socket.type != SOCK_INT) {
    return false;
  }
  if (socket.typeinfo->subtype != subtype_) {
    return false;
  }
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != identifier_) {
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

bNodeSocket &Int::update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const
{
  if (socket.type != SOCK_INT) {
    return this->build(ntree, node, (eNodeSocketInOut)socket.in_out);
  }
  if (socket.typeinfo->subtype != subtype_) {
    modify_subtype_except_for_storage(socket, subtype_);
  }
  bNodeSocketValueInt &value = *(bNodeSocketValueInt *)socket.default_value;
  value.min = soft_min_value_;
  value.max = soft_max_value_;
  value.subtype = subtype_;
  return socket;
}

/* --------------------------------------------------------------------
 * Vector.
 */

bNodeSocket &Vector::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out, SOCK_VECTOR, subtype_, identifier_.c_str(), name_.c_str());
  bNodeSocketValueVector &value = *(bNodeSocketValueVector *)socket.default_value;
  copy_v3_v3(value.value, default_value_);
  return socket;
}

bool Vector::matches(const bNodeSocket &socket) const
{
  if (socket.type != SOCK_VECTOR) {
    return false;
  }
  if (socket.typeinfo->subtype != subtype_) {
    return false;
  }
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != identifier_) {
    return false;
  }
  return true;
}

bNodeSocket &Vector::update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const
{
  if (socket.type != SOCK_VECTOR) {
    return this->build(ntree, node, (eNodeSocketInOut)socket.in_out);
  }
  if (socket.typeinfo->subtype != subtype_) {
    modify_subtype_except_for_storage(socket, subtype_);
  }
  bNodeSocketValueVector &value = *(bNodeSocketValueVector *)socket.default_value;
  value.subtype = subtype_;
  STRNCPY(socket.name, name_.c_str());
  return socket;
}

/* --------------------------------------------------------------------
 * Bool.
 */

bNodeSocket &Bool::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out, SOCK_BOOLEAN, PROP_NONE, identifier_.c_str(), name_.c_str());
  bNodeSocketValueBoolean &value = *(bNodeSocketValueBoolean *)socket.default_value;
  value.value = default_value_;
  return socket;
}

bool Bool::matches(const bNodeSocket &socket) const
{
  if (socket.type != SOCK_BOOLEAN) {
    return false;
  }
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != identifier_) {
    return false;
  }
  return true;
}

/* --------------------------------------------------------------------
 * Color.
 */

bNodeSocket &Color::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out, SOCK_RGBA, PROP_NONE, identifier_.c_str(), name_.c_str());
  bNodeSocketValueRGBA &value = *(bNodeSocketValueRGBA *)socket.default_value;
  copy_v4_v4(value.value, default_value_);
  return socket;
}

bool Color::matches(const bNodeSocket &socket) const
{
  if (socket.type != SOCK_RGBA) {
    return false;
  }
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != identifier_) {
    return false;
  }
  return true;
}

/* --------------------------------------------------------------------
 * String.
 */

bNodeSocket &String::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out, SOCK_STRING, PROP_NONE, identifier_.c_str(), name_.c_str());
  return socket;
}

bool String::matches(const bNodeSocket &socket) const
{
  if (socket.type != SOCK_STRING) {
    return false;
  }
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != identifier_) {
    return false;
  }
  return true;
}

/* --------------------------------------------------------------------
 * IDSocketDeclaration.
 */

namespace detail {
bNodeSocket &build_id_socket(bNodeTree &ntree,
                             bNode &node,
                             eNodeSocketInOut in_out,
                             const CommonIDSocketData &data,
                             StringRefNull name,
                             StringRefNull identifier)
{
  bNodeSocket &socket = *nodeAddSocket(
      &ntree, &node, in_out, data.idname, name.c_str(), identifier.c_str());
  if (data.hide_label) {
    socket.flag |= SOCK_HIDE_LABEL;
  }
  return socket;
}

bool matches_id_socket(const bNodeSocket &socket,
                       const CommonIDSocketData &data,
                       StringRefNull name,
                       StringRefNull identifier)
{
  if (!STREQ(socket.idname, data.idname)) {
    return false;
  }
  if (data.hide_label != ((socket.flag & SOCK_HIDE_LABEL) != 0)) {
    return false;
  }
  if (socket.name != name) {
    return false;
  }
  if (socket.identifier != identifier) {
    return false;
  }
  return true;
}
}  // namespace detail

/* --------------------------------------------------------------------
 * Geometry.
 */

bNodeSocket &Geometry::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddSocket(
      &ntree, &node, in_out, "NodeSocketGeometry", identifier_.c_str(), name_.c_str());
  return socket;
}

bool Geometry::matches(const bNodeSocket &socket) const
{
  if (socket.type != SOCK_GEOMETRY) {
    return false;
  }
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != identifier_) {
    return false;
  }
  return true;
}

}  // namespace blender::nodes::decl
