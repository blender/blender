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
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 * Implements the Sculpt Mode tools
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_ghash.h"
#include "BLI_gsqueue.h"
#include "BLI_hash.h"
#include "BLI_link_utils.h"
#include "BLI_linklist.h"
#include "BLI_linklist_stack.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_memarena.h"
#include "BLI_rand.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"
#include "atomic_ops.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.h"
#include "BKE_brush.h"
#include "BKE_brush_engine.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_kelvinlet.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_fair.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_mirror.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "IMB_colormanagement.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_space_api.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------- */
/** \name SculptProjectVector
 *
 * Fast-path for #project_plane_v3_v3v3
 *
 * \{ */

typedef struct SculptProjectVector {
  float plane[3];
  float len_sq;
  float len_sq_inv_neg;
  bool is_valid;

} SculptProjectVector;

static bool plane_point_side_flip(const float co[3], const float plane[4], const bool flip)
{
  float d = plane_point_side_v3(plane, co);
  if (flip) {
    d = -d;
  }
  return d <= 0.0f;
}

/**
 * \param plane: Direction, can be any length.
 */
static void sculpt_project_v3_cache_init(SculptProjectVector *spvc, const float plane[3])
{
  copy_v3_v3(spvc->plane, plane);
  spvc->len_sq = len_squared_v3(spvc->plane);
  spvc->is_valid = (spvc->len_sq > FLT_EPSILON);
  spvc->len_sq_inv_neg = (spvc->is_valid) ? -1.0f / spvc->len_sq : 0.0f;
}

/**
 * Calculate the projection.
 */
static void sculpt_project_v3(const SculptProjectVector *spvc, const float vec[3], float r_vec[3])
{
#if 0
  project_plane_v3_v3v3(r_vec, vec, spvc->plane);
#else
  /* inline the projection, cache `-1.0 / dot_v3_v3(v_proj, v_proj)` */
  madd_v3_v3fl(r_vec, spvc->plane, dot_v3v3(vec, spvc->plane) * spvc->len_sq_inv_neg);
#endif
}

/** \} */

static void calc_sculpt_plane(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3], float r_area_co[3])
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (SCULPT_stroke_is_main_symmetry_pass(ss->cache) &&
      (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache) ||
       !(brush->flag & BRUSH_ORIGINAL_PLANE) || !(brush->flag & BRUSH_ORIGINAL_NORMAL))) {
    switch (brush->sculpt_plane) {
      case SCULPT_DISP_DIR_VIEW:
        copy_v3_v3(r_area_no, ss->cache->true_view_normal);
        break;

      case SCULPT_DISP_DIR_X:
        ARRAY_SET_ITEMS(r_area_no, 1.0f, 0.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Y:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 1.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Z:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 0.0f, 1.0f);
        break;

      case SCULPT_DISP_DIR_AREA:
        SCULPT_calc_area_normal_and_center(sd, ob, nodes, totnode, r_area_no, r_area_co);
        if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
          project_plane_v3_v3v3(r_area_no, r_area_no, ss->cache->view_normal);
          normalize_v3(r_area_no);
        }
        break;

      default:
        break;
    }

    /* For flatten center. */
    /* Flatten center has not been calculated yet if we are not using the area normal. */
    if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA) {
      SCULPT_calc_area_center(sd, ob, nodes, totnode, r_area_co);
    }

    /* For area normal. */
    if ((!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) &&
        (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
      copy_v3_v3(r_area_no, ss->cache->sculpt_normal);
    }
    else {
      copy_v3_v3(ss->cache->sculpt_normal, r_area_no);
    }

    /* For flatten center. */
    if ((!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) &&
        (brush->flag & BRUSH_ORIGINAL_PLANE)) {
      copy_v3_v3(r_area_co, ss->cache->last_center);
    }
    else {
      copy_v3_v3(ss->cache->last_center, r_area_co);
    }
  }
  else {
    /* For area normal. */
    copy_v3_v3(r_area_no, ss->cache->sculpt_normal);

    /* For flatten center. */
    copy_v3_v3(r_area_co, ss->cache->last_center);

    /* For area normal. */
    flip_v3(r_area_no, ss->cache->mirror_symmetry_pass);

    /* For flatten center. */
    flip_v3(r_area_co, ss->cache->mirror_symmetry_pass);

    /* For area normal. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_no);

    /* For flatten center. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_co);

    /* Shift the plane for the current tile. */
    add_v3_v3(r_area_co, ss->cache->plane_offset);
  }
}

static void sculpt_rake_rotate(const SculptSession *ss,
                               const float sculpt_co[3],
                               const float v_co[3],
                               float factor,
                               float r_delta[3])
{
  float vec_rot[3];

#if 0
  /* lerp */
  sub_v3_v3v3(vec_rot, v_co, sculpt_co);
  mul_qt_v3(ss->cache->rake_rotation_symmetry, vec_rot);
  add_v3_v3(vec_rot, sculpt_co);
  sub_v3_v3v3(r_delta, vec_rot, v_co);
  mul_v3_fl(r_delta, factor);
#else
  /* slerp */
  float q_interp[4];
  sub_v3_v3v3(vec_rot, v_co, sculpt_co);

  copy_qt_qt(q_interp, ss->cache->rake_rotation_symmetry);
  pow_qt_fl_normalized(q_interp, factor);
  mul_qt_v3(q_interp, vec_rot);

  add_v3_v3(vec_rot, sculpt_co);
  sub_v3_v3v3(r_delta, vec_rot, v_co);
#endif
}

/**
 * Align the grab delta to the brush normal.
 *
 * \param grab_delta: Typically from `ss->cache->grab_delta_symmetry`.
 */
static void sculpt_project_v3_normal_align(SculptSession *ss,
                                           const float normal_weight,
                                           float grab_delta[3])
{
  /* Signed to support grabbing in (to make a hole) as well as out. */
  const float len_signed = dot_v3v3(ss->cache->sculpt_normal_symm, grab_delta);

  /* This scale effectively projects the offset so dragging follows the cursor,
   * as the normal points towards the view, the scale increases. */
  float len_view_scale;
  {
    float view_aligned_normal[3];
    project_plane_v3_v3v3(
        view_aligned_normal, ss->cache->sculpt_normal_symm, ss->cache->view_normal);
    len_view_scale = fabsf(dot_v3v3(view_aligned_normal, ss->cache->sculpt_normal_symm));
    len_view_scale = (len_view_scale > FLT_EPSILON) ? 1.0f / len_view_scale : 1.0f;
  }

  mul_v3_fl(grab_delta, 1.0f - normal_weight);
  madd_v3_v3fl(
      grab_delta, ss->cache->sculpt_normal_symm, (len_signed * normal_weight) * len_view_scale);
}

/************************************** Brushes ******************************/

/****** Twist Brush **********/

static void do_twist_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;

  PBVHVertexIter vd;
  const bool flip = (ss->cache->bstrength < 0.0f);
  const float bstrength = flip ? -ss->cache->bstrength : ss->cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  float stroke_direction[3];
  float stroke_line[2][3];
  normalize_v3_v3(stroke_direction, ss->cache->grab_delta_symmetry);
  copy_v3_v3(stroke_line[0], ss->cache->location);
  add_v3_v3v3(stroke_line[1], stroke_line[0], stroke_direction);

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {

    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    if (fade == 0.0f) {
      continue;
    }

    float local_vert_co[3];
    float rotation_axis[3] = {0.0, 1.0, 0.0};
    float origin[3] = {0.0, 0.0, 0.0f};
    float vertex_in_line[3];
    float scaled_mat[4][4];
    float scaled_mat_inv[4][4];

    copy_m4_m4(scaled_mat, mat);
    invert_m4(scaled_mat);
    mul_v3_fl(scaled_mat[2], 0.7f * fade * (1.0f - bstrength));
    invert_m4(scaled_mat);

    invert_m4_m4(scaled_mat_inv, scaled_mat);

    mul_v3_m4v3(local_vert_co, scaled_mat, vd.co);
    closest_to_line_v3(vertex_in_line, local_vert_co, rotation_axis, origin);
    float p_to_rotate[3];
    sub_v3_v3v3(p_to_rotate, local_vert_co, vertex_in_line);
    float p_rotated[3];
    rotate_v3_v3v3fl(p_rotated, p_to_rotate, rotation_axis, 2.0f * bstrength * fade);
    add_v3_v3(p_rotated, vertex_in_line);
    mul_v3_m4v3(p_rotated, scaled_mat_inv, p_rotated);

    float disp[3];
    sub_v3_v3v3(disp, p_rotated, vd.co);
    mul_v3_fl(disp, bstrength * fade);
    add_v3_v3(vd.co, disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_twist_brush_post_smooth_task_cb_ex(void *__restrict userdata,
                                                  const int n,
                                                  const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    float local_vert_co[3];
    float scaled_mat[4][4];
    copy_m4_m4(scaled_mat, mat);
    invert_m4(scaled_mat);
    invert_m4(scaled_mat);
    mul_v3_m4v3(local_vert_co, scaled_mat, vd.co);

    const float brush_fade = SCULPT_brush_strength_factor(ss,
                                                          brush,
                                                          vd.co,
                                                          sqrtf(test.dist),
                                                          vd.no,
                                                          vd.fno,
                                                          vd.mask ? *vd.mask : 0.0f,
                                                          vd.vertex,
                                                          thread_id);

    float smooth_fade = SCULPT_brush_strength_factor(ss,
                                                     brush,
                                                     vd.co,
                                                     local_vert_co[0],
                                                     vd.no,
                                                     vd.fno,
                                                     vd.mask ? *vd.mask : 0.0f,
                                                     vd.vertex,
                                                     thread_id);

    if (brush_fade == 0.0f) {
      // continue;
    }

    if (smooth_fade == 0.0f) {
      // continue;
    }

    smooth_fade = 1.0f - min_ff(fabsf(local_vert_co[0]), 1.0f);
    smooth_fade = pow3f(smooth_fade);

    float disp[3];

    /*
            SCULPT_neighbor_coords_average(ss, avg, vd.index);

            sub_v3_v3v3(disp, avg, vd.co);
            mul_v3_fl(disp, 1.0f - smooth_fade);
            add_v3_v3(vd.co, disp);
            */

    float final_co[3];
    SCULPT_relax_vertex(
        ss, &vd, clamp_f(smooth_fade, 0.0f, 1.0f), SCULPT_BOUNDARY_DEFAULT, final_co);

    sub_v3_v3v3(disp, final_co, vd.co);
    add_v3_v3(vd.co, disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_twist_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

  /* The sculpt-plane normal (whatever its set to). */
  float area_no_sp[3];

  /* Geometry normal */
  float area_no[3];
  float area_co[3];

  float scale[4][4];
  float tmat[4][4];
  float mat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no_sp, area_co);
  SCULPT_tilt_apply_to_normal(area_no_sp, ss->cache, brush->tilt_strength_factor);

  if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
    SCULPT_calc_area_normal(sd, ob, nodes, totnode, area_no);
  }
  else {
    copy_v3_v3(area_no, area_no_sp);
  }

  /* Delay the first daub because grab delta is not setup. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  /*
    mul_v3_v3v3(temp, area_no_sp, ss->cache->scale);
    mul_v3_fl(temp, displace);
    add_v3_v3(area_co, temp);
    */

  /* Initialize brush local-space matrix. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], area_co);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  /* Scale brush local space matrix. */
  scale_m4_fl(scale, ss->cache->radius * 0.5f);
  mul_m4_m4m4(tmat, mat, scale);

  /* Scale rotation space. */
  // mul_v3_fl(tmat[2], 0.5f);

  float twist_mat[4][4];
  invert_m4_m4(twist_mat, tmat);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no_sp = area_no_sp,
      .area_co = area_co,
      .mat = twist_mat,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_twist_brush_task_cb_ex, &settings);

  scale_m4_fl(scale, ss->cache->radius);
  mul_m4_m4m4(tmat, mat, scale);
  float smooth_mat[4][4];
  invert_m4_m4(smooth_mat, tmat);
  data.mat = smooth_mat;

  for (int i = 0; i < 2; i++) {
    BLI_task_parallel_range(0, totnode, &data, do_twist_brush_post_smooth_task_cb_ex, &settings);
  }
}

