/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.hh"
#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "ED_fileselect.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "file_intern.hh"
#include "filelist.hh"
#include "fsmenu.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* -------------------------------------------------------------------- */
/** \name File Selection Utilities
 * \{ */

static FileSelection find_file_mouse_rect(SpaceFile *sfile,
                                          ARegion *region,
                                          const rcti *rect_region)
{
  FileSelection sel;

  View2D *v2d = &region->v2d;
  rcti rect_view;
  rctf rect_view_fl;
  rctf rect_region_fl;

  BLI_rctf_rcti_copy(&rect_region_fl, rect_region);

  /* Okay, manipulating v2d rects here is hacky... */
  v2d->mask.ymax -= sfile->layout->offset_top;
  v2d->cur.ymax -= sfile->layout->offset_top;
  UI_view2d_region_to_view_rctf(v2d, &rect_region_fl, &rect_view_fl);
  v2d->mask.ymax += sfile->layout->offset_top;
  v2d->cur.ymax += sfile->layout->offset_top;

  BLI_rcti_init(&rect_view,
                int(v2d->tot.xmin + rect_view_fl.xmin),
                int(v2d->tot.xmin + rect_view_fl.xmax),
                int(v2d->tot.ymax - rect_view_fl.ymin),
                int(v2d->tot.ymax - rect_view_fl.ymax));

  sel = ED_fileselect_layout_offset_rect(sfile->layout, &rect_view);

  return sel;
}

enum FileSelect {
  FILE_SELECT_NOTHING = 0,
  FILE_SELECT_DIR = 1,
  FILE_SELECT_FILE = 2,
};

static void clamp_to_filelist(int numfiles, FileSelection *sel)
{
  /* box select before the first file */
  if ((sel->first < 0) && (sel->last >= 0)) {
    sel->first = 0;
  }
  /* don't select if everything is outside filelist */
  if ((sel->first >= numfiles) && ((sel->last < 0) || (sel->last >= numfiles))) {
    sel->first = -1;
    sel->last = -1;
  }

  /* fix if last file invalid */
  if ((sel->first > 0) && (sel->last < 0)) {
    sel->last = numfiles - 1;
  }

  /* clamp */
  if (sel->first >= numfiles) {
    sel->first = numfiles - 1;
  }
  if (sel->last >= numfiles) {
    sel->last = numfiles - 1;
  }
}

static FileSelection file_selection_get(bContext *C, const rcti *rect, bool fill)
{
  ARegion *region = CTX_wm_region(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  int numfiles = filelist_files_ensure(sfile->files);
  FileSelection sel;

  sel = find_file_mouse_rect(sfile, region, rect);
  if (!((sel.first == -1) && (sel.last == -1))) {
    clamp_to_filelist(numfiles, &sel);
  }

  /* if desired, fill the selection up from the last selected file to the current one */
  if (fill && (sel.last >= 0) && (sel.last < numfiles)) {
    int f;
    /* Try to find a smaller-index selected item. */
    for (f = sel.last; f >= 0; f--) {
      if (filelist_entry_select_index_get(sfile->files, f, CHECK_ALL)) {
        break;
      }
    }
    if (f >= 0) {
      sel.first = f + 1;
    }
    /* If none found, try to find a higher-index selected item. */
    else {
      for (f = sel.first; f < numfiles; f++) {
        if (filelist_entry_select_index_get(sfile->files, f, CHECK_ALL)) {
          break;
        }
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
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  int numfiles = filelist_files_ensure(sfile->files);
  const FileDirEntry *file;

  /* make the selected file active */
  if ((selected_idx >= 0) && (selected_idx < numfiles) &&
      (file = filelist_file(sfile->files, selected_idx)))
  {
    params->highlight_file = selected_idx;
    params->active_file = selected_idx;

    if (file->typeflag & FILE_TYPE_DIR) {
      const bool is_parent_dir = FILENAME_IS_PARENT(file->relpath);

      if (do_diropen == false) {
        retval = FILE_SELECT_DIR;
      }
      /* the path is too long and we are not going up! */
      else if (!is_parent_dir && strlen(params->dir) + strlen(file->relpath) >= FILE_MAX) {
        // XXX error("Path too long, cannot enter this directory");
      }
      else {
        if (is_parent_dir) {
          /* Avoids `/../../`. */
          BLI_path_parent_dir(params->dir);

          if (params->recursion_level > 1) {
            /* Disable `dirtree` recursion when going up in tree. */
            params->recursion_level = 0;
            filelist_setrecursion(sfile->files, params->recursion_level);
          }
        }
        else if (file->redirection_path) {
          STRNCPY(params->dir, file->redirection_path);
          BLI_path_abs(params->dir, BKE_main_blendfile_path(bmain));
          BLI_path_normalize_dir(params->dir, sizeof(params->dir));
        }
        else {
          BLI_path_abs(params->dir, BKE_main_blendfile_path(bmain));
          BLI_path_normalize_dir(params->dir, sizeof(params->dir));
          BLI_path_append_dir(params->dir, sizeof(params->dir), file->relpath);
        }

        ED_file_change_dir(C);
        retval = FILE_SELECT_DIR;
      }
    }
    else {
      retval = FILE_SELECT_FILE;
    }
    fileselect_file_set(C, sfile, selected_idx);
  }
  return retval;
}

/**
 * \warning Loops over all files so better use cautiously.
 */
static bool file_is_any_selected(FileList *files)
{
  const int numfiles = filelist_files_ensure(files);
  int i;

  /* Is any file selected ? */
  for (i = 0; i < numfiles; i++) {
    if (filelist_entry_select_index_get(files, i, CHECK_ALL)) {
      return true;
    }
  }

  return false;
}

static FileSelection file_current_selection_range_get(FileList *files)
{
  const int numfiles = filelist_files_ensure(files);
  FileSelection selection = {-1, -1};

  /* Iterate over the files once but in two loops, one to find the first selected file, and the
   * other to find the last. */

  int file_index;
  for (file_index = 0; file_index < numfiles; file_index++) {
    if (filelist_entry_is_selected(files, file_index)) {
      /* First selected entry found. */
      selection.first = file_index;
      break;
    }
  }

  for (; file_index < numfiles; file_index++) {
    if (filelist_entry_is_selected(files, file_index)) {
      selection.last = file_index;
      /* Keep looping, we may find more selected files. */
    }
  }

  return selection;
}

/**
 * If \a file is outside viewbounds, this adjusts view to make sure it's inside
 */
static void file_ensure_inside_viewbounds(ARegion *region, SpaceFile *sfile, const int file)
{
  FileLayout *layout = ED_fileselect_get_layout(sfile, region);
  rctf *cur = &region->v2d.cur;
  rcti rect;
  bool changed = true;

  file_tile_boundbox(region, layout, file, &rect);

  /* down - also use if tile is higher than viewbounds so view is aligned to file name */
  if (cur->ymin > rect.ymin || layout->tile_h > region->winy) {
    cur->ymin = rect.ymin - (2 * layout->tile_border_y);
    cur->ymax = cur->ymin + region->winy;
  }
  /* up */
  else if ((cur->ymax - layout->offset_top) < rect.ymax) {
    cur->ymax = rect.ymax + layout->tile_border_y + layout->offset_top;
    cur->ymin = cur->ymax - region->winy;
  }
  /* left - also use if tile is wider than viewbounds so view is aligned to file name */
  else if (cur->xmin > rect.xmin || layout->tile_w > region->winx) {
    cur->xmin = rect.xmin - layout->tile_border_x;
    cur->xmax = cur->xmin + region->winx;
  }
  /* right */
  else if (cur->xmax < rect.xmax) {
    cur->xmax = rect.xmax + (2 * layout->tile_border_x);
    cur->xmin = cur->xmax - region->winx;
  }
  else {
    BLI_assert(cur->xmin <= rect.xmin && cur->xmax >= rect.xmax && cur->ymin <= rect.ymin &&
               (cur->ymax - layout->offset_top) >= rect.ymax);
    changed = false;
  }

  if (changed) {
    UI_view2d_curRect_validate(&region->v2d);
  }
}

static void file_ensure_selection_inside_viewbounds(ARegion *region,
                                                    SpaceFile *sfile,
                                                    FileSelection *sel)
{
  const FileLayout *layout = ED_fileselect_get_layout(sfile, region);

  if (((layout->flag & FILE_LAYOUT_HOR) && region->winx <= (1.2f * layout->tile_w)) &&
      ((layout->flag & FILE_LAYOUT_VER) && region->winy <= (2.0f * layout->tile_h)))
  {
    return;
  }

  /* Adjust view to display selection. Doing iterations for first and last
   * selected item makes view showing as much of the selection possible.
   * Not really useful if tiles are (almost) bigger than viewbounds though. */
  file_ensure_inside_viewbounds(region, sfile, sel->last);
  file_ensure_inside_viewbounds(region, sfile, sel->first);
}

static FileSelect file_select(
    bContext *C, const rcti *rect, FileSelType select, bool fill, bool do_diropen)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  FileSelect retval = FILE_SELECT_NOTHING;
  FileSelection sel = file_selection_get(C, rect, fill); /* get the selection */
  const FileCheckType check_type = (params->flag & FILE_DIRSEL_ONLY) ? CHECK_DIRS : CHECK_ALL;

  /* flag the files as selected in the filelist */
  filelist_entries_select_index_range_set(
      sfile->files, &sel, select, FILE_SEL_SELECTED, check_type);

  /* Don't act on multiple selected files */
  if (sel.first != sel.last) {
    select = FileSelType(0);
  }

  /* Do we have a valid selection and are we actually selecting */
  if ((sel.last >= 0) && (select != FILE_SEL_REMOVE)) {
    /* Check last selection, if selected, act on the file or dir */
    if (filelist_entry_select_index_get(sfile->files, sel.last, check_type)) {
      retval = file_select_do(C, sel.last, do_diropen);
    }
  }

  if (select != FILE_SEL_ADD && !file_is_any_selected(sfile->files)) {
    params->active_file = -1;
  }
  else if (sel.last >= 0) {
    ARegion *region = CTX_wm_region(C);
    file_ensure_selection_inside_viewbounds(region, sfile, &sel);
  }

  /* update operator for name change event */
  file_draw_check(C);

  return retval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bookmark Utilities
 * \{ */

/**
 * Local utility to write #BLENDER_BOOKMARK_FILE, reporting an error on failure.
 */
static bool fsmenu_write_file_and_refresh_or_report_error(FSMenu *fsmenu,
                                                          ScrArea *area,
                                                          ReportList *reports)
{
  /* NOTE: use warning instead of error here, because the bookmark operation may be part of
   * other actions which should not cause the operator to fail entirely. */
  const std::optional<std::string> cfgdir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG,
                                                                        nullptr);
  if (!cfgdir.has_value()) {
    BKE_report(reports, RPT_ERROR, "Unable to create configuration directory to write bookmarks");
    return false;
  }

  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), cfgdir->c_str(), BLENDER_BOOKMARK_FILE);
  if (!fsmenu_write_file(fsmenu, filepath)) {
    BKE_reportf(reports, RPT_ERROR, "Unable to open or write bookmark file \"%s\"", filepath);
    return false;
  }

  ED_area_tag_refresh(area);
  ED_area_tag_redraw(area);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

static int file_box_select_find_last_selected(SpaceFile *sfile,
                                              ARegion *region,
                                              const FileSelection *sel,
                                              const int mouse_xy[2])
{
  FileLayout *layout = ED_fileselect_get_layout(sfile, region);
  rcti bounds_first, bounds_last;
  int dist_first, dist_last;
  float mouseco_view[2];

  UI_view2d_region_to_view(&region->v2d, UNPACK2(mouse_xy), &mouseco_view[0], &mouseco_view[1]);

  file_tile_boundbox(region, layout, sel->first, &bounds_first);
  file_tile_boundbox(region, layout, sel->last, &bounds_last);

  /* are first and last in the same column (horizontal layout)/row (vertical layout)? */
  if ((layout->flag & FILE_LAYOUT_HOR && bounds_first.xmin == bounds_last.xmin) ||
      (layout->flag & FILE_LAYOUT_VER && bounds_first.ymin != bounds_last.ymin))
  {
    /* use vertical distance */
    const int my_loc = int(mouseco_view[1]);
    dist_first = BLI_rcti_length_y(&bounds_first, my_loc);
    dist_last = BLI_rcti_length_y(&bounds_last, my_loc);
  }
  else {
    /* use horizontal distance */
    const int mx_loc = int(mouseco_view[0]);
    dist_first = BLI_rcti_length_x(&bounds_first, mx_loc);
    dist_last = BLI_rcti_length_x(&bounds_last, mx_loc);
  }

  return (dist_first < dist_last) ? sel->first : sel->last;
}

static wmOperatorStatus file_box_select_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  FileSelection sel;
  rcti rect;

  wmOperatorStatus result;

  result = WM_gesture_box_modal(C, op, event);

  if (result == OPERATOR_RUNNING_MODAL) {
    WM_operator_properties_border_to_rcti(op, &rect);

    ED_fileselect_layout_isect_rect(sfile->layout, &region->v2d, &rect, &rect);

    sel = file_selection_get(C, &rect, false);
    if ((sel.first != params->sel_first) || (sel.last != params->sel_last)) {
      int idx;

      file_select_deselect_all(sfile, FILE_SEL_HIGHLIGHTED);
      filelist_entries_select_index_range_set(
          sfile->files, &sel, FILE_SEL_ADD, FILE_SEL_HIGHLIGHTED, CHECK_ALL);
      WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

      for (idx = sel.last; idx >= 0; idx--) {
        const FileDirEntry *file = filelist_file(sfile->files, idx);

        /* Don't highlight read-only file (".." or ".") on box select. */
        if (FILENAME_IS_CURRPAR(file->relpath)) {
          filelist_entry_select_set(
              sfile->files, file, FILE_SEL_REMOVE, FILE_SEL_HIGHLIGHTED, CHECK_ALL);
        }

        /* make sure highlight_file is no readonly file */
        if (sel.last == idx) {
          params->highlight_file = idx;
        }
      }
    }
    params->sel_first = sel.first;
    params->sel_last = sel.last;
    params->active_file = file_box_select_find_last_selected(sfile, region, &sel, event->mval);
  }
  else {
    params->highlight_file = -1;
    params->sel_first = params->sel_last = -1;
    fileselect_file_set(C, sfile, params->active_file);
    file_select_deselect_all(sfile, FILE_SEL_HIGHLIGHTED);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
  }

  return result;
}

static wmOperatorStatus file_box_select_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  rcti rect;
  FileSelect ret;

  WM_operator_properties_border_to_rcti(op, &rect);

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    file_select_deselect_all(sfile, FILE_SEL_SELECTED);
  }

  ED_fileselect_layout_isect_rect(sfile->layout, &region->v2d, &rect, &rect);

  ret = file_select(C, &rect, select ? FILE_SEL_ADD : FILE_SEL_REMOVE, false, false);

  /* unselect '..' parent entry - it's not supposed to be selected if more than
   * one file is selected */
  filelist_entry_parent_select_set(sfile->files, FILE_SEL_REMOVE, FILE_SEL_SELECTED, CHECK_ALL);

  if (FILE_SELECT_DIR == ret) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
  }
  else if (FILE_SELECT_FILE == ret) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
  }
  return OPERATOR_FINISHED;
}

