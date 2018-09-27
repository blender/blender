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
#include "BLI_linklist.h"

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
#include <errno.h>

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
	sel.last = filelist_files_ensure(sfile->files) - 1;

	filelist_entries_select_index_range_set(sfile->files, &sel, FILE_SEL_REMOVE, flag, CHECK_ALL);
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
	int numfiles = filelist_files_ensure(sfile->files);
	FileSelection sel;

	sel = find_file_mouse_rect(sfile, ar, rect);
	if (!((sel.first == -1) && (sel.last == -1)) ) {
		clamp_to_filelist(numfiles, &sel);
	}


	/* if desired, fill the selection up from the last selected file to the current one */
	if (fill && (sel.last >= 0) && (sel.last < numfiles) ) {
		int f;
		/* Try to find a smaller-index selected item. */
		for (f = sel.last; f >= 0; f--) {
			if (filelist_entry_select_index_get(sfile->files, f, CHECK_ALL) )
				break;
		}
		if (f >= 0) {
			sel.first = f + 1;
		}
		/* If none found, try to find a higher-index selected item. */
		else {
			for (f = sel.first; f < numfiles; f++) {
				if (filelist_entry_select_index_get(sfile->files, f, CHECK_ALL) )
					break;
			}
			if (f < numfiles) {
				sel.last = f - 1;
			}
		}
	}
	return sel;
}

static FileSelect file_select_do(bContext *C, int selected_idx, bool do_diropen)
{
	Main *bmain = CTX_data_main(C);
	FileSelect retval = FILE_SELECT_NOTHING;
	SpaceFile *sfile = CTX_wm_space_file(C);
	FileSelectParams *params = ED_fileselect_get_params(sfile);
	int numfiles = filelist_files_ensure(sfile->files);
	const FileDirEntry *file;

	/* make the selected file active */
	if ((selected_idx >= 0) &&
	    (selected_idx < numfiles) &&
	    (file = filelist_file(sfile->files, selected_idx)))
	{
		params->highlight_file = selected_idx;
		params->active_file = selected_idx;

		if (file->typeflag & FILE_TYPE_DIR) {
			const bool is_parent_dir = FILENAME_IS_PARENT(file->relpath);

			if (do_diropen == false) {
				params->file[0] = '\0';
				retval = FILE_SELECT_DIR;
			}
			/* the path is too long and we are not going up! */
			else if (!is_parent_dir && strlen(params->dir) + strlen(file->relpath) >= FILE_MAX) {
				// XXX error("Path too long, cannot enter this directory");
			}
			else {
				if (is_parent_dir) {
					/* avoids /../../ */
					BLI_parent_dir(params->dir);

					if (params->recursion_level > 1) {
						/* Disable 'dirtree' recursion when going up in tree. */
						params->recursion_level = 0;
						filelist_setrecursion(sfile->files, params->recursion_level);
					}
				}
				else {
					BLI_cleanup_dir(BKE_main_blendfile_path(bmain), params->dir);
					strcat(params->dir, file->relpath);
					BLI_add_slash(params->dir);
				}

				ED_file_change_dir(C);
				retval = FILE_SELECT_DIR;
			}
		}
		else {
			retval = FILE_SELECT_FILE;
		}
		fileselect_file_set(sfile, selected_idx);
	}
	return retval;
}

/**
 * \warning: loops over all files so better use cautiously
 */
static bool file_is_any_selected(struct FileList *files)
{
	const int numfiles = filelist_files_ensure(files);
	int i;

	/* Is any file selected ? */
	for (i = 0; i < numfiles; ++i) {
		if (filelist_entry_select_index_get(files, i, CHECK_ALL)) {
			return true;
		}
	}

	return false;
}

/**
 * If \a file is outside viewbounds, this adjusts view to make sure it's inside
 */
static void file_ensure_inside_viewbounds(ARegion *ar, SpaceFile *sfile, const int file)
{
	FileLayout *layout = ED_fileselect_get_layout(sfile, ar);
	rctf *cur = &ar->v2d.cur;
	rcti rect;
	bool changed = true;

	file_tile_boundbox(ar, layout, file, &rect);

	/* down - also use if tile is higher than viewbounds so view is aligned to file name */
	if (cur->ymin > rect.ymin || layout->tile_h > ar->winy) {
		cur->ymin = rect.ymin - (2 * layout->tile_border_y);
		cur->ymax = cur->ymin + ar->winy;
	}
	/* up */
	else if (cur->ymax < rect.ymax) {
		cur->ymax = rect.ymax + layout->tile_border_y;
		cur->ymin = cur->ymax - ar->winy;
	}
	/* left - also use if tile is wider than viewbounds so view is aligned to file name */
	else if (cur->xmin > rect.xmin || layout->tile_w > ar->winx) {
		cur->xmin = rect.xmin - layout->tile_border_x;
		cur->xmax = cur->xmin + ar->winx;
	}
	/* right */
	else if (cur->xmax < rect.xmax) {
		cur->xmax = rect.xmax + (2 * layout->tile_border_x);
		cur->xmin = cur->xmax - ar->winx;
	}
	else {
		BLI_assert(cur->xmin <= rect.xmin && cur->xmax >= rect.xmax &&
		           cur->ymin <= rect.ymin && cur->ymax >= rect.ymax);
		changed = false;
	}

	if (changed) {
		UI_view2d_curRect_validate(&ar->v2d);
	}
}


