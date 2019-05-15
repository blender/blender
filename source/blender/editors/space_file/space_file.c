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

#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_fileselect.h"

#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "file_intern.h"  // own include
#include "fsmenu.h"
#include "filelist.h"
#include "GPU_framebuffer.h"

/* ******************** default callbacks for file space ***************** */

static SpaceLink *file_new(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
  ARegion *ar;
  SpaceFile *sfile;

  sfile = MEM_callocN(sizeof(SpaceFile), "initfile");
  sfile->spacetype = SPACE_FILE;

  /* header */
  ar = MEM_callocN(sizeof(ARegion), "header for file");
  BLI_addtail(&sfile->regionbase, ar);
  ar->regiontype = RGN_TYPE_HEADER;
  /* Ignore user preference "USER_HEADER_BOTTOM" here (always show top for new types). */
  ar->alignment = RGN_ALIGN_TOP;

  /* Tools region */
  ar = MEM_callocN(sizeof(ARegion), "tools region for file");
  BLI_addtail(&sfile->regionbase, ar);
  ar->regiontype = RGN_TYPE_TOOLS;
  ar->alignment = RGN_ALIGN_LEFT;

  /* Tool props (aka operator) region */
  ar = MEM_callocN(sizeof(ARegion), "tool props region for file");
  BLI_addtail(&sfile->regionbase, ar);
  ar->regiontype = RGN_TYPE_TOOL_PROPS;
  ar->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;

  /* ui list region */
  ar = MEM_callocN(sizeof(ARegion), "ui region for file");
  BLI_addtail(&sfile->regionbase, ar);
  ar->regiontype = RGN_TYPE_UI;
  ar->alignment = RGN_ALIGN_TOP;

  /* main region */
  ar = MEM_callocN(sizeof(ARegion), "main region for file");
  BLI_addtail(&sfile->regionbase, ar);
  ar->regiontype = RGN_TYPE_WINDOW;
  ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
  ar->v2d.align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
  ar->v2d.keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
  ar->v2d.keeptot = V2D_KEEPTOT_STRICT;
  ar->v2d.minzoom = ar->v2d.maxzoom = 1.0f;

  return (SpaceLink *)sfile;
}

/* not spacelink itself */
static void file_free(SpaceLink *sl)
{
  SpaceFile *sfile = (SpaceFile *)sl;

  BLI_assert(sfile->previews_timer == NULL);

  if (sfile->files) {
    // XXXXX would need to do thumbnails_stop here, but no context available
    filelist_freelib(sfile->files);
    filelist_free(sfile->files);
    MEM_freeN(sfile->files);
    sfile->files = NULL;
  }

  if (sfile->folders_prev) {
    folderlist_free(sfile->folders_prev);
    MEM_freeN(sfile->folders_prev);
    sfile->folders_prev = NULL;
  }

  if (sfile->folders_next) {
    folderlist_free(sfile->folders_next);
    MEM_freeN(sfile->folders_next);
    sfile->folders_next = NULL;
  }

  if (sfile->params) {
    MEM_freeN(sfile->params);
    sfile->params = NULL;
  }

  if (sfile->layout) {
    MEM_freeN(sfile->layout);
    sfile->layout = NULL;
  }
}

/* spacetype; init callback, area size changes, screen set, etc */
static void file_init(wmWindowManager *wm, ScrArea *sa)
{
  SpaceFile *sfile = (SpaceFile *)sa->spacedata.first;
  struct FSMenu *fsmenu = ED_fsmenu_get();

  /* refresh system directory list */
  fsmenu_refresh_system_category(fsmenu);

  /* Update bookmarks 'valid' state. */
  fsmenu_refresh_bookmarks_status(wm, fsmenu);

  if (sfile->layout) {
    sfile->layout->dirty = true;
  }
}

static void file_exit(wmWindowManager *wm, ScrArea *sa)
{
  SpaceFile *sfile = (SpaceFile *)sa->spacedata.first;

  if (sfile->previews_timer) {
    WM_event_remove_timer_notifier(wm, NULL, sfile->previews_timer);
    sfile->previews_timer = NULL;
  }

  ED_fileselect_exit(wm, sa, sfile);
}

