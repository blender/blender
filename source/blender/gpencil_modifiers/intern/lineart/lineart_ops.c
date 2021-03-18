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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "BLI_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"

#include "UI_resources.h"

#include "MOD_gpencil_lineart.h"
#include "MOD_lineart.h"

#include "lineart_intern.h"

static void clear_strokes(Object *ob, GpencilModifierData *md, int frame)
{
  if (md->type != eGpencilModifierType_Lineart) {
    return;
  }
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
  bGPdata *gpd = ob->data;

  bGPDlayer *gpl = BKE_gpencil_layer_get_by_name(gpd, lmd->target_layer, 1);
  if (!gpl) {
    return;
  }
  bGPDframe *gpf = BKE_gpencil_layer_frame_find(gpl, frame);

  if (!gpf) {
    /* No greasepencil frame found. */
    return;
  }

  BKE_gpencil_layer_frame_delete(gpl, gpf);
}

static bool bake_strokes(Object *ob, Depsgraph *dg, GpencilModifierData *md, int frame)
{
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
  bGPdata *gpd = ob->data;

  bGPDlayer *gpl = BKE_gpencil_layer_get_by_name(gpd, lmd->target_layer, 1);
  if (!gpl) {
    return false;
  }
  bool only_use_existing_gp_frames = false;
  bGPDframe *gpf = (only_use_existing_gp_frames ?
                        BKE_gpencil_layer_frame_find(gpl, frame) :
                        BKE_gpencil_layer_frame_get(gpl, frame, GP_GETFRAME_ADD_NEW));

  if (!gpf) {
    /* No greasepencil frame created or found. */
    return false;
  }

  MOD_lineart_compute_feature_lines(dg, lmd);

  MOD_lineart_gpencil_generate(
      lmd->render_buffer,
      dg,
      ob,
      gpl,
      gpf,
      lmd->source_type,
      lmd->source_type == LRT_SOURCE_OBJECT ? (void *)lmd->source_object :
                                              (void *)lmd->source_collection,
      lmd->level_start,
      lmd->use_multiple_levels ? lmd->level_end : lmd->level_start,
      lmd->target_material ? BKE_gpencil_object_material_index_get(ob, lmd->target_material) : 0,
      lmd->line_types,
      lmd->transparency_flags,
      lmd->transparency_mask,
      lmd->thickness,
      lmd->opacity,
      lmd->pre_sample_length,
      lmd->source_vertex_group,
      lmd->vgname,
      lmd->flags);

  MOD_lineart_destroy_render_data(lmd);

  return true;
}

typedef struct LineartBakeJob {
  wmWindowManager *wm;
  void *owner;
  short *stop, *do_update;
  float *progress;

  /* C or ob must have one != NULL. */
  bContext *C;
  LinkNode *objects;
  Scene *scene;
  Depsgraph *dg;
  int frame;
  int frame_begin;
  int frame_end;
  int frame_orig;
  int frame_increment;
  bool overwrite_frames;
} LineartBakeJob;

static bool lineart_gpencil_bake_single_target(LineartBakeJob *bj, Object *ob, int frame)
{
  bool touched = false;
  if (ob->type != OB_GPENCIL || G.is_break) {
    return false;
  }

  if (bj->overwrite_frames) {
    LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
      clear_strokes(ob, md, frame);
    }
  }

  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    if (bake_strokes(ob, bj->dg, md, frame)) {
      touched = true;
    }
  }

  return touched;
}

static void lineart_gpencil_guard_modifiers(LineartBakeJob *bj)
{
  for (LinkNode *l = bj->objects; l; l = l->next) {
    Object *ob = l->link;
    LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
      if (md->type == eGpencilModifierType_Lineart) {
        LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
        lmd->flags |= LRT_GPENCIL_IS_BAKED;
      }
    }
  }
}

