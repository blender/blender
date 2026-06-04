/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "AS_asset_library.hh"

#include "BKE_global.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "file_banner.hh"
#include "file_intern.hh"
#include "filelist.hh"

namespace blender {

static bool file_banner_remote_asset_libraries_poll(const SpaceFile &sfile)
{
  const bool is_online_allowed = G.f & G_FLAG_INTERNET_ALLOW;
  const bool was_choice_made = U.extension_flag & USER_EXTENSION_FLAG_ONLINE_ACCESS_HANDLED;
  if (is_online_allowed || was_choice_made) {
    return false;
  }

  /* With remote libraries, there may be already-downloaded assets available that should be
   * displayed. Don't show the "internet access required" hint until done loading, and only if
   * there are no already-downloaded assets to display. */
  if (!filelist_is_ready(sfile.files) || !filelist_files_num_entries(sfile.files)) {
    return false;
  }

  const FileAssetSelectParams *asset_params = ED_fileselect_get_asset_params(&sfile);
  if (!asset_params) {
    return false;
  }
  if (!asset_system::is_or_contains_remote_libraries(asset_params->asset_library_ref)) {
    return false;
  }

  return true;
}

static void file_banner_remote_asset_libraries_layout(const SpaceFile & /*sfile*/,
                                                      ui::Layout &layout)
{
  layout.label(IFACE_("Internet access disabled, only showing assets that are available offline."),
               ICON_INTERNET_OFFLINE);

  ui::Layout &right_row = layout.row(false);
  right_row.alignment_set(ui::LayoutAlign::Right);

  PointerRNA props = right_row.op("WM_OT_context_set_boolean", IFACE_("Continue Offline"), ICON_X);
  RNA_string_set(&props, "data_path", "preferences.extensions.use_online_access_handled");
  RNA_boolean_set(&props, "value", true);

  right_row.op("extensions.userpref_allow_online", IFACE_("Allow Online Access"), ICON_CHECKMARK);
}

BannerType remote_libraries_online_access_required_banner{
    .poll = file_banner_remote_asset_libraries_poll,
    .layout = file_banner_remote_asset_libraries_layout,
};

}  // namespace blender
