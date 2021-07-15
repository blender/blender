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
#include "BKE_object.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "ED_undo.h"

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_geometry_nodes_eval_log.hh"

#include "node_intern.h"

using blender::IndexRange;
using blender::Map;
using blender::Set;
using blender::StringRef;
namespace geo_log = blender::nodes::geometry_nodes_eval_log;
using geo_log::GeometryAttributeInfo;

struct AttributeSearchData {
  const bNodeTree *tree;
  const bNode *node;
  bNodeSocket *socket;
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

static bool attribute_search_item_add(uiSearchItems *items, const GeometryAttributeInfo &item)
{
  const StringRef data_type_name = attribute_data_type_string(item.data_type);
  const StringRef domain_name = attribute_domain_string(item.domain);
  std::string search_item_text = domain_name + " " + MENU_SEP + item.name + UI_SEP_CHAR +
                                 data_type_name;

  return UI_search_item_add(
      items, search_item_text.c_str(), (void *)&item, ICON_NONE, UI_BUT_HAS_SEP_CHAR, 0);
}

static GeometryAttributeInfo &get_dummy_item_info()
{
  static GeometryAttributeInfo info;
  return info;
}

static void attribute_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  AttributeSearchData *data = static_cast<AttributeSearchData *>(arg);

  SpaceNode *snode = CTX_wm_space_node(C);
  const geo_log::NodeLog *node_log = geo_log::ModifierLog::find_node_by_node_editor_context(
      *snode, *data->node);
  if (node_log == nullptr) {
    return;
  }
  blender::Vector<const GeometryAttributeInfo *> infos = node_log->lookup_available_attributes();

  GeometryAttributeInfo &dummy_info = get_dummy_item_info();

  /* Any string may be valid, so add the current search string along with the hints. */
  if (str[0] != '\0') {
    bool contained = false;
    for (const GeometryAttributeInfo *attribute_info : infos) {
      if (attribute_info->name == str) {
        contained = true;
        break;
      }
    }
    if (!contained) {
      dummy_info.name = str;
      UI_search_item_add(items, str, &dummy_info, ICON_ADD, 0, 0);
    }
  }

  if (str[0] == '\0' && !is_first) {
    /* Allow clearing the text field when the string is empty, but not on the first pass,
     * or opening an attribute field for the first time would show this search item. */
    dummy_info.name = str;
    UI_search_item_add(items, str, &dummy_info, ICON_X, 0, 0);
  }

  /* Don't filter when the menu is first opened, but still run the search
   * so the items are in the same order they will appear in while searching. */
  const char *string = is_first ? "" : str;

  StringSearch *search = BLI_string_search_new();
  for (const GeometryAttributeInfo *item : infos) {
    BLI_string_search_add(search, item->name.c_str(), (void *)item);
  }

  GeometryAttributeInfo **filtered_items;
  const int filtered_amount = BLI_string_search_query(search, string, (void ***)&filtered_items);

  for (const int i : IndexRange(filtered_amount)) {
    const GeometryAttributeInfo *item = filtered_items[i];
    if (!attribute_search_item_add(items, *item)) {
      break;
    }
  }

  MEM_freeN(filtered_items);
  BLI_string_search_free(search);
}

static void attribute_search_exec_fn(bContext *C, void *data_v, void *item_v)
{
  if (item_v == nullptr) {
    return;
  }
  AttributeSearchData *data = static_cast<AttributeSearchData *>(data_v);
  GeometryAttributeInfo *item = (GeometryAttributeInfo *)item_v;

  bNodeSocket &socket = *data->socket;
  bNodeSocketValueString *value = static_cast<bNodeSocketValueString *>(socket.default_value);
  BLI_strncpy(value->value, item->name.c_str(), MAX_NAME);

  ED_undo_push(C, "Assign Attribute Name");
}

void node_geometry_add_attribute_search_button(const bContext *UNUSED(C),
                                               const bNodeTree *node_tree,
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

  AttributeSearchData *data = OBJECT_GUARDED_NEW(
      AttributeSearchData, {node_tree, node, (bNodeSocket *)socket_ptr->data});

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
