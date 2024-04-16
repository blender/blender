/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#pragma once

struct AssetLibraryReference;
struct bContext;
struct BlendHandle;
struct FileIndexerType;
struct FileList;
struct FileSelection;
struct ID;
struct ImBuf;
struct bUUID;
struct wmWindowManager;
namespace blender::asset_system {
class AssetLibrary;
class AssetRepresentation;
}  // namespace blender::asset_system

struct FileDirEntry;

typedef uint32_t FileUID;

enum FileSelType {
  FILE_SEL_REMOVE = 0,
  FILE_SEL_ADD = 1,
  FILE_SEL_TOGGLE = 2,
};

enum FileCheckType {
  CHECK_DIRS = 1,
  CHECK_FILES = 2,
  CHECK_ALL = 3,
};

void filelist_setsorting(FileList *filelist, short sort, bool invert_sort);
void filelist_sort(FileList *filelist);

void filelist_setfilter_options(FileList *filelist,
                                bool do_filter,
                                bool hide_dot,
                                bool hide_parent,
                                uint64_t filter,
                                uint64_t filter_id,
                                bool filter_assets_only,
                                const char *filter_glob,
                                const char *filter_search);
/**
 * Set the indexer to be used by the filelist.
 *
 * The given indexer allocation should be handled by the caller or defined statically.
 */
void filelist_setindexer(FileList *filelist, const FileIndexerType *indexer);
/**
 * \param catalog_id: The catalog that should be filtered by if \a catalog_visibility is
 * #FILE_SHOW_ASSETS_FROM_CATALOG. May be NULL otherwise.
 */
void filelist_set_asset_catalog_filter_options(
    FileList *filelist,
    eFileSel_Params_AssetCatalogVisibility catalog_visibility,
    const bUUID *catalog_id);
void filelist_tag_needs_filtering(FileList *filelist);
void filelist_filter(FileList *filelist);
/**
 * \param asset_library_ref: May be NULL to unset the library.
 */
void filelist_setlibrary(FileList *filelist, const AssetLibraryReference *asset_library_ref);

void filelist_init_icons();
void filelist_free_icons();
void filelist_file_get_full_path(const FileList *filelist,
                                 const FileDirEntry *file,
                                 char r_filepath[/*FILE_MAX_LIBEXTRA*/]);
bool filelist_file_is_preview_pending(const FileList *filelist, const FileDirEntry *file);
ImBuf *filelist_getimage(FileList *filelist, int index);
ImBuf *filelist_file_getimage(const FileDirEntry *file);
ImBuf *filelist_geticon_image_ex(const FileDirEntry *file);
ImBuf *filelist_geticon_image(FileList *filelist, int index);
int filelist_geticon(FileList *filelist, int index, bool is_main);

FileList *filelist_new(short type);
void filelist_settype(FileList *filelist, short type);
void filelist_clear(FileList *filelist);
void filelist_clear_ex(FileList *filelist,
                       bool do_asset_library,
                       bool do_cache,
                       bool do_selection);
/**
 * A "smarter" version of #filelist_clear() that calls partial clearing based on the filelist
 * force-reset flags.
 */
void filelist_clear_from_reset_tag(FileList *filelist);
void filelist_free(FileList *filelist);

/**
 * Get the root path of the file list. To get the full path for a file, use
 * #filelist_file_get_full_path().
 */
const char *filelist_dir(const FileList *filelist);
bool filelist_is_dir(const FileList *filelist, const char *path);
/**
 * May modify in place given `dirpath`, which is expected to be #FILE_MAX_LIBEXTRA length.
 */
void filelist_setdir(FileList *filelist, char dirpath[1090 /*FILE_MAX_LIBEXTRA*/]);

/**
 * Limited version of full update done by space_file's file_refresh(),
 * to be used by operators and such.
 * Ensures given filelist is ready to be used (i.e. it is filtered and sorted),
 * unless it is tagged for a full refresh.
 */
int filelist_files_ensure(FileList *filelist);
int filelist_needs_reading(const FileList *filelist);
/**
 * Request a file from the file browser cache, adding it to the cache if necessary.
 *
 * As a rule of thumb, this can be used for operations on individual files (e.g. selection, active,
 * renaming, etc.). But avoid calling this on many files (like when iterating the entire list), to
 * not create a bunch of cache entries for a single operation. While a bit against the point of
 * "intern" entries, in this case it's probably better to have queries like
 * #filelist_entry_get_id(), that take a file index and return data directly from the
 * #FileListInternEntry.
 */
