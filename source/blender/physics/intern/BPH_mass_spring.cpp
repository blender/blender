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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup bph
 */

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_cloth.h"
#include "BKE_collision.h"
#include "BKE_effect.h"
}

#include "BPH_mass_spring.h"
#include "implicit.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

static float I3[3][3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};

/* Number of off-diagonal non-zero matrix blocks.
 * Basically there is one of these for each vertex-vertex interaction.
 */
static int cloth_count_nondiag_blocks(Cloth *cloth)
{
  LinkNode *link;
  int nondiag = 0;

  for (link = cloth->springs; link; link = link->next) {
    ClothSpring *spring = (ClothSpring *)link->link;
    switch (spring->type) {
      case CLOTH_SPRING_TYPE_BENDING_HAIR:
        /* angular bending combines 3 vertices */
        nondiag += 3;
        break;

      default:
        /* all other springs depend on 2 vertices only */
        nondiag += 1;
        break;
    }
  }

  return nondiag;
}

int BPH_cloth_solver_init(Object *UNUSED(ob), ClothModifierData *clmd)
{
  Cloth *cloth = clmd->clothObject;
  ClothVertex *verts = cloth->verts;
  const float ZERO[3] = {0.0f, 0.0f, 0.0f};
  Implicit_Data *id;
  unsigned int i, nondiag;

  nondiag = cloth_count_nondiag_blocks(cloth);
  cloth->implicit = id = BPH_mass_spring_solver_create(cloth->mvert_num, nondiag);

  for (i = 0; i < cloth->mvert_num; i++) {
    BPH_mass_spring_set_vertex_mass(id, i, verts[i].mass);
  }

  for (i = 0; i < cloth->mvert_num; i++) {
    BPH_mass_spring_set_motion_state(id, i, verts[i].x, ZERO);
  }

  return 1;
}

void BPH_cloth_solver_free(ClothModifierData *clmd)
{
  Cloth *cloth = clmd->clothObject;

  if (cloth->implicit) {
    BPH_mass_spring_solver_free(cloth->implicit);
    cloth->implicit = NULL;
  }
}

void BKE_cloth_solver_set_positions(ClothModifierData *clmd)
{
  Cloth *cloth = clmd->clothObject;
  ClothVertex *verts = cloth->verts;
  unsigned int mvert_num = cloth->mvert_num, i;
  ClothHairData *cloth_hairdata = clmd->hairdata;
  Implicit_Data *id = cloth->implicit;

  for (i = 0; i < mvert_num; i++) {
    if (cloth_hairdata) {
      ClothHairData *root = &cloth_hairdata[i];
      BPH_mass_spring_set_rest_transform(id, i, root->rot);
    }
    else {
      BPH_mass_spring_set_rest_transform(id, i, I3);
    }

    BPH_mass_spring_set_motion_state(id, i, verts[i].x, verts[i].v);
  }
}

static bool collision_response(ClothModifierData *clmd,
                               CollisionModifierData *collmd,
                               CollPair *collpair,
                               float dt,
                               float restitution,
                               float r_impulse[3])
{
  Cloth *cloth = clmd->clothObject;
  int index = collpair->ap1;
  bool result = false;

  float v1[3], v2_old[3], v2_new[3], v_rel_old[3], v_rel_new[3];
  float epsilon2 = BLI_bvhtree_get_epsilon(collmd->bvhtree);

  float margin_distance = (float)collpair->distance - epsilon2;
  float mag_v_rel;

  zero_v3(r_impulse);

  if (margin_distance > 0.0f) {
    return false; /* XXX tested before already? */
  }

  /* only handle static collisions here */
  if (collpair->flag & COLLISION_IN_FUTURE) {
    return false;
  }

  /* velocity */
  copy_v3_v3(v1, cloth->verts[index].v);
  collision_get_collider_velocity(v2_old, v2_new, collmd, collpair);
  /* relative velocity = velocity of the cloth point relative to the collider */
  sub_v3_v3v3(v_rel_old, v1, v2_old);
  sub_v3_v3v3(v_rel_new, v1, v2_new);
  /* normal component of the relative velocity */
  mag_v_rel = dot_v3v3(v_rel_old, collpair->normal);

  /* only valid when moving toward the collider */
  if (mag_v_rel < -ALMOST_ZERO) {
    float v_nor_old, v_nor_new;
    float v_tan_old[3], v_tan_new[3];
    float bounce, repulse;

    /* Collision response based on
     * "Simulating Complex Hair with Robust Collision Handling" (Choe, Choi, Ko, ACM SIGGRAPH 2005)
     * http://graphics.snu.ac.kr/publications/2005-choe-HairSim/Choe_2005_SCA.pdf
     */

    v_nor_old = mag_v_rel;
    v_nor_new = dot_v3v3(v_rel_new, collpair->normal);

    madd_v3_v3v3fl(v_tan_old, v_rel_old, collpair->normal, -v_nor_old);
    madd_v3_v3v3fl(v_tan_new, v_rel_new, collpair->normal, -v_nor_new);

    bounce = -v_nor_old * restitution;

    repulse = -margin_distance / dt; /* base repulsion velocity in normal direction */
    /* XXX this clamping factor is quite arbitrary ...
     * not sure if there is a more scientific approach, but seems to give good results
     */
    CLAMP(repulse, 0.0f, 4.0f * bounce);

    if (margin_distance < -epsilon2) {
      mul_v3_v3fl(r_impulse, collpair->normal, max_ff(repulse, bounce) - v_nor_new);
    }
    else {
      bounce = 0.0f;
      mul_v3_v3fl(r_impulse, collpair->normal, repulse - v_nor_new);
    }

    result = true;
  }

  return result;
}

