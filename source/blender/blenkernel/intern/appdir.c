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
 */

/** \file blender/blenlib/intern/appdir.c
 *  \ingroup bke
 *
 * Access to application level directories.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "BKE_appdir.h"  /* own include */

#include "GHOST_Path-api.h"

#include "../blenkernel/BKE_blender.h"  /* BLENDER_VERSION, bad level include (no function call) */

#include "MEM_guardedalloc.h"

#ifdef WIN32
#  include "utf_winfunc.h"
#  include "utfconv.h"
#  include <io.h>
#  ifdef _WIN32_IE
#    undef _WIN32_IE
#  endif
#  define _WIN32_IE 0x0501
#  include <windows.h>
#  include <shlobj.h>
#  include "BLI_winstuff.h"
#else /* non windows */
#  ifdef WITH_BINRELOC
#    include "binreloc.h"
#  endif
#  include <unistd.h>  /* mkdtemp on OSX (and probably all *BSD?), not worth making specific check for this OS. */
#endif /* WIN32 */

/* local */
static char bprogname[FILE_MAX];    /* full path to program executable */
static char bprogdir[FILE_MAX];     /* full path to directory in which executable is located */
static char btempdir_base[FILE_MAX];          /* persistent temporary directory */
static char btempdir_session[FILE_MAX] = "";  /* volatile temporary directory */

/* This is now only used to really get the user's default document folder */
/* On Windows I chose the 'Users/<MyUserName>/Documents' since it's used
 * as default location to save documents */
const char *BKE_appdir_folder_default(void)
{
#ifndef WIN32
	const char * const xdg_documents_dir = getenv("XDG_DOCUMENTS_DIR");

	if (xdg_documents_dir)
		return xdg_documents_dir;

	return getenv("HOME");
#else /* Windows */
	static char documentfolder[MAXPATHLEN];
	HRESULT hResult;

	/* Check for %HOME% env var */
	if (uput_getenv("HOME", documentfolder, MAXPATHLEN)) {
		if (BLI_is_dir(documentfolder)) return documentfolder;
	}
				
	/* add user profile support for WIN 2K / NT.
	 * This is %APPDATA%, which translates to either
	 * %USERPROFILE%\Application Data or since Vista
	 * to %USERPROFILE%\AppData\Roaming
	 */
	hResult = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, documentfolder);
		
	if (hResult == S_OK) {
		if (BLI_is_dir(documentfolder)) return documentfolder;
	}
		
	return NULL;
#endif /* WIN32 */
}


// #define PATH_DEBUG

/* returns a formatted representation of the specified version number. Non-reentrant! */
static char *blender_version_decimal(const int ver)
{
	static char version_str[5];
	sprintf(version_str, "%d.%02d", ver / 100, ver % 100);
	return version_str;
}

/**
 * Concatenates path_base, (optional) path_sep and (optional) folder_name into targetpath,
 * returning true if result points to a directory.
 */
static bool test_path(char *targetpath, const char *path_base, const char *path_sep, const char *folder_name)
{
	char tmppath[FILE_MAX];
	
	if (path_sep) BLI_join_dirfile(tmppath, sizeof(tmppath), path_base, path_sep);
	else BLI_strncpy(tmppath, path_base, sizeof(tmppath));

	/* rare cases folder_name is omitted (when looking for ~/.blender/2.xx dir only) */
	if (folder_name)
		BLI_make_file_string("/", targetpath, tmppath, folder_name);
	else
		BLI_strncpy(targetpath, tmppath, sizeof(tmppath));
	/* FIXME: why is "//" on front of tmppath expanded to "/" (by BLI_join_dirfile)
	 * if folder_name is specified but not otherwise? */

	if (BLI_is_dir(targetpath)) {
#ifdef PATH_DEBUG
		printf("\t%s found: %s\n", __func__, targetpath);
#endif
		return true;
	}
	else {
#ifdef PATH_DEBUG
		printf("\t%s missing: %s\n", __func__, targetpath);
#endif
		//targetpath[0] = '\0';
		return false;
	}
}

/**
 * Puts the value of the specified environment variable into *path if it exists
 * and points at a directory. Returns true if this was done.
 */
