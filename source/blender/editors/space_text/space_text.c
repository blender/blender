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
 * \ingroup sptext
 */

#include <string.h>

#include "DNA_text_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "text_format.h"
#include "text_intern.h" /* own include */

/* ******************** default callbacks for text space ***************** */

static SpaceLink *text_create(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
  ARegion *region;
  SpaceText *stext;

  stext = MEM_callocN(sizeof(SpaceText), "inittext");
  stext->spacetype = SPACE_TEXT;

  stext->lheight = 12;
  stext->tabnumber = 4;
  stext->margin_column = 80;
  stext->showsyntax = true;
  stext->showlinenrs = true;

  /* header */
  region = MEM_callocN(sizeof(ARegion), "header for text");

  BLI_addtail(&stext->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* footer */
  region = MEM_callocN(sizeof(ARegion), "footer for text");
  BLI_addtail(&stext->regionbase, region);
  region->regiontype = RGN_TYPE_FOOTER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;

  /* properties region */
  region = MEM_callocN(sizeof(ARegion), "properties region for text");

  BLI_addtail(&stext->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_RIGHT;
  region->flag = RGN_FLAG_HIDDEN;

  /* main region */
  region = MEM_callocN(sizeof(ARegion), "main region for text");

  BLI_addtail(&stext->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  return (SpaceLink *)stext;
}

/* not spacelink itself */
static void text_free(SpaceLink *sl)
{
  SpaceText *stext = (SpaceText *)sl;

  stext->text = NULL;
  text_free_caches(stext);
}

/* spacetype; init callback */
static void text_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(area))
{
}

static SpaceLink *text_duplicate(SpaceLink *sl)
{
  SpaceText *stextn = MEM_dupallocN(sl);

  /* clear or remove stuff from old */

  stextn->runtime.drawcache = NULL; /* space need its own cache */

  return (SpaceLink *)stextn;
}

static void text_listener(const wmSpaceTypeListenerParams *params)
{
  ScrArea *area = params->area;
  wmNotifier *wmn = params->notifier;
  SpaceText *st = area->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_TEXT:
      /* check if active text was changed, no need to redraw if text isn't active
       * (reference == NULL) means text was unlinked, should update anyway for this
       * case -- no way to know was text active before unlinking or not */
      if (wmn->reference && wmn->reference != st->text) {
        break;
      }

      switch (wmn->data) {
        case ND_DISPLAY:
        case ND_CURSOR:
          ED_area_tag_redraw(area);
          break;
      }

      switch (wmn->action) {
        case NA_EDITED:
          if (st->text) {
            text_drawcache_tag_update(st, 1);
            text_update_edited(st->text);
          }

          ED_area_tag_redraw(area);
          ATTR_FALLTHROUGH; /* fall down to tag redraw */
        case NA_ADDED:
        case NA_REMOVED:
        case NA_SELECTED:
          ED_area_tag_redraw(area);
          break;
      }

      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_TEXT) {
        ED_area_tag_redraw(area);
      }
      break;
  }
}

