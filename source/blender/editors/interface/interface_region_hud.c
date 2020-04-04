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
 * \ingroup edinterface
 *
 * Floating Persistent Region
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "BLT_translation.h"

#include "ED_screen.h"
#include "ED_undo.h"

#include "GPU_framebuffer.h"
#include "interface_intern.h"

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */
struct HudRegionData {
  short regionid;
};

static bool last_redo_poll(const bContext *C, short region_type)
{
  wmOperator *op = WM_operator_last_redo(C);
  if (op == NULL) {
    return false;
  }

  bool success = false;
  {
    /* Make sure that we are using the same region type as the original
     * operator call. Otherwise we would be polling the operator with the
     * wrong context.
     */
    ScrArea *area = CTX_wm_area(C);
    ARegion *region_op = (region_type != -1) ? BKE_area_find_region_type(area, region_type) : NULL;
    ARegion *region_prev = CTX_wm_region(C);
    CTX_wm_region_set((bContext *)C, region_op);

    if (WM_operator_repeat_check(C, op) && WM_operator_check_ui_empty(op->type) == false) {
      success = WM_operator_poll((bContext *)C, op->type);
    }
    CTX_wm_region_set((bContext *)C, region_prev);
  }
  return success;
}

static void hud_region_hide(ARegion *region)
{
  region->flag |= RGN_FLAG_HIDDEN;
  /* Avoids setting 'AREA_FLAG_REGION_SIZE_UPDATE'
   * since other regions don't depend on this. */
  BLI_rcti_init(&region->winrct, 0, 0, 0, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Redo Panel
 * \{ */

static bool hud_panel_operator_redo_poll(const bContext *C, PanelType *UNUSED(pt))
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_HUD);
  if (region != NULL) {
    struct HudRegionData *hrd = region->regiondata;
    if (hrd != NULL) {
      return last_redo_poll(C, hrd->regionid);
    }
  }
  return false;
}

static void hud_panel_operator_redo_draw_header(const bContext *C, Panel *panel)
{
  wmOperator *op = WM_operator_last_redo(C);
  BLI_strncpy(panel->drawname, WM_operatortype_name(op->type, op->ptr), sizeof(panel->drawname));
}

static void hud_panel_operator_redo_draw(const bContext *C, Panel *panel)
{
  wmOperator *op = WM_operator_last_redo(C);
  if (op == NULL) {
    return;
  }
  if (!WM_operator_check_ui_enabled(C, op->type->name)) {
    uiLayoutSetEnabled(panel->layout, false);
  }
  uiLayout *col = uiLayoutColumn(panel->layout, false);
  uiTemplateOperatorRedoProperties(col, C);
}

static void hud_panels_register(ARegionType *art, int space_type, int region_type)
{
  PanelType *pt;

  pt = MEM_callocN(sizeof(PanelType), __func__);
  strcpy(pt->idname, "OPERATOR_PT_redo");
  strcpy(pt->label, N_("Redo"));
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw_header = hud_panel_operator_redo_draw_header;
  pt->draw = hud_panel_operator_redo_draw;
  pt->poll = hud_panel_operator_redo_poll;
  pt->space_type = space_type;
  pt->region_type = region_type;
  pt->flag |= PNL_DEFAULT_CLOSED;
  BLI_addtail(&art->paneltypes, pt);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for Floating Region
 * \{ */

static void hud_region_init(wmWindowManager *wm, ARegion *region)
{
  ED_region_panels_init(wm, region);
  UI_region_handlers_add(&region->handlers);
  region->flag |= RGN_FLAG_TEMP_REGIONDATA;
}

static void hud_region_free(ARegion *region)
{
  MEM_SAFE_FREE(region->regiondata);
}

static void hud_region_layout(const bContext *C, ARegion *region)
{
  struct HudRegionData *hrd = region->regiondata;
  if (hrd == NULL || !last_redo_poll(C, hrd->regionid)) {
    ED_region_tag_redraw(region);
    hud_region_hide(region);
    return;
  }

  int size_y = region->sizey;

  ED_region_panels_layout(C, region);

  if (region->panels.first && (region->sizey != size_y)) {
    int winx_new = UI_DPI_FAC * (region->sizex + 0.5f);
    int winy_new = UI_DPI_FAC * (region->sizey + 0.5f);
    View2D *v2d = &region->v2d;

    if (region->flag & RGN_FLAG_SIZE_CLAMP_X) {
      CLAMP_MAX(winx_new, region->winx);
    }
    if (region->flag & RGN_FLAG_SIZE_CLAMP_Y) {
      CLAMP_MAX(winy_new, region->winy);
    }

    region->winx = winx_new;
    region->winy = winy_new;

    region->winrct.xmax = (region->winrct.xmin + region->winx) - 1;
    region->winrct.ymax = (region->winrct.ymin + region->winy) - 1;

    UI_view2d_region_reinit(v2d, V2D_COMMONVIEW_PANELS_UI, region->winx, region->winy);

    /* Weak, but needed to avoid glitches, especially with hi-dpi
     * (where resizing the view glitches often).
     * Fortunately this only happens occasionally. */
    ED_region_panels_layout(C, region);
  }

  /* restore view matrix */
  UI_view2d_view_restore(C);
}

static void hud_region_draw(const bContext *C, ARegion *region)
{
  UI_view2d_view_ortho(&region->v2d);
  wmOrtho2_region_pixelspace(region);
  GPU_clear_color(0, 0, 0, 0.0f);
  GPU_clear(GPU_COLOR_BIT);

  if ((region->flag & RGN_FLAG_HIDDEN) == 0) {
    ui_draw_menu_back(NULL,
                      NULL,
                      &(rcti){
                          .xmax = region->winx,
                          .ymax = region->winy,
                      });
    ED_region_panels_draw(C, region);
  }
}

ARegionType *ED_area_type_hud(int space_type)
{
  ARegionType *art = MEM_callocN(sizeof(ARegionType), __func__);
  art->regionid = RGN_TYPE_HUD;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;
  art->layout = hud_region_layout;
  art->draw = hud_region_draw;
  art->init = hud_region_init;
  art->free = hud_region_free;

  /* We need to indicate a preferred size to avoid false `RGN_FLAG_TOO_SMALL`
   * the first time the region is created. */
  art->prefsizex = AREAMINX;
  art->prefsizey = HEADERY;

  hud_panels_register(art, space_type, art->regionid);

  art->lock = 1; /* can become flag, see BKE_spacedata_draw_locks */
  return art;
}

static ARegion *hud_region_add(ScrArea *area)
{
  ARegion *region = MEM_callocN(sizeof(ARegion), "area region");
  ARegion *region_win = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  if (region_win) {
    BLI_insertlinkbefore(&area->regionbase, region_win, region);
  }
  else {
    BLI_addtail(&area->regionbase, region);
  }
  region->regiontype = RGN_TYPE_HUD;
  region->alignment = RGN_ALIGN_FLOAT;
  region->overlap = true;
  region->flag |= RGN_FLAG_DYNAMIC_SIZE;

  if (region_win) {
    float x, y;

    UI_view2d_scroller_size_get(&region_win->v2d, &x, &y);
    region->runtime.offset_x = x;
    region->runtime.offset_y = y;
  }

  return region;
}

void ED_area_type_hud_clear(wmWindowManager *wm, ScrArea *area_keep)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area != area_keep) {
        LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
          if (region->regiontype == RGN_TYPE_HUD) {
            if ((region->flag & RGN_FLAG_HIDDEN) == 0) {
              hud_region_hide(region);
              ED_region_tag_redraw(region);
              ED_area_tag_redraw(area);
            }
          }
        }
      }
    }
  }
}

