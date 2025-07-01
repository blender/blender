/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_string_ref.hh"

#include "DNA_customdata_types.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "BLT_translation.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"

#include "NOD_geometry_nodes_log.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_string_search.hh"

#include <fmt/format.h>

using blender::nodes::geo_eval_log::GeometryAttributeInfo;

namespace blender::ui {

static StringRef attribute_data_type_string(const eCustomDataType type)
{
  const char *name = nullptr;
  RNA_enum_name_from_value(rna_enum_attribute_type_items, type, &name);
  return StringRef(IFACE_(name));
}

static StringRef attribute_domain_string(const bke::AttrDomain domain)
{
  const char *name = nullptr;
  RNA_enum_name_from_value(rna_enum_attribute_domain_items, int(domain), &name);
  return StringRef(IFACE_(name));
}

static bool attribute_search_item_add(uiSearchItems *items, const GeometryAttributeInfo &item)
{
  std::string search_item_text = fmt::format(
      "{} " UI_MENU_ARROW_SEP "{}" UI_SEP_CHAR_S "{}",
      attribute_domain_string(*item.domain),
      item.name,
      attribute_data_type_string(*bke::attr_type_to_custom_data_type(*item.data_type)));
  return UI_search_item_add(
      items, search_item_text, (void *)&item, ICON_NONE, UI_BUT_HAS_SEP_CHAR, 0);
}

void attribute_search_add_items(StringRef str,
                                const bool can_create_attribute,
                                Span<const GeometryAttributeInfo *> infos,
                                uiSearchItems *seach_items,
                                const bool is_first)
{
  static GeometryAttributeInfo dummy_info;

  /* Any string may be valid, so add the current search string along with the hints. */
  if (!str.is_empty()) {
    bool contained = false;
    for (const GeometryAttributeInfo *attribute_info : infos) {
      if (attribute_info->name == str) {
        contained = true;
        break;
      }
    }
    if (!contained) {
      dummy_info.name = str;
      UI_search_item_add(
          seach_items, str, &dummy_info, can_create_attribute ? ICON_ADD : ICON_NONE, 0, 0);
    }
  }

  if (str.is_empty() && !is_first) {
    /* Allow clearing the text field when the string is empty, but not on the first pass,
     * or opening an attribute field for the first time would show this search item. */
    dummy_info.name = str;
    UI_search_item_add(seach_items, str, &dummy_info, ICON_X, 0, 0);
  }

  /* Don't filter when the menu is first opened, but still run the search
   * so the items are in the same order they will appear in while searching. */
  const StringRef string = is_first ? "" : str;

  ui::string_search::StringSearch<const GeometryAttributeInfo> search;
  for (const GeometryAttributeInfo *item : infos) {
    if (!bke::allow_procedural_attribute_access(item->name)) {
      continue;
    }
    search.add(item->name, item);
  }

  const Vector<const GeometryAttributeInfo *> filtered_items = search.query(string);

  for (const GeometryAttributeInfo *item : filtered_items) {
    if (!attribute_search_item_add(seach_items, *item)) {
      break;
    }
  }
}

}  // namespace blender::ui