void FILE_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->description = "Activate/select the file(s) contained in the border";
  ot->idname = "FILE_OT_select_box";

  /* API callbacks. */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = file_box_select_exec;
  ot->modal = file_box_select_modal;
  /* Operator works for file or asset browsing */
  ot->poll = ED_operator_file_active;
  ot->cancel = WM_gesture_box_cancel;

  /* properties */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Pick Operator
 * \{ */

static rcti file_select_mval_to_select_rect(const int mval[2])
{
  rcti rect;
  rect.xmin = rect.xmax = mval[0];
  rect.ymin = rect.ymax = mval[1];
  return rect;
}

static wmOperatorStatus file_select_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelect ret;
  rcti rect;
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool fill = RNA_boolean_get(op->ptr, "fill");
  const bool do_diropen = RNA_boolean_get(op->ptr, "open");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  const bool only_activate_if_selected = RNA_boolean_get(op->ptr, "only_activate_if_selected");
  /* Used so right mouse clicks can do both, activate and spawn the context menu. */
  const bool pass_through = RNA_boolean_get(op->ptr, "pass_through");
  bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");

  if (region->regiontype != RGN_TYPE_WINDOW) {
    return OPERATOR_CANCELLED;
  }

  int mval[2];
  mval[0] = RNA_int_get(op->ptr, "mouse_x");
  mval[1] = RNA_int_get(op->ptr, "mouse_y");
  rect = file_select_mval_to_select_rect(mval);

  if (!ED_fileselect_layout_is_inside_pt(sfile->layout, &region->v2d, rect.xmin, rect.ymin)) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  if (extend || fill) {
    wait_to_deselect_others = false;
  }

  wmOperatorStatus ret_val = OPERATOR_FINISHED;

  const FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  if (params) {
    int idx = params->highlight_file;
    int numfiles = filelist_files_ensure(sfile->files);

    if ((idx >= 0) && (idx < numfiles)) {
      const bool is_selected = filelist_entry_select_index_get(sfile->files, idx, CHECK_ALL) &
                               FILE_SEL_SELECTED;
      if (only_activate_if_selected && is_selected) {
        /* Don't deselect other items. */
      }
      else if (wait_to_deselect_others && is_selected) {
        ret_val = OPERATOR_RUNNING_MODAL;
      }
      /* single select, deselect all selected first */
      else if (!extend) {
        file_select_deselect_all(sfile, FILE_SEL_SELECTED);
      }
    }
  }

  ret = file_select(C, &rect, extend ? FILE_SEL_TOGGLE : FILE_SEL_ADD, fill, do_diropen);

  if (extend) {
    /* unselect '..' parent entry - it's not supposed to be selected if more
     * than one file is selected */
    filelist_entry_parent_select_set(sfile->files, FILE_SEL_REMOVE, FILE_SEL_SELECTED, CHECK_ALL);
  }

  if (ret == FILE_SELECT_NOTHING) {
    if (deselect_all) {
      file_select_deselect_all(sfile, FILE_SEL_SELECTED);
    }
  }
  else if (ret == FILE_SELECT_DIR) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
  }
  else if (ret == FILE_SELECT_FILE) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
  }

  WM_event_add_mousemove(CTX_wm_window(C)); /* for directory changes */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  if ((ret_val == OPERATOR_FINISHED) && pass_through) {
    ret_val |= OPERATOR_PASS_THROUGH;
  }
  return ret_val;
}