static FileSelect file_select(bContext *C, const rcti *rect, FileSelType select, bool fill, bool do_diropen)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	FileSelect retval = FILE_SELECT_NOTHING;
	FileSelection sel = file_selection_get(C, rect, fill); /* get the selection */
	const FileCheckType check_type = (sfile->params->flag & FILE_DIRSEL_ONLY) ? CHECK_DIRS : CHECK_ALL;

	/* flag the files as selected in the filelist */
	filelist_entries_select_index_range_set(sfile->files, &sel, select, FILE_SEL_SELECTED, check_type);

	/* Don't act on multiple selected files */
	if (sel.first != sel.last) select = 0;

	/* Do we have a valid selection and are we actually selecting */
	if ((sel.last >= 0) && (select != FILE_SEL_REMOVE)) {
		/* Check last selection, if selected, act on the file or dir */
		if (filelist_entry_select_index_get(sfile->files, sel.last, check_type)) {
			retval = file_select_do(C, sel.last, do_diropen);
		}
	}

	if (select != FILE_SEL_ADD && !file_is_any_selected(sfile->files)) {
		sfile->params->active_file = -1;
	}
	else {
		ARegion *ar = CTX_wm_region(C);
		const FileLayout *layout = ED_fileselect_get_layout(sfile, ar);

		/* Adjust view to display selection. Doing iterations for first and last
		 * selected item makes view showing as much of the selection possible.
		 * Not really useful if tiles are (almost) bigger than viewbounds though. */
		if (((layout->flag & FILE_LAYOUT_HOR) && ar->winx > (1.2f * layout->tile_w)) ||
		    ((layout->flag & FILE_LAYOUT_VER) && ar->winy > (2.0f * layout->tile_h)))
		{
			file_ensure_inside_viewbounds(ar, sfile, sel.last);
			file_ensure_inside_viewbounds(ar, sfile, sel.first);
		}
	}

	/* update operator for name change event */
	file_draw_check(C);

	return retval;
}

static int file_border_select_find_last_selected(
        SpaceFile *sfile, ARegion *ar, const FileSelection *sel,
        const int mouse_xy[2])
{
	FileLayout *layout = ED_fileselect_get_layout(sfile, ar);
	rcti bounds_first, bounds_last;
	int dist_first, dist_last;
	float mouseco_view[2];

	UI_view2d_region_to_view(&ar->v2d, UNPACK2(mouse_xy), &mouseco_view[0], &mouseco_view[1]);

	file_tile_boundbox(ar, layout, sel->first, &bounds_first);
	file_tile_boundbox(ar, layout, sel->last, &bounds_last);

	/* are first and last in the same column (horizontal layout)/row (vertical layout)? */
	if ((layout->flag & FILE_LAYOUT_HOR && bounds_first.xmin == bounds_last.xmin) ||
	    (layout->flag & FILE_LAYOUT_VER && bounds_first.ymin != bounds_last.ymin))
	{
		/* use vertical distance */
		const int my_loc = (int)mouseco_view[1];
		dist_first = BLI_rcti_length_y(&bounds_first, my_loc);
		dist_last = BLI_rcti_length_y(&bounds_last, my_loc);
	}
	else {
		/* use horizontal distance */
		const int mx_loc = (int)mouseco_view[0];
		dist_first = BLI_rcti_length_x(&bounds_first, mx_loc);
		dist_last = BLI_rcti_length_x(&bounds_last, mx_loc);
	}

	return (dist_first < dist_last) ? sel->first : sel->last;
}

static int file_border_select_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	FileSelectParams *params = ED_fileselect_get_params(sfile);
	FileSelection sel;
	rcti rect;

	int result;

	result = WM_gesture_border_modal(C, op, event);

	if (result == OPERATOR_RUNNING_MODAL) {
		WM_operator_properties_border_to_rcti(op, &rect);

		BLI_rcti_isect(&(ar->v2d.mask), &rect, &rect);

		sel = file_selection_get(C, &rect, 0);
		if ((sel.first != params->sel_first) || (sel.last != params->sel_last)) {
			int idx;

			file_deselect_all(sfile, FILE_SEL_HIGHLIGHTED);
			filelist_entries_select_index_range_set(sfile->files, &sel, FILE_SEL_ADD, FILE_SEL_HIGHLIGHTED, CHECK_ALL);
			WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

			for (idx = sel.last; idx >= 0; idx--) {
				const FileDirEntry *file = filelist_file(sfile->files, idx);

				/* dont highlight readonly file (".." or ".") on border select */
				if (FILENAME_IS_CURRPAR(file->relpath)) {
					filelist_entry_select_set(sfile->files, file, FILE_SEL_REMOVE, FILE_SEL_HIGHLIGHTED, CHECK_ALL);
				}

				/* make sure highlight_file is no readonly file */
				if (sel.last == idx) {
					params->highlight_file = idx;
				}
			}
		}
		params->sel_first = sel.first; params->sel_last = sel.last;
		params->active_file = file_border_select_find_last_selected(sfile, ar, &sel, event->mval);
	}
	else {
		params->highlight_file = -1;
		params->sel_first = params->sel_last = -1;
		fileselect_file_set(sfile, params->active_file);
		file_deselect_all(sfile, FILE_SEL_HIGHLIGHTED);
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);
	}

	return result;
}

static int file_border_select_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	rcti rect;
	FileSelect ret;
	const bool select = !RNA_boolean_get(op->ptr, "deselect");
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	WM_operator_properties_border_to_rcti(op, &rect);

	if (!extend) {
		file_deselect_all(sfile, FILE_SEL_SELECTED);
	}

	BLI_rcti_isect(&(ar->v2d.mask), &rect, &rect);

	ret = file_select(C, &rect, select ? FILE_SEL_ADD : FILE_SEL_REMOVE, false, false);

	/* unselect '..' parent entry - it's not supposed to be selected if more than one file is selected */
	filelist_entry_select_index_set(sfile->files, 0, FILE_SEL_REMOVE, FILE_SEL_SELECTED, CHECK_ALL);

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
	ot->invoke = WM_gesture_border_invoke;
	ot->exec = file_border_select_exec;
	ot->modal = file_border_select_modal;
	ot->poll = ED_operator_file_active;
	ot->cancel = WM_gesture_border_cancel;

	/* properties */
	WM_operator_properties_gesture_border_select(ot);
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
		int idx = sfile->params->highlight_file;
		int numfiles = filelist_files_ensure(sfile->files);

		if ((idx >= 0) && (idx < numfiles)) {
			/* single select, deselect all selected first */
			if (!extend) {
				file_deselect_all(sfile, FILE_SEL_SELECTED);
			}
		}
	}

	ret = file_select(C, &rect, extend ? FILE_SEL_TOGGLE : FILE_SEL_ADD, fill, do_diropen);

	if (extend) {
		/* unselect '..' parent entry - it's not supposed to be selected if more than one file is selected */
		filelist_entry_select_index_set(sfile->files, 0, FILE_SEL_REMOVE, FILE_SEL_SELECTED, CHECK_ALL);
	}

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

