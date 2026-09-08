/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include "BKE_global.hh"

#include "BLI_listbase.hh"
#include "BLT_translation.hh"

#include "DNA_screen_types.h"

#include "UI_interface_layout.hh"
#include "UI_tree_view.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "ED_asset_library_ui.hh"

#include "userpref_intern.hh"

namespace blender {

constexpr int FIXED_ITEMS_COUNT = 2;

static Vector<AnyAssetLibraryDefinition> userpref_ui_asset_libraries()
{
  Vector<AnyAssetLibraryDefinition> result;

  result.append(AnyAssetLibraryDefinition{ASSET_LIBRARY_ALL, nullptr});
  result.append(AnyAssetLibraryDefinition{ASSET_LIBRARY_ESSENTIALS, nullptr});

  BLI_assert(result.size() == FIXED_ITEMS_COUNT);

  for (bUserAssetLibrary &user_library : U.asset_libraries) {
    if (!USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries) &&
        user_library.flag & ASSET_LIBRARY_USE_REMOTE_URL)
    {
      continue;
    }
    result.append(AnyAssetLibraryDefinition{ASSET_LIBRARY_CUSTOM, &user_library});
  }

  return result;
}

int userpref_ui_asset_libraries_count()
{
  /* Instead of constructing the vector (potentially allocating memory), just count the list items
   * and use the fixed item count. */
  if (USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries)) {
    const int count = U.asset_libraries.count() + FIXED_ITEMS_COUNT;
    BLI_assert(count == userpref_ui_asset_libraries().size());
    return count;
  }

  /* In case remote libraries are disabled, just retrieve the count from the available items. */
  return userpref_ui_asset_libraries().size();
}

std::optional<int> userpref_ui_asset_libraries_index_from_user_library(
    const bUserAssetLibrary &user_library)
{
  int i = 0;

  const Vector<AnyAssetLibraryDefinition> libraries = userpref_ui_asset_libraries();
  for (const AnyAssetLibraryDefinition &library : libraries) {
    if (library.user_library && library.user_library == &user_library) {
      return i;
    }
    i++;
  }

  return std::nullopt;
}

struct AssetLibraryListItem : public AssetLibraryListItemCommon {

  /* Use the constructor from AssetLibraryListItemCommon. */
  using AssetLibraryListItemCommon::AssetLibraryListItemCommon;

  void build_row(ui::Layout &row) override
  {
    const bool is_remote_library = library.user_library &&
                                   (library.user_library->flag & ASSET_LIBRARY_USE_REMOTE_URL);
    const bool project_library = library.user_library &&
                                 (library.user_library->flag & ASSET_LIBRARY_PROJECT_DEFINED);

    if (library.user_library) {
      row.label(label_, is_remote_library ? ICON_INTERNET : ICON_DISK_DRIVE);

      if (project_library) {
        row.active_set(false);
        ui::Layout &sub = row.row(true);
        /* Draw text grayed out. */
        sub.alignment_set(ui::LayoutAlign::Right);
        sub.label(IFACE_("Project Defined"), ICON_NONE);
      }
    }
    else {
      row.label(label_, ICON_NONE);

      ui::Layout &sub = row.row(true);
      /* Draw text grayed out. */
      sub.active_set(false);
      sub.alignment_set(ui::LayoutAlign::Right);
      sub.label(IFACE_("Built-In"), ICON_NONE);
    }

    if (library.user_library && library.user_library->is_enabled() && is_remote_library &&
        !library.user_library->remote_url[0])
    {
      row.label("", ICON_STATUS_ERROR);
    }

    if (library.user_library && !project_library) {
      PointerRNA ptr = RNA_pointer_create_discrete(
          nullptr, RNA_UserAssetLibrary, library.user_library);
      row.prop(&ptr,
               "enabled",
               UI_ITEM_NONE,
               "",
               library.user_library->is_enabled() ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT);
    }
  }

  void on_activate(bContext & /*C*/) override
  {
    U.active_asset_library = index_in_list;
  }
  std::optional<bool> should_be_active() const override
  {
    return U.active_asset_library == index_in_list;
  }
};

void userpref_asset_libraries_panel_draw(const bContext *C, Panel *panel)
{
  Vector<AnyAssetLibraryDefinition> libraries = userpref_ui_asset_libraries();

  ui::Layout &layout = *panel->layout;

  ui::Layout &row = layout.row(false);

  draw_library_list<AssetLibraryListItem>(*C, row, libraries, "Asset Libraries Preferences");

  ui::Layout &col = row.column(true);
  if (USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries)) {
    col.op_menu_enum(C, "preferences.asset_library_add", "type", "", ICON_ADD);
  }
  else {
    PointerRNA props = col.op("preferences.asset_library_add", "", ICON_ADD);
    RNA_enum_set(&props, "type", ASSET_LIBRARY_LOCAL);
  }

  ui::Layout &sub = col.row(true);
  const bool active_idx_in_range = U.active_asset_library >= 0 &&
                                   U.active_asset_library < libraries.size();
  const bool is_custom_library = libraries[U.active_asset_library].type == ASSET_LIBRARY_CUSTOM;
  const bool is_project_library = is_custom_library &&
                                  (libraries[U.active_asset_library].user_library->flag &
                                   ASSET_LIBRARY_PROJECT_DEFINED);
  sub.enabled_set(active_idx_in_range && is_custom_library && !is_project_library);
  PointerRNA props = sub.op("preferences.asset_library_remove", "", ICON_REMOVE);
  /* Convert from UI-items list index to #U.asset_libraries index. */
  RNA_int_set(&props, "index", U.active_asset_library - FIXED_ITEMS_COUNT);

  if (!active_idx_in_range) {
    return;
  }

  layout.separator();

  if (is_project_library) {
    layout.label(IFACE_("Settings of project asset libraries can be edited in Project Setup."),
                 ICON_NONE);
    return;
  }
  draw_active_library_settings(C, layout, libraries[U.active_asset_library]);
}

}  // namespace blender
