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
#include "BLI_edgehash.h"
#include "BLI_gsqueue.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_rand.h"
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
#include "BKE_bvhutils.h"
#include "BKE_ccg.h"
#include "BKE_collision.h"
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
#include "DEG_depsgraph_query.h"

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

/* Experimental features. */

#define USE_SOLVER_RIPPLE_CONSTRAINT false
#define BENDING_CONSTRAINTS

#ifdef BENDING_CONSTRAINTS
#  define TOT_CONSTRAINT_TYPES 2
#else
#  define TOT_CONSTRAINT_TYPES 1
#endif

typedef enum { CON_LENGTH, CON_BEND } ClothConstraintTypes;
struct {
  int type;
  int totelem;
  size_t size;
  int offset;
} constraint_types[TOT_CONSTRAINT_TYPES] = {{CON_LENGTH, 2, sizeof(SculptClothLengthConstraint)},
#ifdef BENDING_CONSTRAINTS
                                            {CON_BEND, 4, sizeof(SculptClothBendConstraint)}
#endif
};

/* clang-format off */
enum {
  CLOTH_POS_POS,
  CLOTH_POS_INIT,
  CLOTH_POS_SOFT,
  CLOTH_POS_DEF
};
/* clang-format on */

#ifdef CLOTH_NO_POS_PTR
#  define PACK_POS_TYPE(index, type) ((index) | ((type) << 26))
#  define UNPACK_POS_TYPE(index) ((index) >> 26)
#  define UNPACK_POS_INDEX(index) ((index) & ~(3 << 26))
#  define GET_POS_PTR(index) (&cloth_sim->pos)[UNPACK_POS_TYPE(index)][UNPACK_POS_INDEX(index)]
//#  define GET_POS_PTR_TYPE(index, type) (&cloth_sim->pos)[type][index]
#else
#  define PACK_POS_TYPE(index, type) index
#  define UNPACK_POS_INDEX(index) index
#endif

/* C port of:
   https://github.com/InteractiveComputerGraphics/PositionBasedDynamics/blob/master/PositionBasedDynamics/PositionBasedDynamics.cpp
  MIT license
 */
#ifdef BENDING_CONSTRAINTS

#  ifdef BENDING_DOUBLE_FLOATS
static bool calc_bending_gradients(const SculptClothBendConstraint *constraint,
                                   float gradients[4][3])
{
  // derivatives from Bridson, Simulation of Clothing with Folds and Wrinkles
  // his modes correspond to the derivatives of the bending angle arccos(n1 dot n2) with correct
  // scaling
  const double invMass0 = 1.0f, invMass1 = 1.0f, invMass2 = 1.0f, invMass3 = 1.0f;
  float *_p0 = constraint->elems[0].position;
  float *_p1 = constraint->elems[1].position;
  float *_p2 = constraint->elems[2].position;
  float *_p3 = constraint->elems[3].position;

  double p0[3], p1[3], p2[3], p3[3], d0[3], d1[3], d2[3], d3[3];

  copy_v3db_v3fl(p0, _p0);
  copy_v3db_v3fl(p1, _p1);
  copy_v3db_v3fl(p2, _p2);
  copy_v3db_v3fl(p3, _p3);

  if (invMass0 == 0.0 && invMass1 == 0.0)
    return false;

  double e[3];
  sub_v3_v3v3_db(e, p3, p2);
  float elen = len_v3_db(e);
  const double eps = 1e-6;

  if (elen < eps) {
    return false;
  }

  double invElen = 1.0f / elen;

  double tmp1[3], tmp2[3], tmp3[3], tmp4[3], n1[3], n2[3];

  sub_v3_v3v3_db(tmp1, p2, p0);
  sub_v3_v3v3_db(tmp2, p3, p0);
  cross_v3_v3v3_db(n1, tmp1, tmp2);

  if (dot_v3v3_db(n1, n1) == 0.0) {
    return false;
  }

  mul_v3db_db(n1, 1.0 / dot_v3v3_db(n1, n1));

  sub_v3_v3v3_db(tmp1, p3, p1);
  sub_v3_v3v3_db(tmp2, p2, p1);
  cross_v3_v3v3_db(n2, tmp1, tmp2);

  if (dot_v3v3_db(n2, n2) == 0.0) {
    return false;
  }
  mul_v3db_db(n2, 1.0 / dot_v3v3_db(n2, n2));

  mul_v3_v3db_db(d0, n1, elen);
  mul_v3_v3db_db(d1, n2, elen);

  //  Vector3r d2 = (p0 - p3).dot(e) * invElen * n1 + (p1 - p3).dot(e) * invElen * n2;
  sub_v3_v3v3_db(tmp1, p0, p3);
  double fac = dot_v3v3_db(tmp1, e) * invElen;
  mul_v3_v3db_db(d2, n1, fac);

  sub_v3_v3v3_db(tmp1, p1, p3);
  fac = dot_v3v3_db(tmp1, e) * invElen;
  mul_v3_v3db_db(tmp2, n2, fac);

  add_v3_v3_db(d2, tmp2);

  // Vector3r d3 = (p2 - p0).dot(e) * invElen * n1 + (p2 - p1).dot(e) * invElen * n2;
  sub_v3_v3v3_db(tmp1, p2, p0);
  fac = dot_v3v3_db(tmp1, e) * invElen;
  mul_v3_v3db_db(d3, n1, fac);

  sub_v3_v3v3_db(tmp1, p2, p1);
  fac = dot_v3v3_db(tmp1, e) * invElen;
  mul_v3_v3db_db(tmp2, n2, fac);

  add_v3_v3_db(d3, tmp2);

  normalize_v3_db(n1);
  normalize_v3_db(n2);

  double dot = dot_v3v3_db(n1, n2);

  CLAMP(dot, -1.0f, 1.0f);
  double phi = acos(dot);

  // Real phi = (-0.6981317 * dot * dot - 0.8726646) * dot + 1.570796;	// fast approximation

  double lambda = invMass0 * dot_v3v3_db(d0, d0) + invMass1 * dot_v3v3_db(d1, d1) +
                  invMass2 * dot_v3v3_db(d2, d2) + invMass3 * dot_v3v3_db(d3, d3);

  if (lambda == 0.0)
    return false;

  // stability
  // 1.5 is the largest magic number I found to be stable in all cases :-)
  // if (stiffness > 0.5 && fabs(phi - b.restAngle) > 1.5)
  //	stiffness = 0.5;

  lambda = (phi - constraint->rest_angle) / lambda * constraint->stiffness;

  cross_v3_v3v3_db(tmp1, n1, n2);
  if (dot_v3v3_db(tmp1, e) > 0.0f) {
    lambda = -lambda;
  }

  mul_v3db_db(d0, -invMass0 * lambda);
  mul_v3db_db(d1, -invMass1 * lambda);
  mul_v3db_db(d2, -invMass2 * lambda);
  mul_v3db_db(d3, -invMass3 * lambda);

  copy_v3fl_v3db(gradients[0], d0);
  copy_v3fl_v3db(gradients[1], d1);
  copy_v3fl_v3db(gradients[2], d2);
  copy_v3fl_v3db(gradients[3], d3);

  return true;
}
#  else
static bool calc_bending_gradients(const SculptClothSimulation *cloth_sim,
                                   const SculptClothBendConstraint *constraint,
                                   float gradients[4][3])
{
  // derivatives from Bridson, Simulation of Clothing with Folds and Wrinkles
  // his modes correspond to the derivatives of the bending angle arccos(n1 dot n2) with correct
  // scaling
  const float invMass0 = 1.0f, invMass1 = 1.0f, invMass2 = 1.0f, invMass3 = 1.0f;
#    ifndef CLOTH_NO_POS_PTR
  float *p0 = constraint->elems[0].position;
  float *p1 = constraint->elems[1].position;
  float *p2 = constraint->elems[2].position;
  float *p3 = constraint->elems[3].position;
#    else
  float *p0 = GET_POS_PTR(constraint->elems[0].index);
  float *p1 = GET_POS_PTR(constraint->elems[1].index);
  float *p2 = GET_POS_PTR(constraint->elems[2].index);
  float *p3 = GET_POS_PTR(constraint->elems[3].index);
#    endif

  float *d0 = gradients[0];
  float *d1 = gradients[1];
  float *d2 = gradients[2];
  float *d3 = gradients[3];

  if (invMass0 == 0.0 && invMass1 == 0.0)
    return false;

  float e[3];
  sub_v3_v3v3(e, p3, p2);
  float elen = len_v3(e);
  const float eps = 1e-6;

  if (elen < eps) {
    return false;
  }

  float invElen = 1.0f / elen;

  float tmp1[3], tmp2[3], n1[3], n2[3];

  sub_v3_v3v3(tmp1, p2, p0);
  sub_v3_v3v3(tmp2, p3, p0);
  cross_v3_v3v3(n1, tmp1, tmp2);

  if (dot_v3v3(n1, n1) == 0.0) {
    return false;
  }

  mul_v3_fl(n1, 1.0 / dot_v3v3(n1, n1));

  sub_v3_v3v3(tmp1, p3, p1);
  sub_v3_v3v3(tmp2, p2, p1);
  cross_v3_v3v3(n2, tmp1, tmp2);

  if (dot_v3v3(n2, n2) == 0.0) {
    return false;
  }
  mul_v3_fl(n2, 1.0 / dot_v3v3(n2, n2));

  mul_v3_v3fl(d0, n1, elen);
  mul_v3_v3fl(d1, n2, elen);

  //  Vector3r d2 = (p0 - p3).dot(e) * invElen * n1 + (p1 - p3).dot(e) * invElen * n2;
  sub_v3_v3v3(tmp1, p0, p3);
  float fac = dot_v3v3(tmp1, e) * invElen;
  mul_v3_v3fl(d2, n1, fac);

  sub_v3_v3v3(tmp1, p1, p3);
  fac = dot_v3v3(tmp1, e) * invElen;
  mul_v3_v3fl(tmp2, n2, fac);

  add_v3_v3(d2, tmp2);

  // Vector3r d3 = (p2 - p0).dot(e) * invElen * n1 + (p2 - p1).dot(e) * invElen * n2;
  sub_v3_v3v3(tmp1, p2, p0);
  fac = dot_v3v3(tmp1, e) * invElen;
  mul_v3_v3fl(d3, n1, fac);

  sub_v3_v3v3(tmp1, p2, p1);
  fac = dot_v3v3(tmp1, e) * invElen;
  mul_v3_v3fl(tmp2, n2, fac);

  add_v3_v3(d3, tmp2);

  normalize_v3(n1);
  normalize_v3(n2);

  float dot = dot_v3v3(n1, n2);

  CLAMP(dot, -1.0f, 1.0f);
  float phi = acos(dot);

  // Real phi = (-0.6981317 * dot * dot - 0.8726646) * dot + 1.570796;	// fast approximation

  float lambda = invMass0 * dot_v3v3(d0, d0) + invMass1 * dot_v3v3(d1, d1) +
                 invMass2 * dot_v3v3(d2, d2) + invMass3 * dot_v3v3(d3, d3);

  if (lambda == 0.0)
    return false;

  // stability
  // 1.5 is the largest magic number I found to be stable in all cases :-)
  // if (stiffness > 0.5 && fabs(phi - b.restAngle) > 1.5)
  //	stiffness = 0.5;

  lambda = (phi - constraint->rest_angle) / lambda * constraint->stiffness;

  cross_v3_v3v3(tmp1, n1, n2);
  if (dot_v3v3(tmp1, e) > 0.0f) {
    lambda = -lambda;
  }

  mul_v3_fl(d0, -invMass0 * lambda);
  mul_v3_fl(d1, -invMass1 * lambda);
  mul_v3_fl(d2, -invMass2 * lambda);
  mul_v3_fl(d3, -invMass3 * lambda);

  return true;
}
#  endif

