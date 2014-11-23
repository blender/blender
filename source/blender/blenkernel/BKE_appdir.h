/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_APPDIR_H__
#define __BKE_APPDIR_H__

/** \file BKE_appdir.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

const char *BLI_getDefaultDocumentFolder(void);

const char *BLI_get_folder(int folder_id, const char *subfolder);
const char *BLI_get_folder_create(int folder_id, const char *subfolder);
const char *BLI_get_user_folder_notest(int folder_id, const char *subfolder);
const char *BLI_get_folder_version(const int id, const int ver, const bool do_check);


/* Initialize path to program executable */
void BLI_init_program_path(const char *argv0);
/* Initialize path to temporary directory.
 * NOTE: On Window userdir will be set to the temporary directory! */
void BLI_temp_dir_init(char *userdir);

const char *BLI_program_path(void);
const char *BLI_program_dir(void);
const char *BLI_temp_dir_session(void);
const char *BLI_temp_dir_base(void);
void BLI_system_temporary_dir(char *dir);
void BLI_temp_dir_session_purge(void);


/* folder_id */

/* general, will find based on user/local/system priority */
#define BLENDER_DATAFILES           2

/* user-specific */
#define BLENDER_USER_CONFIG         31
#define BLENDER_USER_DATAFILES      32
#define BLENDER_USER_SCRIPTS        33
#define BLENDER_USER_AUTOSAVE       34

/* system */
#define BLENDER_SYSTEM_DATAFILES    52
#define BLENDER_SYSTEM_SCRIPTS      53
#define BLENDER_SYSTEM_PYTHON       54

/* for BLI_get_folder_version only */
#define BLENDER_RESOURCE_PATH_USER      0
#define BLENDER_RESOURCE_PATH_LOCAL     1
#define BLENDER_RESOURCE_PATH_SYSTEM    2

#define BLENDER_STARTUP_FILE    "startup.blend"
#define BLENDER_USERPREF_FILE   "userpref.blend"
#define BLENDER_QUIT_FILE       "quit.blend"
#define BLENDER_BOOKMARK_FILE   "bookmarks.txt"
#define BLENDER_HISTORY_FILE    "recent-files.txt"


#ifdef __cplusplus
}
#endif

#endif  /* __BKE_APPDIR_H__ */