void FILE_OT_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select";
  ot->idname = "FILE_OT_select";
  ot->description = "Handle mouse clicks to select and activate items";

  /* API callbacks. */
  ot->invoke = WM_generic_select_invoke;
  ot->exec = file_select_exec;
  ot->modal = WM_generic_select_modal;
  /* Operator works for file or asset browsing */
  ot->poll = ED_operator_file_active;

  /* properties */
  WM_operator_properties_generic_select(ot);
  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         false,
                         "Extend",
                         "Extend selection instead of deselecting everything first");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "fill", false, "Fill", "Select everything beginning with the last selection");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_FILEBROWSER);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "open", true, "Open", "Open a directory when selecting it");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "deselect_all",
                         false,
                         "Deselect On Nothing",
                         "Deselect all when nothing under the cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "only_activate_if_selected",
                         false,
                         "Only Activate if Selected",
                         "Do not change selection if the item under the cursor is already "
                         "selected, only activate it");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "pass_through",
                         false,
                         "Pass Through",
                         "Even on successful execution, pass the event on so other operators can "
                         "execute on it as well");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Walk Operator
 * \{ */

/**
 * \returns true if selection has changed
 */
static bool file_walk_select_selection_set(bContext *C,
                                           wmWindow *win,
                                           ARegion *region,
                                           SpaceFile *sfile,
                                           const int direction,
                                           const int numfiles,
                                           const int active_old,
                                           const int active_new,
                                           const int other_site,
                                           const bool has_selection,
                                           const bool extend,
                                           const bool fill)
{
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  FileList *files = sfile->files;
  const int last_sel = params->active_file; /* store old value */
  int active = active_old; /* could use active_old instead, just for readability */
  bool deselect = false;

  BLI_assert(params);

  if (numfiles == 0) {
    /* No files visible, nothing to do. */
    return false;
  }

  if (has_selection) {
    if (extend && filelist_entry_select_index_get(files, active_old, CHECK_ALL) &&
        filelist_entry_select_index_get(files, active_new, CHECK_ALL))
    {
      /* conditions for deselecting: initial file is selected, new file is
       * selected and either other_side isn't selected/found or we use fill */
      deselect = (fill || other_site == -1 ||
                  !filelist_entry_select_index_get(files, other_site, CHECK_ALL));

      /* don't change highlight_file here since we either want to deselect active or we want
       * to walk through a block of selected files without selecting/deselecting anything */
      params->active_file = active_new;
      /* but we want to change active if we use fill
       * (needed to get correct selection bounds) */
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
    if (ELEM(direction, UI_SELECT_WALK_UP, UI_SELECT_WALK_LEFT)) {
      params->active_file = active = numfiles - 1;
    }
    /* select first file */
    else if (ELEM(direction, UI_SELECT_WALK_DOWN, UI_SELECT_WALK_RIGHT)) {
      params->active_file = active = 0;
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

    /* unselect '..' parent entry - it's not supposed to be selected if more
     * than one file is selected */
    filelist_entry_parent_select_set(files, FILE_SEL_REMOVE, FILE_SEL_SELECTED, CHECK_ALL);
  }
  else {
    /* deselect all first */
    file_select_deselect_all(sfile, FILE_SEL_SELECTED);

    /* highlight file under mouse pos */
    params->highlight_file = -1;
    WM_event_add_mousemove(win);
  }

  /* do the actual selection */
  if (fill) {
    FileSelection sel = {std::min(active, last_sel), std::max(active, last_sel)};

    /* fill selection between last and first selected file */
    filelist_entries_select_index_range_set(
        files, &sel, deselect ? FILE_SEL_REMOVE : FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);
    /* entire sel is cleared here, so select active again */
    if (deselect) {
      filelist_entry_select_index_set(files, active, FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);
    }

    /* unselect '..' parent entry - it's not supposed to be selected if more
     * than one file is selected */
    if ((sel.last - sel.first) > 1) {
      filelist_entry_parent_select_set(files, FILE_SEL_REMOVE, FILE_SEL_SELECTED, CHECK_ALL);
    }
  }
  else {
    filelist_entry_select_index_set(
        files, active, deselect ? FILE_SEL_REMOVE : FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);
  }

  BLI_assert(IN_RANGE(active, -1, numfiles));
  fileselect_file_set(C, sfile, params->active_file);

  /* ensure newly selected file is inside viewbounds */
  file_ensure_inside_viewbounds(region, sfile, params->active_file);

  /* selection changed */
  return true;
}

/**
 * \returns true if selection has changed
 */
static bool file_walk_select_do(bContext *C,
                                SpaceFile *sfile,
                                FileSelectParams *params,
                                const int direction,
                                const bool extend,
                                const bool fill)
{
  wmWindow *win = CTX_wm_window(C);
  ARegion *region = CTX_wm_region(C);
  FileList *files = sfile->files;
  const int numfiles = filelist_files_ensure(files);
  const bool has_selection = file_is_any_selected(files);
  const int active_old = params->active_file;
  int active_new = -1;
  int other_site = -1; /* file on the other site of active_old */

  /* *** get all needed files for handling selection *** */

  if (numfiles == 0) {
    /* No files visible, nothing to do. */
    return false;
  }

  if (has_selection) {
    FileLayout *layout = ED_fileselect_get_layout(sfile, region);
    const int idx_shift = (layout->flag & FILE_LAYOUT_HOR) ? layout->rows : layout->flow_columns;

    if ((layout->flag & FILE_LAYOUT_HOR && direction == UI_SELECT_WALK_UP) ||
        (layout->flag & FILE_LAYOUT_VER && direction == UI_SELECT_WALK_LEFT))
    {
      active_new = active_old - 1;
      other_site = active_old + 1;
    }
    else if ((layout->flag & FILE_LAYOUT_HOR && direction == UI_SELECT_WALK_DOWN) ||
             (layout->flag & FILE_LAYOUT_VER && direction == UI_SELECT_WALK_RIGHT))
    {
      active_new = active_old + 1;
      other_site = active_old - 1;
    }
    else if ((layout->flag & FILE_LAYOUT_HOR && direction == UI_SELECT_WALK_LEFT) ||
             (layout->flag & FILE_LAYOUT_VER && direction == UI_SELECT_WALK_UP))
    {
      active_new = active_old - idx_shift;
      other_site = active_old + idx_shift;
    }
    else if ((layout->flag & FILE_LAYOUT_HOR && direction == UI_SELECT_WALK_RIGHT) ||
             (layout->flag & FILE_LAYOUT_VER && direction == UI_SELECT_WALK_DOWN))
    {

      active_new = active_old + idx_shift;
      other_site = active_old - idx_shift;
    }
    else {
      BLI_assert(0);
    }

    if (!IN_RANGE(active_new, -1, numfiles)) {
      if (extend) {
        /* extend to invalid file -> abort */
        return false;
      }
      /* if we don't extend, selecting '..' (index == 0) is allowed so
       * using key selection to go to parent directory is possible */
      if (active_new != 0) {
        /* select initial file */
        active_new = active_old;
      }
    }
    if (!IN_RANGE(other_site, 0, numfiles)) {
      other_site = -1;
    }
  }

  return file_walk_select_selection_set(C,
                                        win,
                                        region,
                                        sfile,
                                        direction,
                                        numfiles,
                                        active_old,
                                        active_new,
                                        other_site,
                                        has_selection,
                                        extend,
                                        fill);
}

static wmOperatorStatus file_walk_select_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  SpaceFile *sfile = (SpaceFile *)CTX_wm_space_data(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  const int direction = RNA_enum_get(op->ptr, "direction");
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool fill = RNA_boolean_get(op->ptr, "fill");

  if (file_walk_select_do(C, sfile, params, direction, extend, fill)) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void FILE_OT_select_walk(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Walk Select/Deselect File";
  ot->description = "Select/Deselect files by walking through them";
  ot->idname = "FILE_OT_select_walk";

  /* API callbacks. */
  ot->invoke = file_walk_select_invoke;
  /* Operator works for file or asset browsing */
  ot->poll = ED_operator_file_active;

  /* properties */
  WM_operator_properties_select_walk_direction(ot);
  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         false,
                         "Extend",
                         "Extend selection instead of deselecting everything first");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "fill", false, "Fill", "Select everything beginning with the last selection");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_FILEBROWSER);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select All Operator
 * \{ */

static wmOperatorStatus file_select_all_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  FileSelection sel;
  const int numfiles = filelist_files_ensure(sfile->files);
  int action = RNA_enum_get(op->ptr, "action");

  if (action == SEL_TOGGLE) {
    action = file_is_any_selected(sfile->files) ? SEL_DESELECT : SEL_SELECT;
  }

  sel.first = 0;
  sel.last = numfiles - 1;

  FileCheckType check_type;
  FileSelType filesel_type;

  switch (action) {
    case SEL_SELECT:
    case SEL_INVERT: {
      check_type = (params->flag & FILE_DIRSEL_ONLY) ? CHECK_DIRS : CHECK_FILES;
      filesel_type = (action == SEL_INVERT) ? FILE_SEL_TOGGLE : FILE_SEL_ADD;
      break;
    }
    case SEL_DESELECT: {
      check_type = CHECK_ALL;
      filesel_type = FILE_SEL_REMOVE;
      break;
    }
    default: {
      BLI_assert(0);
      return OPERATOR_CANCELLED;
    }
  }

  filelist_entries_select_index_range_set(
      sfile->files, &sel, filesel_type, FILE_SEL_SELECTED, check_type);

  params->active_file = -1;
  if (action != SEL_DESELECT) {
    for (int i = 0; i < numfiles; i++) {
      if (filelist_entry_select_index_get(sfile->files, i, check_type)) {
        params->active_file = i;
        break;
      }
    }
  }

  file_draw_check(C);
  WM_event_add_mousemove(CTX_wm_window(C));
  ED_area_tag_redraw(area);

  return OPERATOR_FINISHED;
}

void FILE_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All Files";
  ot->description = "Select or deselect all files";
  ot->idname = "FILE_OT_select_all";

  /* API callbacks. */
  ot->exec = file_select_all_exec;
  /* Operator works for file or asset browsing */
  ot->poll = ED_operator_file_active;

  /* properties */
  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Selected Operator
 * \{ */

static wmOperatorStatus file_view_selected_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelection sel = file_current_selection_range_get(sfile->files);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (sel.first == -1 && sel.last == -1 && params->active_file == -1) {
    /* Nothing was selected. */
    return OPERATOR_CANCELLED;
  }

  /* Extend the selection area with the active file, as it may not be selected but still is
   * important to have in view. NOTE: active_file gets -1 after a search has been cleared/updated.
   */
  if (params->active_file != -1) {
    if (sel.first == -1 || params->active_file < sel.first) {
      sel.first = params->active_file;
    }
    if (sel.last == -1 || params->active_file > sel.last) {
      sel.last = params->active_file;
    }
  }

  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  file_ensure_selection_inside_viewbounds(region, sfile, &sel);

  file_draw_check(C);
  WM_event_add_mousemove(CTX_wm_window(C));
  ED_area_tag_redraw(area);

  return OPERATOR_FINISHED;
}

void FILE_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame Selected";
  ot->description = "Scroll the selected files into view";
  ot->idname = "FILE_OT_view_selected";

  /* API callbacks. */
  ot->exec = file_view_selected_exec;
  /* Operator works for file or asset browsing */
  ot->poll = ED_operator_file_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Bookmark Operator
 * \{ */

/* Note we could get rid of this one, but it's used by some addon so...
 * Does not hurt keeping it around for now. */
static wmOperatorStatus bookmark_select_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceFile *sfile = CTX_wm_space_file(C);

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "dir");
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  char entry[256];

  RNA_property_string_get(op->ptr, prop, entry);
  STRNCPY(params->dir, entry);
  BLI_path_abs(params->dir, BKE_main_blendfile_path(bmain));
  BLI_path_normalize_dir(params->dir, sizeof(params->dir));
  ED_file_change_dir(C);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);

  return OPERATOR_FINISHED;
}

void FILE_OT_select_bookmark(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Directory";
  ot->description = "Select a bookmarked directory";
  ot->idname = "FILE_OT_select_bookmark";

  /* API callbacks. */
  ot->exec = bookmark_select_exec;
  /* Bookmarks are for file browsing only (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active;

  /* properties */
  prop = RNA_def_string(ot->srna, "dir", nullptr, FILE_MAXDIR, "Directory", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Bookmark Operator
 * \{ */

static wmOperatorStatus bookmark_add_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FSMenu *fsmenu = ED_fsmenu_get();
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (params->dir[0] != '\0') {

    fsmenu_insert_entry(
        fsmenu, FS_CATEGORY_BOOKMARKS, params->dir, nullptr, ICON_FILE_FOLDER, FS_INSERT_SAVE);
    fsmenu_write_file_and_refresh_or_report_error(fsmenu, area, op->reports);
  }
  return OPERATOR_FINISHED;
}

void FILE_OT_bookmark_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Bookmark";
  ot->description = "Add a bookmark for the selected/active directory";
  ot->idname = "FILE_OT_bookmark_add";

  /* API callbacks. */
  ot->exec = bookmark_add_exec;
  /* Bookmarks are for file browsing only (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Bookmark Operator
 * \{ */

static wmOperatorStatus bookmark_delete_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FSMenu *fsmenu = ED_fsmenu_get();
  int nentries = ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_BOOKMARKS);

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "index");
  const int index = RNA_property_is_set(op->ptr, prop) ? RNA_property_int_get(op->ptr, prop) :
                                                         sfile->bookmarknr;
  if ((index > -1) && (index < nentries)) {
    fsmenu_remove_entry(fsmenu, FS_CATEGORY_BOOKMARKS, index);
    fsmenu_write_file_and_refresh_or_report_error(fsmenu, area, op->reports);
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

  /* API callbacks. */
  ot->exec = bookmark_delete_exec;
  /* Bookmarks are for file browsing only (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active;

  /* properties */
  prop = RNA_def_int(ot->srna, "index", -1, -1, 20000, "Index", "", -1, 20000);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cleanup Bookmark Operator
 * \{ */

static wmOperatorStatus bookmark_cleanup_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  FSMenu *fsmenu = ED_fsmenu_get();
  FSMenuEntry *fsme_next, *fsme = ED_fsmenu_get_category(fsmenu, FS_CATEGORY_BOOKMARKS);
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
    fsmenu_write_file_and_refresh_or_report_error(fsmenu, area, op->reports);
  }

  return OPERATOR_FINISHED;
}