FileDirEntry *filelist_file(FileList *filelist, int index);
FileDirEntry *filelist_file_ex(FileList *filelist, int index, bool use_request);

/**
 * Find a file from a file name, or more precisely, its file-list relative path, inside the
 * filtered items. \return The index of the found file or -1.
 */
int filelist_file_find_path(FileList *filelist, const char *file);
/**
 * Find a file representing \a id.
 * \return The index of the found file or -1.
 */
int filelist_file_find_id(const FileList *filelist, const ID *id);
/**
 * Get the ID a file represents (if any). For #FILE_MAIN, #FILE_MAIN_ASSET.
 */
ID *filelist_file_get_id(const FileDirEntry *file);
/**
 * Same as #filelist_file_get_id(), but gets the file by index (doesn't require the file to be
 * cached, uses #FileListInternEntry only). */
ID *filelist_entry_get_id(const FileList *filelist, int index);
blender::asset_system::AssetRepresentation *filelist_entry_get_asset_representation(
    const FileList *filelist, const int index);
/**
 * Get the #FileDirEntry.relpath value without requiring the #FileDirEntry to be available (doesn't
 * require the file to be cached, uses #FileListInternEntry only).
 */
const char *filelist_entry_get_relpath(const FileList *filelist, int index);
bool filelist_uid_is_set(const FileUID uid);
void filelist_uid_unset(FileUID *r_uid);
void filelist_file_cache_slidingwindow_set(FileList *filelist, size_t window_size);
/**
 * Load in cache all entries "around" given index (as much as block cache may hold).
 */
bool filelist_file_cache_block(FileList *filelist, int index);

bool filelist_needs_force_reset(const FileList *filelist);
void filelist_tag_force_reset(FileList *filelist);
void filelist_tag_force_reset_mainfiles(FileList *filelist);
bool filelist_pending(const FileList *filelist);
bool filelist_needs_reset_on_main_changes(const FileList *filelist);
bool filelist_is_ready(const FileList *filelist);

unsigned int filelist_entry_select_set(const FileList *filelist,
                                       const FileDirEntry *entry,
                                       FileSelType select,
                                       const eDirEntry_SelectFlag flag,
                                       FileCheckType check);
void filelist_entry_select_index_set(FileList *filelist,
                                     int index,
                                     FileSelType select,
                                     eDirEntry_SelectFlag flag,
                                     FileCheckType check);
void filelist_entries_select_index_range_set(FileList *filelist,
                                             FileSelection *sel,
                                             FileSelType select,
                                             eDirEntry_SelectFlag flag,
                                             FileCheckType check);
eDirEntry_SelectFlag filelist_entry_select_get(FileList *filelist,
                                               FileDirEntry *entry,
                                               FileCheckType check);
eDirEntry_SelectFlag filelist_entry_select_index_get(FileList *filelist,
                                                     int index,
                                                     FileCheckType check);
bool filelist_entry_is_selected(FileList *filelist, int index);
/**
 * Set selection of the '..' parent entry, but only if it's actually visible.
 */
void filelist_entry_parent_select_set(FileList *filelist,
                                      FileSelType select,
                                      eDirEntry_SelectFlag flag,
                                      FileCheckType check);

void filelist_setrecursion(FileList *filelist, int recursion_level);

blender::asset_system::AssetLibrary *filelist_asset_library(FileList *filelist);

BlendHandle *filelist_lib(FileList *filelist);
/**
 * \param dir: Must be #FILE_MAX_LIBEXTRA long!
 */
bool filelist_islibrary(FileList *filelist, char *dir, char **r_group);
void filelist_freelib(FileList *filelist);

/**
 * Return the total raw number of entries listed in the given `filelist`, whether they are
 * filtered out or not.
 */
int filelist_files_num_entries(FileList *filelist);

void filelist_readjob_start(FileList *filelist, int space_notifier, const bContext *C);
void filelist_readjob_stop(FileList *filelist, wmWindowManager *wm);
int filelist_readjob_running(FileList *filelist, wmWindowManager *wm);

bool filelist_cache_previews_update(FileList *filelist);
void filelist_cache_previews_set(FileList *filelist, bool use_previews);
bool filelist_cache_previews_running(FileList *filelist);
bool filelist_cache_previews_done(FileList *filelist);
