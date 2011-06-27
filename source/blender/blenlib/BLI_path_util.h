/*
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BLI_PATH_UTIL_H
#define BLI_PATH_UTIL_H

/** \file BLI_path_util.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct direntry;

const char *BLI_getDefaultDocumentFolder(void);

char *BLI_get_folder(int folder_id, const char *subfolder);
char *BLI_get_folder_create(int folder_id, const char *subfolder);
char *BLI_get_user_folder_notest(int folder_id, const char *subfolder);
char *BLI_get_folder_version(const int id, const int ver, const int do_check);

/* folder_id */

/* general, will find based on user/local/system priority */
#define BLENDER_DATAFILES			2

/* user-specific */
#define BLENDER_USER_CONFIG			31
#define BLENDER_USER_DATAFILES		32
#define BLENDER_USER_SCRIPTS		33
#define BLENDER_USER_PLUGINS		34
#define BLENDER_USER_AUTOSAVE		35

/* system */
#define BLENDER_SYSTEM_DATAFILES	52
#define BLENDER_SYSTEM_SCRIPTS		53
#define BLENDER_SYSTEM_PLUGINS		54
#define BLENDER_SYSTEM_PYTHON		54

#define BLENDER_TEMP				80

#define BLENDER_USERFOLDER(id) (id >= BLENDER_USER_CONFIG && id <= BLENDER_USER_PLUGINS)

/* for BLI_get_folder_version only */
#define BLENDER_RESOURCE_PATH_USER		0
#define BLENDER_RESOURCE_PATH_LOCAL		1
#define BLENDER_RESOURCE_PATH_SYSTEM	2

#define BLENDER_STARTUP_FILE	"startup.blend"
#define BLENDER_BOOKMARK_FILE	"bookmarks.txt"
#define BLENDER_HISTORY_FILE	"recent-files.txt"

#ifdef WIN32
#define BLENDER_USER_FORMAT		"%s\\Blender Foundation\\Blender\\%s"
#define BLENDER_SYSTEM_FORMAT		"%s\\Blender Foundation\\Blender\\%s"
#elif defined(__APPLE__)
#define BLENDER_USER_FORMAT			"%s/Blender/%s"
#define BLENDER_SYSTEM_FORMAT			"%s/Blender/%s"
#else
#define BLENDER_USER_FORMAT			"%s/.blender/%s"
#define BLENDER_SYSTEM_FORMAT			"%s/blender/%s"
#endif

#ifdef WIN32
#define SEP '\\'
#define ALTSEP '/'
#else
#define SEP '/'
#define ALTSEP '\\'
#endif

void BLI_setenv(const char *env, const char *val);
void BLI_setenv_if_new(const char *env, const char* val);

void BLI_make_file_string(const char *relabase, char *string,  const char *dir, const char *file);
void BLI_make_exist(char *dir);
void BLI_make_existing_file(const char *name);
void BLI_split_dirfile(const char *string, char *dir, char *file);
void BLI_join_dirfile(char *string, const size_t maxlen, const char *dir, const char *file);
char *BLI_path_basename(char *path);
int BKE_rebase_path(char *abs, size_t abs_len, char *rel, size_t rel_len, const char *base_dir, const char *src_dir, const char *dest_dir);
char *BLI_last_slash(const char *string);
int	  BLI_add_slash(char *string);
void  BLI_del_slash(char *string);
char *BLI_first_slash(char *string);

void BLI_getlastdir(const char* dir, char *last, const size_t maxlen);
int BLI_testextensie(const char *str, const char *ext);
int BLI_testextensie_array(const char *str, const char **ext_array);
int BLI_testextensie_glob(const char *str, const char *ext_fnmatch);
int BLI_replace_extension(char *path, size_t maxlen, const char *ext);
void BLI_uniquename(struct ListBase *list, void *vlink, const char defname[], char delim, short name_offs, short len);
int BLI_uniquename_cb(int (*unique_check)(void *, const char *), void *arg, const char defname[], char delim, char *name, short name_len);
void BLI_newname(char * name, int add);
int BLI_stringdec(const char *string, char *head, char *start, unsigned short *numlen);
void BLI_stringenc(char *string, const char *head, const char *tail, unsigned short numlen, int pic);
int BLI_split_name_num(char *left, int *nr, const char *name, const char delim);
void BLI_splitdirstring(char *di,char *fi);

/* make sure path separators conform to system one */
void BLI_clean(char *path);

/**
	 * dir can be any input, like from buttons, and this function
	 * converts it to a regular full path.
	 * Also removes garbage from directory paths, like /../ or double slashes etc 
	 */
void BLI_cleanup_file(const char *relabase, char *dir); /* removes trailing slash */
void BLI_cleanup_dir(const char *relabase, char *dir); /* same as above but adds a trailing slash */
void BLI_cleanup_path(const char *relabase, char *dir); /* doesn't touch trailing slash */

/* go back one directory */
int BLI_parent_dir(char *path);

/* return whether directory is root and thus has no parent dir */
int BLI_has_parent(char *path);

	/**
	 * Blender's path code replacement function.
	 * Bases @a path strings leading with "//" by the
	 * directory @a basepath, and replaces instances of
	 * '#' with the @a framenum. Results are written
	 * back into @a path.
	 * 
	 * @a path The path to convert
	 * @a basepath The directory to base relative paths with.
	 * @a framenum The framenumber to replace the frame code with.
	 * @retval Returns true if the path was relative (started with "//").
	 */
int BLI_path_abs(char *path, const char *basepath);
int BLI_path_frame(char *path, int frame, int digits);
int BLI_path_frame_range(char *path, int sta, int end, int digits);
int BLI_path_cwd(char *path);
void BLI_path_rel(char *file, const char *relfile);

#ifdef WIN32
#  define BLI_path_cmp BLI_strcasecmp
#  define BLI_path_ncmp BLI_strncasecmp
#else
#  define BLI_path_cmp strcmp
#  define BLI_path_ncmp strncmp
#endif

	/**
	 * Change every @a from in @a string into @a to. The
	 * result will be in @a string
	 *
	 * @a string The string to work on
	 * @a from The character to replace
	 * @a to The character to replace with
	 */
void BLI_char_switch(char *string, char from, char to);

/**
	 * Checks if name is a fully qualified filename to an executable.
	 * If not it searches $PATH for the file. On Windows it also
	 * adds the correct extension (.com .exe etc) from
	 * $PATHEXT if necessary. Also on Windows it translates
	 * the name to its 8.3 version to prevent problems with
	 * spaces and stuff. Final result is returned in fullname.
	 *
	 * @param fullname The full path and full name of the executable
	 * @param name The name of the executable (usually argv[0]) to be checked
	 */
void BLI_where_am_i(char *fullname, const size_t maxlen, const char *name);

	/**
	 * Gets the temp directory when blender first runs.
	 * If the default path is not found, use try $TEMP
	 * 
	 * Also make sure the temp dir has a trailing slash
	 *
	 * @param fullname The full path to the temp directory
	 */
void BLI_where_is_temp(char *fullname, const size_t maxlen, int usertemp);


#ifdef WITH_ICONV
void BLI_string_to_utf8(char *original, char *utf_8, const char *code);
#endif

#ifdef __cplusplus
}
#endif

#endif

