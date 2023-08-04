/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edutil
 *
 * \name Generic Gizmo Utilities.
 */

#include <cstring>

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "DNA_workspace_types.h"

#include "WM_api.hh"
#include "WM_toolsystem.h"
#include "WM_types.hh"

#include "ED_gizmo_utils.h"

bool ED_gizmo_poll_or_unlink_delayed_from_operator(const bContext *C,
                                                   wmGizmoGroupType *gzgt,
                                                   const char *idname)
{
#if 0
  /* Causes selection to continue showing the last gizmo. */
  wmOperator *op = WM_operator_last_redo(C);
#else
  wmWindowManager *wm = CTX_wm_manager(C);
  wmOperator *op = static_cast<wmOperator *>(wm->operators.last);
#endif

  if (op == nullptr || !STREQ(op->type->idname, idname)) {
    WM_gizmo_group_type_unlink_delayed_ptr(gzgt);
    return false;
  }
  return true;
}

bool ED_gizmo_poll_or_unlink_delayed_from_tool_ex(const bContext *C,
                                                  wmGizmoGroupType *gzgt,
                                                  const char *gzgt_idname)
{
  bToolRef_Runtime *tref_rt = WM_toolsystem_runtime_from_context((bContext *)C);
  if ((tref_rt == nullptr) || !STREQ(gzgt_idname, tref_rt->gizmo_group)) {
    ScrArea *area = CTX_wm_area(C);
    wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
    WM_gizmo_group_unlink_delayed_ptr_from_space(gzgt, gzmap_type, area);
    if (gzgt->users == 0) {
      WM_gizmo_group_type_unlink_delayed_ptr(gzgt);
    }
    return false;
  }
  return true;
}

bool ED_gizmo_poll_or_unlink_delayed_from_tool(const bContext *C, wmGizmoGroupType *gzgt)
{
  return ED_gizmo_poll_or_unlink_delayed_from_tool_ex(C, gzgt, gzgt->idname);
}
