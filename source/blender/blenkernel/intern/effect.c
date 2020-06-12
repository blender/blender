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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdarg.h>
#include <stddef.h>

#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_noise.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BKE_anim_path.h" /* needed for where_on_path */
#include "BKE_bvhutils.h"
#include "BKE_collection.h"
#include "BKE_collision.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_fluid.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "RE_render_ext.h"
#include "RE_shader_ext.h"

EffectorWeights *BKE_effector_add_weights(Collection *collection)
{
  EffectorWeights *weights = MEM_callocN(sizeof(EffectorWeights), "EffectorWeights");
  int i;

  for (i = 0; i < NUM_PFIELD_TYPES; i++) {
    weights->weight[i] = 1.0f;
  }

  weights->global_gravity = 1.0f;

  weights->group = collection;

  return weights;
}
PartDeflect *BKE_partdeflect_new(int type)
{
  PartDeflect *pd;

  pd = MEM_callocN(sizeof(PartDeflect), "PartDeflect");

  pd->forcefield = type;
  pd->pdef_sbdamp = 0.1f;
  pd->pdef_sbift = 0.2f;
  pd->pdef_sboft = 0.02f;
  pd->pdef_cfrict = 5.0f;
  pd->seed = ((uint)(ceil(PIL_check_seconds_timer())) + 1) % 128;
  pd->f_strength = 1.0f;
  pd->f_damp = 1.0f;

  /* set sensible defaults based on type */
  switch (type) {
    case PFIELD_VORTEX:
      pd->shape = PFIELD_SHAPE_PLANE;
      break;
    case PFIELD_WIND:
      pd->shape = PFIELD_SHAPE_PLANE;
      pd->f_flow = 1.0f;        /* realistic wind behavior */
      pd->f_wind_factor = 1.0f; /* only act perpendicularly to a surface */
      break;
    case PFIELD_TEXTURE:
      pd->f_size = 1.0f;
      break;
    case PFIELD_FLUIDFLOW:
      pd->f_flow = 1.0f;
      break;
  }
  pd->flag = PFIELD_DO_LOCATION | PFIELD_DO_ROTATION | PFIELD_CLOTH_USE_CULLING;

  return pd;
}

/************************ PARTICLES ***************************/

PartDeflect *BKE_partdeflect_copy(const struct PartDeflect *pd_src)
{
  if (pd_src == NULL) {
    return NULL;
  }
  PartDeflect *pd_dst = MEM_dupallocN(pd_src);
  if (pd_dst->rng != NULL) {
    pd_dst->rng = BLI_rng_copy(pd_dst->rng);
  }
  return pd_dst;
}

void BKE_partdeflect_free(PartDeflect *pd)
{
  if (!pd) {
    return;
  }
  if (pd->rng) {
    BLI_rng_free(pd->rng);
  }
  MEM_freeN(pd);
}

/******************** EFFECTOR RELATIONS ***********************/

static void precalculate_effector(struct Depsgraph *depsgraph, EffectorCache *eff)
{
  float ctime = DEG_get_ctime(depsgraph);
  uint cfra = (uint)(ctime >= 0 ? ctime : -ctime);
  if (!eff->pd->rng) {
    eff->pd->rng = BLI_rng_new(eff->pd->seed + cfra);
  }
  else {
    BLI_rng_srandom(eff->pd->rng, eff->pd->seed + cfra);
  }

  if (eff->pd->forcefield == PFIELD_GUIDE && eff->ob->type == OB_CURVE) {
    Curve *cu = eff->ob->data;
    if (cu->flag & CU_PATH) {
      if (eff->ob->runtime.curve_cache == NULL || eff->ob->runtime.curve_cache->path == NULL ||
          eff->ob->runtime.curve_cache->path->data == NULL) {
        BKE_displist_make_curveTypes(depsgraph, eff->scene, eff->ob, false, false);
      }

      if (eff->ob->runtime.curve_cache->path && eff->ob->runtime.curve_cache->path->data) {
        where_on_path(
            eff->ob, 0.0, eff->guide_loc, eff->guide_dir, NULL, &eff->guide_radius, NULL);
        mul_m4_v3(eff->ob->obmat, eff->guide_loc);
        mul_mat3_m4_v3(eff->ob->obmat, eff->guide_dir);
      }
    }
  }
  else if (eff->pd->shape == PFIELD_SHAPE_SURFACE) {
    eff->surmd = (SurfaceModifierData *)BKE_modifiers_findby_type(eff->ob, eModifierType_Surface);
    if (eff->ob->type == OB_CURVE) {
      eff->flag |= PE_USE_NORMAL_DATA;
    }
  }
  else if (eff->psys) {
    psys_update_particle_tree(eff->psys, ctime);
  }
}

static void add_effector_relation(ListBase *relations,
                                  Object *ob,
                                  ParticleSystem *psys,
                                  PartDeflect *pd)
{
  EffectorRelation *relation = MEM_callocN(sizeof(EffectorRelation), "EffectorRelation");
  relation->ob = ob;
  relation->psys = psys;
  relation->pd = pd;

  BLI_addtail(relations, relation);
}

