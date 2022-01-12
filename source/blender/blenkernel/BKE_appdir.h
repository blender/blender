/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#pragma once

/** \file
 * \ingroup bke
 *
 * \note on naming: typical _get() suffix is omitted here,
 * since its the main purpose of the API.
 */

#include <stddef.h>

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;

/**
 * Sanity check to ensure correct API use in debug mode.
 *
 * Run this once the first level of arguments has been passed so we can be sure
 * `--env-system-datafiles`, and other `--env-*` arguments has been passed.
 *
 * Without this any callers to this module that run early on,
 * will miss out on changes from parsing arguments.
 */
void BKE_appdir_init(void);
void BKE_appdir_exit(void);

/**
 * Get the folder that's the "natural" starting point for browsing files on an OS.
 * - Unix: `$HOME`
 * - Windows: `%userprofile%/Documents`
 *
 * \note On Windows `Users/{MyUserName}/Documents` is used as it's the default location to save
 * documents.
 */
const char *BKE_appdir_folder_default(void) ATTR_WARN_UNUSED_RESULT;
const char *BKE_appdir_folder_root(void) ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
const char *BKE_appdir_folder_default_or_root(void) ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
/**
 * Get the user's home directory, i.e.
 * - Unix: `$HOME`
 * - Windows: `%userprofile%`
 */
const char *BKE_appdir_folder_home(void);
/**
 * Get the user's document directory, i.e.
 * - Linux: `$HOME/Documents`
 * - Windows: `%userprofile%/Documents`
 *
 * If this can't be found using OS queries (via Ghost), try manually finding it.
 *
 * \returns True if the path is valid and points to an existing directory.
 */
bool BKE_appdir_folder_documents(char *dir);
/**
 * Get the user's cache directory, i.e.
 * - Linux: `$HOME/.cache/blender/`
 * - Windows: `%USERPROFILE%\AppData\Local\Blender Foundation\Blender\`
 * - MacOS: `/Library/Caches/Blender`
 *
 * \returns True if the path is valid. It doesn't create or checks format
 * if the `blender` folder exists. It does check if the parent of the path exists.
 */
bool BKE_appdir_folder_caches(char *r_path, size_t path_len);
/**
 * Get a folder out of the \a folder_id presets for paths.
 *
 * \param subfolder: The name of a directory to check for,
 * this may contain path separators but must resolve to a directory, checked with #BLI_is_dir.
 * \return The path if found, NULL string if not.
 */
bool BKE_appdir_folder_id_ex(int folder_id, const char *subfolder, char *path, size_t path_len);
const char *BKE_appdir_folder_id(int folder_id, const char *subfolder);
/**
 * Returns the path to a folder in the user area, creating it if it doesn't exist.
 */
const char *BKE_appdir_folder_id_create(int folder_id, const char *subfolder);
/**
 * Returns the path to a folder in the user area without checking that it actually exists first.
 */
const char *BKE_appdir_folder_id_user_notest(int folder_id, const char *subfolder);
/**
 * Returns the path of the top-level version-specific local, user or system directory.
 * If check_is_dir, then the result will be NULL if the directory doesn't exist.
 */
const char *BKE_appdir_folder_id_version(int folder_id, int version, bool check_is_dir);

/**
 * Check if this is an install with user files kept together
 * with the Blender executable and its installation files.
 */
bool BKE_appdir_app_is_portable_install(void);
/**
 * Return true if templates exist
 */
bool BKE_appdir_app_template_any(void);
bool BKE_appdir_app_template_id_search(const char *app_template, char *path, size_t path_len);
bool BKE_appdir_app_template_has_userpref(const char *app_template);
void BKE_appdir_app_templates(struct ListBase *templates);

/**
 * Initialize path to program executable.
 */
void BKE_appdir_program_path_init(const char *argv0);

/**
 * Path to executable
 */
const char *BKE_appdir_program_path(void);
/**
 * Path to directory of executable
 */
const char *BKE_appdir_program_dir(void);

/**
 * Gets a good default directory for fonts.
 */
bool BKE_appdir_font_folder_default(char *dir);

/**
 * Find Python executable.
 */
bool BKE_appdir_program_python_search(char *fullpath,
                                      const size_t fullpath_len,
                                      int version_major,
                                      int version_minor);

/**
 * Initialize path to temporary directory.
 */
void BKE_tempdir_init(const char *userdir);

/**
 * Path to persistent temporary directory (with trailing slash)
 */
const char *BKE_tempdir_base(void);
/**
 * Path to temporary directory (with trailing slash)
 */
const char *BKE_tempdir_session(void);
/**
 * Delete content of this instance's temp dir.
 */
void BKE_tempdir_session_purge(void);

/* folder_id */
enum {
  /* general, will find based on user/local/system priority */
  BLENDER_DATAFILES = 2,

  /* user-specific */
  BLENDER_USER_CONFIG = 31,
  BLENDER_USER_DATAFILES = 32,
  BLENDER_USER_SCRIPTS = 33,
  BLENDER_USER_AUTOSAVE = 34,

  /* system */
  BLENDER_SYSTEM_DATAFILES = 52,
  BLENDER_SYSTEM_SCRIPTS = 53,
  BLENDER_SYSTEM_PYTHON = 54,
};

/* for BKE_appdir_folder_id_version only */
enum {
  BLENDER_RESOURCE_PATH_USER = 0,
  BLENDER_RESOURCE_PATH_LOCAL = 1,
  BLENDER_RESOURCE_PATH_SYSTEM = 2,
};

#define BLENDER_STARTUP_FILE "startup.blend"
#define BLENDER_USERPREF_FILE "userpref.blend"
#define BLENDER_QUIT_FILE "quit.blend"
#define BLENDER_BOOKMARK_FILE "bookmarks.txt"
#define BLENDER_HISTORY_FILE "recent-files.txt"
#define BLENDER_PLATFORM_SUPPORT_FILE "platform_support.txt"

#ifdef __cplusplus
}
#endif
