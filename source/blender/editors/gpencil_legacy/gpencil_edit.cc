/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 * Operators for editing Grease Pencil strokes.
 */

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "DNA_object_enums.h"
#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_lasso_2d.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_global.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_paint.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_view2d.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_image.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "ANIM_keyframing.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "gpencil_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Copy/Paste Strokes Utilities
 *
 * Grease Pencil stroke data copy/paste buffer:
 * - The copy operation collects all segments of selected strokes,
 *   dumping "ready to be copied" copies of the strokes into the buffer.
 * - The paste operation makes a copy of those elements, and adds them
 *   to the active layer. This effectively flattens down the strokes
 *   from several different layers into a single layer.
 * \{ */

ListBase gpencil_strokes_copypastebuf = {nullptr, nullptr};

/* Hash for hanging on to all the colors used by strokes in the buffer
 *
 * This is needed to prevent dangling and unsafe pointers when pasting across data-blocks,
 * or after a color used by a stroke in the buffer gets deleted (via user action or undo).
 */
static GHash *gpencil_strokes_copypastebuf_colors = nullptr;

void ED_gpencil_strokes_copybuf_free()
{
  bGPDstroke *gps, *gpsn;

  /* Free the colors buffer.
   * NOTE: This is done before the strokes so that the pointers are still safe. */
  if (gpencil_strokes_copypastebuf_colors) {
    BLI_ghash_free(gpencil_strokes_copypastebuf_colors, nullptr, MEM_freeN);
    gpencil_strokes_copypastebuf_colors = nullptr;
  }

  /* Free the stroke buffer */
  for (gps = static_cast<bGPDstroke *>(gpencil_strokes_copypastebuf.first); gps; gps = gpsn) {
    gpsn = gps->next;

    if (gps->points) {
      MEM_freeN(gps->points);
    }
    if (gps->dvert) {
      BKE_gpencil_free_stroke_weights(gps);
      MEM_freeN(gps->dvert);
    }

    MEM_SAFE_FREE(gps->triangles);

    BLI_freelinkN(&gpencil_strokes_copypastebuf, gps);
  }

  gpencil_strokes_copypastebuf.first = gpencil_strokes_copypastebuf.last = nullptr;
}

/** \} */

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
static int gpencil_actframe_delete_exec(bContext *C, wmOperator *op)
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
