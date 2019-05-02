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
 * \ingroup edphys
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_dynamicpaint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_object_deform.h"
#include "BKE_dynamicpaint.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_object.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "PIL_time.h"

#include "WM_types.h"
#include "WM_api.h"

#include "physics_intern.h" /* own include */

static int surface_slot_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  DynamicPaintModifierData *pmd = NULL;
  Object *cObject = ED_object_context(C);
  DynamicPaintCanvasSettings *canvas;
  DynamicPaintSurface *surface;

  /* Make sure we're dealing with a canvas */
  pmd = (DynamicPaintModifierData *)modifiers_findByType(cObject, eModifierType_DynamicPaint);
  if (!pmd || !pmd->canvas) {
    return OPERATOR_CANCELLED;
  }

  canvas = pmd->canvas;
  surface = dynamicPaint_createNewSurface(canvas, CTX_data_scene(C));

  if (!surface) {
    return OPERATOR_CANCELLED;
  }

  canvas->active_sur = 0;
  for (surface = surface->prev; surface; surface = surface->prev) {
    canvas->active_sur++;
  }

  return OPERATOR_FINISHED;
}

/* add surface slot */
void DPAINT_OT_surface_slot_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Surface Slot";
  ot->idname = "DPAINT_OT_surface_slot_add";
  ot->description = "Add a new Dynamic Paint surface slot";

  /* api callbacks */
  ot->exec = surface_slot_add_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int surface_slot_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  DynamicPaintModifierData *pmd = NULL;
  Object *obj_ctx = ED_object_context(C);
  DynamicPaintCanvasSettings *canvas;
  DynamicPaintSurface *surface;
  int id = 0;

  /* Make sure we're dealing with a canvas */
  pmd = (DynamicPaintModifierData *)modifiers_findByType(obj_ctx, eModifierType_DynamicPaint);
  if (!pmd || !pmd->canvas) {
    return OPERATOR_CANCELLED;
  }

  canvas = pmd->canvas;
  surface = canvas->surfaces.first;

  /* find active surface and remove it */
  for (; surface; surface = surface->next) {
    if (id == canvas->active_sur) {
      canvas->active_sur -= 1;
      dynamicPaint_freeSurface(pmd, surface);
      break;
    }
    id++;
  }

  DEG_id_tag_update(&obj_ctx->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, obj_ctx);

  return OPERATOR_FINISHED;
}

/* remove surface slot */
void DPAINT_OT_surface_slot_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Surface Slot";
  ot->idname = "DPAINT_OT_surface_slot_remove";
  ot->description = "Remove the selected surface slot";

  /* api callbacks */
  ot->exec = surface_slot_remove_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int type_toggle_exec(bContext *C, wmOperator *op)
{

  Object *cObject = ED_object_context(C);
  Scene *scene = CTX_data_scene(C);
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)modifiers_findByType(
      cObject, eModifierType_DynamicPaint);
  int type = RNA_enum_get(op->ptr, "type");

  if (!pmd) {
    return OPERATOR_CANCELLED;
  }

  /* if type is already enabled, toggle it off */
  if (type == MOD_DYNAMICPAINT_TYPE_CANVAS && pmd->canvas) {
    dynamicPaint_freeCanvas(pmd);
  }
  else if (type == MOD_DYNAMICPAINT_TYPE_BRUSH && pmd->brush) {
    dynamicPaint_freeBrush(pmd);
  }
  /* else create a new type */
  else {
    if (!dynamicPaint_createType(pmd, type, scene)) {
      return OPERATOR_CANCELLED;
    }
  }

  /* update dependency */
  DEG_id_tag_update(&cObject->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, cObject);

  return OPERATOR_FINISHED;
}

void DPAINT_OT_type_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Toggle Type Active";
  ot->idname = "DPAINT_OT_type_toggle";
  ot->description = "Toggle whether given type is active or not";

  /* api callbacks */
  ot->exec = type_toggle_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna,
                      "type",
                      rna_enum_prop_dynamicpaint_type_items,
                      MOD_DYNAMICPAINT_TYPE_CANVAS,
                      "Type",
                      "");
  ot->prop = prop;
}

