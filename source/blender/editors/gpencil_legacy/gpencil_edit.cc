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
#include "BKE_gpencil_curve_legacy.h"
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

static GHash *gpencil_strokes_copypastebuf_colors_name_to_material_create(Main *bmain)
{
  GHash *name_to_ma = BLI_ghash_str_new(__func__);

  for (Material *ma = static_cast<Material *>(bmain->materials.first); ma != nullptr;
       ma = static_cast<Material *>(ma->id.next))
  {
    char *name = BKE_id_to_unique_string_key(&ma->id);
    BLI_ghash_insert(name_to_ma, name, ma);
  }

  return name_to_ma;
}

static void gpencil_strokes_copypastebuf_colors_name_to_material_free(GHash *name_to_ma)
{
  BLI_ghash_free(name_to_ma, MEM_freeN, nullptr);
}

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

GHash *gpencil_copybuf_validate_colormap(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  GHash *new_colors = BLI_ghash_int_new("GPencil Paste Dst Colors");
  GHashIterator gh_iter;

  /* For each color, check if exist and add if not */
  GHash *name_to_ma = gpencil_strokes_copypastebuf_colors_name_to_material_create(bmain);

  GHASH_ITER (gh_iter, gpencil_strokes_copypastebuf_colors) {
    int *key = static_cast<int *>(BLI_ghashIterator_getKey(&gh_iter));
    const char *ma_name = static_cast<const char *>(BLI_ghashIterator_getValue(&gh_iter));
    Material *ma = static_cast<Material *>(BLI_ghash_lookup(name_to_ma, ma_name));

    BKE_gpencil_object_material_ensure(bmain, ob, ma);

    /* Store this mapping (for use later when pasting) */
    if (!BLI_ghash_haskey(new_colors, POINTER_FROM_INT(*key))) {
      BLI_ghash_insert(new_colors, POINTER_FROM_INT(*key), ma);
    }
  }

  gpencil_strokes_copypastebuf_colors_name_to_material_free(name_to_ma);

  return new_colors;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Blank Frame Operator
 * \{ */

static int gpencil_blank_frame_add_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Scene *scene = CTX_data_scene(C);
  int cfra = scene->r.cfra;

  bGPDlayer *active_gpl = BKE_gpencil_layer_active_get(gpd);

  const bool all_layers = RNA_boolean_get(op->ptr, "all_layers");

  /* Initialize data-block and an active layer if nothing exists yet. */
  if (ELEM(nullptr, gpd, active_gpl)) {
    /* Let's just be lazy, and call the "Add New Layer" operator,
     * which sets everything up as required. */
    WM_operator_name_call(
        C, "GPENCIL_OT_layer_annotation_add", WM_OP_EXEC_DEFAULT, nullptr, nullptr);
  }

  /* Go through each layer, adding a frame after the active one
   * and/or shunting all the others out of the way
   */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    if ((all_layers == false) && (gpl != active_gpl)) {
      continue;
    }

    /* 1) Check for an existing frame on the current frame */
    bGPDframe *gpf = BKE_gpencil_layer_frame_find(gpl, cfra);
    if (gpf) {
      /* Shunt all frames after (and including) the existing one later by 1-frame */
      for (; gpf; gpf = gpf->next) {
        gpf->framenum += 1;
      }
    }

    /* 2) Now add a new frame, with nothing in it */
    gpl->actframe = BKE_gpencil_layer_frame_get(gpl, cfra, GP_GETFRAME_ADD_NEW);
  }
  CTX_DATA_END;

  /* notifiers */
  if (gpd != nullptr) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_blank_frame_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Insert Blank Frame";
  ot->idname = "GPENCIL_OT_blank_frame_add";
  ot->description =
      "Insert a blank frame on the current frame "
      "(all subsequently existing frames, if any, are shifted right by one frame)";

  /* callbacks */
  ot->exec = gpencil_blank_frame_add_exec;
  ot->poll = gpencil_add_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_boolean(ot->srna,
                         "all_layers",
                         false,
                         "All Layers",
                         "Create blank frame in all layers, not only active");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
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
  const bool is_annotation = STREQ(op->idname, "GPENCIL_OT_annotation_active_frame_delete");

  bGPdata *gpd = (!is_annotation) ? ED_gpencil_data_get_active(C) :
                                    ED_annotation_data_get_active(C);

  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  Scene *scene = CTX_data_scene(C);

  bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_USE_PREV);

  /* if there's no existing Grease-Pencil data there, add some */
  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No grease pencil data");
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

/* -------------------------------------------------------------------- */
/** \name Delete/Dissolve Utilities
 * \{ */

enum eGP_DeleteMode {
  /* delete selected stroke points */
  GP_DELETEOP_POINTS = 0,
  /* delete selected strokes */
  GP_DELETEOP_STROKES = 1,
  /* delete active frame */
  GP_DELETEOP_FRAME = 2,
};

enum eGP_DissolveMode {
  /* dissolve all selected points */
  GP_DISSOLVE_POINTS = 0,
  /* dissolve between selected points */
  GP_DISSOLVE_BETWEEN = 1,
  /* dissolve unselected points */
  GP_DISSOLVE_UNSELECT = 2,
};

/* ----------------------------------- */

/* Split selected strokes into segments, splitting on selected points */
static int gpencil_delete_selected_points(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  bool changed = false;

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

        if (gpf == nullptr) {
          continue;
        }

        /* simply delete strokes which are selected */
        LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* check if the color is editable */
          if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
            continue;
          }

          if (gps->flag & GP_STROKE_SELECT) {
            /* deselect old stroke, since it will be used as template for the new strokes */
            gps->flag &= ~GP_STROKE_SELECT;
            BKE_gpencil_stroke_select_index_reset(gps);

            if (is_curve_edit) {
              bGPDcurve *gpc = gps->editcurve;
              BKE_gpencil_curve_delete_tagged_points(
                  gpd, gpf, gps, gps->next, gpc, GP_CURVE_POINT_SELECT);
            }
            else {
              /* delete unwanted points by splitting stroke into several smaller ones */
              BKE_gpencil_stroke_delete_tagged_points(
                  gpd, gpf, gps, gps->next, GP_SPOINT_SELECT, false, false, 0);
            }

            changed = true;
          }
        }
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

int gpencil_delete_selected_point_wrap(bContext *C)
{
  return gpencil_delete_selected_points(C);
}

/** \} */

bool ED_object_gpencil_exit(Main *bmain, Object *ob)
{
  bool ok = false;
  if (ob) {
    bGPdata *gpd = (bGPdata *)ob->data;

    gpd->flag &= ~(GP_DATA_STROKE_PAINTMODE | GP_DATA_STROKE_EDITMODE | GP_DATA_STROKE_SCULPTMODE |
                   GP_DATA_STROKE_WEIGHTMODE | GP_DATA_STROKE_VERTEXMODE);

    ob->restore_mode = ob->mode;
    ob->mode &= ~(OB_MODE_PAINT_GPENCIL_LEGACY | OB_MODE_EDIT_GPENCIL_LEGACY |
                  OB_MODE_SCULPT_GPENCIL_LEGACY | OB_MODE_WEIGHT_GPENCIL_LEGACY |
                  OB_MODE_VERTEX_GPENCIL_LEGACY);

    /* Inform all evaluated versions that we changed the mode. */
    DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_SYNC_TO_EVAL);
    ok = true;
  }
  return ok;
}

/** \} */
