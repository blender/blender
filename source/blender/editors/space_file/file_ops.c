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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Andrea Weikert (c) 2008 Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_file/file_ops.c
 *  \ingroup spfile
 */

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_fileops_types.h"

#include "BLO_readfile.h"

#include "BKE_appdir.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"
#include "BKE_report.h"
#include "BKE_main.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "ED_screen.h"
#include "ED_fileselect.h"

#include "UI_interface.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "file_intern.h"
#include "filelist.h"
#include "fsmenu.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ---------- FILE SELECTION ------------ */
static FileSelection find_file_mouse_rect(SpaceFile *sfile, ARegion *ar, const rcti *rect_region)
{
	FileSelection sel;
	
	View2D *v2d = &ar->v2d;
	rcti rect_view;
	rctf rect_view_fl;
	rctf rect_region_fl;

	BLI_rctf_rcti_copy(&rect_region_fl, rect_region);

	UI_view2d_region_to_view_rctf(v2d, &rect_region_fl, &rect_view_fl);

	BLI_rcti_init(&rect_view,
	              (int)(v2d->tot.xmin + rect_view_fl.xmin),
	              (int)(v2d->tot.xmin + rect_view_fl.xmax),
	              (int)(v2d->tot.ymax - rect_view_fl.ymin),
	              (int)(v2d->tot.ymax - rect_view_fl.ymax));

	sel  = ED_fileselect_layout_offset_rect(sfile->layout, &rect_view);
	
	return sel;
}

static void file_deselect_all(SpaceFile *sfile, unsigned int flag)
{
	FileSelection sel;
	sel.first = 0;
	sel.last = filelist_numfiles(sfile->files) - 1;
	
	filelist_select(sfile->files, &sel, FILE_SEL_REMOVE, flag, CHECK_ALL);
}

typedef enum FileSelect { 
	FILE_SELECT_NOTHING = 0,
	FILE_SELECT_DIR = 1, 
	FILE_SELECT_FILE = 2 
} FileSelect;

static void clamp_to_filelist(int numfiles, FileSelection *sel)
{
	/* border select before the first file */
	if ( (sel->first < 0) && (sel->last >= 0) ) {
		sel->first = 0;
	}
	/* don't select if everything is outside filelist */
	if ( (sel->first >= numfiles) && ((sel->last < 0) || (sel->last >= numfiles)) ) {
		sel->first = -1;
		sel->last = -1;
	}
	
	/* fix if last file invalid */
	if ( (sel->first > 0) && (sel->last < 0) )
		sel->last = numfiles - 1;

	/* clamp */
	if ( (sel->first >= numfiles) ) {
		sel->first = numfiles - 1;
	}
	if ( (sel->last >= numfiles) ) {
		sel->last = numfiles - 1;
	}
}

static FileSelection file_selection_get(bContext *C, const rcti *rect, bool fill)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	int numfiles = filelist_numfiles(sfile->files);
	FileSelection sel;

	sel = find_file_mouse_rect(sfile, ar, rect);
	if (!((sel.first == -1) && (sel.last == -1)) ) {
		clamp_to_filelist(numfiles, &sel);
	}


	/* if desired, fill the selection up from the last selected file to the current one */
	if (fill && (sel.last >= 0) && (sel.last < numfiles) ) {
		int f = sel.last;
		while (f >= 0) {
			if (filelist_is_selected(sfile->files, f, CHECK_ALL) )
				break;
			f--;
		}
		if (f >= 0) {
			sel.first = f + 1;
		}
	}
	return sel;
}

static FileSelect file_select_do(bContext *C, int selected_idx, bool do_diropen)
{
	FileSelect retval = FILE_SELECT_NOTHING;
	SpaceFile *sfile = CTX_wm_space_file(C);
	FileSelectParams *params = ED_fileselect_get_params(sfile);
	int numfiles = filelist_numfiles(sfile->files);
	struct direntry *file;

	/* make the selected file active */
	if ((selected_idx >= 0) &&
	    (selected_idx < numfiles) &&
	    (file = filelist_file(sfile->files, selected_idx)))
	{
		params->active_file = selected_idx;

		if (S_ISDIR(file->type)) {
			if (do_diropen == false) {
				params->file[0] = '\0';
				retval = FILE_SELECT_DIR;
			}
			/* the path is too long and we are not going up! */
			else if (!FILENAME_IS_PARENT(file->relname) && strlen(params->dir) + strlen(file->relname) >= FILE_MAX) {
				// XXX error("Path too long, cannot enter this directory");
			}
			else {
				if (FILENAME_IS_PARENT(file->relname)) {
					/* avoids /../../ */
					BLI_parent_dir(params->dir);
				}
				else {
					BLI_cleanup_dir(G.main->name, params->dir);
					strcat(params->dir, file->relname);
					BLI_add_slash(params->dir);
				}

				file_change_dir(C, 0);
				retval = FILE_SELECT_DIR;
			}
		}
		else {
			if (file->relname) {
				BLI_strncpy(params->file, file->relname, FILE_MAXFILE);
			}
			retval = FILE_SELECT_FILE;
		}
	}
	return retval;
}


static FileSelect file_select(bContext *C, const rcti *rect, FileSelType select, bool fill, bool do_diropen)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	FileSelect retval = FILE_SELECT_NOTHING;
	FileSelection sel = file_selection_get(C, rect, fill); /* get the selection */
	const FileCheckType check_type = (sfile->params->flag & FILE_DIRSEL_ONLY) ? CHECK_DIRS : CHECK_ALL;
	
	/* flag the files as selected in the filelist */
	filelist_select(sfile->files, &sel, select, FILE_SEL_SELECTED, check_type);
	
	/* Don't act on multiple selected files */
	if (sel.first != sel.last) select = 0;

	/* Do we have a valid selection and are we actually selecting */
	if ((sel.last >= 0) && ((select == FILE_SEL_ADD) || (select == FILE_SEL_TOGGLE))) {
		/* Check last selection, if selected, act on the file or dir */
		if (filelist_is_selected(sfile->files, sel.last, check_type)) {
			retval = file_select_do(C, sel.last, do_diropen);
		}
	}

	/* update operator for name change event */
	file_draw_check_cb(C, NULL, NULL);
	
	return retval;
}

