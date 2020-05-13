/*
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
 */

/** \file
 * \ingroup spfile
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

/* path/file handling stuff */
#ifdef WIN32
#  include "BLI_winstuff.h"
#  include <direct.h>
#  include <io.h>
#else
#  include <dirent.h>
#  include <sys/times.h>
#  include <unistd.h>
#endif

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_fnmatch.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BLO_readfile.h"

#include "BLT_translation.h"

#include "BKE_appdir.h"
#include "BKE_context.h"
#include "BKE_main.h"

#include "BLF_api.h"

#include "ED_fileselect.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_view2d.h"

#include "file_intern.h"
#include "filelist.h"

#define VERTLIST_MAJORCOLUMN_WIDTH (25 * UI_UNIT_X)

FileSelectParams *ED_fileselect_get_params(struct SpaceFile *sfile)
{
  if (!sfile->params) {
    ED_fileselect_set_params(sfile);
  }
  return sfile->params;
}

/**
 * \note RNA_struct_property_is_set_ex is used here because we want
 *       the previously used settings to be used here rather then overriding them */
short ED_fileselect_set_params(SpaceFile *sfile)
{
  FileSelectParams *params;
  wmOperator *op = sfile->op;

  const char *blendfile_path = BKE_main_blendfile_path_from_global();

  /* create new parameters if necessary */
  if (!sfile->params) {
    sfile->params = MEM_callocN(sizeof(FileSelectParams), "fileselparams");
    /* set path to most recently opened .blend */
    BLI_split_dirfile(blendfile_path,
                      sfile->params->dir,
                      sfile->params->file,
                      sizeof(sfile->params->dir),
                      sizeof(sfile->params->file));
    sfile->params->filter_glob[0] = '\0';
    sfile->params->thumbnail_size = U_default.file_space_data.thumbnail_size;
    sfile->params->details_flags = U_default.file_space_data.details_flags;
    sfile->params->filter_id = U_default.file_space_data.filter_id;
  }

  params = sfile->params;

  /* set the parameters from the operator, if it exists */
  if (op) {
    PropertyRNA *prop;
    const bool is_files = (RNA_struct_find_property(op->ptr, "files") != NULL);
    const bool is_filepath = (RNA_struct_find_property(op->ptr, "filepath") != NULL);
    const bool is_filename = (RNA_struct_find_property(op->ptr, "filename") != NULL);
    const bool is_directory = (RNA_struct_find_property(op->ptr, "directory") != NULL);
    const bool is_relative_path = (RNA_struct_find_property(op->ptr, "relative_path") != NULL);

    BLI_strncpy_utf8(
        params->title, WM_operatortype_name(op->type, op->ptr), sizeof(params->title));

    if ((prop = RNA_struct_find_property(op->ptr, "filemode"))) {
      params->type = RNA_property_int_get(op->ptr, prop);
    }
    else {
      params->type = FILE_SPECIAL;
    }

    if (is_filepath && RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
      char name[FILE_MAX];
      RNA_string_get(op->ptr, "filepath", name);
      if (params->type == FILE_LOADLIB) {
        BLI_strncpy(params->dir, name, sizeof(params->dir));
        sfile->params->file[0] = '\0';
      }
      else {
        BLI_split_dirfile(name,
                          sfile->params->dir,
                          sfile->params->file,
                          sizeof(sfile->params->dir),
                          sizeof(sfile->params->file));
      }
    }
    else {
      if (is_directory && RNA_struct_property_is_set_ex(op->ptr, "directory", false)) {
        RNA_string_get(op->ptr, "directory", params->dir);
        sfile->params->file[0] = '\0';
      }

      if (is_filename && RNA_struct_property_is_set_ex(op->ptr, "filename", false)) {
        RNA_string_get(op->ptr, "filename", params->file);
      }
    }

    if (params->dir[0]) {
      BLI_path_normalize_dir(blendfile_path, params->dir);
      BLI_path_abs(params->dir, blendfile_path);
    }

    params->flag = 0;
    if (is_directory == true && is_filename == false && is_filepath == false &&
        is_files == false) {
      params->flag |= FILE_DIRSEL_ONLY;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "check_existing"))) {
      params->flag |= RNA_property_boolean_get(op->ptr, prop) ? FILE_CHECK_EXISTING : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "hide_props_region"))) {
      params->flag |= RNA_property_boolean_get(op->ptr, prop) ? FILE_HIDE_TOOL_PROPS : 0;
    }

    params->filter = 0;
    if ((prop = RNA_struct_find_property(op->ptr, "filter_blender"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_BLENDER : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_blenlib"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_BLENDERLIB : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_backup"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_BLENDER_BACKUP : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_image"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_IMAGE : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_movie"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_MOVIE : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_python"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_PYSCRIPT : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_font"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_FTFONT : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_sound"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_SOUND : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_text"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_TEXT : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_archive"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_ARCHIVE : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_folder"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_FOLDER : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_btx"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_BTX : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_collada"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_COLLADA : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_alembic"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_ALEMBIC : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_usd"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_USD : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_volume"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? FILE_TYPE_VOLUME : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_glob"))) {
      /* Protection against pyscripts not setting proper size limit... */
      char *tmp = RNA_property_string_get_alloc(
          op->ptr, prop, params->filter_glob, sizeof(params->filter_glob), NULL);
      if (tmp != params->filter_glob) {
        BLI_strncpy(params->filter_glob, tmp, sizeof(params->filter_glob));
        MEM_freeN(tmp);

        /* Fix stupid things that truncating might have generated,
         * like last group being a 'match everything' wildcard-only one... */
        BLI_path_extension_glob_validate(params->filter_glob);
      }
      params->filter |= (FILE_TYPE_OPERATOR | FILE_TYPE_FOLDER);
    }
    else {
      params->filter_glob[0] = '\0';
    }

    if (params->filter != 0) {
      if (U.uiflag & USER_FILTERFILEEXTS) {
        params->flag |= FILE_FILTER;
      }
      else {
        params->flag &= ~FILE_FILTER;
      }
    }

    if (U.uiflag & USER_HIDE_DOT) {
      params->flag |= FILE_HIDE_DOT;
    }
    else {
      params->flag &= ~FILE_HIDE_DOT;
    }

    if (params->type == FILE_LOADLIB) {
      params->flag |= RNA_boolean_get(op->ptr, "link") ? FILE_LINK : 0;
      params->flag |= RNA_boolean_get(op->ptr, "autoselect") ? FILE_AUTOSELECT : 0;
      params->flag |= RNA_boolean_get(op->ptr, "active_collection") ? FILE_ACTIVE_COLLECTION : 0;
    }

    if ((prop = RNA_struct_find_property(op->ptr, "display_type"))) {
      params->display = RNA_property_enum_get(op->ptr, prop);
    }

    if ((prop = RNA_struct_find_property(op->ptr, "sort_method"))) {
      params->sort = RNA_property_enum_get(op->ptr, prop);
    }
    else {
      params->sort = U_default.file_space_data.sort_type;
    }

    if (params->display == FILE_DEFAULTDISPLAY) {
      params->display = U_default.file_space_data.display_type;
    }

    if (is_relative_path) {
      if ((prop = RNA_struct_find_property(op->ptr, "relative_path"))) {
        if (!RNA_property_is_set_ex(op->ptr, prop, false)) {
          RNA_property_boolean_set(op->ptr, prop, (U.flag & USER_RELPATHS) != 0);
        }
      }
    }
  }
  else {
    /* default values, if no operator */
    params->type = FILE_UNIX;
    params->flag |= U_default.file_space_data.flag;
    params->flag &= ~FILE_DIRSEL_ONLY;
    params->display = FILE_VERTICALDISPLAY;
    params->sort = FILE_SORT_ALPHA;
    params->filter = 0;
    params->filter_glob[0] = '\0';
  }

  /* operator has no setting for this */
  params->active_file = -1;

  /* initialize the list with previous folders */
  if (!sfile->folders_prev) {
    sfile->folders_prev = folderlist_new();
  }

  if (!sfile->params->dir[0]) {
    if (blendfile_path[0] != '\0') {
      BLI_split_dir_part(blendfile_path, sfile->params->dir, sizeof(sfile->params->dir));
    }
    else {
      const char *doc_path = BKE_appdir_folder_default();
      if (doc_path) {
        BLI_strncpy(sfile->params->dir, doc_path, sizeof(sfile->params->dir));
      }
    }
  }

  folderlist_pushdir(sfile->folders_prev, sfile->params->dir);

  /* switching thumbnails needs to recalc layout [#28809] */
  if (sfile->layout) {
    sfile->layout->dirty = true;
  }

  return 1;
}