static bool test_env_path(char *path, const char *envvar)
{
	const char *env = envvar ? getenv(envvar) : NULL;
	if (!env) return false;
	
	if (BLI_is_dir(env)) {
		BLI_strncpy(path, env, FILE_MAX);
#ifdef PATH_DEBUG
		printf("\t%s env %s found: %s\n", __func__, envvar, env);
#endif
		return true;
	}
	else {
		path[0] = '\0';
#ifdef PATH_DEBUG
		printf("\t%s env %s missing: %s\n", __func__, envvar, env);
#endif
		return false;
	}
}

/**
 * Constructs in \a targetpath the name of a directory relative to a version-specific
 * subdirectory in the parent directory of the Blender executable.
 *
 * \param targetpath  String to return path
 * \param folder_name  Optional folder name within version-specific directory
 * \param subfolder_name  Optional subfolder name within folder_name
 * \param ver  To construct name of version-specific directory within bprogdir
 * \return true if such a directory exists.
 */
static bool get_path_local(char *targetpath, const char *folder_name, const char *subfolder_name, const int ver)
{
	char relfolder[FILE_MAX];
	
#ifdef PATH_DEBUG
	printf("%s...\n", __func__);
#endif

	if (folder_name) {
		if (subfolder_name) {
			BLI_join_dirfile(relfolder, sizeof(relfolder), folder_name, subfolder_name);
		}
		else {
			BLI_strncpy(relfolder, folder_name, sizeof(relfolder));
		}
	}
	else {
		relfolder[0] = '\0';
	}

	/* try EXECUTABLE_DIR/2.5x/folder_name - new default directory for local blender installed files */
#ifdef __APPLE__
	static char osx_resourses[FILE_MAX]; /* due new codesign situation in OSX > 10.9.5 we must move the blender_version dir with contents to Resources */
	sprintf(osx_resourses, "%s../Resources", bprogdir);
	return test_path(targetpath, osx_resourses, blender_version_decimal(ver), relfolder);
#else
	return test_path(targetpath, bprogdir, blender_version_decimal(ver), relfolder);
#endif
}

/**
 * Is this an install with user files kept together with the Blender executable and its
 * installation files.
 */
static bool is_portable_install(void)
{
	/* detect portable install by the existence of config folder */
	const int ver = BLENDER_VERSION;
	char path[FILE_MAX];

	return get_path_local(path, "config", NULL, ver);
}

/**
 * Returns the path of a folder within the user-files area.
 *
 *
 * \param targetpath  String to return path
 * \param folder_name  default name of folder within user area
 * \param subfolder_name  optional name of subfolder within folder
 * \param envvar  name of environment variable which, if defined, overrides folder_name
 * \param ver  Blender version, used to construct a subdirectory name
 * \return true if it was able to construct such a path.
 */
static bool get_path_user(char *targetpath, const char *folder_name, const char *subfolder_name, const char *envvar, const int ver)
{
	char user_path[FILE_MAX];
	const char *user_base_path;

	/* for portable install, user path is always local */
	if (is_portable_install())
		return get_path_local(targetpath, folder_name, subfolder_name, ver);
	
	user_path[0] = '\0';

	if (test_env_path(user_path, envvar)) {
		if (subfolder_name) {
			return test_path(targetpath, user_path, NULL, subfolder_name);
		}
		else {
			BLI_strncpy(targetpath, user_path, FILE_MAX);
			return true;
		}
	}

	user_base_path = (const char *)GHOST_getUserDir(ver, blender_version_decimal(ver));
	if (user_base_path)
		BLI_strncpy(user_path, user_base_path, FILE_MAX);

	if (!user_path[0])
		return false;
	
#ifdef PATH_DEBUG
	printf("%s: %s\n", __func__, user_path);
#endif
	
	if (subfolder_name) {
		return test_path(targetpath, user_path, folder_name, subfolder_name);
	}
	else {
		return test_path(targetpath, user_path, NULL, folder_name);
	}
}

