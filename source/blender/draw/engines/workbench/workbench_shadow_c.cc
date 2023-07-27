/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Shadow:
 *
 * Use stencil shadow buffer to cast a sharp shadow over opaque surfaces.
 *
 * After the main pre-pass we render shadow volumes using custom depth & stencil states to
 * set the stencil of shadowed area to anything but 0.
 *
 * Then the shading pass will shade the areas with stencil not equal 0 differently.
 */

#include "DRW_render.h"

#include "BKE_object.h"

#include "BLI_math.h"

#include "workbench_engine.h"
#include "workbench_private.h"

static void compute_parallel_lines_nor_and_dist(const float v1[2],
                                                const float v2[2],
                                                const float v3[2],
                                                float r_line[4])
{
  sub_v2_v2v2(r_line, v2, v1);
  /* Find orthogonal vector. */
  SWAP(float, r_line[0], r_line[1]);
  r_line[0] = -r_line[0];
  /* Edge distances. */
  r_line[2] = dot_v2v2(r_line, v1);
  r_line[3] = dot_v2v2(r_line, v3);
  /* Make sure r_line[2] is the minimum. */
  if (r_line[2] > r_line[3]) {
    SWAP(float, r_line[2], r_line[3]);
  }
}

static void workbench_shadow_update(WORKBENCH_PrivateData *wpd)
{
  wpd->shadow_changed = !compare_v3v3(
      wpd->shadow_cached_direction, wpd->shadow_direction_ws, 1e-5f);

  if (wpd->shadow_changed) {
    const float up[3] = {0.0f, 0.0f, 1.0f};
    unit_m4(wpd->shadow_mat);

    /* TODO: fix singularity. */
    copy_v3_v3(wpd->shadow_mat[2], wpd->shadow_direction_ws);
    cross_v3_v3v3(wpd->shadow_mat[0], wpd->shadow_mat[2], up);
    normalize_v3(wpd->shadow_mat[0]);
    cross_v3_v3v3(wpd->shadow_mat[1], wpd->shadow_mat[2], wpd->shadow_mat[0]);

    invert_m4_m4(wpd->shadow_inv, wpd->shadow_mat);

    copy_v3_v3(wpd->shadow_cached_direction, wpd->shadow_direction_ws);
  }

  float planes[6][4];
  DRW_culling_frustum_planes_get(nullptr, planes);
  /* we only need the far plane. */
  copy_v4_v4(wpd->shadow_far_plane, planes[2]);

  BoundBox frustum_corners;
  DRW_culling_frustum_corners_get(nullptr, &frustum_corners);

  float shadow_near_corners[4][3];
  mul_v3_mat3_m4v3(shadow_near_corners[0], wpd->shadow_inv, frustum_corners.vec[0]);
  mul_v3_mat3_m4v3(shadow_near_corners[1], wpd->shadow_inv, frustum_corners.vec[3]);
  mul_v3_mat3_m4v3(shadow_near_corners[2], wpd->shadow_inv, frustum_corners.vec[7]);
  mul_v3_mat3_m4v3(shadow_near_corners[3], wpd->shadow_inv, frustum_corners.vec[4]);

  INIT_MINMAX(wpd->shadow_near_min, wpd->shadow_near_max);
  for (int i = 0; i < 4; i++) {
    minmax_v3v3_v3(wpd->shadow_near_min, wpd->shadow_near_max, shadow_near_corners[i]);
  }

  compute_parallel_lines_nor_and_dist(shadow_near_corners[0],
                                      shadow_near_corners[1],
                                      shadow_near_corners[2],
                                      wpd->shadow_near_sides[0]);
  compute_parallel_lines_nor_and_dist(shadow_near_corners[1],
                                      shadow_near_corners[2],
                                      shadow_near_corners[0],
                                      wpd->shadow_near_sides[1]);
}

