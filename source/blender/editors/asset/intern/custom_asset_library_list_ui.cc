/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include "BKE_global.hh"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"

#include "UI_interface_layout.hh"
#include "UI_tree_view.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "ED_asset_library_ui.hh"

namespace blender {

AssetLibraryListItemCommon::AssetLibraryListItemCommon(const AnyAssetLibraryDefinition &library,
                                                       const int index_in_list)
    : library(library), index_in_list(index_in_list)
{

  if (library.user_library) {
    label_ = library.user_library->name;
  }
  else {
    const char *name_cstr;
    RNA_enum_name_gettexted(
        rna_enum_asset_library_type_items, library.type, BLT_I18NCONTEXT_DEFAULT, &name_cstr);
    label_ = name_cstr;
  }
}

bool AssetLibraryListItemCommon::supports_renaming() const
{
  return library.user_library != nullptr;
}

bool AssetLibraryListItemCommon::rename(const bContext &C, StringRefNull new_name)
{
  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, RNA_UserAssetLibrary, library.user_library);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "name");
  RNA_property_string_set(&ptr, prop, new_name.c_str());
  RNA_property_update(&const_cast<bContext &>(C), &ptr, prop);
  return true;
}

void draw_active_library_settings(const bContext *C,
                                  ui::Layout &layout,
                                  const AnyAssetLibraryDefinition &library)
{
  if (library.type == ASSET_LIBRARY_ESSENTIALS) {
    PointerRNA prefs_ptr = RNA_pointer_create_discrete(nullptr, RNA_PreferencesAssetLibraries, &U);

    ui::Layout &row = layout.row(false);
    row.active_set((G.f & G_FLAG_INTERNET_ALLOW) != 0);
    row.prop(&prefs_ptr,
             "use_online_essentials",
             UI_ITEM_NONE,
             IFACE_("Include Online Essentials"),
             ICON_NONE);
  }

  if (library.user_library) {
    PointerRNA library_ptr = RNA_pointer_create_discrete(
        nullptr, RNA_UserAssetLibrary, library.user_library);

    if (library.user_library->flag & ASSET_LIBRARY_USE_REMOTE_URL) {
      if (USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries)) {
        ui::Layout &row = layout.row(false);
        row.red_alert_set(!library.user_library->remote_url[0]);
        row.prop(&library_ptr,
                 RNA_struct_find_property(&library_ptr, "remote_url"),
                 RNA_NO_INDEX,
                 0,
                 UI_ITEM_NONE,
                 "",
                 ICON_INTERNET,
                 IFACE_("Repository URL"));
      }
      layout.prop(&library_ptr, "import_method", UI_ITEM_NONE, IFACE_("Import Method"), ICON_NONE);

      if (ui::Layout *panel = layout.panel(C, "advanced", true, IFACE_("Advanced"))) {
        panel->use_property_split_set(true);
        ui::Layout &col = panel->column(true, IFACE_("Authentication"));
        col.prop(&library_ptr, "use_auth_token", UI_ITEM_NONE, std::nullopt, ICON_NONE);

        if (library.user_library->flag & ASSET_LIBRARY_USE_AUTH_TOKEN) {
          if (!library.user_library->auth_token) {
            col.red_alert_set(true);
          }
          col.prop(&library_ptr,
                   RNA_struct_find_property(&library_ptr, "auth_token"),
                   RNA_NO_INDEX,
                   0,
                   UI_ITEM_NONE,
                   IFACE_("Secret"),
                   library.user_library->auth_token ? ICON_LOCKED : ICON_UNLOCKED,
                   std::nullopt);
        }
      }
    }
    else {
      if (library.user_library->invalid_uuid) {
        ui::Layout &row = layout.row(false);
        row.red_alert_set(true);
        row.label_multiline(
            "Asset Library has an invalid identifier (invalid UUID). Either manually edit the "
            "configuration file or regenerate it here.",
            ICON_ERROR,
            ui::UI_STYLE_TEXT_LEFT,
            7);
        ui::Layout &row2 = row.row(false);
        row2.alignment_set(ui::LayoutAlign::Left);
        row2.red_alert_set(false);
        row2.button("Regenerate UUID", 0, [library](blender::bContext & /*C*/) {
          MEM_delete(library.user_library->invalid_uuid);
          library.user_library->invalid_uuid = nullptr;
        });
      }

      layout.prop(&library_ptr, "path", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout.prop(&library_ptr, "import_method", UI_ITEM_NONE, IFACE_("Import Method"), ICON_NONE);
      layout.prop(&library_ptr, "use_relative_path", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }
}

}  // namespace blender