static int file_border_select_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	FileSelectParams *params = ED_fileselect_get_params(sfile);
	FileSelection sel;
	rcti rect;

	int result;

	result = WM_border_select_modal(C, op, event);

	if (result == OPERATOR_RUNNING_MODAL) {

		WM_operator_properties_border_to_rcti(op, &rect);

		BLI_rcti_isect(&(ar->v2d.mask), &rect, &rect);

		sel = file_selection_get(C, &rect, 0);
		if ( (sel.first != params->sel_first) || (sel.last != params->sel_last) ) {
			int idx;

			file_deselect_all(sfile, FILE_SEL_HIGHLIGHTED);
			filelist_select(sfile->files, &sel, FILE_SEL_ADD, FILE_SEL_HIGHLIGHTED, CHECK_ALL);
			WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

			/* dont highlight readonly file (".." or ".") on border select */
			for (idx = sel.last; idx >= 0; idx--) {
				struct direntry *file = filelist_file(sfile->files, idx);

				if (FILENAME_IS_CURRPAR(file->relname)) {
					file->selflag &= ~FILE_SEL_HIGHLIGHTED;
				}

				/* active_file gets highlighted as well - make sure it is no readonly file */
				if (sel.last == idx) {
					params->active_file = idx;
				}
			}
		}
		params->sel_first = sel.first; params->sel_last = sel.last;

	}
	else {
		params->active_file = -1;
		params->sel_first = params->sel_last = -1;
		file_deselect_all(sfile, FILE_SEL_HIGHLIGHTED);
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);
	}

	return result;
}

static int file_border_select_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	rcti rect;
	FileSelect ret;
	const bool select = (RNA_int_get(op->ptr, "gesture_mode") == GESTURE_MODAL_SELECT);
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	WM_operator_properties_border_to_rcti(op, &rect);

	if (!extend) {
		SpaceFile *sfile = CTX_wm_space_file(C);

		file_deselect_all(sfile, FILE_SEL_SELECTED);
	}

	BLI_rcti_isect(&(ar->v2d.mask), &rect, &rect);

	ret = file_select(C, &rect, select ? FILE_SEL_ADD : FILE_SEL_REMOVE, false, false);
	if (FILE_SELECT_DIR == ret) {
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
	}
	else if (FILE_SELECT_FILE == ret) {
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);
	}
	return OPERATOR_FINISHED;
}

void FILE_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Activate/Select File";
	ot->description = "Activate/select the file(s) contained in the border";
	ot->idname = "FILE_OT_select_border";
	
	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = file_border_select_exec;
	ot->modal = file_border_select_modal;
	ot->poll = ED_operator_file_active;
	ot->cancel = WM_border_select_cancel;

	/* properties */
	WM_operator_properties_gesture_border(ot, 1);
}

static int file_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	FileSelect ret;
	rcti rect;
	const bool extend = RNA_boolean_get(op->ptr, "extend");
	const bool fill = RNA_boolean_get(op->ptr, "fill");
	const bool do_diropen = RNA_boolean_get(op->ptr, "open");

	if (ar->regiontype != RGN_TYPE_WINDOW)
		return OPERATOR_CANCELLED;

	rect.xmin = rect.xmax = event->mval[0];
	rect.ymin = rect.ymax = event->mval[1];

	if (!BLI_rcti_isect_pt(&ar->v2d.mask, rect.xmin, rect.ymin))
		return OPERATOR_CANCELLED;

	if (sfile && sfile->params) {
		int idx = sfile->params->active_file;

		if (idx >= 0) {
			struct direntry *file = filelist_file(sfile->files, idx);
			if (FILENAME_IS_CURRPAR(file->relname)) {
				/* skip - If a readonly file (".." or ".") is selected, skip deselect all! */
			}
			else {
				/* single select, deselect all selected first */
				if (!extend) file_deselect_all(sfile, FILE_SEL_SELECTED);
			}
		}
	}

	ret = file_select(C, &rect, extend ? FILE_SEL_TOGGLE : FILE_SEL_ADD, fill, do_diropen);
	if (FILE_SELECT_DIR == ret)
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
	else if (FILE_SELECT_FILE == ret)
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

	WM_event_add_mousemove(C); /* for directory changes */
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

	return OPERATOR_FINISHED;
}

void FILE_OT_select(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Activate/Select File";
	ot->description = "Activate/select file";
	ot->idname = "FILE_OT_select";
	
	/* api callbacks */
	ot->invoke = file_select_invoke;
	ot->poll = ED_operator_file_active;

	/* properties */
	prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting everything first");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "fill", false, "Fill", "Select everything beginning with the last selection");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "open", true, "Open", "Open a directory when selecting it");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int file_select_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	FileSelection sel;
	int numfiles = filelist_numfiles(sfile->files);
	int i;
	bool is_selected = false;

	sel.first = 0; 
	sel.last = numfiles - 1;

	/* Is any file selected ? */
	for (i = 0; i < numfiles; ++i) {
		if (filelist_is_selected(sfile->files, i, CHECK_ALL)) {
			is_selected = true;
			break;
		}
	}
	/* select all only if previously no file was selected */
	if (is_selected) {
		filelist_select(sfile->files, &sel, FILE_SEL_REMOVE, FILE_SEL_SELECTED, CHECK_ALL);
	}
	else {
		const FileCheckType check_type = (sfile->params->flag & FILE_DIRSEL_ONLY) ? CHECK_DIRS : CHECK_FILES;
		filelist_select(sfile->files, &sel, FILE_SEL_ADD, FILE_SEL_SELECTED, check_type);
	}
	ED_area_tag_redraw(sa);
	return OPERATOR_FINISHED;
}