static void do_fill_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    if (!SCULPT_plane_point_side(vd.co, test.plane_tool)) {
      continue;
    }

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    sub_v3_v3v3(val, intr, vd.co);

    if (!SCULPT_plane_trim(ss->cache, brush, val)) {
      continue;
    }

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], val, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_fill_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];
  float offset = SCULPT_brush_plane_offset_get(sd, ss);

  float displace;

  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  SCULPT_tilt_apply_to_normal(area_no, ss->cache, brush->tilt_strength_factor);

  displace = radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_fill_brush_task_cb_ex, &settings);
}

static void do_scrape_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    if (SCULPT_plane_point_side(vd.co, test.plane_tool)) {
      continue;
    }

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    sub_v3_v3v3(val, intr, vd.co);

    if (!SCULPT_plane_trim(ss->cache, brush, val)) {
      continue;
    }

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], val, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_scrape_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];
  float offset = SCULPT_brush_plane_offset_get(sd, ss);

  float displace;

  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  SCULPT_tilt_apply_to_normal(area_no, ss->cache, brush->tilt_strength_factor);

  displace = -radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_scrape_brush_task_cb_ex, &settings);
}

/* -------------------------------------------------------------------- */
/** \name Sculpt Clay Thumb Brush
 * \{ */

static void do_clay_thumb_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;
  const float *area_no_sp = data->area_no_sp;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = data->clay_strength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  float plane_tilt[4];
  float normal_tilt[3];
  float imat[4][4];

  invert_m4_m4(imat, mat);
  rotate_v3_v3v3fl(normal_tilt, area_no_sp, imat[0], DEG2RADF(-ss->cache->clay_thumb_front_angle));

  /* Plane aligned to the geometry normal (back part of the brush). */
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no_sp);
  /* Tilted plane (front part of the brush). */
  plane_from_point_normal_v3(plane_tilt, area_co, normal_tilt);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    float local_co[3];
    mul_v3_m4v3(local_co, mat, vd.co);
    float intr[3], intr_tilt[3];
    float val[3];

    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    closest_to_plane_normalized_v3(intr_tilt, plane_tilt, vd.co);

    /* Mix the deformation of the aligned and the tilted plane based on the brush space vertex
     * coordinates. */
    /* We can also control the mix with a curve if it produces noticeable artifacts in the center
     * of the brush. */
    const float tilt_mix = local_co[1] > 0.0f ? 0.0f : 1.0f;
    interp_v3_v3v3(intr, intr, intr_tilt, tilt_mix);
    sub_v3_v3v3(val, intr_tilt, vd.co);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], val, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static float sculpt_clay_thumb_get_stabilized_pressure(StrokeCache *cache)
{
  float final_pressure = 0.0f;
  for (int i = 0; i < SCULPT_CLAY_STABILIZER_LEN; i++) {
    final_pressure += cache->clay_pressure_stabilizer[i];
  }
  return final_pressure / SCULPT_CLAY_STABILIZER_LEN;
}

void SCULPT_do_clay_thumb_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;
  const float offset = SCULPT_brush_plane_offset_get(sd, ss);
  const float displace = radius * (0.25f + offset);

  /* Sampled geometry normal and area center. */
  float area_no_sp[3];
  float area_no[3];
  float area_co[3];

  float temp[3];
  float mat[4][4];
  float scale[4][4];
  float tmat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no_sp, area_co);

  if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
    SCULPT_calc_area_normal(sd, ob, nodes, totnode, area_no);
  }
  else {
    copy_v3_v3(area_no, area_no_sp);
  }

  /* Delay the first daub because grab delta is not setup. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    ss->cache->clay_thumb_front_angle = 0.0f;
    return;
  }

  /* Simulate the clay accumulation by increasing the plane angle as more samples are added to
   * the stroke. */
  if (SCULPT_stroke_is_main_symmetry_pass(ss->cache)) {
    ss->cache->clay_thumb_front_angle += 0.8f;
    ss->cache->clay_thumb_front_angle = clamp_f(ss->cache->clay_thumb_front_angle, 0.0f, 60.0f);
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  /* Displace the brush planes. */
  copy_v3_v3(area_co, ss->cache->location);
  mul_v3_v3v3(temp, area_no_sp, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  /* Initialize brush local-space matrix. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], ss->cache->location);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  /* Scale brush local space matrix. */
  scale_m4_fl(scale, ss->cache->radius);
  mul_m4_m4m4(tmat, mat, scale);
  invert_m4_m4(mat, tmat);

  float clay_strength = ss->cache->bstrength *
                        sculpt_clay_thumb_get_stabilized_pressure(ss->cache);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no_sp = area_no_sp,
      .area_co = ss->cache->location,
      .mat = mat,
      .clay_strength = clay_strength,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_clay_thumb_brush_task_cb_ex, &settings);
}

static void do_flatten_brush_task_cb_ex(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    float intr[3];
    float val[3];

    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

    sub_v3_v3v3(val, intr, vd.co);

    if (SCULPT_plane_trim(ss->cache, brush, val)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.vertex,
                                                                  thread_id);
      mul_v3_v3fl(proxy[vd.i], val, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_flatten_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];

  float offset = SCULPT_brush_plane_offset_get(sd, ss);
  float displace;
  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  SCULPT_tilt_apply_to_normal(area_no, ss->cache, brush->tilt_strength_factor);

  displace = radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_flatten_brush_task_cb_ex, &settings);
}

/* -------------------------------------------------------------------- */
/** \name Sculpt Clay Brush
 * \{ */

typedef struct ClaySampleData {
  float plane_dist[2];
} ClaySampleData;

static void calc_clay_surface_task_cb(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  ClaySampleData *csd = tls->userdata_chunk;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;
  float plane[4];

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);

  /* Apply the brush normal radius to the test before sampling. */
  float test_radius = sqrtf(test.radius_squared);
  test_radius *= brush->normal_radius_factor;
  test.radius_squared = test_radius * test_radius;
  plane_from_point_normal_v3(plane, area_co, area_no);

  if (is_zero_v4(plane)) {
    return;
  }

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    float plane_dist = dist_signed_to_plane_v3(vd.co, plane);
    float plane_dist_abs = fabsf(plane_dist);
    if (plane_dist > 0.0f) {
      csd->plane_dist[0] = MIN2(csd->plane_dist[0], plane_dist_abs);
    }
    else {
      csd->plane_dist[1] = MIN2(csd->plane_dist[1], plane_dist_abs);
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void calc_clay_surface_reduce(const void *__restrict UNUSED(userdata),
                                     void *__restrict chunk_join,
                                     void *__restrict chunk)
{
  ClaySampleData *join = chunk_join;
  ClaySampleData *csd = chunk;
  join->plane_dist[0] = MIN2(csd->plane_dist[0], join->plane_dist[0]);
  join->plane_dist[1] = MIN2(csd->plane_dist[1], join->plane_dist[1]);
}

static void do_clay_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = fabsf(ss->cache->bstrength);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    SCULPT_vertex_check_origdata(ss, vd.vertex);

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

    sub_v3_v3v3(val, intr, vd.co);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], val, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(data->nodes[n]);
}

void SCULPT_do_clay_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = fabsf(ss->cache->radius);
  const float initial_radius = fabsf(ss->cache->initial_radius);
  bool flip = ss->cache->bstrength < 0.0f;

  float offset = SCULPT_brush_plane_offset_get(sd, ss);
  float displace;

  float area_no[3];
  float area_co[3];
  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  SculptThreadedTaskData sample_data = {
      .sd = NULL,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .totnode = totnode,
      .area_no = area_no,
      .area_co = ss->cache->location,
  };

  ClaySampleData csd = {{0}};

  TaskParallelSettings sample_settings;
  BKE_pbvh_parallel_range_settings(&sample_settings, true, totnode);
  sample_settings.func_reduce = calc_clay_surface_reduce;
  sample_settings.userdata_chunk = &csd;
  sample_settings.userdata_chunk_size = sizeof(ClaySampleData);

  BLI_task_parallel_range(0, totnode, &sample_data, calc_clay_surface_task_cb, &sample_settings);

  float d_offset = (csd.plane_dist[0] + csd.plane_dist[1]);
  d_offset = min_ff(radius, d_offset);
  d_offset = d_offset / radius;
  d_offset = 1.0f - d_offset;
  displace = fabsf(initial_radius * (0.25f + offset + (d_offset * 0.15f)));
  if (flip) {
    displace = -displace;
  }

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  copy_v3_v3(area_co, ss->cache->location);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_clay_brush_task_cb_ex, &settings);
}

