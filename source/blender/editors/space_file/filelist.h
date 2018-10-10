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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_file/filelist.h
 *  \ingroup spfile
 */


#ifndef __FILELIST_H__
#define __FILELIST_H__

#ifdef __cplusplus
extern "C" {
#endif

struct BlendHandle;
struct FileList;
struct FileSelection;
struct wmWindowManager;

struct FileDirEntry;

typedef enum FileSelType {
	FILE_SEL_REMOVE = 0,
	FILE_SEL_ADD    = 1,
	FILE_SEL_TOGGLE = 2
} FileSelType;

typedef enum FileCheckType {
	CHECK_DIRS = 1,
	CHECK_FILES = 2,
	CHECK_ALL = 3
} FileCheckType;

struct ListBase    *folderlist_new(void);
void                folderlist_free(struct ListBase *folderlist);
struct ListBase    *folderlist_duplicate(ListBase *folderlist);
void                folderlist_popdir(struct ListBase *folderlist, char *dir);
void                folderlist_pushdir(struct ListBase *folderlist, const char *dir);
const char         *folderlist_peeklastdir(struct ListBase *folderdist);
int                 folderlist_clear_next(struct SpaceFile *sfile);


void                filelist_setsorting(struct FileList *filelist, const short sort);
void                filelist_sort(struct FileList *filelist);

void                filelist_setfilter_options(struct FileList *filelist, const bool do_filter,
                                               const bool hide_dot, const bool hide_parent,
                                               const unsigned int filter, const unsigned int filter_id,
                                               const char *filter_glob, const char *filter_search);
void                filelist_filter(struct FileList *filelist);

void                filelist_init_icons(void);
void                filelist_free_icons(void);
void                filelist_imgsize(struct FileList *filelist, short w, short h);
struct ImBuf       *filelist_getimage(struct FileList *filelist, const int index);
struct ImBuf       *filelist_geticon_image(struct FileList *filelist, const int index);
int                 filelist_geticon(struct FileList *filelist, const int index, const bool is_main);

struct FileList    *filelist_new(short type);
void                filelist_clear(struct FileList *filelist);
void                filelist_clear_ex(struct FileList *filelist, const bool do_cache, const bool do_selection);
void                filelist_free(struct FileList *filelist);

const char         *filelist_dir(struct FileList *filelist);
bool                filelist_is_dir(struct FileList *filelist, const char *path);
void                filelist_setdir(struct FileList *filelist, char *r_dir);

int                 filelist_files_ensure(struct FileList *filelist);
int                 filelist_empty(struct FileList *filelist);
FileDirEntry       *filelist_file(struct FileList *filelist, int index);
int                 filelist_file_findpath(struct FileList *filelist, const char *file);
FileDirEntry       *filelist_entry_find_uuid(struct FileList *filelist, const int uuid[4]);
void                filelist_file_cache_slidingwindow_set(struct FileList *filelist, size_t window_size);
bool                filelist_file_cache_block(struct FileList *filelist, const int index);

bool                filelist_force_reset(struct FileList *filelist);
bool                filelist_pending(struct FileList *filelist);
bool                filelist_is_ready(struct FileList *filelist);

unsigned int        filelist_entry_select_set(const struct FileList *filelist, const struct FileDirEntry *entry, FileSelType select, unsigned int flag, FileCheckType check);
void                filelist_entry_select_index_set(struct FileList *filelist, const int index, FileSelType select, unsigned int flag, FileCheckType check);
void                filelist_entries_select_index_range_set(struct FileList *filelist, FileSelection *sel, FileSelType select, unsigned int flag, FileCheckType check);
unsigned int        filelist_entry_select_get(struct FileList *filelist, struct FileDirEntry *entry, FileCheckType check);
unsigned int        filelist_entry_select_index_get(struct FileList *filelist, const int index, FileCheckType check);

void                filelist_setrecursion(struct FileList *filelist, const int recursion_level);

struct BlendHandle *filelist_lib(struct FileList *filelist);
bool                filelist_islibrary(struct FileList *filelist, char *dir, char **group);
void                filelist_freelib(struct FileList *filelist);

void                filelist_readjob_start(struct FileList *filelist, const struct bContext *C);
void                filelist_readjob_stop(struct wmWindowManager *wm, struct ScrArea *sa);
int                 filelist_readjob_running(struct wmWindowManager *wm, struct ScrArea *sa);

bool                filelist_cache_previews_update(struct FileList *filelist);
void                filelist_cache_previews_set(struct FileList *filelist, const bool use_previews);
bool                filelist_cache_previews_running(struct FileList *filelist);

#ifdef __cplusplus
}
#endif

#endif
