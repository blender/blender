/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include <algorithm>

#include "BLI_listbase.h"

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"
#include "BKE_modifier.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DNA_curves_types.h"
#include "DNA_modifier_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "MOD_lineart.hh"

namespace blender::ed::greasepencil {

void get_lineart_modifier_limits(const Object &ob,
                                 blender::ed::greasepencil::LineartLimitInfo &info)
{
  bool is_first = true;
  LISTBASE_FOREACH (const ModifierData *, md, &ob.modifiers) {
    if (md->type == eModifierType_GreasePencilLineart) {
      const auto *lmd = reinterpret_cast<const GreasePencilLineartModifierData *>(md);
      if (is_first || (lmd->flags & MOD_LINEART_USE_CACHE)) {
        info.min_level = std::min<int>(info.min_level, lmd->level_start);
        info.max_level = std::max<int>(
            info.max_level, lmd->use_multiple_levels ? lmd->level_end : lmd->level_start);
        info.edge_types |= lmd->edge_types;
        info.shadow_selection = std::max(info.shadow_selection, lmd->shadow_selection);
        info.silhouette_selection = std::max(info.silhouette_selection, lmd->silhouette_selection);
        is_first = false;
      }
    }
  }
}

void set_lineart_modifier_limits(GreasePencilLineartModifierData &lmd,
                                 const blender::ed::greasepencil::LineartLimitInfo &info,
                                 const bool cache_is_ready)
{
  BLI_assert(lmd.modifier.type == eModifierType_GreasePencilLineart);
  if ((!cache_is_ready) || (lmd.flags & MOD_LINEART_USE_CACHE)) {
    lmd.level_start_override = info.min_level;
    lmd.level_end_override = info.max_level;
    lmd.edge_types_override = info.edge_types;
    lmd.shadow_selection_override = info.shadow_selection;
    lmd.shadow_use_silhouette_override = info.silhouette_selection;
  }
  else {
    lmd.level_start_override = lmd.level_start;
    lmd.level_end_override = lmd.level_end;
    lmd.edge_types_override = lmd.edge_types;
    lmd.shadow_selection_override = lmd.shadow_selection;
    lmd.shadow_use_silhouette_override = lmd.silhouette_selection;
  }
}

GreasePencilLineartModifierData *get_first_lineart_modifier(const Object &ob)
{
  /* This function always gets the first line art modifier regardless of their visibility, because
   * cached line art configuration are always inside the first line art modifier. */
  LISTBASE_FOREACH (ModifierData *, i_md, &ob.modifiers) {
    if (i_md->type == eModifierType_GreasePencilLineart) {
      return reinterpret_cast<GreasePencilLineartModifierData *>(i_md);
    }
  }
  return nullptr;
}

}  // namespace blender::ed::greasepencil

struct LineartBakeJob {
  wmWindowManager *wm;
  void *owner;
  bool *stop, *do_update;
  float *progress;

  /* C or ob must have one != nullptr. */
  bContext *C;
  blender::Vector<Object *> objects;
  Scene *scene;
  Depsgraph *dg;
  int frame;
  int frame_begin;
  int frame_end;
  int frame_orig;
  int frame_increment;
  bool overwrite_frames;
};

static bool clear_strokes(Object *ob, ModifierData *md, int frame)
{
  if (md->type != eModifierType_GreasePencilLineart) {
    return false;
  }
  GreasePencilLineartModifierData *lmd = reinterpret_cast<GreasePencilLineartModifierData *>(md);
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ob->data);

  blender::bke::greasepencil::TreeNode *node = grease_pencil->find_node_by_name(lmd->target_layer);
  if (!node || !node->is_layer()) {
    return false;
  }
  blender::bke::greasepencil::Layer &layer = node->as_layer();

  if (layer.start_frame_at(frame) == frame) {
    blender::bke::greasepencil::Drawing *drawing = grease_pencil->get_drawing_at(layer, frame);
    if (!drawing) {
      return false;
    }
    drawing->strokes_for_write() = {};
  }

  return true;
}