/* The subset of FileSelectParams.flag items we store into preferences. */
#define PARAMS_FLAGS_REMEMBERED (FILE_HIDE_DOT | FILE_SORT_INVERT)

void ED_fileselect_window_params_get(const wmWindow *win, int win_size[2], bool *is_maximized)
{
  /* Get DPI/pixelsize independent size to be stored in preferences. */
  WM_window_set_dpi(win); /* Ensure the DPI is taken from the right window. */

  win_size[0] = WM_window_pixels_x(win) / UI_DPI_FAC;
  win_size[1] = WM_window_pixels_y(win) / UI_DPI_FAC;

  *is_maximized = WM_window_is_maximized(win);
}

void ED_fileselect_set_params_from_userdef(SpaceFile *sfile)
{
  wmOperator *op = sfile->op;
  UserDef_FileSpaceData *sfile_udata = &U.file_space_data;

  ED_fileselect_set_params(sfile);

  if (!op) {
    return;
  }

  if (!RNA_struct_property_is_set(op->ptr, "display_type")) {
    sfile->params->display = sfile_udata->display_type;
  }
  if (!RNA_struct_property_is_set(op->ptr, "sort_method")) {
    sfile->params->sort = sfile_udata->sort_type;
  }
  sfile->params->thumbnail_size = sfile_udata->thumbnail_size;
  sfile->params->details_flags = sfile_udata->details_flags;
  sfile->params->filter_id = sfile_udata->filter_id;

  /* Combine flags we take from params with the flags we take from userdef. */
  sfile->params->flag = (sfile->params->flag & ~PARAMS_FLAGS_REMEMBERED) |
                        (sfile_udata->flag & PARAMS_FLAGS_REMEMBERED);
}