void workbench_shadow_data_update(WORKBENCH_PrivateData *wpd, WORKBENCH_UBO_World *wd)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;

  float view_matrix[4][4];
  DRW_view_viewmat_get(nullptr, view_matrix, false);

  /* Turn the light in a way where it's more user friendly to control. */
  copy_v3_v3(wpd->shadow_direction_ws, scene->display.light_direction);
  SWAP(float, wpd->shadow_direction_ws[2], wpd->shadow_direction_ws[1]);
  wpd->shadow_direction_ws[2] = -wpd->shadow_direction_ws[2];
  wpd->shadow_direction_ws[0] = -wpd->shadow_direction_ws[0];

  /* Shadow direction. */
  mul_v3_mat3_m4v3(wd->shadow_direction_vs, view_matrix, wpd->shadow_direction_ws);

  /* Clamp to avoid overshadowing and shading errors. */
  float focus = clamp_f(scene->display.shadow_focus, 0.0001f, 0.99999f);
  wd->shadow_shift = scene->display.shadow_shift;
  wd->shadow_focus = 1.0f - focus * (1.0f - wd->shadow_shift);

  if (SHADOW_ENABLED(wpd)) {
    wd->shadow_mul = wpd->shading.shadow_intensity;
    wd->shadow_add = 1.0f - wd->shadow_mul;
  }
  else {
    wd->shadow_mul = 0.0f;
    wd->shadow_add = 1.0f;
  }
}

void workbench_shadow_cache_init(WORKBENCH_Data *data)
{
  WORKBENCH_PassList *psl = data->psl;
  WORKBENCH_PrivateData *wpd = data->stl->wpd;
  GPUShader *sh;
  DRWShadingGroup *grp;

  if (SHADOW_ENABLED(wpd)) {
    workbench_shadow_update(wpd);

#if DEBUG_SHADOW_VOLUME
    DRWState depth_pass_state = DRW_STATE_DEPTH_LESS;
    DRWState depth_fail_state = DRW_STATE_DEPTH_GREATER_EQUAL;
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL;
#else
    DRWState depth_pass_state = DRW_STATE_WRITE_STENCIL_SHADOW_PASS;
    DRWState depth_fail_state = DRW_STATE_WRITE_STENCIL_SHADOW_FAIL;
    DRWState state = DRW_STATE_DEPTH_LESS | DRW_STATE_STENCIL_ALWAYS;
#endif

    /* TODO(fclem): Merge into one pass with sub-passes. */
    DRW_PASS_CREATE(psl->shadow_ps[0], state | depth_pass_state);
    DRW_PASS_CREATE(psl->shadow_ps[1], state | depth_fail_state);

    /* Stencil Shadow passes. */
    for (int manifold = 0; manifold < 2; manifold++) {
      sh = workbench_shader_shadow_pass_get(manifold);
      wpd->shadow_pass_grp[manifold] = grp = DRW_shgroup_create(sh, psl->shadow_ps[0]);
      DRW_shgroup_stencil_mask(grp, 0xFF); /* Needed once to set the stencil state for the pass. */

      sh = workbench_shader_shadow_fail_get(manifold, false);
      wpd->shadow_fail_grp[manifold] = grp = DRW_shgroup_create(sh, psl->shadow_ps[1]);
      DRW_shgroup_stencil_mask(grp, 0xFF); /* Needed once to set the stencil state for the pass. */

      sh = workbench_shader_shadow_fail_get(manifold, true);
      wpd->shadow_fail_caps_grp[manifold] = grp = DRW_shgroup_create(sh, psl->shadow_ps[1]);
    }
  }
  else {
    psl->shadow_ps[0] = nullptr;
    psl->shadow_ps[1] = nullptr;
  }
}