#  if 0
// attempt at highly unphysical but faster bending solver
static bool calc_bending_gradients_2(const SculptClothBendConstraint *constraint,
                                     float gradients[4][3])
{
  float *p0 = constraint->elems[0].position;
  float *p1 = constraint->elems[1].position;
  float *p2 = constraint->elems[2].position;
  float *p3 = constraint->elems[3].position;

  float t1[3], t2[3], t3[3];
  float t1a[3], t2a[3], t3a[3];
  float mid[3];

  add_v3_v3v3(mid, p0, p1);
  mul_v3_fl(mid, 0.5f);

  sub_v3_v3v3(t1a, p2, mid);
  sub_v3_v3v3(t2a, p3, mid);
  sub_v3_v3v3(t3, p1, p0);

  cross_v3_v3v3(t1, t1a, t3);
  cross_v3_v3v3(t2, t3, t2a);

  cross_v3_v3v3(t3a, t1, t2);
  if (dot_v3v3(t3a, t3) > 0.0f) {
    negate_v3(t1);
    negate_v3(t2);
  }
  /*

  on factor;
  off period;

  load_package "avector";

  t1 := avec(t1x, t1y, t1z);
  t2 := avec(t2x, t2y, t2z);
  n  := avec(nx, ny, nz);

  f1 := (t1 dot t2 - t1len*t2len*goalth) * (VMOD t1 - t1len**2) * (VMOD t2 - t2len**2);

  */
  float len = dot_v3v3(t1, t1) + dot_v3v3(t2, t2);

  normalize_v3(t1);
  normalize_v3(t2);

  float th = saacos(dot_v3v3(t1, t2));
  float err = th - constraint->rest_angle;

  zero_v3(gradients[0]);
  zero_v3(gradients[1]);

  float f = 3000.0f;

  add_v3_v3v3(t3, t1, t2);
  normalize_v3(t3);

  mul_v3_v3fl(gradients[0], t3, -err * len * f);
  mul_v3_v3fl(gradients[1], t3, -err * len * f);
  mul_v3_v3fl(gradients[2], t1, err * len * f);
  mul_v3_v3fl(gradients[3], t2, err * len * f);

  return true;
}
#  endif
#endif

static void cloth_brush_simulation_location_get(SculptSession *ss,
                                                const Brush *brush,
                                                float r_location[3])
{
  if (!ss->cache || !brush) {
    zero_v3(r_location);
    return;
  }

  if (ss->cache->cloth_sim->simulation_area_type == BRUSH_CLOTH_SIMULATION_AREA_LOCAL) {
    copy_v3_v3(r_location, ss->cache->initial_location);
    return;
  }
  copy_v3_v3(r_location, ss->cache->location);
}

PBVHNode **SCULPT_cloth_brush_affected_nodes_gather(SculptSession *ss,
                                                    Brush *brush,
                                                    int *r_totnode)
{
  BLI_assert(ss->cache);
  // BLI_assert(brush->sculpt_tool == SCULPT_TOOL_CLOTH);
  PBVHNode **nodes = NULL;

  switch (SCULPT_get_int(ss, cloth_simulation_area_type, NULL, brush)) {
    case BRUSH_CLOTH_SIMULATION_AREA_LOCAL: {
      SculptSearchSphereData data = {
          .ss = ss,
          .radius_squared = square_f(ss->cache->initial_radius *
                                     (1.0 + SCULPT_get_float(ss, cloth_sim_limit, NULL, brush))),
          .original = false,
          .ignore_fully_ineffective = false,
          .center = ss->cache->initial_location,
      };
      BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, r_totnode);
    } break;
    case BRUSH_CLOTH_SIMULATION_AREA_GLOBAL:
      BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, r_totnode);
      break;
    case BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC: {
      SculptSearchSphereData data = {
          .ss = ss,
          .radius_squared = square_f(ss->cache->radius *
                                     (1.0 + SCULPT_get_float(ss, cloth_sim_limit, NULL, brush))),
          .original = false,
          .ignore_fully_ineffective = false,
          .center = ss->cache->location,
      };
      BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, r_totnode);
    } break;
  }

  return nodes;
}

static float cloth_brush_simulation_falloff_get(const SculptClothSimulation *cloth_sim,
                                                const Brush *brush,
                                                const float radius,
                                                const float location[3],
                                                const float co[3])
{
  if (brush->sculpt_tool != SCULPT_TOOL_CLOTH) {
    /* All brushes that are not the cloth brush do not use simulation areas.
       TODO: new command list situation may change this, investigate
    */
    return 1.0f;
  }

  /* Global simulation does not have any falloff as the entire mesh is being simulated. */
  if (cloth_sim->simulation_area_type == BRUSH_CLOTH_SIMULATION_AREA_GLOBAL) {
    return 1.0f;
  }

  const float distance = len_v3v3(location, co);
  const float limit = radius + (radius * cloth_sim->sim_limit);
  const float falloff = radius + (radius * cloth_sim->sim_limit * cloth_sim->sim_falloff);

  if (distance > limit) {
    /* Outside the limits. */
    return 0.0f;
  }
  if (distance < falloff) {
    /* Before the falloff area. */
    return 1.0f;
  }
  /* Do a smooth-step transition inside the falloff area. */
  float p = 1.0f - ((distance - falloff) / (limit - falloff));
  return 3.0f * p * p - 2.0f * p * p * p;
}

#define CLOTH_LENGTH_CONSTRAINTS_BLOCK 100000
// solver iterations now depend on if bending constriants are on,
// if so fewer iterations are taken
//#define CLOTH_SIMULATION_ITERATIONS 5

#define CLOTH_SOLVER_DISPLACEMENT_FACTOR 0.6f
#define CLOTH_MAX_CONSTRAINTS_PER_VERTEX 1024
#define CLOTH_SIMULATION_TIME_STEP 0.01f
#define CLOTH_DEFORMATION_SNAKEHOOK_STRENGTH 0.35f
#define CLOTH_DEFORMATION_TARGET_STRENGTH 0.5f
#define CLOTH_DEFORMATION_GRAB_STRENGTH 0.5f

static bool cloth_brush_sim_has_length_constraint(SculptClothSimulation *cloth_sim,
                                                  const int v1,
                                                  const int v2)
{
  return BLI_edgeset_haskey(cloth_sim->created_length_constraints, v1, v2);
}

static bool cloth_brush_sim_has_bend_constraint(SculptClothSimulation *cloth_sim,
                                                const int v1,
                                                const int v2)
{
  return BLI_edgeset_haskey(cloth_sim->created_bend_constraints, v1, v2);
}

static void cloth_brush_reallocate_constraints(SculptClothSimulation *cloth_sim)
{
  for (int i = 0; i < TOT_CONSTRAINT_TYPES; i++) {
    if (cloth_sim->tot_constraints[i] >= cloth_sim->capacity_constraints[i]) {
      cloth_sim->capacity_constraints[i] += CLOTH_LENGTH_CONSTRAINTS_BLOCK;

      cloth_sim->constraints[i] = MEM_reallocN_id(cloth_sim->constraints[i],
                                                  cloth_sim->capacity_constraints[i] *
                                                      constraint_types[i].size,
                                                  "cloth constraint array");
    }
  }
}

static void *cloth_add_constraint(SculptClothSimulation *cloth_sim, int type)
{
  cloth_sim->tot_constraints[type]++;
  cloth_brush_reallocate_constraints(cloth_sim);

  char *ptr = (char *)cloth_sim->constraints[type];

  SculptClothConstraint *con =
      (SculptClothConstraint *)(ptr + constraint_types[type].size *
                                          (cloth_sim->tot_constraints[type] - 1));
  con->ctype = type;

  return (void *)con;
}

#ifdef BENDING_CONSTRAINTS
static void cloth_brush_add_bend_constraint(SculptSession *ss,
                                            SculptClothSimulation *cloth_sim,
                                            const int node_index,
                                            const int v1i,
                                            const int v2i,
                                            const int v3i,
                                            const int v4i,
                                            const bool use_persistent)
{
  SculptClothBendConstraint *bend_constraint = (SculptClothBendConstraint *)cloth_add_constraint(
      cloth_sim, CON_BEND);

  SculptVertRef v1, v2, v3, v4;

  v1 = BKE_pbvh_table_index_to_vertex(ss->pbvh, v1i);
  v2 = BKE_pbvh_table_index_to_vertex(ss->pbvh, v2i);
  v3 = BKE_pbvh_table_index_to_vertex(ss->pbvh, v3i);
  v4 = BKE_pbvh_table_index_to_vertex(ss->pbvh, v4i);

  bend_constraint->elems[0].index = PACK_POS_TYPE(v1i, CLOTH_POS_POS);
  bend_constraint->elems[1].index = PACK_POS_TYPE(v2i, CLOTH_POS_POS);
  bend_constraint->elems[2].index = PACK_POS_TYPE(v3i, CLOTH_POS_POS);
  bend_constraint->elems[3].index = PACK_POS_TYPE(v4i, CLOTH_POS_POS);

  bend_constraint->node = node_index;

#  ifndef CLOTH_NO_POS_PTR
  bend_constraint->elems[0].position = cloth_sim->pos[v1i];
  bend_constraint->elems[1].position = cloth_sim->pos[v2i];
  bend_constraint->elems[2].position = cloth_sim->pos[v3i];
  bend_constraint->elems[3].position = cloth_sim->pos[v4i];
#  endif

  const float *co1, *co2, *co3, *co4;

  if (use_persistent) {
    co1 = SCULPT_vertex_persistent_co_get(ss, v1);
    co2 = SCULPT_vertex_persistent_co_get(ss, v2);
    co3 = SCULPT_vertex_persistent_co_get(ss, v3);
    co4 = SCULPT_vertex_persistent_co_get(ss, v4);
  }
  else {
    co1 = SCULPT_vertex_co_get(ss, v1);
    co2 = SCULPT_vertex_co_get(ss, v2);
    co3 = SCULPT_vertex_co_get(ss, v3);
    co4 = SCULPT_vertex_co_get(ss, v4);
  }

  float t1[3], t2[3];
  normal_tri_v3(t1, co1, co3, co2);
  normal_tri_v3(t2, co2, co4, co1);

  bend_constraint->rest_angle = saacos(dot_v3v3(t1, t2));
  bend_constraint->stiffness = cloth_sim->bend_stiffness;
  bend_constraint->strength = 1.0f;

  /* Add the constraint to the #GSet to avoid creating it again. */
  BLI_edgeset_add(cloth_sim->created_bend_constraints, v1i, v2i);
}
#endif

static void cloth_brush_add_length_constraint(SculptSession *ss,
                                              SculptClothSimulation *cloth_sim,
                                              const int node_index,
                                              const int v1i,
                                              const int v2i,
                                              const bool use_persistent)
{
  SculptClothLengthConstraint *length_constraint = cloth_add_constraint(cloth_sim, CON_LENGTH);
  SculptVertRef v1, v2;

  v1 = BKE_pbvh_table_index_to_vertex(ss->pbvh, v1i);
  v2 = BKE_pbvh_table_index_to_vertex(ss->pbvh, v2i);

  length_constraint->elems[0].index = PACK_POS_TYPE(v1i, CLOTH_POS_POS);
  length_constraint->elems[1].index = PACK_POS_TYPE(v2i, CLOTH_POS_POS);

  length_constraint->node = node_index;

#ifndef CLOTH_NO_POS_PTR
  length_constraint->elems[0].position = cloth_sim->pos[v1i];
  length_constraint->elems[1].position = cloth_sim->pos[v2i];
#endif

  length_constraint->type = SCULPT_CLOTH_CONSTRAINT_STRUCTURAL;

  if (use_persistent) {
    length_constraint->length = len_v3v3(SCULPT_vertex_persistent_co_get(ss, v1),
                                         SCULPT_vertex_persistent_co_get(ss, v2));
  }
  else {
    length_constraint->length = len_v3v3(SCULPT_vertex_co_get(ss, v1),
                                         SCULPT_vertex_co_get(ss, v2));
  }
  length_constraint->strength = 1.0f;

  /* Reallocation if the array capacity is exceeded. */
  cloth_brush_reallocate_constraints(cloth_sim);

  /* Add the constraint to the #GSet to avoid creating it again. */
  BLI_edgeset_add(cloth_sim->created_length_constraints, v1i, v2i);
}

static void cloth_brush_add_softbody_constraint(SculptClothSimulation *cloth_sim,
                                                const int node_index,
                                                const int v,
                                                const float strength)
{
  SculptClothLengthConstraint *length_constraint = cloth_add_constraint(cloth_sim, CON_LENGTH);

  length_constraint->elems[0].index = PACK_POS_TYPE(v, CLOTH_POS_POS);
  length_constraint->elems[1].index = PACK_POS_TYPE(v, CLOTH_POS_SOFT);

  length_constraint->node = node_index;

#ifndef CLOTH_NO_POS_PTR
  length_constraint->elems[0].position = cloth_sim->pos[v];
  length_constraint->elems[1].position = cloth_sim->softbody_pos[v];
#endif

  length_constraint->type = SCULPT_CLOTH_CONSTRAINT_SOFTBODY;

  length_constraint->length = 0.0f;
  length_constraint->strength = strength;

  /* Reallocation if the array capacity is exceeded. */
  cloth_brush_reallocate_constraints(cloth_sim);
}

