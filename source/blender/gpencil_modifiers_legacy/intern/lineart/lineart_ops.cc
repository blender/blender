/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_scene_types.h"

#include "MOD_gpencil_legacy_lineart.h"
#include "MOD_lineart.h"

static bool lineart_mod_is_disabled(GpencilModifierData *md)
{
  BLI_assert(md->type == eGpencilModifierType_Lineart);

  const GpencilModifierTypeInfo *info = BKE_gpencil_modifier_get_info(
      GpencilModifierType(md->type));

  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;

  /* Toggle on and off the baked flag as we are only interested in if something else is disabling
   * it. We can assume that the guard function has already toggled this on for all modifiers that
   * are sent here. */
  lmd->flags &= (~LRT_GPENCIL_IS_BAKED);
  bool disabled = info->is_disabled(md, false);
  lmd->flags |= LRT_GPENCIL_IS_BAKED;

  return disabled;
}

static void clear_strokes(Object *ob, GpencilModifierData *md, int frame)
{
  if (md->type != eGpencilModifierType_Lineart) {
    return;
  }
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

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

static bool bake_strokes(Object *ob,
                         Depsgraph *dg,
                         LineartCache **lc,
                         GpencilModifierData *md,
                         int frame,
                         bool is_first)
{
  /* Modifier data sanity check. */
  if (lineart_mod_is_disabled(md)) {
    return false;
  }

  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

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
  LineartCache *local_lc = *lc;
  if (!(*lc)) {
    MOD_lineart_compute_feature_lines(dg, lmd, lc, !(ob->dtx & OB_DRAW_IN_FRONT));
    MOD_lineart_destroy_render_data(lmd);
  }
  else {
    if (is_first || !(lmd->flags & LRT_GPENCIL_USE_CACHE)) {
      MOD_lineart_compute_feature_lines(dg, lmd, &local_lc, !(ob->dtx & OB_DRAW_IN_FRONT));
      MOD_lineart_destroy_render_data(lmd);
    }
    MOD_lineart_chain_clear_picked_flag(local_lc);
    lmd->cache = local_lc;
  }

  MOD_lineart_gpencil_generate(
      lmd->cache,
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
      lmd->edge_types,
      lmd->mask_switches,
      lmd->material_mask_bits,
      lmd->intersection_mask,
      lmd->thickness,
      lmd->opacity,
      lmd->shadow_selection,
      lmd->silhouette_selection,
      lmd->source_vertex_group,
      lmd->vgname,
      lmd->flags,
      lmd->calculation_flags);

  if (!(lmd->flags & LRT_GPENCIL_USE_CACHE)) {
    /* Clear local cache. */
    if (!is_first) {
      MOD_lineart_clear_cache(&local_lc);
    }
    /* Restore the original cache pointer so the modifiers below still have access to the "global"
     * cache. */
    lmd->cache = gpd->runtime.lineart_cache;
  }

  return true;
}

struct LineartBakeJob {
  wmWindowManager *wm;
  void *owner;
  bool *stop, *do_update;
  float *progress;

  /* C or ob must have one != nullptr. */
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
};

static bool lineart_gpencil_bake_single_target(LineartBakeJob *bj, Object *ob, int frame)
{
  bool touched = false;
  if (ob->type != OB_GPENCIL_LEGACY || G.is_break) {
    return false;
  }

  if (bj->overwrite_frames) {
    LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
      if (md->type == eGpencilModifierType_Lineart) {
        clear_strokes(ob, md, frame);
      }
    }
  }

  GpencilLineartLimitInfo info = BKE_gpencil_get_lineart_modifier_limits(ob);

  LineartCache *lc = nullptr;
  bool is_first = true;
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    if (md->type != eGpencilModifierType_Lineart) {
      continue;
    }
    BKE_gpencil_set_lineart_modifier_limits(md, &info, is_first);
    if (bake_strokes(ob, bj->dg, &lc, md, frame, is_first)) {
      touched = true;
      is_first = false;
    }
  }
  MOD_lineart_clear_cache(&lc);

  return touched;
}

static void lineart_gpencil_guard_modifiers(LineartBakeJob *bj)
{
  for (LinkNode *l = bj->objects; l; l = l->next) {
    Object *ob = static_cast<Object *>(l->link);
    LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
      if (md->type == eGpencilModifierType_Lineart) {
        LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
        lmd->flags |= LRT_GPENCIL_IS_BAKED;
      }
    }
  }
}

static void lineart_gpencil_bake_startjob(void *customdata,
                                          bool *stop,
                                          bool *do_update,
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
      Object *ob = static_cast<Object *>(l->link);
      if (lineart_gpencil_bake_single_target(bj, ob, frame)) {
        DEG_id_tag_update((ID *)ob->data, ID_RECALC_GEOMETRY);
        WM_event_add_notifier(bj->C, NC_GPENCIL | ND_DATA | NA_EDITED, ob);
      }
    }

    /* Update and refresh the progress bar. */
    *bj->progress = float(frame - bj->frame_begin) / (bj->frame_end - bj->frame_begin);
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
  LineartBakeJob *bj = static_cast<LineartBakeJob *>(customdata);

  WM_set_locked_interface(CTX_wm_manager(bj->C), false);

  WM_main_add_notifier(NC_SCENE | ND_FRAME, bj->scene);

  for (LinkNode *l = bj->objects; l; l = l->next) {
    WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, (Object *)l->link);
  }

  BLI_linklist_free(bj->objects, nullptr);
}