static int output_toggle_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  DynamicPaintSurface *surface;
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)modifiers_findByType(
      ob, eModifierType_DynamicPaint);
  int output = RNA_enum_get(op->ptr, "output"); /* currently only 1/0 */

  if (!pmd || !pmd->canvas) {
    return OPERATOR_CANCELLED;
  }
  surface = get_activeSurface(pmd->canvas);

  /* if type is already enabled, toggle it off */
  if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
    int exists = dynamicPaint_outputLayerExists(surface, ob, output);
    const char *name;

    if (output == 0) {
      name = surface->output_name;
    }
    else {
      name = surface->output_name2;
    }

    /* Vertex Color Layer */
    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      if (!exists) {
        ED_mesh_color_add(ob->data, name, true, true);
      }
      else {
        ED_mesh_color_remove_named(ob->data, name);
      }
    }
    /* Vertex Weight Layer */
    else if (surface->type == MOD_DPAINT_SURFACE_T_WEIGHT) {
      if (!exists) {
        BKE_object_defgroup_add_name(ob, name);
        DEG_relations_tag_update(CTX_data_main(C));
      }
      else {
        bDeformGroup *defgroup = defgroup_find_name(ob, name);
        if (defgroup) {
          BKE_object_defgroup_remove(ob, defgroup);
          DEG_relations_tag_update(CTX_data_main(C));
        }
      }
    }
  }

  return OPERATOR_FINISHED;
}

void DPAINT_OT_output_toggle(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_output_toggle_types[] = {
      {0, "A", 0, "Output A", ""},
      {1, "B", 0, "Output B", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Toggle Output Layer";
  ot->idname = "DPAINT_OT_output_toggle";
  ot->description = "Add or remove Dynamic Paint output data layer";

  /* api callbacks */
  ot->exec = output_toggle_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "output", prop_output_toggle_types, 0, "Output Toggle", "");
}

/***************************** Image Sequence Baking ******************************/

typedef struct DynamicPaintBakeJob {
  /* from wmJob */
  void *owner;
  short *stop, *do_update;
  float *progress;

  struct Main *bmain;
  Scene *scene;
  Depsgraph *depsgraph;
  Object *ob;

  DynamicPaintSurface *surface;
  DynamicPaintCanvasSettings *canvas;

  int success;
  double start;
} DynamicPaintBakeJob;

static void dpaint_bake_free(void *customdata)
{
  DynamicPaintBakeJob *job = customdata;
  MEM_freeN(job);
}

static void dpaint_bake_endjob(void *customdata)
{
  DynamicPaintBakeJob *job = customdata;
  DynamicPaintCanvasSettings *canvas = job->canvas;

  canvas->flags &= ~MOD_DPAINT_BAKING;

  dynamicPaint_freeSurfaceData(job->surface);

  G.is_rendering = false;
  BKE_spacedata_draw_locks(false);

  WM_set_locked_interface(G_MAIN->wm.first, false);

  /* Bake was successful:
   * Report for ended bake and how long it took */
  if (job->success) {
    /* Show bake info */
    WM_reportf(
        RPT_INFO, "DynamicPaint: Bake complete! (%.2f)", PIL_check_seconds_timer() - job->start);
  }
  else {
    if (strlen(canvas->error)) { /* If an error occurred */
      WM_reportf(RPT_ERROR, "DynamicPaint: Bake failed: %s", canvas->error);
    }
    else { /* User canceled the bake */
      WM_report(RPT_WARNING, "Baking canceled!");
    }
  }
}

/*
 * Do actual bake operation. Loop through to-be-baked frames.
 * Returns 0 on failure.
 */
static void dynamicPaint_bakeImageSequence(DynamicPaintBakeJob *job)
{
  DynamicPaintSurface *surface = job->surface;
  Object *cObject = job->ob;
  DynamicPaintCanvasSettings *canvas = surface->canvas;
  Scene *input_scene = DEG_get_input_scene(job->depsgraph);
  Scene *scene = job->scene;
  int frame = 1, orig_frame;
  int frames;

  frames = surface->end_frame - surface->start_frame + 1;
  if (frames <= 0) {
    BLI_strncpy(canvas->error, N_("No frames to bake"), sizeof(canvas->error));
    return;
  }

  /* Show progress bar. */
  *(job->do_update) = true;

  /* Set frame to start point (also inits modifier data) */
  frame = surface->start_frame;
  orig_frame = input_scene->r.cfra;
  input_scene->r.cfra = (int)frame;
  ED_update_for_newframe(job->bmain, job->depsgraph);

  /* Init surface */
  if (!dynamicPaint_createUVSurface(scene, surface, job->progress, job->do_update)) {
    job->success = 0;
    return;
  }

  /* Loop through selected frames */
  for (frame = surface->start_frame; frame <= surface->end_frame; frame++) {
    /* The first 10% are for createUVSurface... */
    const float progress = 0.1f + 0.9f * (frame - surface->start_frame) / (float)frames;
    surface->current_frame = frame;

    /* If user requested stop, quit baking */
    if (G.is_break) {
      job->success = 0;
      return;
    }

    /* Update progress bar */
    *(job->do_update) = true;
    *(job->progress) = progress;

    /* calculate a frame */
    input_scene->r.cfra = (int)frame;
    ED_update_for_newframe(job->bmain, job->depsgraph);
    if (!dynamicPaint_calculateFrame(surface, job->depsgraph, scene, cObject, frame)) {
      job->success = 0;
      return;
    }

    /*
     * Save output images
     */
    {
      char filename[FILE_MAX];

      /* primary output layer */
      if (surface->flags & MOD_DPAINT_OUT1) {
        /* set filepath */
        BLI_join_dirfile(
            filename, sizeof(filename), surface->image_output_path, surface->output_name);
        BLI_path_frame(filename, frame, 4);

        /* save image */
        dynamicPaint_outputSurfaceImage(surface, filename, 0);
      }
      /* secondary output */
      if (surface->flags & MOD_DPAINT_OUT2 && surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
        /* set filepath */
        BLI_join_dirfile(
            filename, sizeof(filename), surface->image_output_path, surface->output_name2);
        BLI_path_frame(filename, frame, 4);

        /* save image */
        dynamicPaint_outputSurfaceImage(surface, filename, 1);
      }
    }
  }

  input_scene->r.cfra = orig_frame;
  ED_update_for_newframe(job->bmain, job->depsgraph);
}

static void dpaint_bake_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
  DynamicPaintBakeJob *job = customdata;

  job->stop = stop;
  job->do_update = do_update;
  job->progress = progress;
  job->start = PIL_check_seconds_timer();
  job->success = 1;

  G.is_break = false; /* reset BKE_blender_test_break*/

  /* XXX annoying hack: needed to prevent data corruption when changing
   * scene frame in separate threads
   */
  G.is_rendering = true;
  BKE_spacedata_draw_locks(true);

  dynamicPaint_bakeImageSequence(job);

  *do_update = true;
  *stop = 0;
}

