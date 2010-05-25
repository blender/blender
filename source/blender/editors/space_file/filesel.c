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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/* path/file handeling stuff */
#ifndef WIN32
  #include <dirent.h>
  #include <unistd.h>
#else
  #include <io.h>
  #include "BLI_winstuff.h"
#endif

#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_storage_types.h"
#include "BLI_dynstr.h"

#include "BLO_readfile.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"

#include "BLF_api.h"


#include "ED_fileselect.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "file_intern.h"
#include "filelist.h"

#if defined WIN32 && !defined _LIBC
# include "BLI_fnmatch.h" /* use fnmatch included in blenlib */
#else
# include <fnmatch.h>
#endif

FileSelectParams* ED_fileselect_get_params(struct SpaceFile *sfile)
{
	if (!sfile->params) {
		ED_fileselect_set_params(sfile);
	}
	return sfile->params;
}

short ED_fileselect_set_params(SpaceFile *sfile)
{
	char name[FILE_MAX], dir[FILE_MAX], file[FILE_MAX];
	FileSelectParams *params;
	wmOperator *op = sfile->op;

	/* create new parameters if necessary */
	if (!sfile->params) {
		sfile->params= MEM_callocN(sizeof(FileSelectParams), "fileselparams");
		/* set path to most recently opened .blend */
		BLI_strncpy(sfile->params->dir, G.sce, sizeof(sfile->params->dir));
		BLI_split_dirfile(G.sce, dir, file);
		BLI_strncpy(sfile->params->file, file, sizeof(sfile->params->file));
		BLI_make_file_string(G.sce, sfile->params->dir, dir, ""); /* XXX needed ? - also solve G.sce */
	}

	params = sfile->params;

	/* set the parameters from the operator, if it exists */
	if (op) {
		BLI_strncpy(params->title, op->type->name, sizeof(params->title));

		if(RNA_struct_find_property(op->ptr, "filemode"))
			params->type = RNA_int_get(op->ptr, "filemode");
		else
			params->type = FILE_SPECIAL;

		if (RNA_property_is_set(op->ptr, "path")) {
			RNA_string_get(op->ptr, "path", name);
			if (params->type == FILE_LOADLIB) {
				BLI_strncpy(params->dir, name, sizeof(params->dir));
				BLI_cleanup_dir(G.sce, params->dir);	
			} else { 
				/* if operator has path set, use it, otherwise keep the last */
				BLI_path_abs(name, G.sce);
				BLI_split_dirfile(name, dir, file);
				BLI_strncpy(params->file, file, sizeof(params->file));
				BLI_make_file_string(G.sce, params->dir, dir, ""); /* XXX needed ? - also solve G.sce */
			}
		}
		params->filter = 0;
		if(RNA_struct_find_property(op->ptr, "filter_blender"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_blender") ? BLENDERFILE : 0;
		if(RNA_struct_find_property(op->ptr, "filter_image"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_image") ? IMAGEFILE : 0;
		if(RNA_struct_find_property(op->ptr, "filter_movie"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_movie") ? MOVIEFILE : 0;
		if(RNA_struct_find_property(op->ptr, "filter_text"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_text") ? TEXTFILE : 0;
		if(RNA_struct_find_property(op->ptr, "filter_python"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_python") ? PYSCRIPTFILE : 0;
		if(RNA_struct_find_property(op->ptr, "filter_font"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_font") ? FTFONTFILE : 0;
		if(RNA_struct_find_property(op->ptr, "filter_sound"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_sound") ? SOUNDFILE : 0;
		if(RNA_struct_find_property(op->ptr, "filter_text"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_text") ? TEXTFILE : 0;
		if(RNA_struct_find_property(op->ptr, "filter_folder"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_folder") ? FOLDERFILE : 0;
		if(RNA_struct_find_property(op->ptr, "filter_btx"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_btx") ? BTXFILE : 0;
		if(RNA_struct_find_property(op->ptr, "filter_collada"))
			params->filter |= RNA_boolean_get(op->ptr, "filter_collada") ? COLLADAFILE : 0;
		if (params->filter != 0) {
			if (U.uiflag & USER_FILTERFILEEXTS) {
				params->flag |= FILE_FILTER;
			} else {
				params->flag &= ~FILE_FILTER;
			}
		}

		if (U.uiflag & USER_HIDE_DOT) {
			params->flag |= FILE_HIDE_DOT;
		} else {
			params->flag &= ~FILE_HIDE_DOT;
		}
		

		if (params->type == FILE_LOADLIB) {
			params->flag |= RNA_boolean_get(op->ptr, "link") ? FILE_LINK : 0;
			params->flag |= RNA_boolean_get(op->ptr, "autoselect") ? FILE_AUTOSELECT : 0;
			params->flag |= RNA_boolean_get(op->ptr, "active_layer") ? FILE_ACTIVELAY : 0;
		}

		if(params->filter & (IMAGEFILE|MOVIEFILE))
			params->display= FILE_IMGDISPLAY;
		else
			params->display= FILE_SHORTDISPLAY;
		
	} else {
		/* default values, if no operator */
		params->type = FILE_UNIX;
		params->flag |= FILE_HIDE_DOT;
		params->display = FILE_SHORTDISPLAY;
		params->filter = 0;
		params->sort = FILE_SORT_ALPHA;
	}
	return 1;
}

void ED_fileselect_reset_params(SpaceFile *sfile)
{
	sfile->params->type = FILE_UNIX;
	sfile->params->flag = 0;
	sfile->params->title[0] = '\0';
}

int ED_fileselect_layout_numfiles(FileLayout* layout, struct ARegion *ar)
{
	int numfiles;

	if (layout->flag & FILE_LAYOUT_HOR) {
		int width = ar->v2d.cur.xmax - ar->v2d.cur.xmin - 2*layout->tile_border_x;
		numfiles = (float)width/(float)layout->tile_w+0.5;
		return numfiles*layout->rows;
	} else {
		int height = ar->v2d.cur.ymax - ar->v2d.cur.ymin - 2*layout->tile_border_y;
		numfiles = (float)height/(float)layout->tile_h+0.5;
		return numfiles*layout->columns;
	}
}

int ED_fileselect_layout_offset(FileLayout* layout, int clamp_bounds, int x, int y)
{
	int offsetx, offsety;
	int active_file;

	if (layout == NULL)
		return 0;
	
	offsetx = (x)/(layout->tile_w + 2*layout->tile_border_x);
	offsety = (y)/(layout->tile_h + 2*layout->tile_border_y);
	
	if (clamp_bounds) {
		CLAMP(offsetx, 0, layout->columns-1);
		CLAMP(offsety, 0, layout->rows-1);
	} else {
		if (offsetx > layout->columns-1) return -1 ;
		if (offsety > layout->rows-1) return -1 ;
	}
	
	if (layout->flag & FILE_LAYOUT_HOR) 
		active_file = layout->rows*offsetx + offsety;
	else
		active_file = offsetx + layout->columns*offsety;
	return active_file;
}

void ED_fileselect_layout_tilepos(FileLayout* layout, int tile, int *x, int *y)
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
	return BLF_width(style->widget.uifont_id, (char *)str);
}

float file_font_pointsize()
{
	float s;
	char tmp[2] = "X";
	uiStyle *style= U.uistyles.first;
	uiStyleFontSet(&style->widget);
	s = BLF_height(style->widget.uifont_id, tmp);
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
			if (len > layout->column_widths[COLUMN_NAME]) layout->column_widths[COLUMN_NAME] = len + 20;
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
	FileSelectParams *params = ED_fileselect_get_params(sfile);
	FileLayout *layout=0;
	View2D *v2d= &ar->v2d;
	int maxlen = 0;
	int numfiles;
	int textheight;
	if (sfile->layout == 0) {
		sfile->layout = MEM_callocN(sizeof(struct FileLayout), "file_layout");
		sfile->layout->dirty = 1;
	} 

	if (!sfile->layout->dirty) return;

	numfiles = filelist_numfiles(sfile->files);
	textheight = file_font_pointsize();
	layout = sfile->layout;
	layout->textheight = textheight;

	if (params->display == FILE_IMGDISPLAY) {
		layout->prv_w = 96;
		layout->prv_h = 96;
		layout->tile_border_x = 6;
		layout->tile_border_y = 6;
		layout->prv_border_x = 6;
		layout->prv_border_y = 6;
		layout->tile_w = layout->prv_w + 2*layout->prv_border_x;
		layout->tile_h = layout->prv_h + 2*layout->prv_border_y + textheight;
		layout->width= (v2d->cur.xmax - v2d->cur.xmin - 2*layout->tile_border_x);
		layout->columns= layout->width / (layout->tile_w + 2*layout->tile_border_x);
		if(layout->columns > 0)
			layout->rows= numfiles/layout->columns + 1; // XXX dirty, modulo is zero
		else {
			layout->columns = 1;
			layout->rows= numfiles + 1; // XXX dirty, modulo is zero
		}
		layout->height= sfile->layout->rows*(layout->tile_h+2*layout->tile_border_y) + layout->tile_border_y*2;
		layout->flag = FILE_LAYOUT_VER;
	} else {
		layout->prv_w = 0;
		layout->prv_h = 0;
		layout->tile_border_x = 8;
		layout->tile_border_y = 2;
		layout->prv_border_x = 0;
		layout->prv_border_y = 0;
		layout->tile_h = textheight*3/2;
		layout->height= v2d->cur.ymax - v2d->cur.ymin - 2*layout->tile_border_y;
		layout->rows = layout->height / (layout->tile_h + 2*layout->tile_border_y);
        
		column_widths(sfile->files, layout);

		if (params->display == FILE_SHORTDISPLAY) {
			maxlen = layout->column_widths[COLUMN_NAME] + 12 +
					 layout->column_widths[COLUMN_SIZE];
			maxlen += 20; // for icon
		} else {
			maxlen = layout->column_widths[COLUMN_NAME] + 12 +
#ifndef WIN32
					 layout->column_widths[COLUMN_MODE1] + 12 +
					 layout->column_widths[COLUMN_MODE2] + 12 +
					 layout->column_widths[COLUMN_MODE3] + 12 +
					 layout->column_widths[COLUMN_OWNER] + 12 +
#endif
					 layout->column_widths[COLUMN_DATE] + 12 +
					 layout->column_widths[COLUMN_TIME] + 12 +
					 layout->column_widths[COLUMN_SIZE];
			maxlen += 20; // for icon
		}
		layout->tile_w = maxlen;
		if(layout->rows > 0)
			layout->columns = numfiles/layout->rows + 1; // XXX dirty, modulo is zero
		else {
			layout->rows = 1;
			layout->columns = numfiles + 1; // XXX dirty, modulo is zero
		}
		layout->width = sfile->layout->columns * (layout->tile_w + 2*layout->tile_border_x) + layout->tile_border_x*2;
		layout->flag = FILE_LAYOUT_HOR;
	}
	layout->dirty= 0;
}

FileLayout* ED_fileselect_get_layout(struct SpaceFile *sfile, struct ARegion *ar)
{
	if (!sfile->layout) {
		ED_fileselect_init_layout(sfile, ar);
	}
	return sfile->layout;
}

void file_change_dir(bContext *C, int checkdir)
{
	SpaceFile *sfile= CTX_wm_space_file(C);

	if (sfile->params) {

		ED_fileselect_clear(C, sfile);

		if(checkdir && BLI_is_dir(sfile->params->dir)==0) {
			BLI_strncpy(sfile->params->dir, filelist_dir(sfile->files), sizeof(sfile->params->dir));
			/* could return but just refresh the current dir */
		}
		filelist_setdir(sfile->files, sfile->params->dir);
		
		if(folderlist_clear_next(sfile))
			folderlist_free(sfile->folders_next);

		folderlist_pushdir(sfile->folders_prev, sfile->params->dir);

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
				file->flags |= ACTIVEFILE;
				match = 1;
			}
		}
	}
	return match;
}

void autocomplete_directory(struct bContext *C, char *str, void *arg_v)
{
	SpaceFile *sfile= CTX_wm_space_file(C);

	/* search if str matches the beginning of name */
	if(str[0] && sfile->files) {
		char dirname[FILE_MAX];

		DIR *dir;
		struct dirent *de;
		
		BLI_split_dirfile(str, dirname, NULL);

		dir = opendir(dirname);

		if(dir) {
			AutoComplete *autocpl= autocomplete_begin(str, FILE_MAX);

			while ((de = readdir(dir)) != NULL) {
				if (strcmp(".", de->d_name)==0 || strcmp("..", de->d_name)==0) {
					/* pass */
				}
				else {
					char path[FILE_MAX];
					struct stat status;
					
					BLI_join_dirfile(path, dirname, de->d_name);

					if (stat(path, &status) == 0) {
						if (S_ISDIR(status.st_mode)) { /* is subdir */
							autocomplete_do_name(autocpl, path);
						}
					}
				}
			}
			closedir(dir);

			autocomplete_end(autocpl, str);
			if (BLI_exists(str)) {
				BLI_add_slash(str);
			} else {
				BLI_strncpy(sfile->params->dir, str, sizeof(sfile->params->dir));
			}
		}
	}
}

void autocomplete_file(struct bContext *C, char *str, void *arg_v)
{
	SpaceFile *sfile= CTX_wm_space_file(C);

	/* search if str matches the beginning of name */
	if(str[0] && sfile->files) {
		AutoComplete *autocpl= autocomplete_begin(str, FILE_MAX);
		int nentries = filelist_numfiles(sfile->files);
		int i;

		for(i= 0; i<nentries; ++i) {
			struct direntry* file = filelist_file(sfile->files, i);
			if (file && S_ISREG(file->type)) {
				autocomplete_do_name(autocpl, file->relname);
			}
		}
		autocomplete_end(autocpl, str);
	}
}

void ED_fileselect_clear(struct bContext *C, struct SpaceFile *sfile)
{
	thumbnails_stop(sfile->files, C);
	filelist_freelib(sfile->files);
	filelist_free(sfile->files);
	sfile->params->active_file = -1;
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);
}

void ED_fileselect_exit(struct bContext *C, struct SpaceFile *sfile)
{
	thumbnails_stop(sfile->files, C);
}