static void text_operatortypes(void)
{
  WM_operatortype_append(TEXT_OT_new);
  WM_operatortype_append(TEXT_OT_open);
  WM_operatortype_append(TEXT_OT_reload);
  WM_operatortype_append(TEXT_OT_unlink);
  WM_operatortype_append(TEXT_OT_save);
  WM_operatortype_append(TEXT_OT_save_as);
  WM_operatortype_append(TEXT_OT_make_internal);
  WM_operatortype_append(TEXT_OT_run_script);
  WM_operatortype_append(TEXT_OT_refresh_pyconstraints);

  WM_operatortype_append(TEXT_OT_paste);
  WM_operatortype_append(TEXT_OT_copy);
  WM_operatortype_append(TEXT_OT_cut);
  WM_operatortype_append(TEXT_OT_duplicate_line);

  WM_operatortype_append(TEXT_OT_convert_whitespace);
  WM_operatortype_append(TEXT_OT_comment_toggle);
  WM_operatortype_append(TEXT_OT_unindent);
  WM_operatortype_append(TEXT_OT_indent);
  WM_operatortype_append(TEXT_OT_indent_or_autocomplete);

  WM_operatortype_append(TEXT_OT_select_line);
  WM_operatortype_append(TEXT_OT_select_all);
  WM_operatortype_append(TEXT_OT_select_word);

  WM_operatortype_append(TEXT_OT_move_lines);

  WM_operatortype_append(TEXT_OT_jump);
  WM_operatortype_append(TEXT_OT_move);
  WM_operatortype_append(TEXT_OT_move_select);
  WM_operatortype_append(TEXT_OT_delete);
  WM_operatortype_append(TEXT_OT_overwrite_toggle);

  WM_operatortype_append(TEXT_OT_selection_set);
  WM_operatortype_append(TEXT_OT_cursor_set);
  WM_operatortype_append(TEXT_OT_scroll);
  WM_operatortype_append(TEXT_OT_scroll_bar);
  WM_operatortype_append(TEXT_OT_line_number);

  WM_operatortype_append(TEXT_OT_line_break);
  WM_operatortype_append(TEXT_OT_insert);

  WM_operatortype_append(TEXT_OT_find);
  WM_operatortype_append(TEXT_OT_find_set_selected);
  WM_operatortype_append(TEXT_OT_replace);
  WM_operatortype_append(TEXT_OT_replace_set_selected);

  WM_operatortype_append(TEXT_OT_start_find);

  WM_operatortype_append(TEXT_OT_to_3d_object);

  WM_operatortype_append(TEXT_OT_resolve_conflict);

  WM_operatortype_append(TEXT_OT_autocomplete);
}

static void text_keymap(struct wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Text Generic", SPACE_TEXT, 0);
  WM_keymap_ensure(keyconf, "Text", SPACE_TEXT, 0);
}

const char *text_context_dir[] = {"edit_text", NULL};

static int /*eContextResult*/ text_context(const bContext *C,
                                           const char *member,
                                           bContextDataResult *result)
{
  SpaceText *st = CTX_wm_space_text(C);

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, text_context_dir);
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "edit_text")) {
    if (st->text != NULL) {
      CTX_data_id_pointer_set(result, &st->text->id);
    }
    return CTX_RESULT_OK;
  }

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

/********************* main region ********************/

/* add handlers, stuff you only do once or on area/region changes */
static void text_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;
  ListBase *lb;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_STANDARD, region->winx, region->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Text Generic", SPACE_TEXT, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);
  keymap = WM_keymap_ensure(wm->defaultconf, "Text", SPACE_TEXT, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);

  /* add drop boxes */
  lb = WM_dropboxmap_find("Text", SPACE_TEXT, RGN_TYPE_WINDOW);

  WM_event_add_dropbox_handler(&region->handlers, lb);
}

static void text_main_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceText *st = CTX_wm_space_text(C);
  // View2D *v2d = &region->v2d;

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  // UI_view2d_view_ortho(v2d);

  /* data... */
  draw_text_main(st, region);

  /* reset view matrix */
  // UI_view2d_view_restore(C);

  /* scrollers? */
}

static void text_cursor(wmWindow *win, ScrArea *area, ARegion *region)
{
  SpaceText *st = area->spacedata.first;
  int wmcursor = WM_CURSOR_TEXT_EDIT;

  if (st->text && BLI_rcti_isect_pt(&st->runtime.scroll_region_handle,
                                    win->eventstate->xy[0] - region->winrct.xmin,
                                    st->runtime.scroll_region_handle.ymin)) {
    wmcursor = WM_CURSOR_DEFAULT;
  }

  WM_cursor_set(win, wmcursor);
}

/* ************* dropboxes ************* */

static bool text_drop_poll(bContext *UNUSED(C), wmDrag *drag, const wmEvent *UNUSED(event))
{
  if (drag->type == WM_DRAG_PATH) {
    /* rule might not work? */
    if (ELEM(drag->icon, ICON_FILE_SCRIPT, ICON_FILE_TEXT, ICON_FILE_BLANK)) {
      return true;
    }
  }
  return false;
}

