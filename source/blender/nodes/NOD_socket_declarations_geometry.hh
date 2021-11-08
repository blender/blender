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

#include "BKE_geometry_set.hh"

#include "NOD_socket_declarations.hh"

namespace blender::nodes::decl {

class GeometryBuilder;

class Geometry : public SocketDeclaration {
 private:
  blender::Vector<GeometryComponentType> supported_types_;
  bool only_realized_data_ = false;
  bool only_instances_ = false;

  friend GeometryBuilder;

 public:
  using Builder = GeometryBuilder;

  bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const override;
  bool matches(const bNodeSocket &socket) const override;

  Span<GeometryComponentType> supported_types() const;
  bool only_realized_data() const;
  bool only_instances() const;
};

class GeometryBuilder : public SocketDeclarationBuilder<Geometry> {
 public:
  GeometryBuilder &supported_type(GeometryComponentType supported_type);
  GeometryBuilder &supported_type(blender::Vector<GeometryComponentType> supported_types);
  GeometryBuilder &only_realized_data(bool value = true);
  GeometryBuilder &only_instances(bool value = true);
};

}  // namespace blender::nodes::decl

namespace blender::nodes {
MAKE_EXTERN_SOCKET_DECLARATION(decl::Geometry)
}