static void cloth_brush_add_pin_constraint(SculptClothSimulation *cloth_sim,
                                           const int node_index,
                                           const int v,
                                           const float strength)
{
  SculptClothLengthConstraint *length_constraint = cloth_add_constraint(cloth_sim, CON_LENGTH);

  length_constraint->elems[0].index = PACK_POS_TYPE(v, CLOTH_POS_POS);
  length_constraint->elems[1].index = PACK_POS_TYPE(v, CLOTH_POS_INIT);

  length_constraint->node = node_index;

#ifndef CLOTH_NO_POS_PTR
  length_constraint->elems[0].position = cloth_sim->pos[v];
  length_constraint->elems[1].position = cloth_sim->init_pos[v];
#endif

  length_constraint->type = SCULPT_CLOTH_CONSTRAINT_PIN;

  length_constraint->length = 0.0f;
  length_constraint->strength = strength;

  /* Reallocation if the array capacity is exceeded. */
  cloth_brush_reallocate_constraints(cloth_sim);
}

static void cloth_brush_add_deformation_constraint(SculptClothSimulation *cloth_sim,
                                                   const int node_index,
                                                   const int v,
                                                   const float strength)
{
  SculptClothLengthConstraint *length_constraint = cloth_add_constraint(cloth_sim, CON_LENGTH);

  length_constraint->elems[0].index = PACK_POS_TYPE(v, CLOTH_POS_POS);
  length_constraint->elems[1].index = PACK_POS_TYPE(v, CLOTH_POS_DEF);

  length_constraint->node = node_index;

  length_constraint->type = SCULPT_CLOTH_CONSTRAINT_DEFORMATION;

#ifndef CLOTH_NO_POS_PTR
  length_constraint->elems[0].position = cloth_sim->pos[v];
  length_constraint->elems[1].position = cloth_sim->deformation_pos[v];
#endif

  length_constraint->length = 0.0f;
  length_constraint->strength = strength;

  /* Reallocation if the array capacity is exceeded. */
  cloth_brush_reallocate_constraints(cloth_sim);
}

static void do_cloth_brush_build_constraints_task_cb_ex(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  PBVHNode *node = data->nodes[n];

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
    SCULPT_ensure_epmap(ss);
  }

  const int node_index = POINTER_AS_INT(BLI_ghash_lookup(data->cloth_sim->node_state_index, node));
  if (data->cloth_sim->node_state[node_index] != SCULPT_CLOTH_NODE_UNINITIALIZED) {
    /* The simulation already contains constraints for this node. */
    return;
  }

  PBVHVertexIter vd;

  const bool pin_simulation_boundary = ss->cache != NULL && brush != NULL &&
                                       brush->flag2 & BRUSH_CLOTH_PIN_SIMULATION_BOUNDARY &&
                                       brush->cloth_simulation_area_type !=
                                           BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC;

  const bool use_persistent = brush != NULL && brush->flag & BRUSH_PERSISTENT;

  /* Brush can be NULL in tools that use the solver without relying of constraints with deformation
   * positions. */
  const bool cloth_is_deform_brush = ss->cache != NULL && brush != NULL &&
                                     SCULPT_is_cloth_deform_brush(brush);

  const bool use_falloff_plane = brush->cloth_force_falloff_type ==
                                 BRUSH_CLOTH_FORCE_FALLOFF_PLANE;
  float radius_squared = 0.0f;
  if (cloth_is_deform_brush) {
    radius_squared = ss->cache->initial_radius * ss->cache->initial_radius;
  }

  bool use_bending = data->cloth_sim->use_bending;

  /* Only limit the constraint creation to a radius when the simulation is local. */
  const float cloth_sim_radius_squared = brush->cloth_simulation_area_type ==
                                                 BRUSH_CLOTH_SIMULATION_AREA_LOCAL ?
                                             data->cloth_sim_radius * data->cloth_sim_radius :
                                             FLT_MAX;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    const float len_squared = len_squared_v3v3(vd.co, data->cloth_sim_initial_location);
    if (len_squared < cloth_sim_radius_squared) {

      SculptVertexNeighborIter ni;
      int build_indices[CLOTH_MAX_CONSTRAINTS_PER_VERTEX];
      SculptEdgeRef build_edges[CLOTH_MAX_CONSTRAINTS_PER_VERTEX];

      int tot_indices = 0;
      bool have_edges = false;

      build_indices[tot_indices] = vd.index;
      tot_indices++;

      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
        build_indices[tot_indices] = ni.index;

        if (ni.has_edge) {
          have_edges = true;
          build_edges[tot_indices - 1] = ni.edge;
        }
        else {
          build_edges[tot_indices - 1].i = SCULPT_REF_NONE;
        }

        tot_indices++;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

      if (data->cloth_sim->softbody_strength > 0.0f) {
        cloth_brush_add_softbody_constraint(data->cloth_sim, node_index, vd.index, 1.0f);
      }

#ifdef BENDING_CONSTRAINTS

      for (int c_i = 0; use_bending && c_i < tot_indices - 1; c_i++) {
        if (have_edges && build_edges[c_i].i && build_edges[c_i].i != SCULPT_REF_NONE) {
          SculptEdgeRef edge = build_edges[c_i];

          if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
            BMEdge *e = (BMEdge *)edge.i;

            if (!e->l) {
              continue;
            }

            int v1i = e->v1->head.index;
            int v2i = e->v2->head.index;

            if (cloth_brush_sim_has_bend_constraint(data->cloth_sim, v1i, v2i)) {
              continue;
            }

            BMLoop *l1, *l2;

            l1 = e->l;
            l2 = e->l->radial_next;

            if (l1 != l2) {
              l1 = l1->next->next;
              l2 = l2->next->next;

              cloth_brush_add_bend_constraint(ss,
                                              data->cloth_sim,
                                              node_index,
                                              e->v1->head.index,
                                              e->v2->head.index,
                                              l1->v->head.index,
                                              l2->v->head.index,
                                              use_persistent);

              if (l1->f->len == 4 && l2->f->len == 4) {
                cloth_brush_add_bend_constraint(ss,
                                                data->cloth_sim,
                                                node_index,
                                                e->v1->head.index,
                                                e->v2->head.index,
                                                l1->next->v->head.index,
                                                l2->next->v->head.index,
                                                use_persistent);
              }
              else if (l1->f->len == 4) {
                cloth_brush_add_bend_constraint(ss,
                                                data->cloth_sim,
                                                node_index,
                                                e->v1->head.index,
                                                e->v2->head.index,
                                                l1->next->v->head.index,
                                                l2->v->head.index,
                                                use_persistent);
              }
              else if (l2->f->len == 4) {
                cloth_brush_add_bend_constraint(ss,
                                                data->cloth_sim,
                                                node_index,
                                                e->v1->head.index,
                                                e->v2->head.index,
                                                l1->v->head.index,
                                                l2->next->v->head.index,
                                                use_persistent);
              }
            }
          }
          else if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
            MeshElemMap *map = ss->epmap + edge.i;
            if (map->count != 2) {
              continue;
            }

            MPoly *mp1 = ss->mpoly + map->indices[0];
            MPoly *mp2 = ss->mpoly + map->indices[1];
            MLoop *ml;
            int ml1 = -1, ml2 = -1;

            ml = ss->mloop + mp1->loopstart;
            for (int j = 0; j < mp1->totloop; j++, ml++) {
              if (ml->e == edge.i) {
                ml1 = j;
                break;
              }
            }

            ml = ss->mloop + mp2->loopstart;
            for (int j = 0; j < mp2->totloop; j++, ml++) {
              if (ml->e == edge.i) {
                ml2 = j;
                break;
              }
            }

            if (ml1 == -1 || ml2 == -1) {
              continue;
            }

            int v1i = ss->medge[edge.i].v1;
            int v2i = ss->medge[edge.i].v2;

            ml1 = (ml1 + 2) % mp1->totloop;
            ml2 = (ml2 + 2) % mp2->totloop;

            cloth_brush_add_bend_constraint(ss,
                                            data->cloth_sim,
                                            node_index,
                                            v1i,
                                            v2i,
                                            ss->mloop[mp1->loopstart + ml1].v,
                                            ss->mloop[mp2->loopstart + ml2].v,
                                            use_persistent);

            if (mp1->totloop == 4 && mp2->totloop == 4) {
              ml1 = (ml1 + 1) % mp1->totloop;
              ml2 = (ml2 + 1) % mp2->totloop;

              cloth_brush_add_bend_constraint(ss,
                                              data->cloth_sim,
                                              node_index,
                                              v1i,
                                              v2i,
                                              ss->mloop[mp1->loopstart + ml1].v,
                                              ss->mloop[mp2->loopstart + ml2].v,
                                              use_persistent);
            }
            else if (mp1->totloop == 4) {
              ml1 = (ml1 + 1) % mp1->loopstart;

              cloth_brush_add_bend_constraint(ss,
                                              data->cloth_sim,
                                              node_index,
                                              v1i,
                                              v2i,
                                              ss->mloop[mp1->loopstart + ml1].v,
                                              ss->mloop[mp2->loopstart + ml2].v,
                                              use_persistent);
            }
            else if (mp2->totloop == 4) {
              ml2 = (ml2 + 1) % mp2->loopstart;

              cloth_brush_add_bend_constraint(ss,
                                              data->cloth_sim,
                                              node_index,
                                              v1i,
                                              v2i,
                                              ss->mloop[mp1->loopstart + ml1].v,
                                              ss->mloop[mp2->loopstart + ml2].v,
                                              use_persistent);
            }
          }
        }
        else if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
          const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
          int v1i = vd.index;

          const int grid_size = key->grid_size;
          const int grid_index1 = v1i / key->grid_area;
          const int vertex_index1 = v1i - grid_index1 * key->grid_area;

          SubdivCCGCoord c1 = {.grid_index = grid_index1,
                               .x = vertex_index1 % key->grid_size,
                               .y = vertex_index1 / key->grid_size};

          bool inside1 = c1.x > 1 && c1.x < grid_size - 2 && c1.y > 1 && c1.y < grid_size - 2;

          for (int i = 0; i < tot_indices - 1; i++) {
            int v2i = build_indices[i + 1];

            if (cloth_brush_sim_has_bend_constraint(data->cloth_sim, v1i, v2i)) {
              continue;
            }

            int v3i = -1, v4i = -1, v5i = -1, v6i = -1;

            const int grid_index2 = v2i / key->grid_area;
            const int vertex_index2 = v2i - grid_index1 * key->grid_area;

            SubdivCCGCoord c2 = {.grid_index = grid_index2,
                                 .x = vertex_index2 % key->grid_size,
                                 .y = vertex_index2 / key->grid_size};

            bool inside2 = c2.x > 1 && c2.x < grid_size - 2 && c2.y > 1 && c2.y < grid_size - 2;

            if (inside1 && inside2 && grid_index1 == grid_index2) {
              int x1, y1, x2, y2, x3, y3, x4, y4;

              if (c1.x == c2.x) {
                x1 = c1.x + 1;
                x2 = c1.x + 1;

                y1 = c1.y;
                y2 = c2.y;

                x3 = c1.x - 1;
                x4 = c1.x - 1;

                y3 = c1.y;
                y4 = c2.y;
              }
              else {
                y1 = c1.y + 1;
                y2 = c1.y + 1;

                x1 = c1.x;
                x2 = c2.x;

                y3 = c1.y - 1;
                y4 = c1.y - 1;

                x3 = c1.x;
                x4 = c2.x;
              }

              v3i = y1 * grid_size + x1 + grid_index1 * key->grid_area;
              v4i = y2 * grid_size + x2 + grid_index1 * key->grid_area;
              v5i = y3 * grid_size + x3 + grid_index1 * key->grid_area;
              v6i = y4 * grid_size + x4 + grid_index1 * key->grid_area;

              // printf("\n%d %d %d %d %d %d\n", v1i, v2i, v3i, v4i, v5i, v6i);
            }
            else { /* for grid boundaries use slow brute search to get adjacent verts */
              SculptVertexNeighborIter ni2 = {0}, ni3 = {0}, ni4 = {0};
              v3i = -1;
              v4i = -1;
              v5i = -1;
              v6i = -1;

              memset(&ni, 0, sizeof(ni));

              SculptVertRef vertex2 = BKE_pbvh_table_index_to_vertex(ss->pbvh, v2i);

              SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
                if (ni.vertex.i == vertex2.i) {
                  continue;
                }

                SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex2, ni2) {
                  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, ni2.vertex, ni3) {
                    if (ni3.vertex.i == ni.vertex.i) {
                      if (v3i == -1 && !ELEM(ni3.vertex.i, v1i, v2i) &&
                          !ELEM(ni2.vertex.i, v1i, v2i)) {
                        v3i = ni2.vertex.i;
                        v4i = ni3.vertex.i;
                      }
                      else if (v5i == -1 && !ELEM(ni3.vertex.i, v1i, v2i, v3i, v4i) &&
                               !ELEM(ni2.vertex.i, v1i, v2i, v3i, v4i)) {
                        v5i = ni2.vertex.i;
                        v6i = ni3.vertex.i;
                        goto break_all;
                      }
                    }
                  }
                  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni3);
                }
                SCULPT_VERTEX_NEIGHBORS_ITER_END(ni2);
              }
              SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
            break_all:
              SCULPT_VERTEX_NEIGHBORS_ITER_FREE(ni);
              SCULPT_VERTEX_NEIGHBORS_ITER_FREE(ni2);
              SCULPT_VERTEX_NEIGHBORS_ITER_FREE(ni3);
              SCULPT_VERTEX_NEIGHBORS_ITER_FREE(ni4);
            }
            if (v5i == -1) {  // should only happen on mesh boundaries
              continue;
            }

            cloth_brush_add_bend_constraint(
                ss, data->cloth_sim, node_index, v1i, v2i, v3i, v6i, use_persistent);
            cloth_brush_add_bend_constraint(
                ss, data->cloth_sim, node_index, v1i, v2i, v4i, v5i, use_persistent);
          }
        }
      }
