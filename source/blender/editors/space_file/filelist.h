/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef FILELIST_H
#define FILELIST_H

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

struct FileList *	filelist_new(short type);
void				filelist_init_icons();
void				filelist_free_icons();
int					filelist_find(struct FileList* filelist, char *file);
void				filelist_free(struct FileList* filelist);
void				filelist_sort(struct FileList* filelist, short sort);
int					filelist_numfiles(struct FileList* filelist);
const char *		filelist_dir(struct FileList* filelist);
void				filelist_setdir(struct FileList* filelist, const char *dir);
struct direntry *	filelist_file(struct FileList* filelist, int index);
void				filelist_hidedot(struct FileList* filelist, short hide);
void				filelist_setfilter(struct FileList* filelist, unsigned int filter);
void				filelist_filter(struct FileList* filelist);
void				filelist_swapselect(struct FileList* filelist);
void				filelist_imgsize(struct FileList* filelist, short w, short h);
void				filelist_loadimage(struct FileList* filelist, int index);
void				filelist_loadimage_timer(struct FileList* filelist);
struct ImBuf *		filelist_getimage(struct FileList* filelist, int index);
struct ImBuf *		filelist_geticon(struct FileList* filelist, int index);
short				filelist_changed(struct FileList* filelist);
void				filelist_readdir(struct FileList* filelist);

int					filelist_empty(struct FileList* filelist);
void				filelist_parent(struct FileList* filelist);
void				filelist_setfiletypes(struct FileList* filelist, short has_quicktime);


int					filelist_islibrary (struct FileList* filelist, char* dir, char* group);
void				filelist_from_main(struct FileList* filelist);
void				filelist_from_library(struct FileList* filelist);
void				filelist_freelib(struct FileList* filelist);
void				filelist_hideparent(struct FileList* filelist, short hide);

struct ListBase *	folderlist_new();
void				folderlist_free(struct ListBase* folderlist);
void				folderlist_popdir(struct ListBase* folderlist, char *dir);
void				folderlist_pushdir(struct ListBase* folderlist, const char *dir);
int					folderlist_clear_next(struct SpaceFile* sfile);

#ifdef __cplusplus
}
#endif

#endif