/**
 * \returns true if selection has changed
 */
static bool file_walk_select_selection_set(
        bContext *C, SpaceFile *sfile,
        const int direction, const int numfiles,
        const int active_old, const int active_new, const int other_site,
        const bool has_selection, const bool extend, const bool fill)
{
	FileSelectParams *params = sfile->params;
	struct FileList *files = sfile->files;
	const int last_sel = params->active_file; /* store old value */
	int active = active_old; /* could use active_old instead, just for readability */
	bool deselect = false;

	BLI_assert(params);

	if (has_selection) {
		if (extend &&
		    filelist_entry_select_index_get(files, active_old, CHECK_ALL) &&
		    filelist_entry_select_index_get(files, active_new, CHECK_ALL))
		{
			/* conditions for deselecting: initial file is selected, new file is
			 * selected and either other_side isn't selected/found or we use fill */
			deselect = (fill || other_site == -1 ||
			            !filelist_entry_select_index_get(files, other_site, CHECK_ALL));

			/* don't change highlight_file here since we either want to deselect active or we want to
			 * walk through a block of selected files without selecting/deselecting anything */
			params->active_file = active_new;
			/* but we want to change active if we use fill (needed to get correct selection bounds) */
			if (deselect && fill) {
				active = active_new;
			}
		}
		else {
			/* regular selection change */
			params->active_file = active = active_new;
		}
	}
	else {
		/* select last file */
		if (ELEM(direction, FILE_SELECT_WALK_UP, FILE_SELECT_WALK_LEFT)) {
			params->active_file = active = numfiles - 1;
		}
		/* select first file */
		else if (ELEM(direction, FILE_SELECT_WALK_DOWN, FILE_SELECT_WALK_RIGHT)) {
			params->active_file = active = extend ? 1 : 0;
		}
		else {
			BLI_assert(0);
		}
	}

	if (active < 0) {
		return false;
	}

	if (extend) {
		/* highlight the active walker file for extended selection for better visual feedback */
		params->highlight_file = params->active_file;

		/* unselect '..' parent entry - it's not supposed to be selected if more than one file is selected */
		filelist_entry_select_index_set(files, 0, FILE_SEL_REMOVE, FILE_SEL_SELECTED, CHECK_ALL);
	}
	else {
		/* deselect all first */
		file_deselect_all(sfile, FILE_SEL_SELECTED);

		/* highlight file under mouse pos */
		params->highlight_file = -1;
		WM_event_add_mousemove(C);
	}

	/* do the actual selection */
	if (fill) {
		FileSelection sel = { MIN2(active, last_sel), MAX2(active, last_sel) };

		/* clamping selection to not include '..' parent entry */
		if (sel.first == 0) {
			sel.first = 1;
		}

		/* fill selection between last and first selected file */
		filelist_entries_select_index_range_set(
		            files, &sel, deselect ? FILE_SEL_REMOVE : FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);
		/* entire sel is cleared here, so select active again */
		if (deselect) {
			filelist_entry_select_index_set(files, active, FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);
		}
	}
	else {
		filelist_entry_select_index_set(
		            files, active, deselect ? FILE_SEL_REMOVE : FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);
	}

	BLI_assert(IN_RANGE(active, -1, numfiles));
	fileselect_file_set(sfile, params->active_file);

	/* ensure newly selected file is inside viewbounds */
	file_ensure_inside_viewbounds(CTX_wm_region(C), sfile, params->active_file);

	/* selection changed */
	return true;
}

/**
 * \returns true if selection has changed
 */
static bool file_walk_select_do(
        bContext *C, SpaceFile *sfile,
        FileSelectParams *params, const int direction,
        const bool extend, const bool fill)
{
	struct FileList *files = sfile->files;
	const int numfiles = filelist_files_ensure(files);
	const bool has_selection = file_is_any_selected(files);
	const int active_old = params->active_file;
	int active_new = -1;
	int other_site = -1; /* file on the other site of active_old */


	/* *** get all needed files for handling selection *** */

	if (has_selection) {
		ARegion *ar = CTX_wm_region(C);
		FileLayout *layout = ED_fileselect_get_layout(sfile, ar);
		const int idx_shift = (layout->flag & FILE_LAYOUT_HOR) ? layout->rows : layout->columns;

		if ((layout->flag & FILE_LAYOUT_HOR && direction == FILE_SELECT_WALK_UP) ||
		    (layout->flag & FILE_LAYOUT_VER && direction == FILE_SELECT_WALK_LEFT))
		{
			active_new = active_old - 1;
			other_site = active_old + 1;
		}
		else if ((layout->flag & FILE_LAYOUT_HOR && direction == FILE_SELECT_WALK_DOWN) ||
		         (layout->flag & FILE_LAYOUT_VER && direction == FILE_SELECT_WALK_RIGHT))
		{
			active_new = active_old + 1;
			other_site = active_old - 1;
		}
		else if ((layout->flag & FILE_LAYOUT_HOR && direction == FILE_SELECT_WALK_LEFT) ||
		         (layout->flag & FILE_LAYOUT_VER && direction == FILE_SELECT_WALK_UP))
		{
			active_new = active_old - idx_shift;
			other_site = active_old + idx_shift;
		}
		else if ((layout->flag & FILE_LAYOUT_HOR && direction == FILE_SELECT_WALK_RIGHT) ||
		         (layout->flag & FILE_LAYOUT_VER && direction == FILE_SELECT_WALK_DOWN))
		{

			active_new = active_old + idx_shift;
			other_site = active_old - idx_shift;
		}
		else {
			BLI_assert(0);
		}

		if (!IN_RANGE(active_new, 0, numfiles)) {
			if (extend) {
				/* extend to invalid file -> abort */
				return false;
			}
			/* if we don't extend, selecting '..' (index == 0) is allowed so
			 * using key selection to go to parent directory is possible */
			else if (active_new != 0) {
				/* select initial file */
				active_new = active_old;
			}
		}
		if (!IN_RANGE(other_site, 0, numfiles)) {
			other_site = -1;
		}
	}

	return file_walk_select_selection_set(
	            C, sfile, direction, numfiles, active_old, active_new, other_site, has_selection, extend, fill);
}