/* Init constraint matrix
 * This is part of the modified CG method suggested by Baraff/Witkin in
 * "Large Steps in Cloth Simulation" (Siggraph 1998)
 */
static void cloth_setup_constraints(ClothModifierData *clmd,
                                    ColliderContacts *contacts,
                                    int totcolliders,
                                    float dt)
{
  Cloth *cloth = clmd->clothObject;
  Implicit_Data *data = cloth->implicit;
  ClothVertex *verts = cloth->verts;
  int mvert_num = cloth->mvert_num;
  int i, j, v;

  const float ZERO[3] = {0.0f, 0.0f, 0.0f};

  BPH_mass_spring_clear_constraints(data);

  for (v = 0; v < mvert_num; v++) {
    if (verts[v].flags & CLOTH_VERT_FLAG_PINNED) {
      /* pinned vertex constraints */
      BPH_mass_spring_add_constraint_ndof0(data, v, ZERO); /* velocity is defined externally */
    }

    verts[v].impulse_count = 0;
  }

  for (i = 0; i < totcolliders; ++i) {
    ColliderContacts *ct = &contacts[i];
    for (j = 0; j < ct->totcollisions; ++j) {
      CollPair *collpair = &ct->collisions[j];
      // float restitution = (1.0f - clmd->coll_parms->damping) * (1.0f - ct->ob->pd->pdef_sbdamp);
      float restitution = 0.0f;
      int v = collpair->face1;
      float impulse[3];

      /* pinned verts handled separately */
      if (verts[v].flags & CLOTH_VERT_FLAG_PINNED) {
        continue;
      }

      /* XXX cheap way of avoiding instability from multiple collisions in the same step
       * this should eventually be supported ...
       */
      if (verts[v].impulse_count > 0) {
        continue;
      }

      /* calculate collision response */
      if (!collision_response(clmd, ct->collmd, collpair, dt, restitution, impulse)) {
        continue;
      }

      BPH_mass_spring_add_constraint_ndof2(data, v, collpair->normal, impulse);
      ++verts[v].impulse_count;
    }
  }
}

/* computes where the cloth would be if it were subject to perfectly stiff edges
 * (edge distance constraints) in a lagrangian solver.  then add forces to help
 * guide the implicit solver to that state.  this function is called after
 * collisions*/
static int UNUSED_FUNCTION(cloth_calc_helper_forces)(Object *UNUSED(ob),
                                                     ClothModifierData *clmd,
                                                     float (*initial_cos)[3],
                                                     float UNUSED(step),
                                                     float dt)
{
  Cloth *cloth = clmd->clothObject;
  float(*cos)[3] = (float(*)[3])MEM_callocN(sizeof(float[3]) * cloth->mvert_num,
                                            "cos cloth_calc_helper_forces");
  float *masses = (float *)MEM_callocN(sizeof(float) * cloth->mvert_num,
                                       "cos cloth_calc_helper_forces");
  LinkNode *node;
  ClothSpring *spring;
  ClothVertex *cv;
  int i, steps;

  cv = cloth->verts;
  for (i = 0; i < cloth->mvert_num; i++, cv++) {
    copy_v3_v3(cos[i], cv->tx);

    if (cv->goal == 1.0f || len_squared_v3v3(initial_cos[i], cv->tx) != 0.0f) {
      masses[i] = 1e+10;
    }
    else {
      masses[i] = cv->mass;
    }
  }

  steps = 55;
  for (i = 0; i < steps; i++) {
    for (node = cloth->springs; node; node = node->next) {
      /* ClothVertex *cv1, *cv2; */ /* UNUSED */
      int v1, v2;
      float len, c, l, vec[3];

      spring = (ClothSpring *)node->link;
      if (spring->type != CLOTH_SPRING_TYPE_STRUCTURAL &&
          spring->type != CLOTH_SPRING_TYPE_SHEAR) {
        continue;
      }

      v1 = spring->ij;
      v2 = spring->kl;
      /* cv1 = cloth->verts + v1; */ /* UNUSED */
      /* cv2 = cloth->verts + v2; */ /* UNUSED */
      len = len_v3v3(cos[v1], cos[v2]);

      sub_v3_v3v3(vec, cos[v1], cos[v2]);
      normalize_v3(vec);

      c = (len - spring->restlen);
      if (c == 0.0f) {
        continue;
      }

      l = c / ((1.0f / masses[v1]) + (1.0f / masses[v2]));

      mul_v3_fl(vec, -(1.0f / masses[v1]) * l);
      add_v3_v3(cos[v1], vec);

      sub_v3_v3v3(vec, cos[v2], cos[v1]);
      normalize_v3(vec);

      mul_v3_fl(vec, -(1.0f / masses[v2]) * l);
      add_v3_v3(cos[v2], vec);
    }
  }

  cv = cloth->verts;
  for (i = 0; i < cloth->mvert_num; i++, cv++) {
    float vec[3];

    /*compute forces*/
    sub_v3_v3v3(vec, cos[i], cv->tx);
    mul_v3_fl(vec, cv->mass * dt * 20.0f);
    add_v3_v3(cv->tv, vec);
    // copy_v3_v3(cv->tx, cos[i]);
  }

  MEM_freeN(cos);
  MEM_freeN(masses);

  return 1;
}