static void add_effector_evaluation(ListBase **effectors,
                                    Depsgraph *depsgraph,
                                    Scene *scene,
                                    Object *ob,
                                    ParticleSystem *psys,
                                    PartDeflect *pd)
{
  if (*effectors == NULL) {
    *effectors = MEM_callocN(sizeof(ListBase), "effector effectors");
  }

  EffectorCache *eff = MEM_callocN(sizeof(EffectorCache), "EffectorCache");
  eff->depsgraph = depsgraph;
  eff->scene = scene;
  eff->ob = ob;
  eff->psys = psys;
  eff->pd = pd;
  eff->frame = -1;
  BLI_addtail(*effectors, eff);

  precalculate_effector(depsgraph, eff);
}

/* Create list of effector relations in the collection or entire scene.
 * This is used by the depsgraph to build relations, as well as faster
 * lookup of effectors during evaluation. */
ListBase *BKE_effector_relations_create(Depsgraph *depsgraph,
                                        ViewLayer *view_layer,
                                        Collection *collection)
{
  Base *base = BKE_collection_or_layer_objects(view_layer, collection);
  const bool for_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const int base_flag = (for_render) ? BASE_ENABLED_RENDER : BASE_ENABLED_VIEWPORT;

  ListBase *relations = MEM_callocN(sizeof(ListBase), "effector relations");

  for (; base; base = base->next) {
    if (!(base->flag & base_flag)) {
      continue;
    }

    Object *ob = base->object;

    if (ob->pd && ob->pd->forcefield) {
      add_effector_relation(relations, ob, NULL, ob->pd);
    }

    LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
      ParticleSettings *part = psys->part;

      if (psys_check_enabled(ob, psys, for_render)) {
        if (part->pd && part->pd->forcefield) {
          add_effector_relation(relations, ob, psys, part->pd);
        }
        if (part->pd2 && part->pd2->forcefield) {
          add_effector_relation(relations, ob, psys, part->pd2);
        }
      }
    }
  }

  return relations;
}

void BKE_effector_relations_free(ListBase *lb)
{
  if (lb) {
    BLI_freelistN(lb);
    MEM_freeN(lb);
  }
}

/* Create effective list of effectors from relations built beforehand. */
ListBase *BKE_effectors_create(Depsgraph *depsgraph,
                               Object *ob_src,
                               ParticleSystem *psys_src,
                               EffectorWeights *weights)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ListBase *relations = DEG_get_effector_relations(depsgraph, weights->group);
  ListBase *effectors = NULL;

  if (!relations) {
    return NULL;
  }

  LISTBASE_FOREACH (EffectorRelation *, relation, relations) {
    /* Get evaluated object. */
    Object *ob = (Object *)DEG_get_evaluated_id(depsgraph, &relation->ob->id);

    if (relation->psys) {
      /* Get evaluated particle system. */
      ParticleSystem *psys = BLI_findstring(
          &ob->particlesystem, relation->psys->name, offsetof(ParticleSystem, name));
      ParticleSettings *part = psys->part;

      if (psys == psys_src && (part->flag & PART_SELF_EFFECT) == 0) {
        continue;
      }

      PartDeflect *pd = (relation->pd == relation->psys->part->pd) ? part->pd : part->pd2;
      if (weights->weight[pd->forcefield] == 0.0f) {
        continue;
      }

      add_effector_evaluation(&effectors, depsgraph, scene, ob, psys, pd);
    }
    else {
      /* Object effector. */
      if (ob == ob_src) {
        continue;
      }
      else if (weights->weight[ob->pd->forcefield] == 0.0f) {
        continue;
      }
      else if (ob->pd->shape == PFIELD_SHAPE_POINTS && BKE_object_get_evaluated_mesh(ob) == NULL) {
        continue;
      }

      add_effector_evaluation(&effectors, depsgraph, scene, ob, NULL, ob->pd);
    }
  }

  return effectors;
}

void BKE_effectors_free(ListBase *lb)
{
  if (lb) {
    LISTBASE_FOREACH (EffectorCache *, eff, lb) {
      if (eff->guide_data) {
        MEM_freeN(eff->guide_data);
      }
    }

    BLI_freelistN(lb);
    MEM_freeN(lb);
  }
}

void pd_point_from_particle(ParticleSimulationData *sim,
                            ParticleData *pa,
                            ParticleKey *state,
                            EffectedPoint *point)
{
  ParticleSettings *part = sim->psys->part;
  point->loc = state->co;
  point->vel = state->vel;
  point->index = pa - sim->psys->particles;
  point->size = pa->size;
  point->charge = 0.0f;

  if (part->pd && part->pd->forcefield == PFIELD_CHARGE) {
    point->charge += part->pd->f_strength;
  }

  if (part->pd2 && part->pd2->forcefield == PFIELD_CHARGE) {
    point->charge += part->pd2->f_strength;
  }

  point->vel_to_sec = 1.0f;
  point->vel_to_frame = psys_get_timestep(sim);

  point->flag = 0;

  if (sim->psys->part->flag & PART_ROT_DYN) {
    point->ave = state->ave;
    point->rot = state->rot;
  }
  else {
    point->ave = point->rot = NULL;
  }

  point->psys = sim->psys;
}

