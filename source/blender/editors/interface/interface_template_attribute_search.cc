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

/** \file
 * \ingroup edinterface
 */

#include "BLI_string_ref.hh"
#include "BLI_string_search.h"

#include "DNA_customdata_types.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "BLT_translation.h"

#include "NOD_geometry_nodes_eval_log.hh"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_resources.h"

using blender::nodes::geometry_nodes_eval_log::GeometryAttributeInfo;

namespace blender::ui {

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

static bool attribute_search_item_add(uiSearchItems *items, const GeometryAttributeInfo &item)
{
  const StringRef data_type_name = attribute_data_type_string(item.data_type);
  const StringRef domain_name = attribute_domain_string(item.domain);
  std::string search_item_text = domain_name + " " + UI_MENU_ARROW_SEP + item.name + UI_SEP_CHAR +
                                 data_type_name;

  return UI_search_item_add(
      items, search_item_text.c_str(), (void *)&item, ICON_NONE, UI_BUT_HAS_SEP_CHAR, 0);
}

void attribute_search_add_items(StringRefNull str,
                                const bool is_output,
                                Span<const GeometryAttributeInfo *> infos,
                                uiSearchItems *seach_items,
                                const bool is_first)
{
  static GeometryAttributeInfo dummy_info;

  /* Any string may be valid, so add the current search string along with the hints. */
  if (str[0] != '\0') {
    bool contained = false;
    for (const GeometryAttributeInfo *attribute_info : infos) {
      if (attribute_info->name == str) {
        contained = true;
        break;
      }
    }
    if (!contained && is_output) {
      dummy_info.name = str;
      UI_search_item_add(seach_items, str.c_str(), &dummy_info, ICON_ADD, 0, 0);
    }
  }

  if (str[0] == '\0' && !is_first) {
    /* Allow clearing the text field when the string is empty, but not on the first pass,
     * or opening an attribute field for the first time would show this search item. */
    dummy_info.name = str;
    UI_search_item_add(seach_items, str.c_str(), &dummy_info, ICON_X, 0, 0);
  }

  /* Don't filter when the menu is first opened, but still run the search
   * so the items are in the same order they will appear in while searching. */
  const char *string = is_first ? "" : str.c_str();

  StringSearch *search = BLI_string_search_new();
  for (const GeometryAttributeInfo *item : infos) {

    /* Don't show the legacy "normal" attribute. */
    if (item->name == "normal" && item->domain == ATTR_DOMAIN_FACE) {
      continue;
    }

    BLI_string_search_add(search, item->name.c_str(), (void *)item);
  }

  GeometryAttributeInfo **filtered_items;
  const int filtered_amount = BLI_string_search_query(search, string, (void ***)&filtered_items);

  for (const int i : IndexRange(filtered_amount)) {
    const GeometryAttributeInfo *item = filtered_items[i];
    if (!attribute_search_item_add(seach_items, *item)) {
      break;
    }
  }

  MEM_freeN(filtered_items);
  BLI_string_search_free(search);
}

}  // namespace blender::ui