/**
 * Update the user-preference data for the file space. In fact, this also contains some
 * non-FileSelectParams data, but we can safely ignore this.
 *
 * \param temp_win_size: If the browser was opened in a temporary window,
 * pass its size here so we can store that in the preferences. Otherwise NULL.
 */
void ED_fileselect_params_to_userdef(SpaceFile *sfile,
                                     int temp_win_size[2],
                                     const bool is_maximized)
{
  UserDef_FileSpaceData *sfile_udata_new = &U.file_space_data;
  UserDef_FileSpaceData sfile_udata_old = U.file_space_data;

  sfile_udata_new->display_type = sfile->params->display;
  sfile_udata_new->thumbnail_size = sfile->params->thumbnail_size;
  sfile_udata_new->sort_type = sfile->params->sort;
  sfile_udata_new->details_flags = sfile->params->details_flags;
  sfile_udata_new->flag = sfile->params->flag & PARAMS_FLAGS_REMEMBERED;
  sfile_udata_new->filter_id = sfile->params->filter_id;

  if (temp_win_size && !is_maximized) {
    sfile_udata_new->temp_win_sizex = temp_win_size[0];
    sfile_udata_new->temp_win_sizey = temp_win_size[1];
  }

  /* Tag prefs as dirty if something has changed. */
  if (memcmp(sfile_udata_new, &sfile_udata_old, sizeof(sfile_udata_old)) != 0) {
    U.runtime.is_dirty = true;
  }
}

void ED_fileselect_reset_params(SpaceFile *sfile)
{
  sfile->params->type = FILE_UNIX;
  sfile->params->flag = 0;
  sfile->params->title[0] = '\0';
  sfile->params->active_file = -1;
}

/**
 * Sets FileSelectParams->file (name of selected file)
 */
void fileselect_file_set(SpaceFile *sfile, const int index)
{
  const struct FileDirEntry *file = filelist_file(sfile->files, index);
  if (file && file->relpath && file->relpath[0] && !(file->typeflag & FILE_TYPE_DIR)) {
    BLI_strncpy(sfile->params->file, file->relpath, FILE_MAXFILE);
  }
}

int ED_fileselect_layout_numfiles(FileLayout *layout, ARegion *region)
{
  int numfiles;

  /* Values in pixels.
   *
   * - *_item: size of each (row|col), (including padding)
   * - *_view: (x|y) size of the view.
   * - *_over: extra pixels, to take into account, when the fit isnt exact
   *   (needed since you may see the end of the previous column and the beginning of the next).
   *
   * Could be more clever and take scrolling into account,
   * but for now don't bother.
   */
  if (layout->flag & FILE_LAYOUT_HOR) {
    const int x_item = layout->tile_w + (2 * layout->tile_border_x);
    const int x_view = (int)(BLI_rctf_size_x(&region->v2d.cur));
    const int x_over = x_item - (x_view % x_item);
    numfiles = (int)((float)(x_view + x_over) / (float)(x_item));
    return numfiles * layout->rows;
  }
  else {
    const int y_item = layout->tile_h + (2 * layout->tile_border_y);
    const int y_view = (int)(BLI_rctf_size_y(&region->v2d.cur)) - layout->offset_top;
    const int y_over = y_item - (y_view % y_item);
    numfiles = (int)((float)(y_view + y_over) / (float)(y_item));
    return numfiles * layout->flow_columns;
  }
}