void pd_point_from_loc(Scene *scene, float *loc, float *vel, int index, EffectedPoint *point)
{
  point->loc = loc;
  point->vel = vel;
  point->index = index;
  point->size = 0.0f;

  point->vel_to_sec = (float)scene->r.frs_sec;
  point->vel_to_frame = 1.0f;

  point->flag = 0;

  point->ave = point->rot = NULL;
  point->psys = NULL;
}
void pd_point_from_soft(Scene *scene, float *loc, float *vel, int index, EffectedPoint *point)
{
  point->loc = loc;
  point->vel = vel;
  point->index = index;
  point->size = 0.0f;

  point->vel_to_sec = (float)scene->r.frs_sec;
  point->vel_to_frame = 1.0f;

  point->flag = PE_WIND_AS_SPEED;

  point->ave = point->rot = NULL;

  point->psys = NULL;
}
/************************************************/
/*          Effectors       */
/************************************************/

// triangle - ray callback function
static void eff_tri_ray_hit(void *UNUSED(userData),
                            int UNUSED(index),
                            const BVHTreeRay *UNUSED(ray),
                            BVHTreeRayHit *hit)
{
  /* whenever we hit a bounding box, we don't check further */
  hit->dist = -1;
  hit->index = 1;
}

// get visibility of a wind ray
static float eff_calc_visibility(ListBase *colliders,
                                 EffectorCache *eff,
                                 EffectorData *efd,
                                 EffectedPoint *point)
{
  const int raycast_flag = BVH_RAYCAST_DEFAULT & ~BVH_RAYCAST_WATERTIGHT;
  ListBase *colls = colliders;
  ColliderCache *col;
  float norm[3], len = 0.0;
  float visibility = 1.0, absorption = 0.0;

  if (!(eff->pd->flag & PFIELD_VISIBILITY)) {
    return visibility;
  }
  if (!colls) {
    colls = BKE_collider_cache_create(eff->depsgraph, eff->ob, NULL);
  }
  if (!colls) {
    return visibility;
  }

  negate_v3_v3(norm, efd->vec_to_point);
  len = normalize_v3(norm);

  /* check all collision objects */
  for (col = colls->first; col; col = col->next) {
    CollisionModifierData *collmd = col->collmd;

    if (col->ob == eff->ob) {
      continue;
    }
    if (collmd->bvhtree) {
      BVHTreeRayHit hit;

      hit.index = -1;
      hit.dist = len + FLT_EPSILON;

      /* check if the way is blocked */
      if (BLI_bvhtree_ray_cast_ex(collmd->bvhtree,
                                  point->loc,
                                  norm,
                                  0.0f,
                                  &hit,
                                  eff_tri_ray_hit,
                                  NULL,
                                  raycast_flag) != -1) {
        absorption = col->ob->pd->absorption;

        /* visibility is only between 0 and 1, calculated from 1-absorption */
        visibility *= CLAMPIS(1.0f - absorption, 0.0f, 1.0f);

        if (visibility <= 0.0f) {
          break;
        }
      }
    }
  }

  if (!colliders) {
    BKE_collider_cache_free(&colls);
  }

  return visibility;
}

// noise function for wind e.g.
static float wind_func(struct RNG *rng, float strength)
{
  int random = (BLI_rng_get_int(rng) + 1) % 128; /* max 2357 */
  float force = BLI_rng_get_float(rng) + 1.0f;
  float ret;
  float sign = 0;

  /* Dividing by 2 is not giving equal sign distribution. */
  sign = ((float)random > 64.0f) ? 1.0f : -1.0f;

  ret = sign * ((float)random / force) * strength / 128.0f;

  return ret;
}

/* maxdist: zero effect from this distance outwards (if usemax) */
/* mindist: full effect up to this distance (if usemin) */
/* power: falloff with formula 1/r^power */
static float falloff_func(
    float fac, int usemin, float mindist, int usemax, float maxdist, float power)
{
  /* first quick checks */
  if (usemax && fac > maxdist) {
    return 0.0f;
  }

  if (usemin && fac < mindist) {
    return 1.0f;
  }

  if (!usemin) {
    mindist = 0.0;
  }

  return pow((double)(1.0f + fac - mindist), (double)(-power));
}

static float falloff_func_dist(PartDeflect *pd, float fac)
{
  return falloff_func(fac,
                      pd->flag & PFIELD_USEMIN,
                      pd->mindist,
                      pd->flag & PFIELD_USEMAX,
                      pd->maxdist,
                      pd->f_power);
}

static float falloff_func_rad(PartDeflect *pd, float fac)
{
  return falloff_func(fac,
                      pd->flag & PFIELD_USEMINR,
                      pd->minrad,
                      pd->flag & PFIELD_USEMAXR,
                      pd->maxrad,
                      pd->f_power_r);
}

float effector_falloff(EffectorCache *eff,
                       EffectorData *efd,
                       EffectedPoint *UNUSED(point),
                       EffectorWeights *weights)
{
  float temp[3];
  float falloff = weights ? weights->weight[0] * weights->weight[eff->pd->forcefield] : 1.0f;
  float fac, r_fac;

  fac = dot_v3v3(efd->nor, efd->vec_to_point2);

  if (eff->pd->zdir == PFIELD_Z_POS && fac < 0.0f) {
    falloff = 0.0f;
  }
  else if (eff->pd->zdir == PFIELD_Z_NEG && fac > 0.0f) {
    falloff = 0.0f;
  }
  else {
    switch (eff->pd->falloff) {
      case PFIELD_FALL_SPHERE:
        falloff *= falloff_func_dist(eff->pd, efd->distance);
        break;

      case PFIELD_FALL_TUBE:
        falloff *= falloff_func_dist(eff->pd, fabsf(fac));
        if (falloff == 0.0f) {
          break;
        }

        madd_v3_v3v3fl(temp, efd->vec_to_point2, efd->nor, -fac);
        r_fac = len_v3(temp);
        falloff *= falloff_func_rad(eff->pd, r_fac);
        break;
      case PFIELD_FALL_CONE:
        falloff *= falloff_func_dist(eff->pd, fabsf(fac));
        if (falloff == 0.0f) {
          break;
        }

        r_fac = RAD2DEGF(saacos(fac / len_v3(efd->vec_to_point2)));
        falloff *= falloff_func_rad(eff->pd, r_fac);

        break;
    }
  }

  return falloff;
}