static void do_clay_strips_brush_task_cb_ex(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;
  const float *area_no_sp = data->area_no_sp;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  SculptBrushTest test;
  float(*proxy)[3];
  const bool flip = (ss->cache->bstrength < 0.0f);
  const float bstrength = flip ? -ss->cache->bstrength : ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SCULPT_brush_test_init(ss, &test);
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no_sp);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!SCULPT_brush_test_cube(&test, vd.co, mat, brush->tip_roundness)) {
      continue;
    }

    if (!plane_point_side_flip(vd.co, test.plane_tool, flip)) {
      continue;
    }

    float vertex_no[3];
    SCULPT_vertex_normal_get(ss, vd.vertex, vertex_no);
    if (dot_v3v3(area_no_sp, vertex_no) <= -0.1f) {
      continue;
    }

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    sub_v3_v3v3(val, intr, vd.co);

    if (!SCULPT_plane_trim(ss->cache, brush, val)) {
      continue;
    }

    /* The normal from the vertices is ignored, it causes glitch with planes, see: T44390. */
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                ss->cache->radius * test.dist,
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], val, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(data->nodes[n]);
}

void SCULPT_do_clay_strips_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const bool flip = (ss->cache->bstrength < 0.0f);
  const float radius = flip ? -ss->cache->radius : ss->cache->radius;
  const float offset = SCULPT_brush_plane_offset_get(sd, ss);
  const float displace = radius * (0.18f + offset);

  SCULPT_vertex_random_access_ensure(ss);

  /* The sculpt-plane normal (whatever its set to). */
  float area_no_sp[3];

  /* Geometry normal */
  float area_no[3];
  float area_co[3];

  float temp[3];
  float mat[4][4];
  float scale[4][4];
  float tmat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no_sp, area_co);
  SCULPT_tilt_apply_to_normal(area_no_sp, ss->cache, brush->tilt_strength_factor);

  if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
    SCULPT_calc_area_normal(sd, ob, nodes, totnode, area_no);
  }
  else {
    copy_v3_v3(area_no, area_no_sp);
  }

  /* Delay the first daub because grab delta is not setup. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  mul_v3_v3v3(temp, area_no_sp, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  /* Clay Strips uses a cube test with falloff in the XY axis (not in Z) and a plane to deform the
   * vertices. When in Add mode, vertices that are below the plane and inside the cube are move
   * towards the plane. In this situation, there may be cases where a vertex is outside the cube
   * but below the plane, so won't be deformed, causing artifacts. In order to prevent these
   * artifacts, this displaces the test cube space in relation to the plane in order to
   * deform more vertices that may be below it. */
  /* The 0.7 and 1.25 factors are arbitrary and don't have any relation between them, they were set
   * by doing multiple tests using the default "Clay Strips" brush preset. */
  float area_co_displaced[3];
  madd_v3_v3v3fl(area_co_displaced, area_co, area_no, -radius * 0.7f);

  /* Initialize brush local-space matrix. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], area_co_displaced);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  /* Scale brush local space matrix. */
  scale_m4_fl(scale, ss->cache->radius);
  mul_m4_m4m4(tmat, mat, scale);

  /* Deform the local space in Z to scale the test cube. As the test cube does not have falloff in
   * Z this does not produce artifacts in the falloff cube and allows to deform extra vertices
   * during big deformation while keeping the surface as uniform as possible. */
  mul_v3_fl(tmat[2], 1.25f);

  invert_m4_m4(mat, tmat);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no_sp = area_no_sp,
      .area_co = area_co,
      .mat = mat,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_clay_strips_brush_task_cb_ex, &settings);
}

static void do_snake_hook_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  SculptProjectVector *spvc = data->spvc;
  const float *grab_delta = data->grab_delta;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;
  const bool do_rake_rotation = ss->cache->is_rake_rotation_valid;
  const bool do_pinch = (data->crease_pinch_factor != 0.5f);
  const float pinch = do_pinch ? (2.0f * (0.5f - data->crease_pinch_factor) *
                                  (len_v3(grab_delta) / ss->cache->radius)) :
                                 0.0f;

  const bool do_elastic = brush->snake_hook_deform_type == BRUSH_SNAKE_HOOK_DEFORM_ELASTIC;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  KelvinletParams params;
  BKE_kelvinlet_init_params(&params, ss->cache->radius, bstrength, 1.0f, 0.4f);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!do_elastic && !sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    float fade;
    if (do_elastic) {
      fade = 1.0f;
    }
    else {
      fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.vertex,
                                                      thread_id);
    }

    mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

    /* Negative pinch will inflate, helps maintain volume. */
    if (do_pinch) {
      float delta_pinch_init[3], delta_pinch[3];

      sub_v3_v3v3(delta_pinch, vd.co, test.location);
      if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
        project_plane_v3_v3v3(delta_pinch, delta_pinch, ss->cache->true_view_normal);
      }

      /* Important to calculate based on the grabbed location
       * (intentionally ignore fade here). */
      add_v3_v3(delta_pinch, grab_delta);

      sculpt_project_v3(spvc, delta_pinch, delta_pinch);

      copy_v3_v3(delta_pinch_init, delta_pinch);

      float pinch_fade = pinch * fade;
      /* When reducing, scale reduction back by how close to the center we are,
       * so we don't pinch into nothingness. */
      if (pinch > 0.0f) {
        /* Square to have even less impact for close vertices. */
        pinch_fade *= pow2f(min_ff(1.0f, len_v3(delta_pinch) / ss->cache->radius));
      }
      mul_v3_fl(delta_pinch, 1.0f + pinch_fade);
      sub_v3_v3v3(delta_pinch, delta_pinch_init, delta_pinch);
      add_v3_v3(proxy[vd.i], delta_pinch);
    }

    if (do_rake_rotation) {
      float delta_rotate[3];
      sculpt_rake_rotate(ss, test.location, vd.co, fade, delta_rotate);
      add_v3_v3(proxy[vd.i], delta_rotate);
    }

    if (do_elastic) {
      float disp[3];
      BKE_kelvinlet_grab_triscale(disp, &params, vd.co, ss->cache->location, proxy[vd.i]);
      mul_v3_fl(disp, bstrength * 20.0f);
      if (vd.mask) {
        mul_v3_fl(disp, 1.0f - *vd.mask);
      }
      mul_v3_fl(disp, SCULPT_automasking_factor_get(ss->cache->automasking, ss, vd.vertex));
      copy_v3_v3(proxy[vd.i], disp);
    }

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_snake_hook_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const float bstrength = ss->cache->bstrength;
  float grab_delta[3];

  SculptProjectVector spvc;

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (bstrength < 0.0f) {
    negate_v3(grab_delta);
  }

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  float crease_pinch_factor = SCULPT_get_float(ss, crease_pinch_factor, sd, brush);

  /* Optionally pinch while painting. */
  if (crease_pinch_factor != 0.5f) {
    sculpt_project_v3_cache_init(&spvc, grab_delta);
  }

  SculptThreadedTaskData data = {.sd = sd,
                                 .ob = ob,
                                 .brush = brush,
                                 .nodes = nodes,
                                 .spvc = &spvc,
                                 .grab_delta = grab_delta,
                                 .crease_pinch_factor = crease_pinch_factor};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_snake_hook_brush_task_cb_ex, &settings);
}

static void do_thumb_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *cono = data->cono;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                orig_data.co,
                                                                sqrtf(test.dist),
                                                                orig_data.no,
                                                                NULL,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], cono, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_thumb_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];
  float tmp[3], cono[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  cross_v3_v3v3(tmp, ss->cache->sculpt_normal_symm, grab_delta);
  cross_v3_v3v3(cono, tmp, ss->cache->sculpt_normal_symm);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .cono = cono,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_thumb_brush_task_cb_ex, &settings);
}

static void do_rotate_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float angle = data->angle;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_vertex_check_origdata(ss, vd.vertex);

    float *co = SCULPT_vertex_origco_get(ss, vd.vertex);
    float *no = SCULPT_vertex_origno_get(ss, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, co)) {
      continue;
    }

    float vec[3], rot[3][3];
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                co,
                                                                sqrtf(test.dist),
                                                                NULL,
                                                                no,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    sub_v3_v3v3(vec, co, ss->cache->location);
    axis_angle_normalized_to_mat3(rot, ss->cache->sculpt_normal_symm, angle * fade);
    mul_v3_m3v3(proxy[vd.i], rot, vec);
    add_v3_v3(proxy[vd.i], ss->cache->location);
    sub_v3_v3(proxy[vd.i], co);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(data->nodes[n]);
}

void SCULPT_do_rotate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  static const int flip[8] = {1, -1, -1, 1, -1, 1, 1, -1};
  const float angle = ss->cache->vertex_rotation * flip[ss->cache->mirror_symmetry_pass];

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .angle = angle,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_rotate_brush_task_cb_ex, &settings);
}

//#define LAYER_FACE_SET_MODE

static void do_layer_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;

  bool use_persistent_base = brush->flag & BRUSH_PERSISTENT;
  const bool is_bmesh = BKE_pbvh_type(ss->pbvh) == PBVH_BMESH;

  if (is_bmesh) {
    use_persistent_base = use_persistent_base && ss->custom_layers[SCULPT_SCL_PERS_CO];

#if 0
    // check if we need to zero displacement factor
    // in first run of brush stroke
    if (!use_persistent_base) {
      int nidx = BKE_pbvh_get_node_index(ss->pbvh, data->nodes[n]);

      bool reset_disp = !BLI_BITMAP_TEST(ss->cache->layer_disp_map, nidx);
      if (reset_disp) {
        PBVHVertexIter vd;

        BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
          BMVert *v = (BMVert *)vd.vertex.i;
          float *disp_factor = BM_ELEM_CD_GET_VOID_P(v, data->cd_layer_disp);

          *disp_factor = 0.0f;

          BLI_BITMAP_SET(ss->cache->layer_disp_map, nidx, true);
        }
        BKE_pbvh_vertex_iter_end;
      }
    }
