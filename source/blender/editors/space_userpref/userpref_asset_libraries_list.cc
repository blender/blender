/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include "BKE_blender_project.hh"
#include "BKE_global.hh"
#include "BKE_preferences.h"

#include "BLI_listbase.hh"
#include "BLT_translation.hh"

#include "DNA_screen_types.h"

#include "UI_interface_layout.hh"
#include "UI_tree_view.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "ED_asset_library_ui.hh"
#include "ED_userpref.hh"

#include "userpref_intern.hh"

namespace blender {

constexpr int FIXED_ITEMS_COUNT = 2;

static Vector<AnyAssetLibraryDefinition> userpref_ui_asset_libraries()
{
  Vector<AnyAssetLibraryDefinition> result;

  result.append(AnyAssetLibraryDefinition{ASSET_LIBRARY_ALL, nullptr});
  result.append(AnyAssetLibraryDefinition{ASSET_LIBRARY_ESSENTIALS, nullptr});

  BLI_assert(result.size() == FIXED_ITEMS_COUNT);

  Vector<bUserAssetLibrary *> extension_libraries;
  for (bUserAssetLibrary &user_library : U.asset_libraries) {
    if (!USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries) &&
        user_library.flag & ASSET_LIBRARY_USE_REMOTE_URL)
    {
      continue;
    }
    if (BKE_preferences_extension_asset_library_repo_get(&U, &user_library)) {
      extension_libraries.append(&user_library);
      continue;
    }
    result.append(AnyAssetLibraryDefinition{ASSET_LIBRARY_CUSTOM, &user_library});
  }

  /* Extension libraries are listed under their repository. */
  for (bUserExtensionRepo &repo : U.extension_repos) {
    bool has_repo_row = false;
    for (bUserAssetLibrary *user_library : extension_libraries) {
      if (BKE_preferences_extension_asset_library_repo_get(&U, user_library) != &repo) {
        continue;
      }
      if (!has_repo_row) {
        result.append(AnyAssetLibraryDefinition{ASSET_LIBRARY_CUSTOM, nullptr, &repo});
        has_repo_row = true;
      }
      result.append(AnyAssetLibraryDefinition{ASSET_LIBRARY_CUSTOM, user_library, &repo});
    }
  }

  return result;
}

int userpref_ui_asset_libraries_count()
{
  /* Instead of constructing the vector (potentially allocating memory), just count the list items
   * and use the fixed item count. */
  if (USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries)) {
    int count = U.asset_libraries.count() + FIXED_ITEMS_COUNT;
    /* A row for each repository with extension libraries. */
    for (bUserExtensionRepo &repo : U.extension_repos) {
      for (bUserAssetLibrary &user_library : U.asset_libraries) {
        if (BKE_preferences_extension_asset_library_repo_get(&U, &user_library) == &repo) {
          count++;
          break;
        }
      }
    }
    BLI_assert(count == userpref_ui_asset_libraries().size());
    return count;
  }

  /* In case remote libraries are disabled, just retrieve the count from the available items. */
  return userpref_ui_asset_libraries().size();
}

