/**
 * $Id:
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
 * Contributor(s): Andrea Weikert (c) 2008 Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"

#include "BLI_blenlib.h"
#include "BLI_storage_types.h"
#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_fileselect.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "file_intern.h"
#include "filelist.h"
#include "fsmenu.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* for events */
#define NOTACTIVE			0
#define ACTIVATE			1
#define INACTIVATE			2


static void set_active_file_thumbs(SpaceFile *sfile, FileSelectParams* params, struct ARegion* ar, short mval[])
{
	float x,y;
	int active_file = -1;
	struct direntry* file;
	int offsetx, offsety;
	int numfiles = filelist_numfiles(params->files);
	int columns;

	View2D* v2d = &ar->v2d;
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	
	offsetx = (x - (v2d->cur.xmin+sfile->tile_border_x))/(sfile->tile_w + sfile->tile_border_x);
	offsety = (-y+sfile->tile_border_y)/(sfile->tile_h + sfile->tile_border_y);
	columns = (v2d->cur.xmax - v2d->cur.xmin) / (sfile->tile_w+ sfile->tile_border_x);
	active_file = offsetx + columns*offsety;

	if (active_file >= 0 && active_file < numfiles )
	{
		params->active_file = active_file;
		if (params->selstate & ACTIVATE) {
			file = filelist_file(params->files, params->active_file);
			file->flags |= ACTIVE;
		}			
	}
}


static void set_active_file(SpaceFile *sfile, FileSelectParams* params, struct ARegion* ar, short mval[])
{
	int offsetx, offsety;
	float x,y;
	int active_file = -1;
	int numfiles = filelist_numfiles(params->files);
	int rows;
	struct direntry* file;

	View2D* v2d = &ar->v2d;
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	
	offsetx = (x-sfile->tile_border_x)/(sfile->tile_w + sfile->tile_border_x);
	offsety = (v2d->cur.ymax-y-sfile->tile_border_y)/(sfile->tile_h + sfile->tile_border_y);
	rows = (v2d->cur.ymax - v2d->cur.ymin - 2*sfile->tile_border_y) / (sfile->tile_h+sfile->tile_border_y);
	active_file = rows*offsetx + offsety;
	if ( (active_file >= 0) && (active_file < numfiles) )
	{
		params->active_file = active_file;
		if (params->selstate & ACTIVATE) {
			file = filelist_file(params->files, params->active_file);
			file->flags |= ACTIVE;
		}			
	} 
}


static void set_active_bookmark(SpaceFile *sfile, FileSelectParams* params, struct ARegion* ar, short y)
{
	int nentries = fsmenu_get_nentries();
	short posy = ar->v2d.mask.ymax - TILE_BORDER_Y - y;
	params->active_bookmark = ((float)posy / (U.fontsize*3.0f/2.0f));
	if (params->active_bookmark < 0 || params->active_bookmark > nentries) {
		params->active_bookmark = -1;
	}
}

static void mouse_select(SpaceFile* sfile, FileSelectParams* params, ARegion* ar, short *mval)
{
	int numfiles = filelist_numfiles(params->files);
	if(mval[0]>ar->v2d.mask.xmin && mval[0]<ar->v2d.mask.xmax
		&& mval[1]>ar->v2d.mask.ymin && mval[1]<ar->v2d.mask.ymax) {
			params->selstate = NOTACTIVE;
			if (params->display) {
				set_active_file_thumbs(sfile, params, ar, mval);
			} else {
				set_active_file(sfile, params, ar, mval);
			}
			if (params->active_file >= 0 && params->active_file < numfiles) {
				struct direntry* file = filelist_file(params->files, params->active_file);
				
				if(file && S_ISDIR(file->type)) {
					/* the path is too long and we are not going up! */
					if (strcmp(file->relname, ".") &&
						strcmp(file->relname, "..") &&
						strlen(params->dir) + strlen(file->relname) >= FILE_MAX ) 
					{
						// XXX error("Path too long, cannot enter this directory");
					} else {
						if (strcmp(file->relname, "..")==0) {
							/* avoids /../../ */
							BLI_parent_dir(params->dir);
						} else {
							strcat(params->dir, file->relname);
							strcat(params->dir,"/");
							params->file[0] = '\0';
							BLI_cleanup_dir(G.sce, params->dir);
						}
						filelist_setdir(params->files, params->dir);
						filelist_free(params->files);
						params->active_file = -1;
					}
				}
				else if (file)
				{
					if (file->relname) {
						BLI_strncpy(params->file, file->relname, FILE_MAXFILE);
						/* XXX
						if(event==MIDDLEMOUSE && filelist_gettype(sfile->files)) 
							imasel_execute(sfile);
						*/
					}
					
				}	
				/* XXX
				if(BIF_filelist_gettype(sfile->files)==FILE_MAIN) {
					active_imasel_object(sfile);
				}
				*/
			}
		}
}

static void mouse_select_bookmark(SpaceFile* sfile, ARegion* ar, short *mval)
{
	if(mval[0]>ar->v2d.mask.xmin && mval[0]<ar->v2d.mask.xmax
	&& mval[1]>ar->v2d.mask.ymin && mval[1]<ar->v2d.mask.ymax) {
		char *selected;
		set_active_bookmark(sfile, sfile->params, ar, mval[1]);
		selected= fsmenu_get_entry(sfile->params->active_bookmark);			
		/* which string */
		if (selected) {
			FileSelectParams* params = sfile->params;
			BLI_strncpy(params->dir, selected, sizeof(params->dir));
			BLI_cleanup_dir(G.sce, params->dir);
			filelist_free(params->files);	
			filelist_setdir(params->files, params->dir);
			params->file[0] = '\0';			
			params->active_file = -1;
		}
	}
}

static int file_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	short mval[2];
	
	/* note; otherwise opengl select won't work. do this for every glSelectBuffer() */
	wmSubWindowSet(CTX_wm_window(C), ar->swinid);
	
	mval[0]= event->x - ar->winrct.xmin;
	mval[1]= event->y - ar->winrct.ymin;
	mouse_select(sfile, sfile->params, ar, mval);
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	return OPERATOR_FINISHED;
}


void ED_FILE_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Activate/Select File";
	ot->idname= "ED_FILE_OT_select";
	
	/* api callbacks */
	ot->invoke= file_select_invoke;
	ot->poll= ED_operator_file_active;
}


static int bookmark_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	short mval[2];
	
	/* note; otherwise opengl select won't work. do this for every glSelectBuffer() */
	wmSubWindowSet(CTX_wm_window(C), ar->swinid);
	
	mval[0]= event->x - ar->winrct.xmin;
	mval[1]= event->y - ar->winrct.ymin;
	mouse_select_bookmark(sfile, ar, mval);
	ED_area_tag_redraw(sa);
	return OPERATOR_FINISHED;
}

void ED_FILE_OT_select_bookmark(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Directory";
	ot->idname= "ED_FILE_OT_select_bookmark";
	
	/* api callbacks */
	ot->invoke= bookmark_select_invoke;
	ot->poll= ED_operator_file_active;
}