void FILE_OT_select_all_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All Files";
	ot->description = "Select or deselect all files";
	ot->idname = "FILE_OT_select_all_toggle";
	
	/* api callbacks */
	ot->exec = file_select_all_exec;
	ot->poll = ED_operator_file_active;

	/* properties */
}

/* ---------- BOOKMARKS ----------- */

static int bookmark_select_exec(bContext *C, wmOperator *op)
{
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (RNA_struct_find_property(op->ptr, "dir")) {
		char entry[256];
		FileSelectParams *params = sfile->params;

		RNA_string_get(op->ptr, "dir", entry);
		BLI_strncpy(params->dir, entry, sizeof(params->dir));
		BLI_cleanup_dir(G.main->name, params->dir);
		file_change_dir(C, 1);

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
	}
	
	return OPERATOR_FINISHED;
}

void FILE_OT_select_bookmark(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Directory";
	ot->description = "Select a bookmarked directory";
	ot->idname = "FILE_OT_select_bookmark";
	
	/* api callbacks */
	ot->exec = bookmark_select_exec;
	ot->poll = ED_operator_file_active;

	/* properties */
	prop = RNA_def_string(ot->srna, "dir", NULL, FILE_MAXDIR, "Dir", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int bookmark_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	struct FSMenu *fsmenu = fsmenu_get();
	struct FileSelectParams *params = ED_fileselect_get_params(sfile);

	if (params->dir[0] != '\0') {
		char name[FILE_MAX];
	
		fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, params->dir, FS_INSERT_SAVE);
		BLI_make_file_string("/", name, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL), BLENDER_BOOKMARK_FILE);
		fsmenu_write_file(fsmenu, name);
	}

	ED_area_tag_redraw(sa);
	return OPERATOR_FINISHED;
}

void FILE_OT_bookmark_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Bookmark";
	ot->description = "Add a bookmark for the selected/active directory";
	ot->idname = "FILE_OT_bookmark_add";
	
	/* api callbacks */
	ot->exec = bookmark_add_exec;
	ot->poll = ED_operator_file_active;
}

static int bookmark_delete_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa = CTX_wm_area(C);
	struct FSMenu *fsmenu = fsmenu_get();
	int nentries = fsmenu_get_nentries(fsmenu, FS_CATEGORY_BOOKMARKS);
	
	if (RNA_struct_find_property(op->ptr, "index")) {
		int index = RNA_int_get(op->ptr, "index");
		if ((index > -1) && (index < nentries)) {
			char name[FILE_MAX];
			
			fsmenu_remove_entry(fsmenu, FS_CATEGORY_BOOKMARKS, index);
			BLI_make_file_string("/", name, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL), BLENDER_BOOKMARK_FILE);
			fsmenu_write_file(fsmenu, name);
			ED_area_tag_redraw(sa);
		}
	}

	return OPERATOR_FINISHED;
}

void FILE_OT_bookmark_delete(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Delete Bookmark";
	ot->description = "Delete selected bookmark";
	ot->idname = "FILE_OT_bookmark_delete";
	
	/* api callbacks */
	ot->exec = bookmark_delete_exec;
	ot->poll = ED_operator_file_active;

	/* properties */
	prop = RNA_def_int(ot->srna, "index", -1, -1, 20000, "Index", "", -1, 20000);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int reset_recent_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	char name[FILE_MAX];
	struct FSMenu *fsmenu = fsmenu_get();
	
	while (fsmenu_get_entry(fsmenu, FS_CATEGORY_RECENT, 0) != NULL) {
		fsmenu_remove_entry(fsmenu, FS_CATEGORY_RECENT, 0);
	}
	BLI_make_file_string("/", name, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL), BLENDER_BOOKMARK_FILE);
	fsmenu_write_file(fsmenu, name);
	ED_area_tag_redraw(sa);
		
	return OPERATOR_FINISHED;
}

void FILE_OT_reset_recent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reset Recent";
	ot->description = "Reset Recent files";
	ot->idname = "FILE_OT_reset_recent";
	
	/* api callbacks */
	ot->exec = reset_recent_exec;
	ot->poll = ED_operator_file_active;

}

int file_highlight_set(SpaceFile *sfile, ARegion *ar, int mx, int my)
{
	View2D *v2d = &ar->v2d;
	FileSelectParams *params;
	int numfiles, origfile;

	if (sfile == NULL || sfile->files == NULL) return 0;

	numfiles = filelist_numfiles(sfile->files);
	params = ED_fileselect_get_params(sfile);

	origfile = params->active_file;

	mx -= ar->winrct.xmin;
	my -= ar->winrct.ymin;

	if (BLI_rcti_isect_pt(&ar->v2d.mask, mx, my)) {
		float fx, fy;
		int active_file;

		UI_view2d_region_to_view(v2d, mx, my, &fx, &fy);

		active_file = ED_fileselect_layout_offset(sfile->layout, (int)(v2d->tot.xmin + fx), (int)(v2d->tot.ymax - fy));

		if ((active_file >= 0) && (active_file < numfiles))
			params->active_file = active_file;
		else
			params->active_file = -1;
	}
	else
		params->active_file = -1;

	return (params->active_file != origfile);
}

static int file_highlight_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (!file_highlight_set(sfile, ar, event->x, event->y))
		return OPERATOR_CANCELLED;

	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

void FILE_OT_highlight(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Highlight File";
	ot->description = "Highlight selected file(s)";
	ot->idname = "FILE_OT_highlight";
	
	/* api callbacks */
	ot->invoke = file_highlight_invoke;
	ot->poll = ED_operator_file_active;
}

