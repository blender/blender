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

/** \file
 * \ingroup bke
 *
 * Access to application level directories.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "BLI_utildefines.h"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_blender_version.h"
#include "BKE_appdir.h" /* own include */

#include "GHOST_Path-api.h"

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

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
/* mkdtemp on OSX (and probably all *BSD?), not worth making specific check for this OS. */
#  include <unistd.h>
#endif /* WIN32 */

/* local */
static CLG_LogRef LOG = {"bke.appdir"};
static char bprogname[FILE_MAX];     /* full path to program executable */
static char bprogdir[FILE_MAX];      /* full path to directory in which executable is located */
static char btempdir_base[FILE_MAX]; /* persistent temporary directory */
static char btempdir_session[FILE_MAX] = ""; /* volatile temporary directory */

/* This is now only used to really get the user's default document folder */
/* On Windows I chose the 'Users/<MyUserName>/Documents' since it's used
 * as default location to save documents */
const char *BKE_appdir_folder_default(void)
{
#ifndef WIN32
  const char *const xdg_documents_dir = BLI_getenv("XDG_DOCUMENTS_DIR");

  if (xdg_documents_dir) {
    return xdg_documents_dir;
  }

  return BLI_getenv("HOME");
#else  /* Windows */
  static char documentfolder[MAXPATHLEN];
  HRESULT hResult;

  /* Check for %HOME% env var */
  if (uput_getenv("HOME", documentfolder, MAXPATHLEN)) {
    if (BLI_is_dir(documentfolder)) {
      return documentfolder;
    }
  }

  /* add user profile support for WIN 2K / NT.
   * This is %APPDATA%, which translates to either
   * %USERPROFILE%\Application Data or since Vista
   * to %USERPROFILE%\AppData\Roaming
   */
  hResult = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, documentfolder);

  if (hResult == S_OK) {
    if (BLI_is_dir(documentfolder)) {
      return documentfolder;
    }
  }

  return NULL;
#endif /* WIN32 */
}

// #define PATH_DEBUG

/* returns a formatted representation of the specified version number. Non-reentrant! */
static char *blender_version_decimal(const int ver)
{
  static char version_str[5];
  BLI_assert(ver < 1000);
  BLI_snprintf(version_str, sizeof(version_str), "%d.%02d", ver / 100, ver % 100);
  return version_str;
}

/**
 * Concatenates path_base, (optional) path_sep and (optional) folder_name into targetpath,
 * returning true if result points to a directory.
 */
static bool test_path(char *targetpath,
                      size_t targetpath_len,
                      const char *path_base,
                      const char *path_sep,
                      const char *folder_name)
{
  char tmppath[FILE_MAX];

  if (path_sep) {
    BLI_join_dirfile(tmppath, sizeof(tmppath), path_base, path_sep);
  }
  else {
    BLI_strncpy(tmppath, path_base, sizeof(tmppath));
  }

  /* rare cases folder_name is omitted (when looking for ~/.config/blender/2.xx dir only) */
  if (folder_name) {
    BLI_join_dirfile(targetpath, targetpath_len, tmppath, folder_name);
  }
  else {
    BLI_strncpy(targetpath, tmppath, targetpath_len);
  }
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
    // targetpath[0] = '\0';
    return false;
  }
}

/**
 * Puts the value of the specified environment variable into *path if it exists
 * and points at a directory. Returns true if this was done.
 */
static bool test_env_path(char *path, const char *envvar)
{
  const char *env = envvar ? BLI_getenv(envvar) : NULL;
  if (!env) {
    return false;
  }

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
 * \param targetpath: String to return path
 * \param folder_name: Optional folder name within version-specific directory
 * \param subfolder_name: Optional subfolder name within folder_name
 * \param ver: To construct name of version-specific directory within bprogdir
 * \return true if such a directory exists.
 */
static bool get_path_local(char *targetpath,
                           size_t targetpath_len,
                           const char *folder_name,
                           const char *subfolder_name,
                           const int ver)
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

  /* Try EXECUTABLE_DIR/2.5x/folder_name -
   * new default directory for local blender installed files. */
#ifdef __APPLE__
  /* Due new codesign situation in OSX > 10.9.5
   * we must move the blender_version dir with contents to Resources. */
  char osx_resourses[FILE_MAX];
  BLI_snprintf(osx_resourses, sizeof(osx_resourses), "%s../Resources", bprogdir);
  /* Remove the '/../' added above. */
  BLI_cleanup_path(NULL, osx_resourses);
  return test_path(
      targetpath, targetpath_len, osx_resourses, blender_version_decimal(ver), relfolder);
#else
  return test_path(targetpath, targetpath_len, bprogdir, blender_version_decimal(ver), relfolder);
#endif
}

