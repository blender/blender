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

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"

#include "ED_fileselect.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_framebuffer.h"
#include "file_intern.h" /* own include */
#include "filelist.h"
#include "fsmenu.h"

static ARegion *file_ui_region_ensure(ScrArea *area, ARegion *region_prev)
{
  ARegion *region;

  if ((region = BKE_area_find_region_type(area, RGN_TYPE_UI)) != NULL) {
    return region;
  }

  region = MEM_callocN(sizeof(ARegion), "execute region for file");
  BLI_insertlinkafter(&area->regionbase, region_prev, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_TOP;
  region->flag = RGN_FLAG_DYNAMIC_SIZE;

  return region;
}

static ARegion *file_execute_region_ensure(ScrArea *area, ARegion *region_prev)
{
  ARegion *region;

  if ((region = BKE_area_find_region_type(area, RGN_TYPE_EXECUTE)) != NULL) {
    return region;
  }

  region = MEM_callocN(sizeof(ARegion), "execute region for file");
  BLI_insertlinkafter(&area->regionbase, region_prev, region);
  region->regiontype = RGN_TYPE_EXECUTE;
  region->alignment = RGN_ALIGN_BOTTOM;
  region->flag = RGN_FLAG_DYNAMIC_SIZE;

  return region;
}

static ARegion *file_tool_props_region_ensure(ScrArea *area, ARegion *region_prev)
{
  ARegion *region;

  if ((region = BKE_area_find_region_type(area, RGN_TYPE_TOOL_PROPS)) != NULL) {
    return region;
  }

  /* add subdiv level; after execute region */
  region = MEM_callocN(sizeof(ARegion), "tool props for file");
  BLI_insertlinkafter(&area->regionbase, region_prev, region);
  region->regiontype = RGN_TYPE_TOOL_PROPS;
  region->alignment = RGN_ALIGN_RIGHT;
  region->flag = RGN_FLAG_HIDDEN;

  return region;
}

/* ******************** default callbacks for file space ***************** */

static SpaceLink *file_create(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
  ARegion *region;
  SpaceFile *sfile;

  sfile = MEM_callocN(sizeof(SpaceFile), "initfile");
  sfile->spacetype = SPACE_FILE;

  /* header */
  region = MEM_callocN(sizeof(ARegion), "header for file");
  BLI_addtail(&sfile->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  /* Ignore user preference "USER_HEADER_BOTTOM" here (always show top for new types). */
  region->alignment = RGN_ALIGN_TOP;

  /* Tools region */
  region = MEM_callocN(sizeof(ARegion), "tools region for file");
  BLI_addtail(&sfile->regionbase, region);
  region->regiontype = RGN_TYPE_TOOLS;
  region->alignment = RGN_ALIGN_LEFT;

  /* ui list region */
  region = MEM_callocN(sizeof(ARegion), "ui region for file");
  BLI_addtail(&sfile->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_TOP;
  region->flag |= RGN_FLAG_DYNAMIC_SIZE;

  /* Tool props and execute region are added as needed, see file_refresh(). */

  /* main region */
  region = MEM_callocN(sizeof(ARegion), "main region for file");
  BLI_addtail(&sfile->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;
  region->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
  region->v2d.align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
  region->v2d.keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
  region->v2d.keeptot = V2D_KEEPTOT_STRICT;
  region->v2d.minzoom = region->v2d.maxzoom = 1.0f;

  return (SpaceLink *)sfile;
}

/* not spacelink itself */
static void file_free(SpaceLink *sl)
{
  SpaceFile *sfile = (SpaceFile *)sl;

  BLI_assert(sfile->previews_timer == NULL);

  if (sfile->files) {
    /* XXX would need to do thumbnails_stop here, but no context available */
    filelist_freelib(sfile->files);
    filelist_free(sfile->files);
    MEM_freeN(sfile->files);
    sfile->files = NULL;
  }

  folder_history_list_free(sfile);

  MEM_SAFE_FREE(sfile->params);
  MEM_SAFE_FREE(sfile->asset_params);
  MEM_SAFE_FREE(sfile->runtime);

  if (sfile->layout) {
    MEM_freeN(sfile->layout);
    sfile->layout = NULL;
  }
}

/* spacetype; init callback, area size changes, screen set, etc */
static void file_init(wmWindowManager *UNUSED(wm), ScrArea *area)
{
  SpaceFile *sfile = (SpaceFile *)area->spacedata.first;

  if (sfile->layout) {
    sfile->layout->dirty = true;
  }

  if (sfile->runtime == NULL) {
    sfile->runtime = MEM_callocN(sizeof(*sfile->runtime), __func__);
  }
  /* Validate the params right after file read. */
  fileselect_refresh_params(sfile);
}

static void file_exit(wmWindowManager *wm, ScrArea *area)
{
  SpaceFile *sfile = (SpaceFile *)area->spacedata.first;

  if (sfile->previews_timer) {
    WM_event_remove_timer_notifier(wm, NULL, sfile->previews_timer);
    sfile->previews_timer = NULL;
  }

  ED_fileselect_exit(wm, NULL, sfile);
}

static SpaceLink *file_duplicate(SpaceLink *sl)
{
  SpaceFile *sfileo = (SpaceFile *)sl;
  SpaceFile *sfilen = MEM_dupallocN(sl);

  /* clear or remove stuff from old */
  sfilen->op = NULL; /* file window doesn't own operators */
  sfilen->runtime = NULL;

  sfilen->previews_timer = NULL;
  sfilen->smoothscroll_timer = NULL;

  FileSelectParams *active_params_old = ED_fileselect_get_active_params(sfileo);
  if (active_params_old) {
    sfilen->files = filelist_new(active_params_old->type);
    filelist_setdir(sfilen->files, active_params_old->dir);
  }

  if (sfileo->params) {
    sfilen->params = MEM_dupallocN(sfileo->params);
  }
  if (sfileo->asset_params) {
    sfilen->asset_params = MEM_dupallocN(sfileo->asset_params);
  }

  sfilen->folder_histories = folder_history_list_duplicate(&sfileo->folder_histories);

  if (sfileo->layout) {
    sfilen->layout = MEM_dupallocN(sfileo->layout);
  }
  return (SpaceLink *)sfilen;
}

static void file_ensure_valid_region_state(bContext *C,
                                           wmWindowManager *wm,
                                           wmWindow *win,
                                           ScrArea *area,
                                           SpaceFile *sfile,
                                           FileSelectParams *params)
{
  ARegion *region_tools = BKE_area_find_region_type(area, RGN_TYPE_TOOLS);
  bool needs_init = false; /* To avoid multiple ED_area_init() calls. */

  BLI_assert(region_tools);

  if (sfile->browse_mode == FILE_BROWSE_MODE_ASSETS) {
    file_tool_props_region_ensure(area, region_tools);

    ARegion *region_execute = BKE_area_find_region_type(area, RGN_TYPE_EXECUTE);
    if (region_execute) {
      ED_region_remove(C, area, region_execute);
      needs_init = true;
    }
    ARegion *region_ui = BKE_area_find_region_type(area, RGN_TYPE_UI);
    if (region_ui) {
      ED_region_remove(C, area, region_ui);
      needs_init = true;
    }
  }
  /* If there's an file-operation, ensure we have the option and execute region */
  else if (sfile->op && !BKE_area_find_region_type(area, RGN_TYPE_TOOL_PROPS)) {
    ARegion *region_ui = file_ui_region_ensure(area, region_tools);
    ARegion *region_execute = file_execute_region_ensure(area, region_ui);
    ARegion *region_props = file_tool_props_region_ensure(area, region_execute);

    if (params->flag & FILE_HIDE_TOOL_PROPS) {
      region_props->flag |= RGN_FLAG_HIDDEN;
    }
    else {
      region_props->flag &= ~RGN_FLAG_HIDDEN;
    }

    needs_init = true;
  }
  /* If there's _no_ file-operation, ensure we _don't_ have the option and execute region */
  else if (!sfile->op) {
    ARegion *region_props = BKE_area_find_region_type(area, RGN_TYPE_TOOL_PROPS);
    ARegion *region_execute = BKE_area_find_region_type(area, RGN_TYPE_EXECUTE);
    ARegion *region_ui = file_ui_region_ensure(area, region_tools);
    UNUSED_VARS(region_ui);

    if (region_execute) {
      ED_region_remove(C, area, region_execute);
      needs_init = true;
    }
    if (region_props) {
      ED_region_remove(C, area, region_props);
      needs_init = true;
    }
  }

  if (needs_init) {
    ED_area_init(wm, win, area);
  }
}

/**
 * Tag the space to recreate the file-list.
 */
static void file_tag_reset_list(ScrArea *area, SpaceFile *sfile)
{
  filelist_tag_force_reset(sfile->files);
  ED_area_tag_refresh(area);
}

static void file_refresh(const bContext *C, ScrArea *area)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_ensure_active_params(sfile);
  FileAssetSelectParams *asset_params = ED_fileselect_get_asset_params(sfile);
  struct FSMenu *fsmenu = ED_fsmenu_get();

  fileselect_refresh_params(sfile);
  folder_history_list_ensure_for_active_browse_mode(sfile);

  if (sfile->files && (sfile->tags & FILE_TAG_REBUILD_MAIN_FILES) &&
      filelist_needs_reset_on_main_changes(sfile->files)) {
    filelist_tag_force_reset(sfile->files);
  }
  sfile->tags &= ~FILE_TAG_REBUILD_MAIN_FILES;

  if (!sfile->files) {
    sfile->files = filelist_new(params->type);
    params->highlight_file = -1; /* added this so it opens nicer (ton) */
  }
  filelist_settype(sfile->files, params->type);
  filelist_setdir(sfile->files, params->dir);
  filelist_setrecursion(sfile->files, params->recursion_level);
  filelist_setsorting(sfile->files, params->sort, params->flag & FILE_SORT_INVERT);
  filelist_setlibrary(sfile->files, asset_params ? &asset_params->asset_library : NULL);
  filelist_setfilter_options(
      sfile->files,
      (params->flag & FILE_FILTER) != 0,
      (params->flag & FILE_HIDE_DOT) != 0,
      true, /* Just always hide parent, prefer to not add an extra user option for this. */
      params->filter,
      params->filter_id,
      (params->flag & FILE_ASSETS_ONLY) != 0,
      params->filter_glob,
      params->filter_search);

  /* Update the active indices of bookmarks & co. */
  sfile->systemnr = fsmenu_get_active_indices(fsmenu, FS_CATEGORY_SYSTEM, params->dir);
  sfile->system_bookmarknr = fsmenu_get_active_indices(
      fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, params->dir);
  sfile->bookmarknr = fsmenu_get_active_indices(fsmenu, FS_CATEGORY_BOOKMARKS, params->dir);
  sfile->recentnr = fsmenu_get_active_indices(fsmenu, FS_CATEGORY_RECENT, params->dir);

  if (filelist_needs_force_reset(sfile->files)) {
    filelist_readjob_stop(wm, CTX_data_scene(C));
    filelist_clear(sfile->files);
  }

  if (filelist_needs_reading(sfile->files)) {
    if (!filelist_pending(sfile->files)) {
      filelist_readjob_start(sfile->files, C);
    }
  }

  filelist_sort(sfile->files);
  filelist_filter(sfile->files);

  if (params->display == FILE_IMGDISPLAY) {
    filelist_cache_previews_set(sfile->files, true);
  }
  else {
    filelist_cache_previews_set(sfile->files, false);
    if (sfile->previews_timer) {
      WM_event_remove_timer_notifier(wm, win, sfile->previews_timer);
      sfile->previews_timer = NULL;
    }
  }

  if (params->rename_flag != 0) {
    file_params_renamefile_activate(sfile, params);
  }

  if (sfile->layout) {
    sfile->layout->dirty = true;
  }

  /* Might be called with NULL area, see file_main_region_draw() below. */
  if (area) {
    file_ensure_valid_region_state((bContext *)C, wm, win, area, sfile, params);
  }

  ED_area_tag_redraw(area);
}

void file_on_reload_callback_register(SpaceFile *sfile,
                                      onReloadFn callback,
                                      onReloadFnData custom_data)
{
  sfile->runtime->on_reload = callback;
  sfile->runtime->on_reload_custom_data = custom_data;
}

static void file_on_reload_callback_call(SpaceFile *sfile)
{
  if (sfile->runtime->on_reload == NULL) {
    return;
  }

  sfile->runtime->on_reload(sfile, sfile->runtime->on_reload_custom_data);

  sfile->runtime->on_reload = NULL;
  sfile->runtime->on_reload_custom_data = NULL;
}

static void file_reset_filelist_showing_main_data(ScrArea *area, SpaceFile *sfile)
{
  if (sfile->files && filelist_needs_reset_on_main_changes(sfile->files)) {
    /* Full refresh of the file list if local asset data was changed. Refreshing this view
     * is cheap and users expect this to be updated immediately. */
    file_tag_reset_list(area, sfile);
  }
}

static void file_listener(const wmSpaceTypeListenerParams *params)
{
  ScrArea *area = params->area;
  wmNotifier *wmn = params->notifier;
  SpaceFile *sfile = (SpaceFile *)area->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_SPACE:
      switch (wmn->data) {
        case ND_SPACE_FILE_LIST:
          ED_area_tag_refresh(area);
          break;
        case ND_SPACE_FILE_PARAMS:
          ED_area_tag_refresh(area);
          break;
        case ND_SPACE_FILE_PREVIEW:
          if (sfile->files && filelist_cache_previews_update(sfile->files)) {
            ED_area_tag_refresh(area);
          }
          break;
        case ND_SPACE_ASSET_PARAMS:
          if (sfile->browse_mode == FILE_BROWSE_MODE_ASSETS) {
            ED_area_tag_refresh(area);
          }
          break;
        case ND_SPACE_CHANGED:
          /* If the space was just turned into a file/asset browser, the file-list may need to be
           * updated to reflect latest changes in main data. */
          file_reset_filelist_showing_main_data(area, sfile);
          break;
      }
      switch (wmn->action) {
        case NA_JOB_FINISHED:
          file_on_reload_callback_call(sfile);
          break;
      }
      break;
    case NC_ASSET: {
      switch (wmn->action) {
        case NA_SELECTED:
        case NA_ACTIVATED:
          ED_area_tag_refresh(area);
          break;
        case NA_ADDED:
        case NA_REMOVED:
        case NA_EDITED:
          file_reset_filelist_showing_main_data(area, sfile);
          break;
      }
      break;
    }
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void file_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  /* own keymaps */
  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser Main", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);
}

static void file_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_SPACE:
      switch (wmn->data) {
        case ND_SPACE_FILE_LIST:
          ED_region_tag_redraw(region);
          break;
        case ND_SPACE_FILE_PARAMS:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_ID:
      if (ELEM(wmn->action, NA_SELECTED, NA_ACTIVATED, NA_RENAME)) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void file_main_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  struct wmMsgBus *mbus = params->message_bus;
  bScreen *screen = params->screen;
  ScrArea *area = params->area;
  ARegion *region = params->region;
  SpaceFile *sfile = area->spacedata.first;

  FileSelectParams *file_params = ED_fileselect_ensure_active_params(sfile);
  /* This is a bit odd that a region owns the subscriber for an area,
   * keep for now since all subscribers for WM are regions.
   * May be worth re-visiting later. */
  wmMsgSubscribeValue msg_sub_value_area_tag_refresh = {
      .owner = region,
      .user_data = area,
      .notify = ED_area_do_msg_notify_tag_refresh,
  };

  /* SpaceFile itself. */
  {
    PointerRNA ptr;
    RNA_pointer_create(&screen->id, &RNA_SpaceFileBrowser, sfile, &ptr);

    /* All properties for this space type. */
    WM_msg_subscribe_rna(mbus, &ptr, NULL, &msg_sub_value_area_tag_refresh, __func__);
  }

  /* FileSelectParams */
  {
    PointerRNA ptr;
    RNA_pointer_create(&screen->id, &RNA_FileSelectParams, file_params, &ptr);

    /* All properties for this space type. */
    WM_msg_subscribe_rna(mbus, &ptr, NULL, &msg_sub_value_area_tag_refresh, __func__);
  }
}

static bool file_main_region_needs_refresh_before_draw(SpaceFile *sfile)
{
  /* Needed, because filelist is not initialized on loading */
  if (!sfile->files || filelist_needs_reading(sfile->files)) {
    return true;
  }

  /* File reading tagged the space because main data changed that may require a filelist reset. */
  if (filelist_needs_reset_on_main_changes(sfile->files) &&
      (sfile->tags & FILE_TAG_REBUILD_MAIN_FILES)) {
    return true;
  }

  return false;
}

static void file_main_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_ensure_active_params(sfile);

  View2D *v2d = &region->v2d;

  if (file_main_region_needs_refresh_before_draw(sfile)) {
    file_refresh(C, NULL);
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  /* Allow dynamically sliders to be set, saves notifiers etc. */

  if (params->display == FILE_IMGDISPLAY) {
    v2d->scroll = V2D_SCROLL_RIGHT;
    v2d->keepofs &= ~V2D_LOCKOFS_Y;
    v2d->keepofs |= V2D_LOCKOFS_X;
  }
  else if (params->display == FILE_VERTICALDISPLAY) {
    v2d->scroll = V2D_SCROLL_RIGHT;
    v2d->keepofs &= ~V2D_LOCKOFS_Y;
    v2d->keepofs |= V2D_LOCKOFS_X;
  }
  else {
    v2d->scroll = V2D_SCROLL_BOTTOM;
    v2d->keepofs &= ~V2D_LOCKOFS_X;
    v2d->keepofs |= V2D_LOCKOFS_Y;

    /* XXX this happens on scaling down Screen (like from startup.blend) */
    /* view2d has no type specific for file-window case, which doesn't scroll vertically. */
    if (v2d->cur.ymax < 0) {
      v2d->cur.ymin -= v2d->cur.ymax;
      v2d->cur.ymax = 0;
    }
  }
  /* v2d has initialized flag, so this call will only set the mask correct */
  UI_view2d_region_reinit(v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  /* sets tile/border settings in sfile */
  file_calc_previews(C, region);

  /* set view */
  UI_view2d_view_ortho(v2d);

  /* on first read, find active file */
  if (params->highlight_file == -1) {
    wmEvent *event = CTX_wm_window(C)->eventstate;
    file_highlight_set(sfile, region, event->x, event->y);
  }

  if (!file_draw_hint_if_invalid(sfile, region)) {
    file_draw_list(C, region);
  }

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrollers */
  rcti view_rect;
  ED_fileselect_layout_maskrect(sfile->layout, v2d, &view_rect);
  UI_view2d_scrollers_draw(v2d, &view_rect);
}

static void file_operatortypes(void)
{
  WM_operatortype_append(FILE_OT_select);
  WM_operatortype_append(FILE_OT_select_walk);
  WM_operatortype_append(FILE_OT_select_all);
  WM_operatortype_append(FILE_OT_select_box);
  WM_operatortype_append(FILE_OT_select_bookmark);
  WM_operatortype_append(FILE_OT_highlight);
  WM_operatortype_append(FILE_OT_sort_column_ui_context);
  WM_operatortype_append(FILE_OT_execute);
  WM_operatortype_append(FILE_OT_cancel);
  WM_operatortype_append(FILE_OT_parent);
  WM_operatortype_append(FILE_OT_previous);
  WM_operatortype_append(FILE_OT_next);
  WM_operatortype_append(FILE_OT_refresh);
  WM_operatortype_append(FILE_OT_bookmark_add);
  WM_operatortype_append(FILE_OT_bookmark_delete);
  WM_operatortype_append(FILE_OT_bookmark_cleanup);
  WM_operatortype_append(FILE_OT_bookmark_move);
  WM_operatortype_append(FILE_OT_reset_recent);
  WM_operatortype_append(FILE_OT_hidedot);
  WM_operatortype_append(FILE_OT_filenum);
  WM_operatortype_append(FILE_OT_directory_new);
  WM_operatortype_append(FILE_OT_delete);
  WM_operatortype_append(FILE_OT_rename);
  WM_operatortype_append(FILE_OT_smoothscroll);
  WM_operatortype_append(FILE_OT_filepath_drop);
  WM_operatortype_append(FILE_OT_start_filter);
  WM_operatortype_append(FILE_OT_view_selected);
}

/* NOTE: do not add .blend file reading on this level */
static void file_keymap(struct wmKeyConfig *keyconf)
{
  /* keys for all regions */
  WM_keymap_ensure(keyconf, "File Browser", SPACE_FILE, 0);

  /* keys for main region */
  WM_keymap_ensure(keyconf, "File Browser Main", SPACE_FILE, 0);

  /* keys for button region (top) */
  WM_keymap_ensure(keyconf, "File Browser Buttons", SPACE_FILE, 0);
}

static void file_tools_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  ED_region_panels_init(wm, region);

  /* own keymaps */
  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);
}

static void file_tools_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void file_tools_region_listener(const wmRegionListenerParams *UNUSED(params))
{
}

static void file_tool_props_region_listener(const wmRegionListenerParams *params)
{
  const wmNotifier *wmn = params->notifier;
  ARegion *region = params->region;

  switch (wmn->category) {
    case NC_ID:
      if (ELEM(wmn->action, NA_RENAME)) {
        /* In case the filelist shows ID names. */
        ED_region_tag_redraw(region);
      }
      break;
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void file_header_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_header_init(region);

  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);
}

static void file_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

/* add handlers, stuff you only do once or on area/region changes */
static void file_ui_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);
  region->v2d.keepzoom |= V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y;

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser Buttons", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);
}

