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
 */

/** \file
 * \ingroup spconsole
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_framebuffer.h"
#include "console_intern.h"  // own include

/* ******************** default callbacks for console space ***************** */

static SpaceLink *console_new(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
  ARegion *region;
  SpaceConsole *sconsole;

  sconsole = MEM_callocN(sizeof(SpaceConsole), "initconsole");
  sconsole->spacetype = SPACE_CONSOLE;

  sconsole->lheight = 14;

  /* header */
  region = MEM_callocN(sizeof(ARegion), "header for console");

  BLI_addtail(&sconsole->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* main region */
  region = MEM_callocN(sizeof(ARegion), "main region for text");

  BLI_addtail(&sconsole->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  /* keep in sync with info */
  region->v2d.scroll |= V2D_SCROLL_RIGHT;
  region->v2d.align |= V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_NEG_Y; /* align bottom left */
  region->v2d.keepofs |= V2D_LOCKOFS_X;
  region->v2d.keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
  region->v2d.keeptot = V2D_KEEPTOT_BOUNDS;
  region->v2d.minzoom = region->v2d.maxzoom = 1.0f;

  /* for now, aspect ratio should be maintained, and zoom is clamped within sane default limits */
  // region->v2d.keepzoom = (V2D_KEEPASPECT|V2D_LIMITZOOM);

  return (SpaceLink *)sconsole;
}

/* not spacelink itself */
static void console_free(SpaceLink *sl)
{
  SpaceConsole *sc = (SpaceConsole *)sl;

  while (sc->scrollback.first) {
    console_scrollback_free(sc, sc->scrollback.first);
  }

  while (sc->history.first) {
    console_history_free(sc, sc->history.first);
  }
}

/* spacetype; init callback */
static void console_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(area))
{
}

static SpaceLink *console_duplicate(SpaceLink *sl)
{
  SpaceConsole *sconsolen = MEM_dupallocN(sl);

  /* clear or remove stuff from old */

  /* TODO - duplicate?, then we also need to duplicate the py namespace */
  BLI_listbase_clear(&sconsolen->scrollback);
  BLI_listbase_clear(&sconsolen->history);

  return (SpaceLink *)sconsolen;
}

/* add handlers, stuff you only do once or on area/region changes */
static void console_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;
  ListBase *lb;

  const float prev_y_min = region->v2d.cur.ymin; /* so re-sizing keeps the cursor visible */

  /* force it on init, for old files, until it becomes config */
  region->v2d.scroll = (V2D_SCROLL_RIGHT);

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_CUSTOM, region->winx, region->winy);

  /* always keep the bottom part of the view aligned, less annoying */
  if (prev_y_min != region->v2d.cur.ymin) {
    const float cur_y_range = BLI_rctf_size_y(&region->v2d.cur);
    region->v2d.cur.ymin = prev_y_min;
    region->v2d.cur.ymax = prev_y_min + cur_y_range;
  }

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Console", SPACE_CONSOLE, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);

  /* add drop boxes */
  lb = WM_dropboxmap_find("Console", SPACE_CONSOLE, RGN_TYPE_WINDOW);

  WM_event_add_dropbox_handler(&region->handlers, lb);
}

/* same as 'text_cursor' */
static void console_cursor(wmWindow *win, ScrArea *UNUSED(area), ARegion *region)
{
  int wmcursor = WM_CURSOR_TEXT_EDIT;
  const wmEvent *event = win->eventstate;
  if (UI_view2d_mouse_in_scrollers(region, &region->v2d, event->x, event->y)) {
    wmcursor = WM_CURSOR_DEFAULT;
  }

  WM_cursor_set(win, wmcursor);
}

/* ************* dropboxes ************* */

static bool id_drop_poll(bContext *UNUSED(C),
                         wmDrag *drag,
                         const wmEvent *UNUSED(event),
                         const char **UNUSED(tooltip))
{
  return WM_drag_ID(drag, 0) != NULL;
}

static void id_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_ID(drag, 0);

  /* copy drag path to properties */
  char *text = RNA_path_full_ID_py(G_MAIN, id);
  RNA_string_set(drop->ptr, "text", text);
  MEM_freeN(text);
}

