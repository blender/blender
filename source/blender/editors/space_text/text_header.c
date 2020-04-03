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

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_screen.h"

#include "WM_types.h"

#include "UI_interface.h"

#include "text_intern.h"

/* ************************ header area region *********************** */

/************************** properties ******************************/

static ARegion *text_has_properties_region(ScrArea *area)
{
  ARegion *region, *arnew;

  region = BKE_area_find_region_type(area, RGN_TYPE_UI);
  if (region) {
    return region;
  }

  /* add subdiv level; after header */
  region = BKE_area_find_region_type(area, RGN_TYPE_HEADER);

  /* is error! */
  if (region == NULL) {
    return NULL;
  }

  arnew = MEM_callocN(sizeof(ARegion), "properties region");

  BLI_insertlinkafter(&area->regionbase, region, arnew);
  arnew->regiontype = RGN_TYPE_UI;
  arnew->alignment = RGN_ALIGN_LEFT;

  arnew->flag = RGN_FLAG_HIDDEN;

  return arnew;
}

static bool text_properties_poll(bContext *C)
{
  return (CTX_wm_space_text(C) != NULL);
}

static int text_text_search_exec(bContext *C, wmOperator *UNUSED(op))
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = text_has_properties_region(area);
  SpaceText *st = CTX_wm_space_text(C);

  if (region) {
    if (region->flag & RGN_FLAG_HIDDEN) {
      ED_region_toggle_hidden(C, region);
    }

    UI_panel_category_active_set(region, "Text");

    /* cannot send a button activate yet for case when region wasn't visible yet */
    /* flag gets checked and cleared in main draw callback */
    st->flags |= ST_FIND_ACTIVATE;

    ED_region_tag_redraw(region);
  }
  return OPERATOR_FINISHED;
}

void TEXT_OT_start_find(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Find";
  ot->description = "Start searching text";
  ot->idname = "TEXT_OT_start_find";

  /* api callbacks */
  ot->exec = text_text_search_exec;
  ot->poll = text_properties_poll;
}

/******************** XXX popup menus *******************/

#if 0
{
  // RMB

  uiPopupMenu *pup;

  if (text) {
    pup = UI_popup_menu_begin(C, IFACE_("Text"), ICON_NONE);
    if (txt_has_sel(text)) {
      uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_cut");
      uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_copy");
    }
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_paste");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_new");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_open");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_save");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_save_as");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_run_script");
    UI_popup_menu_end(C, pup);
  }
  else {
    pup = UI_popup_menu_begin(C, IFACE_("File"), ICON_NONE);
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_new");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_open");
    UI_popup_menu_end(C, pup);
  }
}

{
  // Alt+Shift+E

  uiPopupMenu *pup;

  pup = UI_popup_menu_begin(C, IFACE_("Edit"), ICON_NONE);
  uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_cut");
  uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_copy");
  uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_paste");
  UI_popup_menu_end(C, pup);
}

{
  // Alt+Shift+F

  uiPopupMenu *pup;

  if (text) {
    pup = UI_popup_menu_begin(C, IFACE_("Text"), ICON_NONE);
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_new");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_open");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_save");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_save_as");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_run_script");
    UI_popup_menu_end(C, pup);
  }
  else {
    pup = UI_popup_menu_begin(C, IFACE_("File"), ICON_NONE);
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_new");
    uiItemO(layout, NULL, ICON_NONE, "TEXT_OT_open");
    UI_popup_menu_end(C, pup);
  }
}

{
  // Alt+Shift+V

  uiPopupMenu *pup;

  pup = UI_popup_menu_begin(C, IFACE_("Text"), ICON_NONE);
  uiItemEnumO(layout,
              "TEXT_OT_move",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Top of File"),
              0,
              "type",
              FILE_TOP);
  uiItemEnumO(layout,
              "TEXT_OT_move",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Bottom of File"),
              0,
              "type",
              FILE_BOTTOM);
  uiItemEnumO(layout,
              "TEXT_OT_move",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Page Up"),
              0,
              "type",
              PREV_PAGE);
  uiItemEnumO(layout,
              "TEXT_OT_move",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Page Down"),
              0,
              "type",
              NEXT_PAGE);
  UI_popup_menu_end(C, pup);
}
#endif
