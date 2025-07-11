/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "BLI_bounds.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"

#include "DEG_depsgraph_query.hh"

#include "ED_mesh.hh"
#include "ED_particle.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "view3d_intern.hh"
#include "view3d_navigate.hh" /* own include */

using blender::float3;

/* -------------------------------------------------------------------- */
/** \name View All Operator
 *
 * Move & Zoom the view to fit all of its contents.
 * \{ */

static bool view3d_object_skip_minmax(const View3D *v3d,
                                      const RegionView3D *rv3d,
                                      const Object *ob,
                                      const bool skip_camera,
                                      bool *r_only_center)
{
  BLI_assert(ob->id.orig_id == nullptr);
  *r_only_center = false;

  if (skip_camera && (ob == v3d->camera)) {
    return true;
  }

  if ((ob->type == OB_EMPTY) && (ob->empty_drawtype == OB_EMPTY_IMAGE) &&
      !BKE_object_empty_image_frame_is_visible_in_view3d(ob, rv3d))
  {
    *r_only_center = true;
    return false;
  }

  return false;
}

static void view3d_object_calc_minmax(Depsgraph *depsgraph,
                                      Scene *scene,
                                      Object *ob_eval,
                                      const bool only_center,
                                      float3 &min,
                                      float3 &max)
{
  /* Account for duplis. */
  if (BKE_object_minmax_dupli(depsgraph, scene, ob_eval, min, max, false) == 0) {
    /* Use if duplis aren't found. */
    if (only_center) {
      minmax_v3v3_v3(min, max, ob_eval->object_to_world().location());
    }
    else {
      BKE_object_minmax(ob_eval, min, max);
    }
  }
}

static void view3d_from_minmax(bContext *C,
                               View3D *v3d,
                               ARegion *region,
                               const float min[3],
                               const float max[3],
                               bool do_zoom,
                               const int smooth_viewtx)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  float afm[3];
  float size;

  ED_view3d_smooth_view_force_finish(C, v3d, region);

  /* SMOOTHVIEW */
  float ofs_new[3];
  float dist_new;

  sub_v3_v3v3(afm, max, min);
  size = max_fff(afm[0], afm[1], afm[2]);

  if (do_zoom) {
    char persp;

    if (rv3d->is_persp) {
      if (rv3d->persp == RV3D_CAMOB && ED_view3d_camera_lock_check(v3d, rv3d)) {
        persp = RV3D_CAMOB;
      }
      else {
        persp = RV3D_PERSP;
      }
    }
    else { /* ortho */
      if (size < 0.0001f) {
        /* bounding box was a single point so do not zoom */
        do_zoom = false;
      }
      else {
        /* adjust zoom so it looks nicer */
        persp = RV3D_ORTHO;
      }
    }

    if (do_zoom) {
      Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      dist_new = ED_view3d_radius_to_dist(
          v3d, region, depsgraph, persp, true, (size / 2) * VIEW3D_MARGIN);
      if (rv3d->is_persp) {
        /* Don't zoom closer than the near clipping plane. */
        const float dist_min = ED_view3d_dist_soft_min_get(v3d, true);
        CLAMP_MIN(dist_new, dist_min);
      }
    }
  }

  mid_v3_v3v3(ofs_new, min, max);
  negate_v3(ofs_new);

  V3D_SmoothParams sview = {nullptr};
  sview.ofs = ofs_new;
  sview.dist = do_zoom ? &dist_new : nullptr;
  /* The caller needs to use undo begin/end calls. */
  sview.undo_str = nullptr;

  if (rv3d->persp == RV3D_CAMOB && !ED_view3d_camera_lock_check(v3d, rv3d)) {
    rv3d->persp = RV3D_PERSP;
    sview.camera_old = v3d->camera;
  }

  ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);

  /* Smooth-view does view-lock #RV3D_BOXVIEW copy. */
}

/**
 * Same as #view3d_from_minmax but for all regions (except cameras).
 */