static bool is_inside(int x, int y, int cols, int rows)
{
  return ((x >= 0) && (x < cols) && (y >= 0) && (y < rows));
}

FileSelection ED_fileselect_layout_offset_rect(FileLayout *layout, const rcti *rect)
{
  int colmin, colmax, rowmin, rowmax;
  FileSelection sel;
  sel.first = sel.last = -1;

  if (layout == NULL) {
    return sel;
  }

  colmin = (rect->xmin) / (layout->tile_w + 2 * layout->tile_border_x);
  rowmin = (rect->ymin - layout->offset_top) / (layout->tile_h + 2 * layout->tile_border_y);
  colmax = (rect->xmax) / (layout->tile_w + 2 * layout->tile_border_x);
  rowmax = (rect->ymax - layout->offset_top) / (layout->tile_h + 2 * layout->tile_border_y);

  if (is_inside(colmin, rowmin, layout->flow_columns, layout->rows) ||
      is_inside(colmax, rowmax, layout->flow_columns, layout->rows)) {
    CLAMP(colmin, 0, layout->flow_columns - 1);
    CLAMP(rowmin, 0, layout->rows - 1);
    CLAMP(colmax, 0, layout->flow_columns - 1);
    CLAMP(rowmax, 0, layout->rows - 1);
  }

  if ((colmin > layout->flow_columns - 1) || (rowmin > layout->rows - 1)) {
    sel.first = -1;
  }
  else {
    if (layout->flag & FILE_LAYOUT_HOR) {
      sel.first = layout->rows * colmin + rowmin;
    }
    else {
      sel.first = colmin + layout->flow_columns * rowmin;
    }
  }
  if ((colmax > layout->flow_columns - 1) || (rowmax > layout->rows - 1)) {
    sel.last = -1;
  }
  else {
    if (layout->flag & FILE_LAYOUT_HOR) {
      sel.last = layout->rows * colmax + rowmax;
    }
    else {
      sel.last = colmax + layout->flow_columns * rowmax;
    }
  }

  return sel;
}

int ED_fileselect_layout_offset(FileLayout *layout, int x, int y)
{
  int offsetx, offsety;
  int active_file;

  if (layout == NULL) {
    return -1;
  }

  offsetx = (x) / (layout->tile_w + 2 * layout->tile_border_x);
  offsety = (y - layout->offset_top) / (layout->tile_h + 2 * layout->tile_border_y);

  if (offsetx > layout->flow_columns - 1) {
    return -1;
  }
  if (offsety > layout->rows - 1) {
    return -1;
  }

  if (layout->flag & FILE_LAYOUT_HOR) {
    active_file = layout->rows * offsetx + offsety;
  }
  else {
    active_file = offsetx + layout->flow_columns * offsety;
  }
  return active_file;
}

/**
 * Get the currently visible bounds of the layout in screen space. Matches View2D.mask minus the
 * top column-header row.
 */
void ED_fileselect_layout_maskrect(const FileLayout *layout, const View2D *v2d, rcti *r_rect)
{
  *r_rect = v2d->mask;
  r_rect->ymax -= layout->offset_top;
}

bool ED_fileselect_layout_is_inside_pt(const FileLayout *layout, const View2D *v2d, int x, int y)
{
  rcti maskrect;
  ED_fileselect_layout_maskrect(layout, v2d, &maskrect);
  return BLI_rcti_isect_pt(&maskrect, x, y);
}

bool ED_fileselect_layout_isect_rect(const FileLayout *layout,
                                     const View2D *v2d,
                                     const rcti *rect,
                                     rcti *r_dst)
{
  rcti maskrect;
  ED_fileselect_layout_maskrect(layout, v2d, &maskrect);
  return BLI_rcti_isect(&maskrect, rect, r_dst);
}

void ED_fileselect_layout_tilepos(FileLayout *layout, int tile, int *x, int *y)
{
  if (layout->flag == FILE_LAYOUT_HOR) {
    *x = layout->tile_border_x +
         (tile / layout->rows) * (layout->tile_w + 2 * layout->tile_border_x);
    *y = layout->offset_top + layout->tile_border_y +
         (tile % layout->rows) * (layout->tile_h + 2 * layout->tile_border_y);
  }
  else {
    *x = layout->tile_border_x +
         ((tile) % layout->flow_columns) * (layout->tile_w + 2 * layout->tile_border_x);
    *y = layout->offset_top + layout->tile_border_y +
         ((tile) / layout->flow_columns) * (layout->tile_h + 2 * layout->tile_border_y);
  }
}

