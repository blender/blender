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

#include "MEM_guardedalloc.h"

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

static int find_file_mouse(SpaceFile *sfile, struct ARegion* ar, int x, int y)
{
	float fx,fy;
	int active_file = -1;
	View2D* v2d = &ar->v2d;

	UI_view2d_region_to_view(v2d, x, y, &fx, &fy);

	active_file = ED_fileselect_layout_offset(sfile->layout, v2d->tot.xmin + fx, v2d->tot.ymax - fy);
	
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

typedef enum FileSelect { FILE_SELECT_DIR = 1, 
  FILE_SELECT_FILE = 2 } FileSelect;


static void clamp_to_filelist(int numfiles, int *first_file, int *last_file)
{
	/* border select before the first file */
	if ( (*first_file < 0) && (*last_file >=0 ) ) {
		*first_file = 0;
	}
	/* don't select if everything is outside filelist */
	if ( (*first_file >= numfiles) && ((*last_file < 0) || (*last_file >= numfiles)) ) {
		*first_file = -1;
		*last_file = -1;
	}
	
	/* fix if last file invalid */
	if ( (*first_file > 0) && (*last_file < 0) )
		*last_file = numfiles-1;

	/* clamp */
	if ( (*first_file >= numfiles) ) {
		*first_file = numfiles-1;
	}
	if ( (*last_file >= numfiles) ) {
		*last_file = numfiles-1;
	}
}

static FileSelect file_select(SpaceFile* sfile, ARegion* ar, const rcti* rect, short val)
{
	int first_file = -1;
	int last_file = -1;
	int act_file;
	short selecting = (val == LEFTMOUSE);
	FileSelect retval = FILE_SELECT_FILE;

	FileSelectParams *params = ED_fileselect_get_params(sfile);
	// FileLayout *layout = ED_fileselect_get_layout(sfile, ar);

	int numfiles = filelist_numfiles(sfile->files);

	params->selstate = NOTACTIVE;
	first_file = find_file_mouse(sfile, ar, rect->xmin, rect->ymax);
	last_file = find_file_mouse(sfile, ar, rect->xmax, rect->ymin);
	
	clamp_to_filelist(numfiles, &first_file, &last_file);

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

	/* Don't act on multiple selected files */
	if (first_file != last_file) selecting= 0;

	/* make the last file active */
	if (selecting && (last_file >= 0 && last_file < numfiles)) {
		struct direntry* file = filelist_file(sfile->files, last_file);
		params->active_file = last_file;

		if(file && S_ISDIR(file->type)) {
			/* the path is too long and we are not going up! */
			if (strcmp(file->relname, "..") && strlen(params->dir) + strlen(file->relname) >= FILE_MAX ) 
			{
				// XXX error("Path too long, cannot enter this directory");
			} else {
				if (strcmp(file->relname, "..")==0) { 	 
					/* avoids /../../ */ 	 
					BLI_parent_dir(params->dir); 	 
				} else {
					BLI_cleanup_dir(G.sce, params->dir);
					strcat(params->dir, file->relname);
					BLI_add_slash(params->dir);
				}

				file_change_dir(sfile, 0);
				retval = FILE_SELECT_DIR;
			}
		}
		else if (file)
		{
			if (file->relname) {
				BLI_strncpy(params->file, file->relname, FILE_MAXFILE);
			}
			
		}	
	} 
	return retval;
}



static int file_border_select_exec(bContext *C, wmOperator *op)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceFile *sfile= CTX_wm_space_file(C);
	short val;
	rcti rect;

	val= RNA_int_get(op->ptr, "event_type");
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");

	BLI_isect_rcti(&(ar->v2d.mask), &rect, &rect);
	
	if (FILE_SELECT_DIR == file_select(sfile, ar, &rect, val )) {
		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);
	} else {
		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);
	}
	return OPERATOR_FINISHED;
}