static int file_walk_select_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	SpaceFile *sfile = (SpaceFile *)CTX_wm_space_data(C);
	FileSelectParams *params = sfile->params;
	const int direction = RNA_enum_get(op->ptr, "direction");
	const bool extend = RNA_boolean_get(op->ptr, "extend");
	const bool fill = RNA_boolean_get(op->ptr, "fill");

	if (file_walk_select_do(C, sfile, params, direction, extend, fill)) {
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void FILE_OT_select_walk(wmOperatorType *ot)
{
	static const EnumPropertyItem direction_items[] = {
		{FILE_SELECT_WALK_UP,    "UP",    0, "Prev",  ""},
		{FILE_SELECT_WALK_DOWN,  "DOWN",  0, "Next",  ""},
		{FILE_SELECT_WALK_LEFT,  "LEFT",  0, "Left",  ""},
		{FILE_SELECT_WALK_RIGHT, "RIGHT", 0, "Right", ""},
		{0, NULL, 0, NULL, NULL}
	};
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Walk Select/Deselect File";
	ot->description = "Select/Deselect files by walking through them";
	ot->idname = "FILE_OT_select_walk";

	/* api callbacks */
	ot->invoke = file_walk_select_invoke;
	ot->poll = ED_operator_file_active;

	/* properties */
	prop = RNA_def_enum(ot->srna, "direction", direction_items, 0, "Walk Direction",
	                    "Select/Deselect file in this direction");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "extend", false, "Extend",
	                       "Extend selection instead of deselecting everything first");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "fill", false, "Fill", "Select everything beginning with the last selection");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int file_select_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	FileSelection sel;
	const int numfiles = filelist_files_ensure(sfile->files);
	const bool has_selection = file_is_any_selected(sfile->files);

	sel.first = 0;
	sel.last = numfiles - 1;

	/* select all only if previously no file was selected */
	if (has_selection) {
		filelist_entries_select_index_range_set(sfile->files, &sel, FILE_SEL_REMOVE, FILE_SEL_SELECTED, CHECK_ALL);
		sfile->params->active_file = -1;
	}
	else {
		const FileCheckType check_type = (sfile->params->flag & FILE_DIRSEL_ONLY) ? CHECK_DIRS : CHECK_FILES;
		int i;

		filelist_entries_select_index_range_set(sfile->files, &sel, FILE_SEL_ADD, FILE_SEL_SELECTED, check_type);

		/* set active_file to first selected */
		for (i = 0; i < numfiles; i++) {
			if (filelist_entry_select_index_get(sfile->files, i, check_type)) {
				sfile->params->active_file = i;
				break;
			}
		}
	}

	file_draw_check(C);
	WM_event_add_mousemove(C);
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

/* Note we could get rid of this one, but it's used by some addon so... Does not hurt keeping it around for now. */
static int bookmark_select_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	PropertyRNA *prop;

	if ((prop = RNA_struct_find_property(op->ptr, "dir"))) {
		char entry[256];
		FileSelectParams *params = sfile->params;

		RNA_property_string_get(op->ptr, prop, entry);
		BLI_strncpy(params->dir, entry, sizeof(params->dir));
		BLI_cleanup_dir(BKE_main_blendfile_path(bmain), params->dir);
		ED_file_change_dir(C);

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
	struct FSMenu *fsmenu = ED_fsmenu_get();
	struct FileSelectParams *params = ED_fileselect_get_params(sfile);

	if (params->dir[0] != '\0') {
		char name[FILE_MAX];

		fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, params->dir, NULL, FS_INSERT_SAVE);
		BLI_make_file_string("/", name, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL), BLENDER_BOOKMARK_FILE);
		fsmenu_write_file(fsmenu, name);
	}

	ED_area_tag_refresh(sa);
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
	SpaceFile *sfile = CTX_wm_space_file(C);
	struct FSMenu *fsmenu = ED_fsmenu_get();
	int nentries = ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_BOOKMARKS);

	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "index");

	if (prop) {
		int index;
		if (RNA_property_is_set(op->ptr, prop)) {
			index = RNA_property_int_get(op->ptr, prop);
		}
		else {  /* if index unset, use active bookmark... */
			index = sfile->bookmarknr;
		}
		if ((index > -1) && (index < nentries)) {
			char name[FILE_MAX];

			fsmenu_remove_entry(fsmenu, FS_CATEGORY_BOOKMARKS, index);
			BLI_make_file_string("/", name, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL), BLENDER_BOOKMARK_FILE);
			fsmenu_write_file(fsmenu, name);
			ED_area_tag_refresh(sa);
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

static int bookmark_cleanup_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	struct FSMenu *fsmenu = ED_fsmenu_get();
	struct FSMenuEntry *fsme_next, *fsme = ED_fsmenu_get_category(fsmenu, FS_CATEGORY_BOOKMARKS);
	int index;
	bool changed = false;

	for (index = 0; fsme; fsme = fsme_next) {
		fsme_next = fsme->next;

		if (!BLI_is_dir(fsme->path)) {
			fsmenu_remove_entry(fsmenu, FS_CATEGORY_BOOKMARKS, index);
			changed = true;
		}
		else {
			index++;
		}
	}

	if (changed) {
		char name[FILE_MAX];

		BLI_make_file_string("/", name, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL), BLENDER_BOOKMARK_FILE);
		fsmenu_write_file(fsmenu, name);
		fsmenu_refresh_bookmarks_status(fsmenu);
		ED_area_tag_refresh(sa);
		ED_area_tag_redraw(sa);
	}

	return OPERATOR_FINISHED;
}

void FILE_OT_bookmark_cleanup(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Cleanup Bookmarks";
	ot->description = "Delete all invalid bookmarks";
	ot->idname = "FILE_OT_bookmark_cleanup";

	/* api callbacks */
	ot->exec = bookmark_cleanup_exec;
	ot->poll = ED_operator_file_active;

	/* properties */
}