/**
 * Returns the path of a folder within the Blender installation directory.
 *
 * \param targetpath  String to return path
 * \param folder_name  default name of folder within installation area
 * \param subfolder_name  optional name of subfolder within folder
 * \param envvar  name of environment variable which, if defined, overrides folder_name
 * \param ver  Blender version, used to construct a subdirectory name
 * \return  true if it was able to construct such a path.
 */
static bool get_path_system(char *targetpath, const char *folder_name, const char *subfolder_name, const char *envvar, const int ver)
{
	char system_path[FILE_MAX];
	const char *system_base_path;
	char cwd[FILE_MAX];
	char relfolder[FILE_MAX];

	if (folder_name) {
		if (subfolder_name) {
			BLI_join_dirfile(relfolder, sizeof(relfolder), folder_name, subfolder_name);
		}
		else {
			BLI_strncpy(relfolder, folder_name, sizeof(relfolder));
		}
	}
	else {
		relfolder[0] = '\0';
	}

	/* first allow developer only overrides to the system path
	 * these are only used when running blender from source */

	/* try CWD/release/folder_name */
	if (BLI_current_working_dir(cwd, sizeof(cwd))) {
		if (test_path(targetpath, cwd, "release", relfolder)) {
			return true;
		}
	}

	/* try EXECUTABLE_DIR/release/folder_name */
	if (test_path(targetpath, bprogdir, "release", relfolder))
		return true;

	/* end developer overrides */



	system_path[0] = '\0';

	if (test_env_path(system_path, envvar)) {
		if (subfolder_name) {
			return test_path(targetpath, system_path, NULL, subfolder_name);
		}
		else {
			BLI_strncpy(targetpath, system_path, FILE_MAX);
			return true;
		}
	}

	system_base_path = (const char *)GHOST_getSystemDir(ver, blender_version_decimal(ver));
	if (system_base_path)
		BLI_strncpy(system_path, system_base_path, FILE_MAX);
	
	if (!system_path[0])
		return false;
	
#ifdef PATH_DEBUG
	printf("%s: %s\n", __func__, system_path);
#endif
	
	if (subfolder_name) {
		/* try $BLENDERPATH/folder_name/subfolder_name */
		return test_path(targetpath, system_path, folder_name, subfolder_name);
	}
	else {
		/* try $BLENDERPATH/folder_name */
		return test_path(targetpath, system_path, NULL, folder_name);
	}
}

/* get a folder out of the 'folder_id' presets for paths */
/* returns the path if found, NULL string if not */
const char *BKE_appdir_folder_id(const int folder_id, const char *subfolder)
{
	const int ver = BLENDER_VERSION;
	static char path[FILE_MAX] = "";
	
	switch (folder_id) {
		case BLENDER_DATAFILES:     /* general case */
			if (get_path_user(path, "datafiles", subfolder, "BLENDER_USER_DATAFILES", ver)) break;
			if (get_path_local(path, "datafiles", subfolder, ver)) break;
			if (get_path_system(path, "datafiles", subfolder, "BLENDER_SYSTEM_DATAFILES", ver)) break;
			return NULL;
			
		case BLENDER_USER_DATAFILES:
			if (get_path_user(path, "datafiles", subfolder, "BLENDER_USER_DATAFILES", ver)) break;
			return NULL;
			
		case BLENDER_SYSTEM_DATAFILES:
			if (get_path_local(path, "datafiles", subfolder, ver)) break;
			if (get_path_system(path, "datafiles", subfolder, "BLENDER_SYSTEM_DATAFILES", ver)) break;
			return NULL;
			
		case BLENDER_USER_AUTOSAVE:
			if (get_path_user(path, "autosave", subfolder, "BLENDER_USER_DATAFILES", ver)) break;
			return NULL;

		case BLENDER_USER_CONFIG:
			if (get_path_user(path, "config", subfolder, "BLENDER_USER_CONFIG", ver)) break;
			return NULL;
			
		case BLENDER_USER_SCRIPTS:
			if (get_path_user(path, "scripts", subfolder, "BLENDER_USER_SCRIPTS", ver)) break;
			return NULL;
			
		case BLENDER_SYSTEM_SCRIPTS:
			if (get_path_local(path, "scripts", subfolder, ver)) break;
			if (get_path_system(path, "scripts", subfolder, "BLENDER_SYSTEM_SCRIPTS", ver)) break;
			return NULL;
			
		case BLENDER_SYSTEM_PYTHON:
			if (get_path_local(path, "python", subfolder, ver)) break;
			if (get_path_system(path, "python", subfolder, "BLENDER_SYSTEM_PYTHON", ver)) break;
			return NULL;

		default:
			BLI_assert(0);
			break;
	}
	
	return path;
}

