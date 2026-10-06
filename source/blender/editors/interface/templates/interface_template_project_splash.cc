/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <fmt/format.h>

#include "BLI_fileops.hh"

#include "BKE_blender_project.hh"
#include "BKE_context.hh"

#include "BLT_translation.hh"

#include "IMB_imbuf.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"

namespace blender::ui {

void template_project_splash(Layout *layout, bContext *C)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA project_ptr = BKE_blender_project_write_callback(
      bmain, [](bke::BlenderProject *project) {
        return RNA_pointer_create_discrete(nullptr, RNA_BlenderProject, project);
      });
  if (project_ptr.data == nullptr) {
    return;
  }

  const bool has_path = BKE_blender_project_read_callback(
      bmain, [](const bke::BlenderProject *project) {
        return project && !project->get_splash_image_path().is_empty();
      });
  if (!has_path) {
    layout->op("PROJECT_OT_splash_image_create", IFACE_("Create from Image..."), ICON_FILEBROWSER);
    return;
  }

  /* Read-only path, as a custom property to hide the automatic open button
   * and replace it with our own operator. */
  Layout &row = layout->row(true);
  Layout &path_row = row.row(true);
  path_row.enabled_set(false);
  Block *block = layout->block();
  block_layout_set_current(block, &path_row);
  uiDefButR(block,
            ButtonType::Text,
            "",
            0,
            0,
            UI_UNIT_X * 10,
            UI_UNIT_Y,
            &project_ptr,
            "splash_image_path",
            0,
            0,
            0,
            std::nullopt);
  row.op("PROJECT_OT_splash_image_create", "", ICON_FILEBROWSER);
  row.op("PROJECT_OT_splash_image_clear", "", ICON_X);

  layout->separator();

  const std::string filepath = WM_project_splash_image_filepath(bmain);

  BLI_stat_t stat;
  if (!filepath.empty() && BLI_stat(filepath.c_str(), &stat) == 0) {
    /* Reload the image when the file changes. */
    const std::string key = fmt::format(
        "{}:{}:{}", filepath, int64_t(stat.st_mtime), int64_t(stat.st_size));
    const bool loaded = layout->image(key, [&]() -> ImBuf * {
      ImBuf *ibuf = WM_project_splash_image_load(filepath, 1000, true);
      if (ibuf) {
        IMB_premultiply_alpha(ibuf);
      }
      return ibuf;
    });
    if (loaded) {
      return;
    }
  }

  layout->label(IFACE_("Failed to load splash image"), ICON_ERROR);
}

}  // namespace blender::ui