BLI_INLINE void cloth_calc_spring_force(ClothModifierData *clmd, ClothSpring *s)
{
  Cloth *cloth = clmd->clothObject;
  ClothSimSettings *parms = clmd->sim_parms;
  Implicit_Data *data = cloth->implicit;
  bool using_angular = parms->bending_model == CLOTH_BENDING_ANGULAR;
  bool resist_compress = (parms->flags & CLOTH_SIMSETTINGS_FLAG_RESIST_SPRING_COMPRESS) &&
                         !using_angular;

  s->flags &= ~CLOTH_SPRING_FLAG_NEEDED;

  /* Calculate force of bending springs. */
  if ((s->type & CLOTH_SPRING_TYPE_BENDING) && using_angular) {
#ifdef CLOTH_FORCE_SPRING_BEND
    float k, scaling;

    s->flags |= CLOTH_SPRING_FLAG_NEEDED;

    scaling = parms->bending + s->ang_stiffness * fabsf(parms->max_bend - parms->bending);
    k = scaling * s->restlen *
        0.1f; /* Multiplying by 0.1, just to scale the forces to more reasonable values. */

    BPH_mass_spring_force_spring_angular(
        data, s->ij, s->kl, s->pa, s->pb, s->la, s->lb, s->restang, k, parms->bending_damping);
#endif
  }

  /* Calculate force of structural + shear springs. */
  if (s->type & (CLOTH_SPRING_TYPE_STRUCTURAL | CLOTH_SPRING_TYPE_SEWING)) {
#ifdef CLOTH_FORCE_SPRING_STRUCTURAL
    float k_tension, scaling_tension;

    s->flags |= CLOTH_SPRING_FLAG_NEEDED;

    scaling_tension = parms->tension +
                      s->lin_stiffness * fabsf(parms->max_tension - parms->tension);
    k_tension = scaling_tension / (parms->avg_spring_len + FLT_EPSILON);

    if (s->type & CLOTH_SPRING_TYPE_SEWING) {
      /* TODO: verify, half verified (couldn't see error)
       * sewing springs usually have a large distance at first so clamp the force so we don't get
       * tunnelling through colission objects */
      BPH_mass_spring_force_spring_linear(data,
                                          s->ij,
                                          s->kl,
                                          s->restlen,
                                          k_tension,
                                          parms->tension_damp,
                                          0.0f,
                                          0.0f,
                                          false,
                                          false,
                                          parms->max_sewing);
    }
    else {
      float k_compression, scaling_compression;
      scaling_compression = parms->compression +
                            s->lin_stiffness * fabsf(parms->max_compression - parms->compression);
      k_compression = scaling_compression / (parms->avg_spring_len + FLT_EPSILON);

      BPH_mass_spring_force_spring_linear(data,
                                          s->ij,
                                          s->kl,
                                          s->restlen,
                                          k_tension,
                                          parms->tension_damp,
                                          k_compression,
                                          parms->compression_damp,
                                          resist_compress,
                                          using_angular,
                                          0.0f);
    }
#endif
  }
  else if (s->type & CLOTH_SPRING_TYPE_SHEAR) {
#ifdef CLOTH_FORCE_SPRING_SHEAR
    float k, scaling;

    s->flags |= CLOTH_SPRING_FLAG_NEEDED;

    scaling = parms->shear + s->lin_stiffness * fabsf(parms->max_shear - parms->shear);
    k = scaling / (parms->avg_spring_len + FLT_EPSILON);

    BPH_mass_spring_force_spring_linear(data,
                                        s->ij,
                                        s->kl,
                                        s->restlen,
                                        k,
                                        parms->shear_damp,
                                        0.0f,
                                        0.0f,
                                        resist_compress,
                                        false,
                                        0.0f);
#endif
  }
  else if (s->type & CLOTH_SPRING_TYPE_BENDING) { /* calculate force of bending springs */
#ifdef CLOTH_FORCE_SPRING_BEND
    float kb, cb, scaling;

    s->flags |= CLOTH_SPRING_FLAG_NEEDED;

    scaling = parms->bending + s->lin_stiffness * fabsf(parms->max_bend - parms->bending);
    kb = scaling / (20.0f * (parms->avg_spring_len + FLT_EPSILON));

    // Fix for [#45084] for cloth stiffness must have cb proportional to kb
    cb = kb * parms->bending_damping;

    BPH_mass_spring_force_spring_bending(data, s->ij, s->kl, s->restlen, kb, cb);
#endif
  }
  else if (s->type & CLOTH_SPRING_TYPE_BENDING_HAIR) {
#ifdef CLOTH_FORCE_SPRING_BEND
    float kb, cb, scaling;

    s->flags |= CLOTH_SPRING_FLAG_NEEDED;

    /* XXX WARNING: angular bending springs for hair apply stiffness factor as an overall factor,
     * unlike cloth springs! this is crap, but needed due to cloth/hair mixing ... max_bend factor
     * is not even used for hair, so ...
     */
    scaling = s->lin_stiffness * parms->bending;
    kb = scaling / (20.0f * (parms->avg_spring_len + FLT_EPSILON));

    // Fix for [#45084] for cloth stiffness must have cb proportional to kb
    cb = kb * parms->bending_damping;

    /* XXX assuming same restlen for ij and jk segments here,
     * this can be done correctly for hair later. */
    BPH_mass_spring_force_spring_bending_hair(data, s->ij, s->kl, s->mn, s->target, kb, cb);

#  if 0
    {
      float x_kl[3], x_mn[3], v[3], d[3];

      BPH_mass_spring_get_motion_state(data, s->kl, x_kl, v);
      BPH_mass_spring_get_motion_state(data, s->mn, x_mn, v);

      BKE_sim_debug_data_add_dot(clmd->debug_data, x_kl, 0.9, 0.9, 0.9, "target", 7980, s->kl);
      BKE_sim_debug_data_add_line(
          clmd->debug_data, x_kl, x_mn, 0.8, 0.8, 0.8, "target", 7981, s->kl);

      copy_v3_v3(d, s->target);
      BKE_sim_debug_data_add_vector(
          clmd->debug_data, x_kl, d, 0.8, 0.8, 0.2, "target", 7982, s->kl);

      // copy_v3_v3(d, s->target_ij);
      // BKE_sim_debug_data_add_vector(clmd->debug_data, x, d, 1, 0.4, 0.4, "target", 7983, s->kl);
    }
#  endif
#endif
  }
}