int file_cancel_exec(bContext *C, wmOperator *UNUSED(unused))
{
	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	wmOperator *op = sfile->op;
	
	sfile->op = NULL;

	WM_event_fileselect_event(wm, op, EVT_FILESELECT_CANCEL);
	
	return OPERATOR_FINISHED;
}

static int file_operator_poll(bContext *C)
{
	int poll = ED_operator_file_active(C);
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (!sfile || !sfile->op) poll = 0;

	return poll;
}

void FILE_OT_cancel(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Cancel File Load";
	ot->description = "Cancel loading of selected file";
	ot->idname = "FILE_OT_cancel";
	
	/* api callbacks */
	ot->exec = file_cancel_exec;
	ot->poll = file_operator_poll;
}


void file_sfile_to_operator(wmOperator *op, SpaceFile *sfile, char *filepath)
{
	BLI_join_dirfile(filepath, FILE_MAX, sfile->params->dir, sfile->params->file); /* XXX, not real length */
	if (RNA_struct_find_property(op->ptr, "relative_path")) {
		if (RNA_boolean_get(op->ptr, "relative_path")) {
			BLI_path_rel(filepath, G.main->name);
		}
	}

	if (RNA_struct_find_property(op->ptr, "filename")) {
		RNA_string_set(op->ptr, "filename", sfile->params->file);
	}
	if (RNA_struct_find_property(op->ptr, "directory")) {
		RNA_string_set(op->ptr, "directory", sfile->params->dir);
	}
	if (RNA_struct_find_property(op->ptr, "filepath")) {
		RNA_string_set(op->ptr, "filepath", filepath);
	}
	
	/* some ops have multiple files to select */
	/* this is called on operators check() so clear collections first since
	 * they may be already set. */
	{
		PointerRNA itemptr;
		PropertyRNA *prop_files = RNA_struct_find_property(op->ptr, "files");
		PropertyRNA *prop_dirs = RNA_struct_find_property(op->ptr, "dirs");
		int i, numfiles = filelist_numfiles(sfile->files);

		if (prop_files) {
			int num_files = 0;
			RNA_property_collection_clear(op->ptr, prop_files);
			for (i = 0; i < numfiles; i++) {
				if (filelist_is_selected(sfile->files, i, CHECK_FILES)) {
					struct direntry *file = filelist_file(sfile->files, i);
					RNA_property_collection_add(op->ptr, prop_files, &itemptr);
					RNA_string_set(&itemptr, "name", file->relname);
					num_files++;
				}
			}
			/* make sure the file specified in the filename button is added even if no files selected */
			if (0 == num_files) {
				RNA_property_collection_add(op->ptr, prop_files, &itemptr);
				RNA_string_set(&itemptr, "name", sfile->params->file);
			}
		}

		if (prop_dirs) {
			int num_dirs = 0;
			RNA_property_collection_clear(op->ptr, prop_dirs);
			for (i = 0; i < numfiles; i++) {
				if (filelist_is_selected(sfile->files, i, CHECK_DIRS)) {
					struct direntry *file = filelist_file(sfile->files, i);
					RNA_property_collection_add(op->ptr, prop_dirs, &itemptr);
					RNA_string_set(&itemptr, "name", file->relname);
					num_dirs++;
				}
			}
			
			/* make sure the directory specified in the button is added even if no directory selected */
			if (0 == num_dirs) {
				RNA_property_collection_add(op->ptr, prop_dirs, &itemptr);
				RNA_string_set(&itemptr, "name", sfile->params->dir);
			}
		}


	}
}

void file_operator_to_sfile(SpaceFile *sfile, wmOperator *op)
{
	PropertyRNA *prop;

	/* If neither of the above are set, split the filepath back */
	if ((prop = RNA_struct_find_property(op->ptr, "filepath"))) {
		char filepath[FILE_MAX];
		RNA_property_string_get(op->ptr, prop, filepath);
		BLI_split_dirfile(filepath, sfile->params->dir, sfile->params->file, sizeof(sfile->params->dir), sizeof(sfile->params->file));
	}
	else {
		if ((prop = RNA_struct_find_property(op->ptr, "filename"))) {
			RNA_property_string_get(op->ptr, prop, sfile->params->file);
		}
		if ((prop = RNA_struct_find_property(op->ptr, "directory"))) {
			RNA_property_string_get(op->ptr, prop, sfile->params->dir);
		}
	}
	
	/* we could check for relative_path property which is used when converting
	 * in the other direction but doesnt hurt to do this every time */
	BLI_path_abs(sfile->params->dir, G.main->name);

	/* XXX, files and dirs updates missing, not really so important though */
}

void file_draw_check_cb(bContext *C, void *UNUSED(arg1), void *UNUSED(arg2))
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	wmOperator *op = sfile->op;
	if (op) { /* fail on reload */
		if (op->type->check) {
			char filepath[FILE_MAX];
			file_sfile_to_operator(op, sfile, filepath);
			
			/* redraw */
			if (op->type->check(C, op)) {
				file_operator_to_sfile(sfile, op);
	
				/* redraw, else the changed settings wont get updated */
				ED_area_tag_redraw(CTX_wm_area(C));
			}
		}
	}
}

bool file_draw_check_exists(SpaceFile *sfile)
{
	if (sfile->op) { /* fails on reload */
		if (RNA_struct_find_property(sfile->op->ptr, "check_existing")) {
			if (RNA_boolean_get(sfile->op->ptr, "check_existing")) {
				char filepath[FILE_MAX];
				BLI_join_dirfile(filepath, sizeof(filepath), sfile->params->dir, sfile->params->file);
				if (BLI_is_file(filepath)) {
					return true;
				}
			}
		}
	}

	return false;
}