#endif

      /* As we don't know the order of the neighbor vertices, we create all possible combinations
       * between the neighbor and the original vertex as length constraints. */
      /* This results on a pattern that contains structural, shear and bending constraints for all
       * vertices, but constraints are repeated taking more memory than necessary. */
      for (int c_i = 0; c_i < tot_indices; c_i++) {
        for (int c_j = 0; c_j < tot_indices; c_j++) {
          if (c_i != c_j && !cloth_brush_sim_has_length_constraint(
                                data->cloth_sim, build_indices[c_i], build_indices[c_j])) {
            cloth_brush_add_length_constraint(ss,
                                              data->cloth_sim,
                                              node_index,
                                              build_indices[c_i],
                                              build_indices[c_j],
                                              use_persistent);
          }
        }
      }
    }

    if (brush && brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
      /* The cloth brush works by applying forces in most of its modes, but some of them require
       * deformation coordinates to make the simulation stable. */
      if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
        if (use_falloff_plane) {
          /* With plane falloff the strength of the constraints is set when applying the
           * deformation forces. */
          cloth_brush_add_deformation_constraint(
              data->cloth_sim, node_index, vd.index, CLOTH_DEFORMATION_GRAB_STRENGTH);
        }
        else if (len_squared < radius_squared) {
          /* With radial falloff deformation constraints are created with different strengths and
           * only inside the radius of the brush. */
          const float fade = BKE_brush_curve_strength(
              brush, sqrtf(len_squared), ss->cache->radius);
          cloth_brush_add_deformation_constraint(
              data->cloth_sim, node_index, vd.index, fade * CLOTH_DEFORMATION_GRAB_STRENGTH);
        }
      }
      else if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_SNAKE_HOOK) {
        /* Cloth Snake Hook creates deformation constraint with fixed strength because the strength
         * is controlled per iteration using cloth_sim->deformation_strength. */
        cloth_brush_add_deformation_constraint(
            data->cloth_sim, node_index, vd.index, CLOTH_DEFORMATION_SNAKEHOOK_STRENGTH);
      }
    }
    else if (data->cloth_sim->deformation_pos) {
      /* Any other tool that target the cloth simulation handle the falloff in
       * their own code when modifying the deformation coordinates of the simulation, so
       * deformation constraints are created with a fixed strength for all vertices. */
      cloth_brush_add_deformation_constraint(
          data->cloth_sim, node_index, vd.index, CLOTH_DEFORMATION_TARGET_STRENGTH);
    }

    if (pin_simulation_boundary) {
      const float sim_falloff = cloth_brush_simulation_falloff_get(
          data->cloth_sim, brush, ss->cache->initial_radius, ss->cache->location, vd.co);
      /* Vertex is inside the area of the simulation without any falloff applied. */
      if (sim_falloff < 1.0f) {
        /* Create constraints with more strength the closer the vertex is to the simulation
         * boundary. */
        cloth_brush_add_pin_constraint(data->cloth_sim, node_index, vd.index, 1.0f - sim_falloff);
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void cloth_brush_constraint_pos_to_line(SculptClothSimulation *cloth_sim, const int v)
{
  if (!USE_SOLVER_RIPPLE_CONSTRAINT) {
    return;
  }
  float line_points[2][3];
  copy_v3_v3(line_points[0], cloth_sim->init_pos[v]);
  add_v3_v3v3(line_points[1], cloth_sim->init_pos[v], cloth_sim->init_normal[v]);
  closest_to_line_v3(cloth_sim->pos[v], cloth_sim->pos[v], line_points[0], line_points[1]);
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

  const bool use_falloff_plane = brush->cloth_force_falloff_type ==
                                 BRUSH_CLOTH_FORCE_FALLOFF_PLANE;

  PBVHVertexIter vd;
  const float bstrength = ss->cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  /* For Pinch Perpendicular Deform Type. */
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

  KelvinletParams params;
  const float kv_force = 1.0f;
  const float kv_shear_modulus = 1.0f;
  const float kv_poisson_ratio = 0.4f;
  bool use_elastic_drag = false;
  if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_ELASTIC_DRAG) {
    BKE_kelvinlet_init_params(
        &params, ss->cache->radius, kv_force, kv_shear_modulus, kv_poisson_ratio);
    use_elastic_drag = true;
  }

  /* Gravity */
  float gravity[3] = {0.0f};
  if (ss->cache->supports_gravity) {
    madd_v3_v3fl(gravity, ss->cache->gravity_direction, -data->sd->gravity_factor);
  }

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    float force[3];
    float sim_location[3];
    cloth_brush_simulation_location_get(ss, brush, sim_location);
    const float sim_factor = cloth_brush_simulation_falloff_get(
        cloth_sim, brush, ss->cache->radius, sim_location, cloth_sim->init_pos[vd.index]);

    float current_vertex_location[3];
    if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
      copy_v3_v3(current_vertex_location, ss->cache->cloth_sim->init_pos[vd.index]);
    }
    else {
      copy_v3_v3(current_vertex_location, vd.co);
    }

    /* Apply gravity in the entire simulation area. */
    float vertex_gravity[3];
    mul_v3_v3fl(vertex_gravity, gravity, sim_factor);
    cloth_brush_apply_force_to_vertex(ss, ss->cache->cloth_sim, vertex_gravity, vd.index);

    /* When using the plane falloff mode the falloff is not constrained by the brush radius. */
    /* Brushes that use elastic deformation are also not constrained by radius. */
    if (!sculpt_brush_test_sq_fn(&test, current_vertex_location) && !use_falloff_plane &&
        !use_elastic_drag) {
      continue;
    }

    float dist = sqrtf(test.dist);

    if (use_falloff_plane) {
      dist = dist_to_plane_v3(current_vertex_location, deform_plane);
    }

    const float fade = sim_factor * bstrength *
                       SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    current_vertex_location,
                                                    dist,
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
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
        madd_v3_v3v3fl(cloth_sim->deformation_pos[vd.index],
                       cloth_sim->init_pos[vd.index],
                       ss->cache->grab_delta_symmetry,
                       fade);
        if (use_falloff_plane) {
          cloth_sim->deformation_strength[vd.index] = clamp_f(fade, 0.0f, 1.0f);
        }
        else {
          cloth_sim->deformation_strength[vd.index] = 1.0f;
        }
        zero_v3(force);
        break;
      case BRUSH_CLOTH_DEFORM_SNAKE_HOOK:
        copy_v3_v3(cloth_sim->deformation_pos[vd.index], cloth_sim->pos[vd.index]);
        madd_v3_v3fl(cloth_sim->deformation_pos[vd.index], ss->cache->grab_delta_symmetry, fade);
        cloth_sim->deformation_strength[vd.index] = fade;
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
      case BRUSH_CLOTH_DEFORM_ELASTIC_DRAG: {
        float final_disp[3];
        sub_v3_v3v3(brush_disp, ss->cache->location, ss->cache->last_location);
        mul_v3_v3fl(final_disp, brush_disp, ss->cache->bstrength);
        float location[3];
        if (use_falloff_plane) {
          closest_to_plane_v3(location, deform_plane, vd.co);
        }
        else {
          copy_v3_v3(location, ss->cache->location);
        }
        BKE_kelvinlet_grab_triscale(final_disp, &params, vd.co, location, brush_disp);
        mul_v3_fl(final_disp, 20.0f * (1.0f - fade));
        add_v3_v3(cloth_sim->pos[vd.index], final_disp);
        zero_v3(force);
      }

      break;
    }

    cloth_brush_apply_force_to_vertex(ss, ss->cache->cloth_sim, force, vd.index);
  }
  BKE_pbvh_vertex_iter_end;
}

static ListBase *cloth_brush_collider_cache_create(Depsgraph *depsgraph)
{
  ListBase *cache = NULL;
  DEG_OBJECT_ITER_BEGIN (depsgraph,
                         ob,
                         DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | DEG_ITER_OBJECT_FLAG_VISIBLE |
                             DEG_ITER_OBJECT_FLAG_DUPLI) {
    CollisionModifierData *cmd = (CollisionModifierData *)BKE_modifiers_findby_type(
        ob, eModifierType_Collision);
    if (!cmd) {
      continue;
    }

    if (!cmd->bvhtree) {
      continue;
    }
    if (cache == NULL) {
      cache = MEM_callocN(sizeof(ListBase), "ColliderCache array");
    }

    ColliderCache *col = MEM_callocN(sizeof(ColliderCache), "ColliderCache");
    col->ob = ob;
    col->collmd = cmd;
    collision_move_object(cmd, 1.0, 0.0, true);
    BLI_addtail(cache, col);
  }
  DEG_OBJECT_ITER_END;
  return cache;
}

typedef struct ClothBrushCollision {
  CollisionModifierData *col_data;
  struct IsectRayPrecalc isect_precalc;
} ClothBrushCollision;

static void cloth_brush_collision_cb(void *userdata,
                                     int index,
                                     const BVHTreeRay *ray,
                                     BVHTreeRayHit *hit)
{
  ClothBrushCollision *col = (ClothBrushCollision *)userdata;
  CollisionModifierData *col_data = col->col_data;
  MVertTri *verttri = &col_data->tri[index];
  MVert *mverts = col_data->x;
  float *tri[3], no[3], co[3];

  tri[0] = mverts[verttri->tri[0]].co;
  tri[1] = mverts[verttri->tri[1]].co;
  tri[2] = mverts[verttri->tri[2]].co;
  float dist = 0.0f;

  bool tri_hit = isect_ray_tri_watertight_v3(
      ray->origin, &col->isect_precalc, UNPACK3(tri), &dist, NULL);
  normal_tri_v3(no, UNPACK3(tri));
  madd_v3_v3v3fl(co, ray->origin, ray->direction, dist);

  if (tri_hit && dist < hit->dist) {
    hit->index = index;
    hit->dist = dist;

    copy_v3_v3(hit->co, co);
    copy_v3_v3(hit->no, no);
  }
}