void FILE_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Activate/Select File";
	ot->description= "Activate/select the file(s) contained in the border.";
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
	SpaceFile *sfile= CTX_wm_space_file(C);
	short val;
	rcti rect;

	if(ar->regiontype != RGN_TYPE_WINDOW)
		return OPERATOR_CANCELLED;

	rect.xmin = rect.xmax = event->x - ar->winrct.xmin;
	rect.ymin = rect.ymax = event->y - ar->winrct.ymin;
	val = event->val;

	if(!BLI_in_rcti(&ar->v2d.mask, rect.xmin, rect.ymin))
		return OPERATOR_CANCELLED;

	/* single select, deselect all selected first */
	file_deselect_all(sfile);

	if (FILE_SELECT_DIR == file_select(sfile, ar, &rect, val ))
		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);
	else
		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	WM_event_add_mousemove(C); /* for directory changes */
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	return OPERATOR_FINISHED;
}

void FILE_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Activate/Select File";
	ot->description= "Activate/select file.";
	ot->idname= "FILE_OT_select";
	
	/* api callbacks */
	ot->invoke= file_select_invoke;

	/* rna */

	ot->poll= ED_operator_file_active;
}

static int file_select_all_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceFile *sfile= CTX_wm_space_file(C);
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
	ot->description= "Select/deselect all files.";
	ot->idname= "FILE_OT_select_all_toggle";
	
	/* api callbacks */
	ot->invoke= file_select_all_invoke;

	/* rna */

	ot->poll= ED_operator_file_active;
}

/* ---------- BOOKMARKS ----------- */

static int bookmark_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceFile *sfile= CTX_wm_space_file(C);

	if(RNA_struct_find_property(op->ptr, "dir")) {
		char entry[256];
		FileSelectParams* params = sfile->params;

		RNA_string_get(op->ptr, "dir", entry);
		BLI_strncpy(params->dir, entry, sizeof(params->dir));
		BLI_cleanup_dir(G.sce, params->dir);
		file_change_dir(sfile, 1);

		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);
	}
	
	return OPERATOR_FINISHED;
}

void FILE_OT_select_bookmark(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Directory";
	ot->description= "Select a bookmarked directory.";
	ot->idname= "FILE_OT_select_bookmark";
	
	/* api callbacks */
	ot->invoke= bookmark_select_invoke;
	ot->poll= ED_operator_file_active;

	RNA_def_string(ot->srna, "dir", "", 256, "Dir", "");
}

static int bookmark_add_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceFile *sfile= CTX_wm_space_file(C);
	struct FSMenu* fsmenu = fsmenu_get();
	struct FileSelectParams* params= ED_fileselect_get_params(sfile);

	if (params->dir[0] != '\0') {
		char name[FILE_MAX];
	
		fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, params->dir, 0, 1);
		BLI_make_file_string("/", name, BLI_gethome(), ".Bfs");
		fsmenu_write_file(fsmenu, name);
	}

	ED_area_tag_redraw(sa);
	return OPERATOR_FINISHED;
}

void FILE_OT_add_bookmark(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Bookmark";
	ot->description= "Add a bookmark for the selected/active directory.";
	ot->idname= "FILE_OT_add_bookmark";
	
	/* api callbacks */
	ot->invoke= bookmark_add_invoke;
	ot->poll= ED_operator_file_active;
}

static int bookmark_delete_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	struct FSMenu* fsmenu = fsmenu_get();
	int nentries = fsmenu_get_nentries(fsmenu, FS_CATEGORY_BOOKMARKS);
	
	if(RNA_struct_find_property(op->ptr, "index")) {
		int index = RNA_int_get(op->ptr, "index");
		if ( (index >-1) && (index < nentries)) {
			char name[FILE_MAX];
			
			fsmenu_remove_entry(fsmenu, FS_CATEGORY_BOOKMARKS, index);
			BLI_make_file_string("/", name, BLI_gethome(), ".Bfs");
			fsmenu_write_file(fsmenu, name);
			ED_area_tag_redraw(sa);
		}
	}

	return OPERATOR_FINISHED;
}

