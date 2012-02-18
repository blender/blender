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

struct FileList;
struct FolderList;
struct direntry;
struct BlendHandle;
struct Scene;
struct Main;
struct rcti;
struct ReportList;
struct FileSelection;

typedef enum FileSelType {
	FILE_SEL_REMOVE = 0,
	FILE_SEL_ADD =	1,
	FILE_SEL_TOGGLE	 = 2
} FileSelType;

typedef enum FileCheckType
{
	CHECK_DIRS = 1,
	CHECK_FILES = 2,
	CHECK_ALL = 3
} FileCheckType;

struct FileList *	filelist_new(short type);
void				filelist_init_icons(void);
void				filelist_free_icons(void);
int					filelist_find(struct FileList* filelist, const char *file);
void				filelist_free(struct FileList* filelist);
void				filelist_sort(struct FileList* filelist, short sort);
int					filelist_numfiles(struct FileList* filelist);
const char *		filelist_dir(struct FileList* filelist);
void				filelist_setdir(struct FileList* filelist, const char *dir);
struct direntry *	filelist_file(struct FileList* filelist, int index);
void				filelist_select(struct FileList* filelist, FileSelection* sel, FileSelType select, unsigned int flag, FileCheckType check);
void				filelist_select_file(struct FileList* filelist, int index, FileSelType select, unsigned int flag, FileCheckType check);
int					filelist_is_selected(struct FileList* filelist, int index, FileCheckType check);
void				filelist_hidedot(struct FileList* filelist, short hide);
void				filelist_setfilter(struct FileList* filelist, unsigned int filter);
void				filelist_setfilter_types(struct FileList* filelist, const char *filter_glob);
void				filelist_filter(struct FileList* filelist);
void				filelist_imgsize(struct FileList* filelist, short w, short h);
struct ImBuf *		filelist_getimage(struct FileList* filelist, int index);
struct ImBuf *		filelist_geticon(struct FileList* filelist, int index);
short				filelist_changed(struct FileList* filelist);
void				filelist_readdir(struct FileList* filelist);

int					filelist_empty(struct FileList* filelist);
void				filelist_parent(struct FileList* filelist);

struct BlendHandle *filelist_lib(struct FileList* filelist);
int					filelist_islibrary (struct FileList* filelist, char* dir, char* group);
void				filelist_from_main(struct FileList* filelist);
void				filelist_from_library(struct FileList* filelist);
void				filelist_freelib(struct FileList* filelist);
void				filelist_hideparent(struct FileList* filelist, short hide);

struct ListBase *	folderlist_new(void);
void				folderlist_free(struct ListBase* folderlist);
struct ListBase	*	folderlist_duplicate(ListBase* folderlist);
void				folderlist_popdir(struct ListBase* folderlist, char *dir);
void				folderlist_pushdir(struct ListBase* folderlist, const char *dir);
int					folderlist_clear_next(struct SpaceFile* sfile);

void				thumbnails_stop(struct FileList* filelist, const struct bContext* C);
void				thumbnails_start(struct FileList* filelist, const struct bContext* C);
int					thumbnails_running(struct FileList* filelist, const struct bContext* C);

#ifdef __cplusplus
}
#endif

#endif