/**
 * Returns the path to a folder in the user area without checking that it actually exists first.
 */
const char *BKE_appdir_folder_id_user_notest(const int folder_id, const char *subfolder)
{
	const int ver = BLENDER_VERSION;
	static char path[FILE_MAX] = "";

	switch (folder_id) {
		case BLENDER_USER_DATAFILES:
			get_path_user(path, "datafiles", subfolder, "BLENDER_USER_DATAFILES", ver);
			break;
		case BLENDER_USER_CONFIG:
			get_path_user(path, "config", subfolder, "BLENDER_USER_CONFIG", ver);
			break;
		case BLENDER_USER_AUTOSAVE:
			get_path_user(path, "autosave", subfolder, "BLENDER_USER_AUTOSAVE", ver);
			break;
		case BLENDER_USER_SCRIPTS:
			get_path_user(path, "scripts", subfolder, "BLENDER_USER_SCRIPTS", ver);
			break;
		default:
			BLI_assert(0);
			break;
	}

	if ('\0' == path[0]) {
		return NULL;
	}
	return path;
}

/**
 * Returns the path to a folder in the user area, creating it if it doesn't exist.
 */
const char *BKE_appdir_folder_id_create(int folder_id, const char *subfolder)
{
	const char *path;

	/* only for user folders */
	if (!ELEM(folder_id, BLENDER_USER_DATAFILES, BLENDER_USER_CONFIG, BLENDER_USER_SCRIPTS, BLENDER_USER_AUTOSAVE))
		return NULL;
	
	path = BKE_appdir_folder_id(folder_id, subfolder);
	
	if (!path) {
		path = BKE_appdir_folder_id_user_notest(folder_id, subfolder);
		if (path) BLI_dir_create_recursive(path);
	}
	
	return path;
}

/**
 * Returns the path of the top-level version-specific local, user or system directory.
 * If do_check, then the result will be NULL if the directory doesn't exist.
 */
const char *BKE_appdir_folder_id_version(const int folder_id, const int ver, const bool do_check)
{
	static char path[FILE_MAX] = "";
	bool ok;
	switch (folder_id) {
		case BLENDER_RESOURCE_PATH_USER:
			ok = get_path_user(path, NULL, NULL, NULL, ver);
			break;
		case BLENDER_RESOURCE_PATH_LOCAL:
			ok = get_path_local(path, NULL, NULL, ver);
			break;
		case BLENDER_RESOURCE_PATH_SYSTEM:
			ok = get_path_system(path, NULL, NULL, NULL, ver);
			break;
		default:
			path[0] = '\0'; /* in case do_check is false */
			ok = false;
			BLI_assert(!"incorrect ID");
			break;
	}

	if (!ok && do_check) {
		return NULL;
	}

	return path;
}

#ifdef PATH_DEBUG
#  undef PATH_DEBUG
#endif




/* -------------------------------------------------------------------- */
/* Preset paths */

/**
 * Tries appending each of the semicolon-separated extensions in the PATHEXT
 * environment variable (Windows-only) onto *name in turn until such a file is found.
 * Returns success/failure.
 */
static int add_win32_extension(char *name)
{
	int retval = 0;
	int type;

	type = BLI_exists(name);
	if ((type == 0) || S_ISDIR(type)) {
#ifdef _WIN32
		char filename[FILE_MAX];
		char ext[FILE_MAX];
		const char *extensions = getenv("PATHEXT");
		if (extensions) {
			char *temp;
			do {
				strcpy(filename, name);
				temp = strstr(extensions, ";");
				if (temp) {
					strncpy(ext, extensions, temp - extensions);
					ext[temp - extensions] = 0;
					extensions = temp + 1;
					strcat(filename, ext);
				}
				else {
					strcat(filename, extensions);
				}

				type = BLI_exists(filename);
				if (type && (!S_ISDIR(type))) {
					retval = 1;
					strcpy(name, filename);
					break;
				}
			} while (temp);
		}
#endif
	}
	else {
		retval = 1;
	}

	return (retval);
}