int closest_point_on_surface(SurfaceModifierData *surmd,
                             const float co[3],
                             float surface_co[3],
                             float surface_nor[3],
                             float surface_vel[3])
{
  BVHTreeNearest nearest;

  nearest.index = -1;
  nearest.dist_sq = FLT_MAX;

  BLI_bvhtree_find_nearest(
      surmd->bvhtree->tree, co, &nearest, surmd->bvhtree->nearest_callback, surmd->bvhtree);

  if (nearest.index != -1) {
    copy_v3_v3(surface_co, nearest.co);

    if (surface_nor) {
      copy_v3_v3(surface_nor, nearest.no);
    }

    if (surface_vel) {
      const MLoop *mloop = surmd->bvhtree->loop;
      const MLoopTri *lt = &surmd->bvhtree->looptri[nearest.index];

      copy_v3_v3(surface_vel, surmd->v[mloop[lt->tri[0]].v].co);
      add_v3_v3(surface_vel, surmd->v[mloop[lt->tri[1]].v].co);
      add_v3_v3(surface_vel, surmd->v[mloop[lt->tri[2]].v].co);

      mul_v3_fl(surface_vel, (1.0f / 3.0f));
    }
    return 1;
  }

  return 0;
}
int get_effector_data(EffectorCache *eff,
                      EffectorData *efd,
                      EffectedPoint *point,
                      int real_velocity)
{
  float cfra = DEG_get_ctime(eff->depsgraph);
  int ret = 0;

  /* In case surface object is in Edit mode when loading the .blend,
   * surface modifier is never executed and bvhtree never built, see T48415. */
  if (eff->pd && eff->pd->shape == PFIELD_SHAPE_SURFACE && eff->surmd && eff->surmd->bvhtree) {
    /* closest point in the object surface is an effector */
    float vec[3];

    /* using velocity corrected location allows for easier sliding over effector surface */
    copy_v3_v3(vec, point->vel);
    mul_v3_fl(vec, point->vel_to_frame);
    add_v3_v3(vec, point->loc);

    ret = closest_point_on_surface(
        eff->surmd, vec, efd->loc, efd->nor, real_velocity ? efd->vel : NULL);

    efd->size = 0.0f;
  }
  else if (eff->pd && eff->pd->shape == PFIELD_SHAPE_POINTS) {
    /* TODO: hair and points object support */
    Mesh *me_eval = BKE_object_get_evaluated_mesh(eff->ob);
    if (me_eval != NULL) {
      copy_v3_v3(efd->loc, me_eval->mvert[*efd->index].co);
      normal_short_to_float_v3(efd->nor, me_eval->mvert[*efd->index].no);

      mul_m4_v3(eff->ob->obmat, efd->loc);
      mul_mat3_m4_v3(eff->ob->obmat, efd->nor);

      normalize_v3(efd->nor);

      efd->size = 0.0f;

      ret = 1;
    }
  }
  else if (eff->psys) {
    ParticleData *pa = eff->psys->particles + *efd->index;
    ParticleKey state;

    /* exclude the particle itself for self effecting particles */
    if (eff->psys == point->psys && *efd->index == point->index) {
      /* pass */
    }
    else {
      ParticleSimulationData sim = {NULL};
      sim.depsgraph = eff->depsgraph;
      sim.scene = eff->scene;
      sim.ob = eff->ob;
      sim.psys = eff->psys;

      /* TODO: time from actual previous calculated frame (step might not be 1) */
      state.time = cfra - 1.0f;
      ret = psys_get_particle_state(&sim, *efd->index, &state, 0);

      /* TODO */
      // if (eff->pd->forcefiled == PFIELD_HARMONIC && ret==0) {
      //  if (pa->dietime < eff->psys->cfra)
      //      eff->flag |= PE_VELOCITY_TO_IMPULSE;
      //}

      copy_v3_v3(efd->loc, state.co);

      /* rather than use the velocity use rotated x-axis (defaults to velocity) */
      efd->nor[0] = 1.f;
      efd->nor[1] = efd->nor[2] = 0.f;
      mul_qt_v3(state.rot, efd->nor);

      if (real_velocity) {
        copy_v3_v3(efd->vel, state.vel);
      }
      efd->size = pa->size;
    }
  }
  else {
    /* use center of object for distance calculus */
    const Object *ob = eff->ob;

    /* use z-axis as normal*/
    normalize_v3_v3(efd->nor, ob->obmat[2]);

    if (eff->pd && ELEM(eff->pd->shape, PFIELD_SHAPE_PLANE, PFIELD_SHAPE_LINE)) {
      float temp[3], translate[3];
      sub_v3_v3v3(temp, point->loc, ob->obmat[3]);
      project_v3_v3v3(translate, temp, efd->nor);

      /* for vortex the shape chooses between old / new force */
      if (eff->pd->forcefield == PFIELD_VORTEX || eff->pd->shape == PFIELD_SHAPE_LINE) {
        add_v3_v3v3(efd->loc, ob->obmat[3], translate);
      }
      else { /* normally efd->loc is closest point on effector xy-plane */
        sub_v3_v3v3(efd->loc, point->loc, translate);
      }
    }
    else {
      copy_v3_v3(efd->loc, ob->obmat[3]);
    }

    zero_v3(efd->vel);
    efd->size = 0.0f;

    ret = 1;
  }

  if (ret) {
    sub_v3_v3v3(efd->vec_to_point, point->loc, efd->loc);
    efd->distance = len_v3(efd->vec_to_point);

    /* Rest length for harmonic effector,
     * will have to see later if this could be extended to other effectors. */
    if (eff->pd && eff->pd->forcefield == PFIELD_HARMONIC && eff->pd->f_size) {
      mul_v3_fl(efd->vec_to_point, (efd->distance - eff->pd->f_size) / efd->distance);
    }

    if (eff->flag & PE_USE_NORMAL_DATA) {
      copy_v3_v3(efd->vec_to_point2, efd->vec_to_point);
      copy_v3_v3(efd->nor2, efd->nor);
    }
    else {
      /* for some effectors we need the object center every time */
      sub_v3_v3v3(efd->vec_to_point2, point->loc, eff->ob->obmat[3]);
      normalize_v3_v3(efd->nor2, eff->ob->obmat[2]);
    }
  }

  return ret;
}
static void get_effector_tot(
    EffectorCache *eff, EffectorData *efd, EffectedPoint *point, int *tot, int *p, int *step)
{
  *p = 0;
  efd->index = p;

  if (eff->pd->shape == PFIELD_SHAPE_POINTS) {
    /* TODO: hair and points object support */
    Mesh *me_eval = BKE_object_get_evaluated_mesh(eff->ob);
    *tot = me_eval != NULL ? me_eval->totvert : 1;

    if (*tot && eff->pd->forcefield == PFIELD_HARMONIC && point->index >= 0) {
      *p = point->index % *tot;
      *tot = *p + 1;
    }
  }
  else if (eff->psys) {
    *tot = eff->psys->totpart;

    if (eff->pd->forcefield == PFIELD_CHARGE) {
      /* Only the charge of the effected particle is used for
       * interaction, not fall-offs. If the fall-offs aren't the
       * same this will be unphysical, but for animation this
       * could be the wanted behavior. If you want physical
       * correctness the fall-off should be spherical 2.0 anyways.
       */
      efd->charge = eff->pd->f_strength;
    }
    else if (eff->pd->forcefield == PFIELD_HARMONIC &&
             (eff->pd->flag & PFIELD_MULTIPLE_SPRINGS) == 0) {
      /* every particle is mapped to only one harmonic effector particle */
      *p = point->index % eff->psys->totpart;
      *tot = *p + 1;
    }

    if (eff->psys->part->effector_amount) {
      int totpart = eff->psys->totpart;
      int amount = eff->psys->part->effector_amount;

      *step = (totpart > amount) ? totpart / amount : 1;
    }
  }
  else {
    *tot = 1;
  }
}
static void do_texture_effector(EffectorCache *eff,
                                EffectorData *efd,
                                EffectedPoint *point,
                                float *total_force)
{
  TexResult result[4];
  float tex_co[3], strength, force[3];
  float nabla = eff->pd->tex_nabla;
  int hasrgb;
  short mode = eff->pd->tex_mode;
  bool scene_color_manage;

  if (!eff->pd->tex) {
    return;
  }

  result[0].nor = result[1].nor = result[2].nor = result[3].nor = NULL;

  strength = eff->pd->f_strength * efd->falloff;

  copy_v3_v3(tex_co, point->loc);

  if (eff->pd->flag & PFIELD_TEX_OBJECT) {
    mul_m4_v3(eff->ob->imat, tex_co);

    if (eff->pd->flag & PFIELD_TEX_2D) {
      tex_co[2] = 0.0f;
    }
  }
  else if (eff->pd->flag & PFIELD_TEX_2D) {
    float fac = -dot_v3v3(tex_co, efd->nor);
    madd_v3_v3fl(tex_co, efd->nor, fac);
  }

  scene_color_manage = BKE_scene_check_color_management_enabled(eff->scene);

  hasrgb = multitex_ext(
      eff->pd->tex, tex_co, NULL, NULL, 0, result, 0, NULL, scene_color_manage, false);

  if (hasrgb && mode == PFIELD_TEX_RGB) {
    force[0] = (0.5f - result->tr) * strength;
    force[1] = (0.5f - result->tg) * strength;
    force[2] = (0.5f - result->tb) * strength;
  }
  else if (nabla != 0) {
    strength /= nabla;

    tex_co[0] += nabla;
    multitex_ext(
        eff->pd->tex, tex_co, NULL, NULL, 0, result + 1, 0, NULL, scene_color_manage, false);

    tex_co[0] -= nabla;
    tex_co[1] += nabla;
    multitex_ext(
        eff->pd->tex, tex_co, NULL, NULL, 0, result + 2, 0, NULL, scene_color_manage, false);

    tex_co[1] -= nabla;
    tex_co[2] += nabla;
    multitex_ext(
        eff->pd->tex, tex_co, NULL, NULL, 0, result + 3, 0, NULL, scene_color_manage, false);

    if (mode == PFIELD_TEX_GRAD || !hasrgb) { /* if we don't have rgb fall back to grad */
      /* generate intensity if texture only has rgb value */
      if (hasrgb & TEX_RGB) {
        for (int i = 0; i < 4; i++) {
          result[i].tin = (1.0f / 3.0f) * (result[i].tr + result[i].tg + result[i].tb);
        }
      }
      force[0] = (result[0].tin - result[1].tin) * strength;
      force[1] = (result[0].tin - result[2].tin) * strength;
      force[2] = (result[0].tin - result[3].tin) * strength;
    }
    else { /*PFIELD_TEX_CURL*/
      float dbdy, dgdz, drdz, dbdx, dgdx, drdy;

      dbdy = result[2].tb - result[0].tb;
      dgdz = result[3].tg - result[0].tg;
      drdz = result[3].tr - result[0].tr;
      dbdx = result[1].tb - result[0].tb;
      dgdx = result[1].tg - result[0].tg;
      drdy = result[2].tr - result[0].tr;

      force[0] = (dbdy - dgdz) * strength;
      force[1] = (drdz - dbdx) * strength;
      force[2] = (dgdx - drdy) * strength;
    }
  }
  else {
    zero_v3(force);
  }

  if (eff->pd->flag & PFIELD_TEX_2D) {
    float fac = -dot_v3v3(force, efd->nor);
    madd_v3_v3fl(force, efd->nor, fac);
  }

  add_v3_v3(total_force, force);
}
static void do_physical_effector(EffectorCache *eff,
                                 EffectorData *efd,
                                 EffectedPoint *point,
                                 float *total_force)
{
  PartDeflect *pd = eff->pd;
  RNG *rng = pd->rng;
  float force[3] = {0, 0, 0};
  float temp[3];
  float fac;
  float strength = pd->f_strength;
  float damp = pd->f_damp;
  float noise_factor = pd->f_noise;

  if (noise_factor > 0.0f) {
    strength += wind_func(rng, noise_factor);

    if (ELEM(pd->forcefield, PFIELD_HARMONIC, PFIELD_DRAG)) {
      damp += wind_func(rng, noise_factor);
    }
  }

  copy_v3_v3(force, efd->vec_to_point);

  switch (pd->forcefield) {
    case PFIELD_WIND:
      copy_v3_v3(force, efd->nor);
      mul_v3_fl(force, strength * efd->falloff);
      break;
    case PFIELD_FORCE:
      normalize_v3(force);
      if (pd->flag & PFIELD_GRAVITATION) { /* Option: Multiply by 1/distance^2 */
        if (efd->distance < FLT_EPSILON) {
          strength = 0.0f;
        }
        else {
          strength *= powf(efd->distance, -2.0f);
        }
      }
      mul_v3_fl(force, strength * efd->falloff);
      break;
    case PFIELD_VORTEX:
      /* old vortex force */
      if (pd->shape == PFIELD_SHAPE_POINT) {
        cross_v3_v3v3(force, efd->nor, efd->vec_to_point);
        normalize_v3(force);
        mul_v3_fl(force, strength * efd->distance * efd->falloff);
      }
      else {
        /* new vortex force */
        cross_v3_v3v3(temp, efd->nor2, efd->vec_to_point2);
        mul_v3_fl(temp, strength * efd->falloff);

        cross_v3_v3v3(force, efd->nor2, temp);
        mul_v3_fl(force, strength * efd->falloff);

        madd_v3_v3fl(temp, point->vel, -point->vel_to_sec);
        add_v3_v3(force, temp);
      }
      break;
    case PFIELD_MAGNET:
      if (ELEM(eff->pd->shape, PFIELD_SHAPE_POINT, PFIELD_SHAPE_LINE)) {
        /* magnetic field of a moving charge */
        cross_v3_v3v3(temp, efd->nor, efd->vec_to_point);
      }
      else {
        copy_v3_v3(temp, efd->nor);
      }

      normalize_v3(temp);
      mul_v3_fl(temp, strength * efd->falloff);
      cross_v3_v3v3(force, point->vel, temp);
      mul_v3_fl(force, point->vel_to_sec);
      break;
    case PFIELD_HARMONIC:
      mul_v3_fl(force, -strength * efd->falloff);
      copy_v3_v3(temp, point->vel);
      mul_v3_fl(temp, -damp * 2.0f * sqrtf(fabsf(strength)) * point->vel_to_sec);
      add_v3_v3(force, temp);
      break;
    case PFIELD_CHARGE:
      mul_v3_fl(force, point->charge * strength * efd->falloff);
      break;
    case PFIELD_LENNARDJ:
      fac = pow((efd->size + point->size) / efd->distance, 6.0);

      fac = -fac * (1.0f - fac) / efd->distance;

      /* limit the repulsive term drastically to avoid huge forces */
      fac = ((fac > 2.0f) ? 2.0f : fac);

      mul_v3_fl(force, strength * fac);
      break;
    case PFIELD_BOID:
      /* Boid field is handled completely in boids code. */
      return;
    case PFIELD_TURBULENCE:
      if (pd->flag & PFIELD_GLOBAL_CO) {
        copy_v3_v3(temp, point->loc);
      }
      else {
        add_v3_v3v3(temp, efd->vec_to_point2, efd->nor2);
      }
      force[0] = -1.0f + 2.0f * BLI_gTurbulence(pd->f_size, temp[0], temp[1], temp[2], 2, 0, 2);
      force[1] = -1.0f + 2.0f * BLI_gTurbulence(pd->f_size, temp[1], temp[2], temp[0], 2, 0, 2);
      force[2] = -1.0f + 2.0f * BLI_gTurbulence(pd->f_size, temp[2], temp[0], temp[1], 2, 0, 2);
      mul_v3_fl(force, strength * efd->falloff);
      break;
    case PFIELD_DRAG:
      copy_v3_v3(force, point->vel);
      fac = normalize_v3(force) * point->vel_to_sec;

      strength = MIN2(strength, 2.0f);
      damp = MIN2(damp, 2.0f);

      mul_v3_fl(force, -efd->falloff * fac * (strength * fac + damp));
      break;
    case PFIELD_FLUIDFLOW:
      zero_v3(force);
#ifdef WITH_FLUID
      if (pd->f_source) {
        float density;
        if ((density = BKE_fluid_get_velocity_at(pd->f_source, point->loc, force)) >= 0.0f) {
          float influence = strength * efd->falloff;
          if (pd->flag & PFIELD_SMOKE_DENSITY) {
            influence *= density;
          }
          mul_v3_fl(force, influence);
          /* apply flow */
          madd_v3_v3fl(total_force, point->vel, -pd->f_flow * influence);
        }
      }
#endif
      break;
  }

  if (pd->flag & PFIELD_DO_LOCATION) {
    madd_v3_v3fl(total_force, force, 1.0f / point->vel_to_sec);

    if (ELEM(pd->forcefield, PFIELD_HARMONIC, PFIELD_DRAG, PFIELD_FLUIDFLOW) == 0 &&
        pd->f_flow != 0.0f) {
      madd_v3_v3fl(total_force, point->vel, -pd->f_flow * efd->falloff);
    }
  }

  if (point->ave) {
    zero_v3(point->ave);
  }
  if (pd->flag & PFIELD_DO_ROTATION && point->ave && point->rot) {
    float xvec[3] = {1.0f, 0.0f, 0.0f};
    float dave[3];
    mul_qt_v3(point->rot, xvec);
    cross_v3_v3v3(dave, xvec, force);
    if (pd->f_flow != 0.0f) {
      madd_v3_v3fl(dave, point->ave, -pd->f_flow * efd->falloff);
    }
    add_v3_v3(point->ave, dave);
  }
}