static void file_ui_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void file_execution_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);
  region->v2d.keepzoom |= V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y;

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);
}

static void file_execution_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void file_ui_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_SPACE:
      switch (wmn->data) {
        case ND_SPACE_FILE_LIST:
          ED_region_tag_redraw(region);
          break;
      }
      break;
  }
}

static bool filepath_drop_poll(bContext *C,
                               wmDrag *drag,
                               const wmEvent *UNUSED(event),
                               const char **UNUSED(r_tooltip))
{
  if (drag->type == WM_DRAG_PATH) {
    SpaceFile *sfile = CTX_wm_space_file(C);
    if (sfile) {
      return true;
    }
  }
  return false;
}

static void filepath_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  RNA_string_set(drop->ptr, "filepath", drag->path);
}

/* region dropbox definition */
static void file_dropboxes(void)
{
  ListBase *lb = WM_dropboxmap_find("Window", SPACE_EMPTY, RGN_TYPE_WINDOW);

  WM_dropbox_add(lb, "FILE_OT_filepath_drop", filepath_drop_poll, filepath_drop_copy, NULL);
}

static int file_space_subtype_get(ScrArea *area)
{
  SpaceFile *sfile = area->spacedata.first;
  return sfile->browse_mode;
}

static void file_space_subtype_set(ScrArea *area, int value)
{
  SpaceFile *sfile = area->spacedata.first;
  sfile->browse_mode = value;
}

