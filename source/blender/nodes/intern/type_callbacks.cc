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

#include "NOD_node_tree_multi_function.hh"
#include "NOD_type_callbacks.hh"

namespace blender::nodes {

const CPPType *socket_cpp_type_get(const bNodeSocketType &stype)
{
  if (stype.get_cpp_type != nullptr) {
    return stype.get_cpp_type();
  }
  return nullptr;
}

std::optional<MFDataType> socket_mf_type_get(const bNodeSocketType &stype)
{
  const CPPType *cpp_type = socket_cpp_type_get(stype);
  if (cpp_type != nullptr) {
    return MFDataType::ForSingle(*cpp_type);
  }
  return {};
}

bool socket_is_mf_data_socket(const bNodeSocketType &stype)
{
  if (!socket_mf_type_get(stype).has_value()) {
    return false;
  }
  if (stype.expand_in_mf_network == nullptr && stype.get_cpp_value == nullptr) {
    return false;
  }
  return true;
}

bool socket_cpp_value_get(const bNodeSocket &socket, void *r_value)
{
  if (socket.typeinfo->get_cpp_value != nullptr) {
    socket.typeinfo->get_cpp_value(socket, r_value);
    return true;
  }
  return false;
}

void socket_expand_in_mf_network(SocketMFNetworkBuilder &builder)
{
  bNodeSocket &socket = builder.bsocket();
  if (socket.typeinfo->expand_in_mf_network != nullptr) {
    socket.typeinfo->expand_in_mf_network(builder);
  }
  else if (socket.typeinfo->get_cpp_value != nullptr) {
    const CPPType &type = *socket_cpp_type_get(*socket.typeinfo);
    void *buffer = builder.resources().linear_allocator().allocate(type.size(), type.alignment());
    socket.typeinfo->get_cpp_value(socket, buffer);
    builder.set_constant_value(type, buffer);
  }
  else {
    BLI_assert(false);
  }
}

}  // namespace blender::nodes
