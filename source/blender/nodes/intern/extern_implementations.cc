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

namespace blender::nodes {
#define MAKE_EXTERN_SOCKET_IMPLEMENTATION(TYPE) \
  template class SocketDeclarationBuilder<TYPE>; \
  template TYPE::Builder &NodeDeclarationBuilder::add_input<TYPE>(StringRef, StringRef); \
  template TYPE::Builder &NodeDeclarationBuilder::add_output<TYPE>(StringRef, StringRef);

MAKE_EXTERN_SOCKET_IMPLEMENTATION(decl::Float)
MAKE_EXTERN_SOCKET_IMPLEMENTATION(decl::Int)
MAKE_EXTERN_SOCKET_IMPLEMENTATION(decl::Vector)
MAKE_EXTERN_SOCKET_IMPLEMENTATION(decl::Bool)
MAKE_EXTERN_SOCKET_IMPLEMENTATION(decl::Color)
MAKE_EXTERN_SOCKET_IMPLEMENTATION(decl::String)
MAKE_EXTERN_SOCKET_IMPLEMENTATION(decl::Geometry)

#undef MAKE_EXTERN_SOCKET_IMPLEMENTATION
}  // namespace blender::nodes
