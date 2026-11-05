/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_blender_project.hh"
#include "BKE_global.hh"
#include "BKE_screen.hh"

#include "BLI_listbase.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_asset_library_ui.hh"

namespace blender::ui {

static Vector<AnyAssetLibraryDefinition> project_ui_asset_libraries()
{
  Vector<AnyAssetLibraryDefinition> result;

  for (bUserAssetLibrary &user_library : U.asset_libraries) {
    if (!(user_library.flag & ASSET_LIBRARY_PROJECT_DEFINED)) {
      /* Only include project defined libraries. */
      continue;
    }

    if (!USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries) &&
        user_library.flag & ASSET_LIBRARY_USE_REMOTE_URL)
    {
      continue;
    }
    result.append(AnyAssetLibraryDefinition{ASSET_LIBRARY_CUSTOM, &user_library});
  }

  return result;
}

struct ProjectAssetLibraryListItem : public AssetLibraryListItemCommon {

  /* Use the constructor from AssetLibraryListItemCommon. */
  using AssetLibraryListItemCommon::AssetLibraryListItemCommon;

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

    if (library.user_library && !(library.user_library->flag & ASSET_LIBRARY_DISABLED) &&
        is_remote_library && !library.user_library->remote_url[0])
    {
      row.label("", ICON_ERROR);
    }

    if (library.user_library && !is_remote_library && library.user_library->invalid_uuid) {
      row.label("", ICON_ERROR);
      row.enabled_set(false);
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

  void on_activate(bContext &C) override
  {
    bke::BlenderProject *project = BKE_blender_project_get(CTX_data_main(&C));
    if (!project) {
      return;
    }
    project->active_asset_library_index = index_in_list;
  }
  std::optional<bool> should_be_active() const override
  {
    const bke::BlenderProject *project = BKE_blender_project_get(G_MAIN);
    if (!project) {
      return std::nullopt;
    }
    return project->active_asset_library_index == index_in_list;
  }
};

void template_project_asset_libraries_tree(Layout *layout, bContext *C)
{
  const bke::BlenderProject *project = BKE_blender_project_get(CTX_data_main(C));
  if (!project) {
    /* The category is always shown, but there is nothing to edit without a project. */
    return;
  }

  Vector<AnyAssetLibraryDefinition> libraries = project_ui_asset_libraries();
  int active_asset_library = project->active_asset_library_index;

  ui::Layout &row = layout->row(false);

  draw_library_list<ProjectAssetLibraryListItem>(
      *C, row, libraries, "Project Asset Library Preferences");

  ui::Layout &col = row.column(true);
  if (USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries)) {
    col.op_menu_enum(C, "project.asset_library_add", "type", "", ICON_ADD);
  }
  else {
    PointerRNA props = col.op("project.asset_library_add", "", ICON_ADD);
    RNA_enum_set(&props, "type", ASSET_LIBRARY_LOCAL);
  }

  ui::Layout &sub = col.row(true);
  const bool active_idx_in_range = active_asset_library >= 0 &&
                                   active_asset_library < libraries.size();
  const bool is_custom_library = active_idx_in_range &&
                                 libraries[active_asset_library].type == ASSET_LIBRARY_CUSTOM;
  sub.enabled_set(active_idx_in_range && is_custom_library);
  PointerRNA props = sub.op("project.asset_library_remove", "", ICON_REMOVE);
  RNA_int_set(&props, "index", active_asset_library);

  if (!active_idx_in_range) {
    return;
  }

  layout->separator();

  draw_active_library_settings(C, *layout, libraries[active_asset_library]);
}

}  // namespace blender::ui