static void hair_get_boundbox(ClothModifierData *clmd, float gmin[3], float gmax[3])
{
  Cloth *cloth = clmd->clothObject;
  Implicit_Data *data = cloth->implicit;
  unsigned int mvert_num = cloth->mvert_num;
  int i;

  INIT_MINMAX(gmin, gmax);
  for (i = 0; i < mvert_num; i++) {
    float x[3];
    BPH_mass_spring_get_motion_state(data, i, x, NULL);
    DO_MINMAX(x, gmin, gmax);
  }
}

static void cloth_calc_force(
    Scene *scene, ClothModifierData *clmd, float UNUSED(frame), ListBase *effectors, float time)
{
  /* Collect forces and derivatives:  F, dFdX, dFdV */
  Cloth *cloth = clmd->clothObject;
  Implicit_Data *data = cloth->implicit;
  unsigned int i = 0;
  float drag = clmd->sim_parms->Cvi * 0.01f; /* viscosity of air scaled in percent */
  float gravity[3] = {0.0f, 0.0f, 0.0f};
  const MVertTri *tri = cloth->tri;
  unsigned int mvert_num = cloth->mvert_num;
  ClothVertex *vert;

#ifdef CLOTH_FORCE_GRAVITY
  /* global acceleration (gravitation) */
  if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
    /* scale gravity force */
    mul_v3_v3fl(gravity,
                scene->physics_settings.gravity,
                0.001f * clmd->sim_parms->effector_weights->global_gravity);
  }

  vert = cloth->verts;
  for (i = 0; i < cloth->mvert_num; i++, vert++) {
    BPH_mass_spring_force_gravity(data, i, vert->mass, gravity);

    /* Vertex goal springs */
    if ((!(vert->flags & CLOTH_VERT_FLAG_PINNED)) && (vert->goal > FLT_EPSILON)) {
      float goal_x[3], goal_v[3];
      float k;

      /* divide by time_scale to prevent goal vertices' delta locations from being multiplied */
      interp_v3_v3v3(goal_x, vert->xold, vert->xconst, time / clmd->sim_parms->time_scale);
      sub_v3_v3v3(goal_v, vert->xconst, vert->xold); /* distance covered over dt==1 */

      k = vert->goal * clmd->sim_parms->goalspring /
          (clmd->sim_parms->avg_spring_len + FLT_EPSILON);

      BPH_mass_spring_force_spring_goal(
          data, i, goal_x, goal_v, k, clmd->sim_parms->goalfrict * 0.01f);
    }
  }
#endif

  /* cloth_calc_volume_force(clmd); */

#ifdef CLOTH_FORCE_DRAG
  BPH_mass_spring_force_drag(data, drag);
#endif

  /* handle external forces like wind */
  if (effectors) {
    /* cache per-vertex forces to avoid redundant calculation */
    float(*winvec)[3] = (float(*)[3])MEM_callocN(sizeof(float[3]) * mvert_num, "effector forces");
    for (i = 0; i < cloth->mvert_num; i++) {
      float x[3], v[3];
      EffectedPoint epoint;

      BPH_mass_spring_get_motion_state(data, i, x, v);
      pd_point_from_loc(scene, x, v, i, &epoint);
      BKE_effectors_apply(
          effectors, NULL, clmd->sim_parms->effector_weights, &epoint, winvec[i], NULL);
    }

    for (i = 0; i < cloth->tri_num; i++) {
      const MVertTri *vt = &tri[i];
      BPH_mass_spring_force_face_wind(data, vt->tri[0], vt->tri[1], vt->tri[2], winvec);
    }

    /* Hair has only edges */
    if (cloth->tri_num == 0) {
#if 0
      ClothHairData *hairdata = clmd->hairdata;
      ClothHairData *hair_ij, *hair_kl;

      for (LinkNode *link = cloth->springs; link; link = link->next) {
        ClothSpring *spring = (ClothSpring *)link->link;
        if (spring->type == CLOTH_SPRING_TYPE_STRUCTURAL) {
          if (hairdata) {
            hair_ij = &hairdata[spring->ij];
            hair_kl = &hairdata[spring->kl];
            BPH_mass_spring_force_edge_wind(
                data, spring->ij, spring->kl, hair_ij->radius, hair_kl->radius, winvec);
          }
          else {
            BPH_mass_spring_force_edge_wind(data, spring->ij, spring->kl, 1.0f, 1.0f, winvec);
          }
        }
      }
#else
      ClothHairData *hairdata = clmd->hairdata;

      vert = cloth->verts;
      for (i = 0; i < cloth->mvert_num; i++, vert++) {
        if (hairdata) {
          ClothHairData *hair = &hairdata[i];
          BPH_mass_spring_force_vertex_wind(data, i, hair->radius, winvec);
        }
        else {
          BPH_mass_spring_force_vertex_wind(data, i, 1.0f, winvec);
        }
      }
#endif
    }

    MEM_freeN(winvec);
  }

  // calculate spring forces
  for (LinkNode *link = cloth->springs; link; link = link->next) {
    ClothSpring *spring = (ClothSpring *)link->link;
    // only handle active springs
    if (!(spring->flags & CLOTH_SPRING_FLAG_DEACTIVATE)) {
      cloth_calc_spring_force(clmd, spring);
    }
  }
}