void FILE_OT_delete_bookmark(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Bookmark";
	ot->description= "Delete selected bookmark.";
	ot->idname= "FILE_OT_delete_bookmark";
	
	/* api callbacks */
	ot->invoke= bookmark_delete_invoke;
	ot->poll= ED_operator_file_active;

	RNA_def_int(ot->srna, "index", -1, -1, 20000, "Index", "", -1, 20000);
}


static int loadimages_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceFile *sfile= CTX_wm_space_file(C);
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
	ot->description= "Load selected image(s).";
	ot->idname= "FILE_OT_loadimages";
	
	/* api callbacks */
	ot->invoke= loadimages_invoke;
	
	ot->poll= ED_operator_file_active;
}

int file_hilight_set(SpaceFile *sfile, ARegion *ar, int mx, int my)
{
	FileSelectParams* params;
	int numfiles, actfile, origfile;
	
	if(sfile==NULL || sfile->files==NULL) return 0;

	numfiles = filelist_numfiles(sfile->files);
	params = ED_fileselect_get_params(sfile);

	origfile= params->active_file;

	mx -= ar->winrct.xmin;
	my -= ar->winrct.ymin;

	if(BLI_in_rcti(&ar->v2d.mask, mx, my)) {
		actfile = find_file_mouse(sfile, ar, mx , my);

		if((actfile >= 0) && (actfile < numfiles))
			params->active_file=actfile;
		else
			params->active_file= -1;
	}
	else
		params->active_file= -1;

	return (params->active_file != origfile);
}

static int file_highlight_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceFile *sfile= CTX_wm_space_file(C);

	if(!file_hilight_set(sfile, ar, event->x, event->y))
		return OPERATOR_CANCELLED;

	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

void FILE_OT_highlight(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Highlight File";
	ot->description= "Highlight selected file(s).";
	ot->idname= "FILE_OT_highlight";
	
	/* api callbacks */
	ot->invoke= file_highlight_invoke;
	ot->poll= ED_operator_file_active;
}

int file_cancel_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= CTX_wm_space_file(C);

	folderlist_free(sfile->folders_prev);
	folderlist_free(sfile->folders_next);

	WM_event_fileselect_event(C, sfile->op, EVT_FILESELECT_CANCEL);
	sfile->op = NULL;
	
	if (sfile->files) {
		filelist_freelib(sfile->files);
		filelist_free(sfile->files);
		MEM_freeN(sfile->files);
		sfile->files= NULL;
	}
	
	return OPERATOR_FINISHED;
}

int file_operator_poll(bContext *C)
{
	int poll = ED_operator_file_active(C);
	SpaceFile *sfile= CTX_wm_space_file(C);

	if (!sfile || !sfile->op) poll= 0;

	return poll;
}

void FILE_OT_cancel(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Cancel File Load";
	ot->description= "Cancel loading of selected file.";
	ot->idname= "FILE_OT_cancel";
	
	/* api callbacks */
	ot->exec= file_cancel_exec;
	ot->poll= file_operator_poll;
}