/**
 * Is this an install with user files kept together with the Blender executable and its
 * installation files.
 */
bool BKE_appdir_app_is_portable_install(void)
{
  /* detect portable install by the existence of config folder */
  const int ver = BLENDER_VERSION;
  char path[FILE_MAX];

  return get_path_local(path, sizeof(path), "config", NULL, ver);
}

/**
 * Returns the path of a folder from environment variables
 *
 * \param targetpath: String to return path.
 * \param subfolder_name: optional name of subfolder within folder.
 * \param envvar: name of environment variable to check folder_name.
 * \return true if it was able to construct such a path and the path exists.
 */
static bool get_path_environment(char *targetpath,
                                 size_t targetpath_len,
                                 const char *subfolder_name,
                                 const char *envvar)
{
  char user_path[FILE_MAX];

  if (test_env_path(user_path, envvar)) {
    if (subfolder_name) {
      return test_path(targetpath, targetpath_len, user_path, NULL, subfolder_name);
    }
    else {
      BLI_strncpy(targetpath, user_path, FILE_MAX);
      return true;
    }
  }
  return false;
}

/**
 * Returns the path of a folder from environment variables
 *
 * \param targetpath: String to return path.
 * \param subfolder_name: optional name of subfolder within folder.
 * \param envvar: name of environment variable to check folder_name.
 * \return true if it was able to construct such a path.
 */
static bool get_path_environment_notest(char *targetpath,
                                        size_t targetpath_len,
                                        const char *subfolder_name,
                                        const char *envvar)
{
  char user_path[FILE_MAX];

  if (test_env_path(user_path, envvar)) {
    if (subfolder_name) {
      BLI_join_dirfile(targetpath, targetpath_len, user_path, subfolder_name);
      return true;
    }
    else {
      BLI_strncpy(targetpath, user_path, FILE_MAX);
      return true;
    }
  }
  return false;
}

/**
 * Returns the path of a folder within the user-files area.
 * \param targetpath: String to return path
 * \param folder_name: default name of folder within user area
 * \param subfolder_name: optional name of subfolder within folder
 * \param ver: Blender version, used to construct a subdirectory name
 * \return true if it was able to construct such a path.
 */
static bool get_path_user(char *targetpath,
                          size_t targetpath_len,
                          const char *folder_name,
                          const char *subfolder_name,
                          const int ver)
{
  char user_path[FILE_MAX];
  const char *user_base_path;

  /* for portable install, user path is always local */
  if (BKE_appdir_app_is_portable_install()) {
    return get_path_local(targetpath, targetpath_len, folder_name, subfolder_name, ver);
  }
  user_path[0] = '\0';

  user_base_path = (const char *)GHOST_getUserDir(ver, blender_version_decimal(ver));
  if (user_base_path) {
    BLI_strncpy(user_path, user_base_path, FILE_MAX);
  }

  if (!user_path[0]) {
    return false;
  }

#ifdef PATH_DEBUG
  printf("%s: %s\n", __func__, user_path);
#endif

  if (subfolder_name) {
    return test_path(targetpath, targetpath_len, user_path, folder_name, subfolder_name);
  }
  else {
    return test_path(targetpath, targetpath_len, user_path, NULL, folder_name);
  }
}

/**
 * Returns the path of a folder within the Blender installation directory.
 *
 * \param targetpath: String to return path
 * \param folder_name: default name of folder within installation area
 * \param subfolder_name: optional name of subfolder within folder
 * \param ver: Blender version, used to construct a subdirectory name
 * \return  true if it was able to construct such a path.
 */
