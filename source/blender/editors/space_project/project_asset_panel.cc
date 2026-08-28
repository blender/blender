/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
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

#include "project_intern.hh"

namespace blender {

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

std::optional<int> project_ui_asset_libraries_index_from_user_library(
    const bUserAssetLibrary &user_library)
{
  int i = 0;

  const Vector<AnyAssetLibraryDefinition> libraries = project_ui_asset_libraries();
  for (const AnyAssetLibraryDefinition &library : libraries) {
    if (library.user_library && library.user_library == &user_library) {
      return i;
    }
    i++;
  }

  return std::nullopt;
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

    if (library.user_library && library.user_library->is_enabled() && is_remote_library &&
        !library.user_library->remote_url[0])
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
               library.user_library->is_enabled() ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT);
    }
  }

  void on_activate(bContext &C) override
  {
    bke::BlenderProject *project = BKE_blender_project_get(CTX_data_main(&C));
    project->active_asset_library_index = index_in_list;
  }
  std::optional<bool> should_be_active() const override
  {
    bke::BlenderProject *project = BKE_blender_project_get(G_MAIN);
    return project->active_asset_library_index == index_in_list;
  }
};

static void project_asset_panel_draw(const bContext *C, Panel *panel)
{
  Vector<AnyAssetLibraryDefinition> libraries = project_ui_asset_libraries();
  bke::BlenderProject *project = BKE_blender_project_get(CTX_data_main(C));
  int active_asset_library = project->active_asset_library_index;

  ui::Layout &layout = *panel->layout;

  ui::Layout &row = layout.row(false);

  draw_library_list<ProjectAssetLibraryListItem>(
      *C, row, libraries, "Project Asset Library Preferences");

  ui::Layout &col = row.column(true);
  if (USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries)) {
    PointerRNA props = col.op("project.asset_library_add", "", ICON_ADD);
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

  layout.separator();

  draw_active_library_settings(C, layout, libraries[active_asset_library]);
}

void project_asset_panel_register(ARegionType &region_type)
{
  PanelType *panel_type = MEM_new_zeroed<PanelType>(__func__);
  STRNCPY_UTF8(panel_type->idname, "PROJECT_PT_asset_libraries");
  STRNCPY_UTF8(panel_type->label, N_("Asset Libraries"));
  STRNCPY_UTF8(panel_type->category, N_("Asset Libraries"));
  STRNCPY_UTF8(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  STRNCPY(panel_type->context, "assets");
  panel_type->space_type = SPACE_PROJECT;
  panel_type->region_type = RGN_TYPE_WINDOW;
  panel_type->draw = project_asset_panel_draw;
  panel_type->order = 10; /* Make sure the category are put after the other base categoies. */
  BLI_addtail(&region_type.paneltypes, panel_type);
}

}  // namespace blender