/*  -------- BKE_effectors_apply() --------
 * generic force/speed system, now used for particles and softbodies
 * scene       = scene where it runs in, for time and stuff
 * lb           = listbase with objects that take part in effecting
 * opco     = global coord, as input
 * force        = accumulator for force
 * wind_force   = accumulator for force only acting perpendicular to a surface
 * speed        = actual current speed which can be altered
 * cur_time = "external" time in frames, is constant for static particles
 * loc_time = "local" time in frames, range <0-1> for the lifetime of particle
 * par_layer    = layer the caller is in
 * flags        = only used for softbody wind now
 * guide        = old speed of particle
 */
void BKE_effectors_apply(ListBase *effectors,
                         ListBase *colliders,
                         EffectorWeights *weights,
                         EffectedPoint *point,
                         float *force,
                         float *wind_force,
                         float *impulse)
{
  /*
   * Modifies the force on a particle according to its
   * relation with the effector object
   * Different kind of effectors include:
   *     Forcefields: Gravity-like attractor
   *     (force power is related to the inverse of distance to the power of a falloff value)
   *     Vortex fields: swirling effectors
   *     (particles rotate around Z-axis of the object. otherwise, same relation as)
   *     (Forcefields, but this is not done through a force/acceleration)
   *     Guide: particles on a path
   *     (particles are guided along a curve bezier or old nurbs)
   *     (is independent of other effectors)
   */
  EffectorCache *eff;
  EffectorData efd;
  int p = 0, tot = 1, step = 1;

  /* Cycle through collected objects, get total of (1/(gravity_strength * dist^gravity_power)) */
  /* Check for min distance here? (yes would be cool to add that, ton) */

  if (effectors) {
    for (eff = effectors->first; eff; eff = eff->next) {
      /* object effectors were fully checked to be OK to evaluate! */

      get_effector_tot(eff, &efd, point, &tot, &p, &step);

      for (; p < tot; p += step) {
        if (get_effector_data(eff, &efd, point, 0)) {
          efd.falloff = effector_falloff(eff, &efd, point, weights);

          if (efd.falloff > 0.0f) {
            efd.falloff *= eff_calc_visibility(colliders, eff, &efd, point);
          }
          if (efd.falloff > 0.0f) {
            float out_force[3] = {0, 0, 0};

            if (eff->pd->forcefield == PFIELD_TEXTURE) {
              do_texture_effector(eff, &efd, point, out_force);
            }
            else {
              do_physical_effector(eff, &efd, point, out_force);

              /* for softbody backward compatibility */
              if (point->flag & PE_WIND_AS_SPEED && impulse) {
                sub_v3_v3v3(impulse, impulse, out_force);
              }
            }

            if (wind_force) {
              madd_v3_v3fl(force, out_force, 1.0f - eff->pd->f_wind_factor);
              madd_v3_v3fl(wind_force, out_force, eff->pd->f_wind_factor);
            }
            else {
              add_v3_v3(force, out_force);
            }
          }
        }
        else if (eff->flag & PE_VELOCITY_TO_IMPULSE && impulse) {
          /* special case for harmonic effector */
          add_v3_v3v3(impulse, impulse, efd.vel);
        }
      }
    }
  }
}