static bool lineart_mod_is_disabled(Scene *scene, GreasePencilLineartModifierData *md)
{
  BLI_assert(md->modifier.type == eModifierType_GreasePencilLineart);

  /* Toggle on and off the baked flag as we are only interested in if something else is disabling
   * it. We can assume that the guard function has already toggled this on for all modifiers that
   * are sent here. */
  md->flags &= (~MOD_LINEART_IS_BAKED);

  bool enabled = BKE_modifier_is_enabled(
      scene, &md->modifier, eModifierMode_Render | eModifierMode_Realtime);

  md->flags |= MOD_LINEART_IS_BAKED;

  return !enabled;
}

static bool bake_strokes(Object *ob,
                         Depsgraph *dg,
                         LineartCache **lc,
                         GreasePencilLineartModifierData *lmd,
                         int frame,
                         bool is_first)
{
  /* Modifier data sanity check. */
  if (lineart_mod_is_disabled(DEG_get_evaluated_scene(dg), lmd)) {
    return false;
  }

  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ob->data);

  blender::bke::greasepencil::TreeNode *node = grease_pencil->find_node_by_name(lmd->target_layer);
  if (!node || !node->is_layer()) {
    return false;
  }
  blender::bke::greasepencil::Layer &layer = node->as_layer();

  blender::bke::greasepencil::Drawing *drawing = nullptr;
  if (layer.start_frame_at(frame) == frame) {
    drawing = grease_pencil->get_drawing_at(layer, frame);
  }
  else {
    drawing = grease_pencil->insert_frame(layer, frame);
  }
  if (UNLIKELY(!drawing)) {
    return false;
  }

  LineartCache *local_lc = *lc;
  if (!(*lc)) {
    MOD_lineart_compute_feature_lines_v3(dg, *lmd, lc, !(ob->dtx & OB_DRAW_IN_FRONT));
    MOD_lineart_destroy_render_data_v3(lmd);
  }
  else {
    if (is_first || !(lmd->flags & MOD_LINEART_USE_CACHE)) {
      MOD_lineart_compute_feature_lines_v3(dg, *lmd, &local_lc, !(ob->dtx & OB_DRAW_IN_FRONT));
      MOD_lineart_destroy_render_data_v3(lmd);
    }
    MOD_lineart_chain_clear_picked_flag(local_lc);
    lmd->cache = local_lc;
  }

  MOD_lineart_gpencil_generate_v3(
      lmd->cache,
      ob->world_to_object(),
      dg,
      *drawing,
      lmd->source_type,
      lmd->source_object,
      lmd->source_collection,
      lmd->level_start,
      lmd->use_multiple_levels ? lmd->level_end : lmd->level_start,
      lmd->target_material ? BKE_object_material_index_get(ob, lmd->target_material) : 0,
      lmd->edge_types,
      lmd->mask_switches,
      lmd->material_mask_bits,
      lmd->intersection_mask,
      float(lmd->thickness) / 1000.0f,
      lmd->opacity,
      lmd->shadow_selection,
      lmd->silhouette_selection,
      lmd->source_vertex_group,
      lmd->vgname,
      lmd->flags,
      lmd->calculation_flags);

  if (!(lmd->flags & MOD_LINEART_USE_CACHE)) {
    /* Clear local cache. */
    if (!is_first) {
      MOD_lineart_clear_cache(&local_lc);
    }
  }

  return true;
}

