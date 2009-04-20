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

#ifndef BIF_FILELIST_H
#define BIF_FILELIST_H

#ifdef __cplusplus
extern "C" {
#endif

struct FileList;
struct direntry;
struct BlendHandle;

struct FileList *	BIF_filelist_new();
void				BIF_filelist_init_icons();
void				BIF_filelist_free_icons();
struct FileList *	BIF_filelist_copy(struct FileList* filelist);
int					BIF_filelist_find(struct FileList* filelist, char *file);
void				BIF_filelist_free(struct FileList* filelist);
void				BIF_filelist_freelib(struct FileList* filelist);
void				BIF_filelist_sort(struct FileList* filelist, short sort);
int					BIF_filelist_numfiles(struct FileList* filelist);
const char *		BIF_filelist_dir(struct FileList* filelist);
void				BIF_filelist_setdir(struct FileList* filelist, const char *dir);
struct direntry *	BIF_filelist_file(struct FileList* filelist, int index);
void				BIF_filelist_hidedot(struct FileList* filelist, short hide);
void				BIF_filelist_setfilter(struct FileList* filelist, unsigned int filter);
void				BIF_filelist_filter(struct FileList* filelist);
void				BIF_filelist_swapselect(struct FileList* filelist);
void				BIF_filelist_imgsize(struct FileList* filelist, short w, short h);
void				BIF_filelist_loadimage(struct FileList* filelist, int index);
struct ImBuf *		BIF_filelist_getimage(struct FileList* filelist, int index);

void				BIF_filelist_readdir(struct FileList* filelist);

int					BIF_filelist_empty(struct FileList* filelist);
void				BIF_filelist_parent(struct FileList* filelist);
void				BIF_filelist_setfiletypes(struct FileList* filelist, short has_quicktime);
int					BIF_filelist_islibrary (struct FileList* filelist, char* dir, char* group);
void				BIF_filelist_from_main(struct FileList* filelist);
void				BIF_filelist_from_library(struct FileList* filelist);
void				BIF_filelist_append_library(struct FileList* filelist, char *dir, char* file, short flag, int idcode);
void				BIF_filelist_settype(struct FileList* filelist, int type);
short				BIF_filelist_gettype(struct FileList* filelist);
void				BIF_filelist_setipotype(struct FileList* filelist, short ipotype);
void				BIF_filelist_hasfunc(struct FileList* filelist, int has_func);

struct BlendHandle *BIF_filelist_lib(struct FileList* filelist);
int					BIF_groupname_to_code(char *group); /* TODO: where should this go */

#ifdef __cplusplus
}
#endif

#endif

