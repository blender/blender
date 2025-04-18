/* SPDX-FileCopyrightText: 2025 Blender Authors
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

#include "NOD_geometry_nodes_log.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_string_search.hh"

#include <fmt/format.h>

namespace blender::ui {

void grease_pencil_layer_search_add_items(const StringRef str,
                                          const Span<const std::string *> layer_names,
                                          uiSearchItems &seach_items,
                                          const bool is_first)
{
  static std::string dummy_str;

  /* Any string may be valid, so add the current search string along with the hints. */
  if (!str.is_empty()) {
    bool contained = false;
    for (const std::string *name : layer_names) {
      if (name != nullptr && str == *name) {
        contained = true;
      }
    }
    if (!contained) {
      dummy_str = str;
      UI_search_item_add(&seach_items, str, &dummy_str, ICON_NONE, 0, 0);
    }
  }

  if (str.is_empty() && !is_first) {
    /* Allow clearing the text field when the string is empty, but not on the first pass,
     * or opening a layer name field for the first time would show this search item. */
    dummy_str = str;
    UI_search_item_add(&seach_items, str, &dummy_str, ICON_X, 0, 0);
  }

  /* Don't filter when the menu is first opened, but still run the search
   * so the items are in the same order they will appear in while searching. */
  const StringRef string = is_first ? "" : str;

  ui::string_search::StringSearch<const std::string> search;
  for (const std::string *name : layer_names) {
    search.add(*name, name);
  }

  const Vector<const std::string *> filtered_names = search.query(string);
  for (const std::string *name : filtered_names) {
    if (!UI_search_item_add(&seach_items, *name, (void *)name, ICON_NONE, UI_BUT_HAS_SEP_CHAR, 0))
    {
      break;
    }
  }
}

}  // namespace blender::ui
