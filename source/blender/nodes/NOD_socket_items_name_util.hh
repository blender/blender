/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"

#include "NOD_socket_items.hh"

namespace blender::nodes::socket_items {

std::string variable_name_find_short(StringRef src_name, FunctionRef<bool(StringRef)> exists_fn);

std::string variable_name_validate(StringRef name);

template<typename Accessor>
inline std::string variable_name_find_short(const bNode &node, const StringRef src_name)
{
  using ItemT = typename Accessor::ItemT;
  return socket_items::variable_name_find_short(src_name, [&](const StringRef name) {
    const SocketItemsRef<ItemT> items = Accessor::get_items_from_node(const_cast<bNode &>(node));
    return std::any_of(*items.items, *items.items + *items.items_num, [&](ItemT &item) {
      const StringRef item_name = *Accessor::get_name(item);
      return item_name == name;
    });
  });
}

}  // namespace blender::nodes::socket_items
