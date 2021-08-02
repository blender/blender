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

#include <optional>

#include "BKE_node.h"

#include "FN_multi_function_data_type.hh"

namespace blender::nodes {

using fn::CPPType;
using fn::MFDataType;

std::optional<MFDataType> socket_mf_type_get(const bNodeSocketType &stype);
bool socket_is_mf_data_socket(const bNodeSocketType &stype);
void socket_expand_in_mf_network(SocketMFNetworkBuilder &builder);

}  // namespace blender::nodes