/* returns vertexes' motion state */
BLI_INLINE void cloth_get_grid_location(Implicit_Data *data,
                                        float cell_scale,
                                        const float cell_offset[3],
                                        int index,
                                        float x[3],
                                        float v[3])
{
  BPH_mass_spring_get_position(data, index, x);
  BPH_mass_spring_get_new_velocity(data, index, v);

  mul_v3_fl(x, cell_scale);
  add_v3_v3(x, cell_offset);
}

/* returns next spring forming a continuous hair sequence */
BLI_INLINE LinkNode *hair_spring_next(LinkNode *spring_link)
{
  ClothSpring *spring = (ClothSpring *)spring_link->link;
  LinkNode *next = spring_link->next;
  if (next) {
    ClothSpring *next_spring = (ClothSpring *)next->link;
    if (next_spring->type == CLOTH_SPRING_TYPE_STRUCTURAL && next_spring->kl == spring->ij) {
      return next;
    }
  }
  return NULL;
}

/* XXX this is nasty: cloth meshes do not explicitly store
 * the order of hair segments!
 * We have to rely on the spring build function for now,
 * which adds structural springs in reverse order:
 *   (3,4), (2,3), (1,2)
 * This is currently the only way to figure out hair geometry inside this code ...
 */
static LinkNode *cloth_continuum_add_hair_segments(HairGrid *grid,
                                                   const float cell_scale,
                                                   const float cell_offset[3],
                                                   Cloth *cloth,
                                                   LinkNode *spring_link)
{
  Implicit_Data *data = cloth->implicit;
  LinkNode *next_spring_link = NULL; /* return value */
  ClothSpring *spring1, *spring2, *spring3;
  // ClothVertex *verts = cloth->verts;
  // ClothVertex *vert3, *vert4;
  float x1[3], v1[3], x2[3], v2[3], x3[3], v3[3], x4[3], v4[3];
  float dir1[3], dir2[3], dir3[3];

  spring1 = NULL;
  spring2 = NULL;
  spring3 = (ClothSpring *)spring_link->link;

  zero_v3(x1);
  zero_v3(v1);
  zero_v3(dir1);
  zero_v3(x2);
  zero_v3(v2);
  zero_v3(dir2);

  // vert3 = &verts[spring3->kl];
  cloth_get_grid_location(data, cell_scale, cell_offset, spring3->kl, x3, v3);
  // vert4 = &verts[spring3->ij];
  cloth_get_grid_location(data, cell_scale, cell_offset, spring3->ij, x4, v4);
  sub_v3_v3v3(dir3, x4, x3);
  normalize_v3(dir3);

  while (spring_link) {
    /* move on */
    spring1 = spring2;
    spring2 = spring3;

    // vert3 = vert4;

    copy_v3_v3(x1, x2);
    copy_v3_v3(v1, v2);
    copy_v3_v3(x2, x3);
    copy_v3_v3(v2, v3);
    copy_v3_v3(x3, x4);
    copy_v3_v3(v3, v4);

    copy_v3_v3(dir1, dir2);
    copy_v3_v3(dir2, dir3);

    /* read next segment */
    next_spring_link = spring_link->next;
    spring_link = hair_spring_next(spring_link);

    if (spring_link) {
      spring3 = (ClothSpring *)spring_link->link;
      // vert4 = &verts[spring3->ij];
      cloth_get_grid_location(data, cell_scale, cell_offset, spring3->ij, x4, v4);
      sub_v3_v3v3(dir3, x4, x3);
      normalize_v3(dir3);
    }
    else {
      spring3 = NULL;
      // vert4 = NULL;
      zero_v3(x4);
      zero_v3(v4);
      zero_v3(dir3);
    }

    BPH_hair_volume_add_segment(
        grid, x1, v1, x2, v2, x3, v3, x4, v4, spring1 ? dir1 : NULL, dir2, spring3 ? dir3 : NULL);
  }

  return next_spring_link;
}

