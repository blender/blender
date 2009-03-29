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

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

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

/* ---------- FILE SELECTION ------------ */

static int find_file_mouse_hor(SpaceFile *sfile, struct ARegion* ar, short x, short y)
{
	float fx,fy;
	int active_file = -1;
	int numfiles = filelist_numfiles(sfile->files);
	View2D* v2d = &ar->v2d;

	UI_view2d_region_to_view(v2d, x, y, &fx, &fy);

	active_file = ED_fileselect_layout_offset(sfile->layout, v2d->tot.xmin + fx, v2d->tot.ymax - fy);

	printf("FINDFILE %d\n", active_file);
	if ( (active_file < 0) || (active_file >= numfiles) )
	{
		active_file = -1;
	}
	return active_file;
}


static int find_file_mouse_vert(SpaceFile *sfile, struct ARegion* ar, short x, short y)
{
	float fx,fy;
	int active_file = -1;
	int numfiles = filelist_numfiles(sfile->files);
	View2D* v2d = &ar->v2d;

	UI_view2d_region_to_view(v2d, x, y, &fx, &fy);
	
	active_file = ED_fileselect_layout_offset(sfile->layout, v2d->tot.xmin + fx, v2d->tot.ymax - fy);

	if ( (active_file < 0) || (active_file >= numfiles) )
	{
		active_file = -1;
	}
	return active_file;
}

static void file_deselect_all(SpaceFile* sfile)
{
	int numfiles = filelist_numfiles(sfile->files);
	int i;

	for ( i=0; i < numfiles; ++i) {
		struct direntry* file = filelist_file(sfile->files, i);
		if (file && (file->flags & ACTIVE)) {
			file->flags &= ~ACTIVE;
		}
	}
}

static void file_select(SpaceFile* sfile, ARegion* ar, const rcti* rect, short val)
{
	int first_file = -1;
	int last_file = -1;
	int act_file;
	short selecting = (val == LEFTMOUSE);
	FileSelectParams *params = ED_fileselect_get_params(sfile);
	FileLayout *layout = ED_fileselect_get_layout(sfile, ar);

	int numfiles = filelist_numfiles(sfile->files);

	params->selstate = NOTACTIVE;
	if  ( (layout->flag == FILE_LAYOUT_HOR) ) {
		first_file = find_file_mouse_hor(sfile, ar, rect->xmin, rect->ymax);
		last_file = find_file_mouse_hor(sfile, ar, rect->xmax, rect->ymin);
	} else {
		first_file = find_file_mouse_vert(sfile, ar, rect->xmin, rect->ymax);
		last_file = find_file_mouse_vert(sfile, ar, rect->xmax, rect->ymin);
	}
	
	/* select all valid files between first and last indicated */
	if ( (first_file >= 0) && (first_file < numfiles) && (last_file >= 0) && (last_file < numfiles) ) {
		for (act_file = first_file; act_file <= last_file; act_file++) {
			struct direntry* file = filelist_file(sfile->files, act_file);
			if (selecting) 
				file->flags |= ACTIVE;
			else
				file->flags &= ~ACTIVE;
		}
	}
	
	printf("Selecting %d %d\n", first_file, last_file);

	/* make the last file active */
	if (last_file >= 0 && last_file < numfiles) {
		struct direntry* file = filelist_file(sfile->files, last_file);
		params->active_file = last_file;

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
				filelist_setdir(sfile->files, params->dir);
				filelist_free(sfile->files);
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



static int file_border_select_exec(bContext *C, wmOperator *op)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	short val;
	rcti rect;

	val= RNA_int_get(op->ptr, "event_type");
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");

	file_select(sfile, ar, &rect, val );
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	return OPERATOR_FINISHED;
}

void FILE_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Activate/Select File";
	ot->idname= "FILE_OT_select_border";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= file_border_select_exec;
	ot->modal= WM_border_select_modal;

	/* rna */
	RNA_def_int(ot->srna, "event_type", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);

	ot->poll= ED_operator_file_active;
}

static int file_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	short val;
	rcti rect;

	rect.xmin = rect.xmax = event->x - ar->winrct.xmin;
	rect.ymin = rect.ymax = event->y - ar->winrct.ymin;
	val = event->val;

	if (BLI_in_rcti(&ar->v2d.mask, rect.xmin, rect.ymin)) { 

		/* single select, deselect all selected first */
		file_deselect_all(sfile);
		file_select(sfile, ar, &rect, val );
		WM_event_add_notifier(C, NC_WINDOW, NULL);
	}
	return OPERATOR_FINISHED;
}