static void file_space_subtype_item_extend(bContext *UNUSED(C),
                                           EnumPropertyItem **item,
                                           int *totitem)
{
  if (U.experimental.use_asset_browser) {
    RNA_enum_items_add(item, totitem, rna_enum_space_file_browse_mode_items);
  }
  else {
    RNA_enum_items_add_value(
        item, totitem, rna_enum_space_file_browse_mode_items, FILE_BROWSE_MODE_FILES);
  }
}

static const char *file_context_dir[] = {"active_file", "id", NULL};

static int /*eContextResult*/ file_context(const bContext *C,
                                           const char *member,
                                           bContextDataResult *result)
{
  bScreen *screen = CTX_wm_screen(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  BLI_assert(!ED_area_is_global(CTX_wm_area(C)));

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, file_context_dir);
    return CTX_RESULT_OK;
  }

  /* The following member checks return file-list data, check if that needs refreshing first. */
  if (file_main_region_needs_refresh_before_draw(sfile)) {
    return CTX_RESULT_NO_DATA;
  }

  if (CTX_data_equals(member, "active_file")) {
    FileDirEntry *file = filelist_file(sfile->files, params->active_file);
    if (file == NULL) {
      return CTX_RESULT_NO_DATA;
    }

    CTX_data_pointer_set(result, &screen->id, &RNA_FileSelectEntry, file);
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "id")) {
    const FileDirEntry *file = filelist_file(sfile->files, params->active_file);
    if (file == NULL) {
      return CTX_RESULT_NO_DATA;
    }

    ID *id = filelist_file_get_id(file);
    if (id == NULL) {
      return CTX_RESULT_NO_DATA;
    }

    CTX_data_id_pointer_set(result, id);
    return CTX_RESULT_OK;
  }

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