void FILE_OT_bookmark_cleanup(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cleanup Bookmarks";
  ot->description = "Delete all invalid bookmarks";
  ot->idname = "FILE_OT_bookmark_cleanup";

  /* API callbacks. */
  ot->exec = bookmark_cleanup_exec;
  /* Bookmarks are for file browsing only (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active;

  /* properties */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reorder Bookmark Operator
 * \{ */

enum {
  FILE_BOOKMARK_MOVE_TOP = -2,
  FILE_BOOKMARK_MOVE_UP = -1,
  FILE_BOOKMARK_MOVE_DOWN = 1,
  FILE_BOOKMARK_MOVE_BOTTOM = 2,
};

static wmOperatorStatus bookmark_move_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FSMenu *fsmenu = ED_fsmenu_get();
  FSMenuEntry *fsmentry = ED_fsmenu_get_category(fsmenu, FS_CATEGORY_BOOKMARKS);
  const FSMenuEntry *fsmentry_org = fsmentry;

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

  fsmenu_write_file_and_refresh_or_report_error(fsmenu, area, op->reports);

  return OPERATOR_FINISHED;
}

static bool file_bookmark_move_poll(bContext *C)
{
  SpaceFile *sfile = CTX_wm_space_file(C);

  /* Bookmarks are for file browsing only (not asset browsing). */
  if (!ED_operator_file_browsing_active(C)) {
    return false;
  }

  return sfile->bookmarknr != -1;
}

void FILE_OT_bookmark_move(wmOperatorType *ot)
{
  static const EnumPropertyItem slot_move[] = {
      {FILE_BOOKMARK_MOVE_TOP, "TOP", 0, "Top", "Top of the list"},
      {FILE_BOOKMARK_MOVE_UP, "UP", 0, "Up", ""},
      {FILE_BOOKMARK_MOVE_DOWN, "DOWN", 0, "Down", ""},
      {FILE_BOOKMARK_MOVE_BOTTOM, "BOTTOM", 0, "Bottom", "Bottom of the list"},
      {0, nullptr, 0, nullptr, nullptr}};

  /* identifiers */
  ot->name = "Move Bookmark";
  ot->idname = "FILE_OT_bookmark_move";
  ot->description = "Move the active bookmark up/down in the list";

  /* API callbacks. */
  ot->exec = bookmark_move_exec;
  ot->poll = file_bookmark_move_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER; /* No undo! */

  RNA_def_enum(ot->srna,
               "direction",
               slot_move,
               0,
               "Direction",
               "Direction to move the active bookmark towards");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reset Recent Blend Files Operator
 * \{ */

static wmOperatorStatus reset_recent_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  FSMenu *fsmenu = ED_fsmenu_get();

  while (ED_fsmenu_get_entry(fsmenu, FS_CATEGORY_RECENT, 0) != nullptr) {
    fsmenu_remove_entry(fsmenu, FS_CATEGORY_RECENT, 0);
  }

  fsmenu_write_file_and_refresh_or_report_error(fsmenu, area, op->reports);

  return OPERATOR_FINISHED;
}

void FILE_OT_reset_recent(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Recent";
  ot->description = "Reset recent files";
  ot->idname = "FILE_OT_reset_recent";

  /* API callbacks. */
  ot->exec = reset_recent_exec;
  /* File browsing only operator (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Highlight File Operator
 * \{ */

int file_highlight_set(SpaceFile *sfile, ARegion *region, int mx, int my)
{
  View2D *v2d = &region->v2d;
  FileSelectParams *params;
  int numfiles, origfile;

  /* In case blender starts where the mouse is over a File browser,
   * this operator can be invoked when the `sfile` or `sfile->layout` isn't initialized yet. */
  if (sfile == nullptr || sfile->files == nullptr || sfile->layout == nullptr) {
    return 0;
  }

  params = ED_fileselect_get_active_params(sfile);
  /* In case #SpaceFile.browse_mode just changed, the area may be pending a refresh still, which is
   * what creates the params for the current browse mode. See #93508. */
  if (!params) {
    return false;
  }
  numfiles = filelist_files_ensure(sfile->files);

  origfile = params->highlight_file;

  mx -= region->winrct.xmin;
  my -= region->winrct.ymin;

  if (ED_fileselect_layout_is_inside_pt(sfile->layout, v2d, mx, my)) {
    float fx, fy;
    int highlight_file;

    UI_view2d_region_to_view(v2d, mx, my, &fx, &fy);

    highlight_file = ED_fileselect_layout_offset(
        sfile->layout, int(v2d->tot.xmin + fx), int(v2d->tot.ymax - fy));

    if ((highlight_file >= 0) && (highlight_file < numfiles)) {
      params->highlight_file = highlight_file;
    }
    else {
      params->highlight_file = -1;
    }
  }
  else {
    params->highlight_file = -1;
  }

  return (params->highlight_file != origfile);
}

static wmOperatorStatus file_highlight_invoke(bContext *C,
                                              wmOperator * /*op*/,
                                              const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceFile *sfile = CTX_wm_space_file(C);

  if (!file_highlight_set(sfile, region, event->xy[0], event->xy[1])) {
    return OPERATOR_PASS_THROUGH;
  }

  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_PASS_THROUGH;
}

void FILE_OT_highlight(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Highlight File";
  ot->description = "Highlight selected file(s)";
  ot->idname = "FILE_OT_highlight";

  /* API callbacks. */
  ot->invoke = file_highlight_invoke;
  /* Operator works for file or asset browsing */
  ot->poll = ED_operator_file_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sort from Column Operator
 * \{ */

static wmOperatorStatus file_column_sort_ui_context_invoke(bContext *C,
                                                           wmOperator * /*op*/,
                                                           const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);
  SpaceFile *sfile = CTX_wm_space_file(C);

  if (file_attribute_column_header_is_inside(
          &region->v2d, sfile->layout, event->mval[0], event->mval[1]))
  {
    FileSelectParams *params = ED_fileselect_get_active_params(sfile);
    const FileAttributeColumnType column_type = file_attribute_column_type_find_isect(
        &region->v2d, params, sfile->layout, event->mval[0]);

    if (column_type != COLUMN_NONE) {
      const FileAttributeColumn *column = &sfile->layout->attribute_columns[column_type];

      BLI_assert(column->sort_type != FILE_SORT_DEFAULT);
      if (params->sort == column->sort_type) {
        /* Already sorting by selected column -> toggle sort invert (three state logic). */
        params->flag ^= FILE_SORT_INVERT;
      }
      else {
        params->sort = column->sort_type;
        params->flag &= ~FILE_SORT_INVERT;
      }

      WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
    }
  }

  return OPERATOR_PASS_THROUGH;
}

void FILE_OT_sort_column_ui_context(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sort from Column";
  ot->description = "Change sorting to use column under cursor";
  ot->idname = "FILE_OT_sort_column_ui_context";

  /* API callbacks. */
  ot->invoke = file_column_sort_ui_context_invoke;
  /* Operator works for file or asset browsing */
  ot->poll = ED_operator_file_active;

  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cancel File Selector Operator
 * \{ */

static bool file_operator_poll(bContext *C)
{
  bool poll = ED_operator_file_browsing_active(C);
  SpaceFile *sfile = CTX_wm_space_file(C);

  if (!sfile || !sfile->op) {
    poll = false;
  }

  return poll;
}

static wmOperatorStatus file_cancel_exec(bContext *C, wmOperator * /*unused*/)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  wmOperator *op = sfile->op;

  sfile->op = nullptr;

  WM_event_fileselect_event(wm, op, EVT_FILESELECT_CANCEL);

  return OPERATOR_FINISHED;
}

void FILE_OT_cancel(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cancel File Operation";
  ot->description = "Cancel file operation";
  ot->idname = "FILE_OT_cancel";

  /* API callbacks. */
  ot->exec = file_cancel_exec;
  ot->poll = file_operator_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Utilities
 * \{ */

void file_sfile_to_operator_ex(
    bContext *C, Main *bmain, wmOperator *op, SpaceFile *sfile, char *filepath)
{
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  PropertyRNA *prop;
  char dir[FILE_MAX];

  BLI_strncpy(dir, params->dir, FILE_MAX);
  BLI_path_slash_ensure(dir, FILE_MAX);

  /* XXX, not real length */
  if (params->file[0]) {
    BLI_path_join(filepath, FILE_MAX, params->dir, params->file);
  }
  else {
    BLI_strncpy(filepath, dir, FILE_MAX);
  }

  if ((prop = RNA_struct_find_property(op->ptr, "relative_path"))) {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      BLI_path_rel(filepath, BKE_main_blendfile_path(bmain));
      BLI_path_rel(dir, BKE_main_blendfile_path(bmain));
    }
  }

  char value[FILE_MAX];
  if ((prop = RNA_struct_find_property(op->ptr, "filename"))) {
    RNA_property_string_get(op->ptr, prop, value);
    RNA_property_string_set(op->ptr, prop, params->file);
    if (RNA_property_update_check(prop) && !STREQ(params->file, value)) {
      RNA_property_update(C, op->ptr, prop);
    }
  }
  if ((prop = RNA_struct_find_property(op->ptr, "directory"))) {
    RNA_property_string_get(op->ptr, prop, value);
    RNA_property_string_set(op->ptr, prop, dir);
    if (RNA_property_update_check(prop) && !STREQ(dir, value)) {
      RNA_property_update(C, op->ptr, prop);
    }
  }
  if ((prop = RNA_struct_find_property(op->ptr, "filepath"))) {
    RNA_property_string_get(op->ptr, prop, value);
    RNA_property_string_set(op->ptr, prop, filepath);
    if (RNA_property_update_check(prop) && !STREQ(filepath, value)) {
      RNA_property_update(C, op->ptr, prop);
    }
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
          /* Cannot (currently) mix regular items and alias/shortcuts in multiple selection. */
          if (!file->redirection_path) {
            RNA_property_collection_add(op->ptr, prop, &itemptr);
            RNA_string_set(&itemptr, "name", file->relpath);
            num_files++;
          }
        }
      }
      /* make sure the file specified in the filename button is added even if no
       * files selected */
      if (0 == num_files) {
        RNA_property_collection_add(op->ptr, prop, &itemptr);
        RNA_string_set(&itemptr, "name", params->file);
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

      /* make sure the directory specified in the button is added even if no
       * directory selected */
      if (0 == num_dirs) {
        RNA_property_collection_add(op->ptr, prop, &itemptr);
        RNA_string_set(&itemptr, "name", params->dir);
      }
    }
  }
}
void file_sfile_to_operator(bContext *C, Main *bmain, wmOperator *op, SpaceFile *sfile)
{
  char filepath_dummy[FILE_MAX];

  file_sfile_to_operator_ex(C, bmain, op, sfile, filepath_dummy);
}

void file_operator_to_sfile(Main *bmain, SpaceFile *sfile, wmOperator *op)
{
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  PropertyRNA *prop;

  /* If neither of the above are set, split the filepath back */
  if ((prop = RNA_struct_find_property(op->ptr, "filepath"))) {
    char filepath[FILE_MAX];
    RNA_property_string_get(op->ptr, prop, filepath);
    BLI_path_split_dir_file(
        filepath, params->dir, sizeof(params->dir), params->file, sizeof(params->file));
  }
  else {
    if ((prop = RNA_struct_find_property(op->ptr, "filename"))) {
      RNA_property_string_get(op->ptr, prop, params->file);
    }
    if ((prop = RNA_struct_find_property(op->ptr, "directory"))) {
      RNA_property_string_get(op->ptr, prop, params->dir);
    }
  }

  /* we could check for relative_path property which is used when converting
   * in the other direction but doesn't hurt to do this every time */
  BLI_path_abs(params->dir, BKE_main_blendfile_path(bmain));

  /* XXX, files and dirs updates missing, not really so important though */
}

void file_sfile_filepath_set(SpaceFile *sfile, const char *filepath)
{
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  BLI_assert(BLI_exists(filepath));

  if (BLI_is_dir(filepath)) {
    STRNCPY(params->dir, filepath);
  }
  else {
    if ((params->flag & FILE_DIRSEL_ONLY) == 0) {
      BLI_path_split_dir_file(
          filepath, params->dir, sizeof(params->dir), params->file, sizeof(params->file));
    }
    else {
      BLI_path_split_dir_part(filepath, params->dir, sizeof(params->dir));
    }
  }
}

void file_draw_check_ex(bContext *C, ScrArea *area)
{
  /* May happen when manipulating non-active spaces. */
  if (UNLIKELY(area->spacetype != SPACE_FILE)) {
    return;
  }
  SpaceFile *sfile = static_cast<SpaceFile *>(area->spacedata.first);
  wmOperator *op = sfile->op;
  if (op) { /* fail on reload */
    if (op->type->check) {
      Main *bmain = CTX_data_main(C);
      file_sfile_to_operator(C, bmain, op, sfile);

      /* redraw */
      if (op->type->check(C, op)) {
        file_operator_to_sfile(bmain, sfile, op);

        /* redraw, else the changed settings won't get updated */
        ED_area_tag_redraw(area);
      }
    }
  }
}

void file_draw_check(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  file_draw_check_ex(C, area);
}

void file_draw_check_cb(bContext *C, void * /*arg1*/, void * /*arg2*/)
{
  file_draw_check(C);
}

bool file_draw_check_exists(SpaceFile *sfile)
{
  if (sfile->op) { /* fails on reload */
    const FileSelectParams *params = ED_fileselect_get_active_params(sfile);
    if (params && (params->flag & FILE_CHECK_EXISTING)) {
      char filepath[FILE_MAX];
      BLI_path_join(filepath, sizeof(filepath), params->dir, params->file);
      if (BLI_is_file(filepath)) {
        return true;
      }
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name External operations that can performed on files.
 * \{ */

static const EnumPropertyItem file_external_operation[] = {
    {FILE_EXTERNAL_OPERATION_OPEN, "OPEN", 0, "Open", "Open the file"},
    {FILE_EXTERNAL_OPERATION_FOLDER_OPEN, "FOLDER_OPEN", 0, "Open Folder", "Open the folder"},
    {FILE_EXTERNAL_OPERATION_EDIT, "EDIT", 0, "Edit", "Edit the file"},
    {FILE_EXTERNAL_OPERATION_NEW, "NEW", 0, "New", "Create a new file of this type"},
    {FILE_EXTERNAL_OPERATION_FIND, "FIND", 0, "Find File", "Search for files of this type"},
    {FILE_EXTERNAL_OPERATION_SHOW, "SHOW", 0, "Show", "Show this file"},
    {FILE_EXTERNAL_OPERATION_PLAY, "PLAY", 0, "Play", "Play this file"},
    {FILE_EXTERNAL_OPERATION_BROWSE, "BROWSE", 0, "Browse", "Browse this file"},
    {FILE_EXTERNAL_OPERATION_PREVIEW, "PREVIEW", 0, "Preview", "Preview this file"},
    {FILE_EXTERNAL_OPERATION_PRINT, "PRINT", 0, "Print", "Print this file"},
    {FILE_EXTERNAL_OPERATION_INSTALL, "INSTALL", 0, "Install", "Install this file"},
    {FILE_EXTERNAL_OPERATION_RUNAS, "RUNAS", 0, "Run As User", "Run as specific user"},
    {FILE_EXTERNAL_OPERATION_PROPERTIES,
     "PROPERTIES",
     0,
     "Properties",
     "Show OS Properties for this item"},
    {FILE_EXTERNAL_OPERATION_FOLDER_FIND,
     "FOLDER_FIND",
     0,
     "Find in Folder",
     "Search for items in this folder"},
    {FILE_EXTERNAL_OPERATION_FOLDER_CMD,
     "CMD",
     0,
     "Command Prompt Here",
     "Open a command prompt here"},
    {0, nullptr, 0, nullptr, nullptr}};

static wmOperatorStatus file_external_operation_exec(bContext *C, wmOperator *op)
{
  if (!ED_operator_file_browsing_active(C)) {
    /* File browsing only operator (not asset browsing). */
    return OPERATOR_CANCELLED;
  }

  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  if (!sfile || !params) {
    return OPERATOR_CANCELLED;
  }
  char dir[FILE_MAX_LIBEXTRA];
  if (filelist_islibrary(sfile->files, dir, nullptr)) {
    return OPERATOR_CANCELLED;
  }
  int numfiles = filelist_files_ensure(sfile->files);
  FileDirEntry *fileentry = nullptr;
  int num_selected = 0;
  for (int i = 0; i < numfiles; i++) {
    if (filelist_entry_select_index_get(sfile->files, i, CHECK_ALL)) {
      fileentry = filelist_file(sfile->files, i);
      num_selected++;
    }
  }
  if (!fileentry || num_selected > 1) {
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX_LIBEXTRA];
  filelist_file_get_full_path(sfile->files, fileentry, filepath);

  WM_cursor_set(CTX_wm_window(C), WM_CURSOR_WAIT);

#ifdef WIN32
  const FileExternalOperation operation = (FileExternalOperation)RNA_enum_get(op->ptr,
                                                                              "operation");

  if (!(fileentry->typeflag & FILE_TYPE_DIR) &&
      ELEM(operation, FILE_EXTERNAL_OPERATION_FOLDER_OPEN, FILE_EXTERNAL_OPERATION_FOLDER_CMD))
  {
    /* Not a folder path, so for these operations use the root. */
    const char *root = filelist_dir(sfile->files);
    if (BLI_file_external_operation_execute(root, operation)) {
      WM_cursor_set(CTX_wm_window(C), WM_CURSOR_DEFAULT);
      return OPERATOR_FINISHED;
    }
  }
  if (BLI_file_external_operation_execute(filepath, operation)) {
    WM_cursor_set(CTX_wm_window(C), WM_CURSOR_DEFAULT);
    return OPERATOR_FINISHED;
  }
#else
  wmOperatorType *ot = WM_operatortype_find("WM_OT_path_open", true);
  PointerRNA op_props;
  WM_operator_properties_create_ptr(&op_props, ot);
  RNA_string_set(&op_props, "filepath", filepath);
  const wmOperatorStatus retval = WM_operator_name_call_ptr(
      C, ot, blender::wm::OpCallContext::InvokeDefault, &op_props, nullptr);
  WM_operator_properties_free(&op_props);

  if (retval == OPERATOR_FINISHED) {
    WM_cursor_set(CTX_wm_window(C), WM_CURSOR_DEFAULT);
    return OPERATOR_FINISHED;
  }
#endif

  BKE_reportf(
      op->reports, RPT_ERROR, "Failure to perform external file operation on \"%s\"", filepath);
  WM_cursor_set(CTX_wm_window(C), WM_CURSOR_DEFAULT);
  return OPERATOR_CANCELLED;
}

static std::string file_external_operation_get_description(bContext * /*C*/,
                                                           wmOperatorType * /*ot*/,
                                                           PointerRNA *ptr)
{
  const char *description = "";
  RNA_enum_description(file_external_operation, RNA_enum_get(ptr, "operation"), &description);
  return TIP_(description);
}

void FILE_OT_external_operation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "External File Operation";
  ot->idname = "FILE_OT_external_operation";
  ot->description = "Perform external operation on a file or folder";

  /* API callbacks. */
  ot->exec = file_external_operation_exec;
  ot->get_description = file_external_operation_get_description;

  /* flags */
  ot->flag = OPTYPE_REGISTER; /* No undo! */

  /* properties */
  RNA_def_enum(ot->srna,
               "operation",
               file_external_operation,
               FILE_EXTERNAL_OPERATION_OPEN,
               "Operation",
               "Operation to perform on the selected file or path");
}

static void file_os_operations_menu_item(uiLayout *layout,
                                         wmOperatorType *ot,
                                         const char *path,
                                         FileExternalOperation operation)
{
#ifdef WIN32
  if (!BLI_file_external_operation_supported(path, operation)) {
    return;
  }
#else
  UNUSED_VARS(path);
  if (!ELEM(operation, FILE_EXTERNAL_OPERATION_OPEN, FILE_EXTERNAL_OPERATION_FOLDER_OPEN)) {
    return;
  }
#endif

  const char *title = "";
  RNA_enum_name(file_external_operation, operation, &title);

  PointerRNA props_ptr = layout->op(
      ot, IFACE_(title), ICON_NONE, blender::wm::OpCallContext::InvokeDefault, UI_ITEM_NONE);
  RNA_enum_set(&props_ptr, "operation", operation);
}

static void file_os_operations_menu_draw(const bContext *C_const, Menu *menu)
{
  bContext *C = (bContext *)C_const;

  /* File browsing only operator (not asset browsing). */
  if (!ED_operator_file_browsing_active(C)) {
    return;
  }

  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  if (!sfile || !params) {
    return;
  }

  char dir[FILE_MAX_LIBEXTRA];
  if (filelist_islibrary(sfile->files, dir, nullptr)) {
    return;
  }

  int numfiles = filelist_files_ensure(sfile->files);
  FileDirEntry *fileentry = nullptr;
  int num_selected = 0;

  for (int i = 0; i < numfiles; i++) {
    if (filelist_entry_select_index_get(sfile->files, i, CHECK_ALL)) {
      fileentry = filelist_file(sfile->files, i);
      num_selected++;
    }
  }

  if (!fileentry || num_selected > 1) {
    return;
  }

  char path[FILE_MAX_LIBEXTRA];
  filelist_file_get_full_path(sfile->files, fileentry, path);
  const char *root = filelist_dir(sfile->files);

  uiLayout *layout = menu->layout;
  layout->operator_context_set(blender::wm::OpCallContext::InvokeDefault);
  wmOperatorType *ot = WM_operatortype_find("FILE_OT_external_operation", true);

  if (fileentry->typeflag & FILE_TYPE_DIR) {
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_FOLDER_OPEN);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_FOLDER_CMD);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_PROPERTIES);
  }
  else {
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_OPEN);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_EDIT);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_NEW);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_FIND);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_SHOW);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_PLAY);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_BROWSE);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_PREVIEW);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_PRINT);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_INSTALL);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_RUNAS);
    file_os_operations_menu_item(layout, ot, root, FILE_EXTERNAL_OPERATION_FOLDER_OPEN);
    file_os_operations_menu_item(layout, ot, root, FILE_EXTERNAL_OPERATION_FOLDER_CMD);
    file_os_operations_menu_item(layout, ot, path, FILE_EXTERNAL_OPERATION_PROPERTIES);
  }
}