void FILE_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Activate/Select File";
	ot->idname= "FILE_OT_select";
	
	/* api callbacks */
	ot->invoke= file_select_invoke;

	/* rna */

	ot->poll= ED_operator_file_active;
}

static int file_select_all_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	int numfiles = filelist_numfiles(sfile->files);
	int i;
	int select = 1;

	/* if any file is selected, deselect all first */
	for ( i=0; i < numfiles; ++i) {
		struct direntry* file = filelist_file(sfile->files, i);
		if (file && (file->flags & ACTIVE)) {
			file->flags &= ~ACTIVE;
			select = 0;
			ED_area_tag_redraw(sa);
		}
	}
	/* select all only if previously no file was selected */
	if (select) {
		for ( i=0; i < numfiles; ++i) {
			struct direntry* file = filelist_file(sfile->files, i);
			if(file && !S_ISDIR(file->type)) {
				file->flags |= ACTIVE;
				ED_area_tag_redraw(sa);
			}
		}
	}
	return OPERATOR_FINISHED;
}

void FILE_OT_select_all_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select/Deselect all files";
	ot->idname= "FILE_OT_select_all_toggle";
	
	/* api callbacks */
	ot->invoke= file_select_all_invoke;

	/* rna */

	ot->poll= ED_operator_file_active;
}

/* ---------- BOOKMARKS ----------- */

static void set_active_bookmark(FileSelectParams* params, struct ARegion* ar, short x, short y)
{
	int nentries = fsmenu_get_nentries(fsmenu_get(), FS_CATEGORY_BOOKMARKS);
	float fx, fy;
	short posy;

	UI_view2d_region_to_view(&ar->v2d, x, y, &fx, &fy);

	posy = ar->v2d.cur.ymax - 2*TILE_BORDER_Y - fy;
	posy -= U.fontsize*2.0f;	/* header */
	
	params->active_bookmark = ((float)posy / (U.fontsize*2.0f));
	if (params->active_bookmark < 0 || params->active_bookmark > nentries) {
		params->active_bookmark = -1;
	}
}

static int file_select_bookmark_category(SpaceFile* sfile, ARegion* ar, short x, short y, FSMenuCategory category)
{
	struct FSMenu* fsmenu = fsmenu_get();
	int nentries = fsmenu_get_nentries(fsmenu, category);
	int linestep = U.fontsize*2.0f;
	short xs, ys;
	int i;
	int selected = -1;

	for (i=0; i < nentries; ++i) {
		fsmenu_get_pos(fsmenu, category, i, &xs, &ys);
		if ( (y<=ys) && (y>ys-linestep) ) {
			fsmenu_select_entry(fsmenu, category, i);
			selected = i;
			break;
		}
	}
	return selected;
}

static void file_select_bookmark(SpaceFile* sfile, ARegion* ar, short x, short y)
{
	float fx, fy;
	int selected;
	FSMenuCategory category = FS_CATEGORY_SYSTEM;

	if (BLI_in_rcti(&ar->v2d.mask, x, y)) {
		char *entry;

		UI_view2d_region_to_view(&ar->v2d, x, y, &fx, &fy);
		selected = file_select_bookmark_category(sfile, ar, fx, fy, FS_CATEGORY_SYSTEM);
		if (selected<0) {
			category = FS_CATEGORY_BOOKMARKS;
			selected = file_select_bookmark_category(sfile, ar, fx, fy, category);
		}
		if (selected<0) {
			category = FS_CATEGORY_RECENT;
			selected = file_select_bookmark_category(sfile, ar, fx, fy, category);
		}
		
		if (selected>=0) {
			entry= fsmenu_get_entry(fsmenu_get(), category, selected);			
			/* which string */
			if (entry) {
				FileSelectParams* params = sfile->params;
				BLI_strncpy(params->dir, entry, sizeof(params->dir));
				BLI_cleanup_dir(G.sce, params->dir);
				filelist_free(sfile->files);	
				filelist_setdir(sfile->files, params->dir);
				params->file[0] = '\0';			
				params->active_file = -1;
			}
		}
	}
}

static int bookmark_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);	
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);

	short x, y;

	x = event->x - ar->winrct.xmin;
	y = event->y - ar->winrct.ymin;

	file_select_bookmark(sfile, ar, x, y);
	ED_area_tag_redraw(sa);
	return OPERATOR_FINISHED;
}

void FILE_OT_select_bookmark(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Directory";
	ot->idname= "FILE_OT_select_bookmark";
	
	/* api callbacks */
	ot->invoke= bookmark_select_invoke;
	ot->poll= ED_operator_file_active;
}