static void cloth_continuum_fill_grid(HairGrid *grid, Cloth *cloth)
{
#if 0
  Implicit_Data *data = cloth->implicit;
  int mvert_num = cloth->mvert_num;
  ClothVertex *vert;
  int i;

  for (i = 0, vert = cloth->verts; i < mvert_num; i++, vert++) {
    float x[3], v[3];

    cloth_get_vertex_motion_state(data, vert, x, v);
    BPH_hair_volume_add_vertex(grid, x, v);
  }
#else
  LinkNode *link;
  float cellsize, gmin[3], cell_scale, cell_offset[3];

  /* scale and offset for transforming vertex locations into grid space
   * (cell size is 0..1, gmin becomes origin)
   */
  BPH_hair_volume_grid_geometry(grid, &cellsize, NULL, gmin, NULL);
  cell_scale = cellsize > 0.0f ? 1.0f / cellsize : 0.0f;
  mul_v3_v3fl(cell_offset, gmin, cell_scale);
  negate_v3(cell_offset);

  link = cloth->springs;
  while (link) {
    ClothSpring *spring = (ClothSpring *)link->link;
    if (spring->type == CLOTH_SPRING_TYPE_STRUCTURAL) {
      link = cloth_continuum_add_hair_segments(grid, cell_scale, cell_offset, cloth, link);
    }
    else {
      link = link->next;
    }
  }
#endif
  BPH_hair_volume_normalize_vertex_grid(grid);
}

static void cloth_continuum_step(ClothModifierData *clmd, float dt)
{
  ClothSimSettings *parms = clmd->sim_parms;
  Cloth *cloth = clmd->clothObject;
  Implicit_Data *data = cloth->implicit;
  int mvert_num = cloth->mvert_num;
  ClothVertex *vert;

  const float fluid_factor = 0.95f; /* blend between PIC and FLIP methods */
  float smoothfac = parms->velocity_smooth;
  /* XXX FIXME arbitrary factor!!! this should be based on some intuitive value instead,
   * like number of hairs per cell and time decay instead of "strength"
   */
  float density_target = parms->density_target;
  float density_strength = parms->density_strength;
  float gmin[3], gmax[3];
  int i;

  /* clear grid info */
  zero_v3_int(clmd->hair_grid_res);
  zero_v3(clmd->hair_grid_min);
  zero_v3(clmd->hair_grid_max);
  clmd->hair_grid_cellsize = 0.0f;

  hair_get_boundbox(clmd, gmin, gmax);

  /* gather velocities & density */
  if (smoothfac > 0.0f || density_strength > 0.0f) {
    HairGrid *grid = BPH_hair_volume_create_vertex_grid(
        clmd->sim_parms->voxel_cell_size, gmin, gmax);

    cloth_continuum_fill_grid(grid, cloth);

    /* main hair continuum solver */
    BPH_hair_volume_solve_divergence(grid, dt, density_target, density_strength);

    for (i = 0, vert = cloth->verts; i < mvert_num; i++, vert++) {
      float x[3], v[3], nv[3];

      /* calculate volumetric velocity influence */
      BPH_mass_spring_get_position(data, i, x);
      BPH_mass_spring_get_new_velocity(data, i, v);

      BPH_hair_volume_grid_velocity(grid, x, v, fluid_factor, nv);

      interp_v3_v3v3(nv, v, nv, smoothfac);

      /* apply on hair data */
      BPH_mass_spring_set_new_velocity(data, i, nv);
    }

    /* store basic grid info in the modifier data */
    BPH_hair_volume_grid_geometry(grid,
                                  &clmd->hair_grid_cellsize,
                                  clmd->hair_grid_res,
                                  clmd->hair_grid_min,
                                  clmd->hair_grid_max);

#if 0 /* DEBUG hair velocity vector field */
    {
      const int size = 64;
      int i, j;
      float offset[3], a[3], b[3];
      const int axis = 0;
      const float shift = 0.0f;

      copy_v3_v3(offset, clmd->hair_grid_min);
      zero_v3(a);
      zero_v3(b);

      offset[axis] = shift * clmd->hair_grid_cellsize;
      a[(axis + 1) % 3] = clmd->hair_grid_max[(axis + 1) % 3] -
                          clmd->hair_grid_min[(axis + 1) % 3];
      b[(axis + 2) % 3] = clmd->hair_grid_max[(axis + 2) % 3] -
                          clmd->hair_grid_min[(axis + 2) % 3];

      BKE_sim_debug_data_clear_category(clmd->debug_data, "grid velocity");
      for (j = 0; j < size; ++j) {
        for (i = 0; i < size; ++i) {
          float x[3], v[3], gvel[3], gvel_smooth[3], gdensity;

          madd_v3_v3v3fl(x, offset, a, (float)i / (float)(size - 1));
          madd_v3_v3fl(x, b, (float)j / (float)(size - 1));
          zero_v3(v);

          BPH_hair_volume_grid_interpolate(grid, x, &gdensity, gvel, gvel_smooth, NULL, NULL);

          // BKE_sim_debug_data_add_circle(
          //     clmd->debug_data, x, gdensity, 0.7, 0.3, 1,
          //     "grid density", i, j, 3111);
          if (!is_zero_v3(gvel) || !is_zero_v3(gvel_smooth)) {
            float dvel[3];
            sub_v3_v3v3(dvel, gvel_smooth, gvel);
            // BKE_sim_debug_data_add_vector(
            //     clmd->debug_data, x, gvel, 0.4, 0, 1,
            //     "grid velocity", i, j, 3112);
            // BKE_sim_debug_data_add_vector(
            //     clmd->debug_data, x, gvel_smooth, 0.6, 1, 1,
            //     "grid velocity", i, j, 3113);
            BKE_sim_debug_data_add_vector(
                clmd->debug_data, x, dvel, 0.4, 1, 0.7, "grid velocity", i, j, 3114);
#  if 0
            if (gdensity > 0.0f) {
              float col0[3] = {0.0, 0.0, 0.0};
              float col1[3] = {0.0, 1.0, 0.0};
              float col[3];

              interp_v3_v3v3(col, col0, col1,
                             CLAMPIS(gdensity * clmd->sim_parms->density_strength, 0.0, 1.0));
              // BKE_sim_debug_data_add_circle(
              //     clmd->debug_data, x, gdensity * clmd->sim_parms->density_strength, 0, 1, 0.4,
              //     "grid velocity", i, j, 3115);
              // BKE_sim_debug_data_add_dot(
              //     clmd->debug_data, x, col[0], col[1], col[2],
              //     "grid velocity", i, j, 3115);
              BKE_sim_debug_data_add_circle(
                  clmd->debug_data, x, 0.01f, col[0], col[1], col[2], "grid velocity", i, j, 3115);
            }
#  endif
          }
        }
      }
    }
#endif

    BPH_hair_volume_free_vertex_grid(grid);
  }
}