static bool file_os_operations_menu_poll(const bContext *C_const, MenuType * /*mt*/)
{
  bContext *C = (bContext *)C_const;

  /* File browsing only operator (not asset browsing). */
  if (!ED_operator_file_browsing_active(C)) {
    return false;
  }

  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (sfile && params) {
    char dir[FILE_MAX_LIBEXTRA];
    if (filelist_islibrary(sfile->files, dir, nullptr)) {
      return false;
    }

    int numfiles = filelist_files_ensure(sfile->files);
    int num_selected = 0;
    for (int i = 0; i < numfiles; i++) {
      if (filelist_entry_select_index_get(sfile->files, i, CHECK_ALL)) {
        num_selected++;
      }
    }

    if (num_selected > 1) {
      CTX_wm_operator_poll_msg_set(C, "More than one item is selected");
    }
    else if (num_selected < 1) {
      CTX_wm_operator_poll_msg_set(C, "No items are selected");
    }
    else {
      return true;
    }
  }

  return false;
}

void file_external_operations_menu_register()
{
  MenuType *mt;

  mt = MEM_callocN<MenuType>("spacetype file menu file operations");
  STRNCPY_UTF8(mt->idname, "FILEBROWSER_MT_operations_menu");
  STRNCPY_UTF8(mt->label, N_("External"));
  STRNCPY_UTF8(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  mt->draw = file_os_operations_menu_draw;
  mt->poll = file_os_operations_menu_poll;
  WM_menutype_add(mt);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Execute File Window Operator
 * \{ */

/**
 * Execute the active file, as set in the file select params.
 */
static bool file_execute(bContext *C, SpaceFile *sfile)
{
  Main *bmain = CTX_data_main(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  FileDirEntry *file = filelist_file(sfile->files, params->active_file);

  if (file && file->redirection_path) {
    /* redirection_path is an absolute path that takes precedence
     * over using params->dir + params->file. */
    BLI_path_split_dir_file(file->redirection_path,
                            params->dir,
                            sizeof(params->dir),
                            params->file,
                            sizeof(params->file));
    /* Update relpath with redirected filename as well so that the alternative
     * combination of params->dir + relpath remains valid as well. */
    MEM_freeN(file->relpath);
    file->relpath = BLI_strdup(params->file);
  }

  /* directory change */
  if (file && (file->typeflag & FILE_TYPE_DIR)) {
    if (!file->relpath) {
      return false;
    }

    if (FILENAME_IS_PARENT(file->relpath)) {
      BLI_path_parent_dir(params->dir);
    }
    else {
      BLI_path_abs(params->dir, BKE_main_blendfile_path(bmain));
      BLI_path_normalize_native(params->dir);
      BLI_path_append_dir(params->dir, sizeof(params->dir), file->relpath);
    }
    ED_file_change_dir(C);
  }
  /* Opening file, sends events now, so things get handled on window-queue level. */
  else if (sfile->op) {
    ScrArea *area = CTX_wm_area(C);
    FSMenu *fsmenu = ED_fsmenu_get();
    wmOperator *op = sfile->op;
    char filepath[FILE_MAX];

    sfile->op = nullptr;

    file_sfile_to_operator_ex(C, bmain, op, sfile, filepath);

    if (BLI_exists(params->dir)) {
      fsmenu_insert_entry(fsmenu,
                          FS_CATEGORY_RECENT,
                          params->dir,
                          nullptr,
                          ICON_FILE_FOLDER,
                          FSMenuInsert(FS_INSERT_SAVE | FS_INSERT_FIRST));
    }

    fsmenu_write_file_and_refresh_or_report_error(fsmenu, area, op->reports);

    WM_event_fileselect_event(CTX_wm_manager(C), op, EVT_FILESELECT_EXEC);
  }

  return true;
}

static wmOperatorStatus file_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceFile *sfile = CTX_wm_space_file(C);

  if (!file_execute(C, sfile)) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static std::string file_execute_get_description(bContext *C,
                                                wmOperatorType * /*ot*/,
                                                PointerRNA * /*ptr*/)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  if (sfile->op && sfile->op->type && sfile->op->type->description) {
    /* Return the description of the executed operator. Don't use get_description
     * as that will return file details for #WM_OT_open_mainfile. */
    return TIP_(sfile->op->type->description);
  }
  return {};
}

void FILE_OT_execute(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Execute File Window";
  ot->description = "Execute selected file";
  ot->idname = "FILE_OT_execute";
  ot->get_description = file_execute_get_description;

  /* API callbacks. */
  ot->exec = file_exec;
  /* Important since handler is on window level.
   *
   * Avoid using #file_operator_poll since this is also used for entering directories
   * which is used even when the file manager doesn't have an operator. */
  ot->poll = ED_operator_file_browsing_active;
}

/**
 * \returns false if the mouse doesn't hover a selectable item.
 */
static bool file_ensure_hovered_is_active(bContext *C, const wmEvent *event)
{
  rcti rect = file_select_mval_to_select_rect(event->mval);
  if (file_select(C, &rect, FILE_SEL_ADD, false, false) == FILE_SELECT_NOTHING) {
    return false;
  }

  return true;
}

static wmOperatorStatus file_execute_mouse_invoke(bContext *C,
                                                  wmOperator * /*op*/,
                                                  const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceFile *sfile = CTX_wm_space_file(C);

  if (!ED_fileselect_layout_is_inside_pt(
          sfile->layout, &region->v2d, event->mval[0], event->mval[1]))
  {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  /* Note that this isn't needed practically, because the keymap already activates the hovered item
   * on mouse-press. This execute operator is called afterwards on the double-click event then.
   * However relying on this would be fragile and could break with keymap changes, so better to
   * have this mouse-execute operator that makes sure once more that the hovered file is active. */
  if (!file_ensure_hovered_is_active(C, event)) {
    return OPERATOR_CANCELLED;
  }

  if (!file_execute(C, sfile)) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void FILE_OT_mouse_execute(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Execute File";
  ot->description =
      "Perform the current execute action for the file under the cursor (e.g. open the file)";
  ot->idname = "FILE_OT_mouse_execute";

  /* API callbacks. */
  ot->invoke = file_execute_mouse_invoke;
  ot->poll = ED_operator_file_browsing_active;

  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Refresh File List Operator
 * \{ */

static wmOperatorStatus file_refresh_exec(bContext *C, wmOperator * /*unused*/)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FSMenu *fsmenu = ED_fsmenu_get();

  ED_fileselect_clear(wm, sfile);

  /* refresh system directory menu */
  fsmenu_refresh_system_category(fsmenu);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);

  return OPERATOR_FINISHED;
}

void FILE_OT_refresh(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Refresh File List";
  ot->description = "Refresh the file list";
  ot->idname = "FILE_OT_refresh";

  /* API callbacks. */
  ot->exec = file_refresh_exec;
  ot->poll = ED_operator_file_browsing_active; /* <- important, handler is on window level */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Navigate Parent Operator
 * \{ */

static wmOperatorStatus file_parent_exec(bContext *C, wmOperator * /*unused*/)
{
  Main *bmain = CTX_data_main(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (params) {
    if (BLI_path_parent_dir(params->dir)) {
      BLI_path_abs(params->dir, BKE_main_blendfile_path(bmain));
      BLI_path_normalize_dir(params->dir, sizeof(params->dir));
      ED_file_change_dir(C);
      if (params->recursion_level > 1) {
        /* Disable `dirtree` recursion when going up in tree. */
        params->recursion_level = 0;
        filelist_setrecursion(sfile->files, params->recursion_level);
      }
      WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
    }
  }

  return OPERATOR_FINISHED;
}

void FILE_OT_parent(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Parent Directory";
  ot->description = "Move to parent directory";
  ot->idname = "FILE_OT_parent";

  /* API callbacks. */
  ot->exec = file_parent_exec;
  /* File browsing only operator (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active; /* <- important, handler is on window level */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Navigate Previous Operator
 * \{ */

static wmOperatorStatus file_previous_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (params) {
    folderlist_pushdir(sfile->folders_next, params->dir);
    folderlist_popdir(sfile->folders_prev, params->dir);
    folderlist_pushdir(sfile->folders_next, params->dir);

    ED_file_change_dir(C);
  }
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);

  return OPERATOR_FINISHED;
}

void FILE_OT_previous(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Previous Folder";
  ot->description = "Move to previous folder";
  ot->idname = "FILE_OT_previous";

  /* API callbacks. */
  ot->exec = file_previous_exec;
  /* File browsing only operator (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active; /* <- important, handler is on window level */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Navigate Next Operator
 * \{ */

static wmOperatorStatus file_next_exec(bContext *C, wmOperator * /*unused*/)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  if (params) {
    folderlist_pushdir(sfile->folders_prev, params->dir);
    folderlist_popdir(sfile->folders_next, params->dir);

    /* update folders_prev so we can check for it in #folderlist_clear_next() */
    folderlist_pushdir(sfile->folders_prev, params->dir);

    ED_file_change_dir(C);
  }
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);

  return OPERATOR_FINISHED;
}

void FILE_OT_next(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Next Folder";
  ot->description = "Move to next folder";
  ot->idname = "FILE_OT_next";

  /* API callbacks. */
  ot->exec = file_next_exec;
  /* File browsing only operator (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active; /* <- important, handler is on window level */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth Scroll Operator
 * \{ */

/* only meant for timer usage */
static wmOperatorStatus file_smoothscroll_invoke(bContext *C,
                                                 wmOperator * /*op*/,
                                                 const wmEvent *event)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  ARegion *region, *region_ctx = CTX_wm_region(C);
  const bool is_horizontal = (sfile->layout->flag & FILE_LAYOUT_HOR) != 0;
  int i;

  /* escape if not our timer */
  if (sfile->smoothscroll_timer == nullptr || sfile->smoothscroll_timer != event->customdata) {
    return OPERATOR_PASS_THROUGH;
  }

  const int numfiles = filelist_files_ensure(sfile->files);

  /* Due to asynchronous nature of file listing, we may execute this code before `file_refresh()`
   * editing entry is available in our listing,
   * so we also have to handle switching to rename mode here. */
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  if ((params->rename_flag &
       (FILE_PARAMS_RENAME_PENDING | FILE_PARAMS_RENAME_POSTSCROLL_PENDING)) != 0)
  {
    file_params_renamefile_activate(sfile, params);
  }

  /* check if we are editing a name */
  int edit_idx = -1;
  for (i = 0; i < numfiles; i++) {
    if (filelist_entry_select_index_get(sfile->files, i, CHECK_ALL) &
        (FILE_SEL_EDITING | FILE_SEL_HIGHLIGHTED))
    {
      edit_idx = i;
      break;
    }
  }

  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);

  /* if we are not editing, we are done */
  if (edit_idx == -1) {
    /* Do not invalidate timer if file-rename is still pending,
     * we might still be building the filelist and yet have to find edited entry. */
    if (params->rename_flag == 0) {
      file_params_smoothscroll_timer_clear(wm, win, sfile);
    }
    return OPERATOR_PASS_THROUGH;
  }

  /* we need the correct area for scrolling */
  region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  if (!region || region->regiontype != RGN_TYPE_WINDOW) {
    file_params_smoothscroll_timer_clear(wm, win, sfile);
    return OPERATOR_PASS_THROUGH;
  }

  /* Number of items in a block (i.e. lines in a column in horizontal layout, or columns in a line
   * in vertical layout).
   */
  const int items_block_size = is_horizontal ? sfile->layout->rows : sfile->layout->flow_columns;

  /* Scroll offset is the first file in the row/column we are editing in. */
  if (sfile->scroll_offset == 0) {
    sfile->scroll_offset = (edit_idx / items_block_size) * items_block_size;
  }

  const int numfiles_layout = ED_fileselect_layout_numfiles(sfile->layout, region);
  const int first_visible_item = ED_fileselect_layout_offset(
      sfile->layout, int(region->v2d.cur.xmin), int(-region->v2d.cur.ymax));
  const int last_visible_item = first_visible_item + numfiles_layout + 1;

  /* NOTE: the special case for vertical layout is because filename is at the bottom of items then,
   * so we artificially move current row back one step, to ensure we show bottom of
   * active item rather than its top (important in case visible height is low). */
  const int middle_offset = max_ii(
      0, (first_visible_item + last_visible_item) / 2 - (is_horizontal ? 0 : items_block_size));

  const int min_middle_offset = numfiles_layout / 2;
  const int max_middle_offset = ((numfiles / items_block_size) * items_block_size +
                                 ((numfiles % items_block_size) != 0 ? items_block_size : 0)) -
                                (numfiles_layout / 2);
  /* Actual (physical) scrolling info, in pixels, used to detect whether we are fully at the
   * beginning/end of the view. */
  /* Note that there is a weird glitch, that sometimes tot rctf is smaller than cur rctf...
   * that is why we still need to keep the min/max_middle_offset checks too. :( */
  const float min_tot_scroll = is_horizontal ? region->v2d.tot.xmin : -region->v2d.tot.ymax;
  const float max_tot_scroll = is_horizontal ? region->v2d.tot.xmax : -region->v2d.tot.ymin;
  const float min_curr_scroll = is_horizontal ? region->v2d.cur.xmin : -region->v2d.cur.ymax;
  const float max_curr_scroll = is_horizontal ? region->v2d.cur.xmax : -region->v2d.cur.ymin;

  /* Check if we have reached our final scroll position. */
  /* File-list has to be ready, otherwise it makes no sense to stop scrolling yet. */
  const bool is_ready = filelist_is_ready(sfile->files);
  /* Edited item must be in the 'middle' of shown area (kind of approximated).
   * Note that we have to do the check in 'block space', not in 'item space' here. */
  const bool is_centered = (abs(middle_offset / items_block_size -
                                sfile->scroll_offset / items_block_size) == 0);
  /* OR edited item must be towards the beginning, and we are scrolled fully to the start. */
  const bool is_full_start = ((sfile->scroll_offset < min_middle_offset) &&
                              (min_curr_scroll - min_tot_scroll < 1.0f) &&
                              (middle_offset - min_middle_offset < items_block_size));
  /* OR edited item must be towards the end, and we are scrolled fully to the end.
   * This one is crucial (unlike the one for the beginning), because without it scrolling
   * fully to the end, and last column or row will end up only partially drawn. */
  const bool is_full_end = ((sfile->scroll_offset > max_middle_offset) &&
                            (max_tot_scroll - max_curr_scroll < 1.0f) &&
                            (max_middle_offset - middle_offset < items_block_size));

  if (is_ready && (is_centered || is_full_start || is_full_end)) {
    file_params_smoothscroll_timer_clear(wm, win, sfile);
    /* Post-scroll (after rename has been validated by user) is done,
     * rename process is totally finished, cleanup. */
    if ((params->rename_flag & FILE_PARAMS_RENAME_POSTSCROLL_ACTIVE) != 0) {
      file_params_renamefile_clear(params);
    }
    return OPERATOR_FINISHED;
  }

  /* Temporarily set context to the main window region,
   * so that the pan operator works. */
  CTX_wm_region_set(C, region);

  /* scroll one step in the desired direction */
  PointerRNA op_ptr;
  int deltax = 0;
  int deltay = 0;

  /* We adjust speed of scrolling to avoid tens of seconds of it in e.g. directories with tens of
   * thousands of folders... See #65782. */
  /* This will slow down scrolling when approaching final goal, also avoids going too far and
   * having to bounce back... */

  /* Number of blocks (columns in horizontal layout, rows otherwise) between current middle of
   * screen, and final goal position. */
  const int diff_offset = sfile->scroll_offset / items_block_size -
                          middle_offset / items_block_size;
  /* convert diff_offset into pixels. */
  const int diff_offset_delta = abs(diff_offset) *
                                (is_horizontal ?
                                     sfile->layout->tile_w + 2 * sfile->layout->tile_border_x :
                                     sfile->layout->tile_h + 2 * sfile->layout->tile_border_y);
  const int scroll_delta = max_ii(2, diff_offset_delta / 15);

  if (diff_offset < 0) {
    if (is_horizontal) {
      deltax = -scroll_delta;
    }
    else {
      deltay = scroll_delta;
    }
  }
  else {
    if (is_horizontal) {
      deltax = scroll_delta;
    }
    else {
      deltay = -scroll_delta;
    }
  }
  WM_operator_properties_create(&op_ptr, "VIEW2D_OT_pan");
  RNA_int_set(&op_ptr, "deltax", deltax);
  RNA_int_set(&op_ptr, "deltay", deltay);

  WM_operator_name_call(
      C, "VIEW2D_OT_pan", blender::wm::OpCallContext::ExecDefault, &op_ptr, event);
  WM_operator_properties_free(&op_ptr);

  ED_region_tag_redraw(region);

  /* and restore context */
  CTX_wm_region_set(C, region_ctx);

  return OPERATOR_FINISHED;
}

void FILE_OT_smoothscroll(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth Scroll";
  ot->idname = "FILE_OT_smoothscroll";
  ot->description = "Smooth scroll to make editable file visible";

  /* API callbacks. */
  ot->invoke = file_smoothscroll_invoke;
  /* Operator works for file or asset browsing */
  ot->poll = ED_operator_file_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Selector Drop Operator
 * \{ */

static wmOperatorStatus filepath_drop_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
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
      file_sfile_to_operator(C, bmain, sfile->op, sfile);
      file_draw_check(C);
    }

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void FILE_OT_filepath_drop(wmOperatorType *ot)
{
  ot->name = "File Selector Drop";
  ot->idname = "FILE_OT_filepath_drop";

  ot->exec = filepath_drop_exec;
  /* File browsing only operator (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active;

  RNA_def_string_file_path(ot->srna, "filepath", "Path", FILE_MAX, "", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Directory Operator
 * \{ */

/**
 * Create a new, non-existing folder `dirname`, returns true if successful,
 * false if name couldn't be created.
 * The actual name is returned in `r_dirpath`, `r_dirpath_full` contains the complete path,
 * including the new folder name.
 */
static bool new_folder_path(const char *parent,
                            char r_dirpath_full[FILE_MAX],
                            char r_dirname[FILE_MAXFILE])
{
  int i = 1;
  int len = 0;

  BLI_strncpy(r_dirname, "New Folder", FILE_MAXFILE);
  BLI_path_join(r_dirpath_full, FILE_MAX, parent, r_dirname);
  /* check whether r_dirpath_full with the name already exists, in this case
   * add number to the name. Check length of generated name to avoid
   * crazy case of huge number of folders each named 'New Folder (x)' */
  while (BLI_exists(r_dirpath_full) && (len < FILE_MAXFILE)) {
    len = BLI_snprintf_utf8(r_dirname, FILE_MAXFILE, "New Folder(%d)", i);
    BLI_path_join(r_dirpath_full, FILE_MAX, parent, r_dirname);
    i++;
  }

  return (len < FILE_MAXFILE);
}

static wmOperatorStatus file_directory_new_exec(bContext *C, wmOperator *op)
{
  char dirname[FILE_MAXFILE];
  char dirpath[FILE_MAX];
  bool generate_name = true;

  wmWindowManager *wm = CTX_wm_manager(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  const bool do_diropen = RNA_boolean_get(op->ptr, "open");

  if (!params) {
    BKE_report(op->reports, RPT_WARNING, "No parent directory given");
    return OPERATOR_CANCELLED;
  }

  dirpath[0] = '\0';

  {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "directory");
    RNA_property_string_get(op->ptr, prop, dirpath);
    if (dirpath[0] != '\0') {
      generate_name = false;
    }
  }

  if (generate_name) {
    /* create a new, non-existing folder name */
    if (!new_folder_path(params->dir, dirpath, dirname)) {
      BKE_report(op->reports, RPT_ERROR, "Could not create new folder name");
      return OPERATOR_CANCELLED;
    }
  }
  else { /* We assume we are able to generate a valid name! */
    char org_path[FILE_MAX];

    STRNCPY(org_path, dirpath);
    if (BLI_path_make_safe(dirpath)) {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "'%s' given path is OS-invalid, creating '%s' path instead",
                  org_path,
                  dirpath);
    }
  }

  /* create the file */
  errno = 0;
  if (!BLI_dir_create_recursive(dirpath) ||
      /* Should no more be needed,
       * now that BLI_dir_create_recursive returns a success state - but kept just in case. */
      !BLI_exists(dirpath))
  {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Could not create new folder: %s",
                errno ? strerror(errno) : "unknown error");
    return OPERATOR_CANCELLED;
  }

  eFileSel_Params_RenameFlag rename_flag = eFileSel_Params_RenameFlag(params->rename_flag);

  /* If we don't enter the directory directly, remember file to jump into editing. */
  if (do_diropen == false) {
    BLI_assert_msg(params->rename_id == nullptr,
                   "File rename handling should immediately clear rename_id when done, "
                   "because otherwise it will keep taking precedence over renamefile.");
    STRNCPY(params->renamefile, dirname);
    rename_flag = FILE_PARAMS_RENAME_PENDING;
  }

  file_params_invoke_rename_postscroll(wm, CTX_wm_window(C), sfile);
  params->rename_flag = rename_flag;

  /* reload dir to make sure we're seeing what's in the directory */
  ED_fileselect_clear(wm, sfile);

  if (do_diropen) {
    STRNCPY(params->dir, dirpath);
    ED_file_change_dir(C);
  }

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus file_directory_new_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent * /*event*/)
{
  /* NOTE: confirm is needed because this operator is invoked
   * when entering a path from the file selector. Without a confirmation,
   * a typo will create the path without any prompt. See #128567. */
  if (RNA_boolean_get(op->ptr, "confirm")) {
    return WM_operator_confirm_ex(
        C, op, IFACE_("Create new directory?"), nullptr, IFACE_("Create"), ALERT_ICON_NONE, false);
  }
  return file_directory_new_exec(C, op);
}

void FILE_OT_directory_new(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Create New Directory";
  ot->description = "Create a new directory";
  ot->idname = "FILE_OT_directory_new";

  /* API callbacks. */
  ot->invoke = file_directory_new_invoke;
  ot->exec = file_directory_new_exec;
  /* File browsing only operator (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active; /* <- important, handler is on window level */

  prop = RNA_def_string_dir_path(
      ot->srna, "directory", nullptr, FILE_MAX, "Directory", "Name of new directory");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "open", false, "Open", "Open new directory");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  WM_operator_properties_confirm_or_exec(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Refresh File List Operator
 * \{ */

static void file_expand_directory(const Main *bmain, FileSelectParams *params)
{
  /* NOTE: Arbitrary conventions are used here which are OK for user facing logic
   * Attempting to resolve the path to *something* valid is OK for user input
   * but not suitable for `BLI_path_utils.hh` which is used for lower level path handling. */

  /* The path was invalid, fall back to the default root. */
  bool do_reset = false;

  if (params->dir[0] == '\0') {
    do_reset = true;
  }
  else if (BLI_path_is_rel(params->dir)) { /* `//` literal. */
    const char *blendfile_path = BKE_main_blendfile_path(bmain);
    if (blendfile_path[0] != '\0') {
      BLI_path_abs(params->dir, blendfile_path);
    }
    else {
      /* Ignore relative paths for unsaved files.
       * It's enough of a corner case that any attempt to resolve the path
       * is more likely to confuse users about the meaning of `//`. */
      do_reset = true;
    }
  }
  else if (params->dir[0] == '~') {
    /* While path handling expansion typically doesn't support home directory expansion
     * in Blender, this is a convenience to be able to type in a single character.
     * Even though this is a UNIX convention, it's harmless to expand on WIN32 as well. */
    if (const char *home_dir = BLI_dir_home()) {
      char tmpstr[sizeof(params->dir) - 1];
      STRNCPY(tmpstr, params->dir + 1);
      BLI_path_join(params->dir, sizeof(params->dir), home_dir, tmpstr);
    }
    else {
      do_reset = true;
    }
  }
#ifdef WIN32
  else if (BLI_path_is_win32_drive_only(params->dir)) {
    /* Change `C:` --> `C:\`, see: #28102. */
    params->dir[2] = SEP;
    params->dir[3] = '\0';
  }
  else if (BLI_path_is_unc(params->dir)) {
    BLI_path_normalize_unc(params->dir, sizeof(params->dir));
  }
#endif /* WIN32 */

  if (do_reset) {
    STRNCPY(params->dir, BKE_appdir_folder_root());
  }
}

/**
 * \param dir: A normalized path (from user input).
 * \return True when `dir` can be created.
 */
static bool can_create_dir_from_user_input(const char dir[FILE_MAX_LIBEXTRA])
{
  /* NOTE(@ideasman42): This function checks if the user should be prompted
   * to create the path when a non-existent path is entered into the directory field.
   *
   * Typically when the intention is to create a path, it's simply created only showing
   * feedback if the operation failed. In the case of entering a path as text any typo
   * in the would immediately be created (if possible) which isn't good, see: #128567.
   *
   * The reason to treat user input differently here is the user could input anything,
   * e.g. values such as a single space. This resolves to the current-working-directory:
   * `$PWD/ ` which is a valid path name and could be created
   * (this was in fact the behavior until v4.4).
   *
   * As this can be error prone, performs basic sanity checks on `dir`.
   * - Ensure it's an absolute path.
   * - It contains a writable parent path.
   * - For WIN32: The UNC path is a directory and not a "share".
   *
   * While these checks don't need to be comprehensive,
   * they should prevent accidents and confusing situations. */

  /* If the user types in a name with no absolute prefix,
   * creating a directory relative to the CWD doesn't make sense from the UI. */
  if (!BLI_path_is_abs_from_cwd(dir)) {
    return false;
  }

  /* If none of the parents exist, the directory can't be created.
   * This prevents the popup to create a new path showing on WIN32
   * for a drive-letter that doesn't exist (for example). */
  {
    char tdir[FILE_MAX_LIBEXTRA];
    STRNCPY(tdir, dir);
    if (!BLI_path_parent_dir_until_exists(tdir)) {
      return false;
    }
  }

#if defined(WIN32)
  /* For UNC paths we need to check whether the parent of the new
   * directory is a proper directory itself and not a share or the
   * UNC root (server name) itself. Calling #BLI_is_dir does this. */
  if (BLI_path_is_unc(dir)) {
    char tdir[FILE_MAX_LIBEXTRA];
    STRNCPY(tdir, dir);
    BLI_path_parent_dir(tdir);
    if (!BLI_is_dir(tdir)) {
      return false;
    }
  }
#endif /* WIN32 */

  return true;
}

void file_directory_enter_handle(bContext *C, void * /*arg_unused*/, void * /*arg_but*/)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  if (UNLIKELY(params == nullptr)) {
    return;
  }

  const Main *bmain = CTX_data_main(C);
  char old_dir[sizeof(params->dir)];

  STRNCPY(old_dir, params->dir);

  file_expand_directory(bmain, params);

  /* Special case, user may have pasted a path including a file-name into the directory. */
  if (!filelist_is_dir(sfile->files, params->dir)) {
    char tdir[FILE_MAX_LIBEXTRA];
    char *group, *name;

    if (BLI_is_file(params->dir)) {
      char dirpath[sizeof(params->dir)];
      STRNCPY(dirpath, params->dir);
      BLI_path_split_dir_file(
          dirpath, params->dir, sizeof(params->dir), params->file, sizeof(params->file));
    }
    else if (BKE_blendfile_library_path_explode(params->dir, tdir, &group, &name)) {
      if (group) {
        BLI_path_append(tdir, sizeof(tdir), group);
      }
      STRNCPY(params->dir, tdir);
      if (name) {
        STRNCPY(params->file, name);
      }
      else {
        params->file[0] = '\0';
      }
    }
  }

  /* #file_expand_directory will have made absolute. */
  BLI_assert(!BLI_path_is_rel(params->dir));
  BLI_path_normalize_dir(params->dir, sizeof(params->dir));

  if (filelist_is_dir(sfile->files, params->dir)) {
    /* Avoids flickering when nothing's changed. */
    if (!STREQ(params->dir, old_dir)) {
      /* If directory exists, enter it immediately. */
      ED_file_change_dir(C);
    }
  }
  else if (!can_create_dir_from_user_input(params->dir)) {
    const char *lastdir = folderlist_peeklastdir(sfile->folders_prev);
    if (lastdir) {
      STRNCPY(params->dir, lastdir);
    }
  }
  else {
    const char *lastdir = folderlist_peeklastdir(sfile->folders_prev);
    char tdir[FILE_MAX_LIBEXTRA];

    /* If we are "inside" a `.blend` library, we cannot do anything. */
    if (lastdir && BKE_blendfile_library_path_explode(lastdir, tdir, nullptr, nullptr)) {
      STRNCPY(params->dir, lastdir);
    }
    else {
      /* If not, ask to create it and enter if confirmed. */
      wmOperatorType *ot = WM_operatortype_find("FILE_OT_directory_new", false);
      PointerRNA ptr;
      WM_operator_properties_create_ptr(&ptr, ot);
      RNA_string_set(&ptr, "directory", params->dir);
      RNA_boolean_set(&ptr, "open", true);
      /* Enable confirmation prompt, else it's too easy to accidentally create new directories. */
      RNA_boolean_set(&ptr, "confirm", true);

      if (lastdir) {
        STRNCPY(params->dir, lastdir);
      }

      WM_operator_name_call_ptr(C, ot, blender::wm::OpCallContext::InvokeDefault, &ptr, nullptr);
      WM_operator_properties_free(&ptr);
    }
  }

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
}

void file_filename_enter_handle(bContext *C, void * /*arg_unused*/, void *arg_but)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  if (UNLIKELY(params == nullptr)) {
    return;
  }

  const Main *bmain = CTX_data_main(C);
  uiBut *but = static_cast<uiBut *>(arg_but);

  file_expand_directory(bmain, params);

  char matched_file[FILE_MAX] = "";
  int matches = file_select_match(sfile, params->file, matched_file);

  /* It's important this runs *after* #file_select_match,
   * so making "safe" doesn't remove shell-style globing characters. */
  const bool allow_tokens = (params->flag & FILE_PATH_TOKENS_ALLOW) != 0;
  BLI_path_make_safe_filename_ex(params->file, allow_tokens);

  if (matches) {
    /* Replace the pattern (or filename that the user typed in,
     * with the first selected file of the match. */
    STRNCPY(params->file, matched_file);

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

    if (matches == 1) {
      char filepath[sizeof(params->dir)];
      BLI_path_join(filepath, sizeof(params->dir), params->dir, params->file);

      /* If directory, open it and empty filename field. */
      if (filelist_is_dir(sfile->files, filepath)) {
        BLI_path_abs(filepath, BKE_main_blendfile_path(bmain));
        BLI_path_normalize_dir(filepath, sizeof(filepath));
        STRNCPY(params->dir, filepath);
        params->file[0] = '\0';
        ED_file_change_dir(C);
        UI_textbutton_activate_but(C, but);
        WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
      }
    }
    else {
      BLI_assert(matches > 1);
      file_draw_check(C);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Show Hidden Files Operator
 * \{ */

static wmOperatorStatus file_hidedot_exec(bContext *C, wmOperator * /*unused*/)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (params) {
    params->flag ^= FILE_HIDE_DOT;
    ED_fileselect_clear(wm, sfile);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
  }

  return OPERATOR_FINISHED;
}

void FILE_OT_hidedot(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Hide Dot Files";
  ot->description = "Toggle hide hidden dot files";
  ot->idname = "FILE_OT_hidedot";

  /* API callbacks. */
  ot->exec = file_hidedot_exec;
  /* File browsing only operator (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active; /* <- important, handler is on window level */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Increment Filename Operator
 * \{ */

static bool file_filenum_poll(bContext *C)
{
  SpaceFile *sfile = CTX_wm_space_file(C);

  /* File browsing only operator (not asset browsing). */
  if (!ED_operator_file_browsing_active(C)) {
    return false;
  }

  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  return params && (params->flag & FILE_CHECK_EXISTING);
}

/**
 * Looks for a string of digits within filename (using BLI_path_sequence_decode) and adjusts it by
 * add.
 */
static void filenum_newname(char *filename, size_t filename_maxncpy, int add)
{
  char head[FILE_MAXFILE], tail[FILE_MAXFILE];
  int pic;
  ushort digits;

  pic = BLI_path_sequence_decode(filename, head, sizeof(head), tail, sizeof(tail), &digits);

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
  pic = std::max(pic, 0);
  BLI_path_sequence_encode(filename, filename_maxncpy, head, tail, digits, pic);
}

static wmOperatorStatus file_filenum_exec(bContext *C, wmOperator *op)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  ScrArea *area = CTX_wm_area(C);

  int inc = RNA_int_get(op->ptr, "increment");
  if (params && (inc != 0)) {
    filenum_newname(params->file, sizeof(params->file), inc);
    ED_area_tag_redraw(area);
    file_draw_check(C);
    // WM_event_add_notifier(C, NC_WINDOW, nullptr);
  }

  return OPERATOR_FINISHED;
}

void FILE_OT_filenum(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Increment Number in Filename";
  ot->description = "Increment number in filename";
  ot->idname = "FILE_OT_filenum";

  /* API callbacks. */
  ot->exec = file_filenum_exec;
  ot->poll = file_filenum_poll;

  /* props */
  RNA_def_int(ot->srna, "increment", 1, -100, 100, "Increment", "", -100, 100);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rename File/Directory Operator
 * \{ */

static void file_rename_state_activate(SpaceFile *sfile, int file_idx, bool require_selected)
{
  const int numfiles = filelist_files_ensure(sfile->files);

  if ((file_idx >= 0) && (file_idx < numfiles)) {
    FileDirEntry *file = filelist_file(sfile->files, file_idx);

    if ((require_selected == false) ||
        (filelist_entry_select_get(sfile->files, file, CHECK_ALL) & FILE_SEL_SELECTED))
    {
      FileSelectParams *params = ED_fileselect_get_active_params(sfile);

      filelist_entry_select_index_set(
          sfile->files, file_idx, FILE_SEL_ADD, FILE_SEL_EDITING, CHECK_ALL);
      STRNCPY(params->renamefile, file->relpath);
      /* We can skip the pending state,
       * as we can directly set FILE_SEL_EDITING on the expected entry here. */
      params->rename_flag = FILE_PARAMS_RENAME_ACTIVE;
    }
  }
}

static wmOperatorStatus file_rename_exec(bContext *C, wmOperator * /*op*/)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceFile *sfile = (SpaceFile *)CTX_wm_space_data(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (params) {
    file_rename_state_activate(sfile, params->active_file, false);
    ED_area_tag_redraw(area);
  }

  return OPERATOR_FINISHED;
}

void FILE_OT_rename(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rename File or Directory";
  ot->description = "Rename file or file directory";
  ot->idname = "FILE_OT_rename";

  /* API callbacks. */
  ot->exec = file_rename_exec;
  /* File browsing only operator (not asset browsing). */
  ot->poll = ED_operator_file_browsing_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete File Operator
 * \{ */

static bool file_delete_poll(bContext *C)
{
  if (!ED_operator_file_browsing_active(C)) {
    return false;
  }

  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  if (!sfile || !params) {
    return false;
  }

  char dir[FILE_MAX_LIBEXTRA];
  if (filelist_islibrary(sfile->files, dir, nullptr)) {
    return false;
  }

  int numfiles = filelist_files_ensure(sfile->files);
  for (int i = 0; i < numfiles; i++) {
    if (filelist_entry_select_index_get(sfile->files, i, CHECK_ALL)) {
      /* Has a selected file -> the operator can run. */
      return true;
    }
  }

  return false;
}

static bool file_delete_single(const FileList *files,
                               FileDirEntry *file,
                               const char **r_error_message)
{
  char filepath[FILE_MAX_LIBEXTRA];
  filelist_file_get_full_path(files, file, filepath);
  if (BLI_delete_soft(filepath, r_error_message) != 0 || BLI_exists(filepath)) {
    return false;
  }

  return true;
}

static wmOperatorStatus file_delete_exec(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  int numfiles = filelist_files_ensure(sfile->files);

  const char *error_message = nullptr;
  bool report_error = false;
  errno = 0;
  for (int i = 0; i < numfiles; i++) {
    if (filelist_entry_select_index_get(sfile->files, i, CHECK_ALL)) {
      FileDirEntry *file = filelist_file(sfile->files, i);
      if (!file_delete_single(sfile->files, file, &error_message)) {
        report_error = true;
      }
    }
  }

  if (report_error) {
    const char *error_prefix = "Could not delete file or directory: ";
    const char *errno_message = errno ? strerror(errno) : "unknown error";
    if (error_message != nullptr) {
      BKE_reportf(op->reports, RPT_ERROR, "%s%s, %s", error_prefix, error_message, errno_message);
    }
    else {
      BKE_reportf(op->reports, RPT_ERROR, "%s%s", error_prefix, errno_message);
    }
  }

  ED_fileselect_clear(wm, sfile);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus file_delete_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return WM_operator_confirm_ex(
      C, op, IFACE_("Delete selected files?"), nullptr, IFACE_("Delete"), ALERT_ICON_NONE, false);
}

void FILE_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Selected Files";
  ot->description = "Move selected files to the trash or recycle bin";
  ot->idname = "FILE_OT_delete";

  /* API callbacks. */
  ot->invoke = file_delete_invoke;
  ot->exec = file_delete_exec;
  ot->poll = file_delete_poll; /* <- important, handler is on window level */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enter Filter Text Operator
 * \{ */

static wmOperatorStatus file_start_filter_exec(bContext *C, wmOperator * /*op*/)
{
  const ScrArea *area = CTX_wm_area(C);
  const SpaceFile *sfile = CTX_wm_space_file(C);
  const FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (UI_textbutton_activate_rna(C, region, params, "filter_search")) {
        break;
      }
    }
  }

  return OPERATOR_FINISHED;
}

void FILE_OT_start_filter(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Filter";
  ot->description = "Start entering filter text";
  ot->idname = "FILE_OT_start_filter";

  /* API callbacks. */
  ot->exec = file_start_filter_exec;
  /* Operator works for file or asset browsing */
  ot->poll = ED_operator_file_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Directory Path Operator
 * \{ */

static wmOperatorStatus file_edit_directory_path_exec(bContext *C, wmOperator * /*op*/)
{
  const ScrArea *area = CTX_wm_area(C);
  const SpaceFile *sfile = CTX_wm_space_file(C);
  const FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (UI_textbutton_activate_rna(C, region, params, "directory")) {
        break;
      }
    }
  }

  return OPERATOR_FINISHED;
}

void FILE_OT_edit_directory_path(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Directory Path";
  ot->description = "Start editing directory field";
  ot->idname = "FILE_OT_edit_directory_path";

  /* API callbacks. */
  ot->exec = file_edit_directory_path_exec;
  ot->poll = ED_operator_file_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Macro Operators
 * \{ */

void ED_operatormacros_file()
{
  //  wmOperatorType *ot;
  //  wmOperatorTypeMacro *otmacro;

  /* future macros */
}

/** \} */