#endif
  }
  else {
    use_persistent_base = use_persistent_base && ss->custom_layers[SCULPT_SCL_PERS_CO];
  }

  SculptCustomLayer *scl_disp = data->scl;
  SculptCustomLayer *scl_stroke_id = data->scl2;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  const float bstrength = ss->cache->bstrength;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }

    if (!use_persistent_base) {
      int *stroke_id = SCULPT_temp_cdata_get(vd.vertex, scl_stroke_id);

      if (*stroke_id != ss->stroke_id) {
        *((float *)SCULPT_temp_cdata_get(vd.vertex, scl_disp)) = 0.0f;
        *stroke_id = ss->stroke_id;
      }
    }

    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    float *disp_factor;

    if (use_persistent_base) {
      disp_factor = (float *)SCULPT_temp_cdata_get(vd.vertex,
                                                   ss->custom_layers[SCULPT_SCL_PERS_DISP]);
    }
    else {
      disp_factor = (float *)SCULPT_temp_cdata_get(vd.vertex, scl_disp);
    }

    /* When using persistent base, the layer brush (holding Control) invert mode resets the
     * height of the layer to 0. This makes possible to clean edges of previously added layers
     * on top of the base. */
    /* The main direction of the layers is inverted using the regular brush strength with the
     * brush direction property. */
    if (use_persistent_base && ss->cache->invert) {
      (*disp_factor) += fabsf(fade * bstrength * (*disp_factor)) *
                        ((*disp_factor) > 0.0f ? -1.0f : 1.0f);
    }
    else {
      (*disp_factor) += fade * bstrength * (1.05f - fabsf(*disp_factor));
    }
    if (vd.mask) {
      const float clamp_mask = 1.0f - *vd.mask;
      *disp_factor = clamp_f(*disp_factor, -clamp_mask, clamp_mask);
    }
    else {
      *disp_factor = clamp_f(*disp_factor, -1.0f, 1.0f);
    }

    float final_co[3];
    float normal[3];

    if (use_persistent_base) {
      SCULPT_vertex_persistent_normal_get(ss, vd.vertex, normal);
      mul_v3_fl(normal, brush->height);
      madd_v3_v3v3fl(
          final_co, SCULPT_vertex_persistent_co_get(ss, vd.vertex), normal, *disp_factor);
    }
    else {
      normal_short_to_float_v3(normal, orig_data.no);
      mul_v3_fl(normal, brush->height);
      madd_v3_v3v3fl(final_co, orig_data.co, normal, *disp_factor);
    }

    float vdisp[3];
    sub_v3_v3v3(vdisp, final_co, vd.co);
    mul_v3_fl(vdisp, fabsf(fade));
    add_v3_v3v3(final_co, vd.co, vdisp);

    SCULPT_clip(sd, ss, vd.co, final_co);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

#ifdef LAYER_FACE_SET_MODE
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    TableGSet *bm_faces = BKE_pbvh_bmesh_node_faces(data->nodes[n]);
    TableGSet *bm_unique_verts = BKE_pbvh_bmesh_node_unique_verts(data->nodes[n]);
    BMFace *f;

    const int cd_vcol = ss->cd_vcol_offset;

    int fset2 = 1;  // data->face_set;
    const int cd_disp = use_persistent_base ? data->cd_pers_disp : data->cd_layer_disp;
    int f_ni = BKE_pbvh_get_node_index(ss->pbvh, data->nodes[n]);
    BMVert *v;

    TGSET_ITER (v, bm_unique_verts) {
      BMIter iter;
      BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
        if (BM_ELEM_CD_GET_INT(f, ss->cd_face_node_offset) != f_ni) {
          continue;
        }
        if (BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset) < 0) {
          // continue;
        }

        fset2 = 1;
        float height = 0.0;
        int tot = 0;
        BMLoop *l = f->l_first;

        int inside = 0;

        do {
          int v_ni = BM_ELEM_CD_GET_INT(l->v, ss->cd_vert_node_offset);

          float *disp_factor = BM_ELEM_CD_GET_VOID_P(l->v, cd_disp);
          MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, l->v);
          MV_ADD_FLAG(mv, SCULPTVERT_NEED_BOUNDARY);

          if (cd_vcol >= 0) {
            MPropCol *col = BM_ELEM_CD_GET_VOID_P(l->v, cd_vcol);
            col->color[0] = MAX2(*disp_factor, 0.0f);
            col->color[1] = MAX2(-(*disp_factor), 0.0f);
            col->color[2] = 0.0f;
            col->color[3] = 1.0f;
          }

          if (sculpt_brush_test_sq_fn(&test, l->v->co)) {  // mv->origco)) {
            inside++;
          }

          if (*disp_factor < 0.9) {
            fset2 = data->face_set2;
          }

          height += *disp_factor;
          tot++;
        } while ((l = l->next) != f->l_first);

        if (!inside) {
          continue;
        }

        int *ptr = BM_ELEM_CD_GET_VOID_P(f, ss->cd_faceset_offset);
        int old = *ptr;
        atomic_cas_int32(ptr, old, fset2);

        // BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset2);
      }
    }
    TGSET_ITER_END;
  }

#endif
}

void SCULPT_ensure_persistent_layers(SculptSession *ss)
{
  if (!ss->custom_layers[SCULPT_SCL_PERS_CO]) {
    SculptLayerParams params = {.permanent = true, .simple_array = false};

    ss->custom_layers[SCULPT_SCL_PERS_CO] = MEM_callocN(sizeof(SculptCustomLayer), "scl_pers_co");
    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT3,
                                SCULPT_LAYER_PERS_CO,
                                ss->custom_layers[SCULPT_SCL_PERS_CO],
                                &params);

    ss->custom_layers[SCULPT_SCL_PERS_NO] = MEM_callocN(sizeof(SculptCustomLayer), "scl_pers_no");
    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT3,
                                SCULPT_LAYER_PERS_NO,
                                ss->custom_layers[SCULPT_SCL_PERS_NO],
                                &params);

    ss->custom_layers[SCULPT_SCL_PERS_DISP] = MEM_callocN(sizeof(SculptCustomLayer),
                                                          "scl_pers_disp");
    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT,
                                SCULPT_LAYER_PERS_DISP,
                                ss->custom_layers[SCULPT_SCL_PERS_DISP],
                                &params);
  }
}

void SCULPT_do_layer_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

#ifdef LAYER_FACE_SET_MODE
  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    ss->cache->paint_face_set = SCULPT_face_set_next_available_get(ss);
  }

  const int fset = ss->cache->paint_face_set;
#endif

  if ((brush->flag & BRUSH_PERSISTENT) && SCULPT_has_persistent_base(ss)) {
    SCULPT_ensure_persistent_layers(ss);
  }

  if (!ss->custom_layers[SCULPT_SCL_LAYER_DISP]) {
    ss->custom_layers[SCULPT_SCL_LAYER_DISP] = MEM_callocN(sizeof(SculptCustomLayer),
                                                           "layer disp scl");
    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT,
                                SCULPT_LAYER_DISP,
                                ss->custom_layers[SCULPT_SCL_LAYER_DISP],
                                &((SculptLayerParams){.permanent = false, .simple_array = false}));
  }

  if (!ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID]) {
    ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID] = MEM_callocN(sizeof(SculptCustomLayer),
                                                                "layer disp scl");
    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_INT32,
                                SCULPT_LAYER_STROKE_ID,
                                ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID],
                                &((SculptLayerParams){.permanent = false, .simple_array = false}));
  }

  if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
    ss->cache->layer_displacement_factor = ss->custom_layers[SCULPT_SCL_LAYER_DISP]->data;
    ss->cache->layer_stroke_id = ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID]->data;
  }

  SCULPT_vertex_random_access_ensure(ss);

  SculptThreadedTaskData data = {.sd = sd,
                                 .ob = ob,
                                 .brush = brush,
                                 .nodes = nodes,
                                 .scl = ss->custom_layers[SCULPT_SCL_LAYER_DISP],
                                 .scl2 = ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID],
#ifdef LAYER_FACE_SET_MODE
                                 .face_set = fset,
                                 .face_set2 = fset + 1

#endif
  };

  TaskParallelSettings settings;
#ifdef LAYER_FACE_SET_MODE
  BKE_pbvh_parallel_range_settings(&settings, false, totnode);
#else
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
#endif
  BLI_task_parallel_range(0, totnode, &data, do_layer_brush_task_cb_ex, &settings);
}

static void do_inflate_brush_task_cb_ex(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);
    float val[3];

    if (vd.fno) {
      copy_v3_v3(val, vd.fno);
    }
    else {
      normal_short_to_float_v3(val, vd.no);
    }

    mul_v3_fl(val, fade * ss->cache->radius);
    mul_v3_v3v3(proxy[vd.i], val, ss->cache->scale);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_inflate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_inflate_brush_task_cb_ex, &settings);
}

static void do_nudge_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *cono = data->cono;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], cono, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_nudge_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];
  float tmp[3], cono[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  cross_v3_v3v3(tmp, ss->cache->sculpt_normal_symm, grab_delta);
  cross_v3_v3v3(cono, tmp, ss->cache->sculpt_normal_symm);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .cono = cono,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_nudge_brush_task_cb_ex, &settings);
}

/**
 * Used for 'SCULPT_TOOL_CREASE' and 'SCULPT_TOOL_BLOB'
 */
static void do_crease_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  SculptProjectVector *spvc = data->spvc;
  const float flippedbstrength = data->flippedbstrength;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    /* Offset vertex. */
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);
    float val1[3];
    float val2[3];

    /* First we pinch. */
    sub_v3_v3v3(val1, test.location, vd.co);
    if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(val1, val1, ss->cache->view_normal);
    }

    mul_v3_fl(val1, fade * flippedbstrength);

    sculpt_project_v3(spvc, val1, val1);

    /* Then we draw. */
    mul_v3_v3fl(val2, offset, fade);

    add_v3_v3v3(proxy[vd.i], val1, val2);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_crease_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  float offset[3];
  float bstrength = ss->cache->bstrength;
  float flippedbstrength, crease_correction;
  float brush_alpha;

  SculptProjectVector spvc;

  /* Offset with as much as possible factored in already. */
  mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* We divide out the squared alpha and multiply by the squared crease
   * to give us the pinch strength. */
  crease_correction = SCULPT_get_float(ss, crease_pinch_factor, sd, brush);
  crease_correction = crease_correction * crease_correction;

  brush_alpha = BRUSHSET_GET_FINAL_FLOAT(brush->channels, sd->channels, strength, NULL);
  if (brush_alpha > 0.0f) {
    crease_correction /= brush_alpha * brush_alpha;
  }

  /* We always want crease to pinch or blob to relax even when draw is negative. */
  flippedbstrength = (bstrength < 0.0f) ? -crease_correction * bstrength :
                                          crease_correction * bstrength;

  if (brush->sculpt_tool == SCULPT_TOOL_BLOB) {
    flippedbstrength *= -1.0f;
  }

  /* Use surface normal for 'spvc', so the vertices are pinched towards a line instead of a
   * single point. Without this we get a 'flat' surface surrounding the pinch. */
  sculpt_project_v3_cache_init(&spvc, ss->cache->sculpt_normal_symm);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .spvc = &spvc,
      .offset = offset,
      .crease_pinch_factor = SCULPT_get_float(ss, crease_pinch_factor, sd, brush),
      .flippedbstrength = flippedbstrength,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_crease_brush_task_cb_ex, &settings);
}