/**
 * Checks if name is a fully qualified filename to an executable.
 * If not it searches $PATH for the file. On Windows it also
 * adds the correct extension (.com .exe etc) from
 * $PATHEXT if necessary. Also on Windows it translates
 * the name to its 8.3 version to prevent problems with
 * spaces and stuff. Final result is returned in fullname.
 *
 * \param fullname The full path and full name of the executable
 * (must be FILE_MAX minimum)
 * \param name The name of the executable (usually argv[0]) to be checked
 */
static void bli_where_am_i(char *fullname, const size_t maxlen, const char *name)
{
	char filename[FILE_MAX];
	const char *path = NULL, *temp;

#ifdef _WIN32
	const char *separator = ";";
#else
	const char *separator = ":";
#endif

	
#ifdef WITH_BINRELOC
	/* linux uses binreloc since argv[0] is not reliable, call br_init( NULL ) first */
	path = br_find_exe(NULL);
	if (path) {
		BLI_strncpy(fullname, path, maxlen);
		free((void *)path);
		return;
	}
#endif

#ifdef _WIN32
	wchar_t *fullname_16 = MEM_mallocN(maxlen * sizeof(wchar_t), "ProgramPath");
	if (GetModuleFileNameW(0, fullname_16, maxlen)) {
		conv_utf_16_to_8(fullname_16, fullname, maxlen);
		if (!BLI_exists(fullname)) {
			printf("path can't be found: \"%.*s\"\n", (int)maxlen, fullname);
			MessageBox(NULL, "path contains invalid characters or is too long (see console)", "Error", MB_OK);
		}
		MEM_freeN(fullname_16);
		return;
	}

	MEM_freeN(fullname_16);
#endif

	/* unix and non linux */
	if (name && name[0]) {

		BLI_strncpy(fullname, name, maxlen);
		if (name[0] == '.') {
			char wdir[FILE_MAX] = "";
			BLI_current_working_dir(wdir, sizeof(wdir));     /* backup cwd to restore after */

			// not needed but avoids annoying /./ in name
			if (name[1] == SEP)
				BLI_join_dirfile(fullname, maxlen, wdir, name + 2);
			else
				BLI_join_dirfile(fullname, maxlen, wdir, name);

			add_win32_extension(fullname); /* XXX, doesnt respect length */
		}
		else if (BLI_last_slash(name)) {
			// full path
			BLI_strncpy(fullname, name, maxlen);
			add_win32_extension(fullname);
		}
		else {
			// search for binary in $PATH
			path = getenv("PATH");
			if (path) {
				do {
					temp = strstr(path, separator);
					if (temp) {
						strncpy(filename, path, temp - path);
						filename[temp - path] = 0;
						path = temp + 1;
					}
					else {
						strncpy(filename, path, sizeof(filename));
					}
					BLI_path_append(fullname, maxlen, name);
					if (add_win32_extension(filename)) {
						BLI_strncpy(fullname, filename, maxlen);
						break;
					}
				} while (temp);
			}
		}
#if defined(DEBUG)
		if (strcmp(name, fullname)) {
			printf("guessing '%s' == '%s'\n", name, fullname);
		}
#endif
	}
}

void BKE_appdir_program_path_init(const char *argv0)
{
	bli_where_am_i(bprogname, sizeof(bprogname), argv0);
	BLI_split_dir_part(bprogname, bprogdir, sizeof(bprogdir));
}

/**
 * Path to executable
 */
const char *BKE_appdir_program_path(void)
{
	return bprogname;
}

/**
 * Path to directory of executable
 */
const char *BKE_appdir_program_dir(void)
{
	return bprogdir;
}

