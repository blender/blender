/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spassets
 */

#include "BKE_context.h"

#include "ED_screen.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"

#include "asset_browser_intern.hh"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Select All Operator
 *
 * Asset Browser specific version of #UI_OT_view_select_all() that looks up the asset view, so that
 * it doesn't require the view to be under the cursor. So this operator can be displayed in menus
 * too.
 *
 * \{ */

static int select_all_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  uiViewHandle *view = UI_region_view_find_from_idname(region, "asset grid view");

  const int action = RNA_enum_get(op->ptr, "action");

  if (ui::view_select_all_from_action(view, action)) {
    ED_region_tag_redraw(region);
  }

  return OPERATOR_FINISHED;
}

static void ASSETBROWSER_OT_select_all(wmOperatorType *ot)
{
  ot->name = "Select All";
  ot->idname = "ASSETBROWSER_OT_select_all";
  ot->description = "Select or deselect all displayed assets";

  ot->exec = select_all_exec;
  ot->poll = ED_operator_asset_browser_active;

  WM_operator_properties_select_all(ot);
}

/** \} */

void asset_browser_operatortypes()
{
  WM_operatortype_append(ASSETBROWSER_OT_select_all);
}
