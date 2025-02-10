/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <fmt/format.h>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BLO_readfile.hh"

#include "BLT_translation.hh"

#include "BKE_blendfile.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"

#include "IMB_imbuf.hh"
#include "IMB_metadata.hh"
#include "IMB_thumbs.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "interface_intern.hh"

static void uiTemplateRecentFiles_tooltip_func(bContext & /*C*/, uiTooltipData &tip, void *argN)
{
  char *path = (char *)argN;

  /* File name and path. */
  char dirname[FILE_MAX];
  char filename[FILE_MAX];
  BLI_path_split_dir_file(path, dirname, sizeof(dirname), filename, sizeof(filename));
  UI_tooltip_text_field_add(tip, filename, {}, UI_TIP_STYLE_HEADER, UI_TIP_LC_NORMAL);
  UI_tooltip_text_field_add(tip, dirname, {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);

  UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);

  if (!BLI_exists(path)) {
    UI_tooltip_text_field_add(tip, N_("File Not Found"), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_ALERT);
    return;
  }

  /* Blender version. */
  char version_str[128] = {0};
  /* Load the thumbnail from cache if existing, but don't create if not. */
  ImBuf *thumb = IMB_thumb_read(path, THB_LARGE);
  if (thumb) {
    /* Look for version in existing thumbnail if available. */
    IMB_metadata_get_field(
        thumb->metadata, "Thumb::Blender::Version", version_str, sizeof(version_str));
  }

  eFileAttributes attributes = BLI_file_attributes(path);
  if (!version_str[0] && !(attributes & FILE_ATTR_OFFLINE)) {
    /* Load Blender version directly from the file. */
    short version = BLO_version_from_file(path);
    if (version != 0) {
      SNPRINTF(version_str, "%d.%01d", version / 100, version % 100);
    }
  }

  if (version_str[0]) {
    UI_tooltip_text_field_add(
        tip, fmt::format("Blender {}", version_str), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);
    UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
  }

  BLI_stat_t status;
  if (BLI_stat(path, &status) != -1) {
    char date_str[FILELIST_DIRENTRY_DATE_LEN], time_st[FILELIST_DIRENTRY_TIME_LEN];
    bool is_today, is_yesterday;
    std::string day_string;
    BLI_filelist_entry_datetime_to_string(
        nullptr, int64_t(status.st_mtime), false, time_st, date_str, &is_today, &is_yesterday);
    if (is_today || is_yesterday) {
      day_string = (is_today ? N_("Today") : N_("Yesterday")) + std::string(" ");
    }
    UI_tooltip_text_field_add(tip,
                              fmt::format("{}: {}{}{}",
                                          N_("Modified"),
                                          day_string,
                                          (is_today || is_yesterday) ? "" : date_str,
                                          (is_today || is_yesterday) ? time_st : ""),
                              {},
                              UI_TIP_STYLE_NORMAL,
                              UI_TIP_LC_NORMAL);

    if (status.st_size > 0) {
      char size[16];
      BLI_filelist_entry_size_to_string(nullptr, status.st_size, false, size);
      UI_tooltip_text_field_add(
          tip, fmt::format("{}: {}", N_("Size"), size), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);
    }
  }

  if (!thumb) {
    /* try to load from the blend file itself. */
    BlendThumbnail *data = BLO_thumbnail_from_file(path);
    thumb = BKE_main_thumbnail_to_imbuf(nullptr, data);
    if (data) {
      MEM_freeN(data);
    }
  }

  if (thumb) {
    UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
    UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);

    uiTooltipImage image_data;
    float scale = (72.0f * UI_SCALE_FAC) / float(std::max(thumb->x, thumb->y));
    image_data.ibuf = thumb;
    image_data.width = short(float(thumb->x) * scale);
    image_data.height = short(float(thumb->y) * scale);
    image_data.border = true;
    image_data.background = uiTooltipImageBackground::Checkerboard_Themed;
    image_data.premultiplied = true;
    UI_tooltip_image_field_add(tip, image_data);
    IMB_freeImBuf(thumb);
  }
}

int uiTemplateRecentFiles(uiLayout *layout, int rows)
{
  int i = 0;
  LISTBASE_FOREACH_INDEX (RecentFile *, recent, &G.recent_files, i) {
    if (i >= rows) {
      break;
    }

    const char *filename = BLI_path_basename(recent->filepath);
    PointerRNA ptr;
    uiItemFullO(layout,
                "WM_OT_open_mainfile",
                filename,
                BKE_blendfile_extension_check(filename) ? ICON_FILE_BLEND : ICON_FILE_BACKUP,
                nullptr,
                WM_OP_INVOKE_DEFAULT,
                UI_ITEM_NONE,
                &ptr);
    RNA_string_set(&ptr, "filepath", recent->filepath);
    RNA_boolean_set(&ptr, "display_file_selector", false);

    uiBlock *block = uiLayoutGetBlock(layout);
    uiBut *but = ui_but_last(block);
    UI_but_func_tooltip_custom_set(
        but, uiTemplateRecentFiles_tooltip_func, BLI_strdup(recent->filepath), MEM_freeN);
  }

  return i;
}
