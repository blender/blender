/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "DNA_array_utils.hh"
#include "DNA_node_types.h"

#include "BKE_node.h"
#include "BKE_node_enum.hh"
#include "BKE_node_runtime.hh"

using blender::bke::NodeSocketValueMenuRuntimeFlag;

bool bNodeSocketValueMenu::has_conflict() const
{
  return this->runtime_flag & NodeSocketValueMenuRuntimeFlag::NODE_MENU_ITEMS_CONFLICT;
}

blender::Span<NodeEnumItem> NodeEnumDefinition::items() const
{
  return {this->items_array, this->items_num};
}

blender::MutableSpan<NodeEnumItem> NodeEnumDefinition::items_for_write()
{
  return {this->items_array, this->items_num};
}

NodeEnumItem *NodeEnumDefinition::add_item(const blender::StringRef name)
{
  const int insert_index = this->items_num;
  NodeEnumItem *old_items = this->items_array;

  this->items_array = MEM_cnew_array<NodeEnumItem>(this->items_num + 1, __func__);
  std::copy_n(old_items, insert_index, this->items_array);
  NodeEnumItem &new_item = this->items_array[insert_index];
  std::copy_n(old_items + insert_index + 1,
              this->items_num - insert_index,
              this->items_array + insert_index + 1);

  new_item.identifier = this->next_identifier++;
  this->set_item_name(new_item, name);

  this->items_num++;
  MEM_SAFE_FREE(old_items);

  return &new_item;
}

static void free_enum_item(NodeEnumItem *item)
{
  MEM_SAFE_FREE(item->name);
  MEM_SAFE_FREE(item->description);
}

bool NodeEnumDefinition::remove_item(NodeEnumItem &item)
{
  if (!this->items().contains_ptr(&item)) {
    return false;
  }
  const int remove_index = &item - this->items().begin();
  /* DNA fields are 16 bits, can't use directly. */
  int items_num = this->items_num;
  int active_index = this->active_index;
  blender::dna::array::remove_index(
      &this->items_array, &items_num, &active_index, remove_index, free_enum_item);
  this->items_num = int16_t(items_num);
  this->active_index = int16_t(active_index);
  return true;
}

void NodeEnumDefinition::clear()
{
  /* DNA fields are 16 bits, can't use directly. */
  int items_num = this->items_num;
  int active_index = this->active_index;
  blender::dna::array::clear(&this->items_array, &items_num, &active_index, free_enum_item);
  this->items_num = int16_t(items_num);
  this->active_index = int16_t(active_index);
}

bool NodeEnumDefinition::move_item(const int from_index, const int to_index)
{
  if (to_index < this->items_num) {
    const int items_num = this->items_num;
    const int active_index = this->active_index;
    blender::dna::array::move_index(this->items_array, items_num, from_index, to_index);
    this->items_num = int16_t(items_num);
    this->active_index = int16_t(active_index);
  }
  return true;
}

const NodeEnumItem *NodeEnumDefinition::active_item() const
{
  if (blender::IndexRange(this->items_num).contains(this->active_index)) {
    return &this->items()[this->active_index];
  }
  return nullptr;
}

NodeEnumItem *NodeEnumDefinition::active_item()
{
  if (blender::IndexRange(this->items_num).contains(this->active_index)) {
    return &this->items_for_write()[this->active_index];
  }
  return nullptr;
}

void NodeEnumDefinition::active_item_set(NodeEnumItem *item)
{
  this->active_index = this->items().contains_ptr(item) ? item - this->items_array : -1;
}

void NodeEnumDefinition::set_item_name(NodeEnumItem &item, const blender::StringRef name)
{
  char unique_name[MAX_NAME + 4];
  STRNCPY(unique_name, name.data());

  struct Args {
    NodeEnumDefinition *enum_def;
    const NodeEnumItem *item;
  } args = {this, &item};

  const char *default_name = items().is_empty() ? "Name" : items().last().name;
  BLI_uniquename_cb(
      [](void *arg, const char *name) {
        const Args &args = *static_cast<Args *>(arg);
        for (const NodeEnumItem &item : args.enum_def->items()) {
          if (&item != args.item) {
            if (STREQ(item.name, name)) {
              return true;
            }
          }
        }
        return false;
      },
      &args,
      default_name,
      '.',
      unique_name,
      ARRAY_SIZE(unique_name));

  MEM_SAFE_FREE(item.name);
  item.name = BLI_strdup(unique_name);
}