static void lineart_gpencil_bake_startjob(void *customdata,
                                          short *stop,
                                          short *do_update,
                                          float *progress)
{
  LineartBakeJob *bj = (LineartBakeJob *)customdata;
  bj->stop = stop;
  bj->do_update = do_update;
  bj->progress = progress;

  lineart_gpencil_guard_modifiers(bj);

  for (int frame = bj->frame_begin; frame <= bj->frame_end; frame += bj->frame_increment) {

    if (G.is_break) {
      G.is_break = false;
      break;
    }

    BKE_scene_frame_set(bj->scene, frame);
    BKE_scene_graph_update_for_newframe(bj->dg);

    for (LinkNode *l = bj->objects; l; l = l->next) {
      Object *ob = l->link;
      if (lineart_gpencil_bake_single_target(bj, ob, frame)) {
        DEG_id_tag_update((struct ID *)ob->data, ID_RECALC_GEOMETRY);
        WM_event_add_notifier(bj->C, NC_GPENCIL | ND_DATA | NA_EDITED, ob);
      }
    }

    /* Update and refresh the progress bar. */
    *bj->progress = (float)(frame - bj->frame_begin) / (bj->frame_end - bj->frame_begin);
    *bj->do_update = true;
  }

  /* This need to be reset manually. */
  G.is_break = false;

  /* Restore original frame. */
  BKE_scene_frame_set(bj->scene, bj->frame_orig);
  BKE_scene_graph_update_for_newframe(bj->dg);
}

static void lineart_gpencil_bake_endjob(void *customdata)
{
  LineartBakeJob *bj = customdata;

  WM_set_locked_interface(CTX_wm_manager(bj->C), false);

  WM_main_add_notifier(NC_SCENE | ND_FRAME, bj->scene);

  for (LinkNode *l = bj->objects; l; l = l->next) {
    WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, (Object *)l->link);
  }

  BLI_linklist_free(bj->objects, NULL);
}

static int lineart_gpencil_bake_common(bContext *C,
                                       wmOperator *op,
                                       bool bake_all_targets,
                                       bool do_background)
{
  LineartBakeJob *bj = MEM_callocN(sizeof(LineartBakeJob), "LineartBakeJob");

  if (!bake_all_targets) {
    Object *ob = CTX_data_active_object(C);
    if (!ob || ob->type != OB_GPENCIL) {
      WM_report(RPT_ERROR, "No active object or active object isn't a GPencil object.");
      return OPERATOR_FINISHED;
    }
    BLI_linklist_prepend(&bj->objects, ob);
  }
  else {
    /* CTX_DATA_BEGIN is not available for interating in objects while using the Job system. */
    CTX_DATA_BEGIN (C, Object *, ob, visible_objects) {
      if (ob->type == OB_GPENCIL) {
        LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
          if (md->type == eGpencilModifierType_Lineart) {
            BLI_linklist_prepend(&bj->objects, ob);
          }
        }
      }
    }
    CTX_DATA_END;
  }
  bj->C = C;
  Scene *scene = CTX_data_scene(C);
  bj->scene = scene;
  bj->dg = CTX_data_depsgraph_pointer(C);
  bj->frame_begin = scene->r.sfra;
  bj->frame_end = scene->r.efra;
  bj->frame_orig = scene->r.cfra;
  bj->frame_increment = scene->r.frame_step;
  bj->overwrite_frames = true;

  if (do_background) {
    wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                                CTX_wm_window(C),
                                scene,
                                "Line Art",
                                WM_JOB_PROGRESS,
                                WM_JOB_TYPE_LINEART);

    WM_jobs_customdata_set(wm_job, bj, MEM_freeN);
    WM_jobs_timer(wm_job, 0.1, NC_GPENCIL | ND_DATA | NA_EDITED, NC_GPENCIL | ND_DATA | NA_EDITED);
    WM_jobs_callbacks(
        wm_job, lineart_gpencil_bake_startjob, NULL, NULL, lineart_gpencil_bake_endjob);

    WM_set_locked_interface(CTX_wm_manager(C), true);

    WM_jobs_start(CTX_wm_manager(C), wm_job);

    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  float pseduo_progress;
  short pseduo_do_update;
  lineart_gpencil_bake_startjob(bj, NULL, &pseduo_do_update, &pseduo_progress);

  BLI_linklist_free(bj->objects, NULL);
  MEM_freeN(bj);

  return OPERATOR_FINISHED;
}