static const BoundBox *workbench_shadow_object_shadow_bbox_get(WORKBENCH_PrivateData *wpd,
                                                               Object *ob,
                                                               WORKBENCH_ObjectData *oed)
{
  if (oed->shadow_bbox_dirty || wpd->shadow_changed) {
    float tmp_mat[4][4];
    mul_m4_m4m4(tmp_mat, wpd->shadow_inv, ob->object_to_world);

    /* Get AABB in shadow space. */
    INIT_MINMAX(oed->shadow_min, oed->shadow_max);

    /* From object space to shadow space */
    const BoundBox *bbox = BKE_object_boundbox_get(ob);
    for (int i = 0; i < 8; i++) {
      float corner[3];
      mul_v3_m4v3(corner, tmp_mat, bbox->vec[i]);
      minmax_v3v3_v3(oed->shadow_min, oed->shadow_max, corner);
    }
    oed->shadow_depth = oed->shadow_max[2] - oed->shadow_min[2];
    /* Extend towards infinity. */
    oed->shadow_max[2] += 1e4f;

    /* Get extended AABB in world space. */
    BKE_boundbox_init_from_minmax(&oed->shadow_bbox, oed->shadow_min, oed->shadow_max);
    for (int i = 0; i < 8; i++) {
      mul_m4_v3(wpd->shadow_mat, oed->shadow_bbox.vec[i]);
    }
    oed->shadow_bbox_dirty = false;
  }

  return &oed->shadow_bbox;
}

static bool workbench_shadow_object_cast_visible_shadow(WORKBENCH_PrivateData *wpd,
                                                        Object *ob,
                                                        WORKBENCH_ObjectData *oed)
{
  const BoundBox *shadow_bbox = workbench_shadow_object_shadow_bbox_get(wpd, ob, oed);
  const DRWView *default_view = DRW_view_default_get();
  return DRW_culling_box_test(default_view, shadow_bbox);
}

static float workbench_shadow_object_shadow_distance(WORKBENCH_PrivateData *wpd,
                                                     Object *ob,
                                                     WORKBENCH_ObjectData *oed)
{
  const BoundBox *shadow_bbox = workbench_shadow_object_shadow_bbox_get(wpd, ob, oed);

  const int corners[4] = {0, 3, 4, 7};
  float dist = 1e4f, dist_isect;
  for (int i = 0; i < 4; i++) {
    if (isect_ray_plane_v3(shadow_bbox->vec[corners[i]],
                           wpd->shadow_cached_direction,
                           wpd->shadow_far_plane,
                           &dist_isect,
                           true))
    {
      if (dist_isect < dist) {
        dist = dist_isect;
      }
    }
    else {
      /* All rays are parallels. If one fails, the other will too. */
      break;
    }
  }
  return max_ii(dist - oed->shadow_depth, 0);
}

static bool workbench_shadow_camera_in_object_shadow(WORKBENCH_PrivateData *wpd,
                                                     Object *ob,
                                                     WORKBENCH_ObjectData *oed)
{
  /* Just to be sure the min, max are updated. */
  workbench_shadow_object_shadow_bbox_get(wpd, ob, oed);
  /* Test if near plane is in front of the shadow. */
  if (oed->shadow_min[2] > wpd->shadow_near_max[2]) {
    return false;
  }

  /* Separation Axis Theorem test */

  /* Test bbox sides first (faster) */
  if ((oed->shadow_min[0] > wpd->shadow_near_max[0]) ||
      (oed->shadow_max[0] < wpd->shadow_near_min[0]) ||
      (oed->shadow_min[1] > wpd->shadow_near_max[1]) ||
      (oed->shadow_max[1] < wpd->shadow_near_min[1]))
  {
    return false;
  }
  /* Test projected near rectangle sides */
  const float pts[4][2] = {
      {oed->shadow_min[0], oed->shadow_min[1]},
      {oed->shadow_min[0], oed->shadow_max[1]},
      {oed->shadow_max[0], oed->shadow_min[1]},
      {oed->shadow_max[0], oed->shadow_max[1]},
  };

  for (int i = 0; i < 2; i++) {
    float min_dst = FLT_MAX, max_dst = -FLT_MAX;
    for (int j = 0; j < 4; j++) {
      float dst = dot_v2v2(wpd->shadow_near_sides[i], pts[j]);
      /* Do min max */
      if (min_dst > dst) {
        min_dst = dst;
      }
      if (max_dst < dst) {
        max_dst = dst;
      }
    }

    if ((wpd->shadow_near_sides[i][2] > max_dst) || (wpd->shadow_near_sides[i][3] < min_dst)) {
      return false;
    }
  }
  /* No separation axis found. Both shape intersect. */
  return true;
}

