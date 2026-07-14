/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptext
 */

#include "DNA_windowmanager_types.h"

#include "BLI_listbase.hh"
#include "BLI_string_utf8.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_screen.hh"

#include "DNA_text_types.h"

#include "ED_screen.hh"

#include "WM_types.hh"

#include "UI_interface.hh"

#include "text_intern.hh"

namespace blender {

/* ************************ header area region *********************** */

/************************** properties ******************************/

static ARegion *text_has_properties_region(ScrArea *area)
{
  ARegion *region, *arnew;

  region = BKE_area_find_region_type(area, RGN_TYPE_UI);
  if (region) {
    return region;
  }

  /* Add subdiv level; after header. */
  region = BKE_area_find_region_type(area, RGN_TYPE_HEADER);

  /* Is error! */
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

    const char *active_category = ui::panel_category_active_get(region, false);
    if (active_category && !STREQ(active_category, "Text")) {
      ui::panel_category_active_set(region, "Text");
    }

    ED_region_activate_rna_prop(C, region, st, "find_text", "TEXT_PT_find");
  }
  return OPERATOR_FINISHED;
}

void TEXT_OT_start_find(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Find";
  ot->description = "Start searching text";
  ot->idname = "TEXT_OT_start_find";

  /* API callbacks. */
  ot->exec = text_text_search_exec;
  ot->poll = text_properties_poll;
}

}  // namespace blender