static SpaceLink *file_duplicate(SpaceLink *sl)
{
  SpaceFile *sfileo = (SpaceFile *)sl;
  SpaceFile *sfilen = MEM_dupallocN(sl);

  /* clear or remove stuff from old */
  sfilen->op = NULL; /* file window doesn't own operators */

  sfilen->previews_timer = NULL;
  sfilen->smoothscroll_timer = NULL;

  if (sfileo->params) {
    sfilen->files = filelist_new(sfileo->params->type);
    sfilen->params = MEM_dupallocN(sfileo->params);
    filelist_setdir(sfilen->files, sfilen->params->dir);
  }

  if (sfileo->folders_prev) {
    sfilen->folders_prev = folderlist_duplicate(sfileo->folders_prev);
  }

  if (sfileo->folders_next) {
    sfilen->folders_next = folderlist_duplicate(sfileo->folders_next);
  }

  if (sfileo->layout) {
    sfilen->layout = MEM_dupallocN(sfileo->layout);
  }
  return (SpaceLink *)sfilen;
}

static void file_refresh(const bContext *C, ScrArea *sa)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_params(sfile);
  struct FSMenu *fsmenu = ED_fsmenu_get();

  if (!sfile->folders_prev) {
    sfile->folders_prev = folderlist_new();
  }
  if (!sfile->files) {
    sfile->files = filelist_new(params->type);
    params->highlight_file = -1; /* added this so it opens nicer (ton) */
  }
  filelist_setdir(sfile->files, params->dir);
  filelist_setrecursion(sfile->files, params->recursion_level);
  filelist_setsorting(sfile->files, params->sort);
  filelist_setfilter_options(sfile->files,
                             (params->flag & FILE_FILTER) != 0,
                             (params->flag & FILE_HIDE_DOT) != 0,
                             false, /* TODO hide_parent, should be controllable? */
                             params->filter,
                             params->filter_id,
                             params->filter_glob,
                             params->filter_search);

  /* Update the active indices of bookmarks & co. */
  sfile->systemnr = fsmenu_get_active_indices(fsmenu, FS_CATEGORY_SYSTEM, params->dir);
  sfile->system_bookmarknr = fsmenu_get_active_indices(
      fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, params->dir);
  sfile->bookmarknr = fsmenu_get_active_indices(fsmenu, FS_CATEGORY_BOOKMARKS, params->dir);
  sfile->recentnr = fsmenu_get_active_indices(fsmenu, FS_CATEGORY_RECENT, params->dir);

  if (filelist_force_reset(sfile->files)) {
    filelist_readjob_stop(wm, sa);
    filelist_clear(sfile->files);
  }

  if (filelist_empty(sfile->files)) {
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
      WM_event_remove_timer_notifier(wm, CTX_wm_window(C), sfile->previews_timer);
      sfile->previews_timer = NULL;
    }
  }

  if (params->rename_flag != 0) {
    file_params_renamefile_activate(sfile, params);
  }

  if (sfile->layout) {
    sfile->layout->dirty = true;
  }

  /* Might be called with NULL sa, see file_main_region_draw() below. */
  if (sa && BKE_area_find_region_type(sa, RGN_TYPE_TOOLS) == NULL) {
    /* Create TOOLS/TOOL_PROPS regions. */
    file_tools_region(sa);

    ED_area_initialize(wm, CTX_wm_window(C), sa);
  }

  ED_area_tag_redraw(sa);
}

static void file_listener(wmWindow *UNUSED(win),
                          ScrArea *sa,
                          wmNotifier *wmn,
                          Scene *UNUSED(scene))
{
  SpaceFile *sfile = (SpaceFile *)sa->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_SPACE:
      switch (wmn->data) {
        case ND_SPACE_FILE_LIST:
          ED_area_tag_refresh(sa);
          break;
        case ND_SPACE_FILE_PARAMS:
          ED_area_tag_refresh(sa);
          break;
        case ND_SPACE_FILE_PREVIEW:
          if (sfile->files && filelist_cache_previews_update(sfile->files)) {
            ED_area_tag_refresh(sa);
          }
          break;
      }
      break;
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void file_main_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);

  /* own keymaps */
  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser Main", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

