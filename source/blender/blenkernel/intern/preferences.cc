/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * User defined asset library API.
 */

#include <cstring>

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "BKE_appdir.hh"
#include "BKE_asset.hh"
#include "BKE_preferences.h"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "DNA_defaults.h"
#include "DNA_userdef_types.h"

#define U BLI_STATIC_ASSERT(false, "Global 'U' not allowed, only use arguments passed in!")

/* -------------------------------------------------------------------- */
/** \name Preferences File
 * \{ */

namespace blender::bke::preferences {

bool exists()
{
  const std::optional<std::string> cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, nullptr);
  if (!cfgdir.has_value()) {
    return false;
  }

  char userpref[FILE_MAX];
  BLI_path_join(userpref, sizeof(userpref), cfgdir->c_str(), BLENDER_USERPREF_FILE);
  return BLI_exists(userpref);
}

}  // namespace blender::bke::preferences

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Libraries
 * \{ */

bUserAssetLibrary *BKE_preferences_asset_library_add(UserDef *userdef,
                                                     const char *name,
                                                     const char *dirpath)
{
  bUserAssetLibrary *library = DNA_struct_default_alloc(bUserAssetLibrary);

  BLI_addtail(&userdef->asset_libraries, library);
  if (userdef->experimental.no_data_block_packing) {
    library->import_method = ASSET_IMPORT_APPEND_REUSE;
  }
  if (name) {
    BKE_preferences_asset_library_name_set(userdef, library, name);
  }
  if (dirpath) {
    STRNCPY(library->dirpath, dirpath);
  }

  return library;
}

void BKE_preferences_asset_library_remove(UserDef *userdef, bUserAssetLibrary *library)
{
  BLI_freelinkN(&userdef->asset_libraries, library);
}

void BKE_preferences_asset_library_name_set(UserDef *userdef,
                                            bUserAssetLibrary *library,
                                            const char *name)
{
  STRNCPY_UTF8(library->name, name);
  BLI_uniquename(&userdef->asset_libraries,
                 library,
                 name,
                 '.',
                 offsetof(bUserAssetLibrary, name),
                 sizeof(library->name));
}

void BKE_preferences_asset_library_path_set(bUserAssetLibrary *library, const char *path)
{
  STRNCPY(library->dirpath, path);
  if (BLI_is_file(library->dirpath)) {
    BLI_path_parent_dir(library->dirpath);
  }
}

bUserAssetLibrary *BKE_preferences_asset_library_find_index(const UserDef *userdef, int index)
{
  return static_cast<bUserAssetLibrary *>(BLI_findlink(&userdef->asset_libraries, index));
}

bUserAssetLibrary *BKE_preferences_asset_library_find_by_name(const UserDef *userdef,
                                                              const char *name)
{
  return static_cast<bUserAssetLibrary *>(
      BLI_findstring(&userdef->asset_libraries, name, offsetof(bUserAssetLibrary, name)));
}

bUserAssetLibrary *BKE_preferences_asset_library_containing_path(const UserDef *userdef,
                                                                 const char *path)
{
  LISTBASE_FOREACH (bUserAssetLibrary *, asset_lib_pref, &userdef->asset_libraries) {
    if (asset_lib_pref->dirpath[0] && BLI_path_contains(asset_lib_pref->dirpath, path)) {
      return asset_lib_pref;
    }
  }
  return nullptr;
}

int BKE_preferences_asset_library_get_index(const UserDef *userdef,
                                            const bUserAssetLibrary *library)
{
  return BLI_findindex(&userdef->asset_libraries, library);
}

void BKE_preferences_asset_library_default_add(UserDef *userdef)
{
  char documents_path[FILE_MAXDIR];

  /* No home or documents path found, not much we can do. */
  if (!BKE_appdir_folder_documents(documents_path) || !documents_path[0]) {
    return;
  }

  bUserAssetLibrary *library = BKE_preferences_asset_library_add(
      userdef, DATA_(BKE_PREFS_ASSET_LIBRARY_DEFAULT_NAME), nullptr);

  /* Add new "Default" library under '[doc_path]/Blender/Assets'. */
  BLI_path_join(
      library->dirpath, sizeof(library->dirpath), documents_path, N_("Blender"), N_("Assets"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extension Repositories
 * \{ */

/**
 * A string copy that ensures: `[A-Za-z]+[A-Za-z0-9_]*`.
 */
static size_t strncpy_py_module(char *dst, const char *src, const size_t dst_maxncpy)
{
  const size_t dst_len_max = dst_maxncpy - 1;
  dst[0] = '\0';
  size_t i_src = 0, i_dst = 0;
  while (src[i_src] && (i_dst < dst_len_max)) {
    const char c = src[i_src++];
    const bool is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    /* The first character must be `[a-zA-Z]`. */
    if (i_dst == 0 && !is_alpha) {
      continue;
    }
    const bool is_num = (is_alpha == false) && ((c >= '0' && c <= '9') || c == '_');
    if (!(is_alpha || is_num)) {
      continue;
    }
    dst[i_dst++] = c;
  }
  dst[i_dst] = '\0';
  return i_dst;
}

bUserExtensionRepo *BKE_preferences_extension_repo_add(UserDef *userdef,
                                                       const char *name,
                                                       const char *module,
                                                       const char *custom_dirpath)
{
  bUserExtensionRepo *repo = DNA_struct_default_alloc(bUserExtensionRepo);
  BLI_addtail(&userdef->extension_repos, repo);

  /* Set the unique ID-name. */
  BKE_preferences_extension_repo_name_set(userdef, repo, name);

  /* Set the unique module-name. */
  BKE_preferences_extension_repo_module_set(userdef, repo, module);

  /* Set the directory. */
  STRNCPY(repo->custom_dirpath, custom_dirpath);
  BLI_path_normalize(repo->custom_dirpath);
  BLI_path_slash_rstrip(repo->custom_dirpath);

  /* While not a strict rule, ignored paths that already exist, *
   * pointing to the same path is going to logical problems with package-management. */
  LISTBASE_FOREACH (const bUserExtensionRepo *, repo_iter, &userdef->extension_repos) {
    if (repo == repo_iter) {
      continue;
    }
    if (BLI_path_cmp(repo->custom_dirpath, repo_iter->custom_dirpath) == 0) {
      repo->custom_dirpath[0] = '\0';
      break;
    }
  }

  return repo;
}

void BKE_preferences_extension_repo_remove(UserDef *userdef, bUserExtensionRepo *repo)
{
  BLI_freelinkN(&userdef->extension_repos, repo);
}

bUserExtensionRepo *BKE_preferences_extension_repo_add_default_remote(UserDef *userdef)
{
  bUserExtensionRepo *repo = BKE_preferences_extension_repo_add(
      userdef, "extensions.blender.org", "blender_org", "");
  /* The trailing slash on this URL is important, without it a redirect is used. */
  STRNCPY(repo->remote_url, "https://extensions.blender.org/api/v1/extensions/");
  /* Disable `blender.org` by default, the initial "Online Preferences" section gives
   * the option to enable this. */
  repo->flag |= USER_EXTENSION_REPO_FLAG_USE_REMOTE_URL | USER_EXTENSION_REPO_FLAG_SYNC_ON_STARTUP;
  return repo;
}

bUserExtensionRepo *BKE_preferences_extension_repo_add_default_user(UserDef *userdef)
{
  bUserExtensionRepo *repo = BKE_preferences_extension_repo_add(
      userdef, "User Default", "user_default", "");
  return repo;
}

bUserExtensionRepo *BKE_preferences_extension_repo_add_default_system(UserDef *userdef)
{
  bUserExtensionRepo *repo = BKE_preferences_extension_repo_add(userdef, "System", "system", "");
  repo->source = USER_EXTENSION_REPO_SOURCE_SYSTEM;
  return repo;
}

void BKE_preferences_extension_repo_add_defaults_all(UserDef *userdef)
{
  BLI_assert(BLI_listbase_is_empty(&userdef->extension_repos));
  BKE_preferences_extension_repo_add_default_remote(userdef);
  BKE_preferences_extension_repo_add_default_user(userdef);
  BKE_preferences_extension_repo_add_default_system(userdef);
}

void BKE_preferences_extension_repo_name_set(UserDef *userdef,
                                             bUserExtensionRepo *repo,
                                             const char *name)
{
  if (*name == '\0') {
    name = "User Repository";
  }
  STRNCPY_UTF8(repo->name, name);

  BLI_uniquename(&userdef->extension_repos,
                 repo,
                 name,
                 '.',
                 offsetof(bUserExtensionRepo, name),
                 sizeof(repo->name));
}

void BKE_preferences_extension_repo_module_set(UserDef *userdef,
                                               bUserExtensionRepo *repo,
                                               const char *module)
{
  if (strncpy_py_module(repo->module, module, sizeof(repo->module)) == 0) {
    STRNCPY(repo->module, "repository");
  }

  BLI_uniquename(&userdef->extension_repos,
                 repo,
                 module,
                 '_',
                 offsetof(bUserExtensionRepo, module),
                 sizeof(repo->module));
}

bool BKE_preferences_extension_repo_module_is_valid(const bUserExtensionRepo *repo)
{
  /* NOTE: this should only ever return false in the case of corrupt file/memory
   * and can be considered an exceptional situation. */
  char module_test[sizeof(bUserExtensionRepo::module)];
  const size_t module_len = strncpy_py_module(module_test, repo->module, sizeof(repo->module));
  if (module_len == 0) {
    return false;
  }
  if (module_len != STRNLEN(repo->module)) {
    return false;
  }
  return true;
}

void BKE_preferences_extension_repo_custom_dirpath_set(bUserExtensionRepo *repo, const char *path)
{
  STRNCPY(repo->custom_dirpath, path);
}

size_t BKE_preferences_extension_repo_dirpath_get(const bUserExtensionRepo *repo,
                                                  char *dirpath,
                                                  const int dirpath_maxncpy)
{
  if (repo->flag & USER_EXTENSION_REPO_FLAG_USE_CUSTOM_DIRECTORY) {
    return BLI_strncpy_rlen(dirpath, repo->custom_dirpath, dirpath_maxncpy);
  }

  std::optional<std::string> path = std::nullopt;

  uint8_t source = repo->source;
  if (repo->flag & USER_EXTENSION_REPO_FLAG_USE_REMOTE_URL) {
    source = USER_EXTENSION_REPO_SOURCE_USER;
  }

  switch (source) {
    case USER_EXTENSION_REPO_SOURCE_SYSTEM: {
      path = BKE_appdir_folder_id(BLENDER_SYSTEM_EXTENSIONS, nullptr);
      break;
    }
    default: { /* #USER_EXTENSION_REPO_SOURCE_USER. */
      path = BKE_appdir_folder_id_user_notest(BLENDER_USER_EXTENSIONS, nullptr);
      break;
    }
  }

  /* Highly unlikely to fail as the directory doesn't have to exist. */
  if (!path) {
    dirpath[0] = '\0';
    return 0;
  }
  return BLI_path_join(dirpath, dirpath_maxncpy, path.value().c_str(), repo->module);
}

size_t BKE_preferences_extension_repo_user_dirpath_get(const bUserExtensionRepo *repo,
                                                       char *dirpath,
                                                       const int dirpath_maxncpy)
{
  if (std::optional<std::string> path = BKE_appdir_folder_id_user_notest(BLENDER_USER_EXTENSIONS,
                                                                         nullptr))
  {
    return BLI_path_join(dirpath, dirpath_maxncpy, path.value().c_str(), ".user", repo->module);
  }
  return 0;
}

bUserExtensionRepo *BKE_preferences_extension_repo_find_index(const UserDef *userdef, int index)
{
  return static_cast<bUserExtensionRepo *>(BLI_findlink(&userdef->extension_repos, index));
}

bUserExtensionRepo *BKE_preferences_extension_repo_find_by_module(const UserDef *userdef,
                                                                  const char *module)
{
  return static_cast<bUserExtensionRepo *>(
      BLI_findstring(&userdef->extension_repos, module, offsetof(bUserExtensionRepo, module)));
}

static bool url_char_is_delimiter(const char ch)
{
  /* Punctuation (space to comma). */
  if (ch >= 32 && ch <= 44) {
    return true;
  }
  /* Other characters (colon to at-sign). */
  if (ch >= 58 && ch <= 64) {
    return true;
  }
  if (ELEM(ch, '/', '\\')) {
    return true;
  }
  return false;
}

bUserExtensionRepo *BKE_preferences_extension_repo_find_by_remote_url_prefix(
    const UserDef *userdef, const char *remote_url_full, const bool only_enabled)
{
  const int path_full_len = strlen(remote_url_full);
  const int path_full_offset = BKE_preferences_extension_repo_remote_scheme_end(remote_url_full);

  LISTBASE_FOREACH (bUserExtensionRepo *, repo, &userdef->extension_repos) {
    if (only_enabled && (repo->flag & USER_EXTENSION_REPO_FLAG_DISABLED)) {
      continue;
    }

    /* Has a valid remote path to check. */
    if ((repo->flag & USER_EXTENSION_REPO_FLAG_USE_REMOTE_URL) == 0) {
      continue;
    }
    if (repo->remote_url[0] == '\0') {
      continue;
    }

    /* Set path variables which may be offset by the "scheme". */
    const char *path_repo = repo->remote_url;
    const char *path_test = remote_url_full;
    int path_test_len = path_full_len;

    /* Allow paths beginning with both `http` & `https` to be considered equivalent.
     * This is done by skipping the "scheme" prefix both have a scheme. */
    if (path_full_offset) {
      const int path_repo_offset = BKE_preferences_extension_repo_remote_scheme_end(path_repo);
      if (path_repo_offset) {
        path_repo += path_repo_offset;
        path_test += path_full_offset;
        path_test_len -= path_full_offset;
      }
    }

    /* The length of the path without trailing slashes. */
    int path_repo_len = strlen(path_repo);
    while (path_repo_len && ELEM(path_repo[path_repo_len - 1], '/', '\\')) {
      path_repo_len--;
    }

    if (path_test_len <= path_repo_len) {
      continue;
    }
    if (memcmp(path_repo, path_test, path_repo_len) != 0) {
      continue;
    }

    /* A delimiter must follow to ensure `path_test` doesn't reference a longer host-name.
     * Will typically be a `/` or a `:`. */
    if (!url_char_is_delimiter(path_test[path_repo_len])) {
      continue;
    }
    return repo;
  }
  return nullptr;
}

int BKE_preferences_extension_repo_remote_scheme_end(const char *url)
{
  /* Technically the "://" are not part of the scheme, so subtract 3 from the return value. */
  const char *scheme_check[] = {
      "http://",
      "https://",
      "file://",
  };
  for (int i = 0; i < ARRAY_SIZE(scheme_check); i++) {
    const char *scheme = scheme_check[i];
    int scheme_len = strlen(scheme);
    if (strncmp(url, scheme, scheme_len) == 0) {
      return scheme_len - 3;
    }
  }
  return 0;
}

void BKE_preferences_extension_remote_to_name(const char *remote_url,
                                              char name[sizeof(bUserExtensionRepo::name)])
{
#ifdef _WIN32
  const bool is_win32 = true;
#else
  const bool is_win32 = false;
#endif
  const bool is_file = STRPREFIX(remote_url, "file://");
  name[0] = '\0';
  if (int offset = BKE_preferences_extension_repo_remote_scheme_end(remote_url)) {
    /* Skip the `://`. */
    remote_url += (offset + 3);

    if (is_file) {
      if (is_win32) {
        /* Skip the slash prefix for: `/C:/`,
         * not *required* but seems like a bug if it's not done. */
        if (remote_url[0] == '/' && isalpha(remote_url[1]) && (remote_url[2] == ':')) {
          remote_url += 1;
        }
      }
    }
    else {
      /* Skip the `www` as it's not useful information. */
      if (BLI_str_startswith(remote_url, "www.")) {
        remote_url += 4;
      }
    }
  }
  if (UNLIKELY(remote_url[0] == '\0')) {
    return;
  }

  const char *c = remote_url;
  if (is_file) {
    /* TODO: decode the URL, see: #GHOST_URL_decode which is not a public function. */

    /* Don't use domain name only logic for file paths as this causes
     * `file:///path/to/repo/index.json` -> `/path`
     * In this case `/path/to/repo` is preferred. */
    c = BLI_path_basename(remote_url);
    /* Remove trailing slash. */
    while ((remote_url < c) && url_char_is_delimiter(*(c - 1))) {
      c--;
    }
  }
  else {
    /* Skip any delimiters (likely forward slashes for `file:///` on UNIX).
     * Although the `file://` case is handled already. So this is quite unlikely.
     * Skip them anyway because failing to do so may cause the domain to be an empty string. */
    while (*c && url_char_is_delimiter(*c)) {
      c++;
    }
    /* Skip the domain name. */
    while (*c && !url_char_is_delimiter(*c)) {
      c++;
    }
  }

  BLI_strncpy_utf8(
      name, remote_url, std::min(size_t(c - remote_url) + 1, sizeof(bUserExtensionRepo::name)));

  if (is_win32) {
    if (is_file) {
      BLI_path_slash_native(name);
    }
  }
}

int BKE_preferences_extension_repo_get_index(const UserDef *userdef,
                                             const bUserExtensionRepo *repo)
{
  return BLI_findindex(&userdef->extension_repos, repo);
}

void BKE_preferences_extension_repo_read_data(BlendDataReader *reader, bUserExtensionRepo *repo)
{
  if (repo->access_token) {
    BLO_read_string(reader, &repo->access_token);
  }
}

void BKE_preferences_extension_repo_write_data(BlendWriter *writer, const bUserExtensionRepo *repo)
{
  if (repo->access_token) {
    BLO_write_string(writer, repo->access_token);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #bUserAssetShelfSettings
 * \{ */

static bUserAssetShelfSettings *asset_shelf_settings_new(UserDef *userdef,
                                                         const char *shelf_idname)
{
  bUserAssetShelfSettings *settings = DNA_struct_default_alloc(bUserAssetShelfSettings);
  BLI_addtail(&userdef->asset_shelves_settings, settings);
  STRNCPY(settings->shelf_idname, shelf_idname);
  BLI_assert(BLI_listbase_is_empty(&settings->enabled_catalog_paths));
  return settings;
}

static bUserAssetShelfSettings *asset_shelf_settings_ensure(UserDef *userdef,
                                                            const char *shelf_idname)
{
  if (bUserAssetShelfSettings *settings = BKE_preferences_asset_shelf_settings_get(userdef,
                                                                                   shelf_idname))
  {
    return settings;
  }
  return asset_shelf_settings_new(userdef, shelf_idname);
}

bUserAssetShelfSettings *BKE_preferences_asset_shelf_settings_get(const UserDef *userdef,
                                                                  const char *shelf_idname)
{
  return static_cast<bUserAssetShelfSettings *>(
      BLI_findstring(&userdef->asset_shelves_settings,
                     shelf_idname,
                     offsetof(bUserAssetShelfSettings, shelf_idname)));
}

bool BKE_preferences_asset_shelf_settings_is_catalog_path_enabled(const UserDef *userdef,
                                                                  const char *shelf_idname,
                                                                  const char *catalog_path)
{
  const bUserAssetShelfSettings *settings = BKE_preferences_asset_shelf_settings_get(userdef,
                                                                                     shelf_idname);
  if (!settings) {
    return false;
  }
  return BKE_asset_catalog_path_list_has_path(settings->enabled_catalog_paths, catalog_path);
}

bool BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(UserDef *userdef,
                                                                      const char *shelf_idname,
                                                                      const char *catalog_path)
{
  if (BKE_preferences_asset_shelf_settings_is_catalog_path_enabled(
          userdef, shelf_idname, catalog_path))
  {
    return false;
  }

  bUserAssetShelfSettings *settings = asset_shelf_settings_ensure(userdef, shelf_idname);
  BKE_asset_catalog_path_list_add_path(settings->enabled_catalog_paths, catalog_path);
  return true;
}

/** \} */