static int lineart_gpencil_bake_strokes_all_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *UNUSED(event))
{
  return lineart_gpencil_bake_common(C, op, true, true);
}
static int lineart_gpencil_bake_strokes_all_exec(bContext *C, wmOperator *op)
{
  return lineart_gpencil_bake_common(C, op, true, false);
}
static int lineart_gpencil_bake_strokes_invoke(bContext *C,
                                               wmOperator *op,
                                               const wmEvent *UNUSED(event))
{
  return lineart_gpencil_bake_common(C, op, false, true);
}
static int lineart_gpencil_bake_strokes_exec(bContext *C, wmOperator *op)
{
  return lineart_gpencil_bake_common(C, op, false, false);

  return OPERATOR_FINISHED;
}
static int lineart_gpencil_bake_strokes_commom_modal(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *UNUSED(event))
{
  Scene *scene = (Scene *)op->customdata;

  /* no running blender, remove handler and pass through. */
  if (0 == WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_LINEART)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  return OPERATOR_PASS_THROUGH;
}

static void lineart_gpencil_clear_strokes_exec_common(Object *ob)
{
  if (ob->type != OB_GPENCIL) {
    return;
  }
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    if (md->type != eGpencilModifierType_Lineart) {
      continue;
    }
    LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
    bGPdata *gpd = ob->data;

    bGPDlayer *gpl = BKE_gpencil_layer_get_by_name(gpd, lmd->target_layer, 1);
    if (!gpl) {
      continue;
    }
    BKE_gpencil_free_frames(gpl);

    md->mode |= eGpencilModifierMode_Realtime | eGpencilModifierMode_Render;

    lmd->flags &= (~LRT_GPENCIL_IS_BAKED);
  }
  DEG_id_tag_update((struct ID *)ob->data, ID_RECALC_GEOMETRY);
}

static int lineart_gpencil_clear_strokes_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);

  lineart_gpencil_clear_strokes_exec_common(ob);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, ob);

  return OPERATOR_FINISHED;
}
static int lineart_gpencil_clear_strokes_all_exec(bContext *C, wmOperator *op)
{
  CTX_DATA_BEGIN (C, Object *, ob, visible_objects) {
    lineart_gpencil_clear_strokes_exec_common(ob);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, ob);
  }
  CTX_DATA_END;

  BKE_report(op->reports, RPT_INFO, "All line art objects are now cleared.");

  return OPERATOR_FINISHED;
}

/* Bake all line art modifiers on the current object. */
void OBJECT_OT_lineart_bake_strokes(wmOperatorType *ot)
{
  ot->name = "Bake Line Art";
  ot->description = "Bake Line Art for current GPencil object";
  ot->idname = "OBJECT_OT_lineart_bake_strokes";

  ot->invoke = lineart_gpencil_bake_strokes_invoke;
  ot->exec = lineart_gpencil_bake_strokes_exec;
  ot->modal = lineart_gpencil_bake_strokes_commom_modal;
}

/* Bake all lineart objects in the scene. */
void OBJECT_OT_lineart_bake_strokes_all(wmOperatorType *ot)
{
  ot->name = "Bake Line Art (All)";
  ot->description = "Bake all Grease Pencil objects that have a line art modifier";
  ot->idname = "OBJECT_OT_lineart_bake_strokes_all";

  ot->invoke = lineart_gpencil_bake_strokes_all_invoke;
  ot->exec = lineart_gpencil_bake_strokes_all_exec;
  ot->modal = lineart_gpencil_bake_strokes_commom_modal;
}

/* clear all line art modifiers on the current object. */
void OBJECT_OT_lineart_clear(wmOperatorType *ot)
{
  ot->name = "Clear Baked Line Art";
  ot->description = "Clear all strokes in current GPencil obejct";
  ot->idname = "OBJECT_OT_lineart_clear";

  ot->exec = lineart_gpencil_clear_strokes_exec;
}

/* clear all lineart objects in the scene. */
void OBJECT_OT_lineart_clear_all(wmOperatorType *ot)
{
  ot->name = "Clear Baked Line Art (All)";
  ot->description = "Clear all strokes in all Grease Pencil objects that have a line art modifier";
  ot->idname = "OBJECT_OT_lineart_clear_all";

  ot->exec = lineart_gpencil_clear_strokes_all_exec;
}

void WM_operatortypes_lineart(void)
{
  WM_operatortype_append(OBJECT_OT_lineart_bake_strokes);
  WM_operatortype_append(OBJECT_OT_lineart_bake_strokes_all);
  WM_operatortype_append(OBJECT_OT_lineart_clear);
  WM_operatortype_append(OBJECT_OT_lineart_clear_all);
}