/* sends events now, so things get handled on windowqueue level */
int file_exec(bContext *C, wmOperator *exec_op)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	char filepath[FILE_MAX];
	
	if (sfile->op) {
		wmOperator *op = sfile->op;
	
		/* when used as a macro, for doubleclick, 
		 * to prevent closing when doubleclicking on .. item */
		if (RNA_boolean_get(exec_op->ptr, "need_active")) {
			int i, active = 0;
			
			for (i = 0; i < filelist_numfiles(sfile->files); i++) {
				if (filelist_is_selected(sfile->files, i, CHECK_ALL)) {
					active = 1;
					break;
				}
			}
			if (active == 0)
				return OPERATOR_CANCELLED;
		}
		
		sfile->op = NULL;

		file_sfile_to_operator(op, sfile, filepath);

		if (BLI_exists(sfile->params->dir)) {
			fsmenu_insert_entry(fsmenu_get(), FS_CATEGORY_RECENT, sfile->params->dir, FS_INSERT_SAVE | FS_INSERT_FIRST);
		}

		BLI_make_file_string(G.main->name, filepath, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL), BLENDER_BOOKMARK_FILE);
		fsmenu_write_file(fsmenu_get(), filepath);
		WM_event_fileselect_event(wm, op, EVT_FILESELECT_EXEC);

	}
				
	return OPERATOR_FINISHED;
}

void FILE_OT_execute(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Execute File Window";
	ot->description = "Execute selected file";
	ot->idname = "FILE_OT_execute";
	
	/* api callbacks */
	ot->exec = file_exec;
	ot->poll = file_operator_poll; 

	/* properties */
	prop = RNA_def_boolean(ot->srna, "need_active", 0, "Need Active",
	                       "Only execute if there's an active selected file in the file list");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}


int file_parent_exec(bContext *C, wmOperator *UNUSED(unused))
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	
	if (sfile->params) {
		if (BLI_parent_dir(sfile->params->dir)) {
			BLI_cleanup_dir(G.main->name, sfile->params->dir);
			/* if not browsing in .blend file, we still want to check whether the path is a directory */
			if (sfile->params->type == FILE_LOADLIB) {
				char tdir[FILE_MAX], tgroup[FILE_MAX];
				if (BLO_is_a_library(sfile->params->dir, tdir, tgroup)) {
					file_change_dir(C, 0);
				}
				else {
					file_change_dir(C, 1);
				}
			}
			else {
				file_change_dir(C, 1);
			}
			WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
		}
	}
	
	return OPERATOR_FINISHED;

}


void FILE_OT_parent(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Parent File";
	ot->description = "Move to parent directory";
	ot->idname = "FILE_OT_parent";
	
	/* api callbacks */
	ot->exec = file_parent_exec;
	ot->poll = ED_operator_file_active; /* <- important, handler is on window level */
}


static int file_refresh_exec(bContext *C, wmOperator *UNUSED(unused))
{
	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	struct FSMenu *fsmenu = fsmenu_get();

	ED_fileselect_clear(wm, sfile);

	/* refresh system directory menu */
	fsmenu_refresh_system_category(fsmenu);

	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);

	return OPERATOR_FINISHED;

}

void FILE_OT_previous(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Previous Folder";
	ot->description = "Move to previous folder";
	ot->idname = "FILE_OT_previous";
	
	/* api callbacks */
	ot->exec = file_previous_exec;
	ot->poll = ED_operator_file_active; /* <- important, handler is on window level */
}

int file_previous_exec(bContext *C, wmOperator *UNUSED(unused))
{
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile->params) {
		if (!sfile->folders_next)
			sfile->folders_next = folderlist_new();

		folderlist_pushdir(sfile->folders_next, sfile->params->dir);
		folderlist_popdir(sfile->folders_prev, sfile->params->dir);
		folderlist_pushdir(sfile->folders_next, sfile->params->dir);

		file_change_dir(C, 1);
	}
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);

	return OPERATOR_FINISHED;
}

void FILE_OT_next(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Next Folder";
	ot->description = "Move to next folder";
	ot->idname = "FILE_OT_next";
	
	/* api callbacks */
	ot->exec = file_next_exec;
	ot->poll = ED_operator_file_active; /* <- important, handler is on window level */
}

int file_next_exec(bContext *C, wmOperator *UNUSED(unused))
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	if (sfile->params) {
		if (!sfile->folders_next)
			sfile->folders_next = folderlist_new();

		folderlist_pushdir(sfile->folders_prev, sfile->params->dir);
		folderlist_popdir(sfile->folders_next, sfile->params->dir);

		// update folders_prev so we can check for it in folderlist_clear_next()
		folderlist_pushdir(sfile->folders_prev, sfile->params->dir);

		file_change_dir(C, 1);
	}
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);

	return OPERATOR_FINISHED;
}