static void do_pinch_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*stroke_xz)[3] = data->stroke_xz;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  float x_object_space[3];
  float z_object_space[3];
  copy_v3_v3(x_object_space, stroke_xz[0]);
  copy_v3_v3(z_object_space, stroke_xz[1]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);
    float disp_center[3];
    float x_disp[3];
    float z_disp[3];
    /* Calculate displacement from the vertex to the brush center. */
    sub_v3_v3v3(disp_center, test.location, vd.co);

    /* Project the displacement into the X vector (aligned to the stroke). */
    mul_v3_v3fl(x_disp, x_object_space, dot_v3v3(disp_center, x_object_space));

    /* Project the displacement into the Z vector (aligned to the surface normal). */
    mul_v3_v3fl(z_disp, z_object_space, dot_v3v3(disp_center, z_object_space));

    /* Add the two projected vectors to calculate the final displacement.
     * The Y component is removed. */
    add_v3_v3v3(disp_center, x_disp, z_disp);

    if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(disp_center, disp_center, ss->cache->view_normal);
    }
    mul_v3_v3fl(proxy[vd.i], disp_center, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_pinch_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  float area_no[3];
  float area_co[3];

  float mat[4][4];
  calc_sculpt_plane(sd, ob, nodes, totnode, area_no, area_co);

  /* delay the first daub because grab delta is not setup */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  /* Initialize `mat`. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], ss->cache->location);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  float stroke_xz[2][3];
  normalize_v3_v3(stroke_xz[0], mat[0]);
  normalize_v3_v3(stroke_xz[1], mat[2]);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .stroke_xz = stroke_xz,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_pinch_brush_task_cb_ex, &settings);
}

static void do_grab_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *grab_delta = data->grab_delta;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  const bool grab_silhouette = brush->flag2 & BRUSH_GRAB_SILHOUETTE;
  const bool use_geodesic_dists = brush->flag2 & BRUSH_USE_SURFACE_FALLOFF;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }

    float dist;
    if (use_geodesic_dists) {
      dist = ss->cache->geodesic_dists[ss->cache->mirror_symmetry_pass][vd.index];
    }
    else {
      dist = sqrtf(test.dist);
    }

    float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                          brush,
                                                          orig_data.co,
                                                          dist,
                                                          orig_data.no,
                                                          NULL,
                                                          vd.mask ? *vd.mask : 0.0f,
                                                          vd.vertex,
                                                          thread_id);

    if (grab_silhouette) {
      float silhouette_test_dir[3];
      normalize_v3_v3(silhouette_test_dir, grab_delta);
      if (dot_v3v3(ss->cache->initial_normal, ss->cache->grab_delta_symmetry) < 0.0f) {
        mul_v3_fl(silhouette_test_dir, -1.0f);
      }
      float vno[3];
      normal_short_to_float_v3(vno, orig_data.no);
      fade *= max_ff(dot_v3v3(vno, silhouette_test_dir), 0.0f);
    }

    mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_grab_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  float grab_delta[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  if (brush->flag2 & BRUSH_USE_SURFACE_FALLOFF) {
    if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
      const int symm_pass = ss->cache->mirror_symmetry_pass;
      float location[3];
      flip_v3_v3(location, SCULPT_active_vertex_co_get(ss), symm_pass);
      SculptVertRef v = SCULPT_nearest_vertex_get(sd, ob, location, ss->cache->radius, false);
      ss->cache->geodesic_dists[symm_pass] = SCULPT_geodesic_from_vertex(
          ob, v, ss->cache->initial_radius);
    }
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .grab_delta = grab_delta,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_grab_brush_task_cb_ex, &settings);
}

static void do_elastic_deform_brush_task_cb_ex(void *__restrict userdata,
                                               const int n,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *grab_delta = data->grab_delta;
  const float *location = ss->cache->location;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];

  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  float dir;
  if (ss->cache->mouse[0] > ss->cache->initial_mouse[0]) {
    dir = 1.0f;
  }
  else {
    dir = -1.0f;
  }

  if (brush->elastic_deform_type == BRUSH_ELASTIC_DEFORM_TWIST) {
    int symm = ss->cache->mirror_symmetry_pass;
    if (ELEM(symm, 1, 2, 4, 7)) {
      dir = -dir;
    }
  }

  KelvinletParams params;
  float force = len_v3(grab_delta) * dir * bstrength;
  BKE_kelvinlet_init_params(
      &params, ss->cache->radius, force, 1.0f, brush->elastic_deform_volume_preservation);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    float final_disp[3];

    float orig_co[3];
    if (brush->flag2 & BRUSH_USE_SURFACE_FALLOFF) {
      const float geodesic_dist =
          ss->cache->geodesic_dists[ss->cache->mirror_symmetry_pass][vd.index];

      if (geodesic_dist == FLT_MAX) {
        continue;
      }

      float disp[3];
      sub_v3_v3v3(disp, orig_data.co, ss->cache->initial_location);
      normalize_v3(disp);
      mul_v3_fl(disp, geodesic_dist);
      add_v3_v3v3(orig_co, ss->cache->initial_location, disp);
    }
    else {
      copy_v3_v3(orig_co, orig_data.co);
    }

    switch (brush->elastic_deform_type) {
      case BRUSH_ELASTIC_DEFORM_GRAB:
        BKE_kelvinlet_grab(final_disp, &params, orig_co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      case BRUSH_ELASTIC_DEFORM_GRAB_BISCALE: {
        BKE_kelvinlet_grab_biscale(final_disp, &params, orig_co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      }
      case BRUSH_ELASTIC_DEFORM_GRAB_TRISCALE: {
        BKE_kelvinlet_grab_triscale(final_disp, &params, orig_co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      }
      case BRUSH_ELASTIC_DEFORM_SCALE:
        BKE_kelvinlet_scale(final_disp, &params, orig_co, location, ss->cache->sculpt_normal_symm);
        break;
      case BRUSH_ELASTIC_DEFORM_TWIST:
        BKE_kelvinlet_twist(final_disp, &params, orig_co, location, ss->cache->sculpt_normal_symm);
        break;
    }

    if (vd.mask) {
      mul_v3_fl(final_disp, 1.0f - *vd.mask);
    }

    mul_v3_fl(final_disp, SCULPT_automasking_factor_get(ss->cache->automasking, ss, vd.vertex));

    if (dot_v3v3(final_disp, final_disp) > 0.0000001) {
      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }

    copy_v3_v3(proxy[vd.i], final_disp);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_elastic_deform_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  if (brush->flag2 & BRUSH_USE_SURFACE_FALLOFF) {
    if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
      const int symm_pass = ss->cache->mirror_symmetry_pass;
      float location[3];
      flip_v3_v3(location, SCULPT_active_vertex_co_get(ss), symm_pass);
      SculptVertRef v = SCULPT_nearest_vertex_get(
          sd, ob, location, ss->cache->initial_radius, false);
      ss->cache->geodesic_dists[symm_pass] = SCULPT_geodesic_from_vertex(ob, v, FLT_MAX);
    }
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .grab_delta = grab_delta,
  };

  TaskParallelSettings settings;

  SCULPT_vertex_random_access_ensure(ss);

  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_elastic_deform_brush_task_cb_ex, &settings);
}

static void do_draw_sharp_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  float planeco[3], noffset[3];
  copy_v3_v3(planeco, ss->cache->location);
  add_v3_v3(planeco, offset);

  copy_v3_v3(noffset, offset);
  normalize_v3(noffset);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_vertex_check_origdata(ss, vd.vertex);
    MSculptVert *mv = SCULPT_vertex_get_mdyntopo(ss, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, mv->origco)) {
      continue;
    }

    /* Offset vertex. */
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    mv->origco,
                                                    sqrtf(test.dist),
                                                    NULL,
                                                    mv->origno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    mul_v3_v3fl(proxy[vd.i], offset, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(data->nodes[n]);
}

static void do_draw_sharp_brush_task_cb_ex_plane(void *__restrict userdata,
                                                 const int n,
                                                 const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  float planeco[3], noffset[3];
  copy_v3_v3(planeco, ss->cache->location);
  add_v3_v3(planeco, offset);

  copy_v3_v3(noffset, offset);
  normalize_v3(noffset);

  const float bstrength = fabsf(ss->cache->bstrength);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    // SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    SCULPT_vertex_check_origdata(ss, vd.vertex);
    MSculptVert *mv = SCULPT_vertex_get_mdyntopo(ss, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, mv->origco)) {
      continue;
    }
    /* Offset vertex. */
    float fade = SCULPT_brush_strength_factor(ss,
                                              brush,
                                              mv->origco,
                                              sqrtf(test.dist),
                                              NULL,
                                              mv->origno,
                                              vd.mask ? *vd.mask : 0.0f,
                                              vd.vertex,
                                              thread_id);

    float vec[3];
    // copy_v3_v3(noffset, mv->origno);

    // interp_v3_v3v3(planeco, mv->origco, ss->cache->location, 1.0f - fade);
    copy_v3_v3(planeco, ss->cache->location);
    madd_v3_v3fl(planeco, noffset, ss->cache->radius * (fade * 0.5 + 0.5));

    sub_v3_v3v3(vec, mv->origco, planeco);
    madd_v3_v3fl(vec, noffset, -dot_v3v3(noffset, vec));

    add_v3_v3(vec, planeco);
    sub_v3_v3(vec, vd.co);

    mul_v3_v3fl(proxy[vd.i], vec, fade * fade * bstrength);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(data->nodes[n]);
}