/**
 * Check if the region coordinate defined by \a x and \a y are inside the column header.
 */
bool file_attribute_column_header_is_inside(const View2D *v2d,
                                            const FileLayout *layout,
                                            int x,
                                            int y)
{
  rcti header_rect = v2d->mask;
  header_rect.ymin = header_rect.ymax - layout->attribute_column_header_h;
  return BLI_rcti_isect_pt(&header_rect, x, y);
}

bool file_attribute_column_type_enabled(const FileSelectParams *params,
                                        FileAttributeColumnType column)
{
  switch (column) {
    case COLUMN_NAME:
      /* Always enabled */
      return true;
    case COLUMN_DATETIME:
      return (params->details_flags & FILE_DETAILS_DATETIME) != 0;
    case COLUMN_SIZE:
      return (params->details_flags & FILE_DETAILS_SIZE) != 0;
    default:
      return false;
  }
}

/**
 * Find the column type at region coordinate given by \a x (y doesn't matter for this).
 */
FileAttributeColumnType file_attribute_column_type_find_isect(const View2D *v2d,
                                                              const FileSelectParams *params,
                                                              FileLayout *layout,
                                                              int x)
{
  float mx, my;
  int offset_tile;

  UI_view2d_region_to_view(v2d, x, v2d->mask.ymax - layout->offset_top - 1, &mx, &my);
  offset_tile = ED_fileselect_layout_offset(
      layout, (int)(v2d->tot.xmin + mx), (int)(v2d->tot.ymax - my));
  if (offset_tile > -1) {
    int tile_x, tile_y;
    int pos_x = 0;
    int rel_x; /* x relative to the hovered tile */

    ED_fileselect_layout_tilepos(layout, offset_tile, &tile_x, &tile_y);
    /* Column header drawing doesn't use left tile border, so subtract it. */
    rel_x = mx - (tile_x - layout->tile_border_x);

    for (FileAttributeColumnType column = 0; column < ATTRIBUTE_COLUMN_MAX; column++) {
      if (!file_attribute_column_type_enabled(params, column)) {
        continue;
      }
      const int width = layout->attribute_columns[column].width;

      if (IN_RANGE(rel_x, pos_x, pos_x + width)) {
        return column;
      }

      pos_x += width;
    }
  }

  return COLUMN_NONE;
}

float file_string_width(const char *str)
{
  const uiStyle *style = UI_style_get();
  float width;

  UI_fontstyle_set(&style->widget);
  if (style->widget.kerning == 1) { /* for BLF_width */
    BLF_enable(style->widget.uifont_id, BLF_KERNING_DEFAULT);
  }

  width = BLF_width(style->widget.uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);

  if (style->widget.kerning == 1) {
    BLF_disable(style->widget.uifont_id, BLF_KERNING_DEFAULT);
  }

  return width;
}

float file_font_pointsize(void)
{
#if 0
  float s;
  char tmp[2] = "X";
  const uiStyle *style = UI_style_get();
  UI_fontstyle_set(&style->widget);
  s = BLF_height(style->widget.uifont_id, tmp);
  return style->widget.points;
#else
  const uiStyle *style = UI_style_get();
  UI_fontstyle_set(&style->widget);
  return style->widget.points * UI_DPI_FAC;
#endif
}

static void file_attribute_columns_widths(const FileSelectParams *params, FileLayout *layout)
{
  FileAttributeColumn *columns = layout->attribute_columns;
  const bool small_size = SMALL_SIZE_CHECK(params->thumbnail_size);
  const int pad = small_size ? 0 : ATTRIBUTE_COLUMN_PADDING * 2;

  for (int i = 0; i < ATTRIBUTE_COLUMN_MAX; i++) {
    layout->attribute_columns[i].width = 0;
  }

  /* Biggest possible reasonable values... */
  columns[COLUMN_DATETIME].width = file_string_width(small_size ? "23/08/89" :
                                                                  "23 Dec 6789, 23:59") +
                                   pad;
  columns[COLUMN_SIZE].width = file_string_width(small_size ? "98.7 M" : "098.7 MiB") + pad;
  if (params->display == FILE_IMGDISPLAY) {
    columns[COLUMN_NAME].width = ((float)params->thumbnail_size / 8.0f) * UI_UNIT_X;
  }
  /* Name column uses remaining width */
  else {
    int remwidth = layout->tile_w;
    for (FileAttributeColumnType column_type = ATTRIBUTE_COLUMN_MAX - 1; column_type >= 0;
         column_type--) {
      if ((column_type == COLUMN_NAME) ||
          !file_attribute_column_type_enabled(params, column_type)) {
        continue;
      }
      remwidth -= columns[column_type].width;
    }
    columns[COLUMN_NAME].width = remwidth;
  }
}