/* only meant for timer usage */
static int file_smoothscroll_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	ScrArea *sa = CTX_wm_area(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	ARegion *ar, *oldar = CTX_wm_region(C);
	int offset;
	int numfiles, numfiles_layout;
	int edit_idx = 0;
	int i;

	/* escape if not our timer */
	if (sfile->smoothscroll_timer == NULL || sfile->smoothscroll_timer != event->customdata)
		return OPERATOR_PASS_THROUGH;
	
	numfiles = filelist_numfiles(sfile->files);

	/* check if we are editing a name */
	for (i = 0; i < numfiles; ++i) {
		if (filelist_is_selected(sfile->files, i, CHECK_ALL) ) {
			edit_idx = i;
			break;
		}
	}

	/* if we are not editing, we are done */
	if (0 == edit_idx) {
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), sfile->smoothscroll_timer);
		sfile->smoothscroll_timer = NULL;
		return OPERATOR_PASS_THROUGH;
	}

	/* we need the correct area for scrolling */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
	if (!ar || ar->regiontype != RGN_TYPE_WINDOW) {
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), sfile->smoothscroll_timer);
		sfile->smoothscroll_timer = NULL;
		return OPERATOR_PASS_THROUGH;
	}

	offset = ED_fileselect_layout_offset(sfile->layout, (int)ar->v2d.cur.xmin, (int)-ar->v2d.cur.ymax);
	if (offset < 0) offset = 0;

	/* scroll offset is the first file in the row/column we are editing in */
	if (sfile->scroll_offset == 0) {
		if (sfile->layout->flag & FILE_LAYOUT_HOR) {
			sfile->scroll_offset = (edit_idx / sfile->layout->rows) * sfile->layout->rows;
			if (sfile->scroll_offset <= offset) sfile->scroll_offset -= sfile->layout->rows;
		}
		else {
			sfile->scroll_offset = (edit_idx / sfile->layout->columns) * sfile->layout->columns;
			if (sfile->scroll_offset <= offset) sfile->scroll_offset -= sfile->layout->columns;
		}
	}
	
	numfiles_layout = ED_fileselect_layout_numfiles(sfile->layout, ar);
	
	/* check if we have reached our final scroll position */
	if ( (sfile->scroll_offset >= offset) && (sfile->scroll_offset < offset + numfiles_layout) ) {
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), sfile->smoothscroll_timer);
		sfile->smoothscroll_timer = NULL;
		return OPERATOR_FINISHED;
	}

	/* temporarily set context to the main window region, 
	 * so the scroll operators work */
	CTX_wm_region_set(C, ar);
	
	/* scroll one step in the desired direction */
	if (sfile->scroll_offset < offset) {
		if (sfile->layout->flag & FILE_LAYOUT_HOR) {
			WM_operator_name_call(C, "VIEW2D_OT_scroll_left", 0, NULL);
		}
		else {
			WM_operator_name_call(C, "VIEW2D_OT_scroll_up", 0, NULL);
		}
		
	}
	else {
		if (sfile->layout->flag & FILE_LAYOUT_HOR) {
			WM_operator_name_call(C, "VIEW2D_OT_scroll_right", 0, NULL);
		}
		else {
			WM_operator_name_call(C, "VIEW2D_OT_scroll_down", 0, NULL);
		}
	}
	
	ED_region_tag_redraw(ar);
	
	/* and restore context */
	CTX_wm_region_set(C, oldar);
	
	return OPERATOR_FINISHED;
}


void FILE_OT_smoothscroll(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Smooth Scroll";
	ot->idname = "FILE_OT_smoothscroll";
	ot->description = "Smooth scroll to make editable file visible";
	
	/* api callbacks */
	ot->invoke = file_smoothscroll_invoke;
	
	ot->poll = ED_operator_file_active;
}


/* create a new, non-existing folder name, returns 1 if successful, 0 if name couldn't be created.
 * The actual name is returned in 'name', 'folder' contains the complete path, including the new folder name.
 */
static int new_folder_path(const char *parent, char *folder, char *name)
{
	int i = 1;
	int len = 0;

	BLI_strncpy(name, "New Folder", FILE_MAXFILE);
	BLI_join_dirfile(folder, FILE_MAX, parent, name); /* XXX, not real length */
	/* check whether folder with the name already exists, in this case
	 * add number to the name. Check length of generated name to avoid
	 * crazy case of huge number of folders each named 'New Folder (x)' */
	while (BLI_exists(folder) && (len < FILE_MAXFILE)) {
		len = BLI_snprintf(name, FILE_MAXFILE, "New Folder(%d)", i);
		BLI_join_dirfile(folder, FILE_MAX, parent, name); /* XXX, not real length */
		i++;
	}

	return (len < FILE_MAXFILE);
}

int file_directory_new_exec(bContext *C, wmOperator *op)
{
	char name[FILE_MAXFILE];
	char path[FILE_MAX];
	int generate_name = 1;

	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	
	if (!sfile->params) {
		BKE_report(op->reports, RPT_WARNING, "No parent directory given");
		return OPERATOR_CANCELLED;
	}
	
	path[0] = '\0';

	if (RNA_struct_find_property(op->ptr, "directory")) {
		RNA_string_get(op->ptr, "directory", path);
		if (path[0] != '\0') generate_name = 0;
	}

	if (generate_name) {
		/* create a new, non-existing folder name */
		if (!new_folder_path(sfile->params->dir, path, name)) {
			BKE_report(op->reports, RPT_ERROR, "Could not create new folder name");
			return OPERATOR_CANCELLED;
		}
	}

	/* create the file */
	BLI_dir_create_recursive(path);

	if (!BLI_exists(path)) {
		BKE_report(op->reports, RPT_ERROR, "Could not create new folder");
		return OPERATOR_CANCELLED;
	}

	/* now remember file to jump into editing */
	BLI_strncpy(sfile->params->renamefile, name, FILE_MAXFILE);

	/* set timer to smoothly view newly generated file */
	sfile->smoothscroll_timer = WM_event_add_timer(wm, CTX_wm_window(C), TIMER1, 1.0 / 1000.0);  /* max 30 frs/sec */
	sfile->scroll_offset = 0;

	/* reload dir to make sure we're seeing what's in the directory */
	ED_fileselect_clear(wm, sfile);

	if (RNA_boolean_get(op->ptr, "open")) {
		BLI_strncpy(sfile->params->dir, path, sizeof(sfile->params->dir));
		file_change_dir(C, 1);
	}

	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);

	return OPERATOR_FINISHED;
}


void FILE_OT_directory_new(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Create New Directory";
	ot->description = "Create a new directory";
	ot->idname = "FILE_OT_directory_new";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = file_directory_new_exec;
	ot->poll = ED_operator_file_active; /* <- important, handler is on window level */

	prop = RNA_def_string_dir_path(ot->srna, "directory", NULL, FILE_MAX, "Directory", "Name of new directory");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "open", false, "Open", "Open new directory");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}