void SCULPT_do_draw_sharp_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = ss->cache->brush ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  float offset[3];
  const float bstrength = ss->cache->bstrength;

  bool mode = SCULPT_get_int(ss, sharp_mode, sd, brush);
  float plane_offset = SCULPT_get_float(ss, plane_offset, sd, brush);

  /* Offset with as much as possible factored in already. */
  float effective_normal[3];
  SCULPT_tilt_effective_normal_get(ss, brush, effective_normal);

  if (mode == SCULPT_SHARP_PLANE) {  // average with view normal
    add_v3_v3(effective_normal, ss->cache->view_normal);
    normalize_v3(effective_normal);
  }

  mul_v3_v3fl(offset, effective_normal, ss->cache->radius + ss->cache->radius * plane_offset);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* XXX: this shouldn't be necessary, but sculpting crashes in blender2.8 otherwise
   * initialize before threads so they can do curve mapping. */
  BKE_curvemapping_init(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .offset = offset,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);

  if (mode == SCULPT_SHARP_SIMPLE) {
    BLI_task_parallel_range(0, totnode, &data, do_draw_sharp_brush_task_cb_ex, &settings);
  }
  else {
    BLI_task_parallel_range(0, totnode, &data, do_draw_sharp_brush_task_cb_ex_plane, &settings);
  }
}

/* -------------------------------------------------------------------- */
/** \name Sculpt Scene Project Brush
 * \{ */

void SCULPT_stroke_cache_snap_context_init(bContext *C, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;

  if (ss->cache && ss->cache->snap_context) {
    return;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);

  cache->snap_context = ED_transform_snap_object_context_create(scene, 0);
  cache->depsgraph = depsgraph;
}

static void sculpt_scene_project_view_ray_init(Object *ob,
                                               const SculptVertRef vertex,
                                               float r_ray_normal[3],
                                               float r_ray_origin[3])
{
  SculptSession *ss = ob->sculpt;

  float world_space_vertex_co[3];
  mul_v3_m4v3(world_space_vertex_co, ob->obmat, SCULPT_vertex_co_get(ss, vertex));
  if (ss->cache->vc->rv3d->is_persp) {
    sub_v3_v3v3(r_ray_normal, world_space_vertex_co, ss->cache->view_origin);
    normalize_v3(r_ray_normal);
    copy_v3_v3(r_ray_origin, ss->cache->view_origin);
  }
  else {
    mul_v3_mat3_m4v3(r_ray_normal, ob->obmat, ss->cache->view_normal);
    sub_v3_v3v3(r_ray_origin, world_space_vertex_co, r_ray_normal);
  }
}

static void sculpt_scene_project_vertex_normal_ray_init(Object *ob,
                                                        const SculptVertRef vertex,
                                                        const float original_normal[3],
                                                        float r_ray_normal[3],
                                                        float r_ray_origin[3])
{
  SculptSession *ss = ob->sculpt;
  mul_v3_m4v3(r_ray_normal, ob->obmat, original_normal);
  normalize_v3(r_ray_normal);

  mul_v3_m4v3(r_ray_origin, ob->obmat, SCULPT_vertex_co_get(ss, vertex));
}

static void sculpt_scene_project_brush_normal_ray_init(Object *ob,
                                                       const SculptVertRef vertex,
                                                       float r_ray_normal[3],
                                                       float r_ray_origin[3])
{
  SculptSession *ss = ob->sculpt;
  mul_v3_m4v3(r_ray_origin, ob->obmat, SCULPT_vertex_co_get(ss, vertex));
  mul_v3_m4v3(r_ray_normal, ob->obmat, ss->cache->sculpt_normal);
  normalize_v3(r_ray_normal);
}

static bool sculpt_scene_project_raycast(SculptSession *ss,
                                         const float ray_normal[3],
                                         const float ray_origin[3],
                                         const bool use_both_directions,
                                         float r_loc[3])
{
  float hit_co[2][3];
  float hit_len_squared[2];
  bool any_hit = false;
  bool hit = false;
  hit = ED_transform_snap_object_project_ray(ss->cache->snap_context,
                                             ss->cache->depsgraph,
                                             ss->cache->vc->v3d,
                                             &(const struct SnapObjectParams){
                                                 .snap_select = SNAP_NOT_ACTIVE,
                                             },
                                             ray_origin,
                                             ray_normal,
                                             NULL,
                                             hit_co[0],
                                             NULL);
  if (hit) {
    hit_len_squared[0] = len_squared_v3v3(hit_co[0], ray_origin);
    any_hit |= hit;
  }
  else {
    hit_len_squared[0] = FLT_MAX;
  }

  if (!use_both_directions) {
    copy_v3_v3(r_loc, hit_co[0]);
    return any_hit;
  }

  float ray_normal_flip[3];
  mul_v3_v3fl(ray_normal_flip, ray_normal, -1.0f);

  hit = ED_transform_snap_object_project_ray(ss->cache->snap_context,
                                             ss->cache->depsgraph,
                                             ss->cache->vc->v3d,
                                             &(const struct SnapObjectParams){
                                                 .snap_select = SNAP_NOT_ACTIVE,
                                             },
                                             ray_origin,
                                             ray_normal_flip,
                                             NULL,
                                             hit_co[1],
                                             NULL);
  if (hit) {
    hit_len_squared[1] = len_squared_v3v3(hit_co[1], ray_origin);
    any_hit |= hit;
  }
  else {
    hit_len_squared[1] = FLT_MAX;
  }

  if (hit_len_squared[0] <= hit_len_squared[1]) {
    copy_v3_v3(r_loc, hit_co[0]);
  }
  else {
    copy_v3_v3(r_loc, hit_co[1]);
  }
  return any_hit;
}

static void do_scene_project_brush_task_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  const float bstrength = clamp_f(ss->cache->bstrength, 0.0f, 1.0f);
  const Brush *brush = data->brush;

  const int thread_id = BLI_task_parallel_thread_id(tls);

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    if (fade == 0.0f) {
      continue;
    }

    float ray_normal[3];
    float ray_origin[3];
    bool use_both_directions = false;
    switch (brush->scene_project_direction_type) {
      case BRUSH_SCENE_PROJECT_DIRECTION_VIEW:
        sculpt_scene_project_view_ray_init(data->ob, vd.vertex, ray_normal, ray_origin);
        break;
      case BRUSH_SCENE_PROJECT_DIRECTION_VERTEX_NORMAL: {
        float normal[3];
        normal_short_to_float_v3(normal, orig_data.no);
        sculpt_scene_project_vertex_normal_ray_init(
            data->ob, vd.vertex, normal, ray_normal, ray_origin);
        use_both_directions = true;
      } break;
      case BRUSH_SCENE_PROJECT_DIRECTION_BRUSH_NORMAL:
        sculpt_scene_project_brush_normal_ray_init(data->ob, vd.vertex, ray_normal, ray_origin);
        use_both_directions = true;
        break;
    }

    float world_space_hit_co[3];
    float hit_co[3];
    const bool hit = sculpt_scene_project_raycast(
        ss, ray_normal, ray_origin, use_both_directions, world_space_hit_co);
    if (!hit) {
      continue;
    }

    mul_v3_m4v3(hit_co, data->ob->imat, world_space_hit_co);

    float disp[3];
    sub_v3_v3v3(disp, hit_co, vd.co);
    mul_v3_fl(disp, fade);
    add_v3_v3(vd.co, disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_scene_project_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_vertex_random_access_ensure(ob->sculpt);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, false, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_scene_project_brush_task_cb_ex, &settings);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Topology Brush
 * \{ */

static void do_topology_slide_task_cb_ex(void *__restrict userdata,
                                         const int n,
                                         const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    orig_data.co,
                                                    sqrtf(test.dist),
                                                    orig_data.no,
                                                    NULL,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);
    float current_disp[3];
    float current_disp_norm[3];
    float final_disp[3] = {0.0f, 0.0f, 0.0f};

    switch (brush->slide_deform_type) {
      case BRUSH_SLIDE_DEFORM_DRAG:
        sub_v3_v3v3(current_disp, ss->cache->location, ss->cache->last_location);
        break;
      case BRUSH_SLIDE_DEFORM_PINCH:
        sub_v3_v3v3(current_disp, ss->cache->location, vd.co);
        break;
      case BRUSH_SLIDE_DEFORM_EXPAND:
        sub_v3_v3v3(current_disp, vd.co, ss->cache->location);
        break;
    }

    normalize_v3_v3(current_disp_norm, current_disp);
    mul_v3_v3fl(current_disp, current_disp_norm, ss->cache->bstrength);

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
      float vertex_disp[3];
      float vertex_disp_norm[3];
      sub_v3_v3v3(vertex_disp, SCULPT_vertex_co_get(ss, ni.vertex), vd.co);
      normalize_v3_v3(vertex_disp_norm, vertex_disp);
      if (dot_v3v3(current_disp_norm, vertex_disp_norm) > 0.0f) {
        madd_v3_v3fl(final_disp, vertex_disp_norm, dot_v3v3(current_disp, vertex_disp));
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    mul_v3_v3fl(proxy[vd.i], final_disp, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_relax_vertex(SculptSession *ss,
                         PBVHVertexIter *vd,
                         float factor,
                         SculptBoundaryType boundary_mask,
                         float *r_final_pos)
{
  float smooth_pos[3];
  float final_disp[3];
  int avg_count = 0;
  int neighbor_count = 0;
  zero_v3(smooth_pos);

  int bset = boundary_mask;

  // forcibly enable if no ss->cache
  if (ss->cache && (ss->cache->brush->flag2 & BRUSH_SMOOTH_PRESERVE_FACE_SETS)) {
    bset |= SCULPT_BOUNDARY_FACE_SET;
  }

  if (SCULPT_vertex_is_corner(ss, vd->vertex, (SculptCornerType)bset)) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  const int is_boundary = SCULPT_vertex_is_boundary(ss, vd->vertex, bset);

  float boundary_tan_a[3];
  float boundary_tan_b[3];
  bool have_boundary_tan_a = false;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd->vertex, ni) {
    neighbor_count++;

    /* When the vertex to relax is boundary, use only connected boundary vertices for the
     * average position. */
    if (is_boundary) {
      if (!SCULPT_vertex_is_boundary(ss, ni.vertex, bset)) {
        continue;
      }
      add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.vertex));
      avg_count++;

      /* Calculate a normal for the constraint plane using the edges of the boundary. */
      float to_neighbor[3];
      sub_v3_v3v3(to_neighbor, SCULPT_vertex_co_get(ss, ni.vertex), vd->co);
      normalize_v3(to_neighbor);

      if (!have_boundary_tan_a) {
        copy_v3_v3(boundary_tan_a, to_neighbor);
        have_boundary_tan_a = true;
      }
      else {
        copy_v3_v3(boundary_tan_b, to_neighbor);
      }
    }
    else {
      add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.vertex));
      avg_count++;
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (avg_count > 0) {
    mul_v3_fl(smooth_pos, 1.0f / avg_count);
  }
  else {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  float plane[4];
  float smooth_closest_plane[3];
  float vno[3];

  if ((is_boundary) && avg_count == 2 && fabsf(dot_v3v3(boundary_tan_a, boundary_tan_b)) < 0.99f) {
    cross_v3_v3v3(vno, boundary_tan_a, boundary_tan_b);
    normalize_v3(vno);
  }
  else {
    SCULPT_vertex_normal_get(ss, vd->vertex, vno);
  }

  if (is_zero_v3(vno)) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  plane_from_point_normal_v3(plane, vd->co, vno);
  closest_to_plane_v3(smooth_closest_plane, plane, smooth_pos);
  sub_v3_v3v3(final_disp, smooth_closest_plane, vd->co);

  mul_v3_fl(final_disp, factor);
  add_v3_v3v3(r_final_pos, vd->co, final_disp);
}

static void do_topology_relax_task_cb_ex(void *__restrict userdata,
                                         const int n,
                                         const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n]);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  const bool do_reproject = SCULPT_need_reproject;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    orig_data.co,
                                                    sqrtf(test.dist),
                                                    orig_data.no,
                                                    NULL,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    float oldco[3], oldno[3];

    copy_v3_v3(oldco, vd.co);
    SCULPT_vertex_normal_get(ss, vd.vertex, oldno);

    SCULPT_relax_vertex(ss, &vd, fade * bstrength, SCULPT_BOUNDARY_DEFAULT, vd.co);

    if (do_reproject) {
      SCULPT_reproject_cdata(ss, vd.vertex, oldco, oldno);
    }

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_slide_relax_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    return;
  }

  BKE_curvemapping_init(brush->curve);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  if (ss->cache->alt_smooth) {
    SCULPT_boundary_info_ensure(ob);
    for (int i = 0; i < 4; i++) {
      BLI_task_parallel_range(0, totnode, &data, do_topology_relax_task_cb_ex, &settings);
    }
  }
  else {
    BLI_task_parallel_range(0, totnode, &data, do_topology_slide_task_cb_ex, &settings);
  }
}

