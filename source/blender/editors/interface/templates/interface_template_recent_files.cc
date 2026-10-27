/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cmath>

#include <fmt/format.h>

#include "BLI_fileops.hh"
#include "BLI_listbase.hh"
#include "BLI_path_utils.hh"
#include "BLI_set.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"

#include "BLO_readfile.hh"

#include "BLT_date_string.hh"
#include "BLT_lang.hh"
#include "BLT_translation.hh"

#include "BKE_autoexec.hh"
#include "BKE_blender_project.hh"
#include "BKE_blendfile.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"

#include "IMB_imbuf.hh"
#include "IMB_metadata.hh"
#include "IMB_thumbs.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"

#include "UI_resources.hh"
#include "WM_api.hh"
#include "WM_types.hh"

#include "interface_intern.hh"

namespace blender::ui {

static void template_recent_files_tooltip_func(bContext & /*C*/,
                                               TooltipData &tip,
                                               Button * /*but*/,
                                               void *argN)
{
  char *path = static_cast<char *>(argN);

  /* File name and path. */
  char dirname[FILE_MAX];
  char filename[FILE_MAX];
  BLI_path_split_dir_file(path, dirname, sizeof(dirname), filename, sizeof(filename));
  tooltip_text_field_add(tip, filename, {}, TIP_STYLE_HEADER, TIP_LC_NORMAL);
  tooltip_text_field_add(tip, dirname, {}, TIP_STYLE_NORMAL, TIP_LC_VALUE);

  tooltip_text_field_add(tip, {}, {}, TIP_STYLE_SPACER, TIP_LC_NORMAL);

  if (!BLI_exists(path)) {
    tooltip_text_field_add(tip, TIP_("File not found"), {}, TIP_STYLE_NORMAL, TIP_LC_ALERT);
    return;
  }

  /* Blender version. */
  char version_str[128] = {0};
  /* Load the thumbnail from cache if existing, but don't create if not. */
  ImBuf *thumb = IMB_thumb_read(path, THB_LARGE);
  if (thumb) {
    /* Look for version in existing thumbnail if available. */
    IMB_metadata_get_field(
        thumb->metadata(), "Thumb::Blender::Version", version_str, sizeof(version_str));
  }

  eFileAttributes attributes = BLI_file_attributes(path);
  if (!version_str[0] && !(attributes & FILE_ATTR_OFFLINE)) {
    /* Load Blender version directly from the file. */
    short version = BLO_version_from_file(path);
    if (version != 0) {
      SNPRINTF_UTF8(version_str, "%d.%01d", version / 100, version % 100);
    }
  }

  if (version_str[0]) {
    tooltip_text_field_add(
        tip, fmt::format("Blender {}", version_str), {}, TIP_STYLE_NORMAL, TIP_LC_VALUE);
  }

  BLI_stat_t status;
  if (BLI_stat(path, &status) != -1) {
    if (status.st_size > 0) {
      char size[16];
      BLI_filelist_entry_size_to_string(nullptr, status.st_size, false, size);
      tooltip_text_field_add(tip,
                             fmt::format(fmt::runtime(TIP_("Size: {}")), size),
                             {},
                             TIP_STYLE_NORMAL,
                             TIP_LC_VALUE);
    }

    const tm mod_time = date_string::localtime_safe(status.st_mtime);
    const time_t ts_now = time(nullptr);
    const tm now = date_string::localtime_safe(ts_now);
    const char *lang = BLT_lang_get();
    std::string modified_s = date_string::datetime(mod_time,
                                                   lang,
                                                   date_string::DateFormat(U.date_format),
                                                   date_string::TimeFormat(U.time_format),
                                                   &now,
                                                   TIP_("Today"),
                                                   TIP_("Yesterday"));
    tooltip_text_field_add(tip,
                           fmt::format(fmt::runtime(TIP_("Modified: {}")), modified_s),
                           {},
                           TIP_STYLE_NORMAL,
                           TIP_LC_VALUE);
  }

  if (!thumb) {
    /* try to load from the blend file itself. */
    BlendThumbnail *data = BLO_thumbnail_from_file(path);
    thumb = BKE_main_thumbnail_to_imbuf(nullptr, data);
    if (data) {
      MEM_delete(data);
    }
  }

  if (thumb) {
    tooltip_text_field_add(tip, {}, {}, TIP_STYLE_SPACER, TIP_LC_NORMAL);
    tooltip_text_field_add(tip, {}, {}, TIP_STYLE_SPACER, TIP_LC_NORMAL);

    TooltipImage image_data;
    float scale = (72.0f * UI_SCALE_FAC) / float(std::max(thumb->x, thumb->y));
    image_data.ibuf = thumb;
    image_data.width = short(float(thumb->x) * scale);
    image_data.height = short(float(thumb->y) * scale);
    image_data.border = true;
    image_data.background = TooltipImageBackground::Checkerboard_Themed;
    image_data.premultiplied = true;
    tooltip_image_field_add(tip, image_data);
    IMB_freeImBuf(thumb);
  }
}

struct RecentFilesList {
  Vector<std::string> project_files;
  Vector<std::string> other_files;
};

static RecentFilesList recent_files_get(const std::optional<std::string> &project_root,
                                        const int rows,
                                        const int project_rows)
{
  /* In a project, reserve rows for the most recent project files. */
  Set<const RecentFile *> shown;
  if (project_root) {
    for (const RecentFile &recent : G.recent_files) {
      if (shown.size() >= project_rows) {
        break;
      }
      if (BLI_path_contains(project_root->c_str(), recent.filepath)) {
        shown.add(&recent);
      }
    }
  }

  /* Fill remaining rows with the most recent other files, in the project or not. */
  int64_t remaining = rows - shown.size();
  for (const RecentFile &recent : G.recent_files) {
    if (remaining <= 0) {
      break;
    }
    if (shown.add(&recent)) {
      remaining--;
    }
  }

  /* Project files first, each group most recent first. */
  RecentFilesList list;
  for (const RecentFile &recent : G.recent_files) {
    if (!shown.contains(&recent)) {
      continue;
    }
    if (project_root && BLI_path_contains(project_root->c_str(), recent.filepath)) {
      list.project_files.append(recent.filepath);
    }
    else {
      list.other_files.append(recent.filepath);
    }
  }
  return list;
}

static void template_recent_file(Layout *layout, const std::string &filepath)
{
  const char *filename = BLI_path_basename(filepath.c_str());
  PointerRNA ptr = layout->op("WM_OT_open_mainfile",
                              filename,
                              BKE_blendfile_extension_check(filename) ? ICON_FILE_BLEND :
                                                                        ICON_FILE_BACKUP,
                              wm::OpCallContext::InvokeDefault,
                              UI_ITEM_NONE);
  RNA_string_set(&ptr, "filepath", filepath.c_str());
  RNA_boolean_set(&ptr, "display_file_selector", false);
  RNA_boolean_set(&ptr,
                  "use_scripts",
                  BKE_autoexec_default_trust_source(filepath.c_str(),
                                                    {
                                                        .skip_overrides = false,
                                                        .canonicalize = true,
                                                        .strip_filename = true,
                                                    }));

  Block *block = layout->block();
  Button *but = button_last(block);
  button_func_tooltip_custom_set(
      but, template_recent_files_tooltip_func, BLI_strdup(filepath.c_str()), MEM_delete_void);
}

int template_recent_files(Layout *layout,
                          int rows,
                          const RecentFilesFilter filter,
                          const float project_ratio)
{
  const std::optional<std::string> project_root = BKE_blender_project_read_callback(
      G_MAIN, [](const bke::BlenderProject *project) -> std::optional<std::string> {
        if (project == nullptr) {
          return std::nullopt;
        }
        return project->get_root_path();
      });

  const int project_rows = int(std::ceil(rows * project_ratio));
  const RecentFilesList list = recent_files_get(project_root, rows, project_rows);

  int found = 0;
  if (filter != RecentFilesFilter::Other) {
    for (const std::string &filepath : list.project_files) {
      template_recent_file(layout, filepath);
      found++;
    }
  }
  if (filter != RecentFilesFilter::Project) {
    for (const std::string &filepath : list.other_files) {
      template_recent_file(layout, filepath);
      found++;
    }
  }
  return found;
}

static void template_recent_projects_tooltip_func(bContext & /*C*/,
                                                  TooltipData &tip,
                                                  Button * /*but*/,
                                                  void *argN)
{
  const char *path = static_cast<const char *>(argN);
  tooltip_text_field_add(tip, path, {}, TIP_STYLE_NORMAL, TIP_LC_NORMAL);

  if (!BKE_blender_project_dir_is_project(path)) {
    tooltip_text_field_add(tip, {}, {}, TIP_STYLE_SPACER, TIP_LC_NORMAL);
    tooltip_text_field_add(tip, TIP_("Project Not Found"), {}, TIP_STYLE_NORMAL, TIP_LC_ALERT);
  }
}

int template_recent_projects(Layout *layout, int rows)
{
  wmOperatorType *ot = WM_operatortype_find("PROJECT_OT_open", false);

  const std::string active_path = BKE_blender_project_read_callback(
      G_MAIN, [](const bke::BlenderProject *project) -> std::string {
        return project ? project->get_root_path() : "";
      });

  int i = 0;
  for (const RecentProject &project : BKE_blender_project_recent_get()) {
    if (i >= rows) {
      break;
    }

    if (ot) {
      const bool is_active = !active_path.empty() &&
                             BLI_path_cmp_normalized(project.path.c_str(), active_path.c_str()) ==
                                 0;
      PointerRNA ptr = layout->op(ot,
                                  project.name,
                                  is_active ? ICON_PROJECT : ICON_NONE,
                                  wm::OpCallContext::InvokeDefault,
                                  UI_ITEM_NONE);
      RNA_string_set(&ptr, "directory", project.path.c_str());

      Block *block = layout->block();
      Button *but = button_last(block);
      button_func_tooltip_custom_set(but,
                                     template_recent_projects_tooltip_func,
                                     BLI_strdup(project.path.c_str()),
                                     MEM_delete_void);
      if (is_active) {
        button_disable(but, TIP_("Project is already open"));
      }
    }
    else {
      layout->label(project.name, ICON_NONE);
    }
    i++;
  }

  return i;
}

}  // namespace blender::ui
