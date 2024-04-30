/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "DNA_array_utils.hh"
#include "DNA_node_types.h"

#include "BKE_node_enum.hh"

using blender::bke::NodeSocketValueMenuRuntimeFlag;

bool bNodeSocketValueMenu::has_conflict() const
{
  return this->runtime_flag & NodeSocketValueMenuRuntimeFlag::NODE_MENU_ITEMS_CONFLICT;
}

blender::Span<NodeEnumItem> NodeEnumDefinition::items() const
{
  return {this->items_array, this->items_num};
}

blender::MutableSpan<NodeEnumItem> NodeEnumDefinition::items()
{
  return {this->items_array, this->items_num};
}