static void do_fairing_brush_tag_store_task_cb_ex(void *__restrict userdata,
                                                  const int n,
                                                  const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  PBVHVertexIter vd;

  const float bstrength = clamp_f(ss->cache->bstrength, 0.0f, 1.0f);
  const Brush *brush = data->brush;

  const int thread_id = BLI_task_parallel_thread_id(tls);
  const int boundflag = SCULPT_BOUNDARY_MESH | SCULPT_BOUNDARY_SHARP;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    if (SCULPT_vertex_is_boundary(ss, vd.vertex, boundflag)) {
      continue;
    }

    float *prefair = SCULPT_temp_cdata_get(vd.vertex, ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                prefair,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    if (fade == 0.0f) {
      continue;
    }

    float *fairing_fade = SCULPT_temp_cdata_get(vd.vertex,
                                                ss->custom_layers[SCULPT_SCL_FAIRING_FADE]);
    bool *fairing_mask = SCULPT_temp_cdata_get(vd.vertex,
                                               ss->custom_layers[SCULPT_SCL_FAIRING_MASK]);

    *fairing_fade = max_ff(fade, *fairing_fade);
    *fairing_mask = true;
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_fairing_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const int totvert = SCULPT_vertex_count_get(ss);

  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    return;
  }

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  if (!ss->custom_layers[SCULPT_SCL_FAIRING_MASK]) {
    // SCULPT_temp_customlayer_ensure(ss, ATTR_DOMAIN_POINT, CD_PROP_BOOL, "fairing_mask");
    // SCULPT_temp_customlayer_ensure(ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT, "fairing_fade");
    // SCULPT_temp_customlayer_ensure(ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "prefairing_co");

    ss->custom_layers[SCULPT_SCL_FAIRING_MASK] = MEM_callocN(sizeof(SculptCustomLayer),
                                                             "ss->Cache->fairing_mask");
    ss->custom_layers[SCULPT_SCL_FAIRING_FADE] = MEM_callocN(sizeof(SculptCustomLayer),
                                                             "ss->Cache->fairing_fade");
    ss->custom_layers[SCULPT_SCL_PREFAIRING_CO] = MEM_callocN(sizeof(SculptCustomLayer),
                                                              "ss->Cache->prefairing_co");

    SculptLayerParams params = {.permanent = false, .simple_array = true};

    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_BOOL,
                                "fairing_mask",
                                ss->custom_layers[SCULPT_SCL_FAIRING_MASK],
                                &params);

    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT,
                                "fairing_fade",
                                ss->custom_layers[SCULPT_SCL_FAIRING_FADE],
                                &params);

    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT3,
                                "prefairing_co",
                                ss->custom_layers[SCULPT_SCL_PREFAIRING_CO],
                                &params);

    SCULPT_update_customdata_refs(ss);
  }

  if (SCULPT_stroke_is_main_symmetry_pass(ss->cache)) {
    for (int i = 0; i < totvert; i++) {
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

      *(bool *)SCULPT_temp_cdata_get(vertex, ss->custom_layers[SCULPT_SCL_FAIRING_MASK]) = false;
      *(float *)SCULPT_temp_cdata_get(vertex, ss->custom_layers[SCULPT_SCL_FAIRING_FADE]) = 0.0f;
      copy_v3_v3(
          (float *)SCULPT_temp_cdata_get(vertex, ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]),
          SCULPT_vertex_co_get(ss, vertex));
    }
  }

  SCULPT_boundary_info_ensure(ob);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_fairing_brush_tag_store_task_cb_ex, &settings);
}

static void do_fairing_brush_displace_task_cb_ex(void *__restrict userdata,
                                                 const int n,
                                                 const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!*(bool *)SCULPT_temp_cdata_get(vd.vertex, ss->custom_layers[SCULPT_SCL_FAIRING_MASK])) {
      continue;
    }
    float disp[3];
    sub_v3_v3v3(disp,
                vd.co,
                SCULPT_temp_cdata_get(vd.vertex, ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]));
    mul_v3_fl(
        disp,
        *(float *)SCULPT_temp_cdata_get(vd.vertex, ss->custom_layers[SCULPT_SCL_FAIRING_FADE]));
    copy_v3_v3(vd.co,
               SCULPT_temp_cdata_get(vd.vertex, ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]));
    add_v3_v3(vd.co, disp);
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_fairing_brush_exec_fairing_for_cache(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  BLI_assert(BKE_pbvh_type(ss->pbvh) != PBVH_GRIDS);
  BLI_assert(ss->cache);
  Brush *brush = BKE_paint_brush(&sd->paint);
  Mesh *mesh = ob->data;

  if (!ss->custom_layers[SCULPT_SCL_FAIRING_MASK]) {
    return;
  }

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MVert *mvert = SCULPT_mesh_deformed_mverts_get(ss);
      BKE_mesh_prefair_and_fair_vertices(mesh,
                                         mvert,
                                         ss->custom_layers[SCULPT_SCL_FAIRING_MASK]->data,
                                         MESH_FAIRING_DEPTH_TANGENCY);
    } break;
    case PBVH_BMESH: {
      // note that we allocated fairing_mask.data in simple array mode
      BKE_bmesh_prefair_and_fair_vertices(
          ss->bm, ss->custom_layers[SCULPT_SCL_FAIRING_MASK]->data, MESH_FAIRING_DEPTH_TANGENCY);
    } break;
    case PBVH_GRIDS:
      BLI_assert(false);
  }

  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_fairing_brush_displace_task_cb_ex, &settings);
  MEM_freeN(nodes);
}

/** \} */

/** \name Sculpt Multires Displacement Smear Brush
 * \{ */