static void text_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  /* copy drag path to properties */
  RNA_string_set(drop->ptr, "filepath", drag->path);
}

static bool text_drop_paste_poll(bContext *UNUSED(C), wmDrag *drag, const wmEvent *UNUSED(event))
{
  return (drag->type == WM_DRAG_ID);
}

static void text_drop_paste(wmDrag *drag, wmDropBox *drop)
{
  char *text;
  ID *id = WM_drag_get_local_ID(drag, 0);

  /* copy drag path to properties */
  text = RNA_path_full_ID_py(G_MAIN, id);
  RNA_string_set(drop->ptr, "text", text);
  MEM_freeN(text);
}

/* this region dropbox definition */
static void text_dropboxes(void)
{
  ListBase *lb = WM_dropboxmap_find("Text", SPACE_TEXT, RGN_TYPE_WINDOW);

  WM_dropbox_add(lb, "TEXT_OT_open", text_drop_poll, text_drop_copy, NULL, NULL);
  WM_dropbox_add(lb, "TEXT_OT_insert", text_drop_paste_poll, text_drop_paste, NULL, NULL);
}

/* ************* end drop *********** */

/****************** header region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void text_header_region_init(wmWindowManager *UNUSED(wm), ARegion *region)
{
  ED_region_header_init(region);
}

static void text_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

/****************** properties region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void text_properties_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  ED_region_panels_init(wm, region);

  /* own keymaps */
  keymap = WM_keymap_ensure(wm->defaultconf, "Text Generic", SPACE_TEXT, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);
}

static void text_properties_region_draw(const bContext *C, ARegion *region)
{
  SpaceText *st = CTX_wm_space_text(C);

  ED_region_panels(C, region);

  /* this flag trick is make sure buttons have been added already */
  if (st->flags & ST_FIND_ACTIVATE) {
    if (UI_textbutton_activate_rna(C, region, st, "find_text")) {
      /* if the panel was already open we need to do another redraw */
      ScrArea *area = CTX_wm_area(C);
      WM_event_add_notifier(C, NC_SPACE | ND_SPACE_TEXT, area);
    }
    st->flags &= ~ST_FIND_ACTIVATE;
  }
}

static void text_id_remap(ScrArea *UNUSED(area), SpaceLink *slink, ID *old_id, ID *new_id)
{
  SpaceText *stext = (SpaceText *)slink;

  if (!ELEM(GS(old_id->name), ID_TXT)) {
    return;
  }

  if ((ID *)stext->text == old_id) {
    stext->text = (Text *)new_id;
    id_us_ensure_real(new_id);
  }
}

/********************* registration ********************/

/* only called once, from space/spacetypes.c */
void ED_spacetype_text(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype text");
  ARegionType *art;

  st->spaceid = SPACE_TEXT;
  strncpy(st->name, "Text", BKE_ST_MAXNAME);

  st->create = text_create;
  st->free = text_free;
  st->init = text_init;
  st->duplicate = text_duplicate;
  st->operatortypes = text_operatortypes;
  st->keymap = text_keymap;
  st->listener = text_listener;
  st->context = text_context;
  st->dropboxes = text_dropboxes;
  st->id_remap = text_id_remap;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype text region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = text_main_region_init;
  art->draw = text_main_region_draw;
  art->cursor = text_cursor;
  art->event_cursor = true;

  BLI_addhead(&st->regiontypes, art);

  /* regions: properties */
  art = MEM_callocN(sizeof(ARegionType), "spacetype text region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_COMPACT_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI;

  art->init = text_properties_region_init;
  art->draw = text_properties_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype text region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  art->init = text_header_region_init;
  art->draw = text_header_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: footer */
  art = MEM_callocN(sizeof(ARegionType), "spacetype text region");
  art->regionid = RGN_TYPE_FOOTER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FOOTER;
  art->init = text_header_region_init;
  art->draw = text_header_region_draw;
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);

  /* register formatters */
  ED_text_format_register_py();
  ED_text_format_register_osl();
  ED_text_format_register_lua();
  ED_text_format_register_pov();
  ED_text_format_register_pov_ini();
}