#if 0
static void cloth_calc_volume_force(ClothModifierData *clmd)
{
  ClothSimSettings *parms = clmd->sim_parms;
  Cloth *cloth = clmd->clothObject;
  Implicit_Data *data = cloth->implicit;
  int mvert_num = cloth->mvert_num;
  ClothVertex *vert;

  /* 2.0f is an experimental value that seems to give good results */
  float smoothfac = 2.0f * parms->velocity_smooth;
  float collfac = 2.0f * parms->collider_friction;
  float pressfac = parms->pressure;
  float minpress = parms->pressure_threshold;
  float gmin[3], gmax[3];
  int i;

  hair_get_boundbox(clmd, gmin, gmax);

  /* gather velocities & density */
  if (smoothfac > 0.0f || pressfac > 0.0f) {
    HairVertexGrid *vertex_grid = BPH_hair_volume_create_vertex_grid(
        clmd->sim_parms->voxel_res, gmin, gmax);

    vert = cloth->verts;
    for (i = 0; i < mvert_num; i++, vert++) {
      float x[3], v[3];

      if (vert->solver_index < 0) {
        copy_v3_v3(x, vert->x);
        copy_v3_v3(v, vert->v);
      }
      else {
        BPH_mass_spring_get_motion_state(data, vert->solver_index, x, v);
      }
      BPH_hair_volume_add_vertex(vertex_grid, x, v);
    }
    BPH_hair_volume_normalize_vertex_grid(vertex_grid);

    vert = cloth->verts;
    for (i = 0; i < mvert_num; i++, vert++) {
      float x[3], v[3], f[3], dfdx[3][3], dfdv[3][3];

      if (vert->solver_index < 0) {
        continue;
      }

      /* calculate volumetric forces */
      BPH_mass_spring_get_motion_state(data, vert->solver_index, x, v);
      BPH_hair_volume_vertex_grid_forces(
          vertex_grid, x, v, smoothfac, pressfac, minpress, f, dfdx, dfdv);
      /* apply on hair data */
      BPH_mass_spring_force_extern(data, vert->solver_index, f, dfdx, dfdv);
    }

    BPH_hair_volume_free_vertex_grid(vertex_grid);
  }
}
#endif

static void cloth_solve_collisions(
    Depsgraph *depsgraph, Object *ob, ClothModifierData *clmd, float step, float dt)
{
  Cloth *cloth = clmd->clothObject;
  Implicit_Data *id = cloth->implicit;
  ClothVertex *verts = cloth->verts;
  int mvert_num = cloth->mvert_num;
  const float time_multiplier = 1.0f / (clmd->sim_parms->dt * clmd->sim_parms->timescale);
  int i;

  if (!(clmd->coll_parms->flags &
        (CLOTH_COLLSETTINGS_FLAG_ENABLED | CLOTH_COLLSETTINGS_FLAG_SELF))) {
    return;
  }

  if (!clmd->clothObject->bvhtree) {
    return;
  }

  BPH_mass_spring_solve_positions(id, dt);

  /* Update verts to current positions. */
  for (i = 0; i < mvert_num; i++) {
    BPH_mass_spring_get_new_position(id, i, verts[i].tx);

    sub_v3_v3v3(verts[i].tv, verts[i].tx, verts[i].txold);
    zero_v3(verts[i].dcvel);
  }

  if (cloth_bvh_collision(depsgraph,
                          ob,
                          clmd,
                          step / clmd->sim_parms->timescale,
                          dt / clmd->sim_parms->timescale)) {
    for (i = 0; i < mvert_num; i++) {
      if ((clmd->sim_parms->vgroup_mass > 0) && (verts[i].flags & CLOTH_VERT_FLAG_PINNED)) {
        continue;
      }

      BPH_mass_spring_get_new_velocity(id, i, verts[i].tv);
      madd_v3_v3fl(verts[i].tv, verts[i].dcvel, time_multiplier);
      BPH_mass_spring_set_new_velocity(id, i, verts[i].tv);
    }
  }
}