/*
 * Bake Dynamic Paint image sequence surface
 */
static int dynamicpaint_bake_exec(struct bContext *C, struct wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Object *ob_ = ED_object_context(C);
  Object *object_eval = DEG_get_evaluated_object(depsgraph, ob_);
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);

  DynamicPaintSurface *surface;

  /*
   * Get modifier data
   */
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)modifiers_findByType(
      object_eval, eModifierType_DynamicPaint);
  if (pmd == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Bake failed: no Dynamic Paint modifier found");
    return OPERATOR_CANCELLED;
  }

  /* Make sure we're dealing with a canvas */
  DynamicPaintCanvasSettings *canvas = pmd->canvas;
  if (canvas == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Bake failed: invalid canvas");
    return OPERATOR_CANCELLED;
  }
  surface = get_activeSurface(canvas);

  /* Set state to baking and init surface */
  canvas->error[0] = '\0';
  canvas->flags |= MOD_DPAINT_BAKING;

  DynamicPaintBakeJob *job = MEM_mallocN(sizeof(DynamicPaintBakeJob), "DynamicPaintBakeJob");
  job->bmain = CTX_data_main(C);
  job->scene = scene_eval;
  job->depsgraph = CTX_data_depsgraph(C);
  job->ob = object_eval;
  job->canvas = canvas;
  job->surface = surface;

  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              CTX_data_scene(C),
                              "Dynamic Paint Bake",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_DPAINT_BAKE);

  WM_jobs_customdata_set(wm_job, job, dpaint_bake_free);
  WM_jobs_timer(wm_job, 0.1, NC_OBJECT | ND_MODIFIER, NC_OBJECT | ND_MODIFIER);
  WM_jobs_callbacks(wm_job, dpaint_bake_startjob, NULL, NULL, dpaint_bake_endjob);

  WM_set_locked_interface(CTX_wm_manager(C), true);

  /* Bake Dynamic Paint */
  WM_jobs_start(CTX_wm_manager(C), wm_job);

  return OPERATOR_FINISHED;
}

void DPAINT_OT_bake(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Dynamic Paint Bake";
  ot->description = "Bake dynamic paint image sequence surface";
  ot->idname = "DPAINT_OT_bake";

  /* api callbacks */
  ot->exec = dynamicpaint_bake_exec;
  ot->poll = ED_operator_object_active_editable;
}