static void do_displacement_smear_brush_task_cb_ex(void *__restrict userdata,
                                                   const int n,
                                                   const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = clamp_f(ss->cache->bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    float current_disp[3];
    float current_disp_norm[3];
    float interp_limit_surface_disp[3];

    copy_v3_v3(interp_limit_surface_disp, ss->cache->prev_displacement[vd.index]);

    switch (brush->smear_deform_type) {
      case BRUSH_SMEAR_DEFORM_DRAG:
        sub_v3_v3v3(current_disp, ss->cache->location, ss->cache->last_location);
        break;
      case BRUSH_SMEAR_DEFORM_PINCH:
        sub_v3_v3v3(current_disp, ss->cache->location, vd.co);
        break;
      case BRUSH_SMEAR_DEFORM_EXPAND:
        sub_v3_v3v3(current_disp, vd.co, ss->cache->location);
        break;
    }

    normalize_v3_v3(current_disp_norm, current_disp);
    mul_v3_v3fl(current_disp, current_disp_norm, ss->cache->bstrength);

    float weights_accum = 1.0f;

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
      float vertex_disp[3];
      float vertex_disp_norm[3];
      float neighbor_limit_co[3];
      SCULPT_vertex_limit_surface_get(ss, ni.vertex, neighbor_limit_co);
      sub_v3_v3v3(vertex_disp,
                  ss->cache->limit_surface_co[ni.index],
                  ss->cache->limit_surface_co[vd.index]);
      const float *neighbor_limit_surface_disp = ss->cache->prev_displacement[ni.index];
      normalize_v3_v3(vertex_disp_norm, vertex_disp);

      if (dot_v3v3(current_disp_norm, vertex_disp_norm) >= 0.0f) {
        continue;
      }

      const float disp_interp = clamp_f(
          -dot_v3v3(current_disp_norm, vertex_disp_norm), 0.0f, 1.0f);
      madd_v3_v3fl(interp_limit_surface_disp, neighbor_limit_surface_disp, disp_interp);
      weights_accum += disp_interp;
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    mul_v3_fl(interp_limit_surface_disp, 1.0f / weights_accum);

    float new_co[3];
    add_v3_v3v3(new_co, ss->cache->limit_surface_co[vd.index], interp_limit_surface_disp);
    interp_v3_v3v3(vd.co, vd.co, new_co, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_displacement_smear_store_prev_disp_task_cb_ex(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    sub_v3_v3v3(ss->cache->prev_displacement[vd.index],
                SCULPT_vertex_co_get(ss, vd.vertex),
                ss->cache->limit_surface_co[vd.index]);
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_displacement_smear_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;

  BKE_curvemapping_init(brush->curve);
  SCULPT_vertex_random_access_ensure(ss);

  const int totvert = SCULPT_vertex_count_get(ss);
  if (!ss->cache->prev_displacement) {
    ss->cache->prev_displacement = MEM_malloc_arrayN(
        totvert, sizeof(float[3]), "prev displacement");
    ss->cache->limit_surface_co = MEM_malloc_arrayN(totvert, sizeof(float[3]), "limit surface co");

    for (int i = 0; i < totvert; i++) {
      SculptVertRef vref = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

      SCULPT_vertex_limit_surface_get(ss, vref, ss->cache->limit_surface_co[i]);
      sub_v3_v3v3(ss->cache->prev_displacement[i],
                  SCULPT_vertex_co_get(ss, vref),
                  ss->cache->limit_surface_co[i]);
    }
  }
  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(
      0, totnode, &data, do_displacement_smear_store_prev_disp_task_cb_ex, &settings);
  BLI_task_parallel_range(0, totnode, &data, do_displacement_smear_brush_task_cb_ex, &settings);
}

/** \} */

static void do_draw_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    /* Offset vertex. */
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    mul_v3_v3fl(proxy[vd.i], offset, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_draw_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
#if 0
  if (BKE_pbvh_type(ob->sculpt->pbvh) == PBVH_BMESH) {
    void cxx_do_draw_brush(Sculpt * sd, Object * ob, PBVHNode * *nodes, int totnode);

    cxx_do_draw_brush(sd, ob, nodes, totnode);
    return;
  }
#endif

  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];
  const float bstrength = ss->cache->bstrength;

  /* Offset with as much as possible factored in already. */
  float effective_normal[3];
  SCULPT_tilt_effective_normal_get(ss, brush, effective_normal);
  mul_v3_v3fl(offset, effective_normal, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* XXX: this shouldn't be necessary, but sculpting crashes in blender2.8 otherwise
   * initialize before threads so they can do curve mapping. */
  BKE_curvemapping_init(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .offset = offset,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_draw_brush_task_cb_ex, &settings);
}

static void do_topology_rake_bmesh_task_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  PBVHNode *node = data->nodes[n];

  bool do_reproject = SCULPT_need_reproject(ss);

  float direction[3];
  copy_v3_v3(direction, ss->cache->grab_delta_symmetry);

  float tmp[3];
  mul_v3_v3fl(
      tmp, ss->cache->sculpt_normal_symm, dot_v3v3(ss->cache->sculpt_normal_symm, direction));
  sub_v3_v3(direction, tmp);
  normalize_v3(direction);

  /* Cancel if there's no grab data. */
  if (is_zero_v3(direction)) {
    return;
  }

  /*take square root of strength to get stronger behavior at
    lower values, to match previous behavior*/
  const float bstrength = sqrtf(clamp_f(data->strength, 0.0f, 1.0f));

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  const bool use_curvature = data->use_curvature;
  int check_fsets = ss->cache->brush->flag2 & BRUSH_SMOOTH_PRESERVE_FACE_SETS;
  check_fsets = check_fsets ? SCULPT_BOUNDARY_FACE_SET : 0;

  if (use_curvature) {
    SCULPT_curvature_begin(ss, node, false);
  }

  const bool weighted = ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;
  if (weighted || ss->cache->brush->boundary_smooth_factor > 0.0f) {
    BKE_pbvh_check_tri_areas(ss->pbvh, data->nodes[n]);
  }

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

/* ignore boundary verts
  might want to call normal smooth with
  rake's projection in this case, I'm not entirely sure
  - joeedh
*/
#if 0
    if (have_bmesh) {
      BMVert *v = (BMVert *)vd.vertex.i;
      MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v);

       if (mv->flag & (SCULPTVERT_BOUNDARY | SCULPTVERT_FSET_BOUNDARY)) {
        continue;
      }
    }
#endif

    float direction2[3];
    const float fade =
        bstrength *
        SCULPT_brush_strength_factor(
            ss, brush, vd.co, sqrtf(test.dist), vd.no, vd.fno, *vd.mask, vd.vertex, thread_id);

    float avg[3], val[3];

    if (use_curvature) {
      SCULPT_curvature_dir_get(ss, vd.vertex, direction2, false);
    }
    else {
      copy_v3_v3(direction2, direction);
    }

#if 0
    if (SCULPT_vertex_is_boundary(
            ss, vd.vertex, SCULPT_BOUNDARY_SHARP | SCULPT_BOUNDARY_MESH | check_fsets)) {
      continue;
    }

    MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, vd.bm_vert);
    if (check_fsets && (mv->flag & (SCULPTVERT_FSET_CORNER))) {
      continue;
    }

    if (mv->flag & (SCULPTVERT_CORNER | SCULPTVERT_SHARP_CORNER)) {
      continue;
    }
#endif

    // check origdata to be sure we don't mess it up
    SCULPT_vertex_check_origdata(ss, vd.vertex);

    float *co = vd.co;

    float oldco[3];
    float oldno[3];

    copy_v3_v3(oldco, co);
    SCULPT_vertex_normal_get(ss, vd.vertex, oldno);

    SCULPT_bmesh_four_neighbor_average(ss,
                                       avg,
                                       direction2,
                                       vd.bm_vert,
                                       data->rake_projection,
                                       check_fsets,
                                       data->cd_temp,
                                       data->cd_sculpt_vert,
                                       0);

    sub_v3_v3v3(val, avg, co);

    float tan[3];
    copy_v3_v3(tan, val);
    madd_v3_v3fl(tan, vd.bm_vert->no, -dot_v3v3(tan, vd.bm_vert->no));

    MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, vd.bm_vert);
    madd_v3_v3v3fl(mv->origco, mv->origco, tan, fade * 0.5);

    madd_v3_v3v3fl(val, co, val, fade);
    SCULPT_clip(sd, ss, co, val);

    if (do_reproject) {
      SCULPT_reproject_cdata(ss, vd.vertex, oldco, oldno);
    }

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_normals_update(data->nodes[n]);
}

void SCULPT_bmesh_topology_rake(Sculpt *sd,
                                Object *ob,
                                PBVHNode **nodes,
                                const int totnode,
                                float bstrength,
                                bool needs_origco)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  const float strength = bstrength;  // clamp_f(bstrength, 0.0f, 1.0f);

  Brush local_brush;

  // vector4, nto color
  SCULPT_dyntopo_ensure_templayer(ss, CD_PROP_COLOR, "_rake_temp", false);
  int cd_temp = SCULPT_dyntopo_get_templayer(ss, CD_PROP_COLOR, "_rake_temp");

#ifdef SCULPT_DIAGONAL_EDGE_MARKS
  // reset edge flags, single threaded
  for (int i = 0; i < totnode; i++) {
    PBVHNode *node = nodes[i];
    PBVHVertexIter vd;

    SculptBrushTest test;
    SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
        ss, &test, brush->falloff_shape);

    BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
      if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
        continue;
      }

      BMVert *v = vd.bm_vert;
      BMEdge *e = v->e;

      if (!e) {
        continue;
      }

      do {
        e->head.hflag |= BM_ELEM_DRAW;
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
    }
    BKE_pbvh_vertex_iter_end;
  }
#endif

  if (SCULPT_stroke_is_first_brush_step(ss->cache) &&
      (ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT)) {
    BKE_pbvh_update_all_tri_areas(ss->pbvh);
  }

  if (brush->flag2 & BRUSH_TOPOLOGY_RAKE_IGNORE_BRUSH_FALLOFF) {
    local_brush = *brush;
    brush = &local_brush;

    brush->curve_preset = BRUSH_CURVE_SMOOTH;

    /*note that brush hardness is calculated from ss->cache->paint_brush,
      we can't override it by changing the brush here.
      this seems desirably though?*/
  }
  /* Iterations increase both strength and quality. */
  const int iterations = 1 + bstrength * 1.5f;

  int iteration;
  const int count = iterations * strength + 1;
  const float factor = iterations * strength / count * 0.25;

  for (iteration = 0; iteration <= count; iteration++) {

    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
        .strength = factor,
        .cd_temp = cd_temp,
        .use_curvature = SCULPT_get_int(ss, topology_rake_mode, sd, brush),
        .cd_sculpt_vert = ss->cd_sculpt_vert,
        .rake_projection = brush->topology_rake_projection,
        .do_origco = needs_origco};
    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, totnode);

    BLI_task_parallel_range(0, totnode, &data, do_topology_rake_bmesh_task_cb_ex, &settings);

    /* don't update normals just yet */
    // BKE_pbvh_update_normals(ss->pbvh, ss->subdiv_ccg);
  }

  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_update_tri_area(nodes[i]);
  }
}

static void do_mask_brush_draw_task_cb_ex(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    const float fade = SCULPT_brush_strength_factor(
        ss, brush, vd.co, sqrtf(test.dist), vd.no, vd.fno, 0.0f, vd.vertex, thread_id);

    if (bstrength > 0.0f) {
      (*vd.mask) += fade * bstrength * (1.0f - *vd.mask);
    }
    else {
      (*vd.mask) += fade * bstrength * (*vd.mask);
    }
    *vd.mask = clamp_f(*vd.mask, 0.0f, 1.0f);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void do_mask_brush_draw(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_mask_brush_draw_task_cb_ex, &settings);
}

void SCULPT_do_mask_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  switch ((BrushMaskTool)brush->mask_tool) {
    case BRUSH_MASK_DRAW:
      do_mask_brush_draw(sd, ob, nodes, totnode);
      break;
    case BRUSH_MASK_SMOOTH:
      SCULPT_smooth(sd, ob, nodes, totnode, ss->cache->bstrength, true, 0.0f, false);
      break;
  }
}

/* -------------------------------------------------------------------- */
/** \name Sculpt Multires Displacement Eraser Brush
 * \{ */

static void do_displacement_eraser_brush_task_cb_ex(void *__restrict userdata,
                                                    const int n,
                                                    const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = clamp_f(ss->cache->bstrength, 0.0f, 1.0f);

  float(*proxy)[3] = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    float limit_co[3];
    float disp[3];
    SCULPT_vertex_limit_surface_get(ss, vd.vertex, limit_co);
    sub_v3_v3v3(disp, limit_co, vd.co);
    mul_v3_v3fl(proxy[vd.i], disp, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_do_displacement_eraser_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  BKE_curvemapping_init(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_displacement_eraser_brush_task_cb_ex, &settings);
}

/** \} */
