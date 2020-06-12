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
#include "BLI_dial_2d.h"
#include "BLI_ghash.h"
#include "BLI_gsqueue.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_kelvinlet.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
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

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CLOTH_LENGTH_CONSTRAINTS_BLOCK 100000
#define CLOTH_SIMULATION_ITERATIONS 5
#define CLOTH_MAX_CONSTRAINTS_PER_VERTEX 1024
#define CLOTH_SIMULATION_TIME_STEP 0.01f

static void cloth_brush_constraint_key_get(int r_key[2], const int v1, const int v2)
{
  if (v1 < v2) {
    r_key[0] = v1;
    r_key[1] = v2;
  }
  else {
    r_key[0] = v2;
    r_key[1] = v1;
  }
}

static bool cloth_brush_sim_has_length_constraint(SculptClothSimulation *cloth_sim,
                                                  const int v1,
                                                  const int v2)
{
  int constraint[2];
  cloth_brush_constraint_key_get(constraint, v1, v2);
  return BLI_gset_haskey(cloth_sim->created_length_constraints, constraint);
}

static void cloth_brush_add_length_constraint(SculptSession *ss,
                                              SculptClothSimulation *cloth_sim,
                                              const int v1,
                                              const int v2)
{
  cloth_sim->length_constraints[cloth_sim->tot_length_constraints].v1 = v1;
  cloth_sim->length_constraints[cloth_sim->tot_length_constraints].v2 = v2;
  cloth_sim->length_constraints[cloth_sim->tot_length_constraints].length = len_v3v3(
      SCULPT_vertex_co_get(ss, v1), SCULPT_vertex_co_get(ss, v2));

  cloth_sim->tot_length_constraints++;

  /* Reallocation if the array capacity is exceeded. */
  if (cloth_sim->tot_length_constraints >= cloth_sim->capacity_length_constraints) {
    cloth_sim->capacity_length_constraints += CLOTH_LENGTH_CONSTRAINTS_BLOCK;
    cloth_sim->length_constraints = MEM_reallocN_id(cloth_sim->length_constraints,
                                                    cloth_sim->capacity_length_constraints *
                                                        sizeof(SculptClothLengthConstraint),
                                                    "length constraints");
  }

  /* Add the constraint to the GSet to avoid creating it again. */
  int constraint[2];
  cloth_brush_constraint_key_get(constraint, v1, v2);
  BLI_gset_add(cloth_sim->created_length_constraints, constraint);
}