void ED_area_type_hud_ensure(bContext *C, ScrArea *area)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  ED_area_type_hud_clear(wm, area);

  ARegionType *art = BKE_regiontype_from_id(area->type, RGN_TYPE_HUD);
  if (art == NULL) {
    return;
  }

  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_HUD);

  if (region && (region->flag & RGN_FLAG_HIDDEN_BY_USER)) {
    /* The region is intentionally hidden by the user, don't show it. */
    hud_region_hide(region);
    return;
  }

  bool init = false;
  bool was_hidden = region == NULL || region->visible == false;
  ARegion *region_op = CTX_wm_region(C);
  BLI_assert((region_op == NULL) || (region_op->regiontype != RGN_TYPE_HUD));
  if (!last_redo_poll(C, region_op ? region_op->regiontype : -1)) {
    if (region) {
      ED_region_tag_redraw(region);
      hud_region_hide(region);
    }
    return;
  }

  if (region == NULL) {
    init = true;
    region = hud_region_add(area);
    region->type = art;
  }

  /* Let 'ED_area_update_region_sizes' do the work of placing the region.
   * Otherwise we could set the 'region->winrct' & 'region->winx/winy' here. */
  if (init) {
    area->flag |= AREA_FLAG_REGION_SIZE_UPDATE;
  }
  else {
    if (region->flag & RGN_FLAG_HIDDEN) {
      area->flag |= AREA_FLAG_REGION_SIZE_UPDATE;
    }
    region->flag &= ~RGN_FLAG_HIDDEN;
  }

  {
    struct HudRegionData *hrd = region->regiondata;
    if (hrd == NULL) {
      hrd = MEM_callocN(sizeof(*hrd), __func__);
      region->regiondata = hrd;
    }
    if (region_op) {
      hrd->regionid = region_op->regiontype;
    }
    else {
      hrd->regionid = -1;
    }
  }

  if (init) {
    /* This is needed or 'winrct' will be invalid. */
    wmWindow *win = CTX_wm_window(C);
    ED_area_update_region_sizes(wm, win, area);
  }

  ED_region_floating_initialize(region);
  ED_region_tag_redraw(region);

  /* Reset zoom level (not well supported). */
  region->v2d.cur = region->v2d.tot = (rctf){
      .xmax = region->winx,
      .ymax = region->winy,
  };
  region->v2d.minzoom = 1.0f;
  region->v2d.maxzoom = 1.0f;

  region->visible = !(region->flag & RGN_FLAG_HIDDEN);

  /* We shouldn't need to do this every time :S */
  /* XXX, this is evil! - it also makes the menu show on first draw. :( */
  if (region->visible) {
    ARegion *region_prev = CTX_wm_region(C);
    CTX_wm_region_set((bContext *)C, region);
    hud_region_layout(C, region);
    if (was_hidden) {
      region->winx = region->v2d.winx;
      region->winy = region->v2d.winy;
      region->v2d.cur = region->v2d.tot = (rctf){
          .xmax = region->winx,
          .ymax = region->winy,
      };
    }
    CTX_wm_region_set((bContext *)C, region_prev);
  }

  region->visible = !((region->flag & RGN_FLAG_HIDDEN) || (region->flag & RGN_FLAG_TOO_SMALL));
}

/** \} */
