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

#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_search.h"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_node_ui_storage.hh"
#include "BKE_object.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_intern.h"

using blender::IndexRange;
using blender::Map;
using blender::Set;
using blender::StringRef;

struct AttributeSearchData {
  const bNodeTree &node_tree;
  const bNode &node;

  uiBut *search_button;

  /* Used to keep track of a button pointer over multiple redraws. Since the UI code
   * may reallocate the button, without this we might end up with a dangling pointer. */
  uiButStore *button_store;
  uiBlock *button_store_block;
};

static void attribute_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  AttributeSearchData *data = static_cast<AttributeSearchData *>(arg);
  const NodeUIStorage *ui_storage = BKE_node_tree_ui_storage_get_from_context(
      C, data->node_tree, data->node);
  if (ui_storage == nullptr) {
    return;
  }

  const Set<std::string> &attribute_name_hints = ui_storage->attribute_name_hints;

  if (str[0] != '\0' && !attribute_name_hints.contains_as(StringRef(str))) {
    /* Any string may be valid, so add the current search string with the hints. */
    UI_search_item_add(items, str, (void *)str, ICON_ADD, 0, 0);
  }

  if (str[0] == '\0' && !is_first) {
    /* Allow clearing the text field when the string is empty, but not on the first pass,
     * or opening an attribute field for the first time would show this search item. */
    UI_search_item_add(items, str, (void *)str, ICON_X, 0, 0);
  }

  /* Don't filter when the menu is first opened, but still run the search
   * so the items are in the same order they will appear in while searching. */
  const char *string = is_first ? "" : str;

  StringSearch *search = BLI_string_search_new();
  for (const std::string &attribute_name : attribute_name_hints) {
    BLI_string_search_add(search, attribute_name.c_str(), (void *)&attribute_name);
  }

  std::string **filtered_items;
  const int filtered_amount = BLI_string_search_query(search, string, (void ***)&filtered_items);

  for (const int i : IndexRange(filtered_amount)) {
    std::string *item = filtered_items[i];
    if (!UI_search_item_add(items, item->c_str(), item, ICON_NONE, 0, 0)) {
      break;
    }
  }

  MEM_freeN(filtered_items);
  BLI_string_search_free(search);
}

static void attribute_search_free_fn(void *arg)
{
  AttributeSearchData *data = static_cast<AttributeSearchData *>(arg);

  UI_butstore_free(data->button_store_block, data->button_store);
  delete data;
}

void node_geometry_add_attribute_search_button(const bNodeTree *node_tree,
                                               const bNode *node,
                                               PointerRNA *socket_ptr,
                                               uiLayout *layout)
{
  uiBlock *block = uiLayoutGetBlock(layout);
  uiBut *but = uiDefIconTextButR(block,
                                 UI_BTYPE_SEARCH_MENU,
                                 0,
                                 ICON_NONE,
                                 "",
                                 0,
                                 0,
                                 10 * UI_UNIT_X, /* Dummy value, replaced by layout system. */
                                 UI_UNIT_Y,
                                 socket_ptr,
                                 "default_value",
                                 0,
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 "");

  AttributeSearchData *data = new AttributeSearchData{
      *node_tree,
      *node,
      but,
      UI_butstore_create(block),
      block,
  };

  UI_butstore_register(data->button_store, &data->search_button);

  UI_but_func_search_set_results_are_suggestions(but, true);
  UI_but_func_search_set(but,
                         nullptr,
                         attribute_search_update_fn,
                         static_cast<void *>(data),
                         attribute_search_free_fn,
                         nullptr,
                         nullptr);
}
