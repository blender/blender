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
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_storage_types.h"
#include "BLI_dynstr.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"

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


FileSelectParams* ED_fileselect_get_params(struct SpaceFile *sfile)
{
	if (!sfile->params) {
		ED_fileselect_set_params(sfile, FILE_UNIX, "", "/", 0, FILE_SHORTDISPLAY, 0);
	}
	return sfile->params;
}

short ED_fileselect_set_params(SpaceFile *sfile, int type, const char *title, const char *path,
							   short flag, short display, short filter)
{
	char name[FILE_MAX], dir[FILE_MAX], file[FILE_MAX];
	FileSelectParams *params;

	if (!sfile->params) {
		sfile->params= MEM_callocN(sizeof(FileSelectParams), "fileselparams");
	}

	params = sfile->params;

	params->type = type;
	params->flag = flag;
	params->display = display;
	params->filter = filter;

	BLI_strncpy(params->title, title, sizeof(params->title));
	
	BLI_strncpy(name, path, sizeof(name));
	BLI_convertstringcode(name, G.sce);
	
	switch(type) {
		case FILE_MAIN:
			break;
		case FILE_LOADLIB:
			break;
		case FILE_BLENDER:
		case FILE_LOADFONT:
		default:
			{
				BLI_split_dirfile(name, dir, file);
				BLI_strncpy(params->file, file, sizeof(params->file));
				BLI_strncpy(params->dir, dir, sizeof(params->dir));
				BLI_make_file_string(G.sce, params->dir, dir, ""); /* XXX needed ? - also solve G.sce */			
			}
			break;
	}

	return 1;
}

void ED_fileselect_reset_params(SpaceFile *sfile)
{
	sfile->params->type = FILE_UNIX;
	sfile->params->flag = 0;
	sfile->params->title[0] = '\0';
}


int ED_fileselect_layout_offset(FileLayout* layout, int x, int y)
{
	int offsetx, offsety;
	int active_file;

	offsetx = (x)/(layout->tile_w + 2*layout->tile_border_x);
	offsety = (y)/(layout->tile_h + 2*layout->tile_border_y);
	
	if (offsetx > layout->columns-1) offsetx = layout->columns-1 ;
	if (offsety > layout->rows-1) offsety = layout->rows-1 ;

	if (layout->flag & FILE_LAYOUT_HOR) 
		active_file = layout->rows*offsetx + offsety;
	else
		active_file = offsetx + layout->columns*offsety;
	printf("OFFSET %d %d %d %d %d\n", x,y, offsetx, offsety, active_file);
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


void ED_fileselect_init_layout(struct SpaceFile *sfile, struct ARegion *ar)
{
	FileSelectParams* params = ED_fileselect_get_params(sfile);
	View2D *v2d= &ar->v2d;
	int width=0, height=0;
	int rows, columns;
	int i;
	int maxlen = 0;
	int numfiles = filelist_numfiles(sfile->files);

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
		sfile->layout->tile_h = sfile->layout->prv_h + 2*sfile->layout->prv_border_y + U.fontsize;
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
	} else if (params->display == FILE_SHORTDISPLAY) {
		sfile->layout->prv_w = 0;
		sfile->layout->prv_h = 0;
		sfile->layout->tile_border_x = 8;
		sfile->layout->tile_border_y = 2;
		sfile->layout->prv_border_x = 0;
		sfile->layout->prv_border_y = 0;
		sfile->layout->tile_w = 240;
		sfile->layout->tile_h = U.fontsize*3/2;
		sfile->layout->height= v2d->cur.ymax - v2d->cur.ymin;
		sfile->layout->rows = sfile->layout->height / (sfile->layout->tile_h + 2*sfile->layout->tile_border_y);;

		maxlen = filelist_maxnamelen(sfile->files);
		sfile->layout->tile_w = maxlen + 100;
		if(sfile->layout->rows > 0)
			sfile->layout->columns = numfiles/sfile->layout->rows + 1; // XXX dirty, modulo is zero
		else {
			sfile->layout->rows = 1;
			sfile->layout->columns = numfiles + 1; // XXX dirty, modulo is zero
		}
		sfile->layout->width = sfile->layout->columns * (sfile->layout->tile_w + 2*sfile->layout->tile_border_x) + sfile->layout->tile_border_x*2;
		sfile->layout->flag = FILE_LAYOUT_HOR;
	} else {
		sfile->layout->prv_w = 0;
		sfile->layout->prv_h = 0;
		sfile->layout->tile_border_x = 8;
		sfile->layout->tile_border_y = 2;
		sfile->layout->prv_border_x = 0;
		sfile->layout->prv_border_y = 0;
		sfile->layout->tile_w = v2d->cur.xmax - v2d->cur.xmin - 2*sfile->layout->tile_border_x;
		sfile->layout->tile_h = U.fontsize*3/2;
		sfile->layout->width= (v2d->cur.xmax - v2d->cur.xmin + 2*sfile->layout->tile_border_x);
		sfile->layout->rows= numfiles+1;
		sfile->layout->columns= 1;
		sfile->layout->height= sfile->layout->rows*(sfile->layout->tile_h+2*sfile->layout->tile_border_y) + sfile->layout->tile_border_y*2;	
		sfile->layout->flag = FILE_LAYOUT_VER;
	}
}

FileLayout* ED_fileselect_get_layout(struct SpaceFile *sfile, struct ARegion *ar)
{
	if (!sfile->layout) {
		ED_fileselect_init_layout(sfile, ar);
	}
	return sfile->layout;
}