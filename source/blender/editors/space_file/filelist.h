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
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spfile
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct BlendHandle;
struct FileList;
struct FileSelectAssetLibraryUID;
struct FileSelection;
struct wmWindowManager;

struct FileDirEntry;

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

void folderlist_free(struct ListBase *folderlist);
void folderlist_popdir(struct ListBase *folderlist, char *dir);
void folderlist_pushdir(struct ListBase *folderlist, const char *dir);
const char *folderlist_peeklastdir(struct ListBase *folderlist);
int folderlist_clear_next(struct SpaceFile *sfile);

void folder_history_list_ensure_for_active_browse_mode(struct SpaceFile *sfile);
void folder_history_list_free(struct SpaceFile *sfile);
struct ListBase folder_history_list_duplicate(struct ListBase *listbase);

void filelist_setsorting(struct FileList *filelist, const short sort, bool invert_sort);
void filelist_sort(struct FileList *filelist);

void filelist_setfilter_options(struct FileList *filelist,
                                const bool do_filter,
                                const bool hide_dot,
                                const bool hide_parent,
                                const uint64_t filter,
                                const uint64_t filter_id,
                                const bool filter_assets_only,
                                const char *filter_glob,
                                const char *filter_search);
void filelist_filter(struct FileList *filelist);
void filelist_setlibrary(struct FileList *filelist,
                         const struct FileSelectAssetLibraryUID *asset_library);

void filelist_init_icons(void);
void filelist_free_icons(void);
struct ImBuf *filelist_getimage(struct FileList *filelist, const int index);
struct ImBuf *filelist_file_getimage(const FileDirEntry *file);
struct ImBuf *filelist_geticon_image(struct FileList *filelist, const int index);
int filelist_geticon(struct FileList *filelist, const int index, const bool is_main);

struct FileList *filelist_new(short type);
void filelist_settype(struct FileList *filelist, short type);
void filelist_clear(struct FileList *filelist);
void filelist_clear_ex(struct FileList *filelist, const bool do_cache, const bool do_selection);
void filelist_free(struct FileList *filelist);

const char *filelist_dir(struct FileList *filelist);
bool filelist_is_dir(struct FileList *filelist, const char *path);
void filelist_setdir(struct FileList *filelist, char *r_dir);

int filelist_files_ensure(struct FileList *filelist);
int filelist_needs_reading(struct FileList *filelist);
FileDirEntry *filelist_file(struct FileList *filelist, int index);
FileDirEntry *filelist_file_ex(struct FileList *filelist, int index, bool use_request);

int filelist_file_findpath(struct FileList *filelist, const char *file);
struct ID *filelist_file_get_id(const struct FileDirEntry *file);
FileDirEntry *filelist_entry_find_uuid(struct FileList *filelist, const int uuid[4]);
void filelist_file_cache_slidingwindow_set(struct FileList *filelist, size_t window_size);
bool filelist_file_cache_block(struct FileList *filelist, const int index);

bool filelist_needs_force_reset(struct FileList *filelist);
void filelist_tag_force_reset(struct FileList *filelist);
bool filelist_pending(struct FileList *filelist);
bool filelist_needs_reset_on_main_changes(const struct FileList *filelist);
bool filelist_is_ready(struct FileList *filelist);

unsigned int filelist_entry_select_set(const struct FileList *filelist,
                                       const struct FileDirEntry *entry,
                                       FileSelType select,
                                       unsigned int flag,
                                       FileCheckType check);
void filelist_entry_select_index_set(struct FileList *filelist,
                                     const int index,
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
                                             const int index,
                                             FileCheckType check);
bool filelist_entry_is_selected(struct FileList *filelist, const int index);
void filelist_entry_parent_select_set(struct FileList *filelist,
                                      FileSelType select,
                                      unsigned int flag,
                                      FileCheckType check);

void filelist_setrecursion(struct FileList *filelist, const int recursion_level);

struct BlendHandle *filelist_lib(struct FileList *filelist);
bool filelist_islibrary(struct FileList *filelist, char *dir, char **r_group);
void filelist_freelib(struct FileList *filelist);

void filelist_readjob_start(struct FileList *filelist, const struct bContext *C);
void filelist_readjob_stop(struct wmWindowManager *wm, struct Scene *owner_scene);
int filelist_readjob_running(struct wmWindowManager *wm, struct Scene *owner_scene);

bool filelist_cache_previews_update(struct FileList *filelist);
void filelist_cache_previews_set(struct FileList *filelist, const bool use_previews);
bool filelist_cache_previews_running(struct FileList *filelist);

#ifdef __cplusplus
}
#endif