static int lineart_gpencil_bake_common(bContext *C,
                                       wmOperator *op,
                                       bool bake_all_targets,
                                       bool do_background)
{
  LineartBakeJob *bj = static_cast<LineartBakeJob *>(
      MEM_callocN(sizeof(LineartBakeJob), "LineartBakeJob"));

  if (!bake_all_targets) {
    Object *ob = CTX_data_active_object(C);
    if (!ob || ob->type != OB_GPENCIL_LEGACY) {
      WM_report(RPT_ERROR, "No active object or active object isn't a GPencil object");
      return OPERATOR_FINISHED;
    }
    BLI_linklist_prepend(&bj->objects, ob);
  }
  else {
    /* #CTX_DATA_BEGIN is not available for iterating in objects while using the job system. */
    CTX_DATA_BEGIN (C, Object *, ob, visible_objects) {
      if (ob->type == OB_GPENCIL_LEGACY) {
        LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
          if (md->type == eGpencilModifierType_Lineart) {
            BLI_linklist_prepend(&bj->objects, ob);
            break;
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
        wm_job, lineart_gpencil_bake_startjob, nullptr, nullptr, lineart_gpencil_bake_endjob);

    WM_set_locked_interface(CTX_wm_manager(C), true);

    WM_jobs_start(CTX_wm_manager(C), wm_job);

    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  float pseduo_progress;
  bool pseduo_do_update;
  lineart_gpencil_bake_startjob(bj, nullptr, &pseduo_do_update, &pseduo_progress);

  BLI_linklist_free(bj->objects, nullptr);
  MEM_freeN(bj);

  return OPERATOR_FINISHED;
}

static int lineart_gpencil_bake_strokes_all_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent * /*event*/)
{
  return lineart_gpencil_bake_common(C, op, true, true);
}
static int lineart_gpencil_bake_strokes_all_exec(bContext *C, wmOperator *op)
{
  return lineart_gpencil_bake_common(C, op, true, false);
}
static int lineart_gpencil_bake_strokes_invoke(bContext *C,
                                               wmOperator *op,
                                               const wmEvent * /*event*/)
{
  return lineart_gpencil_bake_common(C, op, false, true);
}
static int lineart_gpencil_bake_strokes_exec(bContext *C, wmOperator *op)
{
  return lineart_gpencil_bake_common(C, op, false, false);
}
static int lineart_gpencil_bake_strokes_common_modal(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent * /*event*/)
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
  /* TODO: move these checks to an operator poll function. */
  if ((ob == nullptr) || ob->type != OB_GPENCIL_LEGACY) {
    return;
  }
  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
    if (md->type != eGpencilModifierType_Lineart) {
      continue;
    }
    LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
    bGPdata *gpd = static_cast<bGPdata *>(ob->data);

    bGPDlayer *gpl = BKE_gpencil_layer_get_by_name(gpd, lmd->target_layer, 1);
    if (!gpl) {
      continue;
    }
    BKE_gpencil_free_frames(gpl);
    BKE_gpencil_frame_addnew(gpl, 0);

    md->mode |= eGpencilModifierMode_Realtime | eGpencilModifierMode_Render;

    lmd->flags &= (~LRT_GPENCIL_IS_BAKED);
  }
  DEG_id_tag_update((ID *)ob->data, ID_RECALC_GEOMETRY);
}

static int lineart_gpencil_clear_strokes_exec(bContext *C, wmOperator * /*op*/)
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

  BKE_report(op->reports, RPT_INFO, "All line art objects are now cleared");

  return OPERATOR_FINISHED;
}

void OBJECT_OT_lineart_bake_strokes(wmOperatorType *ot)
{
  ot->name = "Bake Line Art";
  ot->description = "Bake Line Art for current GPencil object";
  ot->idname = "OBJECT_OT_lineart_bake_strokes";

  ot->invoke = lineart_gpencil_bake_strokes_invoke;
  ot->exec = lineart_gpencil_bake_strokes_exec;
  ot->modal = lineart_gpencil_bake_strokes_common_modal;
}

void OBJECT_OT_lineart_bake_strokes_all(wmOperatorType *ot)
{
  ot->name = "Bake Line Art (All)";
  ot->description = "Bake all Grease Pencil objects that have a line art modifier";
  ot->idname = "OBJECT_OT_lineart_bake_strokes_all";

  ot->invoke = lineart_gpencil_bake_strokes_all_invoke;
  ot->exec = lineart_gpencil_bake_strokes_all_exec;
  ot->modal = lineart_gpencil_bake_strokes_common_modal;
}

void OBJECT_OT_lineart_clear(wmOperatorType *ot)
{
  ot->name = "Clear Baked Line Art";
  ot->description = "Clear all strokes in current GPencil object";
  ot->idname = "OBJECT_OT_lineart_clear";

  ot->exec = lineart_gpencil_clear_strokes_exec;
}

void OBJECT_OT_lineart_clear_all(wmOperatorType *ot)
{
  ot->name = "Clear Baked Line Art (All)";
  ot->description = "Clear all strokes in all Grease Pencil objects that have a line art modifier";
  ot->idname = "OBJECT_OT_lineart_clear_all";

  ot->exec = lineart_gpencil_clear_strokes_all_exec;
}

void WM_operatortypes_lineart()
{
  WM_operatortype_append(OBJECT_OT_lineart_bake_strokes);
  WM_operatortype_append(OBJECT_OT_lineart_bake_strokes_all);
  WM_operatortype_append(OBJECT_OT_lineart_clear);
  WM_operatortype_append(OBJECT_OT_lineart_clear_all);
}
