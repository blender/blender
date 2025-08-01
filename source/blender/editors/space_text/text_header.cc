/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptext
 */

#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "DNA_text_types.h"

#include "ED_screen.hh"

#include "WM_types.hh"

#include "UI_interface.hh"

#include "text_intern.hh"

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
  if (region == nullptr) {
    return nullptr;
  }

  arnew = BKE_area_region_new();

  BLI_insertlinkafter(&area->regionbase, region, arnew);
  arnew->regiontype = RGN_TYPE_UI;
  arnew->alignment = RGN_ALIGN_LEFT;

  arnew->flag = RGN_FLAG_HIDDEN;

  return arnew;
}

static bool text_properties_poll(bContext *C)
{
  return (CTX_wm_space_text(C) != nullptr);
}

static wmOperatorStatus text_text_search_exec(bContext *C, wmOperator * /*op*/)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = text_has_properties_region(area);
  SpaceText *st = CTX_wm_space_text(C);

  if (region) {
    Text *text = st->text;

    /* Use active text selection as search query, if selection is on a single line. */
    if (text && (text->curl == text->sell) && (text->curc != text->selc)) {
      const ARegion *active_region = CTX_wm_region(C);
      if (active_region && active_region->regiontype == RGN_TYPE_WINDOW) {
        const char *sel_start = text->curl->line + std::min(text->curc, text->selc);
        const int sel_len = std::abs(text->curc - text->selc);
        BLI_strncpy_utf8(st->findstr, sel_start, std::min(sel_len + 1, ST_MAX_FIND_STR));
      }
    }

    bool draw = false;

    if (region->flag & RGN_FLAG_HIDDEN) {
      ED_region_toggle_hidden(C, region);
      draw = true;
    }

    const char *active_category = UI_panel_category_active_get(region, false);
    if (active_category && !STREQ(active_category, "Text")) {
      UI_panel_category_active_set(region, "Text");
      draw = true;
    }

    /* Build the layout and draw so `find_text` text button can be activated. */
    if (draw) {
      ED_region_do_layout(C, region);
      ED_region_do_draw(C, region);
    }

    UI_textbutton_activate_rna(C, region, st, "find_text");

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

  /* API callbacks. */
  ot->exec = text_text_search_exec;
  ot->poll = text_properties_poll;
}