static void cloth_brush_solve_collision(Object *object,
                                        SculptClothSimulation *cloth_sim,
                                        const int i)
{
  const int raycast_flag = BVH_RAYCAST_DEFAULT & ~(BVH_RAYCAST_WATERTIGHT);

  ColliderCache *collider_cache;
  BVHTreeRayHit hit;

  float obmat_inv[4][4];
  invert_m4_m4(obmat_inv, object->obmat);

  for (collider_cache = cloth_sim->collider_list->first; collider_cache;
       collider_cache = collider_cache->next) {
    float ray_start[3], ray_normal[3];
    float pos_world_space[3], prev_pos_world_space[3];

    mul_v3_m4v3(pos_world_space, object->obmat, cloth_sim->pos[i]);
    mul_v3_m4v3(prev_pos_world_space, object->obmat, cloth_sim->last_iteration_pos[i]);
    sub_v3_v3v3(ray_normal, pos_world_space, prev_pos_world_space);
    copy_v3_v3(ray_start, prev_pos_world_space);
    hit.index = -1;
    hit.dist = len_v3(ray_normal);
    normalize_v3(ray_normal);

    ClothBrushCollision col;
    CollisionModifierData *collmd = collider_cache->collmd;
    col.col_data = collmd;
    isect_ray_tri_watertight_v3_precalc(&col.isect_precalc, ray_normal);

    BLI_bvhtree_ray_cast_ex(collmd->bvhtree,
                            ray_start,
                            ray_normal,
                            0.3f,
                            &hit,
                            cloth_brush_collision_cb,
                            &col,
                            raycast_flag);

    if (hit.index == -1) {
      continue;
    }

    float collision_disp[3];
    float movement_disp[3];
    mul_v3_v3fl(collision_disp, hit.no, 0.005f);
    sub_v3_v3v3(movement_disp, pos_world_space, prev_pos_world_space);
    float friction_plane[4];
    float pos_on_friction_plane[3];
    plane_from_point_normal_v3(friction_plane, hit.co, hit.no);
    closest_to_plane_v3(pos_on_friction_plane, friction_plane, pos_world_space);
    sub_v3_v3v3(movement_disp, pos_on_friction_plane, hit.co);

    /* TODO(pablodp606): This can be exposed in a brush/filter property as friction. */
    mul_v3_fl(movement_disp, 0.35f);

    copy_v3_v3(cloth_sim->pos[i], hit.co);
    add_v3_v3(cloth_sim->pos[i], movement_disp);
    add_v3_v3(cloth_sim->pos[i], collision_disp);
    mul_v3_m4v3(cloth_sim->pos[i], obmat_inv, cloth_sim->pos[i]);
  }
}
static void cloth_simulation_noise_get(float *r_noise,
                                       SculptSession *ss,
                                       const SculptVertRef vertex,
                                       const float strength)
{
  const uint *hash_co = (const uint *)SCULPT_vertex_co_get(ss, vertex);
  for (int i = 0; i < 3; i++) {
    const uint hash = BLI_hash_int_2d(hash_co[0], hash_co[1]) ^ BLI_hash_int_2d(hash_co[2], i);
    r_noise[i] = (hash * (1.0f / 0xFFFFFFFF) - 0.5f) * strength;
  }
}

typedef struct SculptClothTaskData {
  SculptClothConstraint **constraints[TOT_CONSTRAINT_TYPES];
  int tot_constraints[TOT_CONSTRAINT_TYPES];
} SculptClothTaskData;

static void do_cloth_brush_solve_simulation_task_cb_ex(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;

  PBVHNode *node = data->nodes[n];
  PBVHVertexIter vd;
  SculptClothSimulation *cloth_sim = data->cloth_sim;
  const float time_step = data->cloth_time_step;

  const int node_index = POINTER_AS_INT(BLI_ghash_lookup(data->cloth_sim->node_state_index, node));
  if (data->cloth_sim->node_state[node_index] != SCULPT_CLOTH_NODE_ACTIVE) {
    return;
  }

  AutomaskingCache *automasking = SCULPT_automasking_active_cache_get(ss);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    float sim_location[3];
    cloth_brush_simulation_location_get(ss, brush, sim_location);
    const float sim_factor =
        ss->cache ?
            cloth_brush_simulation_falloff_get(
                cloth_sim, brush, ss->cache->radius, sim_location, cloth_sim->init_pos[vd.index]) :
            1.0f;
    if (sim_factor <= 0.0f) {
      continue;
    }

    int i = vd.index;
    float temp[3];
    copy_v3_v3(temp, cloth_sim->pos[i]);

    mul_v3_fl(cloth_sim->acceleration[i], time_step);

    float pos_diff[3];
    sub_v3_v3v3(pos_diff, cloth_sim->pos[i], cloth_sim->prev_pos[i]);
    mul_v3_fl(pos_diff, (1.0f - cloth_sim->damping) * sim_factor);

    const float mask_v = (1.0f - (vd.mask ? *vd.mask : 0.0f)) *
                         SCULPT_automasking_factor_get(automasking, ss, vd.vertex);

    madd_v3_v3fl(cloth_sim->pos[i], pos_diff, mask_v);
    madd_v3_v3fl(cloth_sim->pos[i], cloth_sim->acceleration[i], mask_v);

    /* Prevents the vertices from sliding without creating folds when all vertices and forces are
     * in the same plane. */
    float noise[3];
    cloth_simulation_noise_get(noise, ss, vd.vertex, 0.000001f);
    add_v3_v3(cloth_sim->pos[i], noise);

    if (USE_SOLVER_RIPPLE_CONSTRAINT) {
      cloth_brush_constraint_pos_to_line(cloth_sim, i);
    }

    if (cloth_sim->collider_list != NULL) {
      cloth_brush_solve_collision(data->ob, cloth_sim, i);
    }

    copy_v3_v3(cloth_sim->last_iteration_pos[i], cloth_sim->pos[i]);

    copy_v3_v3(cloth_sim->prev_pos[i], temp);
    copy_v3_v3(cloth_sim->last_iteration_pos[i], cloth_sim->pos[i]);
    copy_v3_fl(cloth_sim->acceleration[i], 0.0f);

    copy_v3_v3(vd.co, cloth_sim->pos[vd.index]);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  /* Disable the simulation on this node, it needs to be enabled again to continue. */
  cloth_sim->node_state[node_index] = SCULPT_CLOTH_NODE_INACTIVE;
}

static void cloth_free_tasks(SculptClothSimulation *cloth_sim)
{
  // printf("Freeing tasks %d\n", BLI_task_parallel_thread_id(NULL));

  for (int i = 0; i < cloth_sim->tot_constraint_tasks; i++) {
    for (int j = 0; j < TOT_CONSTRAINT_TYPES; j++) {
      MEM_SAFE_FREE(cloth_sim->constraint_tasks[i].constraints[j]);
    }
  }

  MEM_SAFE_FREE(cloth_sim->constraint_tasks);
  cloth_sim->constraint_tasks = NULL;
  cloth_sim->tot_constraint_tasks = 0;
}

static void cloth_sort_constraints_for_tasks(SculptSession *ss,
                                             Brush *brush,
                                             SculptClothSimulation *cloth_sim,
                                             int totthread)
{
  SculptClothTaskData *tasks = MEM_calloc_arrayN(
      totthread + 1, sizeof(SculptClothTaskData), "SculptClothTaskData");

  int *vthreads = MEM_calloc_arrayN(
      SCULPT_vertex_count_get(ss), sizeof(*vthreads), "cloth vthreads");

  SculptClothConstraint *con;
  int totcon;

  RNG *rng = BLI_rng_new(BLI_thread_rand(0));

  bool not_dynamic = cloth_sim->simulation_area_type != BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC;

  // for (int ctype = 0; ctype < TOT_CONSTRAINT_TYPES; ctype++) {
  /* start with bending constraints since they have more vertices */
  for (int ctype = TOT_CONSTRAINT_TYPES - 1; ctype >= 0; ctype--) {
    con = cloth_sim->constraints[ctype];
    totcon = cloth_sim->tot_constraints[ctype];

    int size = (int)constraint_types[ctype].size;
    int totelem = constraint_types[ctype].totelem;

#if 0
    int *order = MEM_malloc_arrayN(totcon, sizeof(*order), "rand order");
    for (int i = 0; i < totcon; i++) {
      order[i] = i;
    }
    BLI_rng_shuffle_array(rng, order, sizeof(*order), totcon);
#endif

    char *ptr = (char *)cloth_sim->constraints[ctype];

    for (int _i = 0; _i < totcon; _i++) {
      int i = _i;  // order[_i];
      con = (SculptClothConstraint *)(ptr + size * i);

      bool ok = true;
      int last = 0, same = true;

      for (int j = 0; j < totelem; j++) {
        int threadnr = vthreads[UNPACK_POS_INDEX(con->elems[j].index)];

        if (threadnr) {
          ok = false;
        }

        if (j > 0 && threadnr && last && threadnr != last) {
          same = false;
        }

        if (threadnr) {
          last = threadnr;
        }
      }

      int tasknr;
      if (ok) {
        tasknr = BLI_rng_get_int(rng) % totthread;

        for (int j = 0; j < totelem; j++) {
          vthreads[UNPACK_POS_INDEX(con->elems[j].index)] = tasknr + 1;
        }
      }
      else if (same) {
        tasknr = last - 1;

        for (int j = 0; j < totelem; j++) {
          vthreads[UNPACK_POS_INDEX(con->elems[j].index)] = tasknr + 1;
        }
      }
      else {
        tasks[totthread].tot_constraints[ctype]++;

        con->thread_nr = -1;
        continue;
      }

      con->thread_nr = tasknr;
      tasks[tasknr].tot_constraints[ctype]++;

      /*propagate thread nr to adjacent verts, unless in
        dynamic mode where the performance benefits are
        not worth it*/
      for (int step = 0; not_dynamic && step < totelem; step++) {
        int v = UNPACK_POS_INDEX(con->elems[step].index);

        SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, v);
        SculptVertexNeighborIter ni;

        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
          if (!vthreads[ni.index]) {
            vthreads[ni.index] = tasknr + 1;
          }
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
      }
    }
  }

  int tottask = totthread + 1;
  for (int i = 0; i < tottask; i++) {
    for (int j = 0; j < TOT_CONSTRAINT_TYPES; j++) {
      tasks[i].constraints[j] = MEM_malloc_arrayN(
          tasks[i].tot_constraints[j], sizeof(void *), "cloth task data");
      tasks[i].tot_constraints[j] = 0;
    }
  }

  for (int ctype = 0; ctype < TOT_CONSTRAINT_TYPES; ctype++) {
    con = cloth_sim->constraints[ctype];
    totcon = cloth_sim->tot_constraints[ctype];
    int size = (int)constraint_types[ctype].size;

    for (int i = 0; i < totcon; i++, con = (SculptClothConstraint *)(((char *)con) + size)) {
      int tasknr = con->thread_nr;
      if (tasknr == -1) {
        tasknr = totthread;
      }

      tasks[tasknr].constraints[ctype][tasks[tasknr].tot_constraints[ctype]++] = con;
    }
  }

  BLI_rng_free(rng);

  cloth_sim->constraint_tasks = tasks;
  cloth_sim->tot_constraint_tasks = totthread + 1;

#if 1
  unsigned int size = sizeof(SculptClothLengthConstraint) * cloth_sim->tot_constraints[0];
  size += sizeof(SculptClothBendConstraint) * cloth_sim->tot_constraints[1];

  printf("%.2fmb", (float)size / 1024.0f / 1024.0f);

  for (int i = 0; i < totthread + 1; i++) {
    printf("%d: ", i);

    for (int j = 0; j < TOT_CONSTRAINT_TYPES; j++) {
      printf("  %d", tasks[i].tot_constraints[j]);
    }

    printf("\n");
  }
#endif

  MEM_SAFE_FREE(vthreads);
}