static void file_attribute_columns_init(const FileSelectParams *params, FileLayout *layout)
{
  file_attribute_columns_widths(params, layout);

  layout->attribute_columns[COLUMN_NAME].name = N_("Name");
  layout->attribute_columns[COLUMN_NAME].sort_type = FILE_SORT_ALPHA;
  layout->attribute_columns[COLUMN_NAME].text_align = UI_STYLE_TEXT_LEFT;
  layout->attribute_columns[COLUMN_DATETIME].name = N_("Date Modified");
  layout->attribute_columns[COLUMN_DATETIME].sort_type = FILE_SORT_TIME;
  layout->attribute_columns[COLUMN_DATETIME].text_align = UI_STYLE_TEXT_LEFT;
  layout->attribute_columns[COLUMN_SIZE].name = N_("Size");
  layout->attribute_columns[COLUMN_SIZE].sort_type = FILE_SORT_SIZE;
  layout->attribute_columns[COLUMN_SIZE].text_align = UI_STYLE_TEXT_RIGHT;
}

void ED_fileselect_init_layout(struct SpaceFile *sfile, ARegion *region)
{
  FileSelectParams *params = ED_fileselect_get_params(sfile);
  FileLayout *layout = NULL;
  View2D *v2d = &region->v2d;
  int numfiles;
  int textheight;

  if (sfile->layout == NULL) {
    sfile->layout = MEM_callocN(sizeof(struct FileLayout), "file_layout");
    sfile->layout->dirty = true;
  }
  else if (sfile->layout->dirty == false) {
    return;
  }

  numfiles = filelist_files_ensure(sfile->files);
  textheight = (int)file_font_pointsize();
  layout = sfile->layout;
  layout->textheight = textheight;

  if (params->display == FILE_IMGDISPLAY) {
    layout->prv_w = ((float)params->thumbnail_size / 20.0f) * UI_UNIT_X;
    layout->prv_h = ((float)params->thumbnail_size / 20.0f) * UI_UNIT_Y;
    layout->tile_border_x = 0.3f * UI_UNIT_X;
    layout->tile_border_y = 0.3f * UI_UNIT_X;
    layout->prv_border_x = 0.3f * UI_UNIT_X;
    layout->prv_border_y = 0.3f * UI_UNIT_Y;
    layout->tile_w = layout->prv_w + 2 * layout->prv_border_x;
    layout->tile_h = layout->prv_h + 2 * layout->prv_border_y + textheight;
    layout->width = (int)(BLI_rctf_size_x(&v2d->cur) - 2 * layout->tile_border_x);
    layout->flow_columns = layout->width / (layout->tile_w + 2 * layout->tile_border_x);
    layout->attribute_column_header_h = 0;
    layout->offset_top = 0;
    if (layout->flow_columns > 0) {
      layout->rows = divide_ceil_u(numfiles, layout->flow_columns);
    }
    else {
      layout->flow_columns = 1;
      layout->rows = numfiles;
    }
    layout->height = sfile->layout->rows * (layout->tile_h + 2 * layout->tile_border_y) +
                     layout->tile_border_y * 2 - layout->offset_top;
    layout->flag = FILE_LAYOUT_VER;
  }
  else if (params->display == FILE_VERTICALDISPLAY) {
    int rowcount;

    layout->prv_w = ((float)params->thumbnail_size / 20.0f) * UI_UNIT_X;
    layout->prv_h = ((float)params->thumbnail_size / 20.0f) * UI_UNIT_Y;
    layout->tile_border_x = 0.4f * UI_UNIT_X;
    layout->tile_border_y = 0.1f * UI_UNIT_Y;
    layout->tile_h = textheight * 3 / 2;
    layout->width = (int)(BLI_rctf_size_x(&v2d->cur) - 2 * layout->tile_border_x);
    layout->tile_w = layout->width;
    layout->flow_columns = 1;
    layout->attribute_column_header_h = layout->tile_h * 1.2f + 2 * layout->tile_border_y;
    layout->offset_top = layout->attribute_column_header_h;
    rowcount = (int)(BLI_rctf_size_y(&v2d->cur) - layout->offset_top - 2 * layout->tile_border_y) /
               (layout->tile_h + 2 * layout->tile_border_y);
    file_attribute_columns_init(params, layout);

    layout->rows = MAX2(rowcount, numfiles);
    BLI_assert(layout->rows != 0);
    layout->height = sfile->layout->rows * (layout->tile_h + 2 * layout->tile_border_y) +
                     layout->tile_border_y * 2 + layout->offset_top;
    layout->flag = FILE_LAYOUT_VER;
  }
  else if (params->display == FILE_HORIZONTALDISPLAY) {
    layout->prv_w = ((float)params->thumbnail_size / 20.0f) * UI_UNIT_X;
    layout->prv_h = ((float)params->thumbnail_size / 20.0f) * UI_UNIT_Y;
    layout->tile_border_x = 0.4f * UI_UNIT_X;
    layout->tile_border_y = 0.1f * UI_UNIT_Y;
    layout->tile_h = textheight * 3 / 2;
    layout->attribute_column_header_h = 0;
    layout->offset_top = layout->attribute_column_header_h;
    layout->height = (int)(BLI_rctf_size_y(&v2d->cur) - 2 * layout->tile_border_y);
    /* Padding by full scrollbar H is too much, can overlap tile border Y. */
    layout->rows = (layout->height - V2D_SCROLL_HEIGHT + layout->tile_border_y) /
                   (layout->tile_h + 2 * layout->tile_border_y);
    layout->tile_w = VERTLIST_MAJORCOLUMN_WIDTH;
    file_attribute_columns_init(params, layout);

    if (layout->rows > 0) {
      layout->flow_columns = divide_ceil_u(numfiles, layout->rows);
    }
    else {
      layout->rows = 1;
      layout->flow_columns = numfiles;
    }
    layout->width = sfile->layout->flow_columns * (layout->tile_w + 2 * layout->tile_border_x) +
                    layout->tile_border_x * 2;
    layout->flag = FILE_LAYOUT_HOR;
  }
  layout->dirty = false;
}

