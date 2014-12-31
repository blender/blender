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
struct FolderList;
struct Main;
struct ReportList;
struct Scene;
struct direntry;
struct rcti;
struct wmWindowManager;

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

struct ListBase *   folderlist_new(void);
void                folderlist_free(struct ListBase *folderlist);
struct ListBase *   folderlist_duplicate(ListBase *folderlist);
void                folderlist_popdir(struct ListBase *folderlist, char *dir);
void                folderlist_pushdir(struct ListBase *folderlist, const char *dir);
const char *        folderlist_peeklastdir(struct ListBase *folderdist);
int                 folderlist_clear_next(struct SpaceFile *sfile);


void                filelist_sort(struct FileList *filelist, short sort);

void                filelist_hidedot(struct FileList *filelist, short hide);
void                filelist_setfilter(struct FileList *filelist, unsigned int filter);
void                filelist_setfilter_types(struct FileList *filelist, const char *filter_glob);
void                filelist_filter(struct FileList *filelist);

void                filelist_init_icons(void);
void                filelist_free_icons(void);
void                filelist_imgsize(struct FileList *filelist, short w, short h);
struct ImBuf *      filelist_getimage(struct FileList *filelist, const int index);
struct ImBuf *      filelist_geticon(struct FileList *filelist, const int index);

struct FileList *   filelist_new(short type);
void                filelist_free(struct FileList *filelist);

const char *        filelist_dir(struct FileList *filelist);
void                filelist_readdir(struct FileList *filelist);
void                filelist_setdir(struct FileList *filelist, const char *dir);

int                 filelist_empty(struct FileList *filelist);
int                 filelist_numfiles(struct FileList *filelist);
struct direntry *   filelist_file(struct FileList *filelist, int index);
int                 filelist_find(struct FileList *filelist, const char *file);

short               filelist_changed(struct FileList *filelist);

void                filelist_select(struct FileList *filelist, FileSelection *sel, FileSelType select, unsigned int flag, FileCheckType check);
void                filelist_select_file(struct FileList *filelist, int index, FileSelType select, unsigned int flag, FileCheckType check);
bool                filelist_is_selected(struct FileList *filelist, int index, FileCheckType check);

struct BlendHandle *filelist_lib(struct FileList *filelist);
bool                filelist_islibrary(struct FileList *filelist, char *dir, char *group);
void                filelist_freelib(struct FileList *filelist);

void                thumbnails_start(struct FileList *filelist, const struct bContext *C);
void                thumbnails_stop(struct wmWindowManager *wm, struct FileList *filelist);
int                 thumbnails_running(struct wmWindowManager *wm, struct FileList *filelist);

#ifdef __cplusplus
}
#endif

#endif