/* ======== Simulation Debugging ======== */

SimDebugData *_sim_debug_data = NULL;

uint BKE_sim_debug_data_hash(int i)
{
  return BLI_ghashutil_uinthash((uint)i);
}

uint BKE_sim_debug_data_hash_combine(uint kx, uint ky)
{
#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

  uint a, b, c;

  a = b = c = 0xdeadbeef + (2 << 2) + 13;
  a += kx;
  b += ky;

  c ^= b;
  c -= rot(b, 14);
  a ^= c;
  a -= rot(c, 11);
  b ^= a;
  b -= rot(a, 25);
  c ^= b;
  c -= rot(b, 16);
  a ^= c;
  a -= rot(c, 4);
  b ^= a;
  b -= rot(a, 14);
  c ^= b;
  c -= rot(b, 24);

  return c;

#undef rot
}

static uint debug_element_hash(const void *key)
{
  const SimDebugElement *elem = key;
  return elem->hash;
}

static bool debug_element_compare(const void *a, const void *b)
{
  const SimDebugElement *elem1 = a;
  const SimDebugElement *elem2 = b;

  if (elem1->hash == elem2->hash) {
    return 0;
  }
  return 1;
}

static void debug_element_free(void *val)
{
  SimDebugElement *elem = val;
  MEM_freeN(elem);
}

