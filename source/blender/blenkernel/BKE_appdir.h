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
#ifndef __BKE_APPDIR_H__
#define __BKE_APPDIR_H__

/** \file
 * \ingroup bli
 */
struct ListBase;

/* note on naming: typical _get() suffix is omitted here,
 * since its the main purpose of the API. */
const char *BKE_appdir_folder_default(void);
const char *BKE_appdir_folder_id_ex(const int folder_id,
                                    const char *subfolder,
                                    char *path,
                                    size_t path_len);
const char *BKE_appdir_folder_id(const int folder_id, const char *subfolder);
const char *BKE_appdir_folder_id_create(const int folder_id, const char *subfolder);
const char *BKE_appdir_folder_id_user_notest(const int folder_id, const char *subfolder);
const char *BKE_appdir_folder_id_version(const int folder_id, const int ver, const bool do_check);

bool BKE_appdir_app_is_portable_install(void);
bool BKE_appdir_app_template_any(void);
bool BKE_appdir_app_template_id_search(const char *app_template, char *path, size_t path_len);
bool BKE_appdir_app_template_has_userpref(const char *app_template);
void BKE_appdir_app_templates(struct ListBase *templates);

/* Initialize path to program executable */
void BKE_appdir_program_path_init(const char *argv0);

const char *BKE_appdir_program_path(void);
const char *BKE_appdir_program_dir(void);

/* return OS fonts directory */
bool BKE_appdir_fonts_folder_default(char *dir);

/* find python executable */
bool BKE_appdir_program_python_search(char *fullpath,
                                      const size_t fullpath_len,
                                      const int version_major,
                                      const int version_minor);

/* Initialize path to temporary directory. */
void BKE_tempdir_init(char *userdir);
void BKE_tempdir_system_init(char *dir);

const char *BKE_tempdir_base(void);
const char *BKE_tempdir_session(void);
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

#endif /* __BKE_APPDIR_H__ */