static bool get_path_system(char *targetpath,
                            size_t targetpath_len,
                            const char *folder_name,
                            const char *subfolder_name,
                            const int ver)
{
  char system_path[FILE_MAX];
  const char *system_base_path;
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

  system_path[0] = '\0';
  system_base_path = (const char *)GHOST_getSystemDir(ver, blender_version_decimal(ver));
  if (system_base_path) {
    BLI_strncpy(system_path, system_base_path, FILE_MAX);
  }

  if (!system_path[0]) {
    return false;
  }

#ifdef PATH_DEBUG
  printf("%s: %s\n", __func__, system_path);
#endif

  if (subfolder_name) {
    /* try $BLENDERPATH/folder_name/subfolder_name */
    return test_path(targetpath, targetpath_len, system_path, folder_name, subfolder_name);
  }
  else {
    /* try $BLENDERPATH/folder_name */
    return test_path(targetpath, targetpath_len, system_path, NULL, folder_name);
  }
}

/**
 * Get a folder out of the 'folder_id' presets for paths.
 * returns the path if found, NULL string if not
 *
 * \param subfolder: The name of a directory to check for,
 * this may contain path separators but must resolve to a directory, checked with #BLI_is_dir.
 */
const char *BKE_appdir_folder_id_ex(const int folder_id,
                                    const char *subfolder,
                                    char *path,
                                    size_t path_len)
{
  const int ver = BLENDER_VERSION;

  switch (folder_id) {
    case BLENDER_DATAFILES: /* general case */
      if (get_path_environment(path, path_len, subfolder, "BLENDER_USER_DATAFILES")) {
        break;
      }
      if (get_path_user(path, path_len, "datafiles", subfolder, ver)) {
        break;
      }
      if (get_path_environment(path, path_len, subfolder, "BLENDER_SYSTEM_DATAFILES")) {
        break;
      }
      if (get_path_local(path, path_len, "datafiles", subfolder, ver)) {
        break;
      }
      if (get_path_system(path, path_len, "datafiles", subfolder, ver)) {
        break;
      }
      return NULL;

    case BLENDER_USER_DATAFILES:
      if (get_path_environment(path, path_len, subfolder, "BLENDER_USER_DATAFILES")) {
        break;
      }
      if (get_path_user(path, path_len, "datafiles", subfolder, ver)) {
        break;
      }
      return NULL;

    case BLENDER_SYSTEM_DATAFILES:
      if (get_path_environment(path, path_len, subfolder, "BLENDER_SYSTEM_DATAFILES")) {
        break;
      }
      if (get_path_system(path, path_len, "datafiles", subfolder, ver)) {
        break;
      }
      if (get_path_local(path, path_len, "datafiles", subfolder, ver)) {
        break;
      }
      return NULL;

    case BLENDER_USER_AUTOSAVE:
      if (get_path_environment(path, path_len, subfolder, "BLENDER_USER_DATAFILES")) {
        break;
      }
      if (get_path_user(path, path_len, "autosave", subfolder, ver)) {
        break;
      }
      return NULL;

    case BLENDER_USER_CONFIG:
      if (get_path_environment(path, path_len, subfolder, "BLENDER_USER_CONFIG")) {
        break;
      }
      if (get_path_user(path, path_len, "config", subfolder, ver)) {
        break;
      }
      return NULL;

    case BLENDER_USER_SCRIPTS:
      if (get_path_environment(path, path_len, subfolder, "BLENDER_USER_SCRIPTS")) {
        break;
      }
      if (get_path_user(path, path_len, "scripts", subfolder, ver)) {
        break;
      }
      return NULL;

    case BLENDER_SYSTEM_SCRIPTS:
      if (get_path_environment(path, path_len, subfolder, "BLENDER_SYSTEM_SCRIPTS")) {
        break;
      }
      if (get_path_system(path, path_len, "scripts", subfolder, ver)) {
        break;
      }
      if (get_path_local(path, path_len, "scripts", subfolder, ver)) {
        break;
      }
      return NULL;

    case BLENDER_SYSTEM_PYTHON:
      if (get_path_environment(path, path_len, subfolder, "BLENDER_SYSTEM_PYTHON")) {
        break;
      }
      if (get_path_system(path, path_len, "python", subfolder, ver)) {
        break;
      }
      if (get_path_local(path, path_len, "python", subfolder, ver)) {
        break;
      }
      return NULL;

    default:
      BLI_assert(0);
      break;
  }

  return path;
}