static void cloth_brush_satisfy_constraints_intern(SculptSession *ss,
                                                   Brush *brush,
                                                   SculptClothSimulation *cloth_sim,
                                                   SculptClothTaskData *task,
                                                   bool no_boundary)
{

  AutomaskingCache *automasking = SCULPT_automasking_active_cache_get(ss);
  SculptClothLengthConstraint **constraints = (SculptClothLengthConstraint **)
                                                  task->constraints[CON_LENGTH];

#ifdef BENDING_CONSTRAINTS
  SculptClothBendConstraint **bend_constraints = (SculptClothBendConstraint **)
                                                     task->constraints[CON_BEND];

  for (int i = 0; !no_boundary && i < task->tot_constraints[CON_BEND]; i++) {
    SculptClothBendConstraint *constraint = bend_constraints[i];
    float gradients[4][3];

    if (cloth_sim->node_state[constraint->node] != SCULPT_CLOTH_NODE_ACTIVE) {
      /* Skip all constraints that were created for inactive nodes. */
      continue;
    }

#  ifndef CLOTH_NO_POS_PTR
    for (int j = 0; j < 4; j++) {
      constraint->elems[j].position = cloth_sim->pos[constraint->elems[j].index];
    }
#  endif

    if (!calc_bending_gradients(cloth_sim, constraint, gradients)) {
      continue;
    }

    float sim_location[3];
    cloth_brush_simulation_location_get(ss, brush, sim_location);

    for (int j = 0; j < 4; j++) {
      int vi = UNPACK_POS_INDEX(constraint->elems[j].index);

#  ifndef CLOTH_NO_POS_PTR
      float *pos = constraint->elems[j].position;
#  else
      float *pos = GET_POS_PTR(constraint->elems[j].index);
#  endif

      float sim_factor = ss->cache ? cloth_brush_simulation_falloff_get(cloth_sim,
                                                                        brush,
                                                                        ss->cache->radius,
                                                                        sim_location,
                                                                        cloth_sim->init_pos[vi]) :
                                     1.0f;

      /* increase strength of bending constraints */
      sim_factor = sqrtf(sim_factor);

      if (sim_factor == 0.0f) {
        continue;
      }

      SculptVertRef vref = BKE_pbvh_table_index_to_vertex(ss->pbvh, vi);

      sim_factor *= SCULPT_automasking_factor_get(automasking, ss, vref) * 1.0f -
                    SCULPT_vertex_mask_get(ss, vref);

      madd_v3_v3fl(pos, gradients[j], sim_factor);

      if (USE_SOLVER_RIPPLE_CONSTRAINT) {
        cloth_brush_constraint_pos_to_line(cloth_sim, vi);
      }
    }
  }
#endif
  // return;
  for (int i = 0; i < task->tot_constraints[CON_LENGTH]; i++) {
    const SculptClothLengthConstraint *constraint = constraints[i];

    if (cloth_sim->node_state[constraint->node] != SCULPT_CLOTH_NODE_ACTIVE) {
      /* Skip all constraints that were created for inactive nodes. */
      continue;
    }

#ifndef CLOTH_NO_POS_PTR
    float *pos1 = constraint->elems[0].position;
    float *pos2 = constraint->elems[1].position;
#else
    float *pos1 = GET_POS_PTR(constraint->elems[0].index);
    float *pos2 = GET_POS_PTR(constraint->elems[1].index);
#endif

    const int v1 = UNPACK_POS_INDEX(constraint->elems[0].index);
    const int v2 = UNPACK_POS_INDEX(constraint->elems[1].index);

    const SculptVertRef v1ref = BKE_pbvh_table_index_to_vertex(ss->pbvh, v1);
    const SculptVertRef v2ref = BKE_pbvh_table_index_to_vertex(ss->pbvh, v2);

    float v1_to_v2[3];
    sub_v3_v3v3(v1_to_v2, pos2, pos1);

    const float current_distance = len_v3(v1_to_v2);
    float correction_vector[3];
    float correction_vector_half[3];

    const float constraint_distance = constraint->length +
                                      (cloth_sim->length_constraint_tweak[v1] * 0.5f) +
                                      (cloth_sim->length_constraint_tweak[v2] * 0.5f);

    if (current_distance > 0.0f) {
      mul_v3_v3fl(correction_vector,
                  v1_to_v2,
                  CLOTH_SOLVER_DISPLACEMENT_FACTOR *
                      (1.0f - (constraint_distance / current_distance)));
    }
    else {
      mul_v3_v3fl(correction_vector, v1_to_v2, CLOTH_SOLVER_DISPLACEMENT_FACTOR);
    }

    mul_v3_v3fl(correction_vector_half, correction_vector, 0.5f);

    const float mask_v1 = (1.0f - SCULPT_vertex_mask_get(ss, v1ref)) *
                          SCULPT_automasking_factor_get(automasking, ss, v1ref);
    const float mask_v2 = (1.0f - SCULPT_vertex_mask_get(ss, v2ref)) *
                          SCULPT_automasking_factor_get(automasking, ss, v2ref);

    float sim_location[3];
    cloth_brush_simulation_location_get(ss, brush, sim_location);

    const float sim_factor_v1 = ss->cache ?
                                    cloth_brush_simulation_falloff_get(cloth_sim,
                                                                       brush,
                                                                       ss->cache->radius,
                                                                       sim_location,
                                                                       cloth_sim->init_pos[v1]) :
                                    1.0f;
    const float sim_factor_v2 = ss->cache ?
                                    cloth_brush_simulation_falloff_get(cloth_sim,
                                                                       brush,
                                                                       ss->cache->radius,
                                                                       sim_location,
                                                                       cloth_sim->init_pos[v2]) :
                                    1.0f;

    float deformation_strength = 1.0f;
    if (constraint->type == SCULPT_CLOTH_CONSTRAINT_DEFORMATION) {
      deformation_strength = (cloth_sim->deformation_strength[v1] +
                              cloth_sim->deformation_strength[v2]) *
                             0.5f;
    }

    if (constraint->type == SCULPT_CLOTH_CONSTRAINT_SOFTBODY) {
      const float softbody_plasticity = brush ? brush->cloth_constraint_softbody_strength : 0.0f;
      madd_v3_v3fl(cloth_sim->pos[v1],
                   correction_vector_half,
                   1.0f * mask_v1 * sim_factor_v1 * constraint->strength * softbody_plasticity);
      madd_v3_v3fl(cloth_sim->softbody_pos[v1],
                   correction_vector_half,
                   -1.0f * mask_v1 * sim_factor_v1 * constraint->strength *
                       (1.0f - softbody_plasticity));
    }
    else {
      madd_v3_v3fl(cloth_sim->pos[v1],
                   correction_vector_half,
                   1.0f * mask_v1 * sim_factor_v1 * constraint->strength * deformation_strength);
      if (v1 != v2) {
        madd_v3_v3fl(cloth_sim->pos[v2],
                     correction_vector_half,
                     -1.0f * mask_v2 * sim_factor_v2 * constraint->strength *
                         deformation_strength);
      }
    }
    if (USE_SOLVER_RIPPLE_CONSTRAINT) {
      cloth_brush_constraint_pos_to_line(cloth_sim, v1);
      cloth_brush_constraint_pos_to_line(cloth_sim, v2);
    }
  }
}

typedef struct ConstraintThreadData {
  SculptClothSimulation *cloth_sim;
  SculptSession *ss;
  Brush *brush;
} ConstraintThreadData;

static void cloth_brush_satisfy_constraints_task_cb(void *__restrict userdata,
                                                    const int n,
                                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  ConstraintThreadData *data = (ConstraintThreadData *)userdata;

  cloth_brush_satisfy_constraints_intern(
      data->ss, data->brush, data->cloth_sim, data->cloth_sim->constraint_tasks + n, false);

  if (data->cloth_sim->use_bending) {  // run again without bend constraints
    cloth_brush_satisfy_constraints_intern(
        data->ss, data->brush, data->cloth_sim, data->cloth_sim->constraint_tasks + n, true);
  }
}

static void cloth_brush_satisfy_constraints(SculptSession *ss,
                                            Brush *brush,
                                            SculptClothSimulation *cloth_sim)
{
  ConstraintThreadData data = {.cloth_sim = cloth_sim, .ss = ss, .brush = brush};

  int totthread = BLI_system_thread_count();

  if (!cloth_sim->constraint_tasks) {
    cloth_sort_constraints_for_tasks(ss, brush, cloth_sim, totthread);
  }

  if (!cloth_sim->tot_constraint_tasks) {
    return;
  }

  int iterations = cloth_sim->use_bending ? 2 : 5;

  for (int constraint_it = 0; constraint_it < iterations; constraint_it++) {
    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, cloth_sim->tot_constraint_tasks - 1);

    BLI_task_parallel_range(0,
                            cloth_sim->tot_constraint_tasks - 1,
                            &data,
                            cloth_brush_satisfy_constraints_task_cb,
                            &settings);

    // do thread boundary constraints in main thread
    cloth_brush_satisfy_constraints_task_cb(&data, cloth_sim->tot_constraint_tasks - 1, NULL);
  }
}

void SCULPT_cloth_brush_do_simulation_step(
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
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
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

  BKE_curvemapping_init(brush->curve);

  /* Initialize the grab delta. */
  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);
  normalize_v3(grab_delta);

  apply_forces_data.grab_delta = grab_delta;

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  /* Calculate push offset. */

  if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_PUSH) {
    mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
    mul_v3_v3(offset, ss->cache->scale);
    mul_v3_fl(offset, 2.0f);

    apply_forces_data.offset = offset;
  }

  if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR ||
      brush->cloth_force_falloff_type == BRUSH_CLOTH_FORCE_FALLOFF_PLANE) {
    SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

    /* Initialize stroke local space matrix. */
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

  if (ELEM(brush->cloth_deform_type, BRUSH_CLOTH_DEFORM_SNAKE_HOOK, BRUSH_CLOTH_DEFORM_GRAB)) {
    /* Set the deformation strength to 0. Brushes will initialize the strength in the required
     * area. */
    const int totverts = SCULPT_vertex_count_get(ss);
    for (int i = 0; i < totverts; i++) {
      ss->cache->cloth_sim->deformation_strength[i] = 0.0f;
    }
  }

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(
      0, totnode, &apply_forces_data, do_cloth_brush_apply_forces_task_cb_ex, &settings);
}

/* Allocates nodes state and initializes them to Uninitialized, so constraints can be created for
 * them. */
static void cloth_sim_initialize_default_node_state(SculptSession *ss,
                                                    SculptClothSimulation *cloth_sim)
{
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  cloth_sim->node_state = MEM_malloc_arrayN(
      totnode, sizeof(eSculptClothNodeSimState), "node sim state");
  cloth_sim->node_state_index = BLI_ghash_ptr_new("node sim state indices");
  for (int i = 0; i < totnode; i++) {
    cloth_sim->node_state[i] = SCULPT_CLOTH_NODE_UNINITIALIZED;
    BLI_ghash_insert(cloth_sim->node_state_index, nodes[i], POINTER_FROM_INT(i));
  }
  MEM_SAFE_FREE(nodes);
}

/* Public functions. */
SculptClothSimulation *SCULPT_cloth_brush_simulation_create(SculptSession *ss,
                                                            Object *ob,
                                                            const float cloth_mass,
                                                            const float cloth_damping,
                                                            const float cloth_softbody_strength,
                                                            const bool use_collisions,
                                                            const bool needs_deform_coords)
{
  const int totverts = SCULPT_vertex_count_get(ss);
  SculptClothSimulation *cloth_sim;

  cloth_sim = MEM_callocN(sizeof(SculptClothSimulation), "cloth constraints");

  cloth_sim->simulation_area_type = SCULPT_get_int(ss, cloth_simulation_area_type, NULL, NULL);
  cloth_sim->sim_falloff = SCULPT_get_float(ss, cloth_sim_falloff, NULL, NULL);
  cloth_sim->sim_limit = SCULPT_get_float(ss, cloth_sim_limit, NULL, NULL);

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    if (SCULPT_has_persistent_base(ss)) {
      SCULPT_ensure_persistent_layers(ss, ob);

      cloth_sim->cd_pers_co = ss->custom_layers[SCULPT_SCL_PERS_CO]->cd_offset;
      cloth_sim->cd_pers_no = ss->custom_layers[SCULPT_SCL_PERS_NO]->cd_offset;
      cloth_sim->cd_pers_disp = ss->custom_layers[SCULPT_SCL_PERS_DISP]->cd_offset;
    }
    else {
      cloth_sim->cd_pers_co = cloth_sim->cd_pers_no = cloth_sim->cd_pers_disp = -1;
    }
  }

  for (int i = 0; i < TOT_CONSTRAINT_TYPES; i++) {
    cloth_sim->constraints[i] = MEM_callocN(
        constraint_types[i].size * CLOTH_LENGTH_CONSTRAINTS_BLOCK, "cloth constraints");
    cloth_sim->capacity_constraints[i] = CLOTH_LENGTH_CONSTRAINTS_BLOCK;
  }

  cloth_sim->acceleration = MEM_calloc_arrayN(
      totverts, sizeof(float[3]), "cloth sim acceleration");
  cloth_sim->pos = MEM_calloc_arrayN(totverts, sizeof(float[3]), "cloth sim pos");
  cloth_sim->prev_pos = MEM_calloc_arrayN(totverts, sizeof(float[3]), "cloth sim prev pos");
  cloth_sim->last_iteration_pos = MEM_calloc_arrayN(
      totverts, sizeof(float[3]), "cloth sim last iteration pos");
  cloth_sim->init_pos = MEM_calloc_arrayN(totverts, sizeof(float[3]), "cloth sim init pos");
  cloth_sim->length_constraint_tweak = MEM_calloc_arrayN(
      totverts, sizeof(float), "cloth sim length tweak");

  if (needs_deform_coords) {
    cloth_sim->deformation_pos = MEM_calloc_arrayN(
        totverts, sizeof(float[3]), "cloth sim deformation positions");
    cloth_sim->deformation_strength = MEM_calloc_arrayN(
        totverts, sizeof(float), "cloth sim deformation strength");
  }

  if (cloth_softbody_strength > 0.0f) {
    cloth_sim->softbody_pos = MEM_calloc_arrayN(
        totverts, sizeof(float[3]), "cloth sim softbody pos");
  }

  if (USE_SOLVER_RIPPLE_CONSTRAINT) {
    cloth_sim->init_normal = MEM_calloc_arrayN(totverts, sizeof(float) * 3, "init noramls");
    for (int i = 0; i < totverts; i++) {
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

      SCULPT_vertex_normal_get(ss, vertex, cloth_sim->init_normal[i]);
    }
  }

  cloth_sim->mass = cloth_mass;
  cloth_sim->damping = cloth_damping;
  cloth_sim->softbody_strength = cloth_softbody_strength;

  if (use_collisions) {
    cloth_sim->collider_list = cloth_brush_collider_cache_create(ss->depsgraph);
  }

  cloth_sim_initialize_default_node_state(ss, cloth_sim);

  return cloth_sim;
}

