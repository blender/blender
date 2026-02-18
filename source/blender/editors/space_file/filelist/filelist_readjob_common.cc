/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "AS_asset_library.hh"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_stack.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "BKE_asset.hh"
#include "BKE_blendfile.hh"
#include "BKE_idtype.hh"

#ifdef WIN32
#  include "BKE_appdir.hh"
#  include "BLF_api.hh"
#  include "BLI_winstuff.h"
#endif

#include "DNA_space_enums.h"

#include "ED_file_indexer.hh"
#include "ED_fileselect.hh"

#include "filelist_intern.hh"

#include "filelist_readjob.hh"

namespace blender {

/* helper, could probably go in BKE actually? */
static int groupname_to_code(const char *group)
{
  char buf[BLO_GROUP_MAX];
  char *lslash;

  BLI_assert(group);

  STRNCPY(buf, group);
  lslash = const_cast<char *>(BLI_path_slash_rfind(buf));
  if (lslash) {
    lslash[0] = '\0';
  }

  return buf[0] ? BKE_idtype_idcode_from_name(buf) : 0;
}

/**
 * Append \a filename (or even a path inside of a .blend, like `Material/Material.001`), to the
 * current relative path being read within the filelist root. The returned string needs freeing
 * with #MEM_delete().
 */
char *current_relpath_append(const FileListReadJob *job_params, const char *filename)
{
  char relbase[sizeof(job_params->cur_relbase)];
  STRNCPY(relbase, job_params->cur_relbase);

  /* Early exit, nothing to join. */
  if (!relbase[0]) {
    return BLI_strdup(filename);
  }

  BLI_path_slash_ensure(relbase, sizeof(relbase));

  char relpath[FILE_MAX_LIBEXTRA];
  /* Using #BLI_path_join works but isn't needed as `rel_subdir` has a trailing slash. */
  BLI_string_join(relpath,
                  sizeof(relpath),
                  /* + 2 to remove "//" relative path prefix. */
                  BLI_path_is_rel(relbase) ? relbase + 2 : relbase,
                  filename);

  return BLI_strdup(relpath);
}

/* -------------------------------------------------------------------- */
/** \name Common callbacks.
 * \{ */

bool filelist_checkdir_return_always_valid(const FileList * /*filelist*/,
                                           char /*dirpath*/[FILE_MAX_LIBEXTRA],
                                           const bool /*do_change*/)
{
  return true;
}

static void parent_dir_until_exists_or_default_root(char *dir)
{
  /* Only allow absolute paths as CWD relative doesn't make sense from the UI. */
  if (BLI_path_is_abs_from_cwd(dir) && BLI_path_parent_dir_until_exists(dir)) {
    return;
  }

#ifdef WIN32
  BLI_windows_get_default_root_dir(dir);
#else
  ARRAY_SET_ITEMS(dir, '/', '\0');
#endif
}

bool filelist_checkdir_dir(const FileList * /*filelist*/,
                           char dirpath[FILE_MAX_LIBEXTRA],
                           const bool do_change)
{
  bool is_valid;
  if (do_change) {
    parent_dir_until_exists_or_default_root(dirpath);
    is_valid = true;
  }
  else {
    is_valid = BLI_path_is_abs_from_cwd(dirpath) && BLI_is_dir(dirpath);
  }
  return is_valid;
}

bool filelist_checkdir_lib(const FileList * /*filelist*/,
                           char dirpath[FILE_MAX_LIBEXTRA],
                           const bool do_change)
{
  char tdir[FILE_MAX_LIBEXTRA];
  char *name;

  const bool is_valid = (BLI_is_dir(dirpath) ||
                         (BKE_blendfile_library_path_explode(dirpath, tdir, nullptr, &name) &&
                          BLI_is_file(tdir) && !name));

  if (do_change && !is_valid) {
    /* if not a valid library, we need it to be a valid directory! */
    parent_dir_until_exists_or_default_root(dirpath);
    return true;
  }
  return is_valid;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File-list Directory/Library Reading
 * \{ */

/**
 * \return True if new entries were added to the file list.
 */
bool filelist_readjob_append_entries(FileListReadJob *job_params,
                                     ListBaseT<FileListInternEntry> *from_entries,
                                     int from_entries_num)
{
  BLI_assert(BLI_listbase_count(from_entries) == from_entries_num);
  if (from_entries_num <= 0) {
    return false;
  }

  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */
  std::scoped_lock lock(job_params->lock);
  BLI_movelisttolist(&filelist->filelist.entries, from_entries);
  filelist->filelist.entries_num += from_entries_num;

  return true;
}

#ifdef WIN32
static int filelist_add_userfonts_regpath(HKEY hKeyParent,
                                          LPCSTR subkeyName,
                                          ListBaseT<FileListInternEntry> *entries)
{
  int font_num = 0;
  HKEY key = 0;
  /* Try to open the requested key. */
  if (RegOpenKeyExA(hKeyParent, subkeyName, 0, KEY_ALL_ACCESS, &key) != ERROR_SUCCESS) {
    return 0;
  }

  DWORD index = 0;
  /* Value name and data buffers (ANSI). */
  TCHAR KeyName[255];
  DWORD KeyNameLen = sizeof(KeyName);
  TCHAR KeyValue[FILE_MAX];
  DWORD KeyValueLen = sizeof(KeyValue);
  DWORD valueType;

  /* Enumerate values. */
  while (RegEnumValueA(key,
                       index,
                       (LPSTR)&KeyName,
                       &KeyNameLen,
                       NULL,
                       &valueType,
                       (LPBYTE)&KeyValue,
                       &KeyValueLen) == ERROR_SUCCESS)
  {
    /* Only consider string values (paths). */
    if (valueType == REG_SZ || valueType == REG_EXPAND_SZ) {
      FileListInternEntry *entry = MEM_new<FileListInternEntry>(__func__);
      /* Find last slash to determine basename/relpath portion. */
      const char *val_str = (const char *)KeyValue;
      const char *lslash_str = BLI_path_slash_rfind(val_str);
      const size_t lslash = lslash_str ? (size_t)(lslash_str - val_str) + 1 : 0;

      BLI_stat(val_str, &entry->st);
      entry->relpath = BLI_strdup(val_str + lslash);
      entry->name = BLF_display_name_from_file(val_str);
      entry->free_name = true;
      entry->attributes = FILE_ATTR_READONLY & FILE_ATTR_ALIAS;
      entry->typeflag = FILE_TYPE_FTFONT;
      entry->redirection_path = BLI_strdup(val_str);
      BLI_addtail(entries, entry);
      font_num++;
    }

    KeyNameLen = sizeof(KeyName);
    KeyValueLen = sizeof(KeyValue);
    index++;
  }

  /* Enumerate sub-keys and recurse into them. */
  index = 0;
  while (RegEnumKeyExA(key, index, (LPSTR)&KeyName, &KeyNameLen, NULL, NULL, NULL, NULL) ==
         ERROR_SUCCESS)
  {
    font_num += filelist_add_userfonts_regpath(key, KeyName, entries);
    KeyNameLen = sizeof(KeyName);
    index++;
  }

  RegCloseKey(key);
  return font_num;
}

static int filelist_add_userfonts(ListBaseT<FileListInternEntry> *entries)
{
  return filelist_add_userfonts_regpath(
      HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", entries);
}

#endif

static int filelist_readjob_list_dir(FileListReadJob *job_params,
                                     const char *root,
                                     ListBaseT<FileListInternEntry> *entries,
                                     const char *filter_glob,
                                     const bool do_lib,
                                     const char *main_filepath,
                                     const bool skip_currpar)
{
  direntry *files;
  int entries_num = 0;
  /* Full path of the item. */
  char full_path[FILE_MAX];

#ifdef WIN32
  char fonts_path[FILE_MAXDIR] = {0};
  BKE_appdir_font_folder_default(fonts_path, sizeof(fonts_path));
  BLI_path_slash_ensure(fonts_path, sizeof(fonts_path));
  if (STREQ(root, fonts_path)) {
    entries_num += filelist_add_userfonts(entries);
  }
#endif

  const int files_num = BLI_filelist_dir_contents(root, &files);
  if (files) {
    int i = files_num;
    while (i--) {
      FileListInternEntry *entry;

      if (skip_currpar && FILENAME_IS_CURRPAR(files[i].relname)) {
        continue;
      }

      entry = MEM_new<FileListInternEntry>(__func__);
      entry->relpath = current_relpath_append(job_params, files[i].relname);
      entry->st = files[i].s;

      BLI_path_join(full_path, FILE_MAX, root, files[i].relname);
      char *target = full_path;

      /* Set initial file type and attributes. */
      entry->attributes = BLI_file_attributes(full_path);
      if (S_ISDIR(files[i].s.st_mode)
#ifdef __APPLE__
          && !(ED_path_extension_type(full_path) & FILE_TYPE_BUNDLE)
#endif
      )
      {
        entry->typeflag = FILE_TYPE_DIR;
      }

      /* Is this a file that points to another file? */
      if (entry->attributes & FILE_ATTR_ALIAS) {
        entry->redirection_path = MEM_new_array_zeroed<char>(FILE_MAXDIR, __func__);
        if (BLI_file_alias_target(full_path, entry->redirection_path)) {
          if (BLI_is_dir(entry->redirection_path)) {
            entry->typeflag = FILE_TYPE_DIR;
            BLI_path_slash_ensure(entry->redirection_path, FILE_MAXDIR);
          }
          else {
            entry->typeflag = eFileSel_File_Types(ED_path_extension_type(entry->redirection_path));
          }
          target = entry->redirection_path;
#ifdef WIN32
          /* On Windows don't show `.lnk` extension for valid shortcuts. */
          BLI_path_extension_strip(entry->relpath);
#endif
        }
        else {
          MEM_delete(entry->redirection_path);
          entry->redirection_path = nullptr;
          entry->attributes |= FILE_ATTR_HIDDEN;
        }
      }

      if (!(entry->typeflag & FILE_TYPE_DIR)) {
        if (do_lib && BKE_blendfile_extension_check(target)) {
          /* If we are considering .blend files as libraries, promote them to directory status. */
          entry->typeflag = FILE_TYPE_BLENDER;
          /* prevent current file being used as acceptable dir */
          if (BLI_path_cmp(main_filepath, target) != 0) {
            entry->typeflag |= FILE_TYPE_DIR;
          }
        }
        else {
          entry->typeflag = eFileSel_File_Types(ED_path_extension_type(target));
          if (filter_glob[0] && BLI_path_extension_check_glob(target, filter_glob)) {
            entry->typeflag |= FILE_TYPE_OPERATOR;
          }
        }
      }

#ifndef WIN32
      /* Set linux-style dot files hidden too. */
      if (BLI_path_has_hidden_component(entry->relpath)) {
        entry->attributes |= FILE_ATTR_HIDDEN;
      }
#endif

      BLI_addtail(entries, entry);
      entries_num++;
    }
    BLI_filelist_free(files, files_num);
  }
  return entries_num;
}

/**
 * From here, we are in 'Job Context',
 * i.e. have to be careful about sharing stuff between background working thread.
 * and main one (used by UI among other things).
 */
struct TodoDir {
  int level;
  char *dir;
};

/**
 * Structure to keep the file indexer and its user data together.
 */
struct FileIndexer {
  const FileIndexerType *callbacks;

  /**
   * User data. Contains the result of `callbacks.init_user_data`.
   */
  void *user_data;
};

enum ListLibOptions {
  LIST_LIB_OPTION_NONE = 0,

  /* Will read both the groups + actual ids from the library. Reduces the amount of times that
   * a library needs to be opened. */
  LIST_LIB_RECURSIVE = (1 << 0),

  /* Will only list assets. */
  LIST_LIB_ASSETS_ONLY = (1 << 1),

  /* Add given root as result. */
  LIST_LIB_ADD_PARENT = (1 << 2),
};
ENUM_OPERATORS(ListLibOptions);

static FileListInternEntry *filelist_readjob_list_lib_group_create(
    const FileListReadJob *job_params, const int idcode, const char *group_name)
{
  FileListInternEntry *entry = MEM_new<FileListInternEntry>(__func__);
  entry->relpath = current_relpath_append(job_params, group_name);
  entry->typeflag |= FILE_TYPE_BLENDERLIB | FILE_TYPE_DIR;
  entry->blentype = idcode;
  return entry;
}

void filelist_readjob_list_lib_add_datablock(
    FileListReadJob *job_params,
    ListBaseT<FileListInternEntry> *entries,
    BLODataBlockInfo *datablock_info,
    const bool prefix_relpath_with_group_name,
    const int idcode,
    const char *group_name,
    const std::optional<asset_system::OnlineAssetInfo> online_asset_info)
{
  FileListInternEntry *entry = MEM_new<FileListInternEntry>(__func__);
  if (prefix_relpath_with_group_name) {
    std::string datablock_path = StringRef(group_name) + SEP_STR + datablock_info->name;
    entry->relpath = current_relpath_append(job_params, datablock_path.c_str());
  }
  else {
    entry->relpath = current_relpath_append(job_params, datablock_info->name);
  }
  entry->typeflag |= FILE_TYPE_BLENDERLIB;
  if (datablock_info) {
    entry->blenderlib_has_no_preview = datablock_info->no_preview_found;

    if (datablock_info->name[0] == '.') {
      entry->attributes |= FILE_ATTR_HIDDEN;
    }

    if (datablock_info->asset_data) {

      entry->typeflag |= FILE_TYPE_ASSET;
      if (online_asset_info) {
        entry->typeflag |= FILE_TYPE_ASSET_ONLINE;
      }

      if (job_params->load_asset_library) {
        /* We never want to add assets directly to the "All" library, always add to the actually
         * containing one. */
        BLI_assert((job_params->load_asset_library->library_type() != ASSET_LIBRARY_ALL));

        /* Take ownership over the asset data (shallow copies into unique_ptr managed memory) to
         * pass it on to the asset system. */
        std::unique_ptr metadata = std::make_unique<AssetMetaData>(
            std::move(*datablock_info->asset_data));
        MEM_delete(datablock_info->asset_data);
        /* Give back a non-owning pointer, because the data-block info is still needed (e.g. to
         * update the asset index). */
        datablock_info->asset_data = metadata.get();
        datablock_info->free_asset_data = false;

        entry->asset = online_asset_info ?
                           job_params->load_asset_library->add_external_online_asset(
                               entry->relpath,
                               datablock_info->name,
                               idcode,
                               std::move(metadata),
                               *online_asset_info) :
                           job_params->load_asset_library->add_external_on_disk_asset(
                               entry->relpath, datablock_info->name, idcode, std::move(metadata));
        if (job_params->on_asset_added) {
          (*job_params->on_asset_added)(*entry->get_asset());
        }
      }
    }
  }
  entry->blentype = idcode;
  BLI_addtail(entries, entry);
}

static void filelist_readjob_list_lib_add_datablocks(FileListReadJob *job_params,
                                                     ListBaseT<FileListInternEntry> *entries,
                                                     LinkNode *datablock_infos,
                                                     const bool prefix_relpath_with_group_name,
                                                     const int idcode,
                                                     const char *group_name)
{
  for (LinkNode *ln = datablock_infos; ln; ln = ln->next) {
    BLODataBlockInfo *datablock_info = static_cast<BLODataBlockInfo *>(ln->link);
    filelist_readjob_list_lib_add_datablock(
        job_params, entries, datablock_info, prefix_relpath_with_group_name, idcode, group_name);
  }
}

static FileListInternEntry *filelist_readjob_list_lib_navigate_to_parent_entry_create(
    const FileListReadJob *job_params)
{
  FileListInternEntry *entry = MEM_new<FileListInternEntry>(__func__);
  entry->relpath = current_relpath_append(job_params, FILENAME_PARENT);
  entry->typeflag |= (FILE_TYPE_BLENDERLIB | FILE_TYPE_DIR);
  return entry;
}

static void filelist_readjob_list_lib_add_from_indexer_entries(
    FileListReadJob *job_params,
    ListBaseT<FileListInternEntry> *entries,
    const FileIndexerEntries *indexer_entries,
    const bool prefix_relpath_with_group_name)
{
  for (const LinkNode *ln = indexer_entries->entries; ln; ln = ln->next) {
    FileIndexerEntry *indexer_entry = static_cast<FileIndexerEntry *>(ln->link);
    const char *group_name = BKE_idtype_idcode_to_name(indexer_entry->idcode);
    filelist_readjob_list_lib_add_datablock(job_params,
                                            entries,
                                            &indexer_entry->datablock_info,
                                            prefix_relpath_with_group_name,
                                            indexer_entry->idcode,
                                            group_name);
  }
}

static int filelist_readjob_list_lib_populate_from_index(FileListReadJob *job_params,
                                                         ListBaseT<FileListInternEntry> *entries,
                                                         const ListLibOptions options,
                                                         const int read_from_index,
                                                         const FileIndexerEntries *indexer_entries)
{
  int navigate_to_parent_len = 0;
  if (options & LIST_LIB_ADD_PARENT) {
    FileListInternEntry *entry = filelist_readjob_list_lib_navigate_to_parent_entry_create(
        job_params);
    BLI_addtail(entries, entry);
    navigate_to_parent_len = 1;
  }

  filelist_readjob_list_lib_add_from_indexer_entries(job_params, entries, indexer_entries, true);
  return read_from_index + navigate_to_parent_len;
}

/**
 * \return The number of entries found if the \a root path points to a valid library file.
 *         Otherwise returns no value (#std::nullopt).
 */
static std::optional<int> filelist_readjob_list_lib(FileListReadJob *job_params,
                                                    const char *root,
                                                    ListBaseT<FileListInternEntry> *entries,
                                                    const ListLibOptions options,
                                                    FileIndexer *indexer_runtime)
{
  BLI_assert(indexer_runtime);

  char dir[FILE_MAX_LIBEXTRA], *group;

  BlendHandle *libfiledata = nullptr;

  /* Check if the given root is actually a library. All folders are passed to
   * `filelist_readjob_list_lib` and based on the number of found entries `filelist_readjob_do`
   * will do a dir listing only when this function does not return any entries. */
  /* TODO(jbakker): We should consider introducing its own function to detect if it is a lib and
   * call it directly from `filelist_readjob_do` to increase readability. */
  const bool is_lib = BKE_blendfile_library_path_explode(root, dir, &group, nullptr);
  if (!is_lib) {
    return std::nullopt;
  }

  /* The root path contains an ID group (e.g. "Materials" or "Objects"). */
  const bool has_group = group != nullptr;

  /* Try read from indexer_runtime. */
  /* Indexing returns all entries in a blend file. We should ignore the index when listing a group
   * inside a blend file, so the `entries` isn't filled with undesired entries.
   * This happens when linking or appending data-blocks, where you can navigate into a group (ie
   * Materials/Objects) where you only want to work with partial indexes.
   *
   * Adding support for partial reading/updating indexes would increase the complexity.
   */
  const bool use_indexer = !has_group;
  FileIndexerEntries indexer_entries = {nullptr};
  if (use_indexer) {
    int read_from_index = 0;
    eFileIndexerResult indexer_result = indexer_runtime->callbacks->read_index(
        dir, &indexer_entries, &read_from_index, indexer_runtime->user_data);
    if (indexer_result == FILE_INDEXER_ENTRIES_LOADED) {
      int entries_read = filelist_readjob_list_lib_populate_from_index(
          job_params, entries, options, read_from_index, &indexer_entries);
      ED_file_indexer_entries_clear(&indexer_entries);
      return entries_read;
    }
  }

  /* Open the library file. */
  BlendFileReadReport bf_reports{};
  libfiledata = BLO_blendhandle_from_file(dir, &bf_reports);
  if (libfiledata == nullptr) {
    return std::nullopt;
  }

  /* Add current parent when requested. */
  /* Is the navigate to previous level added to the list of entries. When added the return value
   * should be increased to match the actual number of entries added. It is introduced to keep
   * the code clean and readable and not counting in a single variable. */
  int navigate_to_parent_len = 0;
  if (options & LIST_LIB_ADD_PARENT) {
    FileListInternEntry *entry = filelist_readjob_list_lib_navigate_to_parent_entry_create(
        job_params);
    BLI_addtail(entries, entry);
    navigate_to_parent_len = 1;
  }

  int group_len = 0;
  int datablock_len = 0;
  /* Read only the datablocks from this group. */
  if (has_group) {
    const int idcode = groupname_to_code(group);
    LinkNode *datablock_infos = BLO_blendhandle_get_datablock_info(
        libfiledata, idcode, options & LIST_LIB_ASSETS_ONLY, &datablock_len);
    filelist_readjob_list_lib_add_datablocks(
        job_params, entries, datablock_infos, false, idcode, group);
    BLO_datablock_info_linklist_free(datablock_infos);
  }
  /* Read all datablocks from all groups. */
  else {
    LinkNode *groups = BLO_blendhandle_get_linkable_groups(libfiledata);
    group_len = BLI_linklist_count(groups);

    for (LinkNode *ln = groups; ln; ln = ln->next) {
      const char *group_name = static_cast<char *>(ln->link);
      const int idcode = groupname_to_code(group_name);
      FileListInternEntry *group_entry = filelist_readjob_list_lib_group_create(
          job_params, idcode, group_name);
      BLI_addtail(entries, group_entry);

      if (options & LIST_LIB_RECURSIVE) {
        int group_datablock_len;
        LinkNode *group_datablock_infos = BLO_blendhandle_get_datablock_info(
            libfiledata, idcode, options & LIST_LIB_ASSETS_ONLY, &group_datablock_len);
        filelist_readjob_list_lib_add_datablocks(
            job_params, entries, group_datablock_infos, true, idcode, group_name);
        if (use_indexer) {
          ED_file_indexer_entries_extend_from_datablock_infos(
              &indexer_entries, group_datablock_infos, idcode);
        }
        BLO_datablock_info_linklist_free(group_datablock_infos);
        datablock_len += group_datablock_len;
      }
    }

    BLI_linklist_freeN(groups);
  }

  BLO_blendhandle_close(libfiledata);

  /* Update the index. */
  if (use_indexer) {
    indexer_runtime->callbacks->update_index(dir, &indexer_entries, indexer_runtime->user_data);
    ED_file_indexer_entries_clear(&indexer_entries);
  }

  /* Return the number of items added to entries. */
  int added_entries_len = group_len + datablock_len + navigate_to_parent_len;
  return added_entries_len;
}

static bool filelist_readjob_should_recurse_into_entry(const int max_recursion,
                                                       const bool is_lib,
                                                       const int current_recursion_level,
                                                       FileListInternEntry *entry)
{
  if (max_recursion == 0) {
    /* Recursive loading is disabled. */
    return false;
  }
  if (!is_lib && current_recursion_level > max_recursion) {
    /* No more levels of recursion left. */
    return false;
  }
  /* Show entries when recursion is set to `Blend file` even when `current_recursion_level`
   * exceeds `max_recursion`. */
  if (!is_lib && (current_recursion_level >= max_recursion) &&
      ((entry->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) == 0))
  {
    return false;
  }
  if (entry->typeflag & FILE_TYPE_BLENDERLIB) {
    /* Libraries are already loaded recursively when recursive loaded is used. No need to add
     * them another time. This loading is done with the `LIST_LIB_RECURSIVE` option. */
    return false;
  }
  if (!(entry->typeflag & FILE_TYPE_DIR)) {
    /* Cannot recurse into regular file entries. */
    return false;
  }
  if (FILENAME_IS_CURRPAR(entry->relpath)) {
    /* Don't schedule go to parent entry, (`..`) */
    return false;
  }

  return true;
}

void filelist_readjob_recursive_dir_add_items(const bool do_lib,
                                              FileListReadJob *job_params,
                                              const bool *stop,
                                              bool *do_update,
                                              float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */
  ListBaseT<FileListInternEntry> entries = {nullptr};
  BLI_Stack *todo_dirs;
  TodoDir *td_dir;
  char dir[FILE_MAX_LIBEXTRA];
  char filter_glob[FILE_MAXFILE];
  const char *root = filelist->filelist.root;
  const int max_recursion = filelist->max_recursion;
  int dirs_done_count = 0, dirs_todo_count = 1;

  todo_dirs = BLI_stack_new(sizeof(*td_dir), __func__);
  td_dir = static_cast<TodoDir *>(BLI_stack_push_r(todo_dirs));
  td_dir->level = 1;

  STRNCPY(dir, filelist->filelist.root);
  STRNCPY(filter_glob, filelist->filter_data.filter_glob);

  BLI_path_abs(dir, job_params->main_filepath);
  BLI_path_normalize_dir(dir, sizeof(dir));
  td_dir->dir = BLI_strdup(dir);

  /* Init the file indexer. */
  FileIndexer indexer_runtime{};
  indexer_runtime.callbacks = filelist->indexer;
  if (indexer_runtime.callbacks->init_user_data) {
    indexer_runtime.user_data = indexer_runtime.callbacks->init_user_data(dir, sizeof(dir));
  }

  while (!BLI_stack_is_empty(todo_dirs) && !(*stop)) {
    int entries_num = 0;

    char *subdir;
    char rel_subdir[FILE_MAX_LIBEXTRA];
    int recursion_level;
    bool skip_currpar;

    td_dir = static_cast<TodoDir *>(BLI_stack_peek(todo_dirs));
    subdir = td_dir->dir;
    recursion_level = td_dir->level;
    skip_currpar = (recursion_level > 1);

    BLI_stack_discard(todo_dirs);

    /* ARRRG! We have to be very careful *not to use* common `BLI_path_utils.hh` helpers over
     * entry->relpath itself (nor any path containing it), since it may actually be a datablock
     * name inside .blend file, which can have slashes and backslashes! See #46827.
     * Note that in the end, this means we 'cache' valid relative subdir once here,
     * this is actually better. */
    STRNCPY(rel_subdir, subdir);
    BLI_path_abs(rel_subdir, root);
    BLI_path_normalize_dir(rel_subdir, sizeof(rel_subdir));
    BLI_path_rel(rel_subdir, root);

    /* Update the current relative base path within the filelist root. */
    STRNCPY(job_params->cur_relbase, rel_subdir);

    bool is_lib = false;
    if (do_lib) {
      ListLibOptions list_lib_options = LIST_LIB_OPTION_NONE;
      if (!skip_currpar) {
        list_lib_options |= LIST_LIB_ADD_PARENT;
      }

      /* Libraries are loaded recursively when max_recursion is set. It doesn't check if there is
       * still a recursion level over. */
      if (max_recursion > 0) {
        list_lib_options |= LIST_LIB_RECURSIVE;
      }
      /* Only load assets when browsing an asset library. For normal file browsing we return all
       * entries. `FLF_ASSETS_ONLY` filter can be enabled/disabled by the user. */
      if (job_params->load_asset_library) {
        list_lib_options |= LIST_LIB_ASSETS_ONLY;
      }
      std::optional<int> lib_entries_num = filelist_readjob_list_lib(
          job_params, subdir, &entries, list_lib_options, &indexer_runtime);
      if (lib_entries_num) {
        is_lib = true;
        entries_num += *lib_entries_num;
      }
    }

    if (!is_lib && BLI_is_dir(subdir)) {
      entries_num = filelist_readjob_list_dir(job_params,
                                              subdir,
                                              &entries,
                                              filter_glob,
                                              do_lib,
                                              job_params->main_filepath,
                                              skip_currpar);
    }

    for (FileListInternEntry &entry : entries) {
      entry.uid = filelist_uid_generate(filelist);
      if (!entry.name) {
        entry.name = fileentry_uiname(root, &entry, dir);
      }
      entry.free_name = true;

      if (filelist_readjob_should_recurse_into_entry(
              max_recursion, is_lib, recursion_level, &entry))
      {
        /* We have a directory we want to list, add it to todo list!
         * Using #BLI_path_join works but isn't needed as `root` has a trailing slash. */
        BLI_string_join(dir, sizeof(dir), root, entry.relpath);
        BLI_path_abs(dir, job_params->main_filepath);
        BLI_path_normalize_dir(dir, sizeof(dir));
        td_dir = static_cast<TodoDir *>(BLI_stack_push_r(todo_dirs));
        td_dir->level = recursion_level + 1;
        td_dir->dir = BLI_strdup(dir);
        dirs_todo_count++;
      }
    }

    if (filelist_readjob_append_entries(job_params, &entries, entries_num)) {
      *do_update = true;
    }

    dirs_done_count++;
    *progress = float(dirs_done_count) / float(dirs_todo_count);
    MEM_delete(subdir);
  }

  /* Finalize and free indexer. */
  if (indexer_runtime.callbacks->filelist_finished && BLI_stack_is_empty(todo_dirs)) {
    indexer_runtime.callbacks->filelist_finished(indexer_runtime.user_data);
  }
  if (indexer_runtime.callbacks->free_user_data && indexer_runtime.user_data) {
    indexer_runtime.callbacks->free_user_data(indexer_runtime.user_data);
    indexer_runtime.user_data = nullptr;
  }

  /* If we were interrupted by stop, stack may not be empty and we need to free
   * pending dir paths. */
  while (!BLI_stack_is_empty(todo_dirs)) {
    td_dir = static_cast<TodoDir *>(BLI_stack_peek(todo_dirs));
    MEM_delete(td_dir->dir);
    BLI_stack_discard(todo_dirs);
  }
  BLI_stack_free(todo_dirs);
}

void filelist_readjob_directories_and_libraries(const bool do_lib,
                                                FileListReadJob *job_params,
                                                const bool *stop,
                                                bool *do_update,
                                                float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */

  //  BLI_assert(filelist->filtered == nullptr);
  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET));

  /* A valid, but empty directory from now. */
  filelist->filelist.entries_num = 0;

  filelist_readjob_recursive_dir_add_items(do_lib, job_params, stop, do_update, progress);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset library reading
 * \{ */

/**
 * Load asset library data, which currently means loading the asset catalogs for the library.
 */
void filelist_readjob_load_asset_library_data(FileListReadJob *job_params, bool *do_update)
{
  BLI_assert(job_params->filelist->asset_library_ref);

  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */
  *do_update = false;

  /* See if loading is necessary (and then load). */
  const bool is_force_reload = job_params->reload_asset_library;
  if (!filelist->asset_library || is_force_reload) {
    filelist->asset_library = AS_asset_library_load(job_params->current_main,
                                                    *job_params->filelist->asset_library_ref);
    job_params->reload_asset_library = false;
    *do_update = true;
  }

  /* Not really necessary for this function to do, but otherwise it's up to the caller, and can be
   * forgotten. */
  job_params->load_asset_library = filelist->asset_library;
}

/** \} */

}  // namespace blender
