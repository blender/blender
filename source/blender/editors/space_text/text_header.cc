/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptext
 */

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_screen.h"

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

  arnew = static_cast<ARegion *>(MEM_callocN(sizeof(ARegion), "properties region"));

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

static int text_text_search_exec(bContext *C, wmOperator * /*op*/)
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