/* sends events now, so things get handled on windowqueue level */
int file_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= CTX_wm_space_file(C);
	char name[FILE_MAX];
	
	if(sfile->op) {
		wmOperator *op= sfile->op;
		
		sfile->op = NULL;
		RNA_string_set(op->ptr, "filename", sfile->params->file);
		BLI_strncpy(name, sfile->params->dir, sizeof(name));
		RNA_string_set(op->ptr, "directory", name);
		strcat(name, sfile->params->file);

		if(RNA_struct_find_property(op->ptr, "relative_paths"))
			if(RNA_boolean_get(op->ptr, "relative_paths"))
				BLI_makestringcode(G.sce, name);

		RNA_string_set(op->ptr, "path", name);
		
		/* some ops have multiple files to select */
		{
			PointerRNA itemptr;
			int i, numfiles = filelist_numfiles(sfile->files);
			struct direntry *file;
			if(RNA_struct_find_property(op->ptr, "files")) {
				for (i=0; i<numfiles; i++) {
					file = filelist_file(sfile->files, i);
					if(file->flags & ACTIVE) {
						if ((file->type & S_IFDIR)==0) {
							RNA_collection_add(op->ptr, "files", &itemptr);
							RNA_string_set(&itemptr, "name", file->relname);
						}
					}
				}
			}
			
			if(RNA_struct_find_property(op->ptr, "dirs")) {
				for (i=0; i<numfiles; i++) {
					file = filelist_file(sfile->files, i);
					if(file->flags & ACTIVE) {
						if ((file->type & S_IFDIR)) {
							RNA_collection_add(op->ptr, "dirs", &itemptr);
							RNA_string_set(&itemptr, "name", file->relname);
						}
					}
				}
			}
		}
		
		folderlist_free(sfile->folders_prev);
		folderlist_free(sfile->folders_next);

		fsmenu_insert_entry(fsmenu_get(), FS_CATEGORY_RECENT, sfile->params->dir,0, 1);
		BLI_make_file_string(G.sce, name, BLI_gethome(), ".Bfs");
		fsmenu_write_file(fsmenu_get(), name);
		WM_event_fileselect_event(C, op, EVT_FILESELECT_EXEC);

		filelist_freelib(sfile->files);
		filelist_free(sfile->files);
		MEM_freeN(sfile->files);
		sfile->files= NULL;
	}
				
	return OPERATOR_FINISHED;
}

void FILE_OT_execute(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Execute File Window";
	ot->description= "Execute selected file.";
	ot->idname= "FILE_OT_execute";
	
	/* api callbacks */
	ot->exec= file_exec;
	ot->poll= file_operator_poll; 
}


int file_parent_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= CTX_wm_space_file(C);
	
	if(sfile->params) {
		if (BLI_has_parent(sfile->params->dir)) {
			BLI_parent_dir(sfile->params->dir);
			BLI_cleanup_dir(G.sce, sfile->params->dir);
			file_change_dir(sfile, 0);
			WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);
		}
	}		
	
	return OPERATOR_FINISHED;

}


void FILE_OT_parent(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Parent File";
	ot->description= "Move to parent directory.";
	ot->idname= "FILE_OT_parent";
	
	/* api callbacks */
	ot->exec= file_parent_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}


int file_refresh_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= CTX_wm_space_file(C);
	
	file_change_dir(sfile, 1);

	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);

	return OPERATOR_FINISHED;

}

void FILE_OT_previous(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Previous Folder";
	ot->description= "Move to previous folder.";
	ot->idname= "FILE_OT_previous";
	
	/* api callbacks */
	ot->exec= file_previous_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}

int file_previous_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= CTX_wm_space_file(C);

	if(sfile->params) {
		if (!sfile->folders_next)
			sfile->folders_next = folderlist_new();

		folderlist_pushdir(sfile->folders_next, sfile->params->dir);
		folderlist_popdir(sfile->folders_prev, sfile->params->dir);
		folderlist_pushdir(sfile->folders_next, sfile->params->dir);

		file_change_dir(sfile, 1);
	}
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);

	return OPERATOR_FINISHED;
}

void FILE_OT_next(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Next Folder";
	ot->description= "Move to next folder.";
	ot->idname= "FILE_OT_next";
	
	/* api callbacks */
	ot->exec= file_next_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}

int file_next_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= CTX_wm_space_file(C);
		if(sfile->params) {
			if (!sfile->folders_next)
			sfile->folders_next = folderlist_new();

		folderlist_pushdir(sfile->folders_prev, sfile->params->dir);
		folderlist_popdir(sfile->folders_next, sfile->params->dir);

		// update folder_prev so we can check for it in folderlist_clear_next()
		folderlist_pushdir(sfile->folders_prev, sfile->params->dir);

		file_change_dir(sfile, 1);
	}		
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);

	return OPERATOR_FINISHED;
}