enum {
	FILE_BOOKMARK_MOVE_TOP = -2,
	FILE_BOOKMARK_MOVE_UP = -1,
	FILE_BOOKMARK_MOVE_DOWN = 1,
	FILE_BOOKMARK_MOVE_BOTTOM = 2,
};

static int bookmark_move_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa = CTX_wm_area(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	struct FSMenu *fsmenu = ED_fsmenu_get();
	struct FSMenuEntry *fsmentry = ED_fsmenu_get_category(fsmenu, FS_CATEGORY_BOOKMARKS);
	const struct FSMenuEntry *fsmentry_org = fsmentry;

	char fname[FILE_MAX];

	const int direction = RNA_enum_get(op->ptr, "direction");
	const int totitems = ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_BOOKMARKS);
	const int act_index = sfile->bookmarknr;
	int new_index;

	if (totitems < 2) {
		return OPERATOR_CANCELLED;
	}

	switch (direction) {
		case FILE_BOOKMARK_MOVE_TOP:
			new_index = 0;
			break;
		case FILE_BOOKMARK_MOVE_BOTTOM:
			new_index = totitems - 1;
			break;
		case FILE_BOOKMARK_MOVE_UP:
		case FILE_BOOKMARK_MOVE_DOWN:
		default:
			new_index = (totitems + act_index + direction) % totitems;
			break;
	}

	if (new_index == act_index) {
		return OPERATOR_CANCELLED;
	}

	BLI_linklist_move_item((LinkNode **)&fsmentry, act_index, new_index);
	if (fsmentry != fsmentry_org) {
		ED_fsmenu_set_category(fsmenu, FS_CATEGORY_BOOKMARKS, fsmentry);
	}

	/* Need to update active bookmark number. */
	sfile->bookmarknr = new_index;

	BLI_make_file_string("/", fname, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL), BLENDER_BOOKMARK_FILE);
	fsmenu_write_file(fsmenu, fname);

	ED_area_tag_redraw(sa);
	return OPERATOR_FINISHED;
}

void FILE_OT_bookmark_move(wmOperatorType *ot)
{
	static const EnumPropertyItem slot_move[] = {
		{FILE_BOOKMARK_MOVE_TOP, "TOP", 0, "Top", "Top of the list"},
		{FILE_BOOKMARK_MOVE_UP, "UP", 0, "Up", ""},
		{FILE_BOOKMARK_MOVE_DOWN, "DOWN", 0, "Down", ""},
		{FILE_BOOKMARK_MOVE_BOTTOM, "BOTTOM", 0, "Bottom", "Bottom of the list"},
		{ 0, NULL, 0, NULL, NULL }
	};

	/* identifiers */
	ot->name = "Move Bookmark";
	ot->idname = "FILE_OT_bookmark_move";
	ot->description = "Move the active bookmark up/down in the list";

	/* api callbacks */
	ot->poll = ED_operator_file_active;
	ot->exec = bookmark_move_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER;  /* No undo! */

	RNA_def_enum(ot->srna, "direction", slot_move, 0, "Direction",
	             "Direction to move the active bookmark towards");
}