static void view3d_from_minmax_multi(bContext *C,
                                     View3D *v3d,
                                     const float min[3],
                                     const float max[3],
                                     const bool do_zoom,
                                     const int smooth_viewtx)
{
  ScrArea *area = CTX_wm_area(C);
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
      /* when using all regions, don't jump out of camera view,
       * but _do_ allow locked cameras to be moved */
      if ((rv3d->persp != RV3D_CAMOB) || ED_view3d_camera_lock_check(v3d, rv3d)) {
        view3d_from_minmax(C, v3d, region, min, max, do_zoom, smooth_viewtx);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name High Level Viewport Bounds Calculation
 *
 * Move & Zoom the view to fit all of its contents.
 * \{ */

std::optional<blender::Bounds<float3>> view3d_calc_minmax_visible(Depsgraph *depsgraph,
                                                                  ScrArea *area,
                                                                  ARegion *region,
                                                                  const bool use_all_regions,
                                                                  const bool clip_bounds)
{
  /* NOTE: we could support calculating this without requiring a #View3D or #RegionView3D
   * Currently this isn't needed. */

  const View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  const RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  Scene *scene = DEG_get_input_scene(depsgraph);
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);

  float3 min, max;
  INIT_MINMAX(min, max);

  bool changed = false;

  const bool skip_camera = (ED_view3d_camera_lock_check(v3d, rv3d) ||
                            /* any one of the regions may be locked */
                            (use_all_regions && v3d->flag2 & V3D_LOCK_CAMERA));

  BKE_view_layer_synced_ensure(scene_eval, view_layer_eval);
  LISTBASE_FOREACH (Base *, base_eval, BKE_view_layer_object_bases_get(view_layer_eval)) {
    if (BASE_VISIBLE(v3d, base_eval)) {
      bool only_center = false;
      Object *ob = DEG_get_original(base_eval->object);
      if (view3d_object_skip_minmax(v3d, rv3d, ob, skip_camera, &only_center)) {
        continue;
      }
      view3d_object_calc_minmax(depsgraph, scene, base_eval->object, only_center, min, max);
      changed = true;
    }
  }

  if (changed) {
    if (clip_bounds && RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
      /* This is an approximation, see function documentation for details. */
      ED_view3d_clipping_clamp_minmax(rv3d, min, max);
    }
  }

  if (!changed) {
    return std::nullopt;
  }
  return blender::Bounds<float3>(min, max);
}

std::optional<blender::Bounds<float3>> view3d_calc_minmax_selected(Depsgraph *depsgraph,
                                                                   ScrArea *area,
                                                                   ARegion *region,
                                                                   const bool use_all_regions,
                                                                   const bool clip_bounds,
                                                                   bool *r_do_zoom)
{
  /* NOTE: we could support calculating this without requiring a #View3D or #RegionView3D
   * Currently this isn't needed. */

  const View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  const RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  Scene *scene = DEG_get_input_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);

  const Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);

  BKE_view_layer_synced_ensure(scene_eval, view_layer_eval);
  Object *ob_eval = BKE_view_layer_active_object_get(view_layer_eval);
  Object *obedit = OBEDIT_FROM_OBACT(ob_eval);
  const bool is_face_map = (region->runtime->gizmo_map &&
                            WM_gizmomap_is_any_selected(region->runtime->gizmo_map));
  const bool skip_camera = (ED_view3d_camera_lock_check(v3d, rv3d) ||
                            /* any one of the regions may be locked */
                            (use_all_regions && v3d->flag2 & V3D_LOCK_CAMERA));

  float3 min, max;
  INIT_MINMAX(min, max);
  bool changed = false;

  *r_do_zoom = true;

  if (is_face_map) {
    ob_eval = nullptr;
  }

  if (ob_eval && (ob_eval->mode & OB_MODE_WEIGHT_PAINT)) {
    /* hard-coded exception, we look for the one selected armature */
    /* this is weak code this way, we should make a generic
     * active/selection callback interface once... */
    Base *base_eval;
    for (base_eval = (Base *)BKE_view_layer_object_bases_get(view_layer_eval)->first; base_eval;
         base_eval = base_eval->next)
    {
      if (BASE_SELECTED_EDITABLE(v3d, base_eval)) {
        if (base_eval->object->type == OB_ARMATURE) {
          if (base_eval->object->mode & OB_MODE_POSE) {
            break;
          }
        }
      }
    }
    if (base_eval) {
      ob_eval = base_eval->object;
    }
  }

  if (is_face_map) {
    changed = WM_gizmomap_minmax(region->runtime->gizmo_map, true, true, min, max);
  }
  else if (obedit) {
    /* only selected */
    FOREACH_OBJECT_IN_MODE_BEGIN (
        scene_eval, view_layer_eval, v3d, obedit->type, obedit->mode, ob_eval_iter)
    {
      changed |= ED_view3d_minmax_verts(scene_eval, ob_eval_iter, min, max);
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  else if (ob_eval && (ob_eval->mode & OB_MODE_POSE)) {
    FOREACH_OBJECT_IN_MODE_BEGIN (
        scene_eval, view_layer_eval, v3d, ob_eval->type, ob_eval->mode, ob_eval_iter)
    {
      const std::optional<blender::Bounds<float3>> bounds = BKE_pose_minmax(ob_eval_iter, true);
      if (bounds) {
        const blender::Bounds<float3> world_bounds = blender::bounds::transform_bounds<float, 4>(
            ob_eval->object_to_world(), *bounds);
        minmax_v3v3_v3(min, max, world_bounds.min);
        minmax_v3v3_v3(min, max, world_bounds.max);
        changed = true;
      }
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  else if (BKE_paint_select_face_test(ob_eval)) {
    changed = paintface_minmax(ob_eval, min, max);
  }
  else if (ob_eval && (ob_eval->mode & OB_MODE_PARTICLE_EDIT)) {
    changed = PE_minmax(depsgraph, scene, view_layer, min, max);
  }
  else if (ob_eval && (ob_eval->mode & OB_MODE_SCULPT_CURVES)) {
    FOREACH_OBJECT_IN_MODE_BEGIN (
        scene_eval, view_layer_eval, v3d, ob_eval->type, ob_eval->mode, ob_eval_iter)
    {
      changed |= ED_view3d_minmax_verts(scene_eval, ob_eval_iter, min, max);
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  else if (ob_eval && (ob_eval->mode & (OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT |
                                        OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)))
  {
    PaintMode mode = PaintMode::Invalid;
    if (ob_eval->mode & OB_MODE_SCULPT) {
      mode = PaintMode::Sculpt;
    }
    else if (ob_eval->mode & OB_MODE_VERTEX_PAINT) {
      mode = PaintMode::Vertex;
    }
    else if (ob_eval->mode & OB_MODE_WEIGHT_PAINT) {
      mode = PaintMode::Weight;
    }
    else if (ob_eval->mode & OB_MODE_TEXTURE_PAINT) {
      mode = PaintMode::Texture3D;
    }
    Paint *paint = BKE_paint_get_active_from_paintmode(scene, mode);
    BKE_paint_stroke_get_average(paint, ob_eval, min);
    copy_v3_v3(max, min);
    changed = true;
    *r_do_zoom = false;
  }
  else {
    LISTBASE_FOREACH (Base *, base_eval, BKE_view_layer_object_bases_get(view_layer_eval)) {
      if (BASE_SELECTED(v3d, base_eval)) {
        bool only_center = false;
        Object *ob = DEG_get_original(base_eval->object);
        if (view3d_object_skip_minmax(v3d, rv3d, ob, skip_camera, &only_center)) {
          continue;
        }
        view3d_object_calc_minmax(depsgraph, scene, base_eval->object, only_center, min, max);
        changed = true;
      }
    }
  }

  if (changed) {
    if (clip_bounds && RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
      /* This is an approximation, see function documentation for details. */
      ED_view3d_clipping_clamp_minmax(rv3d, min, max);
    }
  }

  if (!changed) {
    return std::nullopt;
  }
  return blender::Bounds<float3>(min, max);
}

bool view3d_calc_point_in_selected_bounds(Depsgraph *depsgraph,
                                          ViewLayer *view_layer,
                                          const View3D *v3d,
                                          const blender::float3 &point,
                                          const float scale_margin)
{
  Scene *scene = DEG_get_input_scene(depsgraph);

  LISTBASE_FOREACH (const Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (!BASE_SELECTED(v3d, base)) {
      continue;
    }
    Object *ob = base->object;
    BLI_assert(!DEG_is_original(ob));

    float3 min, max;
    view3d_object_calc_minmax(depsgraph, scene, ob, false, min, max);

    blender::Bounds<float3> bounds{min, max};

    bounds.scale_from_center(float3(scale_margin));

    float3 local_min = blender::math::transform_point(ob->object_to_world(), bounds.min);
    float3 local_max = blender::math::transform_point(ob->object_to_world(), bounds.max);

    if (point[0] >= local_min[0] && point[1] >= local_min[1] && point[2] >= local_min[2] &&
        point[0] <= local_max[0] && point[1] <= local_max[1] && point[2] <= local_max[2])
    {
      return true;
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View All Operator
 *
 * Move & Zoom the view to fit all of its contents.
 * \{ */

static wmOperatorStatus view3d_all_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);

  const bool use_all_regions = RNA_boolean_get(op->ptr, "use_all_regions");
  const bool center = RNA_boolean_get(op->ptr, "center");
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  std::optional<blender::Bounds<float3>> bounds = view3d_calc_minmax_visible(
      depsgraph, area, region, use_all_regions, true);
  if (center) {
    /* in 2.4x this also move the cursor to (0, 0, 0) (with shift+c). */
    View3DCursor *cursor = &scene->cursor;

    cursor->set_matrix(blender::float4x4::identity(), false);

    wmMsgBus *mbus = CTX_wm_message_bus(C);
    WM_msg_publish_rna_prop(mbus, &scene->id, &scene->cursor, View3DCursor, location);

    DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  }

  if (!bounds.has_value()) {
    ED_region_tag_redraw(region);
    /* TODO: should this be cancel?
     * I think no, because we always move the cursor, with or without
     * object, but in this case there is no change in the scene,
     * only the cursor so I choice a ED_region_tag like
     * view3d_smooth_view do for the center_cursor.
     * See bug #22640.
     */
    return OPERATOR_FINISHED;
  }

  float3 &min = bounds.value().min;
  float3 &max = bounds.value().max;

  if (center) {
    minmax_v3v3_v3(min, max, float3(0.0f));
  }

  ED_view3d_smooth_view_undo_begin(C, area);
  if (use_all_regions) {
    view3d_from_minmax_multi(C, v3d, min, max, true, smooth_viewtx);
  }
  else {
    view3d_from_minmax(C, v3d, region, min, max, true, smooth_viewtx);
  }

  ED_view3d_smooth_view_undo_end(C, area, op->type->name, false);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame All";
  ot->description = "View all objects in scene";
  ot->idname = "VIEW3D_OT_view_all";

  /* API callbacks. */
  ot->exec = view3d_all_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;

  /* properties */
  view3d_operator_properties_common(ot, V3D_OP_PROP_USE_ALL_REGIONS);
  RNA_def_boolean(ot->srna, "center", false, "Center", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Selected Operator
 *
 * Move & Zoom the view to fit selected contents.
 * \{ */

static wmOperatorStatus viewselected_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  bool do_zoom = true;
  const bool use_all_regions = RNA_boolean_get(op->ptr, "use_all_regions");
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const std::optional<blender::Bounds<float3>> bounds = view3d_calc_minmax_selected(
      depsgraph, area, region, use_all_regions, true, &do_zoom);

  if (!bounds.has_value()) {
    return OPERATOR_FINISHED;
  }

  const float3 &min = bounds.value().min;
  const float3 &max = bounds.value().max;

  ED_view3d_smooth_view_undo_begin(C, area);
  if (use_all_regions) {
    view3d_from_minmax_multi(C, v3d, min, max, do_zoom, smooth_viewtx);
  }
  else {
    view3d_from_minmax(C, v3d, region, min, max, do_zoom, smooth_viewtx);
  }

  ED_view3d_smooth_view_undo_end(C, area, op->type->name, false);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame Selected";
  ot->description = "Move the view to the selection center";
  ot->idname = "VIEW3D_OT_view_selected";

  /* API callbacks. */
  ot->exec = viewselected_exec;
  ot->poll = view3d_zoom_or_dolly_poll;

  /* flags */
  ot->flag = 0;

  /* properties */
  view3d_operator_properties_common(ot, V3D_OP_PROP_USE_ALL_REGIONS);
}

/** \} */
