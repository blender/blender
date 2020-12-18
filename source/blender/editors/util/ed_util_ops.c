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
 * Utility operators for UI data or for the UI to use.
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "DNA_windowmanager_types.h"

#include "ED_util.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

static int lib_fake_user_toggle_exec(bContext *C, wmOperator *op)
{
  PropertyPointerRNA pprop;
  PointerRNA idptr = PointerRNA_NULL;

  UI_context_active_but_prop_get_templateID(C, &pprop.ptr, &pprop.prop);

  if (pprop.prop) {
    idptr = RNA_property_pointer_get(&pprop.ptr, pprop.prop);
  }

  if ((pprop.prop == NULL) || RNA_pointer_is_null(&idptr) || !RNA_struct_is_ID(idptr.type)) {
    BKE_report(
        op->reports, RPT_ERROR, "Incorrect context for running data-block fake user toggling");
    return OPERATOR_CANCELLED;
  }

  ID *id = idptr.data;

  if ((id->lib != NULL) || (ELEM(GS(id->name), ID_GR, ID_SCE, ID_SCR, ID_TXT, ID_OB, ID_WS))) {
    BKE_report(op->reports, RPT_ERROR, "Data-block type does not support fake user");
    return OPERATOR_CANCELLED;
  }

  if (ID_FAKE_USERS(id)) {
    id_fake_user_clear(id);
  }
  else {
    id_fake_user_set(id);
  }

  return OPERATOR_FINISHED;
}

static void ED_OT_lib_fake_user_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Fake User";
  ot->description = "Save this data-block even if it has no users";
  ot->idname = "ED_OT_lib_fake_user_toggle";

  /* api callbacks */
  ot->exec = lib_fake_user_toggle_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

static int lib_unlink_exec(bContext *C, wmOperator *op)
{
  PropertyPointerRNA pprop;
  PointerRNA idptr;

  UI_context_active_but_prop_get_templateID(C, &pprop.ptr, &pprop.prop);

  if (pprop.prop) {
    idptr = RNA_property_pointer_get(&pprop.ptr, pprop.prop);
  }

  if ((pprop.prop == NULL) || RNA_pointer_is_null(&idptr) || !RNA_struct_is_ID(idptr.type)) {
    BKE_report(
        op->reports, RPT_ERROR, "Incorrect context for running data-block fake user toggling");
    return OPERATOR_CANCELLED;
  }

  memset(&idptr, 0, sizeof(idptr));
  RNA_property_pointer_set(&pprop.ptr, pprop.prop, idptr, NULL);
  RNA_property_update(C, &pprop.ptr, pprop.prop);

  return OPERATOR_FINISHED;
}

static void ED_OT_lib_unlink(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unlink Data-Block";
  ot->description = "Remove a usage of a data-block, clearing the assignment";
  ot->idname = "ED_OT_lib_unlink";

  /* api callbacks */
  ot->exec = lib_unlink_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

static int ed_flush_edits_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  ED_editors_flush_edits(bmain);
  return OPERATOR_FINISHED;
}

static void ED_OT_flush_edits(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flush Edits";
  ot->description = "Flush edit data from active editing modes";
  ot->idname = "ED_OT_flush_edits";

  /* api callbacks */
  ot->exec = ed_flush_edits_exec;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;
}

void ED_operatortypes_edutils(void)
{
  WM_operatortype_append(ED_OT_lib_fake_user_toggle);
  WM_operatortype_append(ED_OT_lib_unlink);
  WM_operatortype_append(ED_OT_flush_edits);
}