static void do_cloth_brush_build_constraints_task_cb_ex(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (len_squared_v3v3(vd.co, data->cloth_sim_initial_location) <
        data->cloth_sim_radius * data->cloth_sim_radius) {

      SculptVertexNeighborIter ni;
      int build_indices[CLOTH_MAX_CONSTRAINTS_PER_VERTEX];
      int tot_indices = 0;
      build_indices[tot_indices] = vd.index;
      tot_indices++;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.index, ni) {
        build_indices[tot_indices] = ni.index;
        tot_indices++;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

      /* As we don't know the order of the neighbor vertices, we create all possible combinations
       * between the neighbor and the original vertex as length constraints. */
      /* This results on a pattern that contains structural, shear and bending constraints for all
       * vertices, but constraints are repeated taking more memory than necessary. */

      for (int c_i = 0; c_i < tot_indices; c_i++) {
        for (int c_j = 0; c_j < tot_indices; c_j++) {
          if (c_i != c_j && !cloth_brush_sim_has_length_constraint(
                                data->cloth_sim, build_indices[c_i], build_indices[c_j])) {
            cloth_brush_add_length_constraint(
                ss, data->cloth_sim, build_indices[c_i], build_indices[c_j]);
          }
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static float cloth_brush_simulation_falloff_get(const Brush *brush,
                                                const float radius,
                                                const float location[3],
                                                const float co[3])
{
  const float distance = len_v3v3(location, co);
  const float limit = radius + (radius * brush->cloth_sim_limit);
  const float falloff = radius + (radius * brush->cloth_sim_limit * brush->cloth_sim_falloff);

  if (distance > limit) {
    /* Outiside the limits. */
    return 0.0f;
  }
  else if (distance < falloff) {
    /* Before the falloff area. */
    return 1.0f;
  }
  else {
    /* Do a smoothstep transition inside the falloff area. */
    float p = 1.0f - ((distance - falloff) / (limit - falloff));
    return 3.0f * p * p - 2.0f * p * p * p;
  }
}

static void cloth_brush_apply_force_to_vertex(SculptSession *UNUSED(ss),
                                              SculptClothSimulation *cloth_sim,
                                              const float force[3],
                                              const int vertex_index)
{
  madd_v3_v3fl(cloth_sim->acceleration[vertex_index], force, 1.0f / cloth_sim->mass);
}

static void do_cloth_brush_apply_forces_task_cb_ex(void *__restrict userdata,
                                                   const int n,
                                                   const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  SculptClothSimulation *cloth_sim = ss->cache->cloth_sim;
  const float *offset = data->offset;
  const float *grab_delta = data->grab_delta;
  float(*imat)[4] = data->mat;

  const bool use_falloff_plane = !SCULPT_is_cloth_deform_brush(brush) &&
                                 brush->cloth_force_falloff_type ==
                                     BRUSH_CLOTH_FORCE_FALLOFF_PLANE;

  PBVHVertexIter vd;
  const float bstrength = ss->cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  /* For Pich Perpendicular Deform Type. */
  float x_object_space[3];
  float z_object_space[3];
  if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR) {
    normalize_v3_v3(x_object_space, imat[0]);
    normalize_v3_v3(z_object_space, imat[2]);
  }

  /* For Plane Force Falloff. */
  float deform_plane[4];
  float plane_normal[3];
  if (use_falloff_plane) {
    normalize_v3_v3(plane_normal, grab_delta);
    plane_from_point_normal_v3(deform_plane, data->area_co, plane_normal);
  }

  /* Gravity */
  float gravity[3] = {0.0f};
  if (ss->cache->supports_gravity) {
    madd_v3_v3fl(
        gravity, ss->cache->gravity_direction, -ss->cache->radius * data->sd->gravity_factor);
  }

  /* Original data for deform brushes. */
  SculptOrigVertData orig_data;
  if (SCULPT_is_cloth_deform_brush(brush)) {
    SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);
  }

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    float force[3];
    const float sim_factor = cloth_brush_simulation_falloff_get(
        brush, ss->cache->radius, ss->cache->initial_location, cloth_sim->init_pos[vd.index]);

    float current_vertex_location[3];
    if (SCULPT_is_cloth_deform_brush(brush)) {
      SCULPT_orig_vert_data_update(&orig_data, &vd);
      copy_v3_v3(current_vertex_location, orig_data.co);
    }
    else {
      copy_v3_v3(current_vertex_location, vd.co);
    }

    /* When using the plane falloff mode the falloff is not constrained by the brush radius. */
    if (sculpt_brush_test_sq_fn(&test, current_vertex_location) || use_falloff_plane) {

      float dist = sqrtf(test.dist);

      if (use_falloff_plane) {
        dist = dist_to_plane_v3(vd.co, deform_plane);
      }

      const float fade = sim_factor * bstrength *
                         SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      current_vertex_location,
                                                      dist,
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      thread_id);

      float brush_disp[3];
      float normal[3];
      if (vd.no) {
        normal_short_to_float_v3(normal, vd.no);
      }
      else {
        copy_v3_v3(normal, vd.fno);
      }

      switch (brush->cloth_deform_type) {
        case BRUSH_CLOTH_DEFORM_DRAG:
          sub_v3_v3v3(brush_disp, ss->cache->location, ss->cache->last_location);
          normalize_v3(brush_disp);
          mul_v3_v3fl(force, brush_disp, fade);
          break;
        case BRUSH_CLOTH_DEFORM_PUSH:
          /* Invert the fade to push inwards. */
          mul_v3_v3fl(force, offset, -fade);
          break;
        case BRUSH_CLOTH_DEFORM_GRAB:
          /* Grab writes the positions in the simulation directly without applying forces. */
          madd_v3_v3v3fl(
              cloth_sim->pos[vd.index], orig_data.co, ss->cache->grab_delta_symmetry, fade);
          zero_v3(force);
          break;
        case BRUSH_CLOTH_DEFORM_PINCH_POINT:
          if (use_falloff_plane) {
            float distance = dist_signed_to_plane_v3(vd.co, deform_plane);
            copy_v3_v3(brush_disp, plane_normal);
            mul_v3_fl(brush_disp, -distance);
          }
          else {
            sub_v3_v3v3(brush_disp, ss->cache->location, vd.co);
          }
          normalize_v3(brush_disp);
          mul_v3_v3fl(force, brush_disp, fade);
          break;
        case BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR: {
          float disp_center[3];
          float x_disp[3];
          float z_disp[3];
          sub_v3_v3v3(disp_center, ss->cache->location, vd.co);
          normalize_v3(disp_center);
          mul_v3_v3fl(x_disp, x_object_space, dot_v3v3(disp_center, x_object_space));
          mul_v3_v3fl(z_disp, z_object_space, dot_v3v3(disp_center, z_object_space));
          add_v3_v3v3(disp_center, x_disp, z_disp);
          mul_v3_v3fl(force, disp_center, fade);
        } break;
        case BRUSH_CLOTH_DEFORM_INFLATE:
          mul_v3_v3fl(force, normal, fade);
          break;
        case BRUSH_CLOTH_DEFORM_EXPAND:
          cloth_sim->length_constraint_tweak[vd.index] += fade * 0.1f;
          zero_v3(force);
          break;
      }

      madd_v3_v3fl(force, gravity, fade);

      cloth_brush_apply_force_to_vertex(ss, ss->cache->cloth_sim, force, vd.index);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static SculptClothSimulation *cloth_brush_simulation_create(SculptSession *ss,
                                                            const float cloth_mass,
                                                            const float cloth_damping)
{
  const int totverts = SCULPT_vertex_count_get(ss);
  SculptClothSimulation *cloth_sim;

  cloth_sim = MEM_callocN(sizeof(SculptClothSimulation), "cloth constraints");

  cloth_sim->length_constraints = MEM_callocN(sizeof(SculptClothLengthConstraint) *
                                                  CLOTH_LENGTH_CONSTRAINTS_BLOCK,
                                              "cloth length constraints");
  cloth_sim->capacity_length_constraints = CLOTH_LENGTH_CONSTRAINTS_BLOCK;

  cloth_sim->acceleration = MEM_callocN(sizeof(float) * 3 * totverts, "cloth sim acceleration");
  cloth_sim->pos = MEM_callocN(sizeof(float) * 3 * totverts, "cloth sim pos");
  cloth_sim->prev_pos = MEM_callocN(sizeof(float) * 3 * totverts, "cloth sim prev pos");
  cloth_sim->init_pos = MEM_callocN(sizeof(float) * 3 * totverts, "cloth sim init pos");
  cloth_sim->length_constraint_tweak = MEM_callocN(sizeof(float) * totverts,
                                                   "cloth sim length tweak");

  cloth_sim->mass = cloth_mass;
  cloth_sim->damping = cloth_damping;

  return cloth_sim;
}

static void do_cloth_brush_solve_simulation_task_cb_ex(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  PBVHVertexIter vd;
  SculptClothSimulation *cloth_sim = data->cloth_sim;
  const float time_step = data->cloth_time_step;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    const float sim_factor = ss->cache ? cloth_brush_simulation_falloff_get(
                                             brush,
                                             ss->cache->radius,
                                             ss->cache->initial_location,
                                             cloth_sim->init_pos[vd.index]) :
                                         1.0f;
    if (sim_factor > 0.0f) {
      int i = vd.index;
      float temp[3];
      copy_v3_v3(temp, cloth_sim->pos[i]);

      mul_v3_fl(cloth_sim->acceleration[i], time_step);

      float pos_diff[3];
      sub_v3_v3v3(pos_diff, cloth_sim->pos[i], cloth_sim->prev_pos[i]);
      mul_v3_fl(pos_diff, (1.0f - cloth_sim->damping));

      const float mask_v = (1.0f - (vd.mask ? *vd.mask : 0.0f)) *
                           SCULPT_automasking_factor_get(ss, vd.index);
      madd_v3_v3fl(cloth_sim->pos[i], pos_diff, mask_v);
      madd_v3_v3fl(cloth_sim->pos[i], cloth_sim->acceleration[i], mask_v);

      copy_v3_v3(cloth_sim->prev_pos[i], temp);

      copy_v3_fl(cloth_sim->acceleration[i], 0.0f);

      copy_v3_v3(vd.co, cloth_sim->pos[vd.index]);
      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void cloth_brush_build_nodes_constraints(Sculpt *sd,
                                                Object *ob,
                                                PBVHNode **nodes,
                                                int totnode,
                                                SculptClothSimulation *cloth_sim,
                                                float initial_location[3],
                                                const float radius)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* TODO: Multi-threaded needs to be disabled for this task until implementing the optimization of
   * storing the constraints per node. */
  /* Currently all constrains are added to the same global array which can't be accessed from
   * different threads. */
  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, false, totnode);

  cloth_sim->created_length_constraints = BLI_gset_new(
      BLI_ghashutil_inthash_v2_p, BLI_ghashutil_inthash_v2_cmp, "created length constraints");

  SculptThreadedTaskData build_constraints_data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .cloth_sim = cloth_sim,
      .cloth_sim_initial_location = initial_location,
      .cloth_sim_radius = radius,
  };
  BLI_task_parallel_range(
      0, totnode, &build_constraints_data, do_cloth_brush_build_constraints_task_cb_ex, &settings);

  BLI_gset_free(cloth_sim->created_length_constraints, NULL);
}

static void cloth_brush_satisfy_constraints(SculptSession *ss,
                                            Brush *brush,
                                            SculptClothSimulation *cloth_sim)
{
  for (int constraint_it = 0; constraint_it < CLOTH_SIMULATION_ITERATIONS; constraint_it++) {
    for (int i = 0; i < cloth_sim->tot_length_constraints; i++) {

      const SculptClothLengthConstraint *constraint = &cloth_sim->length_constraints[i];
      const int v1 = constraint->v1;
      const int v2 = constraint->v2;

      float v1_to_v2[3];
      sub_v3_v3v3(v1_to_v2, cloth_sim->pos[v2], cloth_sim->pos[v1]);
      const float current_distance = len_v3(v1_to_v2);
      float correction_vector[3];
      float correction_vector_half[3];

      const float constraint_distance = constraint->length +
                                        (cloth_sim->length_constraint_tweak[v1] * 0.5f) +
                                        (cloth_sim->length_constraint_tweak[v2] * 0.5f);

      if (current_distance > 0.0f) {
        mul_v3_v3fl(correction_vector, v1_to_v2, 1.0f - (constraint_distance / current_distance));
      }
      else {
        copy_v3_v3(correction_vector, v1_to_v2);
      }

      mul_v3_v3fl(correction_vector_half, correction_vector, 0.5f);

      const float mask_v1 = (1.0f - SCULPT_vertex_mask_get(ss, v1)) *
                            SCULPT_automasking_factor_get(ss, v1);
      const float mask_v2 = (1.0f - SCULPT_vertex_mask_get(ss, v2)) *
                            SCULPT_automasking_factor_get(ss, v2);

      const float sim_factor_v1 = ss->cache ? cloth_brush_simulation_falloff_get(
                                                  brush,
                                                  ss->cache->radius,
                                                  ss->cache->initial_location,
                                                  cloth_sim->init_pos[v1]) :
                                              1.0f;
      const float sim_factor_v2 = ss->cache ? cloth_brush_simulation_falloff_get(
                                                  brush,
                                                  ss->cache->radius,
                                                  ss->cache->initial_location,
                                                  cloth_sim->init_pos[v2]) :
                                              1.0f;

      madd_v3_v3fl(cloth_sim->pos[v1], correction_vector_half, 1.0f * mask_v1 * sim_factor_v1);
      madd_v3_v3fl(cloth_sim->pos[v2], correction_vector_half, -1.0f * mask_v2 * sim_factor_v2);
    }
  }
}

static void cloth_brush_do_simulation_step(
    Sculpt *sd, Object *ob, SculptClothSimulation *cloth_sim, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* Update the constraints. */
  cloth_brush_satisfy_constraints(ss, brush, cloth_sim);

  /* Solve the simulation and write the final step to the mesh. */
  SculptThreadedTaskData solve_simulation_data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .cloth_time_step = CLOTH_SIMULATION_TIME_STEP,
      .cloth_sim = cloth_sim,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BLI_task_parallel_range(
      0, totnode, &solve_simulation_data, do_cloth_brush_solve_simulation_task_cb_ex, &settings);
}

static void cloth_brush_apply_brush_foces(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  float grab_delta[3];
  float mat[4][4];
  float area_no[3];
  float area_co[3];
  float imat[4][4];
  float offset[3];

  SculptThreadedTaskData apply_forces_data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
      .mat = imat,
  };

  BKE_curvemapping_initialize(brush->curve);

  /* Init the grab delta. */
  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);
  normalize_v3(grab_delta);

  apply_forces_data.grab_delta = grab_delta;

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  /* Calcuate push offset. */

  if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_PUSH) {
    mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
    mul_v3_v3(offset, ss->cache->scale);
    mul_v3_fl(offset, 2.0f);

    apply_forces_data.offset = offset;
  }

  if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR ||
      brush->cloth_force_falloff_type == BRUSH_CLOTH_FORCE_FALLOFF_PLANE) {
    SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

    /* Init stroke local space matrix. */
    cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
    mat[0][3] = 0.0f;
    cross_v3_v3v3(mat[1], area_no, mat[0]);
    mat[1][3] = 0.0f;
    copy_v3_v3(mat[2], area_no);
    mat[2][3] = 0.0f;
    copy_v3_v3(mat[3], ss->cache->location);
    mat[3][3] = 1.0f;
    normalize_m4(mat);

    apply_forces_data.area_co = area_co;
    apply_forces_data.area_no = area_no;
    apply_forces_data.mat = mat;

    /* Update matrix for the cursor preview. */
    if (ss->cache->mirror_symmetry_pass == 0) {
      copy_m4_m4(ss->cache->stroke_local_mat, mat);
    }
  }

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BLI_task_parallel_range(
      0, totnode, &apply_forces_data, do_cloth_brush_apply_forces_task_cb_ex, &settings);
}

