/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include "BKE_global.hh"

#include "BLI_listbase.h"
#include "BLT_translation.hh"

#include "DNA_screen_types.h"

#include "UI_interface_layout.hh"
#include "UI_tree_view.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "userpref_intern.hh"

namespace blender {

struct AnyAssetLibraryDefinition {
  eAssetLibraryType type;
  bUserAssetLibrary *user_library;
};

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

struct AssetLibraryListItem : public ui::AbstractTreeViewItem {
  AnyAssetLibraryDefinition library;
  int index_in_list = 0;

  AssetLibraryListItem(const AnyAssetLibraryDefinition &library, const int index_in_list)
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

  void build_row(ui::Layout &row) override
  {
    const bool is_remote_library = library.user_library &&
                                   (library.user_library->flag & ASSET_LIBRARY_USE_REMOTE_URL);

    if (library.user_library) {
      row.label(label_, is_remote_library ? ICON_INTERNET : ICON_DISK_DRIVE);
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
      row.label("", ICON_ERROR);
    }

    if (library.user_library) {
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
  bool supports_renaming() const override
  {
    return library.user_library != nullptr;
  }
  bool rename(const bContext &C, StringRefNull new_name) override
  {
    PointerRNA ptr = RNA_pointer_create_discrete(
        nullptr, RNA_UserAssetLibrary, library.user_library);
    PropertyRNA *prop = RNA_struct_find_property(&ptr, "name");
    RNA_property_string_set(&ptr, prop, new_name.c_str());
    RNA_property_update(&const_cast<bContext &>(C), &ptr, prop);
    return true;
  }
};

struct AssetLibraryList : public ui::AbstractTreeView {
  void build_tree() override
  {
    this->is_flat_ = true;

    Vector<AnyAssetLibraryDefinition> libraries = userpref_ui_asset_libraries();

    int i = 0;
    for (const AnyAssetLibraryDefinition &library : libraries) {
      add_tree_item<AssetLibraryListItem>(library, i++);
    }
  }
};

static void draw_library_list(const bContext &C, ui::Layout &layout)
{
  ui::Block *block = layout.block();

  ui::AbstractTreeView *tree_view = block_add_view(
      *block, "Asset Libraries Preferences", std::make_unique<AssetLibraryList>());
  tree_view->set_default_rows(5);

  ui::TreeViewBuilder::build_tree_view(C, *tree_view, layout);
}

static void draw_active_library_settings(ui::Layout &layout,
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
    }
    else {
      layout.prop(&library_ptr, "path", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      layout.prop(&library_ptr, "import_method", UI_ITEM_NONE, IFACE_("Import Method"), ICON_NONE);
      layout.prop(&library_ptr, "use_relative_path", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }
}

void userpref_asset_libraries_panel_draw(const bContext *C, Panel *panel)
{
  Vector<AnyAssetLibraryDefinition> libraries = userpref_ui_asset_libraries();

  ui::Layout &layout = *panel->layout;

  ui::Layout &row = layout.row(false);

  draw_library_list(*C, row);

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
  sub.enabled_set(active_idx_in_range &&
                  libraries[U.active_asset_library].type == ASSET_LIBRARY_CUSTOM);
  PointerRNA props = sub.op("preferences.asset_library_remove", "", ICON_REMOVE);
  /* Convert from UI-items list index to #U.asset_libraries index. */
  RNA_int_set(&props, "index", U.active_asset_library - FIXED_ITEMS_COUNT);

  if (!active_idx_in_range) {
    return;
  }

  layout.separator();

  draw_active_library_settings(layout, libraries[U.active_asset_library]);
}

}  // namespace blender