static void file_id_remap(ScrArea *area, SpaceLink *sl, ID *UNUSED(old_id), ID *UNUSED(new_id))
{
  SpaceFile *sfile = (SpaceFile *)sl;

  /* If the file shows main data (IDs), tag it for reset.
   * Full reset of the file list if main data was changed, don't even attempt remap pointers.
   * We could give file list types a id-remap callback, but it's probably not worth it.
   * Refreshing local file lists is relatively cheap. */
  file_reset_filelist_showing_main_data(area, sfile);
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_file(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype file");
  ARegionType *art;

  st->spaceid = SPACE_FILE;
  strncpy(st->name, "File", BKE_ST_MAXNAME);

  st->create = file_create;
  st->free = file_free;
  st->init = file_init;
  st->exit = file_exit;
  st->duplicate = file_duplicate;
  st->refresh = file_refresh;
  st->listener = file_listener;
  st->operatortypes = file_operatortypes;
  st->keymap = file_keymap;
  st->dropboxes = file_dropboxes;
  st->space_subtype_item_extend = file_space_subtype_item_extend;
  st->space_subtype_get = file_space_subtype_get;
  st->space_subtype_set = file_space_subtype_set;
  st->context = file_context;
  st->id_remap = file_id_remap;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype file region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = file_main_region_init;
  art->draw = file_main_region_draw;
  art->listener = file_main_region_listener;
  art->message_subscribe = file_main_region_message_subscribe;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype file region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  art->init = file_header_region_init;
  art->draw = file_header_region_draw;
  // art->listener = file_header_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: ui */
  art = MEM_callocN(sizeof(ARegionType), "spacetype file region");
  art->regionid = RGN_TYPE_UI;
  art->keymapflag = ED_KEYMAP_UI;
  art->listener = file_ui_region_listener;
  art->init = file_ui_region_init;
  art->draw = file_ui_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: execution */
  art = MEM_callocN(sizeof(ARegionType), "spacetype file region");
  art->regionid = RGN_TYPE_EXECUTE;
  art->keymapflag = ED_KEYMAP_UI;
  art->listener = file_ui_region_listener;
  art->init = file_execution_region_init;
  art->draw = file_execution_region_draw;
  BLI_addhead(&st->regiontypes, art);
  file_execute_region_panels_register(art);

  /* regions: channels (directories) */
  art = MEM_callocN(sizeof(ARegionType), "spacetype file region");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = 240;
  art->prefsizey = 60;
  art->keymapflag = ED_KEYMAP_UI;
  art->listener = file_tools_region_listener;
  art->init = file_tools_region_init;
  art->draw = file_tools_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: tool properties */
  art = MEM_callocN(sizeof(ARegionType), "spacetype file operator region");
  art->regionid = RGN_TYPE_TOOL_PROPS;
  art->prefsizex = 240;
  art->prefsizey = 60;
  art->keymapflag = ED_KEYMAP_UI;
  art->listener = file_tool_props_region_listener;
  art->init = file_tools_region_init;
  art->draw = file_tools_region_draw;
  BLI_addhead(&st->regiontypes, art);
  file_tool_props_region_panels_register(art);

  BKE_spacetype_register(st);
}

void ED_file_init(void)
{
  ED_file_read_bookmarks();

  if (G.background == false) {
    filelist_init_icons();
  }

  IMB_thumb_makedirs();
}

void ED_file_exit(void)
{
  fsmenu_free();

  if (G.background == false) {
    filelist_free_icons();
  }
}

void ED_file_read_bookmarks(void)
{
  const char *const cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, NULL);

  fsmenu_free();

  fsmenu_read_system(ED_fsmenu_get(), true);

  if (cfgdir) {
    char name[FILE_MAX];
    BLI_join_dirfile(name, sizeof(name), cfgdir, BLENDER_BOOKMARK_FILE);
    fsmenu_read_bookmarks(ED_fsmenu_get(), name);
  }
}