int file_directory_new_exec(bContext *C, wmOperator *unused)
{
	char tmpstr[FILE_MAX];
	char tmpdir[FILE_MAXFILE];
	int i = 1;

	SpaceFile *sfile= CTX_wm_space_file(C);
	
	if(sfile->params) {
		 
		BLI_strncpy(tmpstr, sfile->params->dir, FILE_MAX);
		BLI_join_dirfile(tmpstr, tmpstr, "New Folder");
		while (BLI_exists(tmpstr)) {
			BLI_snprintf(tmpdir, FILE_MAXFILE, "New Folder(%d)", i++);
			BLI_strncpy(tmpstr, sfile->params->dir, FILE_MAX);
			BLI_join_dirfile(tmpstr, tmpstr, tmpdir);
		}
		BLI_recurdir_fileops(tmpstr);
		if (BLI_exists(tmpstr)) {
			BLI_strncpy(sfile->params->renamefile, tmpdir, FILE_MAXFILE);
		} else {
			filelist_free(sfile->files);
			filelist_parent(sfile->files);
			BLI_strncpy(sfile->params->dir, filelist_dir(sfile->files), FILE_MAX);
		} 
	}		
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);

	return OPERATOR_FINISHED;
}


void FILE_OT_directory_new(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Create New Directory";
	ot->description= "Create a new directory";
	ot->idname= "FILE_OT_directory_new";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= file_directory_new_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}

int file_directory_exec(bContext *C, wmOperator *unused)
{
	char tmpstr[FILE_MAX];

	SpaceFile *sfile= CTX_wm_space_file(C);
	
	if(sfile->params) {
		if ( sfile->params->dir[0] == '~' ) {
			if (sfile->params->dir[1] == '\0') {
				BLI_strncpy(sfile->params->dir, BLI_gethome(), sizeof(sfile->params->dir) );
			} else {
				/* replace ~ with home */
				char homestr[FILE_MAX];
				char *d = &sfile->params->dir[1];

				while ( (*d == '\\') || (*d == '/') )
					d++;
				BLI_strncpy(homestr,  BLI_gethome(), FILE_MAX);
				BLI_join_dirfile(tmpstr, homestr, d);
				BLI_strncpy(sfile->params->dir, tmpstr, sizeof(sfile->params->dir));
			}
		}
#ifdef WIN32
		if (sfile->params->dir[0] == '\0')
			get_default_root(sfile->params->dir);
#endif
		BLI_cleanup_dir(G.sce, sfile->params->dir);
		BLI_add_slash(sfile->params->dir);
		file_change_dir(sfile, 1);

		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);
	}		
	

	return OPERATOR_FINISHED;
}

int file_filename_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= CTX_wm_space_file(C);
	
	if(sfile->params) {
		if (file_select_match(sfile, sfile->params->file))
		{
			sfile->params->file[0] = '\0';
			WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);
		}
	}		

	return OPERATOR_FINISHED;
}


void FILE_OT_refresh(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Refresh Filelist";
	ot->description= "Refresh the file list.";
	ot->idname= "FILE_OT_refresh";
	
	/* api callbacks */
	ot->exec= file_refresh_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}

int file_hidedot_exec(bContext *C, wmOperator *unused)
{
	SpaceFile *sfile= CTX_wm_space_file(C);
	
	if(sfile->params) {
		sfile->params->flag ^= FILE_HIDE_DOT;
		filelist_free(sfile->files);
		sfile->params->active_file = -1;
		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);
	}
	
	return OPERATOR_FINISHED;

}


void FILE_OT_hidedot(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Hide Dot Files";
	ot->description= "Toggle hide hidden dot files.";
	ot->idname= "FILE_OT_hidedot";
	
	/* api callbacks */
	ot->exec= file_hidedot_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}

struct ARegion *file_buttons_region(struct ScrArea *sa)
{
	ARegion *ar, *arnew;
	
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==RGN_TYPE_CHANNELS)
			return ar;

	/* add subdiv level; after header */
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==RGN_TYPE_HEADER)
			break;
	
	/* is error! */
	if(ar==NULL) return NULL;
	
	arnew= MEM_callocN(sizeof(ARegion), "buttons for file panels");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype= RGN_TYPE_CHANNELS;
	arnew->alignment= RGN_ALIGN_LEFT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