/* Public functions. */

/* Main Brush Function. */
void SCULPT_do_cloth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const int totverts = SCULPT_vertex_count_get(ss);

  /* In the first brush step of each symmetry pass, build the constraints for the vertices in all
   * nodes inside the simulation's limits. */
  /* Brush stroke types that restore the mesh on each brush step also need the cloth sim data to be
   * created on each step. */
  if (ss->cache->first_time || !ss->cache->cloth_sim) {

    /* The simulation structure only needs to be created on the first symmetry pass. */
    if (ss->cache->mirror_symmetry_pass == 0) {
      ss->cache->cloth_sim = cloth_brush_simulation_create(
          ss, brush->cloth_mass, brush->cloth_damping);
      for (int i = 0; i < totverts; i++) {
        copy_v3_v3(ss->cache->cloth_sim->prev_pos[i], SCULPT_vertex_co_get(ss, i));
        copy_v3_v3(ss->cache->cloth_sim->init_pos[i], SCULPT_vertex_co_get(ss, i));
      }
    }

    /* Build the constraints. */
    const float radius = ss->cache->initial_radius;
    const float limit = radius + (radius * brush->cloth_sim_limit);
    cloth_brush_build_nodes_constraints(
        sd, ob, nodes, totnode, ss->cache->cloth_sim, ss->cache->location, limit);

    return;
  }

  /* Store the initial state in the simulation. */
  for (int i = 0; i < totverts; i++) {
    copy_v3_v3(ss->cache->cloth_sim->pos[i], SCULPT_vertex_co_get(ss, i));
  }

  /* Apply forces to the vertices. */
  cloth_brush_apply_brush_foces(sd, ob, nodes, totnode);

  /* Update and write the simulation to the nodes. */
  cloth_brush_do_simulation_step(sd, ob, ss->cache->cloth_sim, nodes, totnode);

  return;
}