FileLayout *ED_fileselect_get_layout(struct SpaceFile *sfile, ARegion *region)
{
  if (!sfile->layout) {
    ED_fileselect_init_layout(sfile, region);
  }
  return sfile->layout;
}

void ED_file_change_dir(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  SpaceFile *sfile = CTX_wm_space_file(C);

  if (sfile->params) {
    ED_fileselect_clear(wm, CTX_data_scene(C), sfile);

    /* Clear search string, it is very rare to want to keep that filter while changing dir,
     * and usually very annoying to keep it actually! */
    sfile->params->filter_search[0] = '\0';
    sfile->params->active_file = -1;

    if (!filelist_is_dir(sfile->files, sfile->params->dir)) {
      BLI_strncpy(sfile->params->dir, filelist_dir(sfile->files), sizeof(sfile->params->dir));
      /* could return but just refresh the current dir */
    }
    filelist_setdir(sfile->files, sfile->params->dir);

    if (folderlist_clear_next(sfile)) {
      folderlist_free(sfile->folders_next);
    }

    folderlist_pushdir(sfile->folders_prev, sfile->params->dir);

    file_draw_check(C);
  }
}

int file_select_match(struct SpaceFile *sfile, const char *pattern, char *matched_file)
{
  int match = 0;

  int i;
  FileDirEntry *file;
  int n = filelist_files_ensure(sfile->files);

  /* select any file that matches the pattern, this includes exact match
   * if the user selects a single file by entering the filename
   */
  for (i = 0; i < n; i++) {
    file = filelist_file(sfile->files, i);
    /* Do not check whether file is a file or dir here! Causes T44243
     * (we do accept dirs at this stage). */
    if (fnmatch(pattern, file->relpath, 0) == 0) {
      filelist_entry_select_set(sfile->files, file, FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);
      if (!match) {
        BLI_strncpy(matched_file, file->relpath, FILE_MAX);
      }
      match++;
    }
  }

  return match;
}

int autocomplete_directory(struct bContext *C, char *str, void *UNUSED(arg_v))
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  int match = AUTOCOMPLETE_NO_MATCH;

  /* search if str matches the beginning of name */
  if (str[0] && sfile->files) {
    char dirname[FILE_MAX];

    DIR *dir;
    struct dirent *de;

    BLI_split_dir_part(str, dirname, sizeof(dirname));

    dir = opendir(dirname);

    if (dir) {
      AutoComplete *autocpl = UI_autocomplete_begin(str, FILE_MAX);

      while ((de = readdir(dir)) != NULL) {
        if (FILENAME_IS_CURRPAR(de->d_name)) {
          /* pass */
        }
        else {
          char path[FILE_MAX];
          BLI_stat_t status;

          BLI_join_dirfile(path, sizeof(path), dirname, de->d_name);

          if (BLI_stat(path, &status) == 0) {
            if (S_ISDIR(status.st_mode)) { /* is subdir */
              UI_autocomplete_update_name(autocpl, path);
            }
          }
        }
      }
      closedir(dir);

      match = UI_autocomplete_end(autocpl, str);
      if (match == AUTOCOMPLETE_FULL_MATCH) {
        BLI_path_slash_ensure(str);
      }
    }
  }

  return match;
}

