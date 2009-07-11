/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef WIN32
#include <io.h>
#include <direct.h>
#include "BLI_winstuff.h"
#else
#include <unistd.h>
#include <sys/times.h>
#endif   

#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_storage_types.h"
#include "BLI_dynstr.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"

#include "BLF_api.h"

#include "DNA_userdef_types.h"

#include "ED_screen.h"
#include "ED_util.h"
#include "ED_fileselect.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "file_intern.h"
#include "filelist.h"

#if defined __BeOS
static int fnmatch(const char *pattern, const char *string, int flags)
{
	return 0;
}
#elif defined WIN32 && !defined _LIBC
	/* use fnmatch included in blenlib */
	#include "BLI_fnmatch.h"
#else
	#include <fnmatch.h>
#endif

FileSelectParams* ED_fileselect_get_params(struct SpaceFile *sfile)
{
	if (!sfile->params) {
		ED_fileselect_set_params(sfile, "", NULL, "/", 0, FILE_SHORTDISPLAY, 0, FILE_SORT_ALPHA);
	}
	return sfile->params;
}

short ED_fileselect_set_params(SpaceFile *sfile, const char *title, const char *last_dir, const char *path,
							   short flag, short display, short filter, short sort)
{
	char name[FILE_MAX], dir[FILE_MAX], file[FILE_MAX];
	FileSelectParams *params;

	if (!sfile->params) {
		sfile->params= MEM_callocN(sizeof(FileSelectParams), "fileselparams");
	}

	params = sfile->params;

	params->flag = flag;
	params->display = display;
	params->filter = filter;
	params->sort = sort;

	BLI_strncpy(params->title, title, sizeof(params->title));

	if(last_dir){
		BLI_strncpy(params->dir, last_dir, sizeof(params->dir));
	}
	else {
		BLI_strncpy(name, path, sizeof(name));
		BLI_convertstringcode(name, G.sce);

		BLI_split_dirfile(name, dir, file);
		BLI_strncpy(params->file, file, sizeof(params->file));
		BLI_strncpy(params->dir, dir, sizeof(params->dir));
		BLI_make_file_string(G.sce, params->dir, dir, ""); /* XXX needed ? - also solve G.sce */			
	}

	return 1;
}

void ED_fileselect_reset_params(SpaceFile *sfile)
{
	sfile->params->flag = 0;
	sfile->params->title[0] = '\0';
}


int ED_fileselect_layout_offset(FileLayout* layout, int x, int y)
{
	int offsetx, offsety;
	int active_file;

	if (layout == NULL)
		return NULL;
	
	offsetx = (x)/(layout->tile_w + 2*layout->tile_border_x);
	offsety = (y)/(layout->tile_h + 2*layout->tile_border_y);
	
	if (offsetx > layout->columns-1) offsetx = -1 ;
	if (offsety > layout->rows-1) offsety = -1 ;

	if (layout->flag & FILE_LAYOUT_HOR) 
		active_file = layout->rows*offsetx + offsety;
	else
		active_file = offsetx + layout->columns*offsety;
	return active_file;
}

void ED_fileselect_layout_tilepos(FileLayout* layout, int tile, short *x, short *y)
{
	if (layout->flag == FILE_LAYOUT_HOR) {
		*x = layout->tile_border_x + (tile/layout->rows)*(layout->tile_w+2*layout->tile_border_x);
		*y = layout->tile_border_y + (tile%layout->rows)*(layout->tile_h+2*layout->tile_border_y);
	} else {
		*x = layout->tile_border_x + ((tile)%layout->columns)*(layout->tile_w+2*layout->tile_border_x);
		*y = layout->tile_border_y + ((tile)/layout->columns)*(layout->tile_h+2*layout->tile_border_y);
	}
}

float file_string_width(const char* str)
{
	uiStyle *style= U.uistyles.first;
	uiStyleFontSet(&style->widget);
	return BLF_width((char *)str);
}

float file_font_pointsize()
{
	float s;
	char tmp[2] = "X";
	uiStyle *style= U.uistyles.first;
	uiStyleFontSet(&style->widget);
	s = BLF_height(tmp);
	return style->widget.points;
}

static void column_widths(struct FileList* files, struct FileLayout* layout)
{
	int i;
	int numfiles = filelist_numfiles(files);

	for (i=0; i<MAX_FILE_COLUMN; ++i) {
		layout->column_widths[i] = 0;
	}

	for (i=0; (i < numfiles); ++i)
	{
		struct direntry* file = filelist_file(files, i);	
		if (file) {
			int len;
			len = file_string_width(file->relname);
			if (len > layout->column_widths[COLUMN_NAME]) layout->column_widths[COLUMN_NAME] = len;
			len = file_string_width(file->date);
			if (len > layout->column_widths[COLUMN_DATE]) layout->column_widths[COLUMN_DATE] = len;
			len = file_string_width(file->time);
			if (len > layout->column_widths[COLUMN_TIME]) layout->column_widths[COLUMN_TIME] = len;
			len = file_string_width(file->size);
			if (len > layout->column_widths[COLUMN_SIZE]) layout->column_widths[COLUMN_SIZE] = len;
			len = file_string_width(file->mode1);
			if (len > layout->column_widths[COLUMN_MODE1]) layout->column_widths[COLUMN_MODE1] = len;
			len = file_string_width(file->mode2);
			if (len > layout->column_widths[COLUMN_MODE2]) layout->column_widths[COLUMN_MODE2] = len;
			len = file_string_width(file->mode3);
			if (len > layout->column_widths[COLUMN_MODE3]) layout->column_widths[COLUMN_MODE3] = len;
			len = file_string_width(file->owner);
			if (len > layout->column_widths[COLUMN_OWNER]) layout->column_widths[COLUMN_OWNER] = len;
		}
	}
}