void SCULPT_cloth_simulation_free(struct SculptClothSimulation *cloth_sim)
{
  MEM_SAFE_FREE(cloth_sim->pos);
  MEM_SAFE_FREE(cloth_sim->prev_pos);
  MEM_SAFE_FREE(cloth_sim->acceleration);
  MEM_SAFE_FREE(cloth_sim->length_constraints);
  MEM_SAFE_FREE(cloth_sim->length_constraint_tweak);
  MEM_SAFE_FREE(cloth_sim->init_pos);
  MEM_SAFE_FREE(cloth_sim);
}

/* Cursor drawing function. */
void SCULPT_cloth_simulation_limits_draw(const uint gpuattr,
                                         const Brush *brush,
                                         const float obmat[4][4],
                                         const float location[3],
                                         const float normal[3],
                                         const float rds,
                                         const float line_width,
                                         const float outline_col[3],
                                         const float alpha)
{
  float cursor_trans[4][4], cursor_rot[4][4];
  float z_axis[4] = {0.0f, 0.0f, 1.0f, 0.0f};
  float quat[4];
  copy_m4_m4(cursor_trans, obmat);
  translate_m4(cursor_trans, location[0], location[1], location[2]);
  rotation_between_vecs_to_quat(quat, z_axis, normal);
  quat_to_mat4(cursor_rot, quat);
  GPU_matrix_mul(cursor_trans);
  GPU_matrix_mul(cursor_rot);

  GPU_line_width(line_width);
  immUniformColor3fvAlpha(outline_col, alpha * 0.5f);
  imm_draw_circle_dashed_3d(
      gpuattr, 0, 0, rds + (rds * brush->cloth_sim_limit * brush->cloth_sim_falloff), 320);
  immUniformColor3fvAlpha(outline_col, alpha * 0.7f);
  imm_draw_circle_wire_3d(gpuattr, 0, 0, rds + rds * brush->cloth_sim_limit, 80);
}