static bool path_drop_poll(bContext *UNUSED(C),
                           wmDrag *drag,
                           const wmEvent *UNUSED(event),
                           const char **UNUSED(tooltip))
{
  return (drag->type == WM_DRAG_PATH);
}

static void path_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  char pathname[FILE_MAX + 2];
  BLI_snprintf(pathname, sizeof(pathname), "\"%s\"", drag->path);
  RNA_string_set(drop->ptr, "text", pathname);
}

/* this region dropbox definition */
static void console_dropboxes(void)
{
  ListBase *lb = WM_dropboxmap_find("Console", SPACE_CONSOLE, RGN_TYPE_WINDOW);

  WM_dropbox_add(lb, "CONSOLE_OT_insert", id_drop_poll, id_drop_copy);
  WM_dropbox_add(lb, "CONSOLE_OT_insert", path_drop_poll, path_drop_copy);
}

/* ************* end drop *********** */

static void console_main_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceConsole *sc = CTX_wm_space_console(C);
  View2D *v2d = &region->v2d;

  if (BLI_listbase_is_empty(&sc->scrollback)) {
    WM_operator_name_call((bContext *)C, "CONSOLE_OT_banner", WM_OP_EXEC_DEFAULT, NULL);
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  /* worlks best with no view2d matrix set */
  UI_view2d_view_ortho(v2d);

  /* data... */

  console_history_verify(C); /* make sure we have some command line */
  console_textview_main(sc, region);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrollers */
  UI_view2d_scrollers_draw(v2d, NULL);
}

static void console_operatortypes(void)
{
  /* console_ops.c */
  WM_operatortype_append(CONSOLE_OT_move);
  WM_operatortype_append(CONSOLE_OT_delete);
  WM_operatortype_append(CONSOLE_OT_insert);

  WM_operatortype_append(CONSOLE_OT_indent);
  WM_operatortype_append(CONSOLE_OT_indent_or_autocomplete);
  WM_operatortype_append(CONSOLE_OT_unindent);

  /* for use by python only */
  WM_operatortype_append(CONSOLE_OT_history_append);
  WM_operatortype_append(CONSOLE_OT_scrollback_append);

  WM_operatortype_append(CONSOLE_OT_clear);
  WM_operatortype_append(CONSOLE_OT_clear_line);
  WM_operatortype_append(CONSOLE_OT_history_cycle);
  WM_operatortype_append(CONSOLE_OT_copy);
  WM_operatortype_append(CONSOLE_OT_paste);
  WM_operatortype_append(CONSOLE_OT_select_set);
  WM_operatortype_append(CONSOLE_OT_select_word);
}

static void console_keymap(struct wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Console", SPACE_CONSOLE, 0);
}

/****************** header region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void console_header_region_init(wmWindowManager *UNUSED(wm), ARegion *region)
{
  ED_region_header_init(region);
}

static void console_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

static void console_main_region_listener(wmWindow *UNUSED(win),
                                         ScrArea *area,
                                         ARegion *region,
                                         wmNotifier *wmn,
                                         const Scene *UNUSED(scene))
{
  // SpaceInfo *sinfo = area->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_SPACE: {
      if (wmn->data == ND_SPACE_CONSOLE) {
        if (wmn->action == NA_EDITED) {
          if ((wmn->reference && area) && (wmn->reference == area->spacedata.first)) {
            /* we've modified the geometry (font size), re-calculate rect */
            console_textview_update_rect(wmn->reference, region);
            ED_region_tag_redraw(region);
          }
        }
        else {
          /* generic redraw request */
          ED_region_tag_redraw(region);
        }
      }
      break;
    }
  }
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_console(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype console");
  ARegionType *art;

  st->spaceid = SPACE_CONSOLE;
  strncpy(st->name, "Console", BKE_ST_MAXNAME);

  st->new = console_new;
  st->free = console_free;
  st->init = console_init;
  st->duplicate = console_duplicate;
  st->operatortypes = console_operatortypes;
  st->keymap = console_keymap;
  st->dropboxes = console_dropboxes;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype console region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

  art->init = console_main_region_init;
  art->draw = console_main_region_draw;
  art->cursor = console_cursor;
  art->event_cursor = true;
  art->listener = console_main_region_listener;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype console region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  art->init = console_header_region_init;
  art->draw = console_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}