static void file_expand_directory(bContext *C)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	
	if (sfile->params) {
		/* TODO, what about // when relbase isn't valid? */
		if (G.relbase_valid && BLI_path_is_rel(sfile->params->dir)) {
			BLI_path_abs(sfile->params->dir, G.main->name);
		}
		else if (sfile->params->dir[0] == '~') {
			char tmpstr[sizeof(sfile->params->dir) - 1];
			BLI_strncpy(tmpstr, sfile->params->dir + 1, sizeof(tmpstr));
			BLI_join_dirfile(sfile->params->dir, sizeof(sfile->params->dir), BKE_appdir_folder_default(), tmpstr);
		}

		else if (sfile->params->dir[0] == '\0')
#ifndef WIN32
		{
			sfile->params->dir[0] = '/';
			sfile->params->dir[1] = '\0';
		}
#else
		{
			get_default_root(sfile->params->dir);
		}
		/* change "C:" --> "C:\", [#28102] */
		else if ((isalpha(sfile->params->dir[0]) &&
		          (sfile->params->dir[1] == ':')) &&
		         (sfile->params->dir[2] == '\0'))
		{
			sfile->params->dir[2] = '\\';
			sfile->params->dir[3] = '\0';
		}
		else if (BLI_path_is_unc(sfile->params->dir)) {
			BLI_cleanup_unc(sfile->params->dir, FILE_MAX_LIBEXTRA);
		}
#endif
	}
}

#if defined(WIN32)
static bool can_create_dir(const char *dir)
{
	/* for UNC paths we need to check whether the parent of the new
	 * directory is a proper directory itself and not a share or the
	 * UNC root (server name) itself. Calling BLI_is_dir does this
	 */
	if (BLI_path_is_unc(dir)) {
		char parent[PATH_MAX];
		BLI_strncpy(parent, dir, PATH_MAX);
		BLI_parent_dir(parent);
		return BLI_is_dir(parent);
	}
	return true;
}
#endif

void file_directory_enter_handle(bContext *C, void *UNUSED(arg_unused), void *UNUSED(arg_but))
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	
	if (sfile->params) {
		file_expand_directory(C);

		/* special case, user may have pasted a filepath into the directory */
		if (BLI_is_file(sfile->params->dir)) {
			char path[sizeof(sfile->params->dir)];
			BLI_strncpy(path, sfile->params->dir, sizeof(path));
			BLI_split_dirfile(path, sfile->params->dir, sfile->params->file, sizeof(sfile->params->dir), sizeof(sfile->params->file));
		}

		BLI_cleanup_dir(G.main->name, sfile->params->dir);

		if (BLI_exists(sfile->params->dir)) {
			/* if directory exists, enter it immediately */
			file_change_dir(C, 1);

			/* don't do for now because it selects entire text instead of
			 * placing cursor at the end */
			/* UI_textbutton_activate_but(C, but); */
		}
#if defined(WIN32)
		else if (!can_create_dir(sfile->params->dir)) {
			const char *lastdir = folderlist_peeklastdir(sfile->folders_prev);
			if (lastdir)
				BLI_strncpy(sfile->params->dir, lastdir, sizeof(sfile->params->dir));
		}
#endif
		else {
			const char *lastdir = folderlist_peeklastdir(sfile->folders_prev);

			/* if not, ask to create it and enter if confirmed */
			wmOperatorType *ot = WM_operatortype_find("FILE_OT_directory_new", false);
			PointerRNA ptr;
			WM_operator_properties_create_ptr(&ptr, ot);
			RNA_string_set(&ptr, "directory", sfile->params->dir);
			RNA_boolean_set(&ptr, "open", true);

			if (lastdir)
				BLI_strncpy(sfile->params->dir, lastdir, sizeof(sfile->params->dir));


			WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &ptr);
			WM_operator_properties_free(&ptr);
		}

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
	}
}

void file_filename_enter_handle(bContext *C, void *UNUSED(arg_unused), void *arg_but)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	uiBut *but = arg_but;
	char matched_file[FILE_MAX];
	char filepath[sizeof(sfile->params->dir)];

	if (sfile->params) {
		int matches;
		matched_file[0] = '\0';
		filepath[0] = '\0';

		file_expand_directory(C);

		matches = file_select_match(sfile, sfile->params->file, matched_file);

		if (matches) {
			/* int i, numfiles = filelist_numfiles(sfile->files); */ /* XXX UNUSED */
			sfile->params->file[0] = '\0';
			/* replace the pattern (or filename that the user typed in, with the first selected file of the match */
			BLI_strncpy(sfile->params->file, matched_file, sizeof(sfile->params->file));
			
			WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);
		}

		if (matches == 1) {

			BLI_join_dirfile(filepath, sizeof(sfile->params->dir), sfile->params->dir, sfile->params->file);

			/* if directory, open it and empty filename field */
			if (BLI_is_dir(filepath)) {
				BLI_cleanup_dir(G.main->name, filepath);
				BLI_strncpy(sfile->params->dir, filepath, sizeof(sfile->params->dir));
				sfile->params->file[0] = '\0';
				file_change_dir(C, 1);
				UI_textbutton_activate_but(C, but);
				WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);
			}
			else if (sfile->params->type == FILE_LOADLIB) {
				char tdir[FILE_MAX], tgroup[FILE_MAX];
				BLI_add_slash(filepath);
				if (BLO_is_a_library(filepath, tdir, tgroup)) {
					BLI_cleanup_dir(G.main->name, filepath);
					BLI_strncpy(sfile->params->dir, filepath, sizeof(sfile->params->dir));
					sfile->params->file[0] = '\0';
					file_change_dir(C, 0);
					UI_textbutton_activate_but(C, but);
					WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
				}
			}
		}
	}
}

void FILE_OT_refresh(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Refresh Filelist";
	ot->description = "Refresh the file list";
	ot->idname = "FILE_OT_refresh";
	
	/* api callbacks */
	ot->exec = file_refresh_exec;
	ot->poll = ED_operator_file_active; /* <- important, handler is on window level */
}