void SCULPT_cloth_plane_falloff_preview_draw(const uint gpuattr,
                                             SculptSession *ss,
                                             const float outline_col[3],
                                             float outline_alpha)
{
  float local_mat_inv[4][4];
  invert_m4_m4(local_mat_inv, ss->cache->stroke_local_mat);
  GPU_matrix_mul(ss->cache->stroke_local_mat);

  const float dist = ss->cache->radius;
  const float arrow_x = ss->cache->radius * 0.2f;
  const float arrow_y = ss->cache->radius * 0.1f;

  immUniformColor3fvAlpha(outline_col, outline_alpha);
  GPU_line_width(2.0f);
  immBegin(GPU_PRIM_LINES, 2);
  immVertex3f(gpuattr, dist, 0.0f, 0.0f);
  immVertex3f(gpuattr, -dist, 0.0f, 0.0f);
  immEnd();

  immBegin(GPU_PRIM_TRIS, 6);
  immVertex3f(gpuattr, dist, 0.0f, 0.0f);
  immVertex3f(gpuattr, dist - arrow_x, arrow_y, 0.0f);
  immVertex3f(gpuattr, dist - arrow_x, -arrow_y, 0.0f);

  immVertex3f(gpuattr, -dist, 0.0f, 0.0f);
  immVertex3f(gpuattr, -dist + arrow_x, arrow_y, 0.0f);
  immVertex3f(gpuattr, -dist + arrow_x, -arrow_y, 0.0f);

  immEnd();
}

