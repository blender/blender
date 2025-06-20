/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 * Operators for editing Grease Pencil strokes.
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_lib_id.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

#include "UI_view2d.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_image.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "DEG_depsgraph.hh"

#include "gpencil_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Delete Active Frame Operator
 * \{ */

static bool annotation_actframe_delete_poll(bContext *C)
{
  bGPdata *gpd = ED_annotation_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  /* only if there's an active layer with an active frame */
  return (gpl && gpl->actframe);
}

/* delete active frame - wrapper around API calls */
static wmOperatorStatus gpencil_actframe_delete_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_annotation_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  Scene *scene = CTX_data_scene(C);

  bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_USE_PREV);

  /* if there's no existing Grease-Pencil data there, add some */
  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }
  if (ELEM(nullptr, gpl, gpf)) {
    BKE_report(op->reports, RPT_ERROR, "No active frame to delete");
    return OPERATOR_CANCELLED;
  }

  /* delete it... */
  BKE_gpencil_layer_frame_delete(gpl, gpf);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_annotation_active_frame_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Active Frame";
  ot->idname = "GPENCIL_OT_annotation_active_frame_delete";
  ot->description = "Delete the active frame for the active Annotation Layer";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_actframe_delete_exec;
  ot->poll = annotation_actframe_delete_poll;
}

/** \} */