void BKE_sim_debug_data_set_enabled(bool enable)
{
  if (enable) {
    if (!_sim_debug_data) {
      _sim_debug_data = MEM_callocN(sizeof(SimDebugData), "sim debug data");
      _sim_debug_data->gh = BLI_ghash_new(
          debug_element_hash, debug_element_compare, "sim debug element hash");
    }
  }
  else {
    BKE_sim_debug_data_free();
  }
}

bool BKE_sim_debug_data_get_enabled(void)
{
  return _sim_debug_data != NULL;
}

void BKE_sim_debug_data_free(void)
{
  if (_sim_debug_data) {
    if (_sim_debug_data->gh) {
      BLI_ghash_free(_sim_debug_data->gh, NULL, debug_element_free);
    }
    MEM_freeN(_sim_debug_data);
  }
}

static void debug_data_insert(SimDebugData *debug_data, SimDebugElement *elem)
{
  SimDebugElement *old_elem = BLI_ghash_lookup(debug_data->gh, elem);
  if (old_elem) {
    *old_elem = *elem;
    MEM_freeN(elem);
  }
  else {
    BLI_ghash_insert(debug_data->gh, elem, elem);
  }
}

void BKE_sim_debug_data_add_element(int type,
                                    const float v1[3],
                                    const float v2[3],
                                    const char *str,
                                    float r,
                                    float g,
                                    float b,
                                    const char *category,
                                    uint hash)
{
  uint category_hash = BLI_ghashutil_strhash_p(category);
  SimDebugElement *elem;

  if (!_sim_debug_data) {
    if (G.debug & G_DEBUG_SIMDATA) {
      BKE_sim_debug_data_set_enabled(true);
    }
    else {
      return;
    }
  }

  elem = MEM_callocN(sizeof(SimDebugElement), "sim debug data element");
  elem->type = type;
  elem->category_hash = category_hash;
  elem->hash = hash;
  elem->color[0] = r;
  elem->color[1] = g;
  elem->color[2] = b;
  if (v1) {
    copy_v3_v3(elem->v1, v1);
  }
  else {
    zero_v3(elem->v1);
  }
  if (v2) {
    copy_v3_v3(elem->v2, v2);
  }
  else {
    zero_v3(elem->v2);
  }
  if (str) {
    BLI_strncpy(elem->str, str, sizeof(elem->str));
  }
  else {
    elem->str[0] = '\0';
  }

  debug_data_insert(_sim_debug_data, elem);
}

void BKE_sim_debug_data_remove_element(uint hash)
{
  SimDebugElement dummy;
  if (!_sim_debug_data) {
    return;
  }
  dummy.hash = hash;
  BLI_ghash_remove(_sim_debug_data->gh, &dummy, NULL, debug_element_free);
}

void BKE_sim_debug_data_clear(void)
{
  if (!_sim_debug_data) {
    return;
  }
  if (_sim_debug_data->gh) {
    BLI_ghash_clear(_sim_debug_data->gh, NULL, debug_element_free);
  }
}

void BKE_sim_debug_data_clear_category(const char *category)
{
  int category_hash = (int)BLI_ghashutil_strhash_p(category);

  if (!_sim_debug_data) {
    return;
  }

  if (_sim_debug_data->gh) {
    GHashIterator iter;
    BLI_ghashIterator_init(&iter, _sim_debug_data->gh);
    while (!BLI_ghashIterator_done(&iter)) {
      const SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);

      /* Removing invalidates the current iterator, so step before removing. */
      BLI_ghashIterator_step(&iter);

      if (elem->category_hash == category_hash) {
        BLI_ghash_remove(_sim_debug_data->gh, elem, NULL, debug_element_free);
      }
    }
  }
}