/* Cloth Filter. */

typedef enum eSculpClothFilterType {
  CLOTH_FILTER_GRAVITY,
  CLOTH_FILTER_INFLATE,
  CLOTH_FILTER_EXPAND,
  CLOTH_FILTER_PINCH,
} eSculptClothFilterType;

static EnumPropertyItem prop_cloth_filter_type[] = {
    {CLOTH_FILTER_GRAVITY, "GRAVITY", 0, "Gravity", "Applies gravity to the simulation"},
    {CLOTH_FILTER_INFLATE, "INFLATE", 0, "Inflate", "Inflates the cloth"},
    {CLOTH_FILTER_EXPAND, "EXPAND", 0, "Expand", "Expands the cloth's dimensions"},
    {CLOTH_FILTER_PINCH,
     "PINCH",
     0,
     "Pinch",
     "Pinches the cloth to the point were the cursor was when the filter started"},
    {0, NULL, 0, NULL, NULL},
};

static void cloth_filter_apply_forces_task_cb(void *__restrict userdata,
                                              const int i,
                                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  Sculpt *sd = data->sd;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];

  SculptClothSimulation *cloth_sim = ss->filter_cache->cloth_sim;
  const int filter_type = data->filter_type;

  float sculpt_gravity[3] = {0.0f};
  if (sd->gravity_object) {
    copy_v3_v3(sculpt_gravity, sd->gravity_object->obmat[2]);
  }
  else {
    sculpt_gravity[2] = -1.0f;
  }
  mul_v3_fl(sculpt_gravity, sd->gravity_factor * data->filter_strength);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    float fade = vd.mask ? *vd.mask : 0.0f;
    fade = 1.0f - fade;
    float force[3] = {0.0f, 0.0f, 0.0f};

    if (ss->filter_cache->active_face_set != SCULPT_FACE_SET_NONE) {
      if (!SCULPT_vertex_has_face_set(ss, vd.index, ss->filter_cache->active_face_set)) {
        continue;
      }
    }

    switch (filter_type) {
      case CLOTH_FILTER_GRAVITY:
        force[2] = -data->filter_strength * fade;
        break;
      case CLOTH_FILTER_INFLATE: {
        float normal[3];
        SCULPT_vertex_normal_get(ss, vd.index, normal);
        mul_v3_v3fl(force, normal, fade * data->filter_strength);
      } break;
      case CLOTH_FILTER_EXPAND:
        cloth_sim->length_constraint_tweak[vd.index] += fade * data->filter_strength * 0.01f;
        zero_v3(force);
        break;
      case CLOTH_FILTER_PINCH:
        sub_v3_v3v3(force, ss->filter_cache->cloth_sim_pinch_point, vd.co);
        normalize_v3(force);
        mul_v3_fl(force, fade * data->filter_strength);
        break;
    }

    add_v3_v3(force, sculpt_gravity);

    cloth_brush_apply_force_to_vertex(ss, cloth_sim, force, vd.index);
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(node);
}

