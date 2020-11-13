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
 *
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 * Operators for editing Grease Pencil strokes
 */

/** \file
 * \ingroup edgpencil
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_curve.h"
#include "BKE_gpencil_geom.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_gpencil.h"

#include "DEG_depsgraph.h"

#include "gpencil_intern.h"

/* Poll callback for checking if there is an active layer and we are in curve edit mode. */
static bool gpencil_curve_edit_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == NULL) || (ob->type != OB_GPENCIL)) {
    return false;
  }
  bGPdata *gpd = (bGPdata *)ob->data;
  if (!GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) {
    return false;
  }

  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
  return (gpl != NULL);
}

static int gpencil_stroke_enter_editcurve_mode_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ob->data;

  float error_threshold = RNA_float_get(op->ptr, "error_threshold");
  gpd->curve_edit_threshold = error_threshold;

  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      if (gpf == gpl->actframe) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* only allow selected and non-converted strokes to be transformed */
          if ((gps->flag & GP_STROKE_SELECT && gps->editcurve == NULL) ||
              (gps->editcurve != NULL && gps->editcurve->flag & GP_CURVE_NEEDS_STROKE_UPDATE)) {
            BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps);
            /* Update the selection from the stroke to the curve. */
            BKE_gpencil_editcurve_stroke_sync_selection(gps, gps->editcurve);
            gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
            BKE_gpencil_stroke_geometry_update(gpd, gps);
          }
        }
      }
    }
  }

  gpd->flag |= GP_DATA_CURVE_EDIT_MODE;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_enter_editcurve_mode(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Enter curve edit mode";
  ot->idname = "GPENCIL_OT_stroke_enter_editcurve_mode";
  ot->description = "Called to transform a stroke into a curve";

  /* api callbacks */
  ot->exec = gpencil_stroke_enter_editcurve_mode_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_float(ot->srna,
                       "error_threshold",
                       0.1f,
                       FLT_MIN,
                       100.0f,
                       "Error Threshold",
                       "Threshold on the maximum deviation from the actual stroke",
                       FLT_MIN,
                       10.f);
  RNA_def_property_ui_range(prop, FLT_MIN, 10.0f, 0.1f, 5);
}

static int gpencil_editcurve_set_handle_type_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ob->data;
  const int handle_type = RNA_enum_get(op->ptr, "type");

  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
  {
    for (int i = 0; i < gpc->tot_curve_points; i++) {
      bGPDcurve_point *gpc_pt = &gpc->curve_points[i];

      if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
        BezTriple *bezt = &gpc_pt->bezt;

        if (bezt->f2 & SELECT) {
          bezt->h1 = handle_type;
          bezt->h2 = handle_type;
        }
        else {
          if (bezt->f1 & SELECT) {
            bezt->h1 = handle_type;
          }
          if (bezt->f3 & SELECT) {
            bezt->h2 = handle_type;
          }
        }
      }
    }

    BKE_gpencil_editcurve_recalculate_handles(gps);
    gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
    BKE_gpencil_stroke_geometry_update(gpd, gps);
  }
  GP_EDITABLE_CURVES_END(gps_iter);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_editcurve_set_handle_type(wmOperatorType *ot)
{
  static const EnumPropertyItem editcurve_handle_type_items[] = {
      {HD_FREE, "FREE", 0, "Free", ""},
      {HD_AUTO, "AUTOMATIC", 0, "Automatic", ""},
      {HD_VECT, "VECTOR", 0, "Vector", ""},
      {HD_ALIGN, "ALIGNED", 0, "Aligned", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Set handle type";
  ot->idname = "GPENCIL_OT_stroke_editcurve_set_handle_type";
  ot->description = "Set the type of a edit curve handle";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gpencil_editcurve_set_handle_type_exec;
  ot->poll = gpencil_curve_edit_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", editcurve_handle_type_items, 1, "Type", "Spline type");
}

/** \} */
