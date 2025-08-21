/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup speditordock
 */

#include "BKE_screen.hh"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"

#include "ED_screen.hh"

#include "MEM_guardedalloc.h"

#include "UI_interface.hh"
#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"

#include "WM_api.hh"

#include "ED_editor_dock.hh"
#include "editor_dock_intern.hh"

namespace blender::ed::editor_dock {

static void editor_toggle(bContext *C, ScrArea *docked_area, SpaceLink *space)
{
  SpaceLink *sl_old = static_cast<SpaceLink *>(docked_area->spacedata.first);
  const bool is_visible = docked_area->v3->vec.x - docked_area->v1->vec.x > 1;
  const bool change_space = sl_old != space;

  if (change_space) {
    activate_docked_space(C, docked_area, space);
  }

  const int width = UI_UNIT_X * 16;

  rcti screen_rect;
  WM_window_screen_rect_calc(CTX_wm_window(C), &screen_rect);
  if (is_visible && change_space) {
    /* Pass. Just switching editors. */
  }
  else if (is_visible) {
    docked_area->v1->vec.x = docked_area->v2->vec.x = screen_rect.xmin;
    docked_area->v1->vec.y = docked_area->v4->vec.y = screen_rect.ymin;
    docked_area->v3->vec.x = docked_area->v4->vec.x = screen_rect.xmin;
    docked_area->v2->vec.y = docked_area->v3->vec.y = screen_rect.ymin;
  }
  else {
    docked_area->v1->vec.x = docked_area->v2->vec.x = screen_rect.xmax;
    docked_area->v1->vec.y = docked_area->v4->vec.y = screen_rect.ymin;
    docked_area->v3->vec.x = docked_area->v4->vec.x = screen_rect.xmax + width;
    docked_area->v2->vec.y = docked_area->v3->vec.y = screen_rect.ymax;
  }

  bScreen *screen = CTX_wm_screen(C);
  screen->do_refresh = true;
}

static void editor_dock_draw(const bContext *C, Panel *panel)
{
  const bScreen *screen = CTX_wm_screen(C);
  uiLayout &layout = *panel->layout;

  layout.ui_units_x_set(1.5f);
  layout.emboss_set(ui::EmbossType::NoneOrStatus);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if ((area->flag & AREA_FLAG_DOCKED) == 0) {
      continue;
    }

    const bool is_visible = area->v4->vec.x - area->v1->vec.x > 1;

    LISTBASE_FOREACH_BACKWARD (LinkData *, node, &area->docked_spaces_ordered) {
      SpaceLink *space = static_cast<SpaceLink *>(node->data);
      const bool is_active = is_visible && (space == area->spacedata.first);

      uiLayout &row = layout.row(false);
      if (is_active) {
        row.emboss_set(ui::EmbossType::Emboss);
      }

      uiBut *but = uiDefIconBut(layout.block(),
                                ButType::Tab,
                                0,
                                ED_spacedata_icon(space),
                                0,
                                0,
                                UI_UNIT_X * 1.5f,
                                UI_UNIT_Y * 1.5f,
                                nullptr,
                                0,
                                0,
                                "");
      UI_but_func_pushed_state_set(but, [is_active](const uiBut &) { return is_active; });
      UI_but_func_set(but, [area, space](bContext &C) { editor_toggle(&C, area, space); });
      UI_but_func_quick_tooltip_set(but,
                                    [space](const uiBut *) { return ED_spacedata_name(space); });
      UI_but_drawflag_disable(but, UI_BUT_ICON_LEFT);

      ui::block_layout_set_current(layout.block(), &layout);
    }

    layout.op_menu_enum(C, "SCREEN_OT_editor_dock_add_editor", "type", "", ICON_ADD);
  }
}

void main_region_panels_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN<PanelType>("spacetype editor dock main region panels");
  STRNCPY_UTF8(pt->idname, "EDITORDOCK_PT_editor_dock");
  STRNCPY_UTF8(pt->label, N_("Editor Dock"));
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PANEL_TYPE_NO_HEADER;
  pt->draw = editor_dock_draw;
  BLI_addtail(&art->paneltypes, pt);
}

}  // namespace blender::ed::editor_dock