void SCULPT_cloth_brush_ensure_nodes_constraints(
    Sculpt *sd,
    Object *ob,
    PBVHNode **nodes,
    int totnode,
    SculptClothSimulation *cloth_sim,
    /* Cannot be `const`, because it is assigned to a `non-const` variable.
     * NOLINTNEXTLINE: readability-non-const-parameter. */
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

  cloth_sim->created_length_constraints = BLI_edgeset_new("created length constraints");
  cloth_sim->created_bend_constraints = BLI_edgeset_new("created bend constraints");

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

  BLI_edgeset_free(cloth_sim->created_length_constraints);
  BLI_edgeset_free(cloth_sim->created_bend_constraints);
}

void SCULPT_cloth_brush_simulation_init(SculptSession *ss, SculptClothSimulation *cloth_sim)
{
  const int totverts = SCULPT_vertex_count_get(ss);
  const bool has_deformation_pos = cloth_sim->deformation_pos != NULL;
  const bool has_softbody_pos = cloth_sim->softbody_pos != NULL;
  SCULPT_vertex_random_access_ensure(ss);

  for (int i = 0; i < totverts; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    copy_v3_v3(cloth_sim->last_iteration_pos[i], SCULPT_vertex_co_get(ss, vertex));
    copy_v3_v3(cloth_sim->init_pos[i], SCULPT_vertex_co_get(ss, vertex));
    copy_v3_v3(cloth_sim->prev_pos[i], SCULPT_vertex_co_get(ss, vertex));
    if (has_deformation_pos) {
      copy_v3_v3(cloth_sim->deformation_pos[i], SCULPT_vertex_co_get(ss, vertex));
      cloth_sim->deformation_strength[i] = 1.0f;
    }
    if (has_softbody_pos) {
      copy_v3_v3(cloth_sim->softbody_pos[i], SCULPT_vertex_co_get(ss, vertex));
    }
  }
}

void SCULPT_cloth_brush_store_simulation_state(SculptSession *ss, SculptClothSimulation *cloth_sim)
{
  const int totverts = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totverts; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    copy_v3_v3(cloth_sim->pos[i], SCULPT_vertex_co_get(ss, vertex));
  }
}

void SCULPT_cloth_sim_activate_nodes(SculptClothSimulation *cloth_sim,
                                     PBVHNode **nodes,
                                     int totnode)
{
  /* Activate the nodes inside the simulation area. */
  for (int n = 0; n < totnode; n++) {
    const int node_index = POINTER_AS_INT(BLI_ghash_lookup(cloth_sim->node_state_index, nodes[n]));
    cloth_sim->node_state[node_index] = SCULPT_CLOTH_NODE_ACTIVE;
  }
}

static void sculpt_cloth_ensure_constraints_in_simulation_area(Sculpt *sd,
                                                               Object *ob,
                                                               PBVHNode **nodes,
                                                               int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const float radius = ss->cache->initial_radius;
  const float limit = radius + (radius * SCULPT_get_float(ss, cloth_sim_limit, sd, brush));
  float sim_location[3];
  cloth_brush_simulation_location_get(ss, brush, sim_location);
  SCULPT_cloth_brush_ensure_nodes_constraints(
      sd, ob, nodes, totnode, ss->cache->cloth_sim, sim_location, limit);
}

/* Main Brush Function. */
void SCULPT_do_cloth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);

  SCULPT_vertex_random_access_ensure(ss);

  /* Brushes that use anchored strokes and restore the mesh can't rely on symmetry passes and steps
   * count as it is always the first step, so the simulation needs to be created when it does not
   * exist for this stroke. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache) || !ss->cache->cloth_sim) {

    /* The simulation structure only needs to be created on the first symmetry pass. */
    if (SCULPT_stroke_is_first_brush_step(ss->cache) || !ss->cache->cloth_sim) {
      ss->cache->cloth_sim = SCULPT_cloth_brush_simulation_create(
          ss,
          ob,
          SCULPT_get_float(ss, cloth_mass, sd, brush),
          SCULPT_get_float(ss, cloth_damping, sd, brush),
          SCULPT_get_float(ss, cloth_constraint_softbody_strength, sd, brush),
          SCULPT_get_bool(ss, cloth_use_collision, sd, brush),
          SCULPT_is_cloth_deform_brush(brush));

      ss->cache->cloth_sim->bend_stiffness = 0.5f * SCULPT_get_float(
                                                        ss, cloth_bending_stiffness, sd, brush);
      ss->cache->cloth_sim->use_bending = SCULPT_get_int(ss, cloth_solve_bending, sd, brush);
      SCULPT_cloth_brush_simulation_init(ss, ss->cache->cloth_sim);
    }

    ss->cache->cloth_sim->bend_stiffness = 0.5f * SCULPT_get_float(
                                                      ss, cloth_bending_stiffness, sd, brush);
    ss->cache->cloth_sim->use_bending = SCULPT_get_int(ss, cloth_solve_bending, sd, brush);

    if (SCULPT_get_int(ss, cloth_simulation_area_type, sd, brush) ==
        BRUSH_CLOTH_SIMULATION_AREA_LOCAL) {
      /* When using simulation a fixed local simulation area, constraints are created only using
       * the initial stroke position and initial radius (per symmetry pass) instead of per node.
       * This allows to skip unnecessary constraints that will never be simulated, making the
       * solver faster. When the simulation starts for a node, the node gets activated and all its
       * constraints are considered final. As the same node can be included inside the brush radius
       * from multiple symmetry passes, the cloth brush can't activate the node for simulation yet
       * as this will cause the ensure constraints function to skip the node in the next symmetry
       * passes. It needs to build the constraints here and skip simulating the first step, so all
       * passes can add their constraints to all affected nodes. */
      sculpt_cloth_ensure_constraints_in_simulation_area(sd, ob, nodes, totnode);
    }
    /* The first step of a symmetry pass is never simulated as deformation modes need valid delta
     * for brush tip alignment. */
    return;
  }

  /* Ensure the constraints for the nodes. */
  sculpt_cloth_ensure_constraints_in_simulation_area(sd, ob, nodes, totnode);

  // reassign constraints to threads if in dynamic mode
  if (SCULPT_get_int(ss, cloth_simulation_area_type, sd, brush) ==
      BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC) {
    cloth_free_tasks(ss->cache->cloth_sim);
  }

  /* Store the initial state in the simulation. */
  SCULPT_cloth_brush_store_simulation_state(ss, ss->cache->cloth_sim);

  /* Enable the nodes that should be simulated. */
  SCULPT_cloth_sim_activate_nodes(ss->cache->cloth_sim, nodes, totnode);

  /* Apply forces to the vertices. */
  cloth_brush_apply_brush_foces(sd, ob, nodes, totnode);

  /* Update and write the simulation to the nodes. */
  SCULPT_cloth_brush_do_simulation_step(sd, ob, ss->cache->cloth_sim, nodes, totnode);
}

void SCULPT_cloth_simulation_free(struct SculptClothSimulation *cloth_sim)
{
  MEM_SAFE_FREE(cloth_sim->pos);
  MEM_SAFE_FREE(cloth_sim->last_iteration_pos);
  MEM_SAFE_FREE(cloth_sim->prev_pos);
  MEM_SAFE_FREE(cloth_sim->acceleration);
  for (int i = 0; i < TOT_CONSTRAINT_TYPES; i++) {
    MEM_SAFE_FREE(cloth_sim->constraints[i]);
  }
  MEM_SAFE_FREE(cloth_sim->length_constraint_tweak);
  MEM_SAFE_FREE(cloth_sim->deformation_pos);
  MEM_SAFE_FREE(cloth_sim->softbody_pos);
  MEM_SAFE_FREE(cloth_sim->init_pos);
  MEM_SAFE_FREE(cloth_sim->deformation_strength);
  MEM_SAFE_FREE(cloth_sim->node_state);
  cloth_free_tasks(cloth_sim);
  MEM_SAFE_FREE(cloth_sim->init_normal);
  BLI_ghash_free(cloth_sim->node_state_index, NULL, NULL);
  if (cloth_sim->collider_list) {
    BKE_collider_cache_free(&cloth_sim->collider_list);
  }
  MEM_SAFE_FREE(cloth_sim);
}

/* Cursor drawing function. */
void SCULPT_cloth_simulation_limits_draw(const SculptSession *ss,
                                         const Sculpt *sd,
                                         const uint gpuattr,
                                         const Brush *brush,
                                         const float location[3],
                                         const float normal[3],
                                         const float rds,
                                         const float line_width,
                                         const float outline_col[3],
                                         const float alpha)
{
  float cursor_trans[4][4], cursor_rot[4][4];
  const float z_axis[4] = {0.0f, 0.0f, 1.0f, 0.0f};
  float quat[4];
  unit_m4(cursor_trans);
  translate_m4(cursor_trans, location[0], location[1], location[2]);
  rotation_between_vecs_to_quat(quat, z_axis, normal);
  quat_to_mat4(cursor_rot, quat);
  GPU_matrix_push();
  GPU_matrix_mul(cursor_trans);
  GPU_matrix_mul(cursor_rot);

  GPU_line_width(line_width);
  immUniformColor3fvAlpha(outline_col, alpha * 0.5f);
  imm_draw_circle_dashed_3d(gpuattr,
                            0,
                            0,
                            rds + (rds * SCULPT_get_float(ss, cloth_sim_limit, sd, brush) *
                                   SCULPT_get_float(ss, cloth_sim_falloff, sd, brush)),
                            320);
  immUniformColor3fvAlpha(outline_col, alpha * 0.7f);
  imm_draw_circle_wire_3d(
      gpuattr, 0, 0, rds + rds * SCULPT_get_float(ss, cloth_sim_limit, sd, brush), 80);
  GPU_matrix_pop();
}

void SCULPT_cloth_plane_falloff_preview_draw(const uint gpuattr,
                                             SculptSession *ss,
                                             const float outline_col[3],
                                             float outline_alpha)
{
  float local_mat[4][4];
  copy_m4_m4(local_mat, ss->cache->stroke_local_mat);

  if (SCULPT_get_int(ss, cloth_deform_type, NULL, ss->cache->brush) == BRUSH_CLOTH_DEFORM_GRAB) {
    add_v3_v3v3(local_mat[3], ss->cache->true_location, ss->cache->grab_delta);
  }

  GPU_matrix_mul(local_mat);

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
  CLOTH_FILTER_SCALE,
} eSculptClothFilterType;

static EnumPropertyItem prop_cloth_filter_type[] = {
    {CLOTH_FILTER_GRAVITY, "GRAVITY", 0, "Gravity", "Applies gravity to the simulation"},
    {CLOTH_FILTER_INFLATE, "INFLATE", 0, "Inflate", "Inflates the cloth"},
    {CLOTH_FILTER_EXPAND, "EXPAND", 0, "Expand", "Expands the cloth's dimensions"},
    {CLOTH_FILTER_PINCH, "PINCH", 0, "Pinch", "Pulls the cloth to the cursor's start position"},
    {CLOTH_FILTER_SCALE,
     "SCALE",
     0,
     "Scale",
     "Scales the mesh as a soft body using the origin of the object as scale"},
    {0, NULL, 0, NULL, NULL},
};

typedef enum eSculpClothFilterPinchOriginType {
  CLOTH_FILTER_PINCH_ORIGIN_CURSOR,
  CLOTH_FILTER_PINCH_ORIGIN_FACE_SET,
} eSculptClothFilterPinchOriginType;

static EnumPropertyItem prop_cloth_filter_pinch_origin_type[] = {
    {CLOTH_FILTER_PINCH_ORIGIN_CURSOR,
     "CURSOR",
     0,
     "Cursor",
     "Pinches to the location of the cursor"},
    {CLOTH_FILTER_PINCH_ORIGIN_FACE_SET,
     "FACE_SET",
     0,
     "Face Set",
     "Pinches to the average location of the Face Set"},
    {0, NULL, 0, NULL, NULL},
};