static void file_main_region_listener(wmWindow *UNUSED(win),
                                      ScrArea *UNUSED(sa),
                                      ARegion *ar,
                                      wmNotifier *wmn,
                                      const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_SPACE:
      switch (wmn->data) {
        case ND_SPACE_FILE_LIST:
          ED_region_tag_redraw(ar);
          break;
        case ND_SPACE_FILE_PARAMS:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
  }
}

static void file_main_region_message_subscribe(const struct bContext *UNUSED(C),
                                               struct WorkSpace *UNUSED(workspace),
                                               struct Scene *UNUSED(scene),
                                               struct bScreen *screen,
                                               struct ScrArea *sa,
                                               struct ARegion *ar,
                                               struct wmMsgBus *mbus)
{
  SpaceFile *sfile = sa->spacedata.first;
  FileSelectParams *params = ED_fileselect_get_params(sfile);
  /* This is a bit odd that a region owns the subscriber for an area,
   * keep for now since all subscribers for WM are regions.
   * May be worth re-visiting later. */
  wmMsgSubscribeValue msg_sub_value_area_tag_refresh = {
      .owner = ar,
      .user_data = sa,
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
    RNA_pointer_create(&screen->id, &RNA_FileSelectParams, params, &ptr);

    /* All properties for this space type. */
    WM_msg_subscribe_rna(mbus, &ptr, NULL, &msg_sub_value_area_tag_refresh, __func__);
  }
}

static void file_main_region_draw(const bContext *C, ARegion *ar)
{
  /* draw entirely, view changes should be handled here */
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_params(sfile);

  View2D *v2d = &ar->v2d;
  View2DScrollers *scrollers;
  float col[3];

  /* Needed, because filelist is not initialized on loading */
  if (!sfile->files || filelist_empty(sfile->files)) {
    file_refresh(C, NULL);
  }

  /* clear and setup matrix */
  UI_GetThemeColor3fv(TH_BACK, col);
  GPU_clear_color(col[0], col[1], col[2], 0.0);
  GPU_clear(GPU_COLOR_BIT);

  /* Allow dynamically sliders to be set, saves notifiers etc. */

  if (params->display == FILE_IMGDISPLAY) {
    v2d->scroll = V2D_SCROLL_RIGHT;
    v2d->keepofs &= ~V2D_LOCKOFS_Y;
    v2d->keepofs |= V2D_LOCKOFS_X;
  }
  else {
    v2d->scroll = V2D_SCROLL_BOTTOM;
    v2d->keepofs &= ~V2D_LOCKOFS_X;
    v2d->keepofs |= V2D_LOCKOFS_Y;

    /* XXX this happens on scaling down Screen (like from startup.blend) */
    /* view2d has no type specific for filewindow case, which doesn't scroll vertically */
    if (v2d->cur.ymax < 0) {
      v2d->cur.ymin -= v2d->cur.ymax;
      v2d->cur.ymax = 0;
    }
  }
  /* v2d has initialized flag, so this call will only set the mask correct */
  UI_view2d_region_reinit(v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);

  /* sets tile/border settings in sfile */
  file_calc_previews(C, ar);

  /* set view */
  UI_view2d_view_ortho(v2d);

  /* on first read, find active file */
  if (params->highlight_file == -1) {
    wmEvent *event = CTX_wm_window(C)->eventstate;
    file_highlight_set(sfile, ar, event->x, event->y);
  }

  file_draw_list(C, ar);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrollers */
  scrollers = UI_view2d_scrollers_calc(v2d, NULL);
  UI_view2d_scrollers_draw(v2d, scrollers);
  UI_view2d_scrollers_free(scrollers);
}

static void file_operatortypes(void)
{
  WM_operatortype_append(FILE_OT_select);
  WM_operatortype_append(FILE_OT_select_walk);
  WM_operatortype_append(FILE_OT_select_all);
  WM_operatortype_append(FILE_OT_select_box);
  WM_operatortype_append(FILE_OT_select_bookmark);
  WM_operatortype_append(FILE_OT_highlight);
  WM_operatortype_append(FILE_OT_execute);
  WM_operatortype_append(FILE_OT_cancel);
  WM_operatortype_append(FILE_OT_parent);
  WM_operatortype_append(FILE_OT_previous);
  WM_operatortype_append(FILE_OT_next);
  WM_operatortype_append(FILE_OT_refresh);
  WM_operatortype_append(FILE_OT_bookmark_toggle);
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

static void file_tools_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  ar->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  ED_region_panels_init(wm, ar);

  /* own keymaps */
  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

static void file_tools_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_panels(C, ar);
}