static int loadimages_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	if (sfile->files) {
		filelist_loadimage_timer(sfile->files);
		if (filelist_changed(sfile->files)) {
			ED_area_tag_redraw(sa);
		}
	}

	return OPERATOR_FINISHED;
}

void FILE_OT_loadimages(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Load Images";
	ot->idname= "FILE_OT_loadimages";
	
	/* api callbacks */
	ot->invoke= loadimages_invoke;
	
	ot->poll= ED_operator_file_active;
}

int file_hilight_set(SpaceFile *sfile, ARegion *ar, int mx, int my)
{
	FileSelectParams* params;
	FileLayout* layout;
	int numfiles, actfile;
	
	if(sfile==NULL || sfile->files==NULL) return 0;
	
	numfiles = filelist_numfiles(sfile->files);
	params = ED_fileselect_get_params(sfile);
	layout = ED_fileselect_get_layout(sfile, ar);

	if ( (layout->flag == FILE_LAYOUT_HOR)) {
		actfile = find_file_mouse_hor(sfile, ar, mx , my);
	} else {
		actfile = find_file_mouse_vert(sfile, ar, mx, my);
	}
	
	if (params && (actfile >= 0) && (actfile < numfiles) ) {
		params->active_file=actfile;
		return 1;
	}
	return 0;
}

static int file_highlight_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	
	if( file_hilight_set(sfile, ar, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin)) {
		ED_area_tag_redraw(CTX_wm_area(C));
	}
	
	return OPERATOR_FINISHED;
}

void FILE_OT_highlight(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Highlight File";
	ot->idname= "FILE_OT_highlight";
	
	/* api callbacks */
	ot->invoke= file_highlight_invoke;
	ot->poll= ED_operator_file_active;
}

int file_cancel_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	
	WM_event_fileselect_event(C, sfile->op, EVT_FILESELECT_CANCEL);
	sfile->op = NULL;
	
	return OPERATOR_FINISHED;
}

void FILE_OT_cancel(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Cancel File Load";
	ot->idname= "FILE_OT_cancel";
	
	/* api callbacks */
	ot->exec= file_cancel_exec;
	ot->poll= ED_operator_file_active;
}

/* sends events now, so things get handled on windowqueue level */
int file_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	char name[FILE_MAX];
	
	if(sfile->op) {
		wmOperator *op= sfile->op;
		
		sfile->op = NULL;
		BLI_strncpy(name, sfile->params->dir, sizeof(name));
		strcat(name, sfile->params->file);
		RNA_string_set(op->ptr, "filename", name);
		
		fsmenu_insert_entry(fsmenu_get(), FS_CATEGORY_RECENT, sfile->params->dir,0, 1);
		BLI_make_file_string(G.sce, name, BLI_gethome(), ".Bfs");
		fsmenu_write_file(fsmenu_get(), name);
		WM_event_fileselect_event(C, op, EVT_FILESELECT_EXEC);
	}
				
	return OPERATOR_FINISHED;
}

void FILE_OT_exec(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Execute File Window";
	ot->idname= "FILE_OT_exec";
	
	/* api callbacks */
	ot->exec= file_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}


int file_parent_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	
	if(sfile->params) {
		BLI_parent_dir(sfile->params->dir);
		filelist_setdir(sfile->files, sfile->params->dir);
		filelist_free(sfile->files);
		sfile->params->active_file = -1;
	}		
	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;

}


void FILE_OT_parent(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Parent File";
	ot->idname= "FILE_OT_parent";
	
	/* api callbacks */
	ot->exec= file_parent_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}


int file_refresh_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	
	if(sfile->params) {
		filelist_setdir(sfile->files, sfile->params->dir);
		filelist_free(sfile->files);
		sfile->params->active_file = -1;
	}		
	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;

}


void FILE_OT_refresh(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Refresh Filelist";
	ot->idname= "FILE_OT_refresh";
	
	/* api callbacks */
	ot->exec= file_refresh_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}

struct ARegion *file_buttons_region(struct ScrArea *sa)
{
	ARegion *ar;
	
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==RGN_TYPE_CHANNELS)
			return ar;
	return NULL;
}

int file_bookmark_toggle_exec(bContext *C, wmOperator *unused)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= file_buttons_region(sa);
	
	if(ar) {
		ar->flag ^= RGN_FLAG_HIDDEN;
		ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}
	return OPERATOR_FINISHED;
}

void FILE_OT_bookmark_toggle(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Bookmarks";
	ot->idname= "FILE_OT_bookmark_toggle";
	
	/* api callbacks */
	ot->exec= file_bookmark_toggle_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}
