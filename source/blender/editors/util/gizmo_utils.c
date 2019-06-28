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
 * \ingroup edutil
 *
 * \name Generic Gizmo Utilities.
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "DNA_workspace_types.h"

#include "WM_types.h"
#include "WM_api.h"
#include "WM_toolsystem.h"

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
  wmOperator *op = wm->operators.last;
#endif

  if (op == NULL || !STREQ(op->type->idname, idname)) {
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
  if ((tref_rt == NULL) || !STREQ(gzgt_idname, tref_rt->gizmo_group)) {
    ScrArea *sa = CTX_wm_area(C);
    wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
    WM_gizmo_group_unlink_delayed_ptr_from_space(gzgt, gzmap_type, sa);
    if (gzgt->users == 0) {
      WM_gizmo_group_type_unlink_delayed_ptr(gzgt);
    }
    return false;
  }
  return true;
}

/** Can use this as poll function directly. */
bool ED_gizmo_poll_or_unlink_delayed_from_tool(const bContext *C, wmGizmoGroupType *gzgt)
{
  return ED_gizmo_poll_or_unlink_delayed_from_tool_ex(C, gzgt, gzgt->idname);
}