static bool bake_single_target(LineartBakeJob *bj, Object *ob, int frame)
{
  bool touched = false;
  if (G.is_break || ob->type != OB_GREASE_PENCIL) {
    return false;
  }

  if (bj->overwrite_frames) {
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type == eModifierType_GreasePencilLineart) {
        if (clear_strokes(ob, md, frame)) {
          touched = true;
        }
      }
    }
  }

  blender::ed::greasepencil::LineartLimitInfo info;
  blender::ed::greasepencil::get_lineart_modifier_limits(*ob, info);

  LineartCache *lc = nullptr;
  bool is_first = true;
  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type != eModifierType_GreasePencilLineart) {
      continue;
    }
    GreasePencilLineartModifierData *lmd = reinterpret_cast<GreasePencilLineartModifierData *>(md);
    blender::ed::greasepencil::set_lineart_modifier_limits(*lmd, info, is_first);

    if (bake_strokes(ob, bj->dg, &lc, lmd, frame, is_first)) {
      touched = true;
      is_first = false;
    }
  }
  MOD_lineart_clear_cache(&lc);

  return touched;
}

static void guard_modifiers(LineartBakeJob &bj)
{
  for (const int object : bj.objects.index_range()) {
    Object *ob = bj.objects[object];

    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type == eModifierType_GreasePencilLineart) {
        GreasePencilLineartModifierData *lmd = reinterpret_cast<GreasePencilLineartModifierData *>(
            md);
        lmd->flags |= MOD_LINEART_IS_BAKED;
      }
    }
  }
}

static void lineart_bake_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  LineartBakeJob *bj = static_cast<LineartBakeJob *>(customdata);
  bj->stop = &worker_status->stop;
  bj->do_update = &worker_status->do_update;
  bj->progress = &worker_status->progress;

  guard_modifiers(*bj);

  for (int frame = bj->frame_begin; frame <= bj->frame_end; frame += bj->frame_increment) {

    if (G.is_break) {
      G.is_break = false;
      break;
    }

    BKE_scene_frame_set(bj->scene, frame);
    BKE_scene_graph_update_for_newframe(bj->dg);
    DEG_graph_build_from_view_layer(bj->dg);

    for (const int object : bj->objects.index_range()) {
      Object *ob = bj->objects[object];
      if (bake_single_target(bj, ob, frame)) {
        DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_GEOMETRY);
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

static void lineart_bake_endjob(void *customdata)
{
  LineartBakeJob *bj = static_cast<LineartBakeJob *>(customdata);

  WM_set_locked_interface(CTX_wm_manager(bj->C), false);

  WM_main_add_notifier(NC_SCENE | ND_FRAME, bj->scene);

  for (const int object : bj->objects.index_range()) {
    Object *ob = bj->objects[object];
    WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, ob);
  }
}

static void lineart_bake_job_free(void *customdata)
{
  LineartBakeJob *bj = static_cast<LineartBakeJob *>(customdata);
  MEM_delete(bj);
}

static int lineart_bake_common(bContext *C,
                               wmOperator *op,
                               bool bake_all_targets,
                               bool do_background)
{
  LineartBakeJob *bj = MEM_new<LineartBakeJob>(__func__);

  if (!bake_all_targets) {
    Object *ob = CTX_data_active_object(C);
    if (!ob || ob->type != OB_GREASE_PENCIL) {
      WM_report(RPT_ERROR, "No active object, or active object isn't a Grease Pencil object");
      return OPERATOR_CANCELLED;
    }
    bj->objects.append(ob);
  }
  else {
    /* #CTX_DATA_BEGIN is not available for iterating in objects while using the job system. */
    CTX_DATA_BEGIN (C, Object *, ob, visible_objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_GreasePencilLineart) {
          bj->objects.append(ob);
          break;
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

    WM_jobs_customdata_set(wm_job, bj, lineart_bake_job_free);
    WM_jobs_timer(wm_job, 0.1, NC_GPENCIL | ND_DATA | NA_EDITED, NC_GPENCIL | ND_DATA | NA_EDITED);
    WM_jobs_callbacks(wm_job, lineart_bake_startjob, nullptr, nullptr, lineart_bake_endjob);

    WM_set_locked_interface(CTX_wm_manager(C), true);

    WM_jobs_start(CTX_wm_manager(C), wm_job);

    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  wmJobWorkerStatus worker_status = {};
  lineart_bake_startjob(bj, &worker_status);

  MEM_delete(bj);

  return OPERATOR_FINISHED;
}

static int lineart_bake_strokes_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  bool bake_all = RNA_boolean_get(op->ptr, "bake_all");
  return lineart_bake_common(C, op, bake_all, true);
}
static int lineart_bake_strokes_exec(bContext *C, wmOperator *op)
{
  bool bake_all = RNA_boolean_get(op->ptr, "bake_all");
  return lineart_bake_common(C, op, bake_all, false);
}
static int lineart_bake_strokes_common_modal(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
{
  Scene *scene = static_cast<Scene *>(op->customdata);

  /* no running blender, remove handler and pass through. */
  if (WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_LINEART) == 0) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  return OPERATOR_PASS_THROUGH;
}

static void lineart_gpencil_clear_strokes_exec_common(Object *ob)
{
  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type != eModifierType_GreasePencilLineart) {
      continue;
    }
    GreasePencilLineartModifierData *lmd = reinterpret_cast<GreasePencilLineartModifierData *>(md);
    GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ob->data);

    blender::bke::greasepencil::TreeNode *node = grease_pencil->find_node_by_name(
        lmd->target_layer);
    if (!node || !node->is_layer()) {
      return;
    }
    blender::bke::greasepencil::Layer &layer = node->as_layer();

    /* Remove all the keyframes in this layer. */
    grease_pencil->remove_frames(layer, layer.sorted_keys());
    grease_pencil->insert_frame(layer, 0);

    md->mode |= eModifierMode_Realtime | eModifierMode_Render;

    lmd->flags &= (~MOD_LINEART_IS_BAKED);
  }
  DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_GEOMETRY);
}