/**
 * Gets the temp directory when blender first runs.
 * If the default path is not found, use try $TEMP
 * 
 * Also make sure the temp dir has a trailing slash
 *
 * \param fullname The full path to the temporary temp directory
 * \param basename The full path to the persistent temp directory (may be NULL)
 * \param maxlen The size of the fullname buffer
 * \param userdir Directory specified in user preferences 
 */
static void BLI_where_is_temp(char *fullname, char *basename, const size_t maxlen, char *userdir)
{
	/* Clear existing temp dir, if needed. */
	BKE_tempdir_session_purge();

	fullname[0] = '\0';
	if (basename) {
		basename[0] = '\0';
	}

	if (userdir && BLI_is_dir(userdir)) {
		BLI_strncpy(fullname, userdir, maxlen);
	}
	
	
#ifdef WIN32
	if (fullname[0] == '\0') {
		const char *tmp = getenv("TEMP"); /* Windows */
		if (tmp && BLI_is_dir(tmp)) {
			BLI_strncpy(fullname, tmp, maxlen);
		}
	}
#else
	/* Other OS's - Try TMP and TMPDIR */
	if (fullname[0] == '\0') {
		const char *tmp = getenv("TMP");
		if (tmp && BLI_is_dir(tmp)) {
			BLI_strncpy(fullname, tmp, maxlen);
		}
	}
	
	if (fullname[0] == '\0') {
		const char *tmp = getenv("TMPDIR");
		if (tmp && BLI_is_dir(tmp)) {
			BLI_strncpy(fullname, tmp, maxlen);
		}
	}
#endif
	
	if (fullname[0] == '\0') {
		BLI_strncpy(fullname, "/tmp/", maxlen);
	}
	else {
		/* add a trailing slash if needed */
		BLI_add_slash(fullname);
#ifdef WIN32
		if (userdir && userdir != fullname) {
			BLI_strncpy(userdir, fullname, maxlen); /* also set user pref to show %TEMP%. /tmp/ is just plain confusing for Windows users. */
		}
#endif
	}

	/* Now that we have a valid temp dir, add system-generated unique sub-dir. */
	if (basename) {
		/* 'XXXXXX' is kind of tag to be replaced by mktemp-familly by an uuid. */
		char *tmp_name = BLI_strdupcat(fullname, "blender_XXXXXX");
		const size_t ln = strlen(tmp_name) + 1;
		if (ln <= maxlen) {
#ifdef WIN32
			if (_mktemp_s(tmp_name, ln) == 0) {
				BLI_dir_create_recursive(tmp_name);
			}
#else
			mkdtemp(tmp_name);
#endif
		}
		if (BLI_is_dir(tmp_name)) {
			BLI_strncpy(basename, fullname, maxlen);
			BLI_strncpy(fullname, tmp_name, maxlen);
			BLI_add_slash(fullname);
		}
		else {
			printf("Warning! Could not generate a temp file name for '%s', falling back to '%s'\n", tmp_name, fullname);
		}

		MEM_freeN(tmp_name);
	}
}

/**
 * Sets btempdir_base to userdir if specified and is a valid directory, otherwise
 * chooses a suitable OS-specific temporary directory.
 * Sets btempdir_session to a mkdtemp-generated sub-dir of btempdir_base.
 *
 * \note On Window userdir will be set to the temporary directory!
 */
void BKE_tempdir_init(char *userdir)
{
	BLI_where_is_temp(btempdir_session, btempdir_base, FILE_MAX, userdir);
;
}

/**
 * Path to temporary directory (with trailing slash)
 */
const char *BKE_tempdir_session(void)
{
	return btempdir_session[0] ? btempdir_session : BKE_tempdir_base();
}

/**
 * Path to persistent temporary directory (with trailing slash)
 */
const char *BKE_tempdir_base(void)
{
	return btempdir_base;
}

/**
 * Path to the system temporary directory (with trailing slash)
 */
void BKE_tempdir_system_init(char *dir)
{
	BLI_where_is_temp(dir, NULL, FILE_MAX, NULL);
}

/**
 * Delete content of this instance's temp dir.
 */
void BKE_tempdir_session_purge(void)
{
	if (btempdir_session[0] && BLI_is_dir(btempdir_session)) {
		BLI_delete(btempdir_session, true, true);
	}
}