static int sculpt_cloth_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  int filter_type = RNA_enum_get(op->ptr, "type");
  float filter_strength = RNA_float_get(op->ptr, "strength");

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    SCULPT_filter_cache_free(ss);
    SCULPT_undo_push_end();
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  float len = event->prevclickx - event->mval[0];
  filter_strength = filter_strength * -len * 0.001f * UI_DPI_FAC;

  SCULPT_vertex_random_access_init(ss);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true);

  const int totverts = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totverts; i++) {
    copy_v3_v3(ss->filter_cache->cloth_sim->pos[i], SCULPT_vertex_co_get(ss, i));
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
      .filter_type = filter_type,
      .filter_strength = filter_strength,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(
      &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
  BLI_task_parallel_range(
      0, ss->filter_cache->totnode, &data, cloth_filter_apply_forces_task_cb, &settings);

  /* Update and write the simulation to the nodes. */
  cloth_brush_do_simulation_step(
      sd, ob, ss->filter_cache->cloth_sim, ss->filter_cache->nodes, ss->filter_cache->totnode);

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, ob, true);
  }
  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_cloth_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;

  /* Update the active vertex */
  float mouse[2];
  SculptCursorGeometryInfo sgi;
  mouse[0] = event->mval[0];
  mouse[1] = event->mval[1];
  SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);

  SCULPT_vertex_random_access_init(ss);

  /* Needs mask data to be available as it is used when solving the constraints. */
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true);

  SCULPT_undo_push_begin("Cloth filter");
  SCULPT_filter_cache_init(ob, sd);

  const float cloth_mass = RNA_float_get(op->ptr, "cloth_mass");
  const float cloth_damping = RNA_float_get(op->ptr, "cloth_damping");
  ss->filter_cache->cloth_sim = cloth_brush_simulation_create(ss, cloth_mass, cloth_damping);
  copy_v3_v3(ss->filter_cache->cloth_sim_pinch_point, SCULPT_active_vertex_co_get(ss));

  const int totverts = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totverts; i++) {
    copy_v3_v3(ss->filter_cache->cloth_sim->prev_pos[i], SCULPT_vertex_co_get(ss, i));
    copy_v3_v3(ss->filter_cache->cloth_sim->init_pos[i], SCULPT_vertex_co_get(ss, i));
  }

  float origin[3] = {0.0f, 0.0f, 0.0f};
  cloth_brush_build_nodes_constraints(sd,
                                      ob,
                                      ss->filter_cache->nodes,
                                      ss->filter_cache->totnode,
                                      ss->filter_cache->cloth_sim,
                                      origin,
                                      FLT_MAX);

  const bool use_face_sets = RNA_boolean_get(op->ptr, "use_face_sets");
  if (use_face_sets) {
    ss->filter_cache->active_face_set = SCULPT_active_face_set_get(ss);
  }
  else {
    ss->filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SCULPT_OT_cloth_filter(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Filter cloth";
  ot->idname = "SCULPT_OT_cloth_filter";
  ot->description = "Applies a cloth simulation deformation to the entire mesh";

  /* API callbacks. */
  ot->invoke = sculpt_cloth_filter_invoke;
  ot->modal = sculpt_cloth_filter_modal;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* RNA. */
  RNA_def_enum(ot->srna,
               "type",
               prop_cloth_filter_type,
               CLOTH_FILTER_GRAVITY,
               "Filter type",
               "Operation that is going to be applied to the mesh");
  RNA_def_float(
      ot->srna, "strength", 1.0f, -10.0f, 10.0f, "Strength", "Filter Strength", -10.0f, 10.0f);
  RNA_def_float(ot->srna,
                "cloth_mass",
                1.0f,
                0.0f,
                2.0f,
                "Cloth Mass",
                "Mass of each simulation particle",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "cloth_damping",
                0.0f,
                0.0f,
                1.0f,
                "Cloth Damping",
                "How much the applied forces are propagated through the cloth",
                0.0f,
                1.0f);
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_face_sets",
                             false,
                             "Use Face Sets",
                             "Apply the filter only to the Face Set under the cursor");
}
