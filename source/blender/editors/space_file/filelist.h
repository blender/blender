/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup spfile
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AssetLibraryReference;
struct BlendHandle;
struct FileIndexerType;
struct FileList;
struct FileSelection;
struct bUUID;
struct wmWindowManager;

struct FileDirEntry;

typedef uint32_t FileUID;

typedef enum FileSelType {
  FILE_SEL_REMOVE = 0,
  FILE_SEL_ADD = 1,
  FILE_SEL_TOGGLE = 2,
} FileSelType;

typedef enum FileCheckType {
  CHECK_DIRS = 1,
  CHECK_FILES = 2,
  CHECK_ALL = 3,
} FileCheckType;

/* not listbase itself */
void folderlist_free(struct ListBase *folderlist);
void folderlist_popdir(struct ListBase *folderlist, char *dir);
void folderlist_pushdir(struct ListBase *folderlist, const char *dir);
const char *folderlist_peeklastdir(struct ListBase *folderlist);
int folderlist_clear_next(struct SpaceFile *sfile);

void folder_history_list_ensure_for_active_browse_mode(struct SpaceFile *sfile);
void folder_history_list_free(struct SpaceFile *sfile);
struct ListBase folder_history_list_duplicate(struct ListBase *listbase);

void filelist_setsorting(struct FileList *filelist, short sort, bool invert_sort);
void filelist_sort(struct FileList *filelist);

void filelist_setfilter_options(struct FileList *filelist,
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
void filelist_setindexer(struct FileList *filelist, const struct FileIndexerType *indexer);
/**
 * \param catalog_id: The catalog that should be filtered by if \a catalog_visibility is
 * #FILE_SHOW_ASSETS_FROM_CATALOG. May be NULL otherwise.
 */
void filelist_set_asset_catalog_filter_options(
    struct FileList *filelist,
    eFileSel_Params_AssetCatalogVisibility catalog_visibility,
    const struct bUUID *catalog_id);
void filelist_tag_needs_filtering(struct FileList *filelist);
void filelist_filter(struct FileList *filelist);
/**
 * \param asset_library_ref: May be NULL to unset the library.
 */
void filelist_setlibrary(struct FileList *filelist,
                         const struct AssetLibraryReference *asset_library_ref);

void filelist_init_icons(void);
void filelist_free_icons(void);
struct ImBuf *filelist_getimage(struct FileList *filelist, int index);
struct ImBuf *filelist_file_getimage(const FileDirEntry *file);
struct ImBuf *filelist_geticon_image_ex(const FileDirEntry *file);
struct ImBuf *filelist_geticon_image(struct FileList *filelist, int index);
int filelist_geticon(struct FileList *filelist, int index, bool is_main);

struct FileList *filelist_new(short type);
void filelist_settype(struct FileList *filelist, short type);
void filelist_clear(struct FileList *filelist);
void filelist_clear_ex(struct FileList *filelist,
                       bool do_asset_library,
                       bool do_cache,
                       bool do_selection);
/**
 * A "smarter" version of #filelist_clear() that calls partial clearing based on the filelist
 * force-reset flags.
 */
void filelist_clear_from_reset_tag(struct FileList *filelist);
void filelist_free(struct FileList *filelist);

const char *filelist_dir(struct FileList *filelist);
bool filelist_is_dir(struct FileList *filelist, const char *path);
/**
 * May modify in place given r_dir, which is expected to be FILE_MAX_LIBEXTRA length.
 */
void filelist_setdir(struct FileList *filelist, char *r_dir);

/**
 * Limited version of full update done by space_file's file_refresh(),
 * to be used by operators and such.
 * Ensures given filelist is ready to be used (i.e. it is filtered and sorted),
 * unless it is tagged for a full refresh.
 */
int filelist_files_ensure(struct FileList *filelist);
int filelist_needs_reading(struct FileList *filelist);
FileDirEntry *filelist_file(struct FileList *filelist, int index);
FileDirEntry *filelist_file_ex(struct FileList *filelist, int index, bool use_request);

/**
 * Find a file from a file name, or more precisely, its file-list relative path, inside the
 * filtered items. \return The index of the found file or -1.
 */
int filelist_file_find_path(struct FileList *filelist, const char *file);
/**
 * Find a file representing \a id.
 * \return The index of the found file or -1.
 */
int filelist_file_find_id(const struct FileList *filelist, const struct ID *id);
/**
 * Get the ID a file represents (if any). For #FILE_MAIN, #FILE_MAIN_ASSET.
 */
struct ID *filelist_file_get_id(const struct FileDirEntry *file);
bool filelist_uid_is_set(const FileUID uid);
void filelist_uid_unset(FileUID *r_uid);
void filelist_file_cache_slidingwindow_set(struct FileList *filelist, size_t window_size);
/**
 * Load in cache all entries "around" given index (as much as block cache may hold).
 */
bool filelist_file_cache_block(struct FileList *filelist, int index);

bool filelist_needs_force_reset(struct FileList *filelist);
void filelist_tag_force_reset(struct FileList *filelist);
void filelist_tag_force_reset_mainfiles(struct FileList *filelist);
bool filelist_pending(struct FileList *filelist);
bool filelist_needs_reset_on_main_changes(const struct FileList *filelist);
bool filelist_is_ready(struct FileList *filelist);

unsigned int filelist_entry_select_set(const struct FileList *filelist,
                                       const struct FileDirEntry *entry,
                                       FileSelType select,
                                       unsigned int flag,
                                       FileCheckType check);
void filelist_entry_select_index_set(struct FileList *filelist,
                                     int index,
                                     FileSelType select,
                                     unsigned int flag,
                                     FileCheckType check);
void filelist_entries_select_index_range_set(struct FileList *filelist,
                                             FileSelection *sel,
                                             FileSelType select,
                                             unsigned int flag,
                                             FileCheckType check);
unsigned int filelist_entry_select_get(struct FileList *filelist,
                                       struct FileDirEntry *entry,
                                       FileCheckType check);
unsigned int filelist_entry_select_index_get(struct FileList *filelist,
                                             int index,
                                             FileCheckType check);
bool filelist_entry_is_selected(struct FileList *filelist, int index);
/**
 * Set selection of the '..' parent entry, but only if it's actually visible.
 */
void filelist_entry_parent_select_set(struct FileList *filelist,
                                      FileSelType select,
                                      unsigned int flag,
                                      FileCheckType check);

void filelist_setrecursion(struct FileList *filelist, int recursion_level);

struct AssetLibrary *filelist_asset_library(struct FileList *filelist);

struct BlendHandle *filelist_lib(struct FileList *filelist);
/**
 * \param dir: Must be #FILE_MAX_LIBEXTRA long!
 */
bool filelist_islibrary(struct FileList *filelist, char *dir, char **r_group);
void filelist_freelib(struct FileList *filelist);

/** Return the total raw number of entries listed in the given `filelist`, whether they are
 * filtered out or not. */
int filelist_files_num_entries(struct FileList *filelist);

void filelist_readjob_start(struct FileList *filelist,
                            int space_notifier,
                            const struct bContext *C);
void filelist_readjob_stop(struct FileList *filelist, struct wmWindowManager *wm);
int filelist_readjob_running(struct FileList *filelist, struct wmWindowManager *wm);

bool filelist_cache_previews_update(struct FileList *filelist);
void filelist_cache_previews_set(struct FileList *filelist, bool use_previews);
bool filelist_cache_previews_running(struct FileList *filelist);
bool filelist_cache_previews_done(struct FileList *filelist);

#ifdef __cplusplus
}
#endif
