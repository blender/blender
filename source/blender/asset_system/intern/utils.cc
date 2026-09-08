/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

/* For PATH_MAX (at least on Windows). */
#include "BLI_fileops.hh"  // IWYU pragma: keep
#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "MEM_guardedalloc.h"

#include "BKE_global.hh"
#include "BKE_path_templates.hh"

#include "utils.hh"

namespace blender::asset_system::utils {

namespace path_templates = bke::path_templates;

std::string resolve_directory_path(StringRef directory)
{
  if (directory.is_empty()) {
    return "";
  }
  char dir_resolved[PATH_MAX];
  BLI_strncpy(dir_resolved,
              directory.data(),
              /* + 1 for null terminator. */
              std::min(directory.size() + 1, int64_t(sizeof(dir_resolved))));

  path_templates::VariableMap template_variables;
  bke::BlenderProject *project = BKE_blender_project_get(G_MAIN);
  BKE_add_template_variables_general(template_variables, nullptr, project);
  const Vector<path_templates::Error> variable_errors = BKE_path_apply_template(
      dir_resolved, FILE_MAX, template_variables);
  if (!variable_errors.is_empty()) {
    /* Do nothing on errors as the "dir_normalized" variable should still contain the original
     * string. The reset of the code should still gracefully treat this as an invalid path.
     * However it might be nice to bubble up the exact errors in the future.
     */
  }
  BLI_path_slash_native(dir_resolved);
  BLI_path_normalize_dir(dir_resolved, sizeof(dir_resolved));
  return std::string(dir_resolved);
}

std::string resolve_path(StringRefNull path, int64_t max_len)
{
  const int64_t len = (max_len == StringRef::not_found) ? path.size() :
                                                          std::min(max_len, path.size());

  char *buf = BLI_strdupn(path.c_str(), len);

  path_templates::VariableMap template_variables;
  bke::BlenderProject *project = BKE_blender_project_get(G_MAIN);
  BKE_add_template_variables_general(template_variables, nullptr, project);
  const Vector<path_templates::Error> variable_errors = BKE_path_apply_template(
      buf, FILE_MAX, template_variables);
  if (!variable_errors.is_empty()) {
    /* Do nothing on errors as the "dir_normalized" variable should still contain the original
     * string. The reset of the code should still gracefully treat this as an invalid path.
     * However it might be nice to bubble up the exact errors in the future.
     */
  }

  BLI_path_slash_native(buf);
  BLI_path_normalize(buf);

  std::string resolved_path = buf;
  MEM_delete(buf);

  if (len != path.size()) {
    resolved_path = resolved_path + path.substr(len);
  }

  return resolved_path;
}

}  // namespace blender::asset_system::utils