static int reset_recent_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	char name[FILE_MAX];
	struct FSMenu *fsmenu = ED_fsmenu_get();

	while (ED_fsmenu_get_entry(fsmenu, FS_CATEGORY_RECENT, 0) != NULL) {
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

	numfiles = filelist_files_ensure(sfile->files);
	params = ED_fileselect_get_params(sfile);

	origfile = params->highlight_file;

	mx -= ar->winrct.xmin;
	my -= ar->winrct.ymin;

	if (BLI_rcti_isect_pt(&ar->v2d.mask, mx, my)) {
		float fx, fy;
		int highlight_file;

		UI_view2d_region_to_view(v2d, mx, my, &fx, &fy);

		highlight_file = ED_fileselect_layout_offset(sfile->layout, (int)(v2d->tot.xmin + fx), (int)(v2d->tot.ymax - fy));

		if ((highlight_file >= 0) && (highlight_file < numfiles))
			params->highlight_file = highlight_file;
		else
			params->highlight_file = -1;
	}
	else
		params->highlight_file = -1;

	return (params->highlight_file != origfile);
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

static bool file_operator_poll(bContext *C)
{
	bool poll = ED_operator_file_active(C);
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


void file_sfile_to_operator_ex(bContext *C, wmOperator *op, SpaceFile *sfile, char *filepath)
{
	Main *bmain = CTX_data_main(C);
	PropertyRNA *prop;

	BLI_join_dirfile(filepath, FILE_MAX, sfile->params->dir, sfile->params->file); /* XXX, not real length */

	if ((prop = RNA_struct_find_property(op->ptr, "relative_path"))) {
		if (RNA_property_boolean_get(op->ptr, prop)) {
			BLI_path_rel(filepath, BKE_main_blendfile_path(bmain));
		}
	}

	if ((prop = RNA_struct_find_property(op->ptr, "filename"))) {
		RNA_property_string_set(op->ptr, prop, sfile->params->file);
	}
	if ((prop = RNA_struct_find_property(op->ptr, "directory"))) {
		RNA_property_string_set(op->ptr, prop, sfile->params->dir);
	}
	if ((prop = RNA_struct_find_property(op->ptr, "filepath"))) {
		RNA_property_string_set(op->ptr, prop, filepath);
	}

	/* some ops have multiple files to select */
	/* this is called on operators check() so clear collections first since
	 * they may be already set. */
	{
		int i, numfiles = filelist_files_ensure(sfile->files);

		if ((prop = RNA_struct_find_property(op->ptr, "files"))) {
			PointerRNA itemptr;
			int num_files = 0;
			RNA_property_collection_clear(op->ptr, prop);
			for (i = 0; i < numfiles; i++) {
				if (filelist_entry_select_index_get(sfile->files, i, CHECK_FILES)) {
					FileDirEntry *file = filelist_file(sfile->files, i);
					RNA_property_collection_add(op->ptr, prop, &itemptr);
					RNA_string_set(&itemptr, "name", file->relpath);
					num_files++;
				}
			}
			/* make sure the file specified in the filename button is added even if no files selected */
			if (0 == num_files) {
				RNA_property_collection_add(op->ptr, prop, &itemptr);
				RNA_string_set(&itemptr, "name", sfile->params->file);
			}
		}

		if ((prop = RNA_struct_find_property(op->ptr, "dirs"))) {
			PointerRNA itemptr;
			int num_dirs = 0;
			RNA_property_collection_clear(op->ptr, prop);
			for (i = 0; i < numfiles; i++) {
				if (filelist_entry_select_index_get(sfile->files, i, CHECK_DIRS)) {
					FileDirEntry *file = filelist_file(sfile->files, i);
					RNA_property_collection_add(op->ptr, prop, &itemptr);
					RNA_string_set(&itemptr, "name", file->relpath);
					num_dirs++;
				}
			}

			/* make sure the directory specified in the button is added even if no directory selected */
			if (0 == num_dirs) {
				RNA_property_collection_add(op->ptr, prop, &itemptr);
				RNA_string_set(&itemptr, "name", sfile->params->dir);
			}
		}


	}
}
void file_sfile_to_operator(bContext *C, wmOperator *op, SpaceFile *sfile)
{
	char filepath[FILE_MAX];

	file_sfile_to_operator_ex(C, op, sfile, filepath);
}

void file_operator_to_sfile(bContext *C, SpaceFile *sfile, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
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
	 * in the other direction but doesn't hurt to do this every time */
	BLI_path_abs(sfile->params->dir, BKE_main_blendfile_path(bmain));

	/* XXX, files and dirs updates missing, not really so important though */
}

/**
 * Use to set the file selector path from some arbitrary source.
 */
void file_sfile_filepath_set(SpaceFile *sfile, const char *filepath)
{
	BLI_assert(BLI_exists(filepath));

	if (BLI_is_dir(filepath)) {
		BLI_strncpy(sfile->params->dir, filepath, sizeof(sfile->params->dir));
	}
	else {
		if ((sfile->params->flag & FILE_DIRSEL_ONLY) == 0) {
			BLI_split_dirfile(filepath, sfile->params->dir, sfile->params->file,
			                  sizeof(sfile->params->dir), sizeof(sfile->params->file));
		}
		else {
			BLI_split_dir_part(filepath, sfile->params->dir, sizeof(sfile->params->dir));
		}
	}
}

void file_draw_check(bContext *C)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	wmOperator *op = sfile->op;
	if (op) { /* fail on reload */
		if (op->type->check) {
			file_sfile_to_operator(C, op, sfile);

			/* redraw */
			if (op->type->check(C, op)) {
				file_operator_to_sfile(C, sfile, op);

				/* redraw, else the changed settings wont get updated */
				ED_area_tag_redraw(CTX_wm_area(C));
			}
		}
	}
}

/* for use with; UI_block_func_set */
void file_draw_check_cb(bContext *C, void *UNUSED(arg1), void *UNUSED(arg2))
{
	file_draw_check(C);
}

bool file_draw_check_exists(SpaceFile *sfile)
{
	if (sfile->op) { /* fails on reload */
		PropertyRNA *prop;
		if ((prop = RNA_struct_find_property(sfile->op->ptr, "check_existing"))) {
			if (RNA_property_boolean_get(sfile->op->ptr, prop)) {
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

int file_exec(bContext *C, wmOperator *exec_op)
{
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	const struct FileDirEntry *file = filelist_file(sfile->files, sfile->params->active_file);
	char filepath[FILE_MAX];

	/* directory change */
	if (file && (file->typeflag & FILE_TYPE_DIR)) {
		if (!file->relpath) {
			return OPERATOR_CANCELLED;
		}

		if (FILENAME_IS_PARENT(file->relpath)) {
			BLI_parent_dir(sfile->params->dir);
		}
		else {
			BLI_cleanup_path(BKE_main_blendfile_path(bmain), sfile->params->dir);
			BLI_path_append(sfile->params->dir, sizeof(sfile->params->dir) - 1, file->relpath);
			BLI_add_slash(sfile->params->dir);
		}

		ED_file_change_dir(C);
	}
	/* opening file - sends events now, so things get handled on windowqueue level */
	else if (sfile->op) {
		wmOperator *op = sfile->op;

		/* when used as a macro, for doubleclick,
		 * to prevent closing when doubleclicking on .. item */
		if (RNA_boolean_get(exec_op->ptr, "need_active")) {
			const int numfiles = filelist_files_ensure(sfile->files);
			int i, active = 0;

			for (i = 0; i < numfiles; i++) {
				if (filelist_entry_select_index_get(sfile->files, i, CHECK_ALL)) {
					active = 1;
					break;
				}
			}
			if (active == 0)
				return OPERATOR_CANCELLED;
		}

		sfile->op = NULL;

		file_sfile_to_operator_ex(C, op, sfile, filepath);

		if (BLI_exists(sfile->params->dir)) {
			fsmenu_insert_entry(ED_fsmenu_get(), FS_CATEGORY_RECENT, sfile->params->dir, NULL,
			                    FS_INSERT_SAVE | FS_INSERT_FIRST);
		}

		BLI_make_file_string(BKE_main_blendfile_path(bmain), filepath, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL),
		                     BLENDER_BOOKMARK_FILE);
		fsmenu_write_file(ED_fsmenu_get(), filepath);
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
	Main *bmain = CTX_data_main(C);
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile->params) {
		if (BLI_parent_dir(sfile->params->dir)) {
			BLI_cleanup_dir(BKE_main_blendfile_path(bmain), sfile->params->dir);
			ED_file_change_dir(C);
			if (sfile->params->recursion_level > 1) {
				/* Disable 'dirtree' recursion when going up in tree. */
				sfile->params->recursion_level = 0;
				filelist_setrecursion(sfile->files, sfile->params->recursion_level);
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
	ScrArea *sa = CTX_wm_area(C);
	struct FSMenu *fsmenu = ED_fsmenu_get();

	ED_fileselect_clear(wm, sa, sfile);

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

		ED_file_change_dir(C);
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

		ED_file_change_dir(C);
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

	numfiles = filelist_files_ensure(sfile->files);

	/* check if we are editing a name */
	for (i = 0; i < numfiles; ++i) {
		if (filelist_entry_select_index_get(sfile->files, i, CHECK_ALL) ) {
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


static int filepath_drop_exec(bContext *C, wmOperator *op)
{
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile) {
		char filepath[FILE_MAX];

		RNA_string_get(op->ptr, "filepath", filepath);
		if (!BLI_exists(filepath)) {
			BKE_report(op->reports, RPT_ERROR, "File does not exist");
			return OPERATOR_CANCELLED;
		}

		file_sfile_filepath_set(sfile, filepath);

		if (sfile->op) {
			file_sfile_to_operator(C, sfile->op, sfile);
			file_draw_check(C);
		}

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void FILE_OT_filepath_drop(wmOperatorType *ot)
{
	ot->name = "File Selector Drop";
	ot->description = "";
	ot->idname = "FILE_OT_filepath_drop";

	ot->exec = filepath_drop_exec;
	ot->poll = WM_operator_winactive;

	RNA_def_string_file_path(ot->srna, "filepath", "Path", FILE_MAX, "", "");
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
	bool generate_name = true;
	PropertyRNA *prop;

	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	ScrArea *sa = CTX_wm_area(C);

	if (!sfile->params) {
		BKE_report(op->reports, RPT_WARNING, "No parent directory given");
		return OPERATOR_CANCELLED;
	}

	path[0] = '\0';

	if ((prop = RNA_struct_find_property(op->ptr, "directory"))) {
		RNA_property_string_get(op->ptr, prop, path);
		if (path[0] != '\0') {
			generate_name = false;
		}
	}

	if (generate_name) {
		/* create a new, non-existing folder name */
		if (!new_folder_path(sfile->params->dir, path, name)) {
			BKE_report(op->reports, RPT_ERROR, "Could not create new folder name");
			return OPERATOR_CANCELLED;
		}
	}
	else { /* We assume we are able to generate a valid name! */
		char org_path[FILE_MAX];

		BLI_strncpy(org_path, path, sizeof(org_path));
		if (BLI_path_make_safe(path)) {
			BKE_reportf(op->reports, RPT_WARNING, "'%s' given path is OS-invalid, creating '%s' path instead",
			            org_path, path);
		}
	}

	/* create the file */
	errno = 0;
	if (!BLI_dir_create_recursive(path) ||
	    /* Should no more be needed,
	     * now that BLI_dir_create_recursive returns a success state - but kept just in case. */
	    !BLI_exists(path))
	{
		BKE_reportf(op->reports, RPT_ERROR,
		            "Could not create new folder: %s",
		            errno ? strerror(errno) : "unknown error");
		return OPERATOR_CANCELLED;
	}


	/* now remember file to jump into editing */
	BLI_strncpy(sfile->params->renamefile, name, FILE_MAXFILE);

	/* set timer to smoothly view newly generated file */
	sfile->smoothscroll_timer = WM_event_add_timer(wm, CTX_wm_window(C), TIMER1, 1.0 / 1000.0);  /* max 30 frs/sec */
	sfile->scroll_offset = 0;

	/* reload dir to make sure we're seeing what's in the directory */
	ED_fileselect_clear(wm, sa, sfile);

	if (RNA_boolean_get(op->ptr, "open")) {
		BLI_strncpy(sfile->params->dir, path, sizeof(sfile->params->dir));
		ED_file_change_dir(C);
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


/* TODO This should go to BLI_path_utils. */
static void file_expand_directory(bContext *C)
{
	Main *bmain = CTX_data_main(C);
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile->params) {
		if (BLI_path_is_rel(sfile->params->dir)) {
			/* Use of 'default' folder here is just to avoid an error message on '//' prefix. */
			BLI_path_abs(sfile->params->dir, G.relbase_valid ? BKE_main_blendfile_path(bmain) : BKE_appdir_folder_default());
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

/* TODO check we still need this, it's annoying to have OS-specific code here... :/ */
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
	Main *bmain = CTX_data_main(C);
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile->params) {
		file_expand_directory(C);

		/* special case, user may have pasted a filepath into the directory */
		if (!filelist_is_dir(sfile->files, sfile->params->dir)) {
			char tdir[FILE_MAX_LIBEXTRA];
			char *group, *name;

			if (BLI_is_file(sfile->params->dir)) {
				char path[sizeof(sfile->params->dir)];
				BLI_strncpy(path, sfile->params->dir, sizeof(path));
				BLI_split_dirfile(path, sfile->params->dir, sfile->params->file,
				                  sizeof(sfile->params->dir), sizeof(sfile->params->file));
			}
			else if (BLO_library_path_explode(sfile->params->dir, tdir, &group, &name)) {
				if (group) {
					BLI_path_append(tdir, sizeof(tdir), group);
				}
				BLI_strncpy(sfile->params->dir, tdir, sizeof(sfile->params->dir));
				if (name) {
					BLI_strncpy(sfile->params->file, name, sizeof(sfile->params->file));
				}
				else {
					sfile->params->file[0] = '\0';
				}
			}
		}

		BLI_cleanup_dir(BKE_main_blendfile_path(bmain), sfile->params->dir);

		if (filelist_is_dir(sfile->files, sfile->params->dir)) {
			/* if directory exists, enter it immediately */
			ED_file_change_dir(C);

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
			char tdir[FILE_MAX_LIBEXTRA];

			/* If we are 'inside' a blend library, we cannot do anything... */
			if (lastdir && BLO_library_path_explode(lastdir, tdir, NULL, NULL)) {
				BLI_strncpy(sfile->params->dir, lastdir, sizeof(sfile->params->dir));
			}
			else {
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
		}

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
	}
}

void file_filename_enter_handle(bContext *C, void *UNUSED(arg_unused), void *arg_but)
{
	Main *bmain = CTX_data_main(C);
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

		/* *After* file_select_match! */
		BLI_filename_make_safe(sfile->params->file);

		if (matches) {
			/* replace the pattern (or filename that the user typed in, with the first selected file of the match */
			BLI_strncpy(sfile->params->file, matched_file, sizeof(sfile->params->file));

			WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);
		}

		if (matches == 1) {
			BLI_join_dirfile(filepath, sizeof(sfile->params->dir), sfile->params->dir, sfile->params->file);

			/* if directory, open it and empty filename field */
			if (filelist_is_dir(sfile->files, filepath)) {
				BLI_cleanup_dir(BKE_main_blendfile_path(bmain), filepath);
				BLI_strncpy(sfile->params->dir, filepath, sizeof(sfile->params->dir));
				sfile->params->file[0] = '\0';
				ED_file_change_dir(C);
				UI_textbutton_activate_but(C, but);
				WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);
			}
		}
		else if (matches > 1) {
			file_draw_check(C);
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
	ScrArea *sa = CTX_wm_area(C);

	if (sfile->params) {
		sfile->params->flag ^= FILE_HIDE_DOT;
		ED_fileselect_clear(wm, sa, sfile);
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

ARegion *file_tools_region(ScrArea *sa)
{
	ARegion *ar, *arnew;

	if ((ar = BKE_area_find_region_type(sa, RGN_TYPE_TOOLS)) != NULL)
		return ar;

	/* add subdiv level; after header */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

	/* is error! */
	if (ar == NULL)
		return NULL;

	arnew = MEM_callocN(sizeof(ARegion), "tools for file");
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_TOOLS;
	arnew->alignment = RGN_ALIGN_LEFT;

	ar = MEM_callocN(sizeof(ARegion), "tool props for file");
	BLI_insertlinkafter(&sa->regionbase, arnew, ar);
	ar->regiontype = RGN_TYPE_TOOL_PROPS;
	ar->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;

	return arnew;
}

static int file_bookmark_toggle_exec(bContext *C, wmOperator *UNUSED(unused))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = file_tools_region(sa);

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


/**
 * Looks for a string of digits within name (using BLI_stringdec) and adjusts it by add.
 */
static void filenum_newname(char *name, size_t name_size, int add)
{
	char head[FILE_MAXFILE], tail[FILE_MAXFILE];
	char name_temp[FILE_MAXFILE];
	int pic;
	unsigned short digits;

	pic = BLI_stringdec(name, head, tail, &digits);

	/* are we going from 100 -> 99 or from 10 -> 9 */
	if (add < 0 && digits > 0) {
		int i, exp;
		exp = 1;
		for (i = digits; i > 1; i--) {
			exp *= 10;
		}
		if (pic >= exp && (pic + add) < exp) {
			digits--;
		}
	}

	pic += add;
	if (pic < 0)
		pic = 0;
	BLI_stringenc(name_temp, head, tail, digits, pic);
	BLI_strncpy(name, name_temp, name_size);
}

static int file_filenum_exec(bContext *C, wmOperator *op)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	ScrArea *sa = CTX_wm_area(C);

	int inc = RNA_int_get(op->ptr, "increment");
	if (sfile->params && (inc != 0)) {
		filenum_newname(sfile->params->file, sizeof(sfile->params->file), inc);
		ED_area_tag_redraw(sa);
		file_draw_check(C);
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
		int idx = sfile->params->highlight_file;
		int numfiles = filelist_files_ensure(sfile->files);
		if ((0 <= idx) && (idx < numfiles)) {
			FileDirEntry *file = filelist_file(sfile->files, idx);
			filelist_entry_select_index_set(sfile->files, idx, FILE_SEL_ADD, FILE_SEL_EDITING, CHECK_ALL);
			BLI_strncpy(sfile->params->renameedit, file->relpath, FILE_MAXFILE);
			sfile->params->renamefile[0] = '\0';
		}
		ED_area_tag_redraw(sa);
	}

	return OPERATOR_FINISHED;

}

static bool file_rename_poll(bContext *C)
{
	bool poll = ED_operator_file_active(C);
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile && sfile->params) {
		int idx = sfile->params->highlight_file;
		int numfiles = filelist_files_ensure(sfile->files);

		if ((0 <= idx) && (idx < numfiles)) {
			FileDirEntry *file = filelist_file(sfile->files, idx);
			if (FILENAME_IS_CURRPAR(file->relpath)) {
				poll = false;
			}
		}

		if (sfile->params->highlight_file < 0) {
			poll = false;
		}
		else {
			char dir[FILE_MAX_LIBEXTRA];
			if (filelist_islibrary(sfile->files, dir, NULL)) {
				poll = false;
			}
		}
	}
	else {
		poll = false;
	}

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

static bool file_delete_poll(bContext *C)
{
	bool poll = ED_operator_file_active(C);
	SpaceFile *sfile = CTX_wm_space_file(C);

	if (sfile && sfile->params) {
		char dir[FILE_MAX_LIBEXTRA];
		int numfiles = filelist_files_ensure(sfile->files);
		int i;
		int num_selected = 0;

		if (filelist_islibrary(sfile->files, dir, NULL)) poll = 0;
		for (i = 0; i < numfiles; i++) {
			if (filelist_entry_select_index_get(sfile->files, i, CHECK_FILES)) {
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

int file_delete_exec(bContext *C, wmOperator *op)
{
	char str[FILE_MAX];
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile = CTX_wm_space_file(C);
	ScrArea *sa = CTX_wm_area(C);
	FileDirEntry *file;
	int numfiles = filelist_files_ensure(sfile->files);
	int i;

	bool report_error = false;
	errno = 0;
	for (i = 0; i < numfiles; i++) {
		if (filelist_entry_select_index_get(sfile->files, i, CHECK_FILES)) {
			file = filelist_file(sfile->files, i);
			BLI_make_file_string(BKE_main_blendfile_path(bmain), str, sfile->params->dir, file->relpath);
			if (BLI_delete(str, false, false) != 0 ||
			    BLI_exists(str))
			{
				report_error = true;
			}
		}
	}

	if (report_error) {
		BKE_reportf(op->reports, RPT_ERROR,
		            "Could not delete file: %s",
		            errno ? strerror(errno) : "unknown error");
	}

	ED_fileselect_clear(wm, sa, sfile);
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