const char *BKE_appdir_folder_id(const int folder_id, const char *subfolder)
{
  static char path[FILE_MAX] = "";
  return BKE_appdir_folder_id_ex(folder_id, subfolder, path, sizeof(path));
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
      if (get_path_environment_notest(path, sizeof(path), subfolder, "BLENDER_USER_DATAFILES")) {
        break;
      }
      get_path_user(path, sizeof(path), "datafiles", subfolder, ver);
      break;
    case BLENDER_USER_CONFIG:
      if (get_path_environment_notest(path, sizeof(path), subfolder, "BLENDER_USER_CONFIG")) {
        break;
      }
      get_path_user(path, sizeof(path), "config", subfolder, ver);
      break;
    case BLENDER_USER_AUTOSAVE:
      if (get_path_environment_notest(path, sizeof(path), subfolder, "BLENDER_USER_AUTOSAVE")) {
        break;
      }
      get_path_user(path, sizeof(path), "autosave", subfolder, ver);
      break;
    case BLENDER_USER_SCRIPTS:
      if (get_path_environment_notest(path, sizeof(path), subfolder, "BLENDER_USER_SCRIPTS")) {
        break;
      }
      get_path_user(path, sizeof(path), "scripts", subfolder, ver);
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
const char *BKE_appdir_folder_id_create(const int folder_id, const char *subfolder)
{
  const char *path;

  /* only for user folders */
  if (!ELEM(folder_id,
            BLENDER_USER_DATAFILES,
            BLENDER_USER_CONFIG,
            BLENDER_USER_SCRIPTS,
            BLENDER_USER_AUTOSAVE)) {
    return NULL;
  }

  path = BKE_appdir_folder_id(folder_id, subfolder);

  if (!path) {
    path = BKE_appdir_folder_id_user_notest(folder_id, subfolder);
    if (path) {
      BLI_dir_create_recursive(path);
    }
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
      ok = get_path_user(path, sizeof(path), NULL, NULL, ver);
      break;
    case BLENDER_RESOURCE_PATH_LOCAL:
      ok = get_path_local(path, sizeof(path), NULL, NULL, ver);
      break;
    case BLENDER_RESOURCE_PATH_SYSTEM:
      ok = get_path_system(path, sizeof(path), NULL, NULL, ver);
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
 * Checks if name is a fully qualified filename to an executable.
 * If not it searches $PATH for the file. On Windows it also
 * adds the correct extension (.com .exe etc) from
 * $PATHEXT if necessary. Also on Windows it translates
 * the name to its 8.3 version to prevent problems with
 * spaces and stuff. Final result is returned in fullname.
 *
 * \param fullname: The full path and full name of the executable
 * (must be FILE_MAX minimum)
 * \param name: The name of the executable (usually argv[0]) to be checked
 */
static void where_am_i(char *fullname, const size_t maxlen, const char *name)
{
#ifdef WITH_BINRELOC
  /* linux uses binreloc since argv[0] is not reliable, call br_init( NULL ) first */
  {
    const char *path = NULL;
    path = br_find_exe(NULL);
    if (path) {
      BLI_strncpy(fullname, path, maxlen);
      free((void *)path);
      return;
    }
  }
#endif

#ifdef _WIN32
  {
    wchar_t *fullname_16 = MEM_mallocN(maxlen * sizeof(wchar_t), "ProgramPath");
    if (GetModuleFileNameW(0, fullname_16, maxlen)) {
      conv_utf_16_to_8(fullname_16, fullname, maxlen);
      if (!BLI_exists(fullname)) {
        CLOG_ERROR(&LOG, "path can't be found: \"%.*s\"", (int)maxlen, fullname);
        MessageBox(
            NULL, "path contains invalid characters or is too long (see console)", "Error", MB_OK);
      }
      MEM_freeN(fullname_16);
      return;
    }

    MEM_freeN(fullname_16);
  }
#endif

  /* unix and non linux */
  if (name && name[0]) {

    BLI_strncpy(fullname, name, maxlen);
    if (name[0] == '.') {
      BLI_path_cwd(fullname, maxlen);
#ifdef _WIN32
      BLI_path_program_extensions_add_win32(fullname, maxlen);
#endif
    }
    else if (BLI_last_slash(name)) {
      // full path
      BLI_strncpy(fullname, name, maxlen);
#ifdef _WIN32
      BLI_path_program_extensions_add_win32(fullname, maxlen);
#endif
    }
    else {
      BLI_path_program_search(fullname, maxlen, name);
    }
    /* Remove "/./" and "/../" so string comparisons can be used on the path. */
    BLI_cleanup_path(NULL, fullname);

#if defined(DEBUG)
    if (!STREQ(name, fullname)) {
      CLOG_INFO(&LOG, 2, "guessing '%s' == '%s'", name, fullname);
    }
#endif
  }
}

void BKE_appdir_program_path_init(const char *argv0)
{
  where_am_i(bprogname, sizeof(bprogname), argv0);
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

bool BKE_appdir_program_python_search(char *fullpath,
                                      const size_t fullpath_len,
                                      const int version_major,
                                      const int version_minor)
{
#ifdef PYTHON_EXECUTABLE_NAME
  /* passed in from the build-systems 'PYTHON_EXECUTABLE' */
  const char *python_build_def = STRINGIFY(PYTHON_EXECUTABLE_NAME);
#endif
  const char *basename = "python";
  char python_ver[16];
  /* check both possible names */
  const char *python_names[] = {
#ifdef PYTHON_EXECUTABLE_NAME
      python_build_def,
#endif
      python_ver,
      basename,
  };
  int i;

  bool is_found = false;

  BLI_snprintf(python_ver, sizeof(python_ver), "%s%d.%d", basename, version_major, version_minor);

  {
    const char *python_bin_dir = BKE_appdir_folder_id(BLENDER_SYSTEM_PYTHON, "bin");
    if (python_bin_dir) {

      for (i = 0; i < ARRAY_SIZE(python_names); i++) {
        BLI_join_dirfile(fullpath, fullpath_len, python_bin_dir, python_names[i]);

        if (
#ifdef _WIN32
            BLI_path_program_extensions_add_win32(fullpath, fullpath_len)
#else
            BLI_exists(fullpath)
#endif
        ) {
          is_found = true;
          break;
        }
      }
    }
  }

  if (is_found == false) {
    for (i = 0; i < ARRAY_SIZE(python_names); i++) {
      if (BLI_path_program_search(fullpath, fullpath_len, python_names[i])) {
        is_found = true;
        break;
      }
    }
  }

  if (is_found == false) {
    *fullpath = '\0';
  }

  return is_found;
}

/** Keep in sync with `bpy.utils.app_template_paths()` */
static const char *app_template_directory_search[2] = {
    "startup" SEP_STR "bl_app_templates_user",
    "startup" SEP_STR "bl_app_templates_system",
};

static const int app_template_directory_id[2] = {
    /* Only 'USER' */
    BLENDER_USER_SCRIPTS,
    /* Covers 'LOCAL' & 'SYSTEM'. */
    BLENDER_SYSTEM_SCRIPTS,
};

/**
 * Return true if templates exist
 */
bool BKE_appdir_app_template_any(void)
{
  char temp_dir[FILE_MAX];
  for (int i = 0; i < 2; i++) {
    if (BKE_appdir_folder_id_ex(app_template_directory_id[i],
                                app_template_directory_search[i],
                                temp_dir,
                                sizeof(temp_dir))) {
      return true;
    }
  }
  return false;
}

bool BKE_appdir_app_template_id_search(const char *app_template, char *path, size_t path_len)
{
  for (int i = 0; i < 2; i++) {
    char subdir[FILE_MAX];
    BLI_join_dirfile(subdir, sizeof(subdir), app_template_directory_search[i], app_template);
    if (BKE_appdir_folder_id_ex(app_template_directory_id[i], subdir, path, path_len)) {
      return true;
    }
  }
  return false;
}

bool BKE_appdir_app_template_has_userpref(const char *app_template)
{
  /* Test if app template provides a userpref.blend.
   * If not, we will share user preferences with the rest of Blender. */
  if (!app_template && app_template[0]) {
    return false;
  }

  char app_template_path[FILE_MAX];
  if (!BKE_appdir_app_template_id_search(
          app_template, app_template_path, sizeof(app_template_path))) {
    return false;
  }

  char userpref_path[FILE_MAX];
  BLI_path_join(
      userpref_path, sizeof(userpref_path), app_template_path, BLENDER_USERPREF_FILE, NULL);
  return BLI_exists(userpref_path);
}

void BKE_appdir_app_templates(ListBase *templates)
{
  BLI_listbase_clear(templates);

  for (int i = 0; i < 2; i++) {
    char subdir[FILE_MAX];
    if (!BKE_appdir_folder_id_ex(app_template_directory_id[i],
                                 app_template_directory_search[i],
                                 subdir,
                                 sizeof(subdir))) {
      continue;
    }

    struct direntry *dir;
    uint totfile = BLI_filelist_dir_contents(subdir, &dir);
    for (int f = 0; f < totfile; f++) {
      if (!FILENAME_IS_CURRPAR(dir[f].relname) && S_ISDIR(dir[f].type)) {
        char *template = BLI_strdup(dir[f].relname);
        BLI_addtail(templates, BLI_genericNodeN(template));
      }
    }

    BLI_filelist_free(dir, totfile);
  }
}

/**
 * Gets the temp directory when blender first runs.
 * If the default path is not found, use try $TEMP
 *
 * Also make sure the temp dir has a trailing slash
 *
 * \param fullname: The full path to the temporary temp directory
 * \param basename: The full path to the persistent temp directory (may be NULL)
 * \param maxlen: The size of the fullname buffer
 * \param userdir: Directory specified in user preferences
 */
static void where_is_temp(char *fullname, char *basename, const size_t maxlen, char *userdir)
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
    const char *tmp = BLI_getenv("TEMP"); /* Windows */
    if (tmp && BLI_is_dir(tmp)) {
      BLI_strncpy(fullname, tmp, maxlen);
    }
  }
#else
  /* Other OS's - Try TMP and TMPDIR */
  if (fullname[0] == '\0') {
    const char *tmp = BLI_getenv("TMP");
    if (tmp && BLI_is_dir(tmp)) {
      BLI_strncpy(fullname, tmp, maxlen);
    }
  }

  if (fullname[0] == '\0') {
    const char *tmp = BLI_getenv("TMPDIR");
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
      /* also set user pref to show %TEMP%. /tmp/ is just plain confusing for Windows users. */
      BLI_strncpy(userdir, fullname, maxlen);
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
      if (mkdtemp(tmp_name) == NULL) {
        BLI_dir_create_recursive(tmp_name);
      }
#endif
    }
    if (BLI_is_dir(tmp_name)) {
      BLI_strncpy(basename, fullname, maxlen);
      BLI_strncpy(fullname, tmp_name, maxlen);
      BLI_add_slash(fullname);
    }
    else {
      CLOG_WARN(&LOG,
                "Could not generate a temp file name for '%s', falling back to '%s'",
                tmp_name,
                fullname);
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
  where_is_temp(btempdir_session, btempdir_base, FILE_MAX, userdir);
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
  where_is_temp(dir, NULL, FILE_MAX, NULL);
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

/* Gets a good default directory for fonts */
bool BKE_appdir_font_folder_default(char *dir)
{
  bool success = false;
#ifdef WIN32
  wchar_t wpath[FILE_MAXDIR];
  success = SHGetSpecialFolderPathW(0, wpath, CSIDL_FONTS, 0);
  BLI_strncpy_wchar_as_utf8(dir, wpath, FILE_MAXDIR);
#endif
  /* TODO: Values for other platforms. */
  UNUSED_VARS(dir);
  return success;
}