static void file_tools_region_listener(wmWindow *UNUSED(win),
                                       ScrArea *UNUSED(sa),
                                       ARegion *UNUSED(ar),
                                       wmNotifier *UNUSED(wmn),
                                       const Scene *UNUSED(scene))
{
#if 0
  /* context changes */
  switch (wmn->category) {
    /* pass */
  }
#endif
}

/* add handlers, stuff you only do once or on area/region changes */
static void file_header_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  ED_region_header_init(ar);

  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

static void file_header_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_header(C, ar);
}

/* add handlers, stuff you only do once or on area/region changes */
static void file_ui_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "File Browser Buttons", SPACE_FILE, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

static void file_ui_region_draw(const bContext *C, ARegion *ar)
{
  float col[3];
  /* clear */
  UI_GetThemeColor3fv(TH_BACK, col);
  GPU_clear_color(col[0], col[1], col[2], 0.0);
  GPU_clear(GPU_COLOR_BIT);

  /* scrolling here is just annoying, disable it */
  ar->v2d.cur.ymax = BLI_rctf_size_y(&ar->v2d.cur);
  ar->v2d.cur.ymin = 0;

  /* set view2d view matrix for scrolling (without scrollers) */
  UI_view2d_view_ortho(&ar->v2d);

  file_draw_buttons(C, ar);

  UI_view2d_view_restore(C);
}

static void file_ui_region_listener(wmWindow *UNUSED(win),
                                    ScrArea *UNUSED(sa),
                                    ARegion *ar,
                                    wmNotifier *wmn,
                                    const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_SPACE:
      switch (wmn->data) {
        case ND_SPACE_FILE_LIST:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
  }
}

static bool filepath_drop_poll(bContext *C,
                               wmDrag *drag,
                               const wmEvent *UNUSED(event),
                               const char **UNUSED(tooltip))
{
  if (drag->type == WM_DRAG_PATH) {
    SpaceFile *sfile = CTX_wm_space_file(C);
    if (sfile) {
      return 1;
    }
  }
  return 0;
}

static void filepath_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  RNA_string_set(drop->ptr, "filepath", drag->path);
}

/* region dropbox definition */
static void file_dropboxes(void)
{
  ListBase *lb = WM_dropboxmap_find("Window", SPACE_EMPTY, RGN_TYPE_WINDOW);

  WM_dropbox_add(lb, "FILE_OT_filepath_drop", filepath_drop_poll, filepath_drop_copy);
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_file(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype file");
  ARegionType *art;

  st->spaceid = SPACE_FILE;
  strncpy(st->name, "File", BKE_ST_MAXNAME);

  st->new = file_new;
  st->free = file_free;
  st->init = file_init;
  st->exit = file_exit;
  st->duplicate = file_duplicate;
  st->refresh = file_refresh;
  st->listener = file_listener;
  st->operatortypes = file_operatortypes;
  st->keymap = file_keymap;
  st->dropboxes = file_dropboxes;

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
  art->prefsizey = 60;
  art->keymapflag = ED_KEYMAP_UI;
  art->listener = file_ui_region_listener;
  art->init = file_ui_region_init;
  art->draw = file_ui_region_draw;
  BLI_addhead(&st->regiontypes, art);

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
  art->prefsizex = 0;
  art->prefsizey = 360;
  art->keymapflag = ED_KEYMAP_UI;
  art->listener = file_tools_region_listener;
  art->init = file_tools_region_init;
  art->draw = file_tools_region_draw;
  BLI_addhead(&st->regiontypes, art);
  file_panels_register(art);

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
    BLI_make_file_string("/", name, cfgdir, BLENDER_BOOKMARK_FILE);
    fsmenu_read_bookmarks(ED_fsmenu_get(), name);
  }
}