static void cloth_clear_result(ClothModifierData *clmd)
{
  ClothSolverResult *sres = clmd->solver_result;

  sres->status = 0;
  sres->max_error = sres->min_error = sres->avg_error = 0.0f;
  sres->max_iterations = sres->min_iterations = 0;
  sres->avg_iterations = 0.0f;
}

static void cloth_record_result(ClothModifierData *clmd, ImplicitSolverResult *result, float dt)
{
  ClothSolverResult *sres = clmd->solver_result;

  if (sres->status) { /* already initialized ? */
    /* error only makes sense for successful iterations */
    if (result->status == BPH_SOLVER_SUCCESS) {
      sres->min_error = min_ff(sres->min_error, result->error);
      sres->max_error = max_ff(sres->max_error, result->error);
      sres->avg_error += result->error * dt;
    }

    sres->min_iterations = min_ii(sres->min_iterations, result->iterations);
    sres->max_iterations = max_ii(sres->max_iterations, result->iterations);
    sres->avg_iterations += (float)result->iterations * dt;
  }
  else {
    /* error only makes sense for successful iterations */
    if (result->status == BPH_SOLVER_SUCCESS) {
      sres->min_error = sres->max_error = result->error;
      sres->avg_error += result->error * dt;
    }

    sres->min_iterations = sres->max_iterations = result->iterations;
    sres->avg_iterations += (float)result->iterations * dt;
  }

  sres->status |= result->status;
}

int BPH_cloth_solve(
    Depsgraph *depsgraph, Object *ob, float frame, ClothModifierData *clmd, ListBase *effectors)
{
  /* Hair currently is a cloth sim in disguise ...
   * Collision detection and volumetrics work differently then.
   * Bad design, TODO
   */
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  const bool is_hair = (clmd->hairdata != NULL);

  unsigned int i = 0;
  float step = 0.0f, tf = clmd->sim_parms->timescale;
  Cloth *cloth = clmd->clothObject;
  ClothVertex *verts = cloth->verts /*, *cv*/;
  unsigned int mvert_num = cloth->mvert_num;
  float dt = clmd->sim_parms->dt * clmd->sim_parms->timescale;
  Implicit_Data *id = cloth->implicit;
  ColliderContacts *contacts = NULL;
  int totcolliders = 0;

  BKE_sim_debug_data_clear_category("collision");

  if (!clmd->solver_result) {
    clmd->solver_result = (ClothSolverResult *)MEM_callocN(sizeof(ClothSolverResult),
                                                           "cloth solver result");
  }
  cloth_clear_result(clmd);

  if (clmd->sim_parms->vgroup_mass > 0) { /* Do goal stuff. */
    for (i = 0; i < mvert_num; i++) {
      // update velocities with constrained velocities from pinned verts
      if (verts[i].flags & CLOTH_VERT_FLAG_PINNED) {
        float v[3];
        sub_v3_v3v3(v, verts[i].xconst, verts[i].xold);
        // mul_v3_fl(v, clmd->sim_parms->stepsPerFrame);
        /* divide by time_scale to prevent constrained velocities from being multiplied */
        mul_v3_fl(v, 1.0f / clmd->sim_parms->time_scale);
        BPH_mass_spring_set_velocity(id, i, v);
      }
    }
  }

  while (step < tf) {
    ImplicitSolverResult result;

    if (is_hair) {
      /* determine contact points */
      if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED) {
        cloth_find_point_contacts(depsgraph, ob, clmd, 0.0f, tf, &contacts, &totcolliders);
      }

      /* setup vertex constraints for pinned vertices and contacts */
      cloth_setup_constraints(clmd, contacts, totcolliders, dt);
    }
    else {
      /* setup vertex constraints for pinned vertices */
      cloth_setup_constraints(clmd, NULL, 0, dt);
    }

    /* initialize forces to zero */
    BPH_mass_spring_clear_forces(id);

    // calculate forces
    cloth_calc_force(scene, clmd, frame, effectors, step);

    // calculate new velocity and position
    BPH_mass_spring_solve_velocities(id, dt, &result);
    cloth_record_result(clmd, &result, dt);

    /* Calculate collision impulses. */
    if (!is_hair) {
      cloth_solve_collisions(depsgraph, ob, clmd, step, dt);
    }

    if (is_hair) {
      cloth_continuum_step(clmd, dt);
    }

    BPH_mass_spring_solve_positions(id, dt);
    BPH_mass_spring_apply_result(id);

    /* move pinned verts to correct position */
    for (i = 0; i < mvert_num; i++) {
      if (clmd->sim_parms->vgroup_mass > 0) {
        if (verts[i].flags & CLOTH_VERT_FLAG_PINNED) {
          float x[3];
          /* divide by time_scale to prevent pinned vertices'
           * delta locations from being multiplied */
          interp_v3_v3v3(
              x, verts[i].xold, verts[i].xconst, (step + dt) / clmd->sim_parms->time_scale);
          BPH_mass_spring_set_position(id, i, x);
        }
      }

      BPH_mass_spring_get_motion_state(id, i, verts[i].txold, NULL);
    }

    /* free contact points */
    if (contacts) {
      cloth_free_contacts(contacts, totcolliders);
    }

    step += dt;
  }

  /* copy results back to cloth data */
  for (i = 0; i < mvert_num; i++) {
    BPH_mass_spring_get_motion_state(id, i, verts[i].x, verts[i].v);
    copy_v3_v3(verts[i].txold, verts[i].x);
  }

  return 1;
}