static void workbench_init_object_data(DrawData *dd)
{
  WORKBENCH_ObjectData *data = (WORKBENCH_ObjectData *)dd;
  data->shadow_bbox_dirty = true;
}

void workbench_shadow_cache_populate(WORKBENCH_Data *data, Object *ob, const bool has_transp_mat)
{
  WORKBENCH_PrivateData *wpd = data->stl->wpd;

  bool is_manifold;
  struct GPUBatch *geom_shadow = DRW_cache_object_edge_detection_get(ob, &is_manifold);
  if (geom_shadow == nullptr) {
    return;
  }

  WORKBENCH_ObjectData *engine_object_data = (WORKBENCH_ObjectData *)DRW_drawdata_ensure(
      &ob->id,
      &draw_engine_workbench,
      sizeof(WORKBENCH_ObjectData),
      &workbench_init_object_data,
      nullptr);

  if (workbench_shadow_object_cast_visible_shadow(wpd, ob, engine_object_data)) {
    mul_v3_mat3_m4v3(
        engine_object_data->shadow_dir, ob->world_to_object, wpd->shadow_direction_ws);

    DRWShadingGroup *grp;
    bool use_shadow_pass_technique = !workbench_shadow_camera_in_object_shadow(
        wpd, ob, engine_object_data);

    /* Shadow pass technique needs object to be have all its surface opaque. */
    if (has_transp_mat) {
      use_shadow_pass_technique = false;
    }

    /* We cannot use Shadow Pass technique on non-manifold object (see #76168). */
    if (use_shadow_pass_technique && !is_manifold && (wpd->cull_state != 0)) {
      use_shadow_pass_technique = false;
    }

    if (use_shadow_pass_technique) {
      grp = DRW_shgroup_create_sub(wpd->shadow_pass_grp[is_manifold]);
      DRW_shgroup_uniform_vec3(grp, "lightDirection", engine_object_data->shadow_dir, 1);
      DRW_shgroup_uniform_float_copy(grp, "lightDistance", 1e5f);
      DRW_shgroup_call_no_cull(grp, geom_shadow, ob);
#if DEBUG_SHADOW_VOLUME
      DRW_debug_bbox(&engine_object_data->shadow_bbox, blender::float4{1.0f, 0.0f, 0.0f, 1.0f});
#endif
    }
    else {
      float extrude_distance = workbench_shadow_object_shadow_distance(
          wpd, ob, engine_object_data);

      /* TODO(fclem): only use caps if they are in the view frustum. */
      const bool need_caps = true;
      if (need_caps) {
        grp = DRW_shgroup_create_sub(wpd->shadow_fail_caps_grp[is_manifold]);
        DRW_shgroup_uniform_vec3(grp, "lightDirection", engine_object_data->shadow_dir, 1);
        DRW_shgroup_uniform_float_copy(grp, "lightDistance", extrude_distance);
        DRW_shgroup_call_no_cull(grp, DRW_cache_object_surface_get(ob), ob);
      }

      grp = DRW_shgroup_create_sub(wpd->shadow_fail_grp[is_manifold]);
      DRW_shgroup_uniform_vec3(grp, "lightDirection", engine_object_data->shadow_dir, 1);
      DRW_shgroup_uniform_float_copy(grp, "lightDistance", extrude_distance);
      DRW_shgroup_call_no_cull(grp, geom_shadow, ob);
#if DEBUG_SHADOW_VOLUME
      DRW_debug_bbox(&engine_object_data->shadow_bbox, blender::float4{0.0f, 1.0f, 0.0f, 1.0f});
#endif
    }
  }
}