int file_bookmark_toggle_exec(bContext *C, wmOperator *unused)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= file_buttons_region(sa);
	
	if(ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void FILE_OT_bookmark_toggle(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Bookmarks";
	ot->description= "Toggle bookmarks display.";
	ot->idname= "FILE_OT_bookmark_toggle";
	
	/* api callbacks */
	ot->exec= file_bookmark_toggle_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */
}


int file_filenum_exec(bContext *C, wmOperator *op)
{
	SpaceFile *sfile= CTX_wm_space_file(C);
	ScrArea *sa= CTX_wm_area(C);
	
	int inc = RNA_int_get(op->ptr, "increment");
	if(sfile->params && (inc != 0)) {
		BLI_newname(sfile->params->file, inc);
		ED_area_tag_redraw(sa);
		// WM_event_add_notifier(C, NC_WINDOW, NULL);
	}
	
	return OPERATOR_FINISHED;

}

void FILE_OT_filenum(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Increment Number in Filename";
	ot->description= "Increment number in filename.";
	ot->idname= "FILE_OT_filenum";
	
	/* api callbacks */
	ot->exec= file_filenum_exec;
	ot->poll= ED_operator_file_active; /* <- important, handler is on window level */

	/* props */
	RNA_def_int(ot->srna, "increment", 1, 0, 100, "Increment", "", 0,100);
}

int file_rename_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	
	if(sfile->params) {
		int idx = sfile->params->active_file;
		int numfiles = filelist_numfiles(sfile->files);
		if ( (0<=idx) && (idx<numfiles) ) {
			struct direntry *file= filelist_file(sfile->files, idx);
			file->flags |= EDITING;
		}
		ED_area_tag_redraw(sa);
	}
	
	return OPERATOR_FINISHED;

}

int file_rename_poll(bContext *C)
{
	int poll = ED_operator_file_active(C);
	SpaceFile *sfile= CTX_wm_space_file(C);

	if (sfile && sfile->params) {
		if (sfile->params->active_file < 0) { 
			poll= 0;
		} else {
			char dir[FILE_MAX], group[FILE_MAX];	
			if (filelist_islibrary(sfile->files, dir, group)) poll= 0;
		}
	}
	else
		poll= 0;
	return poll;
}

void FILE_OT_rename(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Rename File or Directory";
	ot->description= "Rename file or file directory.";
	ot->idname= "FILE_OT_rename";
	
	/* api callbacks */
	ot->exec= file_rename_exec;
	ot->poll= file_rename_poll; 

}

int file_delete_poll(bContext *C)
{
	int poll = ED_operator_file_active(C);
	SpaceFile *sfile= CTX_wm_space_file(C);
	struct direntry* file;

	if (sfile && sfile->params) {
		if (sfile->params->active_file < 0) { 
			poll= 0;
		} else {
			char dir[FILE_MAX], group[FILE_MAX];	
			if (filelist_islibrary(sfile->files, dir, group)) poll= 0;
			file = filelist_file(sfile->files, sfile->params->active_file);
			if (file && S_ISDIR(file->type)) poll= 0;
		}
	}
	else
		poll= 0;
		
	return poll;
}

int file_delete_exec(bContext *C, wmOperator *op)
{
	char str[FILE_MAX];
	SpaceFile *sfile= CTX_wm_space_file(C);
	struct direntry* file;
	
	
	file = filelist_file(sfile->files, sfile->params->active_file);
	BLI_make_file_string(G.sce, str, sfile->params->dir, file->relname);
	BLI_delete(str, 0, 0);	
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_FILE_LIST, NULL);
	
	return OPERATOR_FINISHED;

}

void FILE_OT_delete(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete File";
	ot->description= "Delete selected file.";
	ot->idname= "FILE_OT_delete";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= file_delete_exec;
	ot->poll= file_delete_poll; /* <- important, handler is on window level */
}