static int file_hidedot_exec(bContext *C, wmOperator *UNUSED(unused))
{
	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	
	if (sfile->params) {
		sfile->params->flag ^= FILE_HIDE_DOT;
		ED_fileselect_clear(wm, sfile);
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
	}
	
	return OPERATOR_FINISHED;
}


void FILE_OT_hidedot(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Hide Dot Files";
	ot->description = "Toggle hide hidden dot files";
	ot->idname = "FILE_OT_hidedot";
	
	/* api callbacks */
	ot->exec = file_hidedot_exec;
	ot->poll = ED_operator_file_active; /* <- important, handler is on window level */
}

ARegion *file_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;
	
	for (ar = sa->regionbase.first; ar; ar = ar->next)
		if (ar->regiontype == RGN_TYPE_CHANNELS)
			return ar;

	/* add subdiv level; after header */
	for (ar = sa->regionbase.first; ar; ar = ar->next)
		if (ar->regiontype == RGN_TYPE_HEADER)
			break;
	
	/* is error! */
	if (ar == NULL) return NULL;
	
	arnew = MEM_callocN(sizeof(ARegion), "buttons for file panels");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_CHANNELS;
	arnew->alignment = RGN_ALIGN_LEFT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

static int file_bookmark_toggle_exec(bContext *C, wmOperator *UNUSED(unused))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = file_buttons_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void FILE_OT_bookmark_toggle(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Bookmarks";
	ot->description = "Toggle bookmarks display";
	ot->idname = "FILE_OT_bookmark_toggle";
	
	/* api callbacks */
	ot->exec = file_bookmark_toggle_exec;
	ot->poll = ED_operator_file_active; /* <- important, handler is on window level */
}


static int file_filenum_exec(bContext *C, wmOperator *op)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	ScrArea *sa = CTX_wm_area(C);
	
	int inc = RNA_int_get(op->ptr, "increment");
	if (sfile->params && (inc != 0)) {
		BLI_newname(sfile->params->file, inc);
		ED_area_tag_redraw(sa);
		file_draw_check_cb(C, NULL, NULL);
		// WM_event_add_notifier(C, NC_WINDOW, NULL);
	}
	
	return OPERATOR_FINISHED;

}

void FILE_OT_filenum(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Increment Number in Filename";
	ot->description = "Increment number in filename";
	ot->idname = "FILE_OT_filenum";
	
	/* api callbacks */
	ot->exec = file_filenum_exec;
	ot->poll = ED_operator_file_active; /* <- important, handler is on window level */

	/* props */
	RNA_def_int(ot->srna, "increment", 1, -100, 100, "Increment", "", -100, 100);
}

static int file_rename_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	SpaceFile *sfile = (SpaceFile *)CTX_wm_space_data(C);
	
	if (sfile->params) {
		int idx = sfile->params->active_file;
		int numfiles = filelist_numfiles(sfile->files);
		if ( (0 <= idx) && (idx < numfiles) ) {
			struct direntry *file = filelist_file(sfile->files, idx);
			filelist_select_file(sfile->files, idx, FILE_SEL_ADD, FILE_SEL_EDITING, CHECK_ALL);
			BLI_strncpy(sfile->params->renameedit, file->relname, FILE_MAXFILE);
			sfile->params->renamefile[0] = '\0';
		}
		ED_area_tag_redraw(sa);
	}
	
	return OPERATOR_FINISHED;

}

static int file_rename_poll(bContext *C)
{
	int poll = ED_operator_file_active(C);
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile && sfile->params) {
		int idx = sfile->params->active_file;

		if (idx >= 0) {
			struct direntry *file = filelist_file(sfile->files, idx);
			if (FILENAME_IS_CURRPAR(file->relname)) {
				poll = 0;
			}
		}

		if (sfile->params->active_file < 0) {
			poll = 0;
		}
		else {
			char dir[FILE_MAX], group[FILE_MAX];
			if (filelist_islibrary(sfile->files, dir, group)) poll = 0;
		}
	}
	else
		poll = 0;
	return poll;
}

void FILE_OT_rename(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rename File or Directory";
	ot->description = "Rename file or file directory";
	ot->idname = "FILE_OT_rename";
	
	/* api callbacks */
	ot->exec = file_rename_exec;
	ot->poll = file_rename_poll; 

}

static int file_delete_poll(bContext *C)
{
	int poll = ED_operator_file_active(C);
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile && sfile->params) {
		char dir[FILE_MAX], group[FILE_MAX];
		int numfiles = filelist_numfiles(sfile->files);
		int i;
		int num_selected = 0;

		if (filelist_islibrary(sfile->files, dir, group)) poll = 0;
		for (i = 0; i < numfiles; i++) {
			if (filelist_is_selected(sfile->files, i, CHECK_FILES)) {
				num_selected++;
			}
		}
		if (num_selected <= 0) {
			poll = 0;
		}
	}
	else
		poll = 0;
		
	return poll;
}

int file_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	char str[FILE_MAX];
	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	struct direntry *file;	
	int numfiles = filelist_numfiles(sfile->files);
	int i;

	for (i = 0; i < numfiles; i++) {
		if (filelist_is_selected(sfile->files, i, CHECK_FILES)) {
			file = filelist_file(sfile->files, i);
			BLI_make_file_string(G.main->name, str, sfile->params->dir, file->relname);
			BLI_delete(str, false, false);
		}
	}
	
	ED_fileselect_clear(wm, sfile);
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
	
	return OPERATOR_FINISHED;

}

void FILE_OT_delete(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Selected Files";
	ot->description = "Delete selected files";
	ot->idname = "FILE_OT_delete";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = file_delete_exec;
	ot->poll = file_delete_poll; /* <- important, handler is on window level */
}


void ED_operatormacros_file(void)
{
//	wmOperatorType *ot;
//	wmOperatorTypeMacro *otmacro;
	
	/* future macros */
}