void ED_fileselect_init_layout(struct SpaceFile *sfile, struct ARegion *ar)
{
	FileSelectParams* params = ED_fileselect_get_params(sfile);
	View2D *v2d= &ar->v2d;
	int maxlen = 0;
	int numfiles = filelist_numfiles(sfile->files);
	int textheight = file_font_pointsize();
	if (sfile->layout == 0) {
		sfile->layout = MEM_callocN(sizeof(struct FileLayout), "file_layout");
	}
	if (params->display == FILE_IMGDISPLAY) {
		sfile->layout->prv_w = 96;
		sfile->layout->prv_h = 96;
		sfile->layout->tile_border_x = 6;
		sfile->layout->tile_border_y = 6;
		sfile->layout->prv_border_x = 6;
		sfile->layout->prv_border_y = 6;
		sfile->layout->tile_w = sfile->layout->prv_w + 2*sfile->layout->prv_border_x;
		sfile->layout->tile_h = sfile->layout->prv_h + 2*sfile->layout->prv_border_y + textheight;
		sfile->layout->width= (v2d->cur.xmax - v2d->cur.xmin - 2*sfile->layout->tile_border_x);
		sfile->layout->columns= sfile->layout->width / (sfile->layout->tile_w + 2*sfile->layout->tile_border_x);
		if(sfile->layout->columns > 0)
			sfile->layout->rows= numfiles/sfile->layout->columns + 1; // XXX dirty, modulo is zero
		else {
			sfile->layout->columns = 1;
			sfile->layout->rows= numfiles + 1; // XXX dirty, modulo is zero
		}
		sfile->layout->height= sfile->layout->rows*(sfile->layout->tile_h+2*sfile->layout->tile_border_y) + sfile->layout->tile_border_y*2;
		sfile->layout->flag = FILE_LAYOUT_VER;
	} else {
		sfile->layout->prv_w = 0;
		sfile->layout->prv_h = 0;
		sfile->layout->tile_border_x = 8;
		sfile->layout->tile_border_y = 2;
		sfile->layout->prv_border_x = 0;
		sfile->layout->prv_border_y = 0;
		sfile->layout->tile_h = textheight*3/2;
		sfile->layout->height= v2d->cur.ymax - v2d->cur.ymin;
		sfile->layout->rows = sfile->layout->height / (sfile->layout->tile_h + 2*sfile->layout->tile_border_y);;
        
		column_widths(sfile->files, sfile->layout);

		if (params->display == FILE_SHORTDISPLAY) {
			maxlen = sfile->layout->column_widths[COLUMN_NAME] +
					 sfile->layout->column_widths[COLUMN_SIZE];
			maxlen += 20+2*10; // for icon and space between columns
		} else {
			maxlen = sfile->layout->column_widths[COLUMN_NAME] +
					 sfile->layout->column_widths[COLUMN_DATE] +
					 sfile->layout->column_widths[COLUMN_TIME] +
					 sfile->layout->column_widths[COLUMN_SIZE];
					/* XXX add mode1, mode2, mode3, owner columns for non-windows platforms */
			maxlen += 20+4*10; // for icon and space between columns
		}
		sfile->layout->tile_w = maxlen + 40;
		if(sfile->layout->rows > 0)
			sfile->layout->columns = numfiles/sfile->layout->rows + 1; // XXX dirty, modulo is zero
		else {
			sfile->layout->rows = 1;
			sfile->layout->columns = numfiles + 1; // XXX dirty, modulo is zero
		}
		sfile->layout->width = sfile->layout->columns * (sfile->layout->tile_w + 2*sfile->layout->tile_border_x) + sfile->layout->tile_border_x*2;
		sfile->layout->flag = FILE_LAYOUT_HOR;
	} 
}

FileLayout* ED_fileselect_get_layout(struct SpaceFile *sfile, struct ARegion *ar)
{
	if (!sfile->layout) {
		ED_fileselect_init_layout(sfile, ar);
	}
	return sfile->layout;
}

void file_change_dir(struct SpaceFile *sfile)
{
	if (sfile->params && BLI_exists(sfile->params->dir)) {
		filelist_setdir(sfile->files, sfile->params->dir);

		if(folderlist_clear_next(sfile))
			folderlist_free(sfile->folders_next);

		folderlist_pushdir(sfile->folders_prev, sfile->params->dir);

		filelist_free(sfile->files);
		sfile->params->active_file = -1;
	}
}

int file_select_match(struct SpaceFile *sfile, const char *pattern)
{
	int match = 0;
	if (strchr(pattern, '*') || strchr(pattern, '?') || strchr(pattern, '[')) {
		int i;
		struct direntry *file;
		int n = filelist_numfiles(sfile->files);

		for (i = 0; i < n; i++) {
			file = filelist_file(sfile->files, i);
			if (fnmatch(pattern, file->relname, 0) == 0) {
				file->flags |= ACTIVE;
				match = 1;
			}
		}
	}
	return match;
}


void autocomplete_directory(struct bContext *C, char *str, void *arg_v)
{
	char tmp[FILE_MAX];
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);

	/* search if str matches the beginning of name */
	if(str[0] && sfile->files) {
		AutoComplete *autocpl= autocomplete_begin(str, FILE_MAX);
		int nentries = filelist_numfiles(sfile->files);
		int i;

		for(i= 0; i<nentries; ++i) {
			struct direntry* file = filelist_file(sfile->files, i);
			char* dir = filelist_dir(sfile->files);
			if (file && S_ISDIR(file->type))	{
				BLI_make_file_string(G.sce, tmp, dir, file->relname);
				autocomplete_do_name(autocpl,tmp);
			}
		}
		autocomplete_end(autocpl, str);
	}
}