static int lineart_gpencil_clear_strokes_exec(bContext *C, wmOperator *op)
{
  bool clear_all = RNA_boolean_get(op->ptr, "clear_all");

  if (clear_all) {
    CTX_DATA_BEGIN (C, Object *, ob, visible_objects) {
      if (ob->type != OB_GREASE_PENCIL) {
        continue;
      }
      lineart_gpencil_clear_strokes_exec_common(ob);
      WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, ob);
    }
    CTX_DATA_END;
    BKE_report(op->reports, RPT_INFO, "All Line Art objects are now cleared of bakes");
  }
  else {
    Object *ob = CTX_data_active_object(C);
    if (ob->type != OB_GREASE_PENCIL) {
      return OPERATOR_CANCELLED;
    }
    lineart_gpencil_clear_strokes_exec_common(ob);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, ob);
    BKE_report(op->reports, RPT_INFO, "Baked strokes are cleared");
  }

  return OPERATOR_FINISHED;
}

static void OBJECT_OT_lineart_bake_strokes(wmOperatorType *ot)
{
  ot->name = "Bake Line Art";
  ot->description = "Bake Line Art for current Grease Pencil object";
  ot->idname = "OBJECT_OT_lineart_bake_strokes";

  ot->poll = blender::ed::greasepencil::active_grease_pencil_poll;
  ot->invoke = lineart_bake_strokes_invoke;
  ot->exec = lineart_bake_strokes_exec;
  ot->modal = lineart_bake_strokes_common_modal;

  RNA_def_boolean(ot->srna, "bake_all", false, "Bake All", "Bake all Line Art modifiers");
}

static void OBJECT_OT_lineart_clear(wmOperatorType *ot)
{
  ot->name = "Clear Baked Line Art";
  ot->description = "Clear all strokes in current Grease Pencil object";
  ot->idname = "OBJECT_OT_lineart_clear";

  ot->poll = blender::ed::greasepencil::active_grease_pencil_poll;
  ot->exec = lineart_gpencil_clear_strokes_exec;

  RNA_def_boolean(ot->srna, "clear_all", false, "Clear All", "Clear all Line Art modifier bakes");
}

void ED_operatortypes_grease_pencil_lineart()
{
  WM_operatortype_append(OBJECT_OT_lineart_bake_strokes);
  WM_operatortype_append(OBJECT_OT_lineart_clear);
}