static EnumPropertyItem prop_cloth_filter_orientation_items[] = {
    {SCULPT_FILTER_ORIENTATION_LOCAL,
     "LOCAL",
     0,
     "Local",
     "Use the local axis to limit the force and set the gravity direction"},
    {SCULPT_FILTER_ORIENTATION_WORLD,
     "WORLD",
     0,
     "World",
     "Use the global axis to limit the force and set the gravity direction"},
    {SCULPT_FILTER_ORIENTATION_VIEW,
     "VIEW",
     0,
     "View",
     "Use the view axis to limit the force and set the gravity direction"},
    {0, NULL, 0, NULL, NULL},
};

typedef enum eClothFilterForceAxis {
  CLOTH_FILTER_FORCE_X = 1 << 0,
  CLOTH_FILTER_FORCE_Y = 1 << 1,
  CLOTH_FILTER_FORCE_Z = 1 << 2,
} eClothFilterForceAxis;

static EnumPropertyItem prop_cloth_filter_force_axis_items[] = {
    {CLOTH_FILTER_FORCE_X, "X", 0, "X", "Apply force in the X axis"},
    {CLOTH_FILTER_FORCE_Y, "Y", 0, "Y", "Apply force in the Y axis"},
    {CLOTH_FILTER_FORCE_Z, "Z", 0, "Z", "Apply force in the Z axis"},
    {0, NULL, 0, NULL, NULL},
};

static bool cloth_filter_is_deformation_filter(eSculptClothFilterType filter_type)
{
  return ELEM(filter_type, CLOTH_FILTER_SCALE);
}

static void cloth_filter_apply_displacement_to_deform_co(const int v_index,
                                                         const float disp[3],
                                                         FilterCache *filter_cache)
{
  float final_disp[3];
  copy_v3_v3(final_disp, disp);
  SCULPT_filter_zero_disabled_axis_components(final_disp, filter_cache);
  add_v3_v3v3(filter_cache->cloth_sim->deformation_pos[v_index],
              filter_cache->cloth_sim->init_pos[v_index],
              final_disp);
}

static void cloth_filter_apply_forces_to_vertices(const int v_index,
                                                  const float force[3],
                                                  const float gravity[3],
                                                  FilterCache *filter_cache)
{
  float final_force[3];
  copy_v3_v3(final_force, force);
  SCULPT_filter_zero_disabled_axis_components(final_force, filter_cache);
  add_v3_v3(final_force, gravity);
  cloth_brush_apply_force_to_vertex(NULL, filter_cache->cloth_sim, final_force, v_index);
}

static void cloth_filter_apply_forces_task_cb(void *__restrict userdata,
                                              const int i,
                                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  Sculpt *sd = data->sd;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(data->ob);

  SculptClothSimulation *cloth_sim = ss->filter_cache->cloth_sim;

  const eSculptClothFilterType filter_type = data->filter_type;
  const bool is_deformation_filter = cloth_filter_is_deformation_filter(filter_type);

  float sculpt_gravity[3] = {0.0f};
  if (sd->gravity_object) {
    copy_v3_v3(sculpt_gravity, sd->gravity_object->obmat[2]);
  }
  else {
    sculpt_gravity[2] = -1.0f;
  }
  mul_v3_fl(sculpt_gravity, sd->gravity_factor * data->filter_strength);

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[i], SCULPT_UNDO_COORDS);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    float fade = vd.mask ? *vd.mask : 0.0f;
    fade *= SCULPT_automasking_factor_get(ss->filter_cache->automasking, ss, vd.vertex);
    fade = 1.0f - fade;
    float force[3] = {0.0f, 0.0f, 0.0f};
    float disp[3], temp[3], transform[3][3];

    if (ss->filter_cache->active_face_set != SCULPT_FACE_SET_NONE) {
      if (!SCULPT_vertex_has_face_set(ss, vd.vertex, ss->filter_cache->active_face_set)) {
        continue;
      }
    }

    switch (filter_type) {
      case CLOTH_FILTER_GRAVITY:
        if (ss->filter_cache->orientation == SCULPT_FILTER_ORIENTATION_VIEW) {
          /* When using the view orientation apply gravity in the -Y axis, this way objects will
           * fall down instead of backwards. */
          force[1] = -data->filter_strength * fade;
        }
        else {
          force[2] = -data->filter_strength * fade;
        }
        SCULPT_filter_to_object_space(force, ss->filter_cache);
        break;
      case CLOTH_FILTER_INFLATE: {
        float normal[3];
        SCULPT_vertex_normal_get(ss, vd.vertex, normal);
        mul_v3_v3fl(force, normal, fade * data->filter_strength);
      } break;
      case CLOTH_FILTER_EXPAND:
        cloth_sim->length_constraint_tweak[vd.index] += fade * data->filter_strength * 0.01f;
        zero_v3(force);
        break;
      case CLOTH_FILTER_PINCH: {
        char symm_area = SCULPT_get_vertex_symm_area(orig_data.co);
        float pinch_point[3];
        copy_v3_v3(pinch_point, ss->filter_cache->cloth_sim_pinch_point);
        SCULPT_flip_v3_by_symm_area(
            pinch_point, symm, symm_area, ss->filter_cache->cloth_sim_pinch_point);
        sub_v3_v3v3(force, pinch_point, vd.co);
        normalize_v3(force);
        mul_v3_fl(force, fade * data->filter_strength);
        break;
      }
      case CLOTH_FILTER_SCALE:
        unit_m3(transform);
        scale_m3_fl(transform, 1.0f + (fade * data->filter_strength));
        copy_v3_v3(temp, cloth_sim->init_pos[vd.index]);
        mul_m3_v3(transform, temp);
        sub_v3_v3v3(disp, temp, cloth_sim->init_pos[vd.index]);
        zero_v3(force);

        break;
    }

    if (is_deformation_filter) {
      cloth_filter_apply_displacement_to_deform_co(vd.index, disp, ss->filter_cache);
    }
    else {
      cloth_filter_apply_forces_to_vertices(vd.index, force, sculpt_gravity, ss->filter_cache);
    }
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
    SCULPT_filter_cache_free(ss, ob);
    SCULPT_undo_push_end(ob);
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  const float len = event->prev_click_xy[0] - event->xy[0];
  filter_strength = filter_strength * -len * 0.001f * UI_DPI_FAC;

  SCULPT_vertex_random_access_ensure(ss);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  const int totverts = SCULPT_vertex_count_get(ss);

  for (int i = 0; i < totverts; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    copy_v3_v3(ss->filter_cache->cloth_sim->pos[i], SCULPT_vertex_co_get(ss, vertex));
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
      .filter_type = filter_type,
      .filter_strength = filter_strength,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, ss->filter_cache->totnode);
  BLI_task_parallel_range(
      0, ss->filter_cache->totnode, &data, cloth_filter_apply_forces_task_cb, &settings);

  /* Activate all nodes. */
  SCULPT_cloth_sim_activate_nodes(
      ss->filter_cache->cloth_sim, ss->filter_cache->nodes, ss->filter_cache->totnode);

  /* Update and write the simulation to the nodes. */
  SCULPT_cloth_brush_do_simulation_step(
      sd, ob, ss->filter_cache->cloth_sim, ss->filter_cache->nodes, ss->filter_cache->totnode);

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, ob, true);
  }
  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
  return OPERATOR_RUNNING_MODAL;
}

static void sculpt_cloth_filter_face_set_pinch_origin_calculate(float r_pinch_origin[3],
                                                                SculptSession *ss)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  const int active_face_set = SCULPT_active_face_set_get(ss);
  float accum[3] = {0.0f};
  int tot = 0;
  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    if (!SCULPT_vertex_has_face_set(ss, vertex, active_face_set)) {
      continue;
    }
    add_v3_v3(accum, SCULPT_vertex_co_get(ss, vertex));
    tot++;
  }
  if (tot > 0) {
    mul_v3_v3fl(r_pinch_origin, accum, 1.0f / tot);
  }
  else {
    copy_v3_v3(r_pinch_origin, SCULPT_active_vertex_co_get(ss));
  }
}

static int sculpt_cloth_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;

  const eSculptClothFilterType filter_type = RNA_enum_get(op->ptr, "type");

  /* Update the active vertex */
  float mouse[2];
  SculptCursorGeometryInfo sgi;
  mouse[0] = event->mval[0];
  mouse[1] = event->mval[1];
  SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false, false);

  SCULPT_vertex_random_access_ensure(ss);

  /* Needs mask data to be available as it is used when solving the constraints. */
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  SCULPT_undo_push_begin(ob, "Cloth filter");
  SCULPT_filter_cache_init(C, ob, sd, SCULPT_UNDO_COORDS);

  ss->filter_cache->automasking = SCULPT_automasking_cache_init(sd, NULL, ob);

  const float cloth_mass = RNA_float_get(op->ptr, "cloth_mass");
  const float cloth_damping = RNA_float_get(op->ptr, "cloth_damping");
  const bool use_collisions = RNA_boolean_get(op->ptr, "use_collisions");
  const int pinch_origin = RNA_enum_get(op->ptr, "pinch_origin");

  ss->filter_cache->cloth_sim = SCULPT_cloth_brush_simulation_create(
      ss,
      ob,
      cloth_mass,
      cloth_damping,
      0.0f,
      use_collisions,
      cloth_filter_is_deformation_filter(filter_type));

  ss->filter_cache->cloth_sim->use_bending = RNA_boolean_get(op->ptr, "use_bending");
  ss->filter_cache->cloth_sim->bend_stiffness = RNA_float_get(op->ptr, "bending_stiffness");

  switch (pinch_origin) {
    case CLOTH_FILTER_PINCH_ORIGIN_CURSOR:
      copy_v3_v3(ss->filter_cache->cloth_sim_pinch_point, SCULPT_active_vertex_co_get(ss));
      break;
    case CLOTH_FILTER_PINCH_ORIGIN_FACE_SET:
      sculpt_cloth_filter_face_set_pinch_origin_calculate(ss->filter_cache->cloth_sim_pinch_point,
                                                          ss);
      break;
  }

  SCULPT_cloth_brush_simulation_init(ss, ss->filter_cache->cloth_sim);

  float origin[3] = {0.0f, 0.0f, 0.0f};
  SCULPT_cloth_brush_ensure_nodes_constraints(sd,
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

  const int force_axis = RNA_enum_get(op->ptr, "force_axis");
  ss->filter_cache->enabled_force_axis[0] = force_axis & CLOTH_FILTER_FORCE_X;
  ss->filter_cache->enabled_force_axis[1] = force_axis & CLOTH_FILTER_FORCE_Y;
  ss->filter_cache->enabled_force_axis[2] = force_axis & CLOTH_FILTER_FORCE_Z;

  SculptFilterOrientation orientation = RNA_enum_get(op->ptr, "orientation");
  ss->filter_cache->orientation = orientation;

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SCULPT_OT_cloth_filter(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Filter Cloth";
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
               "Filter Type",
               "Operation that is going to be applied to the mesh");
  RNA_def_float(
      ot->srna, "strength", 1.0f, -10.0f, 10.0f, "Strength", "Filter strength", -10.0f, 10.0f);
  RNA_def_enum(ot->srna,
               "pinch_origin",
               prop_cloth_filter_pinch_origin_type,
               CLOTH_FILTER_PINCH_ORIGIN_CURSOR,
               "Pinch Origin",
               "Location that is used to direct the pinch force");
  RNA_def_enum_flag(ot->srna,
                    "force_axis",
                    prop_cloth_filter_force_axis_items,
                    CLOTH_FILTER_FORCE_X | CLOTH_FILTER_FORCE_Y | CLOTH_FILTER_FORCE_Z,
                    "Force Axis",
                    "Apply the force in the selected axis");
  RNA_def_enum(ot->srna,
               "orientation",
               prop_cloth_filter_orientation_items,
               SCULPT_FILTER_ORIENTATION_LOCAL,
               "Orientation",
               "Orientation of the axis to limit the filter force");
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
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_collisions",
                             false,
                             "Use Collisions",
                             "Collide with other collider objects in the scene");
  ot->prop = RNA_def_boolean(
      ot->srna, "use_bending", false, "Bending", "Enable bending constraints");
  ot->prop = RNA_def_float(
      ot->srna, "bending_stiffness", 0.5f, 0.0f, 1.0f, "Bending Stiffness", "", 0.0f, 1.0f);
}
