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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

#include "paint_intern.h"
#include "sculpt_intern.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

typedef struct MultiplaneScrapeSampleData {
  float area_cos[2][3];
  float area_nos[2][3];
  int area_count[2];
} MultiplaneScrapeSampleData;

static void calc_multiplane_scrape_surface_task_cb(void *__restrict userdata,
                                                   const int n,
                                                   const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  MultiplaneScrapeSampleData *mssd = tls->userdata_chunk;
  float(*mat)[4] = data->mat;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  /* Apply the brush normal radius to the test before sampling. */
  float test_radius = sqrtf(test.radius_squared);
  test_radius *= brush->normal_radius_factor;
  test.radius_squared = test_radius * test_radius;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {

    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      float local_co[3];
      float normal[3];
      if (vd.no) {
        normal_short_to_float_v3(normal, vd.no);
      }
      else {
        copy_v3_v3(normal, vd.fno);
      }
      mul_v3_m4v3(local_co, mat, vd.co);
      /* Use the brush falloff to weight the sampled normals. */
      const float fade = SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      thread_id);

      /* Sample the normal and area of the +X and -X axis individually. */
      if (local_co[0] > 0.0f) {
        madd_v3_v3fl(mssd->area_nos[0], normal, fade);
        add_v3_v3(mssd->area_cos[0], vd.co);
        mssd->area_count[0]++;
      }
      else {
        madd_v3_v3fl(mssd->area_nos[1], normal, fade);
        add_v3_v3(mssd->area_cos[1], vd.co);
        mssd->area_count[1]++;
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void calc_multiplane_scrape_surface_reduce(const void *__restrict UNUSED(userdata),
                                                  void *__restrict chunk_join,
                                                  void *__restrict chunk)
{
  MultiplaneScrapeSampleData *join = chunk_join;
  MultiplaneScrapeSampleData *mssd = chunk;

  add_v3_v3(join->area_cos[0], mssd->area_cos[0]);
  add_v3_v3(join->area_cos[1], mssd->area_cos[1]);

  add_v3_v3(join->area_nos[0], mssd->area_nos[0]);
  add_v3_v3(join->area_nos[1], mssd->area_nos[1]);

  join->area_count[0] += mssd->area_count[0];
  join->area_count[1] += mssd->area_count[1];
}

static void do_multiplane_scrape_brush_task_cb_ex(void *__restrict userdata,
                                                  const int n,
                                                  const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;
  float(*scrape_planes)[4] = data->multiplane_scrape_planes;

  float angle = data->multiplane_scrape_angle;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = fabsf(ss->cache->bstrength);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {

    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      float local_co[3];
      bool deform = false;

      mul_v3_m4v3(local_co, mat, vd.co);

      if (local_co[0] > 0.0f) {
        deform = !SCULPT_plane_point_side(vd.co, scrape_planes[0]);
      }
      else {
        deform = !SCULPT_plane_point_side(vd.co, scrape_planes[1]);
      }

      if (angle < 0.0f) {
        deform = true;
      }

      if (deform) {
        float intr[3];
        float val[3];

        if (local_co[0] > 0.0f) {
          closest_to_plane_normalized_v3(intr, scrape_planes[0], vd.co);
        }
        else {
          closest_to_plane_normalized_v3(intr, scrape_planes[1], vd.co);
        }

        sub_v3_v3v3(val, intr, vd.co);
        if (SCULPT_plane_trim(ss->cache, brush, val)) {
          /* Deform the local space along the Y axis to avoid artifacts on curved strokes. */
          /* This produces a not round brush tip. */
          local_co[1] *= 2.0f;
          const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                      brush,
                                                                      vd.co,
                                                                      len_v3(local_co),
                                                                      vd.no,
                                                                      vd.fno,
                                                                      vd.mask ? *vd.mask : 0.0f,
                                                                      vd.index,
                                                                      thread_id);

          mul_v3_v3fl(proxy[vd.i], val, fade);

          if (vd.mvert) {
            vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
          }
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

/* Public functions. */

/* Main Brush Function. */
void SCULPT_do_multiplane_scrape_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const bool flip = (ss->cache->bstrength < 0.0f);
  const float radius = flip ? -ss->cache->radius : ss->cache->radius;
  const float offset = SCULPT_brush_plane_offset_get(sd, ss);
  const float displace = -radius * offset;

  /* The sculpt-plane normal (whatever its set to) */
  float area_no_sp[3];

  /* Geometry normal. */
  float area_no[3];
  float area_co[3];

  float temp[3];
  float mat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no_sp, area_co);

  if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
    SCULPT_calc_area_normal(sd, ob, nodes, totnode, area_no);
  }
  else {
    copy_v3_v3(area_no, area_no_sp);
  }

  /* Delay the first daub because grab delta is not setup. */
  if (ss->cache->first_time) {
    ss->cache->multiplane_scrape_angle = 0.0f;
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  mul_v3_v3v3(temp, area_no_sp, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  /* Init brush local space matrix. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], ss->cache->location);
  mat[3][3] = 1.0f;
  normalize_m4(mat);
  invert_m4(mat);

  /* Update matrix for the cursor preview. */
  if (ss->cache->mirror_symmetry_pass == 0 && ss->cache->radial_symmetry_pass == 0) {
    copy_m4_m4(ss->cache->stroke_local_mat, mat);
  }

  /* Dynamic mode. */

  if (brush->flag2 & BRUSH_MULTIPLANE_SCRAPE_DYNAMIC) {
    /* Sample the individual normal and area center of the two areas at both sides of the cursor.
     */
    SculptThreadedTaskData sample_data = {
        .sd = NULL,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
        .totnode = totnode,
        .mat = mat,
    };

    MultiplaneScrapeSampleData mssd = {{{0}}};

    TaskParallelSettings sample_settings;
    BKE_pbvh_parallel_range_settings(&sample_settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
    sample_settings.func_reduce = calc_multiplane_scrape_surface_reduce;
    sample_settings.userdata_chunk = &mssd;
    sample_settings.userdata_chunk_size = sizeof(MultiplaneScrapeSampleData);

    BLI_task_parallel_range(
        0, totnode, &sample_data, calc_multiplane_scrape_surface_task_cb, &sample_settings);

    float sampled_plane_normals[2][3];
    float sampled_plane_co[2][3];
    float sampled_cv[2][3];
    float mid_co[3];

    /* Use the area center of both planes to detect if we are sculpting along a concave or convex
     * edge. */
    mul_v3_v3fl(sampled_plane_co[0], mssd.area_cos[0], 1.0f / (float)mssd.area_count[0]);
    mul_v3_v3fl(sampled_plane_co[1], mssd.area_cos[1], 1.0f / (float)mssd.area_count[1]);
    mid_v3_v3v3(mid_co, sampled_plane_co[0], sampled_plane_co[1]);

    /* Calculate the scrape planes angle based on the sampled normals. */
    mul_v3_v3fl(sampled_plane_normals[0], mssd.area_nos[0], 1.0f / (float)mssd.area_count[0]);
    mul_v3_v3fl(sampled_plane_normals[1], mssd.area_nos[1], 1.0f / (float)mssd.area_count[1]);
    normalize_v3(sampled_plane_normals[0]);
    normalize_v3(sampled_plane_normals[1]);

    float sampled_angle = angle_v3v3(sampled_plane_normals[0], sampled_plane_normals[1]);
    copy_v3_v3(sampled_cv[0], area_no);
    sub_v3_v3v3(sampled_cv[1], ss->cache->location, mid_co);

    sampled_angle += DEG2RADF(brush->multiplane_scrape_angle) * ss->cache->pressure;

    /* Invert the angle if we are sculpting along a concave edge. */
    if (dot_v3v3(sampled_cv[0], sampled_cv[1]) < 0.0f) {
      sampled_angle = -sampled_angle;
    }

    /* In dynamic mode, set the angle to 0 when inverting the brush, so you can trim plane
     * surfaces without changing the brush. */
    if (flip) {
      sampled_angle = 0.0f;
    }
    else {
      copy_v3_v3(area_co, ss->cache->location);
    }

    /* Interpolate between the previous and new sampled angles to avoid artifacts when if angle
     * difference between two samples is too big. */
    ss->cache->multiplane_scrape_angle = interpf(
        RAD2DEGF(sampled_angle), ss->cache->multiplane_scrape_angle, 0.2f);
  }
  else {

    /* Standard mode: Scrape with the brush property fixed angle. */
    copy_v3_v3(area_co, ss->cache->location);
    ss->cache->multiplane_scrape_angle = brush->multiplane_scrape_angle;
    if (flip) {
      ss->cache->multiplane_scrape_angle *= -1.0f;
    }
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .mat = mat,
      .multiplane_scrape_angle = ss->cache->multiplane_scrape_angle,
  };

  /* Calculate the final left and right scrape planes. */
  float plane_no[3];
  float plane_no_rot[3];
  float y_axis[3] = {0.0f, 1.0f, 0.0f};
  float mat_inv[4][4];
  invert_m4_m4(mat_inv, mat);

  mul_v3_mat3_m4v3(plane_no, mat, area_no);
  rotate_v3_v3v3fl(
      plane_no_rot, plane_no, y_axis, DEG2RADF(-ss->cache->multiplane_scrape_angle * 0.5f));
  mul_v3_mat3_m4v3(plane_no, mat_inv, plane_no_rot);
  normalize_v3(plane_no);
  plane_from_point_normal_v3(data.multiplane_scrape_planes[1], area_co, plane_no);

  mul_v3_mat3_m4v3(plane_no, mat, area_no);
  rotate_v3_v3v3fl(
      plane_no_rot, plane_no, y_axis, DEG2RADF(ss->cache->multiplane_scrape_angle * 0.5f));
  mul_v3_mat3_m4v3(plane_no, mat_inv, plane_no_rot);
  normalize_v3(plane_no);
  plane_from_point_normal_v3(data.multiplane_scrape_planes[0], area_co, plane_no);

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BLI_task_parallel_range(0, totnode, &data, do_multiplane_scrape_brush_task_cb_ex, &settings);
}

void SCULPT_multiplane_scrape_preview_draw(const uint gpuattr,
                                           SculptSession *ss,
                                           const float outline_col[3],
                                           const float outline_alpha)
{
  float local_mat_inv[4][4];
  invert_m4_m4(local_mat_inv, ss->cache->stroke_local_mat);
  GPU_matrix_mul(local_mat_inv);
  float angle = ss->cache->multiplane_scrape_angle;
  if (ss->cache->pen_flip || ss->cache->invert) {
    angle = -angle;
  }

  float offset = ss->cache->radius * 0.25f;

  float p[3] = {0.0f, 0.0f, ss->cache->radius};
  float y_axis[3] = {0.0f, 1.0f, 0.0f};
  float p_l[3];
  float p_r[3];
  float area_center[3] = {0.0f, 0.0f, 0.0f};
  rotate_v3_v3v3fl(p_r, p, y_axis, DEG2RADF((angle + 180) * 0.5f));
  rotate_v3_v3v3fl(p_l, p, y_axis, DEG2RADF(-(angle + 180) * 0.5f));

  immBegin(GPU_PRIM_LINES, 14);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] + offset, p_r[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] + offset, p_l[2]);

  immVertex3f(gpuattr, area_center[0], area_center[1] - offset, area_center[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] - offset, p_r[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] - offset, area_center[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] - offset, p_l[2]);

  immVertex3f(gpuattr, area_center[0], area_center[1] - offset, area_center[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);

  immVertex3f(gpuattr, p_r[0], p_r[1] - offset, p_r[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] + offset, p_r[2]);

  immVertex3f(gpuattr, p_l[0], p_l[1] - offset, p_l[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] + offset, p_l[2]);

  immEnd();

  immUniformColor3fvAlpha(outline_col, outline_alpha * 0.1f);
  immBegin(GPU_PRIM_TRIS, 12);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] + offset, p_r[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] - offset, p_r[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] - offset, area_center[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] - offset, p_r[2]);

  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] + offset, p_l[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] - offset, p_l[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] - offset, area_center[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] - offset, p_l[2]);

  immEnd();
}
