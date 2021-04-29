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

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_intern.h"

using blender::IndexRange;
using blender::Map;
using blender::Set;
using blender::StringRef;

struct AttributeSearchData {
  AvailableAttributeInfo &dummy_info_for_search;
  const NodeUIStorage &ui_storage;
  bNodeSocket &socket;
};

/* This class must not have a destructor, since it is used by buttons and freed with #MEM_freeN. */
BLI_STATIC_ASSERT(std::is_trivially_destructible_v<AttributeSearchData>, "");

static StringRef attribute_data_type_string(const CustomDataType type)
{
  const char *name = nullptr;
  RNA_enum_name_from_value(rna_enum_attribute_type_items, type, &name);
  return StringRef(IFACE_(name));
}

static StringRef attribute_domain_string(const AttributeDomain domain)
{
  const char *name = nullptr;
  RNA_enum_name_from_value(rna_enum_attribute_domain_items, domain, &name);
  return StringRef(IFACE_(name));
}

/* Unicode arrow. */
#define MENU_SEP "\xe2\x96\xb6"

static bool attribute_search_item_add(uiSearchItems *items, const AvailableAttributeInfo &item)
{
  const StringRef data_type_name = attribute_data_type_string(item.data_type);
  const StringRef domain_name = attribute_domain_string(item.domain);
  std::string search_item_text = domain_name + " " + MENU_SEP + item.name + UI_SEP_CHAR +
                                 data_type_name;

  return UI_search_item_add(
      items, search_item_text.c_str(), (void *)&item, ICON_NONE, UI_BUT_HAS_SEP_CHAR, 0);
}

static void attribute_search_update_fn(const bContext *UNUSED(C),
                                       void *arg,
                                       const char *str,
                                       uiSearchItems *items,
                                       const bool is_first)
{
  AttributeSearchData *data = static_cast<AttributeSearchData *>(arg);

  const Set<AvailableAttributeInfo> &attribute_hints = data->ui_storage.attribute_hints;

  /* Any string may be valid, so add the current search string along with the hints. */
  if (str[0] != '\0') {
    /* Note that the attribute domain and data type are dummies, since
     * #AvailableAttributeInfo equality is only based on the string. */
    if (!attribute_hints.contains(AvailableAttributeInfo{str, ATTR_DOMAIN_AUTO, CD_PROP_BOOL})) {
      data->dummy_info_for_search.name = std::string(str);
      UI_search_item_add(items, str, &data->dummy_info_for_search, ICON_ADD, 0, 0);
    }
  }

  if (str[0] == '\0' && !is_first) {
    /* Allow clearing the text field when the string is empty, but not on the first pass,
     * or opening an attribute field for the first time would show this search item. */
    data->dummy_info_for_search.name = std::string(str);
    UI_search_item_add(items, str, &data->dummy_info_for_search, ICON_X, 0, 0);
  }

  /* Don't filter when the menu is first opened, but still run the search
   * so the items are in the same order they will appear in while searching. */
  const char *string = is_first ? "" : str;

  StringSearch *search = BLI_string_search_new();
  for (const AvailableAttributeInfo &item : attribute_hints) {
    BLI_string_search_add(search, item.name.c_str(), (void *)&item);
  }

  AvailableAttributeInfo **filtered_items;
  const int filtered_amount = BLI_string_search_query(search, string, (void ***)&filtered_items);

  for (const int i : IndexRange(filtered_amount)) {
    const AvailableAttributeInfo *item = filtered_items[i];
    if (!attribute_search_item_add(items, *item)) {
      break;
    }
  }

  MEM_freeN(filtered_items);
  BLI_string_search_free(search);
}

static void attribute_search_exec_fn(bContext *UNUSED(C), void *data_v, void *item_v)
{
  AttributeSearchData *data = static_cast<AttributeSearchData *>(data_v);
  AvailableAttributeInfo *item = static_cast<AvailableAttributeInfo *>(item_v);

  bNodeSocket &socket = data->socket;
  bNodeSocketValueString *value = static_cast<bNodeSocketValueString *>(socket.default_value);
  BLI_strncpy(value->value, item->name.c_str(), MAX_NAME);
}

void node_geometry_add_attribute_search_button(const bContext *C,
                                               const bNodeTree *node_tree,
                                               const bNode *node,
                                               PointerRNA *socket_ptr,
                                               uiLayout *layout)
{
  const NodeUIStorage *ui_storage = BKE_node_tree_ui_storage_get_from_context(
      C, *node_tree, *node);

  if (ui_storage == nullptr) {
    uiItemR(layout, socket_ptr, "default_value", 0, "", 0);
    return;
  }

  const NodeTreeUIStorage *tree_ui_storage = node_tree->ui_storage;

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

  AttributeSearchData *data = OBJECT_GUARDED_NEW(AttributeSearchData,
                                                 {tree_ui_storage->dummy_info_for_search,
                                                  *ui_storage,
                                                  *static_cast<bNodeSocket *>(socket_ptr->data)});

  UI_but_func_search_set_results_are_suggestions(but, true);
  UI_but_func_search_set_sep_string(but, MENU_SEP);
  UI_but_func_search_set(but,
                         nullptr,
                         attribute_search_update_fn,
                         static_cast<void *>(data),
                         true,
                         nullptr,
                         attribute_search_exec_fn,
                         nullptr);
}
