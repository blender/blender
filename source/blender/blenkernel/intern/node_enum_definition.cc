/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_array_utils.hh"
#include "DNA_node_types.h"

#include "BKE_node_enum.hh"

namespace blender {

using bke::NodeSocketValueMenuRuntimeFlag;

bool bNodeSocketValueMenu::has_conflict() const
{
  return this->runtime_flag & NodeSocketValueMenuRuntimeFlag::NODE_MENU_ITEMS_CONFLICT;
}

Span<NodeEnumItem> NodeEnumDefinition::items() const
{
  return {this->items_array, this->items_num};
}

MutableSpan<NodeEnumItem> NodeEnumDefinition::items()
{
  return {this->items_array, this->items_num};
}

namespace bke {

const RuntimeNodeEnumItem *RuntimeNodeEnumItems::find_item_by_identifier(
    const int identifier) const
{
  for (const RuntimeNodeEnumItem &item : this->items) {
    if (item.identifier == identifier) {
      return &item;
    }
  }
  return nullptr;
}

}  // namespace bke
}  // namespace blender