int autocomplete_file(struct bContext *C, char *str, void *UNUSED(arg_v))
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  int match = AUTOCOMPLETE_NO_MATCH;

  /* search if str matches the beginning of name */
  if (str[0] && sfile->files) {
    AutoComplete *autocpl = UI_autocomplete_begin(str, FILE_MAX);
    int nentries = filelist_files_ensure(sfile->files);
    int i;

    for (i = 0; i < nentries; i++) {
      FileDirEntry *file = filelist_file(sfile->files, i);
      UI_autocomplete_update_name(autocpl, file->relpath);
    }
    match = UI_autocomplete_end(autocpl, str);
  }

  return match;
}

void ED_fileselect_clear(wmWindowManager *wm, Scene *owner_scene, SpaceFile *sfile)
{
  /* only NULL in rare cases - [#29734] */
  if (sfile->files) {
    filelist_readjob_stop(wm, owner_scene);
    filelist_freelib(sfile->files);
    filelist_clear(sfile->files);
  }

  sfile->params->highlight_file = -1;
  WM_main_add_notifier(NC_SPACE | ND_SPACE_FILE_LIST, NULL);
}

void ED_fileselect_exit(wmWindowManager *wm, Scene *owner_scene, SpaceFile *sfile)
{
  if (!sfile) {
    return;
  }
  if (sfile->op) {
    wmWindow *temp_win = WM_window_is_temp_screen(wm->winactive) ? wm->winactive : NULL;
    if (temp_win) {
      int win_size[2];
      bool is_maximized;

      ED_fileselect_window_params_get(temp_win, win_size, &is_maximized);
      ED_fileselect_params_to_userdef(sfile, win_size, is_maximized);
    }
    else {
      ED_fileselect_params_to_userdef(sfile, NULL, false);
    }

    WM_event_fileselect_event(wm, sfile->op, EVT_FILESELECT_EXTERNAL_CANCEL);
    sfile->op = NULL;
  }

  folderlist_free(sfile->folders_prev);
  folderlist_free(sfile->folders_next);

  if (sfile->files) {
    ED_fileselect_clear(wm, owner_scene, sfile);
    filelist_free(sfile->files);
    MEM_freeN(sfile->files);
    sfile->files = NULL;
  }
}

/**
 * Helper used by both main update code, and smooth-scroll timer,
 * to try to enable rename editing from #FileSelectParams.renamefile name.
 */
void file_params_renamefile_activate(SpaceFile *sfile, FileSelectParams *params)
{
  BLI_assert(params->rename_flag != 0);

  if ((params->rename_flag & (FILE_PARAMS_RENAME_ACTIVE | FILE_PARAMS_RENAME_POSTSCROLL_ACTIVE)) !=
      0) {
    return;
  }

  BLI_assert(params->renamefile[0] != '\0');

  const int idx = filelist_file_findpath(sfile->files, params->renamefile);
  if (idx >= 0) {
    FileDirEntry *file = filelist_file(sfile->files, idx);
    BLI_assert(file != NULL);

    if ((params->rename_flag & FILE_PARAMS_RENAME_PENDING) != 0) {
      filelist_entry_select_set(sfile->files, file, FILE_SEL_ADD, FILE_SEL_EDITING, CHECK_ALL);
      params->rename_flag = FILE_PARAMS_RENAME_ACTIVE;
    }
    else if ((params->rename_flag & FILE_PARAMS_RENAME_POSTSCROLL_PENDING) != 0) {
      filelist_entry_select_set(sfile->files, file, FILE_SEL_ADD, FILE_SEL_HIGHLIGHTED, CHECK_ALL);
      params->renamefile[0] = '\0';
      params->rename_flag = FILE_PARAMS_RENAME_POSTSCROLL_ACTIVE;
    }
  }
  /* File listing is now async, only reset renaming if matching entry is not found
   * when file listing is not done. */
  else if (filelist_is_ready(sfile->files)) {
    params->renamefile[0] = '\0';
    params->rename_flag = 0;
  }
}