static std::optional<int> userpref_ui_asset_libraries_index_from_user_library(
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

void AssetLibraryProjectHeading::build_row(ui::Layout &row)
{
  bke::BlenderProject *project = BKE_blender_project_get(G_MAIN);
  if (project) {
    row.label(project->get_name(), ICON_PROJECT);
  }
}

bUserAssetLibrary *ED_userpref_asset_library_active_get()
{
  const Vector<AnyAssetLibraryDefinition> libraries = userpref_ui_asset_libraries();
  if (!libraries.index_range().contains(U.active_asset_library)) {
    return nullptr;
  }
  return libraries[U.active_asset_library].user_library;
}

void ED_userpref_asset_library_active_set(const bUserAssetLibrary &asset_library)
{
  const std::optional<int> index = userpref_ui_asset_libraries_index_from_user_library(
      asset_library);
  if (index) {
    U.active_asset_library = *index;
  }
}

/** Extension libraries are only used while the repository defining them is enabled. */
static bool asset_library_repo_is_disabled(const AnyAssetLibraryDefinition &library)
{
  return library.extension_repo &&
         (library.extension_repo->flag & USER_EXTENSION_REPO_FLAG_DISABLED);
}

struct AssetLibraryListItem : public AssetLibraryListItemCommon {

  using AssetLibraryListItemCommon::AssetLibraryListItemCommon;

  void build_row(ui::Layout &row) override
  {
    const bool is_remote_library = library.user_library &&
                                   (library.user_library->flag & ASSET_LIBRARY_USE_REMOTE_URL);
    const bool project_library = library.user_library &&
                                 (library.user_library->flag & ASSET_LIBRARY_PROJECT_DEFINED);

    /* Draw grayed out, the "enabled" setting stays editable, it just has no effect. */
    if (asset_library_repo_is_disabled(library)) {
      row.active_set(false);
    }

    if (library.user_library) {
      int icon = is_remote_library ? ICON_INTERNET : ICON_DISK_DRIVE;
      if (BKE_preferences_asset_library_owner_get(library.user_library) ==
          bUserAssetLibraryOwner::Extension)
      {
        icon = ICON_EXTENSION;
      }
      row.label(label_, icon);

      /* Disable row if asset library is project defined. */
      row.enabled_set(!project_library);
    }
    else {
      row.label(label_, ICON_NONE);

      ui::Layout &sub = row.row(true);
      /* Draw text grayed out. */
      sub.active_set(false);
      sub.alignment_set(ui::LayoutAlign::Right);
      sub.label(IFACE_("Built-In"), ICON_NONE);
    }

    if (library.user_library && !(library.user_library->flag & ASSET_LIBRARY_DISABLED) &&
        is_remote_library && !library.user_library->remote_url[0])
    {
      row.label("", ICON_STATUS_ERROR);
    }

    if (library.user_library) {
      PointerRNA ptr = RNA_pointer_create_discrete(
          nullptr, RNA_UserAssetLibrary, library.user_library);
      row.prop(&ptr,
               "enabled",
               UI_ITEM_NONE,
               "",
               (library.user_library->flag & ASSET_LIBRARY_DISABLED) ? ICON_CHECKBOX_DEHLT :
                                                                       ICON_CHECKBOX_HLT);
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

/** The collapsible row grouping the libraries of an extension repository. */
struct AssetLibraryRepoItem : public AssetLibraryListItem {

  using AssetLibraryListItem::AssetLibraryListItem;

  void build_row(ui::Layout &row) override
  {
    row.label(label_, ICON_NONE);

    if (asset_library_repo_is_disabled(library)) {
      row.active_set(false);
      ui::Layout &sub = row.row(true);
      /* Draw text grayed out. */
      sub.alignment_set(ui::LayoutAlign::Right);
      sub.label(IFACE_("Disabled"), ICON_NONE);
    }
  }

  bool matches_single(const ui::AbstractTreeViewItem &other) const override
  {
    /* Match by the repository, the name isn't stable across renaming. */
    const auto *other_item = dynamic_cast<const AssetLibraryRepoItem *>(&other);
    return other_item && (library.extension_repo == other_item->library.extension_repo);
  }

  std::optional<bool> should_be_collapsed() const override
  {
    const bUserExtensionRepo &repo = *library.extension_repo;
    return (repo.flag & USER_EXTENSION_REPO_FLAG_ASSET_LIBRARIES_COLLAPSED) != 0;
  }

  bool set_collapsed(const bool collapsed) override
  {
    if (!AbstractTreeViewItem::set_collapsed(collapsed)) {
      return false;
    }
    /* NOTE: also set for changes made by the tree view, user changes set it again through RNA. */
    SET_FLAG_FROM_TEST(library.extension_repo->flag,
                       collapsed,
                       USER_EXTENSION_REPO_FLAG_ASSET_LIBRARIES_COLLAPSED);
    return true;
  }

  void on_collapse_change(bContext &C, const bool is_collapsed) override
  {
    /* Set through RNA so the preferences are tagged as changed. */
    PointerRNA repo_ptr = RNA_pointer_create_discrete(
        nullptr, RNA_UserExtensionRepo, library.extension_repo);
    PropertyRNA *prop = RNA_struct_find_property(&repo_ptr, "show_expanded");
    RNA_property_boolean_set(&repo_ptr, prop, !is_collapsed);
    RNA_property_update(&C, &repo_ptr, prop);
  }
};

void userpref_asset_libraries_panel_draw(const bContext *C, Panel *panel)
{
  Vector<AnyAssetLibraryDefinition> libraries = userpref_ui_asset_libraries();

  ui::Layout &layout = *panel->layout;

  ui::Layout &row = layout.row(false);

  draw_library_list<AssetLibraryListItem, AssetLibraryRepoItem>(
      *C, row, libraries, "Asset Libraries Preferences");

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
  const bUserAssetLibrary *active_library = active_idx_in_range ?
                                                libraries[U.active_asset_library].user_library :
                                                nullptr;
  const bool is_project_library = active_library &&
                                  (BKE_preferences_asset_library_owner_get(active_library) ==
                                   bUserAssetLibraryOwner::Project);
  /* Only user libraries can be removed here, extension libraries are removed by uninstalling
   * the extension & project libraries from the project settings. */
  sub.enabled_set(active_library && (BKE_preferences_asset_library_owner_get(active_library) ==
                                     bUserAssetLibraryOwner::User));
  PointerRNA props = sub.op("preferences.asset_library_remove", "", ICON_REMOVE);
  if (active_library) {
    RNA_int_set(&props, "index", BKE_preferences_asset_library_get_index(&U, active_library));
  }

  if (!active_idx_in_range) {
    return;
  }

  if (is_project_library) {
    ui::Layout &label_row = layout.row(false);
    label_row.label(IFACE_("Edit Project asset libraries in Project Settings."), ICON_NONE);
    ui::Layout &operator_row = label_row.row(false);
    operator_row.alignment_set(ui::LayoutAlign::Right);
    operator_row.op("SCREEN_OT_project_settings_show",
                    "Open Project Settings",
                    ICON_PROJECT,
                    wm::OpCallContext::InvokeDefault,
                    UI_ITEM_NONE);
  }
  else {
    layout.separator();
  }
  if (libraries[U.active_asset_library].is_extension_repo()) {
    layout.label(IFACE_("Asset libraries installed by extensions from this repository."),
                 ICON_NONE);
    return;
  }
  ui::Layout &settings_row = layout.column(false);
  settings_row.enabled_set(!is_project_library);
  draw_active_library_settings(C, settings_row, libraries[U.active_asset_library]);
}

}  // namespace blender
