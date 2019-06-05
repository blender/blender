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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_particle_types.h"
#include "DNA_smoke_types.h"
#include "DNA_scene_types.h"
#include "DNA_dynamicpaint_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"
#include "BLI_rand.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_linklist.h"

#include "BLT_translation.h"

#include "BKE_anim.h"
#include "BKE_animsys.h"

#include "BKE_boids.h"
#include "BKE_cloth.h"
#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_effect.h"
#include "BKE_main.h"
#include "BKE_lattice.h"

#include "BKE_displist.h"
#include "BKE_particle.h"
#include "BKE_material.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"
#include "BKE_cdderivedmesh.h" /* for weight_to_rgb() */
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_deform.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "RE_render_ext.h"

#include "particle_private.h"

unsigned int PSYS_FRAND_SEED_OFFSET[PSYS_FRAND_COUNT];
unsigned int PSYS_FRAND_SEED_MULTIPLIER[PSYS_FRAND_COUNT];
float PSYS_FRAND_BASE[PSYS_FRAND_COUNT];

void psys_init_rng(void)
{
  RNG *rng = BLI_rng_new_srandom(5831); /* arbitrary */
  for (int i = 0; i < PSYS_FRAND_COUNT; ++i) {
    PSYS_FRAND_BASE[i] = BLI_rng_get_float(rng);
    PSYS_FRAND_SEED_OFFSET[i] = (unsigned int)BLI_rng_get_int(rng);
    PSYS_FRAND_SEED_MULTIPLIER[i] = (unsigned int)BLI_rng_get_int(rng);
  }
  BLI_rng_free(rng);
}

static void get_child_modifier_parameters(ParticleSettings *part,
                                          ParticleThreadContext *ctx,
                                          ChildParticle *cpa,
                                          short cpa_from,
                                          int cpa_num,
                                          float *cpa_fuv,
                                          float *orco,
                                          ParticleTexture *ptex);
static void get_cpa_texture(Mesh *mesh,
                            ParticleSystem *psys,
                            ParticleSettings *part,
                            ParticleData *par,
                            int child_index,
                            int face_index,
                            const float fw[4],
                            float *orco,
                            ParticleTexture *ptex,
                            int event,
                            float cfra);

/* few helpers for countall etc. */
int count_particles(ParticleSystem *psys)
{
  ParticleSettings *part = psys->part;
  PARTICLE_P;
  int tot = 0;

  LOOP_SHOWN_PARTICLES
  {
    if (pa->alive == PARS_UNBORN && (part->flag & PART_UNBORN) == 0) {
    }
    else if (pa->alive == PARS_DEAD && (part->flag & PART_DIED) == 0) {
    }
    else {
      tot++;
    }
  }
  return tot;
}
int count_particles_mod(ParticleSystem *psys, int totgr, int cur)
{
  ParticleSettings *part = psys->part;
  PARTICLE_P;
  int tot = 0;

  LOOP_SHOWN_PARTICLES
  {
    if (pa->alive == PARS_UNBORN && (part->flag & PART_UNBORN) == 0) {
    }
    else if (pa->alive == PARS_DEAD && (part->flag & PART_DIED) == 0) {
    }
    else if (p % totgr == cur) {
      tot++;
    }
  }
  return tot;
}
/* we allocate path cache memory in chunks instead of a big contiguous
 * chunk, windows' memory allocater fails to find big blocks of memory often */

#define PATH_CACHE_BUF_SIZE 1024

static ParticleCacheKey *pcache_key_segment_endpoint_safe(ParticleCacheKey *key)
{
  return (key->segments > 0) ? (key + (key->segments - 1)) : key;
}

static ParticleCacheKey **psys_alloc_path_cache_buffers(ListBase *bufs, int tot, int totkeys)
{
  LinkData *buf;
  ParticleCacheKey **cache;
  int i, totkey, totbufkey;

  tot = MAX2(tot, 1);
  totkey = 0;
  cache = MEM_callocN(tot * sizeof(void *), "PathCacheArray");

  while (totkey < tot) {
    totbufkey = MIN2(tot - totkey, PATH_CACHE_BUF_SIZE);
    buf = MEM_callocN(sizeof(LinkData), "PathCacheLinkData");
    buf->data = MEM_callocN(sizeof(ParticleCacheKey) * totbufkey * totkeys, "ParticleCacheKey");

    for (i = 0; i < totbufkey; i++) {
      cache[totkey + i] = ((ParticleCacheKey *)buf->data) + i * totkeys;
    }

    totkey += totbufkey;
    BLI_addtail(bufs, buf);
  }

  return cache;
}

static void psys_free_path_cache_buffers(ParticleCacheKey **cache, ListBase *bufs)
{
  LinkData *buf;

  if (cache) {
    MEM_freeN(cache);
  }

  for (buf = bufs->first; buf; buf = buf->next) {
    MEM_freeN(buf->data);
  }
  BLI_freelistN(bufs);
}

/************************************************/
/*          Getting stuff                       */
/************************************************/
/* get object's active particle system safely */
ParticleSystem *psys_get_current(Object *ob)
{
  ParticleSystem *psys;
  if (ob == NULL) {
    return NULL;
  }

  for (psys = ob->particlesystem.first; psys; psys = psys->next) {
    if (psys->flag & PSYS_CURRENT) {
      return psys;
    }
  }

  return NULL;
}
short psys_get_current_num(Object *ob)
{
  ParticleSystem *psys;
  short i;

  if (ob == NULL) {
    return 0;
  }

  for (psys = ob->particlesystem.first, i = 0; psys; psys = psys->next, i++) {
    if (psys->flag & PSYS_CURRENT) {
      return i;
    }
  }

  return i;
}
void psys_set_current_num(Object *ob, int index)
{
  ParticleSystem *psys;
  short i;

  if (ob == NULL) {
    return;
  }

  for (psys = ob->particlesystem.first, i = 0; psys; psys = psys->next, i++) {
    if (i == index) {
      psys->flag |= PSYS_CURRENT;
    }
    else {
      psys->flag &= ~PSYS_CURRENT;
    }
  }
}

struct LatticeDeformData *psys_create_lattice_deform_data(ParticleSimulationData *sim)
{
  struct LatticeDeformData *lattice_deform_data = NULL;

  if (psys_in_edit_mode(sim->depsgraph, sim->psys) == 0) {
    Object *lattice = NULL;
    ModifierData *md = (ModifierData *)psys_get_modifier(sim->ob, sim->psys);
    bool for_render = DEG_get_mode(sim->depsgraph) == DAG_EVAL_RENDER;
    int mode = for_render ? eModifierMode_Render : eModifierMode_Realtime;

    for (; md; md = md->next) {
      if (md->type == eModifierType_Lattice) {
        if (md->mode & mode) {
          LatticeModifierData *lmd = (LatticeModifierData *)md;
          lattice = lmd->object;
          sim->psys->lattice_strength = lmd->strength;
        }

        break;
      }
    }
    if (lattice) {
      lattice_deform_data = init_latt_deform(lattice, NULL);
    }
  }

  return lattice_deform_data;
}
void psys_disable_all(Object *ob)
{
  ParticleSystem *psys = ob->particlesystem.first;

  for (; psys; psys = psys->next) {
    psys->flag |= PSYS_DISABLED;
  }
}
void psys_enable_all(Object *ob)
{
  ParticleSystem *psys = ob->particlesystem.first;

  for (; psys; psys = psys->next) {
    psys->flag &= ~PSYS_DISABLED;
  }
}

ParticleSystem *psys_orig_get(ParticleSystem *psys)
{
  if (psys->orig_psys == NULL) {
    return psys;
  }
  return psys->orig_psys;
}

struct ParticleSystem *psys_eval_get(Depsgraph *depsgraph, Object *object, ParticleSystem *psys)
{
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object);
  if (object_eval == object) {
    return psys;
  }
  ParticleSystem *psys_eval = object_eval->particlesystem.first;
  while (psys_eval != NULL) {
    if (psys_eval->orig_psys == psys) {
      return psys_eval;
    }
    psys_eval = psys_eval->next;
  }
  return psys_eval;
}

static PTCacheEdit *psys_orig_edit_get(ParticleSystem *psys)
{
  if (psys->orig_psys == NULL) {
    return psys->edit;
  }
  return psys->orig_psys->edit;
}

bool psys_in_edit_mode(Depsgraph *depsgraph, const ParticleSystem *psys)
{
  const ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
  if (view_layer->basact == NULL) {
    /* TODO(sergey): Needs double-check with multi-object edit. */
    return false;
  }
  const bool use_render_params = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const Object *object = view_layer->basact->object;
  if (object->mode != OB_MODE_PARTICLE_EDIT) {
    return false;
  }
  const ParticleSystem *psys_orig = psys_orig_get((ParticleSystem *)psys);
  return (psys_orig->edit || psys->pointcache->edit) && (use_render_params == false);
}

bool psys_check_enabled(Object *ob, ParticleSystem *psys, const bool use_render_params)
{
  ParticleSystemModifierData *psmd;

  if (psys->flag & PSYS_DISABLED || psys->flag & PSYS_DELETE || !psys->part) {
    return 0;
  }

  psmd = psys_get_modifier(ob, psys);
  if (use_render_params) {
    if (!(psmd->modifier.mode & eModifierMode_Render)) {
      return 0;
    }
  }
  else if (!(psmd->modifier.mode & eModifierMode_Realtime)) {
    return 0;
  }

  return 1;
}

bool psys_check_edited(ParticleSystem *psys)
{
  if (psys->part && psys->part->type == PART_HAIR) {
    return (psys->flag & PSYS_EDITED || (psys->edit && psys->edit->edited));
  }
  else {
    return (psys->pointcache->edit && psys->pointcache->edit->edited);
  }
}

void psys_find_group_weights(ParticleSettings *part)
{
  /* Find object pointers based on index. If the collection is linked from
   * another library linking may not have the object pointers available on
   * file load, so we have to retrieve them later. See T49273. */
  ListBase instance_collection_objects = {NULL, NULL};

  if (part->instance_collection) {
    instance_collection_objects = BKE_collection_object_cache_get(part->instance_collection);
  }

  for (ParticleDupliWeight *dw = part->instance_weights.first; dw; dw = dw->next) {
    if (dw->ob == NULL) {
      Base *base = BLI_findlink(&instance_collection_objects, dw->index);
      if (base != NULL) {
        dw->ob = base->object;
      }
    }
  }
}

void psys_check_group_weights(ParticleSettings *part)
{
  ParticleDupliWeight *dw, *tdw;

  if (part->ren_as != PART_DRAW_GR || !part->instance_collection) {
    BLI_freelistN(&part->instance_weights);
    return;
  }

  /* Find object pointers. */
  psys_find_group_weights(part);

  /* Remove NULL objects, that were removed from the collection. */
  dw = part->instance_weights.first;
  while (dw) {
    if (dw->ob == NULL ||
        !BKE_collection_has_object_recursive(part->instance_collection, dw->ob)) {
      tdw = dw->next;
      BLI_freelinkN(&part->instance_weights, dw);
      dw = tdw;
    }
    else {
      dw = dw->next;
    }
  }

  /* Add new objects in the collection. */
  int index = 0;
  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (part->instance_collection, object) {
    dw = part->instance_weights.first;
    while (dw && dw->ob != object) {
      dw = dw->next;
    }

    if (!dw) {
      dw = MEM_callocN(sizeof(ParticleDupliWeight), "ParticleDupliWeight");
      dw->ob = object;
      dw->count = 1;
      BLI_addtail(&part->instance_weights, dw);
    }

    dw->index = index++;
  }
  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

  /* Ensure there is an element marked as current. */
  int current = 0;
  for (dw = part->instance_weights.first; dw; dw = dw->next) {
    if (dw->flag & PART_DUPLIW_CURRENT) {
      current = 1;
      break;
    }
  }

  if (!current) {
    dw = part->instance_weights.first;
    if (dw) {
      dw->flag |= PART_DUPLIW_CURRENT;
    }
  }
}

int psys_uses_gravity(ParticleSimulationData *sim)
{
  return sim->scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY && sim->psys->part &&
         sim->psys->part->effector_weights->global_gravity != 0.0f;
}
/************************************************/
/*          Freeing stuff                       */
/************************************************/
static void fluid_free_settings(SPHFluidSettings *fluid)
{
  if (fluid) {
    MEM_freeN(fluid);
  }
}

/**
 * Free (or release) any data used by this particle settings (does not free the partsett itself).
 */
void BKE_particlesettings_free(ParticleSettings *part)
{
  int a;

  BKE_animdata_free((ID *)part, false);

  for (a = 0; a < MAX_MTEX; a++) {
    MEM_SAFE_FREE(part->mtex[a]);
  }

  if (part->clumpcurve) {
    curvemapping_free(part->clumpcurve);
  }
  if (part->roughcurve) {
    curvemapping_free(part->roughcurve);
  }
  if (part->twistcurve) {
    curvemapping_free(part->twistcurve);
  }

  BKE_partdeflect_free(part->pd);
  BKE_partdeflect_free(part->pd2);

  MEM_SAFE_FREE(part->effector_weights);

  BLI_freelistN(&part->instance_weights);

  boid_free_settings(part->boids);
  fluid_free_settings(part->fluid);
}

void free_hair(Object *object, ParticleSystem *psys, int dynamics)
{
  PARTICLE_P;

  LOOP_PARTICLES
  {
    if (pa->hair) {
      MEM_freeN(pa->hair);
    }
    pa->hair = NULL;
    pa->totkey = 0;
  }

  psys->flag &= ~PSYS_HAIR_DONE;

  if (psys->clmd) {
    if (dynamics) {
      modifier_free((ModifierData *)psys->clmd);
      psys->clmd = NULL;
      PTCacheID pid;
      BKE_ptcache_id_from_particles(&pid, object, psys);
      BKE_ptcache_id_clear(&pid, PTCACHE_CLEAR_ALL, 0);
    }
    else {
      cloth_free_modifier(psys->clmd);
    }
  }

  if (psys->hair_in_mesh) {
    BKE_id_free(NULL, psys->hair_in_mesh);
  }
  psys->hair_in_mesh = NULL;

  if (psys->hair_out_mesh) {
    BKE_id_free(NULL, psys->hair_out_mesh);
  }
  psys->hair_out_mesh = NULL;
}
void free_keyed_keys(ParticleSystem *psys)
{
  PARTICLE_P;

  if (psys->part->type == PART_HAIR) {
    return;
  }

  if (psys->particles && psys->particles->keys) {
    MEM_freeN(psys->particles->keys);

    LOOP_PARTICLES
    {
      if (pa->keys) {
        pa->keys = NULL;
        pa->totkey = 0;
      }
    }
  }
}
static void free_child_path_cache(ParticleSystem *psys)
{
  psys_free_path_cache_buffers(psys->childcache, &psys->childcachebufs);
  psys->childcache = NULL;
  psys->totchildcache = 0;
}
void psys_free_path_cache(ParticleSystem *psys, PTCacheEdit *edit)
{
  if (edit) {
    psys_free_path_cache_buffers(edit->pathcache, &edit->pathcachebufs);
    edit->pathcache = NULL;
    edit->totcached = 0;
  }
  if (psys) {
    psys_free_path_cache_buffers(psys->pathcache, &psys->pathcachebufs);
    psys->pathcache = NULL;
    psys->totcached = 0;

    free_child_path_cache(psys);
  }
}
void psys_free_children(ParticleSystem *psys)
{
  if (psys->child) {
    MEM_freeN(psys->child);
    psys->child = NULL;
    psys->totchild = 0;
  }

  free_child_path_cache(psys);
}
void psys_free_particles(ParticleSystem *psys)
{
  PARTICLE_P;

  if (psys->particles) {
    /* Even though psys->part should never be NULL,
     * this can happen as an exception during deletion.
     * See ID_REMAP_SKIP/FORCE/FLAG_NEVER_NULL_USAGE in BKE_library_remap. */
    if (psys->part && psys->part->type == PART_HAIR) {
      LOOP_PARTICLES
      {
        if (pa->hair) {
          MEM_freeN(pa->hair);
        }
      }
    }

    if (psys->particles->keys) {
      MEM_freeN(psys->particles->keys);
    }

    if (psys->particles->boid) {
      MEM_freeN(psys->particles->boid);
    }

    MEM_freeN(psys->particles);
    psys->particles = NULL;
    psys->totpart = 0;
  }
}
void psys_free_pdd(ParticleSystem *psys)
{
  if (psys->pdd) {
    if (psys->pdd->cdata) {
      MEM_freeN(psys->pdd->cdata);
    }
    psys->pdd->cdata = NULL;

    if (psys->pdd->vdata) {
      MEM_freeN(psys->pdd->vdata);
    }
    psys->pdd->vdata = NULL;

    if (psys->pdd->ndata) {
      MEM_freeN(psys->pdd->ndata);
    }
    psys->pdd->ndata = NULL;

    if (psys->pdd->vedata) {
      MEM_freeN(psys->pdd->vedata);
    }
    psys->pdd->vedata = NULL;

    psys->pdd->totpoint = 0;
    psys->pdd->totpart = 0;
    psys->pdd->partsize = 0;
  }
}
/* free everything */
void psys_free(Object *ob, ParticleSystem *psys)
{
  if (psys) {
    int nr = 0;
    ParticleSystem *tpsys;

    psys_free_path_cache(psys, NULL);

    /* NOTE: We pass dynamics=0 to free_hair() to prevent it from doing an
     * unneeded clear of the cache. But for historical reason that code path
     * was only clearing cloth part of modifier data.
     *
     * Part of the story there is that particle evaluation is trying to not
     * re-allocate thew ModifierData itself, and limits all allocations to
     * the cloth part of it.
     *
     * Why evaluation is relying on hair_free() and in some specific code
     * paths there is beyond me.
     */
    free_hair(ob, psys, 0);
    if (psys->clmd != NULL) {
      modifier_free((ModifierData *)psys->clmd);
    }

    psys_free_particles(psys);

    if (psys->edit && psys->free_edit) {
      psys->free_edit(psys->edit);
    }

    if (psys->child) {
      MEM_freeN(psys->child);
      psys->child = NULL;
      psys->totchild = 0;
    }

    /* check if we are last non-visible particle system */
    for (tpsys = ob->particlesystem.first; tpsys; tpsys = tpsys->next) {
      if (tpsys->part) {
        if (ELEM(tpsys->part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
          nr++;
          break;
        }
      }
    }
    /* clear do-not-draw-flag */
    if (!nr) {
      ob->transflag &= ~OB_DUPLIPARTS;
    }

    psys->part = NULL;

    if ((psys->flag & PSYS_SHARED_CACHES) == 0) {
      BKE_ptcache_free_list(&psys->ptcaches);
    }
    psys->pointcache = NULL;

    BLI_freelistN(&psys->targets);

    BLI_bvhtree_free(psys->bvhtree);
    BLI_kdtree_3d_free(psys->tree);

    if (psys->fluid_springs) {
      MEM_freeN(psys->fluid_springs);
    }

    BKE_effectors_free(psys->effectors);

    if (psys->pdd) {
      psys_free_pdd(psys);
      MEM_freeN(psys->pdd);
    }

    BKE_particle_batch_cache_free(psys);

    MEM_freeN(psys);
  }
}

void psys_copy_particles(ParticleSystem *psys_dst, ParticleSystem *psys_src)
{
  /* Free existing particles. */
  if (psys_dst->particles != psys_src->particles) {
    psys_free_particles(psys_dst);
  }
  if (psys_dst->child != psys_src->child) {
    psys_free_children(psys_dst);
  }
  /* Restore counters. */
  psys_dst->totpart = psys_src->totpart;
  psys_dst->totchild = psys_src->totchild;
  /* Copy particles and children. */
  psys_dst->particles = MEM_dupallocN(psys_src->particles);
  psys_dst->child = MEM_dupallocN(psys_src->child);
  if (psys_dst->part->type == PART_HAIR) {
    ParticleData *pa;
    int p;
    for (p = 0, pa = psys_dst->particles; p < psys_dst->totpart; p++, pa++) {
      pa->hair = MEM_dupallocN(pa->hair);
    }
  }
  if (psys_dst->particles && (psys_dst->particles->keys || psys_dst->particles->boid)) {
    ParticleKey *key = psys_dst->particles->keys;
    BoidParticle *boid = psys_dst->particles->boid;
    ParticleData *pa;
    int p;
    if (key != NULL) {
      key = MEM_dupallocN(key);
    }
    if (boid != NULL) {
      boid = MEM_dupallocN(boid);
    }
    for (p = 0, pa = psys_dst->particles; p < psys_dst->totpart; p++, pa++) {
      if (boid != NULL) {
        pa->boid = boid++;
      }
      if (key != NULL) {
        pa->keys = key;
        key += pa->totkey;
      }
    }
  }
}

/************************************************/
/*          Interpolation                       */
/************************************************/
static float interpolate_particle_value(
    float v1, float v2, float v3, float v4, const float w[4], int four)
{
  float value;

  value = w[0] * v1 + w[1] * v2 + w[2] * v3;
  if (four) {
    value += w[3] * v4;
  }

  CLAMP(value, 0.f, 1.f);

  return value;
}

void psys_interpolate_particle(
    short type, ParticleKey keys[4], float dt, ParticleKey *result, bool velocity)
{
  float t[4];

  if (type < 0) {
    interp_cubic_v3(result->co, result->vel, keys[1].co, keys[1].vel, keys[2].co, keys[2].vel, dt);
  }
  else {
    key_curve_position_weights(dt, t, type);

    interp_v3_v3v3v3v3(result->co, keys[0].co, keys[1].co, keys[2].co, keys[3].co, t);

    if (velocity) {
      float temp[3];

      if (dt > 0.999f) {
        key_curve_position_weights(dt - 0.001f, t, type);
        interp_v3_v3v3v3v3(temp, keys[0].co, keys[1].co, keys[2].co, keys[3].co, t);
        sub_v3_v3v3(result->vel, result->co, temp);
      }
      else {
        key_curve_position_weights(dt + 0.001f, t, type);
        interp_v3_v3v3v3v3(temp, keys[0].co, keys[1].co, keys[2].co, keys[3].co, t);
        sub_v3_v3v3(result->vel, temp, result->co);
      }
    }
  }
}

typedef struct ParticleInterpolationData {
  HairKey *hkey[2];

  Mesh *mesh;
  MVert *mvert[2];

  int keyed;
  ParticleKey *kkey[2];

  PointCache *cache;
  PTCacheMem *pm;

  PTCacheEditPoint *epoint;
  PTCacheEditKey *ekey[2];

  float birthtime, dietime;
  int bspline;
} ParticleInterpolationData;
/**
 * Assumes pointcache->mem_cache exists, so for disk cached particles
 * call #psys_make_temp_pointcache() before use.
 * It uses #ParticleInterpolationData.pm to store the current memory cache frame
 * so it's thread safe.
 */
static void get_pointcache_keys_for_time(Object *UNUSED(ob),
                                         PointCache *cache,
                                         PTCacheMem **cur,
                                         int index,
                                         float t,
                                         ParticleKey *key1,
                                         ParticleKey *key2)
{
  static PTCacheMem *pm = NULL;
  int index1, index2;

  if (index < 0) { /* initialize */
    *cur = cache->mem_cache.first;

    if (*cur) {
      *cur = (*cur)->next;
    }
  }
  else {
    if (*cur) {
      while (*cur && (*cur)->next && (float)(*cur)->frame < t) {
        *cur = (*cur)->next;
      }

      pm = *cur;

      index2 = BKE_ptcache_mem_index_find(pm, index);
      index1 = BKE_ptcache_mem_index_find(pm->prev, index);
      if (index2 < 0) {
        return;
      }

      BKE_ptcache_make_particle_key(key2, index2, pm->data, (float)pm->frame);
      if (index1 < 0) {
        copy_particle_key(key1, key2, 1);
      }
      else {
        BKE_ptcache_make_particle_key(key1, index1, pm->prev->data, (float)pm->prev->frame);
      }
    }
    else if (cache->mem_cache.first) {
      pm = cache->mem_cache.first;
      index2 = BKE_ptcache_mem_index_find(pm, index);
      if (index2 < 0) {
        return;
      }
      BKE_ptcache_make_particle_key(key2, index2, pm->data, (float)pm->frame);
      copy_particle_key(key1, key2, 1);
    }
  }
}
static int get_pointcache_times_for_particle(PointCache *cache,
                                             int index,
                                             float *start,
                                             float *end)
{
  PTCacheMem *pm;
  int ret = 0;

  for (pm = cache->mem_cache.first; pm; pm = pm->next) {
    if (BKE_ptcache_mem_index_find(pm, index) >= 0) {
      *start = pm->frame;
      ret++;
      break;
    }
  }

  for (pm = cache->mem_cache.last; pm; pm = pm->prev) {
    if (BKE_ptcache_mem_index_find(pm, index) >= 0) {
      *end = pm->frame;
      ret++;
      break;
    }
  }

  return ret == 2;
}

float psys_get_dietime_from_cache(PointCache *cache, int index)
{
  PTCacheMem *pm;
  int dietime = 10000000; /* some max value so that we can default to pa->time+lifetime */

  for (pm = cache->mem_cache.last; pm; pm = pm->prev) {
    if (BKE_ptcache_mem_index_find(pm, index) >= 0) {
      return (float)pm->frame;
    }
  }

  return (float)dietime;
}

static void init_particle_interpolation(Object *ob,
                                        ParticleSystem *psys,
                                        ParticleData *pa,
                                        ParticleInterpolationData *pind)
{

  if (pind->epoint) {
    PTCacheEditPoint *point = pind->epoint;

    pind->ekey[0] = point->keys;
    pind->ekey[1] = point->totkey > 1 ? point->keys + 1 : NULL;

    pind->birthtime = *(point->keys->time);
    pind->dietime = *((point->keys + point->totkey - 1)->time);
  }
  else if (pind->keyed) {
    ParticleKey *key = pa->keys;
    pind->kkey[0] = key;
    pind->kkey[1] = pa->totkey > 1 ? key + 1 : NULL;

    pind->birthtime = key->time;
    pind->dietime = (key + pa->totkey - 1)->time;
  }
  else if (pind->cache) {
    float start = 0.0f, end = 0.0f;
    get_pointcache_keys_for_time(ob, pind->cache, &pind->pm, -1, 0.0f, NULL, NULL);
    pind->birthtime = pa ? pa->time : pind->cache->startframe;
    pind->dietime = pa ? pa->dietime : pind->cache->endframe;

    if (get_pointcache_times_for_particle(pind->cache, pa - psys->particles, &start, &end)) {
      pind->birthtime = MAX2(pind->birthtime, start);
      pind->dietime = MIN2(pind->dietime, end);
    }
  }
  else {
    HairKey *key = pa->hair;
    pind->hkey[0] = key;
    pind->hkey[1] = key + 1;

    pind->birthtime = key->time;
    pind->dietime = (key + pa->totkey - 1)->time;

    if (pind->mesh) {
      pind->mvert[0] = &pind->mesh->mvert[pa->hair_index];
      pind->mvert[1] = pind->mvert[0] + 1;
    }
  }
}
static void edit_to_particle(ParticleKey *key, PTCacheEditKey *ekey)
{
  copy_v3_v3(key->co, ekey->co);
  if (ekey->vel) {
    copy_v3_v3(key->vel, ekey->vel);
  }
  key->time = *(ekey->time);
}
static void hair_to_particle(ParticleKey *key, HairKey *hkey)
{
  copy_v3_v3(key->co, hkey->co);
  key->time = hkey->time;
}

static void mvert_to_particle(ParticleKey *key, MVert *mvert, HairKey *hkey)
{
  copy_v3_v3(key->co, mvert->co);
  key->time = hkey->time;
}

static void do_particle_interpolation(ParticleSystem *psys,
                                      int p,
                                      ParticleData *pa,
                                      float t,
                                      ParticleInterpolationData *pind,
                                      ParticleKey *result)
{
  PTCacheEditPoint *point = pind->epoint;
  ParticleKey keys[4];
  int point_vel = (point && point->keys->vel);
  float real_t, dfra, keytime, invdt = 1.f;

  /* billboards wont fill in all of these, so start cleared */
  memset(keys, 0, sizeof(keys));

  /* interpret timing and find keys */
  if (point) {
    if (result->time < 0.0f) {
      real_t = -result->time;
    }
    else {
      real_t = *(pind->ekey[0]->time) +
               t * (*(pind->ekey[0][point->totkey - 1].time) - *(pind->ekey[0]->time));
    }

    while (*(pind->ekey[1]->time) < real_t) {
      pind->ekey[1]++;
    }

    pind->ekey[0] = pind->ekey[1] - 1;
  }
  else if (pind->keyed) {
    /* we have only one key, so let's use that */
    if (pind->kkey[1] == NULL) {
      copy_particle_key(result, pind->kkey[0], 1);
      return;
    }

    if (result->time < 0.0f) {
      real_t = -result->time;
    }
    else {
      real_t = pind->kkey[0]->time +
               t * (pind->kkey[0][pa->totkey - 1].time - pind->kkey[0]->time);
    }

    if (psys->part->phystype == PART_PHYS_KEYED && psys->flag & PSYS_KEYED_TIMING) {
      ParticleTarget *pt = psys->targets.first;

      pt = pt->next;

      while (pt && pa->time + pt->time < real_t) {
        pt = pt->next;
      }

      if (pt) {
        pt = pt->prev;

        if (pa->time + pt->time + pt->duration > real_t) {
          real_t = pa->time + pt->time;
        }
      }
      else {
        real_t = pa->time + ((ParticleTarget *)psys->targets.last)->time;
      }
    }

    CLAMP(real_t, pa->time, pa->dietime);

    while (pind->kkey[1]->time < real_t) {
      pind->kkey[1]++;
    }

    pind->kkey[0] = pind->kkey[1] - 1;
  }
  else if (pind->cache) {
    if (result->time < 0.0f) { /* flag for time in frames */
      real_t = -result->time;
    }
    else {
      real_t = pa->time + t * (pa->dietime - pa->time);
    }
  }
  else {
    if (result->time < 0.0f) {
      real_t = -result->time;
    }
    else {
      real_t = pind->hkey[0]->time +
               t * (pind->hkey[0][pa->totkey - 1].time - pind->hkey[0]->time);
    }

    while (pind->hkey[1]->time < real_t) {
      pind->hkey[1]++;
      pind->mvert[1]++;
    }

    pind->hkey[0] = pind->hkey[1] - 1;
  }

  /* set actual interpolation keys */
  if (point) {
    edit_to_particle(keys + 1, pind->ekey[0]);
    edit_to_particle(keys + 2, pind->ekey[1]);
  }
  else if (pind->mesh) {
    pind->mvert[0] = pind->mvert[1] - 1;
    mvert_to_particle(keys + 1, pind->mvert[0], pind->hkey[0]);
    mvert_to_particle(keys + 2, pind->mvert[1], pind->hkey[1]);
  }
  else if (pind->keyed) {
    memcpy(keys + 1, pind->kkey[0], sizeof(ParticleKey));
    memcpy(keys + 2, pind->kkey[1], sizeof(ParticleKey));
  }
  else if (pind->cache) {
    get_pointcache_keys_for_time(NULL, pind->cache, &pind->pm, p, real_t, keys + 1, keys + 2);
  }
  else {
    hair_to_particle(keys + 1, pind->hkey[0]);
    hair_to_particle(keys + 2, pind->hkey[1]);
  }

  /* set secondary interpolation keys for hair */
  if (!pind->keyed && !pind->cache && !point_vel) {
    if (point) {
      if (pind->ekey[0] != point->keys) {
        edit_to_particle(keys, pind->ekey[0] - 1);
      }
      else {
        edit_to_particle(keys, pind->ekey[0]);
      }
    }
    else if (pind->mesh) {
      if (pind->hkey[0] != pa->hair) {
        mvert_to_particle(keys, pind->mvert[0] - 1, pind->hkey[0] - 1);
      }
      else {
        mvert_to_particle(keys, pind->mvert[0], pind->hkey[0]);
      }
    }
    else {
      if (pind->hkey[0] != pa->hair) {
        hair_to_particle(keys, pind->hkey[0] - 1);
      }
      else {
        hair_to_particle(keys, pind->hkey[0]);
      }
    }

    if (point) {
      if (pind->ekey[1] != point->keys + point->totkey - 1) {
        edit_to_particle(keys + 3, pind->ekey[1] + 1);
      }
      else {
        edit_to_particle(keys + 3, pind->ekey[1]);
      }
    }
    else if (pind->mesh) {
      if (pind->hkey[1] != pa->hair + pa->totkey - 1) {
        mvert_to_particle(keys + 3, pind->mvert[1] + 1, pind->hkey[1] + 1);
      }
      else {
        mvert_to_particle(keys + 3, pind->mvert[1], pind->hkey[1]);
      }
    }
    else {
      if (pind->hkey[1] != pa->hair + pa->totkey - 1) {
        hair_to_particle(keys + 3, pind->hkey[1] + 1);
      }
      else {
        hair_to_particle(keys + 3, pind->hkey[1]);
      }
    }
  }

  dfra = keys[2].time - keys[1].time;
  keytime = (real_t - keys[1].time) / dfra;

  /* convert velocity to timestep size */
  if (pind->keyed || pind->cache || point_vel) {
    invdt = dfra * 0.04f * (psys ? psys->part->timetweak : 1.f);
    mul_v3_fl(keys[1].vel, invdt);
    mul_v3_fl(keys[2].vel, invdt);
    interp_qt_qtqt(result->rot, keys[1].rot, keys[2].rot, keytime);
  }

  /* Now we should have in chronologiacl order k1<=k2<=t<=k3<=k4 with keytime between
   * [0, 1]->[k2, k3] (k1 & k4 used for cardinal & bspline interpolation). */
  psys_interpolate_particle((pind->keyed || pind->cache || point_vel) ?
                                -1 /* signal for cubic interpolation */
                                :
                                (pind->bspline ? KEY_BSPLINE : KEY_CARDINAL),
                            keys,
                            keytime,
                            result,
                            1);

  /* the velocity needs to be converted back from cubic interpolation */
  if (pind->keyed || pind->cache || point_vel) {
    mul_v3_fl(result->vel, 1.f / invdt);
  }
}

static void interpolate_pathcache(ParticleCacheKey *first, float t, ParticleCacheKey *result)
{
  int i = 0;
  ParticleCacheKey *cur = first;

  /* scale the requested time to fit the entire path even if the path is cut early */
  t *= (first + first->segments)->time;

  while (i < first->segments && cur->time < t) {
    cur++;
  }

  if (cur->time == t) {
    *result = *cur;
  }
  else {
    float dt = (t - (cur - 1)->time) / (cur->time - (cur - 1)->time);
    interp_v3_v3v3(result->co, (cur - 1)->co, cur->co, dt);
    interp_v3_v3v3(result->vel, (cur - 1)->vel, cur->vel, dt);
    interp_qt_qtqt(result->rot, (cur - 1)->rot, cur->rot, dt);
    result->time = t;
  }

  /* first is actual base rotation, others are incremental from first */
  if (cur == first || cur - 1 == first) {
    copy_qt_qt(result->rot, first->rot);
  }
  else {
    mul_qt_qtqt(result->rot, first->rot, result->rot);
  }
}

/************************************************/
/*          Particles on a dm                   */
/************************************************/
/* interpolate a location on a face based on face coordinates */
void psys_interpolate_face(MVert *mvert,
                           MFace *mface,
                           MTFace *tface,
                           float (*orcodata)[3],
                           float w[4],
                           float vec[3],
                           float nor[3],
                           float utan[3],
                           float vtan[3],
                           float orco[3])
{
  float *v1 = 0, *v2 = 0, *v3 = 0, *v4 = 0;
  float e1[3], e2[3], s1, s2, t1, t2;
  float *uv1, *uv2, *uv3, *uv4;
  float n1[3], n2[3], n3[3], n4[3];
  float tuv[4][2];
  float *o1, *o2, *o3, *o4;

  v1 = mvert[mface->v1].co;
  v2 = mvert[mface->v2].co;
  v3 = mvert[mface->v3].co;

  normal_short_to_float_v3(n1, mvert[mface->v1].no);
  normal_short_to_float_v3(n2, mvert[mface->v2].no);
  normal_short_to_float_v3(n3, mvert[mface->v3].no);

  if (mface->v4) {
    v4 = mvert[mface->v4].co;
    normal_short_to_float_v3(n4, mvert[mface->v4].no);

    interp_v3_v3v3v3v3(vec, v1, v2, v3, v4, w);

    if (nor) {
      if (mface->flag & ME_SMOOTH) {
        interp_v3_v3v3v3v3(nor, n1, n2, n3, n4, w);
      }
      else {
        normal_quad_v3(nor, v1, v2, v3, v4);
      }
    }
  }
  else {
    interp_v3_v3v3v3(vec, v1, v2, v3, w);

    if (nor) {
      if (mface->flag & ME_SMOOTH) {
        interp_v3_v3v3v3(nor, n1, n2, n3, w);
      }
      else {
        normal_tri_v3(nor, v1, v2, v3);
      }
    }
  }

  /* calculate tangent vectors */
  if (utan && vtan) {
    if (tface) {
      uv1 = tface->uv[0];
      uv2 = tface->uv[1];
      uv3 = tface->uv[2];
      uv4 = tface->uv[3];
    }
    else {
      uv1 = tuv[0];
      uv2 = tuv[1];
      uv3 = tuv[2];
      uv4 = tuv[3];
      map_to_sphere(uv1, uv1 + 1, v1[0], v1[1], v1[2]);
      map_to_sphere(uv2, uv2 + 1, v2[0], v2[1], v2[2]);
      map_to_sphere(uv3, uv3 + 1, v3[0], v3[1], v3[2]);
      if (v4) {
        map_to_sphere(uv4, uv4 + 1, v4[0], v4[1], v4[2]);
      }
    }

    if (v4) {
      s1 = uv3[0] - uv1[0];
      s2 = uv4[0] - uv1[0];

      t1 = uv3[1] - uv1[1];
      t2 = uv4[1] - uv1[1];

      sub_v3_v3v3(e1, v3, v1);
      sub_v3_v3v3(e2, v4, v1);
    }
    else {
      s1 = uv2[0] - uv1[0];
      s2 = uv3[0] - uv1[0];

      t1 = uv2[1] - uv1[1];
      t2 = uv3[1] - uv1[1];

      sub_v3_v3v3(e1, v2, v1);
      sub_v3_v3v3(e2, v3, v1);
    }

    vtan[0] = (s1 * e2[0] - s2 * e1[0]);
    vtan[1] = (s1 * e2[1] - s2 * e1[1]);
    vtan[2] = (s1 * e2[2] - s2 * e1[2]);

    utan[0] = (t1 * e2[0] - t2 * e1[0]);
    utan[1] = (t1 * e2[1] - t2 * e1[1]);
    utan[2] = (t1 * e2[2] - t2 * e1[2]);
  }

  if (orco) {
    if (orcodata) {
      o1 = orcodata[mface->v1];
      o2 = orcodata[mface->v2];
      o3 = orcodata[mface->v3];

      if (mface->v4) {
        o4 = orcodata[mface->v4];

        interp_v3_v3v3v3v3(orco, o1, o2, o3, o4, w);
      }
      else {
        interp_v3_v3v3v3(orco, o1, o2, o3, w);
      }
    }
    else {
      copy_v3_v3(orco, vec);
    }
  }
}
void psys_interpolate_uvs(const MTFace *tface, int quad, const float w[4], float uvco[2])
{
  float v10 = tface->uv[0][0];
  float v11 = tface->uv[0][1];
  float v20 = tface->uv[1][0];
  float v21 = tface->uv[1][1];
  float v30 = tface->uv[2][0];
  float v31 = tface->uv[2][1];
  float v40, v41;

  if (quad) {
    v40 = tface->uv[3][0];
    v41 = tface->uv[3][1];

    uvco[0] = w[0] * v10 + w[1] * v20 + w[2] * v30 + w[3] * v40;
    uvco[1] = w[0] * v11 + w[1] * v21 + w[2] * v31 + w[3] * v41;
  }
  else {
    uvco[0] = w[0] * v10 + w[1] * v20 + w[2] * v30;
    uvco[1] = w[0] * v11 + w[1] * v21 + w[2] * v31;
  }
}

void psys_interpolate_mcol(const MCol *mcol, int quad, const float w[4], MCol *mc)
{
  const char *cp1, *cp2, *cp3, *cp4;
  char *cp;

  cp = (char *)mc;
  cp1 = (const char *)&mcol[0];
  cp2 = (const char *)&mcol[1];
  cp3 = (const char *)&mcol[2];

  if (quad) {
    cp4 = (char *)&mcol[3];

    cp[0] = (int)(w[0] * cp1[0] + w[1] * cp2[0] + w[2] * cp3[0] + w[3] * cp4[0]);
    cp[1] = (int)(w[0] * cp1[1] + w[1] * cp2[1] + w[2] * cp3[1] + w[3] * cp4[1]);
    cp[2] = (int)(w[0] * cp1[2] + w[1] * cp2[2] + w[2] * cp3[2] + w[3] * cp4[2]);
    cp[3] = (int)(w[0] * cp1[3] + w[1] * cp2[3] + w[2] * cp3[3] + w[3] * cp4[3]);
  }
  else {
    cp[0] = (int)(w[0] * cp1[0] + w[1] * cp2[0] + w[2] * cp3[0]);
    cp[1] = (int)(w[0] * cp1[1] + w[1] * cp2[1] + w[2] * cp3[1]);
    cp[2] = (int)(w[0] * cp1[2] + w[1] * cp2[2] + w[2] * cp3[2]);
    cp[3] = (int)(w[0] * cp1[3] + w[1] * cp2[3] + w[2] * cp3[3]);
  }
}

static float psys_interpolate_value_from_verts(
    Mesh *mesh, short from, int index, const float fw[4], const float *values)
{
  if (values == 0 || index == -1) {
    return 0.0;
  }

  switch (from) {
    case PART_FROM_VERT:
      return values[index];
    case PART_FROM_FACE:
    case PART_FROM_VOLUME: {
      MFace *mf = &mesh->mface[index];
      return interpolate_particle_value(
          values[mf->v1], values[mf->v2], values[mf->v3], values[mf->v4], fw, mf->v4);
    }
  }
  return 0.0f;
}

/* conversion of pa->fw to origspace layer coordinates */
static void psys_w_to_origspace(const float w[4], float uv[2])
{
  uv[0] = w[1] + w[2];
  uv[1] = w[2] + w[3];
}

/* conversion of pa->fw to weights in face from origspace */
static void psys_origspace_to_w(OrigSpaceFace *osface, int quad, const float w[4], float neww[4])
{
  float v[4][3], co[3];

  v[0][0] = osface->uv[0][0];
  v[0][1] = osface->uv[0][1];
  v[0][2] = 0.0f;
  v[1][0] = osface->uv[1][0];
  v[1][1] = osface->uv[1][1];
  v[1][2] = 0.0f;
  v[2][0] = osface->uv[2][0];
  v[2][1] = osface->uv[2][1];
  v[2][2] = 0.0f;

  psys_w_to_origspace(w, co);
  co[2] = 0.0f;

  if (quad) {
    v[3][0] = osface->uv[3][0];
    v[3][1] = osface->uv[3][1];
    v[3][2] = 0.0f;
    interp_weights_poly_v3(neww, v, 4, co);
  }
  else {
    interp_weights_poly_v3(neww, v, 3, co);
    neww[3] = 0.0f;
  }
}

/**
 * Find the final derived mesh tessface for a particle, from its original tessface index.
 * This is slow and can be optimized but only for many lookups.
 *
 * \param mesh_final: Final mesh, it may not have the same topology as original mesh.
 * \param mesh_original: Original mesh, use for accessing #MPoly to #MFace mapping.
 * \param findex_orig: The input tessface index.
 * \param fw: Face weights (position of the particle inside the \a findex_orig tessface).
 * \param poly_nodes: May be NULL, otherwise an array of linked list,
 * one for each final \a mesh_final polygon, containing all its tessfaces indices.
 * \return The \a mesh_final tessface index.
 */
int psys_particle_dm_face_lookup(Mesh *mesh_final,
                                 Mesh *mesh_original,
                                 int findex_orig,
                                 const float fw[4],
                                 struct LinkNode **poly_nodes)
{
  MFace *mtessface_final;
  OrigSpaceFace *osface_final;
  int pindex_orig;
  float uv[2], (*faceuv)[2];

  const int *index_mf_to_mpoly_deformed = NULL;
  const int *index_mf_to_mpoly = NULL;
  const int *index_mp_to_orig = NULL;

  const int totface_final = mesh_final->totface;
  const int totface_deformed = mesh_original ? mesh_original->totface : totface_final;

  if (ELEM(0, totface_final, totface_deformed)) {
    return DMCACHE_NOTFOUND;
  }

  index_mf_to_mpoly = CustomData_get_layer(&mesh_final->fdata, CD_ORIGINDEX);
  index_mp_to_orig = CustomData_get_layer(&mesh_final->pdata, CD_ORIGINDEX);
  BLI_assert(index_mf_to_mpoly);

  if (mesh_original) {
    index_mf_to_mpoly_deformed = CustomData_get_layer(&mesh_original->fdata, CD_ORIGINDEX);
  }
  else {
    BLI_assert(mesh_final->runtime.deformed_only);
    index_mf_to_mpoly_deformed = index_mf_to_mpoly;
  }
  BLI_assert(index_mf_to_mpoly_deformed);

  pindex_orig = index_mf_to_mpoly_deformed[findex_orig];

  if (mesh_original == NULL) {
    mesh_original = mesh_final;
  }

  index_mf_to_mpoly_deformed = NULL;

  mtessface_final = mesh_final->mface;
  osface_final = CustomData_get_layer(&mesh_final->fdata, CD_ORIGSPACE);

  if (osface_final == NULL) {
    /* Assume we don't need osface_final data, and we get a direct 1-1 mapping... */
    if (findex_orig < totface_final) {
      // printf("\tNO CD_ORIGSPACE, assuming not needed\n");
      return findex_orig;
    }
    else {
      printf("\tNO CD_ORIGSPACE, error out of range\n");
      return DMCACHE_NOTFOUND;
    }
  }
  else if (findex_orig >= mesh_original->totface) {
    return DMCACHE_NOTFOUND; /* index not in the original mesh */
  }

  psys_w_to_origspace(fw, uv);

  if (poly_nodes) {
    /* we can have a restricted linked list of faces to check, faster! */
    LinkNode *tessface_node = poly_nodes[pindex_orig];

    for (; tessface_node; tessface_node = tessface_node->next) {
      int findex_dst = POINTER_AS_INT(tessface_node->link);
      faceuv = osface_final[findex_dst].uv;

      /* check that this intersects - Its possible this misses :/ -
       * could also check its not between */
      if (mtessface_final[findex_dst].v4) {
        if (isect_point_quad_v2(uv, faceuv[0], faceuv[1], faceuv[2], faceuv[3])) {
          return findex_dst;
        }
      }
      else if (isect_point_tri_v2(uv, faceuv[0], faceuv[1], faceuv[2])) {
        return findex_dst;
      }
    }
  }
  else { /* if we have no node, try every face */
    for (int findex_dst = 0; findex_dst < totface_final; findex_dst++) {
      /* If current tessface from 'final' DM and orig tessface (given by index)
       * map to the same orig poly. */
      if (BKE_mesh_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, findex_dst) ==
          pindex_orig) {
        faceuv = osface_final[findex_dst].uv;

        /* check that this intersects - Its possible this misses :/ -
         * could also check its not between */
        if (mtessface_final[findex_dst].v4) {
          if (isect_point_quad_v2(uv, faceuv[0], faceuv[1], faceuv[2], faceuv[3])) {
            return findex_dst;
          }
        }
        else if (isect_point_tri_v2(uv, faceuv[0], faceuv[1], faceuv[2])) {
          return findex_dst;
        }
      }
    }
  }

  return DMCACHE_NOTFOUND;
}

static int psys_map_index_on_dm(Mesh *mesh,
                                int from,
                                int index,
                                int index_dmcache,
                                const float fw[4],
                                float UNUSED(foffset),
                                int *mapindex,
                                float mapfw[4])
{
  if (index < 0) {
    return 0;
  }

  if (mesh->runtime.deformed_only || index_dmcache == DMCACHE_ISCHILD) {
    /* for meshes that are either only deformed or for child particles, the
     * index and fw do not require any mapping, so we can directly use it */
    if (from == PART_FROM_VERT) {
      if (index >= mesh->totvert) {
        return 0;
      }

      *mapindex = index;
    }
    else { /* FROM_FACE/FROM_VOLUME */
      if (index >= mesh->totface) {
        return 0;
      }

      *mapindex = index;
      copy_v4_v4(mapfw, fw);
    }
  }
  else {
    /* for other meshes that have been modified, we try to map the particle
     * to their new location, which means a different index, and for faces
     * also a new face interpolation weights */
    if (from == PART_FROM_VERT) {
      if (index_dmcache == DMCACHE_NOTFOUND || index_dmcache > mesh->totvert) {
        return 0;
      }

      *mapindex = index_dmcache;
    }
    else { /* FROM_FACE/FROM_VOLUME */
           /* find a face on the derived mesh that uses this face */
      MFace *mface;
      OrigSpaceFace *osface;
      int i;

      i = index_dmcache;

      if (i == DMCACHE_NOTFOUND || i >= mesh->totface) {
        return 0;
      }

      *mapindex = i;

      /* modify the original weights to become
       * weights for the derived mesh face */
      osface = CustomData_get_layer(&mesh->fdata, CD_ORIGSPACE);
      mface = &mesh->mface[i];

      if (osface == NULL) {
        mapfw[0] = mapfw[1] = mapfw[2] = mapfw[3] = 0.0f;
      }
      else {
        psys_origspace_to_w(&osface[i], mface->v4, fw, mapfw);
      }
    }
  }

  return 1;
}

/* interprets particle data to get a point on a mesh in object space */
void psys_particle_on_dm(Mesh *mesh_final,
                         int from,
                         int index,
                         int index_dmcache,
                         const float fw[4],
                         float foffset,
                         float vec[3],
                         float nor[3],
                         float utan[3],
                         float vtan[3],
                         float orco[3])
{
  float tmpnor[3], mapfw[4];
  float(*orcodata)[3];
  int mapindex;

  if (!psys_map_index_on_dm(
          mesh_final, from, index, index_dmcache, fw, foffset, &mapindex, mapfw)) {
    if (vec) {
      vec[0] = vec[1] = vec[2] = 0.0;
    }
    if (nor) {
      nor[0] = nor[1] = 0.0;
      nor[2] = 1.0;
    }
    if (orco) {
      orco[0] = orco[1] = orco[2] = 0.0;
    }
    if (utan) {
      utan[0] = utan[1] = utan[2] = 0.0;
    }
    if (vtan) {
      vtan[0] = vtan[1] = vtan[2] = 0.0;
    }

    return;
  }

  orcodata = CustomData_get_layer(&mesh_final->vdata, CD_ORCO);

  if (from == PART_FROM_VERT) {
    copy_v3_v3(vec, mesh_final->mvert[mapindex].co);

    if (nor) {
      normal_short_to_float_v3(nor, mesh_final->mvert[mapindex].no);
      normalize_v3(nor);
    }

    if (orco) {
      if (orcodata) {
        copy_v3_v3(orco, orcodata[mapindex]);
      }
      else {
        copy_v3_v3(orco, vec);
      }
    }

    if (utan && vtan) {
      utan[0] = utan[1] = utan[2] = 0.0f;
      vtan[0] = vtan[1] = vtan[2] = 0.0f;
    }
  }
  else { /* PART_FROM_FACE / PART_FROM_VOLUME */
    MFace *mface;
    MTFace *mtface;
    MVert *mvert;

    mface = &mesh_final->mface[mapindex];
    mvert = mesh_final->mvert;
    mtface = mesh_final->mtface;

    if (mtface) {
      mtface += mapindex;
    }

    if (from == PART_FROM_VOLUME) {
      psys_interpolate_face(mvert, mface, mtface, orcodata, mapfw, vec, tmpnor, utan, vtan, orco);
      if (nor) {
        copy_v3_v3(nor, tmpnor);
      }

      normalize_v3(
          tmpnor); /* XXX Why not normalize tmpnor before copying it into nor??? -- mont29 */
      mul_v3_fl(tmpnor, -foffset);
      add_v3_v3(vec, tmpnor);
    }
    else {
      psys_interpolate_face(mvert, mface, mtface, orcodata, mapfw, vec, nor, utan, vtan, orco);
    }
  }
}

float psys_particle_value_from_verts(Mesh *mesh, short from, ParticleData *pa, float *values)
{
  float mapfw[4];
  int mapindex;

  if (!psys_map_index_on_dm(
          mesh, from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, &mapindex, mapfw)) {
    return 0.0f;
  }

  return psys_interpolate_value_from_verts(mesh, from, mapindex, mapfw, values);
}

ParticleSystemModifierData *psys_get_modifier(Object *ob, ParticleSystem *psys)
{
  ModifierData *md;
  ParticleSystemModifierData *psmd;

  for (md = ob->modifiers.first; md; md = md->next) {
    if (md->type == eModifierType_ParticleSystem) {
      psmd = (ParticleSystemModifierData *)md;
      if (psmd->psys == psys) {
        return psmd;
      }
    }
  }
  return NULL;
}
/************************************************/
/*          Particles on a shape                */
/************************************************/
/* ready for future use */
static void psys_particle_on_shape(int UNUSED(distr),
                                   int UNUSED(index),
                                   float *UNUSED(fuv),
                                   float vec[3],
                                   float nor[3],
                                   float utan[3],
                                   float vtan[3],
                                   float orco[3])
{
  /* TODO */
  float zerovec[3] = {0.0f, 0.0f, 0.0f};
  if (vec) {
    copy_v3_v3(vec, zerovec);
  }
  if (nor) {
    copy_v3_v3(nor, zerovec);
  }
  if (utan) {
    copy_v3_v3(utan, zerovec);
  }
  if (vtan) {
    copy_v3_v3(vtan, zerovec);
  }
  if (orco) {
    copy_v3_v3(orco, zerovec);
  }
}
/************************************************/
/*          Particles on emitter                */
/************************************************/

void psys_emitter_customdata_mask(ParticleSystem *psys, CustomData_MeshMasks *r_cddata_masks)
{
  MTex *mtex;
  int i;

  if (!psys->part) {
    return;
  }

  for (i = 0; i < MAX_MTEX; i++) {
    mtex = psys->part->mtex[i];
    if (mtex && mtex->mapto && (mtex->texco & TEXCO_UV)) {
      r_cddata_masks->fmask |= CD_MASK_MTFACE;
    }
  }

  if (psys->part->tanfac != 0.0f) {
    r_cddata_masks->fmask |= CD_MASK_MTFACE;
  }

  /* ask for vertexgroups if we need them */
  for (i = 0; i < PSYS_TOT_VG; i++) {
    if (psys->vgroup[i]) {
      r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
      break;
    }
  }

  /* particles only need this if they are after a non deform modifier, and
   * the modifier stack will only create them in that case. */
  r_cddata_masks->lmask |= CD_MASK_ORIGSPACE_MLOOP;
  /* XXX Check we do need all those? */
  r_cddata_masks->vmask |= CD_MASK_ORIGINDEX;
  r_cddata_masks->emask |= CD_MASK_ORIGINDEX;
  r_cddata_masks->pmask |= CD_MASK_ORIGINDEX;

  r_cddata_masks->vmask |= CD_MASK_ORCO;
}

void psys_particle_on_emitter(ParticleSystemModifierData *psmd,
                              int from,
                              int index,
                              int index_dmcache,
                              float fuv[4],
                              float foffset,
                              float vec[3],
                              float nor[3],
                              float utan[3],
                              float vtan[3],
                              float orco[3])
{
  Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(psmd);
  if (psmd && mesh_final) {
    if (psmd->psys->part->distr == PART_DISTR_GRID && psmd->psys->part->from != PART_FROM_VERT) {
      if (vec) {
        copy_v3_v3(vec, fuv);
      }

      if (orco) {
        copy_v3_v3(orco, fuv);
      }
      return;
    }
    /* we cant use the num_dmcache */
    psys_particle_on_dm(
        mesh_final, from, index, index_dmcache, fuv, foffset, vec, nor, utan, vtan, orco);
  }
  else {
    psys_particle_on_shape(from, index, fuv, vec, nor, utan, vtan, orco);
  }
}
/************************************************/
/*          Path Cache                          */
/************************************************/

void precalc_guides(ParticleSimulationData *sim, ListBase *effectors)
{
  EffectedPoint point;
  ParticleKey state;
  EffectorData efd;
  EffectorCache *eff;
  ParticleSystem *psys = sim->psys;
  EffectorWeights *weights = sim->psys->part->effector_weights;
  GuideEffectorData *data;
  PARTICLE_P;

  if (!effectors) {
    return;
  }

  LOOP_PARTICLES
  {
    psys_particle_on_emitter(sim->psmd,
                             sim->psys->part->from,
                             pa->num,
                             pa->num_dmcache,
                             pa->fuv,
                             pa->foffset,
                             state.co,
                             0,
                             0,
                             0,
                             0);

    mul_m4_v3(sim->ob->obmat, state.co);
    mul_mat3_m4_v3(sim->ob->obmat, state.vel);

    pd_point_from_particle(sim, pa, &state, &point);

    for (eff = effectors->first; eff; eff = eff->next) {
      if (eff->pd->forcefield != PFIELD_GUIDE) {
        continue;
      }

      if (!eff->guide_data) {
        eff->guide_data = MEM_callocN(sizeof(GuideEffectorData) * psys->totpart,
                                      "GuideEffectorData");
      }

      data = eff->guide_data + p;

      sub_v3_v3v3(efd.vec_to_point, state.co, eff->guide_loc);
      copy_v3_v3(efd.nor, eff->guide_dir);
      efd.distance = len_v3(efd.vec_to_point);

      copy_v3_v3(data->vec_to_point, efd.vec_to_point);
      data->strength = effector_falloff(eff, &efd, &point, weights);
    }
  }
}

int do_guides(Depsgraph *depsgraph,
              ParticleSettings *part,
              ListBase *effectors,
              ParticleKey *state,
              int index,
              float time)
{
  CurveMapping *clumpcurve = (part->child_flag & PART_CHILD_USE_CLUMP_CURVE) ? part->clumpcurve :
                                                                               NULL;
  CurveMapping *roughcurve = (part->child_flag & PART_CHILD_USE_ROUGH_CURVE) ? part->roughcurve :
                                                                               NULL;
  EffectorCache *eff;
  PartDeflect *pd;
  Curve *cu;
  GuideEffectorData *data;

  float effect[3] = {0.0f, 0.0f, 0.0f}, veffect[3] = {0.0f, 0.0f, 0.0f};
  float guidevec[4], guidedir[3], rot2[4], temp[3];
  float guidetime, radius, weight, angle, totstrength = 0.0f;
  float vec_to_point[3];

  if (effectors) {
    for (eff = effectors->first; eff; eff = eff->next) {
      pd = eff->pd;

      if (pd->forcefield != PFIELD_GUIDE) {
        continue;
      }

      data = eff->guide_data + index;

      if (data->strength <= 0.0f) {
        continue;
      }

      guidetime = time / (1.0f - pd->free_end);

      if (guidetime > 1.0f) {
        continue;
      }

      cu = (Curve *)eff->ob->data;

      if (pd->flag & PFIELD_GUIDE_PATH_ADD) {
        if (where_on_path(
                eff->ob, data->strength * guidetime, guidevec, guidedir, NULL, &radius, &weight) ==
            0) {
          return 0;
        }
      }
      else {
        if (where_on_path(eff->ob, guidetime, guidevec, guidedir, NULL, &radius, &weight) == 0) {
          return 0;
        }
      }

      mul_m4_v3(eff->ob->obmat, guidevec);
      mul_mat3_m4_v3(eff->ob->obmat, guidedir);

      normalize_v3(guidedir);

      copy_v3_v3(vec_to_point, data->vec_to_point);

      if (guidetime != 0.0f) {
        /* curve direction */
        cross_v3_v3v3(temp, eff->guide_dir, guidedir);
        angle = dot_v3v3(eff->guide_dir, guidedir) / (len_v3(eff->guide_dir));
        angle = saacos(angle);
        axis_angle_to_quat(rot2, temp, angle);
        mul_qt_v3(rot2, vec_to_point);

        /* curve tilt */
        axis_angle_to_quat(rot2, guidedir, guidevec[3] - eff->guide_loc[3]);
        mul_qt_v3(rot2, vec_to_point);
      }

      /* curve taper */
      if (cu->taperobj) {
        mul_v3_fl(vec_to_point,
                  BKE_displist_calc_taper(depsgraph,
                                          eff->scene,
                                          cu->taperobj,
                                          (int)(data->strength * guidetime * 100.0f),
                                          100));
      }
      else { /* curve size*/
        if (cu->flag & CU_PATH_RADIUS) {
          mul_v3_fl(vec_to_point, radius);
        }
      }

      if (clumpcurve) {
        curvemapping_changed_all(clumpcurve);
      }
      if (roughcurve) {
        curvemapping_changed_all(roughcurve);
      }

      {
        ParticleKey key;
        float par_co[3] = {0.0f, 0.0f, 0.0f};
        float par_vel[3] = {0.0f, 0.0f, 0.0f};
        float par_rot[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        float orco_offset[3] = {0.0f, 0.0f, 0.0f};

        copy_v3_v3(key.co, vec_to_point);
        do_kink(&key,
                par_co,
                par_vel,
                par_rot,
                guidetime,
                pd->kink_freq,
                pd->kink_shape,
                pd->kink_amp,
                0.f,
                pd->kink,
                pd->kink_axis,
                0,
                0);
        do_clump(&key,
                 par_co,
                 guidetime,
                 orco_offset,
                 pd->clump_fac,
                 pd->clump_pow,
                 1.0f,
                 part->child_flag & PART_CHILD_USE_CLUMP_NOISE,
                 part->clump_noise_size,
                 clumpcurve);
        copy_v3_v3(vec_to_point, key.co);
      }

      add_v3_v3(vec_to_point, guidevec);

      // sub_v3_v3v3(pa_loc, pa_loc, pa_zero);
      madd_v3_v3fl(effect, vec_to_point, data->strength);
      madd_v3_v3fl(veffect, guidedir, data->strength);
      totstrength += data->strength;

      if (pd->flag & PFIELD_GUIDE_PATH_WEIGHT) {
        totstrength *= weight;
      }
    }
  }

  if (totstrength != 0.0f) {
    if (totstrength > 1.0f) {
      mul_v3_fl(effect, 1.0f / totstrength);
    }
    CLAMP(totstrength, 0.0f, 1.0f);
    // add_v3_v3(effect, pa_zero);
    interp_v3_v3v3(state->co, state->co, effect, totstrength);

    normalize_v3(veffect);
    mul_v3_fl(veffect, len_v3(state->vel));
    copy_v3_v3(state->vel, veffect);
    return 1;
  }
  return 0;
}

static void do_path_effectors(ParticleSimulationData *sim,
                              int i,
                              ParticleCacheKey *ca,
                              int k,
                              int steps,
                              float *UNUSED(rootco),
                              float effector,
                              float UNUSED(dfra),
                              float UNUSED(cfra),
                              float *length,
                              float *vec)
{
  float force[3] = {0.0f, 0.0f, 0.0f};
  ParticleKey eff_key;
  EffectedPoint epoint;

  /* Don't apply effectors for dynamic hair, otherwise the effectors don't get applied twice. */
  if (sim->psys->flag & PSYS_HAIR_DYNAMICS) {
    return;
  }

  copy_v3_v3(eff_key.co, (ca - 1)->co);
  copy_v3_v3(eff_key.vel, (ca - 1)->vel);
  copy_qt_qt(eff_key.rot, (ca - 1)->rot);

  pd_point_from_particle(sim, sim->psys->particles + i, &eff_key, &epoint);
  BKE_effectors_apply(sim->psys->effectors,
                      sim->colliders,
                      sim->psys->part->effector_weights,
                      &epoint,
                      force,
                      NULL);

  mul_v3_fl(force,
            effector * powf((float)k / (float)steps, 100.0f * sim->psys->part->eff_hair) /
                (float)steps);

  add_v3_v3(force, vec);

  normalize_v3(force);

  if (k < steps) {
    sub_v3_v3v3(vec, (ca + 1)->co, ca->co);
  }

  madd_v3_v3v3fl(ca->co, (ca - 1)->co, force, *length);

  if (k < steps) {
    *length = len_v3(vec);
  }
}
static void offset_child(ChildParticle *cpa,
                         ParticleKey *par,
                         float *par_rot,
                         ParticleKey *child,
                         float flat,
                         float radius)
{
  copy_v3_v3(child->co, cpa->fuv);
  mul_v3_fl(child->co, radius);

  child->co[0] *= flat;

  copy_v3_v3(child->vel, par->vel);

  if (par_rot) {
    mul_qt_v3(par_rot, child->co);
    copy_qt_qt(child->rot, par_rot);
  }
  else {
    unit_qt(child->rot);
  }

  add_v3_v3(child->co, par->co);
}
float *psys_cache_vgroup(Mesh *mesh, ParticleSystem *psys, int vgroup)
{
  float *vg = 0;

  if (vgroup < 0) {
    /* hair dynamics pinning vgroup */
  }
  else if (psys->vgroup[vgroup]) {
    MDeformVert *dvert = mesh->dvert;
    if (dvert) {
      int totvert = mesh->totvert, i;
      vg = MEM_callocN(sizeof(float) * totvert, "vg_cache");
      if (psys->vg_neg & (1 << vgroup)) {
        for (i = 0; i < totvert; i++) {
          vg[i] = 1.0f - defvert_find_weight(&dvert[i], psys->vgroup[vgroup] - 1);
        }
      }
      else {
        for (i = 0; i < totvert; i++) {
          vg[i] = defvert_find_weight(&dvert[i], psys->vgroup[vgroup] - 1);
        }
      }
    }
  }
  return vg;
}
void psys_find_parents(ParticleSimulationData *sim, const bool use_render_params)
{
  ParticleSystem *psys = sim->psys;
  ParticleSettings *part = sim->psys->part;
  KDTree_3d *tree;
  ChildParticle *cpa;
  ParticleTexture ptex;
  int p, totparent, totchild = sim->psys->totchild;
  float co[3], orco[3];
  int from = PART_FROM_FACE;
  totparent = (int)(totchild * part->parents * 0.3f);

  if (use_render_params && part->child_nbr && part->ren_child_nbr) {
    totparent *= (float)part->child_nbr / (float)part->ren_child_nbr;
  }

  /* hard limit, workaround for it being ignored above */
  if (sim->psys->totpart < totparent) {
    totparent = sim->psys->totpart;
  }

  tree = BLI_kdtree_3d_new(totparent);

  Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(sim->psmd);

  for (p = 0, cpa = sim->psys->child; p < totparent; p++, cpa++) {
    psys_particle_on_emitter(
        sim->psmd, from, cpa->num, DMCACHE_ISCHILD, cpa->fuv, cpa->foffset, co, 0, 0, 0, orco);

    /* Check if particle doesn't exist because of texture influence.
     * Insert only existing particles into kdtree. */
    get_cpa_texture(mesh_final,
                    psys,
                    part,
                    psys->particles + cpa->pa[0],
                    p,
                    cpa->num,
                    cpa->fuv,
                    orco,
                    &ptex,
                    PAMAP_DENS | PAMAP_CHILD,
                    psys->cfra);

    if (ptex.exist >= psys_frand(psys, p + 24)) {
      BLI_kdtree_3d_insert(tree, p, orco);
    }
  }

  BLI_kdtree_3d_balance(tree);

  for (; p < totchild; p++, cpa++) {
    psys_particle_on_emitter(
        sim->psmd, from, cpa->num, DMCACHE_ISCHILD, cpa->fuv, cpa->foffset, co, 0, 0, 0, orco);
    cpa->parent = BLI_kdtree_3d_find_nearest(tree, orco, NULL);
  }

  BLI_kdtree_3d_free(tree);
}

static bool psys_thread_context_init_path(ParticleThreadContext *ctx,
                                          ParticleSimulationData *sim,
                                          Scene *scene,
                                          float cfra,
                                          const bool editupdate,
                                          const bool use_render_params)
{
  ParticleSystem *psys = sim->psys;
  ParticleSettings *part = psys->part;
  int totparent = 0, between = 0;
  int segments = 1 << part->draw_step;
  int totchild = psys->totchild;

  psys_thread_context_init(ctx, sim);

  /*---start figuring out what is actually wanted---*/
  if (psys_in_edit_mode(sim->depsgraph, psys)) {
    ParticleEditSettings *pset = &scene->toolsettings->particle;

    if ((use_render_params == 0) &&
        (psys_orig_edit_get(psys) == NULL || pset->flag & PE_DRAW_PART) == 0) {
      totchild = 0;
    }

    segments = 1 << pset->draw_step;
  }

  if (totchild && part->childtype == PART_CHILD_FACES) {
    totparent = (int)(totchild * part->parents * 0.3f);

    if (use_render_params && part->child_nbr && part->ren_child_nbr) {
      totparent *= (float)part->child_nbr / (float)part->ren_child_nbr;
    }

    /* part->parents could still be 0 so we can't test with totparent */
    between = 1;
  }

  if (use_render_params) {
    segments = 1 << part->ren_step;
  }
  else {
    totchild = (int)((float)totchild * (float)part->disp / 100.0f);
    totparent = MIN2(totparent, totchild);
  }

  if (totchild == 0) {
    return false;
  }

  /* fill context values */
  ctx->between = between;
  ctx->segments = segments;
  if (ELEM(part->kink, PART_KINK_SPIRAL)) {
    ctx->extra_segments = max_ii(part->kink_extra_steps, 1);
  }
  else {
    ctx->extra_segments = 0;
  }
  ctx->totchild = totchild;
  ctx->totparent = totparent;
  ctx->parent_pass = 0;
  ctx->cfra = cfra;
  ctx->editupdate = editupdate;

  psys->lattice_deform_data = psys_create_lattice_deform_data(&ctx->sim);

  /* cache all relevant vertex groups if they exist */
  ctx->vg_length = psys_cache_vgroup(ctx->mesh, psys, PSYS_VG_LENGTH);
  ctx->vg_clump = psys_cache_vgroup(ctx->mesh, psys, PSYS_VG_CLUMP);
  ctx->vg_kink = psys_cache_vgroup(ctx->mesh, psys, PSYS_VG_KINK);
  ctx->vg_rough1 = psys_cache_vgroup(ctx->mesh, psys, PSYS_VG_ROUGH1);
  ctx->vg_rough2 = psys_cache_vgroup(ctx->mesh, psys, PSYS_VG_ROUGH2);
  ctx->vg_roughe = psys_cache_vgroup(ctx->mesh, psys, PSYS_VG_ROUGHE);
  ctx->vg_twist = psys_cache_vgroup(ctx->mesh, psys, PSYS_VG_TWIST);
  if (psys->part->flag & PART_CHILD_EFFECT) {
    ctx->vg_effector = psys_cache_vgroup(ctx->mesh, psys, PSYS_VG_EFFECTOR);
  }

  /* prepare curvemapping tables */
  if ((part->child_flag & PART_CHILD_USE_CLUMP_CURVE) && part->clumpcurve) {
    ctx->clumpcurve = curvemapping_copy(part->clumpcurve);
    curvemapping_changed_all(ctx->clumpcurve);
  }
  else {
    ctx->clumpcurve = NULL;
  }
  if ((part->child_flag & PART_CHILD_USE_ROUGH_CURVE) && part->roughcurve) {
    ctx->roughcurve = curvemapping_copy(part->roughcurve);
    curvemapping_changed_all(ctx->roughcurve);
  }
  else {
    ctx->roughcurve = NULL;
  }
  if ((part->child_flag & PART_CHILD_USE_TWIST_CURVE) && part->twistcurve) {
    ctx->twistcurve = curvemapping_copy(part->twistcurve);
    curvemapping_changed_all(ctx->twistcurve);
  }
  else {
    ctx->twistcurve = NULL;
  }

  return true;
}

static void psys_task_init_path(ParticleTask *task, ParticleSimulationData *sim)
{
  /* init random number generator */
  int seed = 31415926 + sim->psys->seed;

  task->rng_path = BLI_rng_new(seed);
}

/* note: this function must be thread safe, except for branching! */
static void psys_thread_create_path(ParticleTask *task,
                                    struct ChildParticle *cpa,
                                    ParticleCacheKey *child_keys,
                                    int i)
{
  ParticleThreadContext *ctx = task->ctx;
  Object *ob = ctx->sim.ob;
  ParticleSystem *psys = ctx->sim.psys;
  ParticleSettings *part = psys->part;
  ParticleCacheKey **cache = psys->childcache;
  PTCacheEdit *edit = psys_orig_edit_get(psys);
  ParticleCacheKey **pcache = psys_in_edit_mode(ctx->sim.depsgraph, psys) && edit ?
                                  edit->pathcache :
                                  psys->pathcache;
  ParticleCacheKey *child, *key[4];
  ParticleTexture ptex;
  float *cpa_fuv = 0, *par_rot = 0, rot[4];
  float orco[3], hairmat[4][4], dvec[3], off1[4][3], off2[4][3];
  float eff_length, eff_vec[3], weight[4];
  int k, cpa_num;
  short cpa_from;

  if (!pcache) {
    return;
  }

  Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(ctx->sim.psmd);

  if (ctx->between) {
    ParticleData *pa = psys->particles + cpa->pa[0];
    int w, needupdate;
    float foffset, wsum = 0.f;
    float co[3];
    float p_min = part->parting_min;
    float p_max = part->parting_max;
    /* Virtual parents don't work nicely with parting. */
    float p_fac = part->parents > 0.f ? 0.f : part->parting_fac;

    if (ctx->editupdate) {
      needupdate = 0;
      w = 0;
      while (w < 4 && cpa->pa[w] >= 0) {
        if (edit->points[cpa->pa[w]].flag & PEP_EDIT_RECALC) {
          needupdate = 1;
          break;
        }
        w++;
      }

      if (!needupdate) {
        return;
      }
      else {
        memset(child_keys, 0, sizeof(*child_keys) * (ctx->segments + 1));
      }
    }

    /* get parent paths */
    for (w = 0; w < 4; w++) {
      if (cpa->pa[w] >= 0) {
        key[w] = pcache[cpa->pa[w]];
        weight[w] = cpa->w[w];
      }
      else {
        key[w] = pcache[0];
        weight[w] = 0.f;
      }
    }

    /* modify weights to create parting */
    if (p_fac > 0.f) {
      const ParticleCacheKey *key_0_last = pcache_key_segment_endpoint_safe(key[0]);
      for (w = 0; w < 4; w++) {
        if (w && (weight[w] > 0.f)) {
          const ParticleCacheKey *key_w_last = pcache_key_segment_endpoint_safe(key[w]);
          float d;
          if (part->flag & PART_CHILD_LONG_HAIR) {
            /* For long hair use tip distance/root distance as parting
             * factor instead of root to tip angle. */
            float d1 = len_v3v3(key[0]->co, key[w]->co);
            float d2 = len_v3v3(key_0_last->co, key_w_last->co);

            d = d1 > 0.f ? d2 / d1 - 1.f : 10000.f;
          }
          else {
            float v1[3], v2[3];
            sub_v3_v3v3(v1, key_0_last->co, key[0]->co);
            sub_v3_v3v3(v2, key_w_last->co, key[w]->co);
            normalize_v3(v1);
            normalize_v3(v2);

            d = RAD2DEGF(saacos(dot_v3v3(v1, v2)));
          }

          if (p_max > p_min) {
            d = (d - p_min) / (p_max - p_min);
          }
          else {
            d = (d - p_min) <= 0.f ? 0.f : 1.f;
          }

          CLAMP(d, 0.f, 1.f);

          if (d > 0.f) {
            weight[w] *= (1.f - d);
          }
        }
        wsum += weight[w];
      }
      for (w = 0; w < 4; w++) {
        weight[w] /= wsum;
      }

      interp_v4_v4v4(weight, cpa->w, weight, p_fac);
    }

    /* get the original coordinates (orco) for texture usage */
    cpa_num = cpa->num;

    foffset = cpa->foffset;
    cpa_fuv = cpa->fuv;
    cpa_from = PART_FROM_FACE;

    psys_particle_on_emitter(
        ctx->sim.psmd, cpa_from, cpa_num, DMCACHE_ISCHILD, cpa->fuv, foffset, co, 0, 0, 0, orco);

    mul_m4_v3(ob->obmat, co);

    for (w = 0; w < 4; w++) {
      sub_v3_v3v3(off1[w], co, key[w]->co);
    }

    psys_mat_hair_to_global(ob, mesh_final, psys->part->from, pa, hairmat);
  }
  else {
    ParticleData *pa = psys->particles + cpa->parent;
    float co[3];
    if (ctx->editupdate) {
      if (!(edit->points[cpa->parent].flag & PEP_EDIT_RECALC)) {
        return;
      }

      memset(child_keys, 0, sizeof(*child_keys) * (ctx->segments + 1));
    }

    /* get the parent path */
    key[0] = pcache[cpa->parent];

    /* get the original coordinates (orco) for texture usage */
    cpa_from = part->from;

    /*
     * NOTE: Should in theory be the same as:
     * cpa_num = psys_particle_dm_face_lookup(
     *        ctx->sim.psmd->dm_final,
     *        ctx->sim.psmd->dm_deformed,
     *        pa->num, pa->fuv,
     *        NULL);
     */
    cpa_num = (ELEM(pa->num_dmcache, DMCACHE_ISCHILD, DMCACHE_NOTFOUND)) ? pa->num :
                                                                           pa->num_dmcache;

    /* XXX hack to avoid messed up particle num and subsequent crash (#40733) */
    if (cpa_num > mesh_final->totface) {
      cpa_num = 0;
    }
    cpa_fuv = pa->fuv;

    psys_particle_on_emitter(ctx->sim.psmd,
                             cpa_from,
                             cpa_num,
                             DMCACHE_ISCHILD,
                             cpa_fuv,
                             pa->foffset,
                             co,
                             0,
                             0,
                             0,
                             orco);

    psys_mat_hair_to_global(ob, mesh_final, psys->part->from, pa, hairmat);
  }

  child_keys->segments = ctx->segments;

  /* get different child parameters from textures & vgroups */
  get_child_modifier_parameters(part, ctx, cpa, cpa_from, cpa_num, cpa_fuv, orco, &ptex);

  if (ptex.exist < psys_frand(psys, i + 24)) {
    child_keys->segments = -1;
    return;
  }

  /* create the child path */
  for (k = 0, child = child_keys; k <= ctx->segments; k++, child++) {
    if (ctx->between) {
      int w = 0;

      zero_v3(child->co);
      zero_v3(child->vel);
      unit_qt(child->rot);

      for (w = 0; w < 4; w++) {
        copy_v3_v3(off2[w], off1[w]);

        if (part->flag & PART_CHILD_LONG_HAIR) {
          /* Use parent rotation (in addition to emission location) to determine child offset. */
          if (k) {
            mul_qt_v3((key[w] + k)->rot, off2[w]);
          }

          /* Fade the effect of rotation for even lengths in the end */
          project_v3_v3v3(dvec, off2[w], (key[w] + k)->vel);
          madd_v3_v3fl(off2[w], dvec, -(float)k / (float)ctx->segments);
        }

        add_v3_v3(off2[w], (key[w] + k)->co);
      }

      /* child position is the weighted sum of parent positions */
      interp_v3_v3v3v3v3(child->co, off2[0], off2[1], off2[2], off2[3], weight);
      interp_v3_v3v3v3v3(child->vel,
                         (key[0] + k)->vel,
                         (key[1] + k)->vel,
                         (key[2] + k)->vel,
                         (key[3] + k)->vel,
                         weight);

      copy_qt_qt(child->rot, (key[0] + k)->rot);
    }
    else {
      if (k) {
        mul_qt_qtqt(rot, (key[0] + k)->rot, key[0]->rot);
        par_rot = rot;
      }
      else {
        par_rot = key[0]->rot;
      }
      /* offset the child from the parent position */
      offset_child(cpa,
                   (ParticleKey *)(key[0] + k),
                   par_rot,
                   (ParticleKey *)child,
                   part->childflat,
                   part->childrad);
    }

    child->time = (float)k / (float)ctx->segments;
  }

  /* apply effectors */
  if (part->flag & PART_CHILD_EFFECT) {
    for (k = 0, child = child_keys; k <= ctx->segments; k++, child++) {
      if (k) {
        do_path_effectors(&ctx->sim,
                          cpa->pa[0],
                          child,
                          k,
                          ctx->segments,
                          child_keys->co,
                          ptex.effector,
                          0.0f,
                          ctx->cfra,
                          &eff_length,
                          eff_vec);
      }
      else {
        sub_v3_v3v3(eff_vec, (child + 1)->co, child->co);
        eff_length = len_v3(eff_vec);
      }
    }
  }

  {
    ParticleData *pa = NULL;
    ParticleCacheKey *par = NULL;
    float par_co[3];
    float par_orco[3];

    if (ctx->totparent) {
      if (i >= ctx->totparent) {
        pa = &psys->particles[cpa->parent];
        /* this is now threadsafe, virtual parents are calculated before rest of children */
        BLI_assert(cpa->parent < psys->totchildcache);
        par = cache[cpa->parent];
      }
    }
    else if (cpa->parent >= 0) {
      pa = &psys->particles[cpa->parent];
      par = pcache[cpa->parent];

      /* If particle is unexisting, try to pick a viable parent from particles
       * used for interpolation. */
      for (k = 0; k < 4 && pa && (pa->flag & PARS_UNEXIST); k++) {
        if (cpa->pa[k] >= 0) {
          pa = &psys->particles[cpa->pa[k]];
          par = pcache[cpa->pa[k]];
        }
      }

      if (pa->flag & PARS_UNEXIST) {
        pa = NULL;
      }
    }

    if (pa) {
      ListBase modifiers;
      BLI_listbase_clear(&modifiers);

      psys_particle_on_emitter(ctx->sim.psmd,
                               part->from,
                               pa->num,
                               pa->num_dmcache,
                               pa->fuv,
                               pa->foffset,
                               par_co,
                               NULL,
                               NULL,
                               NULL,
                               par_orco);

      psys_apply_child_modifiers(
          ctx, &modifiers, cpa, &ptex, orco, hairmat, child_keys, par, par_orco);
    }
    else {
      zero_v3(par_orco);
    }
  }

  /* Hide virtual parents */
  if (i < ctx->totparent) {
    child_keys->segments = -1;
  }
}

static void exec_child_path_cache(TaskPool *__restrict UNUSED(pool),
                                  void *taskdata,
                                  int UNUSED(threadid))
{
  ParticleTask *task = taskdata;
  ParticleThreadContext *ctx = task->ctx;
  ParticleSystem *psys = ctx->sim.psys;
  ParticleCacheKey **cache = psys->childcache;
  ChildParticle *cpa;
  int i;

  cpa = psys->child + task->begin;
  for (i = task->begin; i < task->end; ++i, ++cpa) {
    BLI_assert(i < psys->totchildcache);
    psys_thread_create_path(task, cpa, cache[i], i);
  }
}

void psys_cache_child_paths(ParticleSimulationData *sim,
                            float cfra,
                            const bool editupdate,
                            const bool use_render_params)
{
  TaskScheduler *task_scheduler;
  TaskPool *task_pool;
  ParticleThreadContext ctx;
  ParticleTask *tasks_parent, *tasks_child;
  int numtasks_parent, numtasks_child;
  int i, totchild, totparent;

  if (sim->psys->flag & PSYS_GLOBAL_HAIR) {
    return;
  }

  /* create a task pool for child path tasks */
  if (!psys_thread_context_init_path(&ctx, sim, sim->scene, cfra, editupdate, use_render_params)) {
    return;
  }

  task_scheduler = BLI_task_scheduler_get();
  task_pool = BLI_task_pool_create(task_scheduler, &ctx);
  totchild = ctx.totchild;
  totparent = ctx.totparent;

  if (editupdate && sim->psys->childcache && totchild == sim->psys->totchildcache) {
    /* just overwrite the existing cache */
  }
  else {
    /* clear out old and create new empty path cache */
    free_child_path_cache(sim->psys);

    sim->psys->childcache = psys_alloc_path_cache_buffers(
        &sim->psys->childcachebufs, totchild, ctx.segments + ctx.extra_segments + 1);
    sim->psys->totchildcache = totchild;
  }

  /* cache parent paths */
  ctx.parent_pass = 1;
  psys_tasks_create(&ctx, 0, totparent, &tasks_parent, &numtasks_parent);
  for (i = 0; i < numtasks_parent; ++i) {
    ParticleTask *task = &tasks_parent[i];

    psys_task_init_path(task, sim);
    BLI_task_pool_push(task_pool, exec_child_path_cache, task, false, TASK_PRIORITY_LOW);
  }
  BLI_task_pool_work_and_wait(task_pool);

  /* cache child paths */
  ctx.parent_pass = 0;
  psys_tasks_create(&ctx, totparent, totchild, &tasks_child, &numtasks_child);
  for (i = 0; i < numtasks_child; ++i) {
    ParticleTask *task = &tasks_child[i];

    psys_task_init_path(task, sim);
    BLI_task_pool_push(task_pool, exec_child_path_cache, task, false, TASK_PRIORITY_LOW);
  }
  BLI_task_pool_work_and_wait(task_pool);

  BLI_task_pool_free(task_pool);

  psys_tasks_free(tasks_parent, numtasks_parent);
  psys_tasks_free(tasks_child, numtasks_child);

  psys_thread_context_free(&ctx);
}

/* figure out incremental rotations along path starting from unit quat */
static void cache_key_incremental_rotation(ParticleCacheKey *key0,
                                           ParticleCacheKey *key1,
                                           ParticleCacheKey *key2,
                                           float *prev_tangent,
                                           int i)
{
  float cosangle, angle, tangent[3], normal[3], q[4];

  switch (i) {
    case 0:
      /* start from second key */
      break;
    case 1:
      /* calculate initial tangent for incremental rotations */
      sub_v3_v3v3(prev_tangent, key0->co, key1->co);
      normalize_v3(prev_tangent);
      unit_qt(key1->rot);
      break;
    default:
      sub_v3_v3v3(tangent, key0->co, key1->co);
      normalize_v3(tangent);

      cosangle = dot_v3v3(tangent, prev_tangent);

      /* note we do the comparison on cosangle instead of
       * angle, since floating point accuracy makes it give
       * different results across platforms */
      if (cosangle > 0.999999f) {
        copy_v4_v4(key1->rot, key2->rot);
      }
      else {
        angle = saacos(cosangle);
        cross_v3_v3v3(normal, prev_tangent, tangent);
        axis_angle_to_quat(q, normal, angle);
        mul_qt_qtqt(key1->rot, q, key2->rot);
      }

      copy_v3_v3(prev_tangent, tangent);
  }
}

/**
 * Calculates paths ready for drawing/rendering
 * - Useful for making use of opengl vertex arrays for super fast strand drawing.
 * - Makes child strands possible and creates them too into the cache.
 * - Cached path data is also used to determine cut position for the editmode tool. */
void psys_cache_paths(ParticleSimulationData *sim, float cfra, const bool use_render_params)
{
  PARTICLE_PSMD;
  ParticleEditSettings *pset = &sim->scene->toolsettings->particle;
  ParticleSystem *psys = sim->psys;
  ParticleSettings *part = psys->part;
  ParticleCacheKey *ca, **cache;

  Mesh *hair_mesh = (psys->part->type == PART_HAIR && psys->flag & PSYS_HAIR_DYNAMICS) ?
                        psys->hair_out_mesh :
                        NULL;

  ParticleKey result;

  Material *ma;
  ParticleInterpolationData pind;
  ParticleTexture ptex;

  PARTICLE_P;

  float birthtime = 0.0, dietime = 0.0;
  float t, time = 0.0, dfra = 1.0 /* , frs_sec = sim->scene->r.frs_sec*/ /*UNUSED*/;
  float col[4] = {0.5f, 0.5f, 0.5f, 1.0f};
  float prev_tangent[3] = {0.0f, 0.0f, 0.0f}, hairmat[4][4];
  float rotmat[3][3];
  int k;
  int segments = (int)pow(2.0, (double)((use_render_params) ? part->ren_step : part->draw_step));
  int totpart = psys->totpart;
  float length, vec[3];
  float *vg_effector = NULL;
  float *vg_length = NULL, pa_length = 1.0f;
  int keyed, baked;

  /* we don't have anything valid to create paths from so let's quit here */
  if ((psys->flag & PSYS_HAIR_DONE || psys->flag & PSYS_KEYED || psys->pointcache) == 0) {
    return;
  }

  if (psys_in_edit_mode(sim->depsgraph, psys)) {
    if ((psys->edit == NULL || pset->flag & PE_DRAW_PART) == 0) {
      return;
    }
  }

  keyed = psys->flag & PSYS_KEYED;
  baked = psys->pointcache->mem_cache.first && psys->part->type != PART_HAIR;

  /* clear out old and create new empty path cache */
  psys_free_path_cache(psys, psys->edit);
  cache = psys->pathcache = psys_alloc_path_cache_buffers(
      &psys->pathcachebufs, totpart, segments + 1);

  psys->lattice_deform_data = psys_create_lattice_deform_data(sim);
  ma = give_current_material(sim->ob, psys->part->omat);
  if (ma && (psys->part->draw_col == PART_DRAW_COL_MAT)) {
    copy_v3_v3(col, &ma->r);
  }

  Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(psmd);

  if ((psys->flag & PSYS_GLOBAL_HAIR) == 0) {
    if ((psys->part->flag & PART_CHILD_EFFECT) == 0) {
      vg_effector = psys_cache_vgroup(mesh_final, psys, PSYS_VG_EFFECTOR);
    }

    if (!psys->totchild) {
      vg_length = psys_cache_vgroup(mesh_final, psys, PSYS_VG_LENGTH);
    }
  }

  /* ensure we have tessfaces to be used for mapping */
  if (part->from != PART_FROM_VERT) {
    BKE_mesh_tessface_ensure(mesh_final);
  }

  /*---first main loop: create all actual particles' paths---*/
  LOOP_PARTICLES
  {
    if (!psys->totchild) {
      psys_get_texture(sim, pa, &ptex, PAMAP_LENGTH, 0.f);
      pa_length = ptex.length * (1.0f - part->randlength * psys_frand(psys, psys->seed + p));
      if (vg_length) {
        pa_length *= psys_particle_value_from_verts(mesh_final, part->from, pa, vg_length);
      }
    }

    pind.keyed = keyed;
    pind.cache = baked ? psys->pointcache : NULL;
    pind.epoint = NULL;
    pind.bspline = (psys->part->flag & PART_HAIR_BSPLINE);
    pind.mesh = hair_mesh;

    memset(cache[p], 0, sizeof(*cache[p]) * (segments + 1));

    cache[p]->segments = segments;

    /*--get the first data points--*/
    init_particle_interpolation(sim->ob, sim->psys, pa, &pind);

    /* hairmat is needed for for non-hair particle too so we get proper rotations */
    psys_mat_hair_to_global(sim->ob, mesh_final, psys->part->from, pa, hairmat);
    copy_v3_v3(rotmat[0], hairmat[2]);
    copy_v3_v3(rotmat[1], hairmat[1]);
    copy_v3_v3(rotmat[2], hairmat[0]);

    if (part->draw & PART_ABS_PATH_TIME) {
      birthtime = MAX2(pind.birthtime, part->path_start);
      dietime = MIN2(pind.dietime, part->path_end);
    }
    else {
      float tb = pind.birthtime;
      birthtime = tb + part->path_start * (pind.dietime - tb);
      dietime = tb + part->path_end * (pind.dietime - tb);
    }

    if (birthtime >= dietime) {
      cache[p]->segments = -1;
      continue;
    }

    dietime = birthtime + pa_length * (dietime - birthtime);

    /*--interpolate actual path from data points--*/
    for (k = 0, ca = cache[p]; k <= segments; k++, ca++) {
      time = (float)k / (float)segments;
      t = birthtime + time * (dietime - birthtime);
      result.time = -t;
      do_particle_interpolation(psys, p, pa, t, &pind, &result);
      copy_v3_v3(ca->co, result.co);

      /* dynamic hair is in object space */
      /* keyed and baked are already in global space */
      if (hair_mesh) {
        mul_m4_v3(sim->ob->obmat, ca->co);
      }
      else if (!keyed && !baked && !(psys->flag & PSYS_GLOBAL_HAIR)) {
        mul_m4_v3(hairmat, ca->co);
      }

      copy_v3_v3(ca->col, col);
    }

    if (part->type == PART_HAIR) {
      HairKey *hkey;

      for (k = 0, hkey = pa->hair; k < pa->totkey; ++k, ++hkey) {
        mul_v3_m4v3(hkey->world_co, hairmat, hkey->co);
      }
    }

    /*--modify paths and calculate rotation & velocity--*/

    if (!(psys->flag & PSYS_GLOBAL_HAIR)) {
      /* apply effectors */
      if ((psys->part->flag & PART_CHILD_EFFECT) == 0) {
        float effector = 1.0f;
        if (vg_effector) {
          effector *= psys_particle_value_from_verts(
              mesh_final, psys->part->from, pa, vg_effector);
        }

        sub_v3_v3v3(vec, (cache[p] + 1)->co, cache[p]->co);
        length = len_v3(vec);

        for (k = 1, ca = cache[p] + 1; k <= segments; k++, ca++) {
          do_path_effectors(
              sim, p, ca, k, segments, cache[p]->co, effector, dfra, cfra, &length, vec);
        }
      }

      /* apply guide curves to path data */
      if (sim->psys->effectors && (psys->part->flag & PART_CHILD_EFFECT) == 0) {
        for (k = 0, ca = cache[p]; k <= segments; k++, ca++) {
          /* ca is safe to cast, since only co and vel are used */
          do_guides(sim->depsgraph,
                    sim->psys->part,
                    sim->psys->effectors,
                    (ParticleKey *)ca,
                    p,
                    (float)k / (float)segments);
        }
      }

      /* lattices have to be calculated separately to avoid mixups between effector calculations */
      if (psys->lattice_deform_data) {
        for (k = 0, ca = cache[p]; k <= segments; k++, ca++) {
          calc_latt_deform(psys->lattice_deform_data, ca->co, psys->lattice_strength);
        }
      }
    }

    /* finally do rotation & velocity */
    for (k = 1, ca = cache[p] + 1; k <= segments; k++, ca++) {
      cache_key_incremental_rotation(ca, ca - 1, ca - 2, prev_tangent, k);

      if (k == segments) {
        copy_qt_qt(ca->rot, (ca - 1)->rot);
      }

      /* set velocity */
      sub_v3_v3v3(ca->vel, ca->co, (ca - 1)->co);

      if (k == 1) {
        copy_v3_v3((ca - 1)->vel, ca->vel);
      }

      ca->time = (float)k / (float)segments;
    }
    /* First rotation is based on emitting face orientation.
     * This is way better than having flipping rotations resulting
     * from using a global axis as a rotation pole (vec_to_quat()).
     * It's not an ideal solution though since it disregards the
     * initial tangent, but taking that in to account will allow
     * the possibility of flipping again. -jahka
     */
    mat3_to_quat_is_ok(cache[p]->rot, rotmat);
  }

  psys->totcached = totpart;

  if (psys->lattice_deform_data) {
    end_latt_deform(psys->lattice_deform_data);
    psys->lattice_deform_data = NULL;
  }

  if (vg_effector) {
    MEM_freeN(vg_effector);
  }

  if (vg_length) {
    MEM_freeN(vg_length);
  }
}

typedef struct CacheEditrPathsIterData {
  Object *object;
  PTCacheEdit *edit;
  ParticleSystemModifierData *psmd;
  ParticleData *pa;
  int segments;
  bool use_weight;
  float sel_col[3];
  float nosel_col[3];
} CacheEditrPathsIterData;

static void psys_cache_edit_paths_iter(void *__restrict iter_data_v,
                                       const int iter,
                                       const ParallelRangeTLS *__restrict UNUSED(tls))
{
  CacheEditrPathsIterData *iter_data = (CacheEditrPathsIterData *)iter_data_v;
  PTCacheEdit *edit = iter_data->edit;
  PTCacheEditPoint *point = &edit->points[iter];
  if (edit->totcached && !(point->flag & PEP_EDIT_RECALC)) {
    return;
  }
  if (point->totkey == 0) {
    return;
  }
  Object *ob = iter_data->object;
  ParticleSystem *psys = edit->psys;
  ParticleCacheKey **cache = edit->pathcache;
  ParticleSystemModifierData *psmd = iter_data->psmd;
  ParticleData *pa = iter_data->pa ? iter_data->pa + iter : NULL;
  PTCacheEditKey *ekey = point->keys;
  const int segments = iter_data->segments;
  const bool use_weight = iter_data->use_weight;

  float birthtime = 0.0f, dietime = 0.0f;
  float hairmat[4][4], rotmat[3][3], prev_tangent[3] = {0.0f, 0.0f, 0.0f};

  ParticleInterpolationData pind;
  pind.keyed = 0;
  pind.cache = NULL;
  pind.epoint = point;
  pind.bspline = psys ? (psys->part->flag & PART_HAIR_BSPLINE) : 0;
  pind.mesh = NULL;

  /* should init_particle_interpolation set this ? */
  if (use_weight) {
    pind.hkey[0] = NULL;
    /* pa != NULL since the weight brush is only available for hair */
    pind.hkey[0] = pa->hair;
    pind.hkey[1] = pa->hair + 1;
  }

  memset(cache[iter], 0, sizeof(*cache[iter]) * (segments + 1));

  cache[iter]->segments = segments;

  /*--get the first data points--*/
  init_particle_interpolation(ob, psys, pa, &pind);

  if (psys) {
    Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(psmd);
    psys_mat_hair_to_global(ob, mesh_final, psys->part->from, pa, hairmat);
    copy_v3_v3(rotmat[0], hairmat[2]);
    copy_v3_v3(rotmat[1], hairmat[1]);
    copy_v3_v3(rotmat[2], hairmat[0]);
  }

  birthtime = pind.birthtime;
  dietime = pind.dietime;

  if (birthtime >= dietime) {
    cache[iter]->segments = -1;
    return;
  }

  /*--interpolate actual path from data points--*/
  ParticleCacheKey *ca;
  int k;
  float t, time = 0.0f, keytime = 0.0f;
  for (k = 0, ca = cache[iter]; k <= segments; k++, ca++) {
    time = (float)k / (float)segments;
    t = birthtime + time * (dietime - birthtime);
    ParticleKey result;
    result.time = -t;
    do_particle_interpolation(psys, iter, pa, t, &pind, &result);
    copy_v3_v3(ca->co, result.co);

    /* non-hair points are already in global space */
    if (psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
      mul_m4_v3(hairmat, ca->co);

      if (k) {
        cache_key_incremental_rotation(ca, ca - 1, ca - 2, prev_tangent, k);

        if (k == segments) {
          copy_qt_qt(ca->rot, (ca - 1)->rot);
        }

        /* set velocity */
        sub_v3_v3v3(ca->vel, ca->co, (ca - 1)->co);

        if (k == 1) {
          copy_v3_v3((ca - 1)->vel, ca->vel);
        }
      }
    }
    else {
      ca->vel[0] = ca->vel[1] = 0.0f;
      ca->vel[2] = 1.0f;
    }

    /* selection coloring in edit mode */
    if (use_weight) {
      if (k == 0) {
        BKE_defvert_weight_to_rgb(ca->col, pind.hkey[1]->weight);
      }
      else {
        /* warning: copied from 'do_particle_interpolation' (without 'mvert' array stepping) */
        float real_t;
        if (result.time < 0.0f) {
          real_t = -result.time;
        }
        else {
          real_t = pind.hkey[0]->time +
                   t * (pind.hkey[0][pa->totkey - 1].time - pind.hkey[0]->time);
        }

        while (pind.hkey[1]->time < real_t) {
          pind.hkey[1]++;
        }
        pind.hkey[0] = pind.hkey[1] - 1;
        /* end copy */

        float w1[3], w2[3];
        keytime = (t - (*pind.ekey[0]->time)) / ((*pind.ekey[1]->time) - (*pind.ekey[0]->time));

        BKE_defvert_weight_to_rgb(w1, pind.hkey[0]->weight);
        BKE_defvert_weight_to_rgb(w2, pind.hkey[1]->weight);

        interp_v3_v3v3(ca->col, w1, w2, keytime);
      }
    }
    else {
      if ((ekey + (pind.ekey[0] - point->keys))->flag & PEK_SELECT) {
        if ((ekey + (pind.ekey[1] - point->keys))->flag & PEK_SELECT) {
          copy_v3_v3(ca->col, iter_data->sel_col);
        }
        else {
          keytime = (t - (*pind.ekey[0]->time)) / ((*pind.ekey[1]->time) - (*pind.ekey[0]->time));
          interp_v3_v3v3(ca->col, iter_data->sel_col, iter_data->nosel_col, keytime);
        }
      }
      else {
        if ((ekey + (pind.ekey[1] - point->keys))->flag & PEK_SELECT) {
          keytime = (t - (*pind.ekey[0]->time)) / ((*pind.ekey[1]->time) - (*pind.ekey[0]->time));
          interp_v3_v3v3(ca->col, iter_data->nosel_col, iter_data->sel_col, keytime);
        }
        else {
          copy_v3_v3(ca->col, iter_data->nosel_col);
        }
      }
    }

    ca->time = t;
  }
  if (psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
    /* First rotation is based on emitting face orientation.
     * This is way better than having flipping rotations resulting
     * from using a global axis as a rotation pole (vec_to_quat()).
     * It's not an ideal solution though since it disregards the
     * initial tangent, but taking that in to account will allow
     * the possibility of flipping again. -jahka
     */
    mat3_to_quat_is_ok(cache[iter]->rot, rotmat);
  }
}

void psys_cache_edit_paths(Depsgraph *depsgraph,
                           Scene *scene,
                           Object *ob,
                           PTCacheEdit *edit,
                           float cfra,
                           const bool use_render_params)
{
  ParticleCacheKey **cache = edit->pathcache;
  ParticleEditSettings *pset = &scene->toolsettings->particle;

  ParticleSystem *psys = edit->psys;

  ParticleData *pa = psys ? psys->particles : NULL;

  int segments = 1 << pset->draw_step;
  int totpart = edit->totpoint, recalc_set = 0;

  segments = MAX2(segments, 4);

  if (!cache || edit->totpoint != edit->totcached) {
    /* Clear out old and create new empty path cache. */
    psys_free_path_cache(edit->psys, edit);
    cache = edit->pathcache = psys_alloc_path_cache_buffers(
        &edit->pathcachebufs, totpart, segments + 1);
    /* Set flag for update (child particles check this too). */
    int i;
    PTCacheEditPoint *point;
    for (i = 0, point = edit->points; i < totpart; i++, point++) {
      point->flag |= PEP_EDIT_RECALC;
    }
    recalc_set = 1;
  }

  const bool use_weight = (pset->brushtype == PE_BRUSH_WEIGHT) && (psys != NULL) &&
                          (psys->particles != NULL);

  CacheEditrPathsIterData iter_data;
  iter_data.object = ob;
  iter_data.edit = edit;
  iter_data.psmd = edit->psmd_eval;
  iter_data.pa = pa;
  iter_data.segments = segments;
  iter_data.use_weight = use_weight;

  if (use_weight) {
    /* use weight painting colors now... */
  }
  else {
    iter_data.sel_col[0] = (float)edit->sel_col[0] / 255.0f;
    iter_data.sel_col[1] = (float)edit->sel_col[1] / 255.0f;
    iter_data.sel_col[2] = (float)edit->sel_col[2] / 255.0f;
    iter_data.nosel_col[0] = (float)edit->nosel_col[0] / 255.0f;
    iter_data.nosel_col[1] = (float)edit->nosel_col[1] / 255.0f;
    iter_data.nosel_col[2] = (float)edit->nosel_col[2] / 255.0f;
  }

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.scheduling_mode = TASK_SCHEDULING_DYNAMIC;
  BLI_task_parallel_range(0, edit->totpoint, &iter_data, psys_cache_edit_paths_iter, &settings);

  edit->totcached = totpart;

  if (psys) {
    ParticleSimulationData sim = {0};
    sim.depsgraph = depsgraph;
    sim.scene = scene;
    sim.ob = ob;
    sim.psys = psys;
    sim.psmd = edit->psmd_eval;

    psys_cache_child_paths(&sim, cfra, true, use_render_params);
  }

  /* clear recalc flag if set here */
  if (recalc_set) {
    PTCacheEditPoint *point;
    int i;
    for (i = 0, point = edit->points; i < totpart; i++, point++) {
      point->flag &= ~PEP_EDIT_RECALC;
    }
  }
}
/************************************************/
/*          Particle Key handling               */
/************************************************/
void copy_particle_key(ParticleKey *to, ParticleKey *from, int time)
{
  if (time) {
    memcpy(to, from, sizeof(ParticleKey));
  }
  else {
    float to_time = to->time;
    memcpy(to, from, sizeof(ParticleKey));
    to->time = to_time;
  }
}
void psys_get_from_key(ParticleKey *key, float loc[3], float vel[3], float rot[4], float *time)
{
  if (loc) {
    copy_v3_v3(loc, key->co);
  }
  if (vel) {
    copy_v3_v3(vel, key->vel);
  }
  if (rot) {
    copy_qt_qt(rot, key->rot);
  }
  if (time) {
    *time = key->time;
  }
}

static void triatomat(float *v1, float *v2, float *v3, float (*uv)[2], float mat[4][4])
{
  float det, w1, w2, d1[2], d2[2];

  memset(mat, 0, sizeof(float) * 4 * 4);
  mat[3][3] = 1.0f;

  /* first axis is the normal */
  normal_tri_v3(mat[2], v1, v2, v3);

  /* second axis along (1, 0) in uv space */
  if (uv) {
    d1[0] = uv[1][0] - uv[0][0];
    d1[1] = uv[1][1] - uv[0][1];
    d2[0] = uv[2][0] - uv[0][0];
    d2[1] = uv[2][1] - uv[0][1];

    det = d2[0] * d1[1] - d2[1] * d1[0];

    if (det != 0.0f) {
      det = 1.0f / det;
      w1 = -d2[1] * det;
      w2 = d1[1] * det;

      mat[1][0] = w1 * (v2[0] - v1[0]) + w2 * (v3[0] - v1[0]);
      mat[1][1] = w1 * (v2[1] - v1[1]) + w2 * (v3[1] - v1[1]);
      mat[1][2] = w1 * (v2[2] - v1[2]) + w2 * (v3[2] - v1[2]);
      normalize_v3(mat[1]);
    }
    else {
      mat[1][0] = mat[1][1] = mat[1][2] = 0.0f;
    }
  }
  else {
    sub_v3_v3v3(mat[1], v2, v1);
    normalize_v3(mat[1]);
  }

  /* third as a cross product */
  cross_v3_v3v3(mat[0], mat[1], mat[2]);
}

static void psys_face_mat(Object *ob, Mesh *mesh, ParticleData *pa, float mat[4][4], int orco)
{
  float v[3][3];
  MFace *mface;
  OrigSpaceFace *osface;
  float(*orcodata)[3];

  int i = (ELEM(pa->num_dmcache, DMCACHE_ISCHILD, DMCACHE_NOTFOUND)) ? pa->num : pa->num_dmcache;
  if (i == -1 || i >= mesh->totface) {
    unit_m4(mat);
    return;
  }

  mface = &mesh->mface[i];
  osface = CustomData_get(&mesh->fdata, i, CD_ORIGSPACE);

  if (orco && (orcodata = CustomData_get_layer(&mesh->vdata, CD_ORCO))) {
    copy_v3_v3(v[0], orcodata[mface->v1]);
    copy_v3_v3(v[1], orcodata[mface->v2]);
    copy_v3_v3(v[2], orcodata[mface->v3]);

    /* ugly hack to use non-transformed orcos, since only those
     * give symmetric results for mirroring in particle mode */
    if (CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX)) {
      BKE_mesh_orco_verts_transform(ob->data, v, 3, 1);
    }
  }
  else {
    copy_v3_v3(v[0], mesh->mvert[mface->v1].co);
    copy_v3_v3(v[1], mesh->mvert[mface->v2].co);
    copy_v3_v3(v[2], mesh->mvert[mface->v3].co);
  }

  triatomat(v[0], v[1], v[2], (osface) ? osface->uv : NULL, mat);
}

void psys_mat_hair_to_object(
    Object *UNUSED(ob), Mesh *mesh, short from, ParticleData *pa, float hairmat[4][4])
{
  float vec[3];

  /* can happen when called from a different object's modifier */
  if (!mesh) {
    unit_m4(hairmat);
    return;
  }

  psys_face_mat(0, mesh, pa, hairmat, 0);
  psys_particle_on_dm(mesh, from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, vec, 0, 0, 0, 0);
  copy_v3_v3(hairmat[3], vec);
}

void psys_mat_hair_to_orco(
    Object *ob, Mesh *mesh, short from, ParticleData *pa, float hairmat[4][4])
{
  float vec[3], orco[3];

  psys_face_mat(ob, mesh, pa, hairmat, 1);
  psys_particle_on_dm(
      mesh, from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, vec, 0, 0, 0, orco);

  /* see psys_face_mat for why this function is called */
  if (CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX)) {
    BKE_mesh_orco_verts_transform(ob->data, &orco, 1, 1);
  }
  copy_v3_v3(hairmat[3], orco);
}

void psys_vec_rot_to_face(Mesh *mesh, ParticleData *pa, float vec[3])
{
  float mat[4][4];

  psys_face_mat(0, mesh, pa, mat, 0);
  transpose_m4(mat); /* cheap inverse for rotation matrix */
  mul_mat3_m4_v3(mat, vec);
}

void psys_mat_hair_to_global(
    Object *ob, Mesh *mesh, short from, ParticleData *pa, float hairmat[4][4])
{
  float facemat[4][4];

  psys_mat_hair_to_object(ob, mesh, from, pa, facemat);

  mul_m4_m4m4(hairmat, ob->obmat, facemat);
}

/************************************************/
/*          ParticleSettings handling           */
/************************************************/
ModifierData *object_add_particle_system(Main *bmain, Scene *scene, Object *ob, const char *name)
{
  ParticleSystem *psys;
  ModifierData *md;
  ParticleSystemModifierData *psmd;

  if (!ob || ob->type != OB_MESH) {
    return NULL;
  }

  if (name == NULL) {
    name = DATA_("ParticleSettings");
  }

  psys = ob->particlesystem.first;
  for (; psys; psys = psys->next) {
    psys->flag &= ~PSYS_CURRENT;
  }

  psys = MEM_callocN(sizeof(ParticleSystem), "particle_system");
  psys->pointcache = BKE_ptcache_add(&psys->ptcaches);
  BLI_addtail(&ob->particlesystem, psys);
  psys_unique_name(ob, psys, name);

  psys->part = BKE_particlesettings_add(bmain, psys->name);

  md = modifier_new(eModifierType_ParticleSystem);
  BLI_strncpy(md->name, psys->name, sizeof(md->name));
  modifier_unique_name(&ob->modifiers, md);

  psmd = (ParticleSystemModifierData *)md;
  psmd->psys = psys;
  BLI_addtail(&ob->modifiers, md);

  psys->totpart = 0;
  psys->flag = PSYS_CURRENT;
  psys->cfra = BKE_scene_frame_get_from_ctime(scene, CFRA + 1);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  return md;
}
void object_remove_particle_system(Main *bmain, Scene *UNUSED(scene), Object *ob)
{
  ParticleSystem *psys = psys_get_current(ob);
  ParticleSystemModifierData *psmd;
  ModifierData *md;

  if (!psys) {
    return;
  }

  /* clear all other appearances of this pointer (like on smoke flow modifier) */
  if ((md = modifiers_findByType(ob, eModifierType_Smoke))) {
    SmokeModifierData *smd = (SmokeModifierData *)md;
    if ((smd->type == MOD_SMOKE_TYPE_FLOW) && smd->flow && smd->flow->psys) {
      if (smd->flow->psys == psys) {
        smd->flow->psys = NULL;
      }
    }
  }

  if ((md = modifiers_findByType(ob, eModifierType_DynamicPaint))) {
    DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
    if (pmd->brush && pmd->brush->psys) {
      if (pmd->brush->psys == psys) {
        pmd->brush->psys = NULL;
      }
    }
  }

  /* clear modifier */
  psmd = psys_get_modifier(ob, psys);
  BLI_remlink(&ob->modifiers, psmd);
  modifier_free((ModifierData *)psmd);

  /* clear particle system */
  BLI_remlink(&ob->particlesystem, psys);
  if (psys->part) {
    id_us_min(&psys->part->id);
  }
  psys_free(ob, psys);

  if (ob->particlesystem.first) {
    ((ParticleSystem *)ob->particlesystem.first)->flag |= PSYS_CURRENT;
  }
  else {
    ob->mode &= ~OB_MODE_PARTICLE_EDIT;
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

static void default_particle_settings(ParticleSettings *part)
{
  part->type = PART_EMITTER;
  part->distr = PART_DISTR_JIT;
  part->draw_as = PART_DRAW_REND;
  part->ren_as = PART_DRAW_HALO;
  part->bb_uv_split = 1;
  part->flag = PART_EDISTR | PART_TRAND | PART_HIDE_ADVANCED_HAIR;

  part->sta = 1.0;
  part->end = 200.0;
  part->lifetime = 50.0;
  part->jitfac = 1.0;
  part->totpart = 1000;
  part->grid_res = 10;
  part->timetweak = 1.0;
  part->courant_target = 0.2;

  part->integrator = PART_INT_MIDPOINT;
  part->phystype = PART_PHYS_NEWTON;
  part->hair_step = 5;
  part->keys_step = 5;
  part->draw_step = 2;
  part->ren_step = 3;
  part->adapt_angle = 5;
  part->adapt_pix = 3;
  part->kink_axis = 2;
  part->kink_amp_clump = 1.f;
  part->kink_extra_steps = 4;
  part->clump_noise_size = 1.0f;
  part->reactevent = PART_EVENT_DEATH;
  part->disp = 100;
  part->from = PART_FROM_FACE;

  part->normfac = 1.0f;

  part->mass = 1.0;
  part->size = 0.05;
  part->childsize = 1.0;

  part->rotmode = PART_ROT_VEL;
  part->avemode = PART_AVE_VELOCITY;

  part->child_nbr = 10;
  part->ren_child_nbr = 100;
  part->childrad = 0.2f;
  part->childflat = 0.0f;
  part->clumppow = 0.0f;
  part->kink_amp = 0.2f;
  part->kink_freq = 2.0;

  part->rough1_size = 1.0;
  part->rough2_size = 1.0;
  part->rough_end_shape = 1.0;

  part->clength = 1.0f;
  part->clength_thres = 0.0f;

  part->draw = 0;
  part->draw_line[0] = 0.5;
  part->path_start = 0.0f;
  part->path_end = 1.0f;

  part->bb_size[0] = part->bb_size[1] = 1.0f;

  part->keyed_loops = 1;

  part->color_vec_max = 1.f;
  part->draw_col = PART_DRAW_COL_MAT;

  if (!part->effector_weights) {
    part->effector_weights = BKE_effector_add_weights(NULL);
  }

  part->omat = 1;
  part->use_modifier_stack = false;
  part->draw_size = 0.1f;

  part->shape_flag = PART_SHAPE_CLOSE_TIP;
  part->shape = 0.0f;
  part->rad_root = 1.0f;
  part->rad_tip = 0.0f;
  part->rad_scale = 0.01f;
}

ParticleSettings *BKE_particlesettings_add(Main *bmain, const char *name)
{
  ParticleSettings *part;

  part = BKE_libblock_alloc(bmain, ID_PA, name, 0);

  default_particle_settings(part);

  return part;
}

void BKE_particlesettings_clump_curve_init(ParticleSettings *part)
{
  CurveMapping *cumap = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);

  cumap->cm[0].curve[0].x = 0.0f;
  cumap->cm[0].curve[0].y = 1.0f;
  cumap->cm[0].curve[1].x = 1.0f;
  cumap->cm[0].curve[1].y = 1.0f;

  curvemapping_initialize(cumap);

  part->clumpcurve = cumap;
}

void BKE_particlesettings_rough_curve_init(ParticleSettings *part)
{
  CurveMapping *cumap = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);

  cumap->cm[0].curve[0].x = 0.0f;
  cumap->cm[0].curve[0].y = 1.0f;
  cumap->cm[0].curve[1].x = 1.0f;
  cumap->cm[0].curve[1].y = 1.0f;

  curvemapping_initialize(cumap);

  part->roughcurve = cumap;
}

void BKE_particlesettings_twist_curve_init(ParticleSettings *part)
{
  CurveMapping *cumap = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);

  cumap->cm[0].curve[0].x = 0.0f;
  cumap->cm[0].curve[0].y = 1.0f;
  cumap->cm[0].curve[1].x = 1.0f;
  cumap->cm[0].curve[1].y = 1.0f;

  curvemapping_initialize(cumap);

  part->twistcurve = cumap;
}

/**
 * Only copy internal data of ParticleSettings ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_particlesettings_copy_data(Main *UNUSED(bmain),
                                    ParticleSettings *part_dst,
                                    const ParticleSettings *part_src,
                                    const int UNUSED(flag))
{
  part_dst->pd = BKE_partdeflect_copy(part_src->pd);
  part_dst->pd2 = BKE_partdeflect_copy(part_src->pd2);
  part_dst->effector_weights = MEM_dupallocN(part_src->effector_weights);
  part_dst->fluid = MEM_dupallocN(part_src->fluid);

  if (part_src->clumpcurve) {
    part_dst->clumpcurve = curvemapping_copy(part_src->clumpcurve);
  }
  if (part_src->roughcurve) {
    part_dst->roughcurve = curvemapping_copy(part_src->roughcurve);
  }
  if (part_src->twistcurve) {
    part_dst->twistcurve = curvemapping_copy(part_src->twistcurve);
  }

  part_dst->boids = boid_copy_settings(part_src->boids);

  for (int a = 0; a < MAX_MTEX; a++) {
    if (part_src->mtex[a]) {
      part_dst->mtex[a] = MEM_dupallocN(part_src->mtex[a]);
    }
  }

  BLI_duplicatelist(&part_dst->instance_weights, &part_src->instance_weights);
}

ParticleSettings *BKE_particlesettings_copy(Main *bmain, const ParticleSettings *part)
{
  ParticleSettings *part_copy;
  BKE_id_copy(bmain, &part->id, (ID **)&part_copy);
  return part_copy;
}

void BKE_particlesettings_make_local(Main *bmain, ParticleSettings *part, const bool lib_local)
{
  BKE_id_make_local_generic(bmain, &part->id, true, lib_local);
}

/************************************************/
/*          Textures                            */
/************************************************/

static int get_particle_uv(Mesh *mesh,
                           ParticleData *pa,
                           int index,
                           const float fuv[4],
                           char *name,
                           float *texco,
                           bool from_vert)
{
  MFace *mf;
  MTFace *tf;
  int i;

  tf = CustomData_get_layer_named(&mesh->fdata, CD_MTFACE, name);

  if (tf == NULL) {
    tf = mesh->mtface;
  }

  if (tf == NULL) {
    return 0;
  }

  if (pa) {
    i = ELEM(pa->num_dmcache, DMCACHE_NOTFOUND, DMCACHE_ISCHILD) ? pa->num : pa->num_dmcache;
    if ((!from_vert && i >= mesh->totface) || (from_vert && i >= mesh->totvert)) {
      i = -1;
    }
  }
  else {
    i = index;
  }

  if (i == -1) {
    texco[0] = 0.0f;
    texco[1] = 0.0f;
    texco[2] = 0.0f;
  }
  else {
    if (from_vert) {
      mf = mesh->mface;

      /* This finds the first face to contain the emitting vertex,
       * this is not ideal, but is mostly fine as UV seams generally
       * map to equal-colored parts of a texture */
      for (int j = 0; j < mesh->totface; j++, mf++) {
        if (ELEM(i, mf->v1, mf->v2, mf->v3, mf->v4)) {
          i = j;
          break;
        }
      }
    }
    else {
      mf = &mesh->mface[i];
    }

    psys_interpolate_uvs(&tf[i], mf->v4, fuv, texco);

    texco[0] = texco[0] * 2.0f - 1.0f;
    texco[1] = texco[1] * 2.0f - 1.0f;
    texco[2] = 0.0f;
  }

  return 1;
}

#define SET_PARTICLE_TEXTURE(type, pvalue, texfac) \
  if ((event & mtex->mapto) & type) { \
    pvalue = texture_value_blend(def, pvalue, value, texfac, blend); \
  } \
  (void)0

#define CLAMP_PARTICLE_TEXTURE_POS(type, pvalue) \
  if (event & type) { \
    CLAMP(pvalue, 0.0f, 1.0f); \
  } \
  (void)0

#define CLAMP_WARP_PARTICLE_TEXTURE_POS(type, pvalue) \
  if (event & type) { \
    if (pvalue < 0.0f) \
      pvalue = 1.0f + pvalue; \
    CLAMP(pvalue, 0.0f, 1.0f); \
  } \
  (void)0

#define CLAMP_PARTICLE_TEXTURE_POSNEG(type, pvalue) \
  if (event & type) { \
    CLAMP(pvalue, -1.0f, 1.0f); \
  } \
  (void)0

static void get_cpa_texture(Mesh *mesh,
                            ParticleSystem *psys,
                            ParticleSettings *part,
                            ParticleData *par,
                            int child_index,
                            int face_index,
                            const float fw[4],
                            float *orco,
                            ParticleTexture *ptex,
                            int event,
                            float cfra)
{
  MTex *mtex, **mtexp = part->mtex;
  int m;
  float value, rgba[4], texvec[3];

  ptex->ivel = ptex->life = ptex->exist = ptex->size = ptex->damp = ptex->gravity = ptex->field =
      ptex->time = ptex->clump = ptex->kink_freq = ptex->kink_amp = ptex->effector = ptex->rough1 =
          ptex->rough2 = ptex->roughe = 1.0f;
  ptex->twist = 1.0f;

  ptex->length = 1.0f - part->randlength * psys_frand(psys, child_index + 26);
  ptex->length *= part->clength_thres < psys_frand(psys, child_index + 27) ? part->clength : 1.0f;

  for (m = 0; m < MAX_MTEX; m++, mtexp++) {
    mtex = *mtexp;
    if (mtex && mtex->tex && mtex->mapto) {
      float def = mtex->def_var;
      short blend = mtex->blendtype;
      short texco = mtex->texco;

      if (ELEM(texco, TEXCO_UV, TEXCO_ORCO) &&
          (ELEM(part->from, PART_FROM_FACE, PART_FROM_VOLUME) == 0 ||
           part->distr == PART_DISTR_GRID)) {
        texco = TEXCO_GLOB;
      }

      switch (texco) {
        case TEXCO_GLOB:
          copy_v3_v3(texvec, par->state.co);
          break;
        case TEXCO_OBJECT:
          copy_v3_v3(texvec, par->state.co);
          if (mtex->object) {
            mul_m4_v3(mtex->object->imat, texvec);
          }
          break;
        case TEXCO_UV:
          if (fw && get_particle_uv(mesh,
                                    NULL,
                                    face_index,
                                    fw,
                                    mtex->uvname,
                                    texvec,
                                    (part->from == PART_FROM_VERT))) {
            break;
          }
          /* no break, failed to get uv's, so let's try orco's */
          ATTR_FALLTHROUGH;
        case TEXCO_ORCO:
          copy_v3_v3(texvec, orco);
          break;
        case TEXCO_PARTICLE:
          /* texture coordinates in range [-1, 1] */
          texvec[0] = 2.f * (cfra - par->time) / (par->dietime - par->time) - 1.f;
          texvec[1] = 0.f;
          texvec[2] = 0.f;
          break;
      }

      externtex(mtex, texvec, &value, rgba, rgba + 1, rgba + 2, rgba + 3, 0, NULL, false, false);

      if ((event & mtex->mapto) & PAMAP_ROUGH) {
        ptex->rough1 = ptex->rough2 = ptex->roughe = texture_value_blend(
            def, ptex->rough1, value, mtex->roughfac, blend);
      }

      SET_PARTICLE_TEXTURE(PAMAP_LENGTH, ptex->length, mtex->lengthfac);
      SET_PARTICLE_TEXTURE(PAMAP_CLUMP, ptex->clump, mtex->clumpfac);
      SET_PARTICLE_TEXTURE(PAMAP_KINK_AMP, ptex->kink_amp, mtex->kinkampfac);
      SET_PARTICLE_TEXTURE(PAMAP_KINK_FREQ, ptex->kink_freq, mtex->kinkfac);
      SET_PARTICLE_TEXTURE(PAMAP_DENS, ptex->exist, mtex->padensfac);
      SET_PARTICLE_TEXTURE(PAMAP_TWIST, ptex->twist, mtex->twistfac);
    }
  }

  CLAMP_PARTICLE_TEXTURE_POS(PAMAP_LENGTH, ptex->length);
  CLAMP_WARP_PARTICLE_TEXTURE_POS(PAMAP_CLUMP, ptex->clump);
  CLAMP_WARP_PARTICLE_TEXTURE_POS(PAMAP_KINK_AMP, ptex->kink_amp);
  CLAMP_WARP_PARTICLE_TEXTURE_POS(PAMAP_KINK_FREQ, ptex->kink_freq);
  CLAMP_WARP_PARTICLE_TEXTURE_POS(PAMAP_ROUGH, ptex->rough1);
  CLAMP_WARP_PARTICLE_TEXTURE_POS(PAMAP_DENS, ptex->exist);
}
void psys_get_texture(
    ParticleSimulationData *sim, ParticleData *pa, ParticleTexture *ptex, int event, float cfra)
{
  Object *ob = sim->ob;
  Mesh *me = (Mesh *)ob->data;
  ParticleSettings *part = sim->psys->part;
  MTex **mtexp = part->mtex;
  MTex *mtex;
  int m;
  float value, rgba[4], co[3], texvec[3];
  int setvars = 0;

  /* initialize ptex */
  ptex->ivel = ptex->life = ptex->exist = ptex->size = ptex->damp = ptex->gravity = ptex->field =
      ptex->length = ptex->clump = ptex->kink_freq = ptex->kink_amp = ptex->effector =
          ptex->rough1 = ptex->rough2 = ptex->roughe = 1.0f;
  ptex->twist = 1.0f;

  ptex->time = (float)(pa - sim->psys->particles) / (float)sim->psys->totpart;

  for (m = 0; m < MAX_MTEX; m++, mtexp++) {
    mtex = *mtexp;
    if (mtex && mtex->tex && mtex->mapto) {
      float def = mtex->def_var;
      short blend = mtex->blendtype;
      short texco = mtex->texco;

      if (texco == TEXCO_UV && (ELEM(part->from, PART_FROM_FACE, PART_FROM_VOLUME) == 0 ||
                                part->distr == PART_DISTR_GRID)) {
        texco = TEXCO_GLOB;
      }

      switch (texco) {
        case TEXCO_GLOB:
          copy_v3_v3(texvec, pa->state.co);
          break;
        case TEXCO_OBJECT:
          copy_v3_v3(texvec, pa->state.co);
          if (mtex->object) {
            mul_m4_v3(mtex->object->imat, texvec);
          }
          break;
        case TEXCO_UV: {
          Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(sim->psmd);
          if (get_particle_uv(mesh_final,
                              pa,
                              0,
                              pa->fuv,
                              mtex->uvname,
                              texvec,
                              (part->from == PART_FROM_VERT))) {
            break;
          }
          /* no break, failed to get uv's, so let's try orco's */
          ATTR_FALLTHROUGH;
        }
        case TEXCO_ORCO:
          psys_particle_on_emitter(sim->psmd,
                                   sim->psys->part->from,
                                   pa->num,
                                   pa->num_dmcache,
                                   pa->fuv,
                                   pa->foffset,
                                   co,
                                   0,
                                   0,
                                   0,
                                   texvec);

          if (me->bb == NULL || (me->bb->flag & BOUNDBOX_DIRTY)) {
            BKE_mesh_texspace_calc(me);
          }
          sub_v3_v3(texvec, me->loc);
          if (me->size[0] != 0.0f) {
            texvec[0] /= me->size[0];
          }
          if (me->size[1] != 0.0f) {
            texvec[1] /= me->size[1];
          }
          if (me->size[2] != 0.0f) {
            texvec[2] /= me->size[2];
          }
          break;
        case TEXCO_PARTICLE:
          /* texture coordinates in range [-1, 1] */
          texvec[0] = 2.f * (cfra - pa->time) / (pa->dietime - pa->time) - 1.f;
          if (sim->psys->totpart > 0) {
            texvec[1] = 2.f * (float)(pa - sim->psys->particles) / (float)sim->psys->totpart - 1.f;
          }
          else {
            texvec[1] = 0.0f;
          }
          texvec[2] = 0.f;
          break;
      }

      externtex(mtex, texvec, &value, rgba, rgba + 1, rgba + 2, rgba + 3, 0, NULL, false, false);

      if ((event & mtex->mapto) & PAMAP_TIME) {
        /* the first time has to set the base value for time regardless of blend mode */
        if ((setvars & MAP_PA_TIME) == 0) {
          int flip = (mtex->timefac < 0.0f);
          float timefac = fabsf(mtex->timefac);
          ptex->time *= 1.0f - timefac;
          ptex->time += timefac * ((flip) ? 1.0f - value : value);
          setvars |= MAP_PA_TIME;
        }
        else {
          ptex->time = texture_value_blend(def, ptex->time, value, mtex->timefac, blend);
        }
      }
      SET_PARTICLE_TEXTURE(PAMAP_LIFE, ptex->life, mtex->lifefac);
      SET_PARTICLE_TEXTURE(PAMAP_DENS, ptex->exist, mtex->padensfac);
      SET_PARTICLE_TEXTURE(PAMAP_SIZE, ptex->size, mtex->sizefac);
      SET_PARTICLE_TEXTURE(PAMAP_IVEL, ptex->ivel, mtex->ivelfac);
      SET_PARTICLE_TEXTURE(PAMAP_FIELD, ptex->field, mtex->fieldfac);
      SET_PARTICLE_TEXTURE(PAMAP_GRAVITY, ptex->gravity, mtex->gravityfac);
      SET_PARTICLE_TEXTURE(PAMAP_DAMP, ptex->damp, mtex->dampfac);
      SET_PARTICLE_TEXTURE(PAMAP_LENGTH, ptex->length, mtex->lengthfac);
      SET_PARTICLE_TEXTURE(PAMAP_TWIST, ptex->twist, mtex->twistfac);
    }
  }

  CLAMP_WARP_PARTICLE_TEXTURE_POS(PAMAP_TIME, ptex->time);
  CLAMP_WARP_PARTICLE_TEXTURE_POS(PAMAP_LIFE, ptex->life);
  CLAMP_WARP_PARTICLE_TEXTURE_POS(PAMAP_DENS, ptex->exist);
  CLAMP_PARTICLE_TEXTURE_POS(PAMAP_SIZE, ptex->size);
  CLAMP_PARTICLE_TEXTURE_POSNEG(PAMAP_IVEL, ptex->ivel);
  CLAMP_PARTICLE_TEXTURE_POSNEG(PAMAP_FIELD, ptex->field);
  CLAMP_PARTICLE_TEXTURE_POSNEG(PAMAP_GRAVITY, ptex->gravity);
  CLAMP_WARP_PARTICLE_TEXTURE_POS(PAMAP_DAMP, ptex->damp);
  CLAMP_PARTICLE_TEXTURE_POS(PAMAP_LENGTH, ptex->length);
}
/************************************************/
/*          Particle State                      */
/************************************************/
float psys_get_timestep(ParticleSimulationData *sim)
{
  return 0.04f * sim->psys->part->timetweak;
}
float psys_get_child_time(
    ParticleSystem *psys, ChildParticle *cpa, float cfra, float *birthtime, float *dietime)
{
  ParticleSettings *part = psys->part;
  float time, life;

  if (part->childtype == PART_CHILD_FACES) {
    int w = 0;
    time = 0.0;
    while (w < 4 && cpa->pa[w] >= 0) {
      time += cpa->w[w] * (psys->particles + cpa->pa[w])->time;
      w++;
    }

    life = part->lifetime * (1.0f - part->randlife * psys_frand(psys, cpa - psys->child + 25));
  }
  else {
    ParticleData *pa = psys->particles + cpa->parent;

    time = pa->time;
    life = pa->lifetime;
  }

  if (birthtime) {
    *birthtime = time;
  }
  if (dietime) {
    *dietime = time + life;
  }

  return (cfra - time) / life;
}
float psys_get_child_size(ParticleSystem *psys,
                          ChildParticle *cpa,
                          float UNUSED(cfra),
                          float *UNUSED(pa_time))
{
  ParticleSettings *part = psys->part;
  float size;  // time XXX

  if (part->childtype == PART_CHILD_FACES) {
    int w = 0;
    size = 0.0;
    while (w < 4 && cpa->pa[w] >= 0) {
      size += cpa->w[w] * (psys->particles + cpa->pa[w])->size;
      w++;
    }
  }
  else {
    size = psys->particles[cpa->parent].size;
  }

  size *= part->childsize;

  if (part->childrandsize != 0.0f) {
    size *= 1.0f - part->childrandsize * psys_frand(psys, cpa - psys->child + 26);
  }

  return size;
}
static void get_child_modifier_parameters(ParticleSettings *part,
                                          ParticleThreadContext *ctx,
                                          ChildParticle *cpa,
                                          short cpa_from,
                                          int cpa_num,
                                          float *cpa_fuv,
                                          float *orco,
                                          ParticleTexture *ptex)
{
  ParticleSystem *psys = ctx->sim.psys;
  int i = cpa - psys->child;

  get_cpa_texture(ctx->mesh,
                  psys,
                  part,
                  psys->particles + cpa->pa[0],
                  i,
                  cpa_num,
                  cpa_fuv,
                  orco,
                  ptex,
                  PAMAP_DENS | PAMAP_CHILD,
                  psys->cfra);

  if (ptex->exist < psys_frand(psys, i + 24)) {
    return;
  }

  if (ctx->vg_length) {
    ptex->length *= psys_interpolate_value_from_verts(
        ctx->mesh, cpa_from, cpa_num, cpa_fuv, ctx->vg_length);
  }
  if (ctx->vg_clump) {
    ptex->clump *= psys_interpolate_value_from_verts(
        ctx->mesh, cpa_from, cpa_num, cpa_fuv, ctx->vg_clump);
  }
  if (ctx->vg_kink) {
    ptex->kink_freq *= psys_interpolate_value_from_verts(
        ctx->mesh, cpa_from, cpa_num, cpa_fuv, ctx->vg_kink);
  }
  if (ctx->vg_rough1) {
    ptex->rough1 *= psys_interpolate_value_from_verts(
        ctx->mesh, cpa_from, cpa_num, cpa_fuv, ctx->vg_rough1);
  }
  if (ctx->vg_rough2) {
    ptex->rough2 *= psys_interpolate_value_from_verts(
        ctx->mesh, cpa_from, cpa_num, cpa_fuv, ctx->vg_rough2);
  }
  if (ctx->vg_roughe) {
    ptex->roughe *= psys_interpolate_value_from_verts(
        ctx->mesh, cpa_from, cpa_num, cpa_fuv, ctx->vg_roughe);
  }
  if (ctx->vg_effector) {
    ptex->effector *= psys_interpolate_value_from_verts(
        ctx->mesh, cpa_from, cpa_num, cpa_fuv, ctx->vg_effector);
  }
  if (ctx->vg_twist) {
    ptex->twist *= psys_interpolate_value_from_verts(
        ctx->mesh, cpa_from, cpa_num, cpa_fuv, ctx->vg_twist);
  }
}
/* get's hair (or keyed) particles state at the "path time" specified in state->time */
void psys_get_particle_on_path(ParticleSimulationData *sim,
                               int p,
                               ParticleKey *state,
                               const bool vel)
{
  PARTICLE_PSMD;
  ParticleSystem *psys = sim->psys;
  ParticleSettings *part = sim->psys->part;
  Material *ma = give_current_material(sim->ob, part->omat);
  ParticleData *pa;
  ChildParticle *cpa;
  ParticleTexture ptex;
  ParticleKey *par = 0, keys[4], tstate;
  ParticleThreadContext ctx; /* fake thread context for child modifiers */
  ParticleInterpolationData pind;

  float t;
  float co[3], orco[3];
  float hairmat[4][4];
  int totpart = psys->totpart;
  int totchild = psys->totchild;
  short between = 0, edit = 0;

  int keyed = part->phystype & PART_PHYS_KEYED && psys->flag & PSYS_KEYED;
  int cached = !keyed && part->type != PART_HAIR;

  float *cpa_fuv;
  int cpa_num;
  short cpa_from;

  /* initialize keys to zero */
  memset(keys, 0, 4 * sizeof(ParticleKey));

  t = state->time;
  CLAMP(t, 0.0f, 1.0f);

  if (p < totpart) {
    /* interpolate pathcache directly if it exist */
    if (psys->pathcache) {
      ParticleCacheKey result;
      interpolate_pathcache(psys->pathcache[p], t, &result);
      copy_v3_v3(state->co, result.co);
      copy_v3_v3(state->vel, result.vel);
      copy_qt_qt(state->rot, result.rot);
    }
    /* otherwise interpolate with other means */
    else {
      pa = psys->particles + p;

      pind.keyed = keyed;
      pind.cache = cached ? psys->pointcache : NULL;
      pind.epoint = NULL;
      pind.bspline = (psys->part->flag & PART_HAIR_BSPLINE);
      /* pind.dm disabled in editmode means we don't get effectors taken into
       * account when subdividing for instance */
      pind.mesh = psys_in_edit_mode(sim->depsgraph, psys) ?
                      NULL :
                      psys->hair_out_mesh; /* XXX Sybren EEK */
      init_particle_interpolation(sim->ob, psys, pa, &pind);
      do_particle_interpolation(psys, p, pa, t, &pind, state);

      if (pind.mesh) {
        mul_m4_v3(sim->ob->obmat, state->co);
        mul_mat3_m4_v3(sim->ob->obmat, state->vel);
      }
      else if (!keyed && !cached && !(psys->flag & PSYS_GLOBAL_HAIR)) {
        if ((pa->flag & PARS_REKEY) == 0) {
          Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(sim->psmd);
          psys_mat_hair_to_global(sim->ob, mesh_final, part->from, pa, hairmat);
          mul_m4_v3(hairmat, state->co);
          mul_mat3_m4_v3(hairmat, state->vel);

          if (sim->psys->effectors && (part->flag & PART_CHILD_GUIDE) == 0) {
            do_guides(
                sim->depsgraph, sim->psys->part, sim->psys->effectors, state, p, state->time);
            /* TODO: proper velocity handling */
          }

          if (psys->lattice_deform_data && edit == 0) {
            calc_latt_deform(psys->lattice_deform_data, state->co, psys->lattice_strength);
          }
        }
      }
    }
  }
  else if (totchild) {
    // invert_m4_m4(imat, ob->obmat);

    /* interpolate childcache directly if it exists */
    if (psys->childcache) {
      ParticleCacheKey result;
      interpolate_pathcache(psys->childcache[p - totpart], t, &result);
      copy_v3_v3(state->co, result.co);
      copy_v3_v3(state->vel, result.vel);
      copy_qt_qt(state->rot, result.rot);
    }
    else {
      float par_co[3], par_orco[3];

      cpa = psys->child + p - totpart;

      if (state->time < 0.0f) {
        t = psys_get_child_time(psys, cpa, -state->time, NULL, NULL);
      }

      if (totchild && part->childtype == PART_CHILD_FACES) {
        /* part->parents could still be 0 so we can't test with totparent */
        between = 1;
      }
      if (between) {
        int w = 0;
        float foffset;

        /* get parent states */
        while (w < 4 && cpa->pa[w] >= 0) {
          keys[w].time = state->time;
          psys_get_particle_on_path(sim, cpa->pa[w], keys + w, 1);
          w++;
        }

        /* get the original coordinates (orco) for texture usage */
        cpa_num = cpa->num;

        foffset = cpa->foffset;
        cpa_fuv = cpa->fuv;
        cpa_from = PART_FROM_FACE;

        psys_particle_on_emitter(
            psmd, cpa_from, cpa_num, DMCACHE_ISCHILD, cpa->fuv, foffset, co, 0, 0, 0, orco);

        /* We need to save the actual root position of the child for
         * positioning it accurately to the surface of the emitter. */
        // copy_v3_v3(cpa_1st, co);

        // mul_m4_v3(ob->obmat, cpa_1st);

        pa = psys->particles + cpa->parent;

        psys_particle_on_emitter(psmd,
                                 part->from,
                                 pa->num,
                                 pa->num_dmcache,
                                 pa->fuv,
                                 pa->foffset,
                                 par_co,
                                 0,
                                 0,
                                 0,
                                 par_orco);
        if (part->type == PART_HAIR) {
          Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(psmd);
          psys_mat_hair_to_global(sim->ob, mesh_final, psys->part->from, pa, hairmat);
        }
        else {
          unit_m4(hairmat);
        }

        pa = 0;
      }
      else {
        /* get the parent state */
        keys->time = state->time;
        psys_get_particle_on_path(sim, cpa->parent, keys, 1);

        /* get the original coordinates (orco) for texture usage */
        pa = psys->particles + cpa->parent;

        cpa_from = part->from;
        cpa_num = pa->num;
        cpa_fuv = pa->fuv;

        psys_particle_on_emitter(psmd,
                                 part->from,
                                 pa->num,
                                 pa->num_dmcache,
                                 pa->fuv,
                                 pa->foffset,
                                 par_co,
                                 0,
                                 0,
                                 0,
                                 par_orco);
        if (part->type == PART_HAIR) {
          Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(psmd);
          psys_particle_on_emitter(
              psmd, cpa_from, cpa_num, DMCACHE_ISCHILD, cpa_fuv, pa->foffset, co, 0, 0, 0, orco);
          psys_mat_hair_to_global(sim->ob, mesh_final, psys->part->from, pa, hairmat);
        }
        else {
          copy_v3_v3(orco, cpa->fuv);
          unit_m4(hairmat);
        }
      }

      /* get different child parameters from textures & vgroups */
      memset(&ctx, 0, sizeof(ParticleThreadContext));
      ctx.sim = *sim;
      ctx.mesh = BKE_particle_modifier_mesh_final_get(psmd);
      ctx.ma = ma;
      /* TODO: assign vertex groups */
      get_child_modifier_parameters(part, &ctx, cpa, cpa_from, cpa_num, cpa_fuv, orco, &ptex);

      if (between) {
        int w = 0;

        state->co[0] = state->co[1] = state->co[2] = 0.0f;
        state->vel[0] = state->vel[1] = state->vel[2] = 0.0f;

        /* child position is the weighted sum of parent positions */
        while (w < 4 && cpa->pa[w] >= 0) {
          state->co[0] += cpa->w[w] * keys[w].co[0];
          state->co[1] += cpa->w[w] * keys[w].co[1];
          state->co[2] += cpa->w[w] * keys[w].co[2];

          state->vel[0] += cpa->w[w] * keys[w].vel[0];
          state->vel[1] += cpa->w[w] * keys[w].vel[1];
          state->vel[2] += cpa->w[w] * keys[w].vel[2];
          w++;
        }
        /* apply offset for correct positioning */
        // add_v3_v3(state->co, cpa_1st);
      }
      else {
        /* offset the child from the parent position */
        offset_child(cpa, keys, keys->rot, state, part->childflat, part->childrad);
      }

      par = keys;

      if (vel) {
        copy_particle_key(&tstate, state, 1);
      }

      /* apply different deformations to the child path */
      ParticleChildModifierContext modifier_ctx = {NULL};
      modifier_ctx.thread_ctx = NULL;
      modifier_ctx.sim = sim;
      modifier_ctx.ptex = &ptex;
      modifier_ctx.cpa = cpa;
      modifier_ctx.orco = orco;
      modifier_ctx.par_co = par->co;
      modifier_ctx.par_vel = par->vel;
      modifier_ctx.par_rot = par->rot;
      modifier_ctx.par_orco = par_orco;
      modifier_ctx.parent_keys = psys->childcache ? psys->childcache[p - totpart] : NULL;
      do_child_modifiers(&modifier_ctx, hairmat, state, t);

      /* try to estimate correct velocity */
      if (vel) {
        ParticleKey tstate_tmp;
        float length = len_v3(state->vel);

        if (t >= 0.001f) {
          tstate_tmp.time = t - 0.001f;
          psys_get_particle_on_path(sim, p, &tstate_tmp, 0);
          sub_v3_v3v3(state->vel, state->co, tstate_tmp.co);
          normalize_v3(state->vel);
        }
        else {
          tstate_tmp.time = t + 0.001f;
          psys_get_particle_on_path(sim, p, &tstate_tmp, 0);
          sub_v3_v3v3(state->vel, tstate_tmp.co, state->co);
          normalize_v3(state->vel);
        }

        mul_v3_fl(state->vel, length);
      }
    }
  }
}
/* gets particle's state at a time, returns 1 if particle exists and can be seen and 0 if not */
int psys_get_particle_state(ParticleSimulationData *sim, int p, ParticleKey *state, int always)
{
  ParticleSystem *psys = sim->psys;
  ParticleSettings *part = psys->part;
  ParticleData *pa = NULL;
  ChildParticle *cpa = NULL;
  float cfra;
  int totpart = psys->totpart;
  float timestep = psys_get_timestep(sim);

  /* negative time means "use current time" */
  cfra = state->time > 0 ? state->time : DEG_get_ctime(sim->depsgraph);

  if (p >= totpart) {
    if (!psys->totchild) {
      return 0;
    }

    if (part->childtype == PART_CHILD_FACES) {
      if (!(psys->flag & PSYS_KEYED)) {
        return 0;
      }

      cpa = psys->child + p - totpart;

      state->time = psys_get_child_time(psys, cpa, cfra, NULL, NULL);

      if (!always) {
        if ((state->time < 0.0f && !(part->flag & PART_UNBORN)) ||
            (state->time > 1.0f && !(part->flag & PART_DIED))) {
          return 0;
        }
      }

      state->time = (cfra - (part->sta + (part->end - part->sta) * psys_frand(psys, p + 23))) /
                    (part->lifetime * psys_frand(psys, p + 24));

      psys_get_particle_on_path(sim, p, state, 1);
      return 1;
    }
    else {
      cpa = sim->psys->child + p - totpart;
      pa = sim->psys->particles + cpa->parent;
    }
  }
  else {
    pa = sim->psys->particles + p;
  }

  if (pa) {
    if (!always) {
      if ((cfra < pa->time && (part->flag & PART_UNBORN) == 0) ||
          (cfra >= pa->dietime && (part->flag & PART_DIED) == 0)) {
        return 0;
      }
    }

    cfra = MIN2(cfra, pa->dietime);
  }

  if (sim->psys->flag & PSYS_KEYED) {
    state->time = -cfra;
    psys_get_particle_on_path(sim, p, state, 1);
    return 1;
  }
  else {
    if (cpa) {
      float mat[4][4];
      ParticleKey *key1;
      float t = (cfra - pa->time) / pa->lifetime;
      float par_orco[3] = {0.0f, 0.0f, 0.0f};

      key1 = &pa->state;
      offset_child(cpa, key1, key1->rot, state, part->childflat, part->childrad);

      CLAMP(t, 0.0f, 1.0f);

      unit_m4(mat);
      ParticleChildModifierContext modifier_ctx = {NULL};
      modifier_ctx.thread_ctx = NULL;
      modifier_ctx.sim = sim;
      modifier_ctx.ptex = NULL;
      modifier_ctx.cpa = cpa;
      modifier_ctx.orco = cpa->fuv;
      modifier_ctx.par_co = key1->co;
      modifier_ctx.par_vel = key1->vel;
      modifier_ctx.par_rot = key1->rot;
      modifier_ctx.par_orco = par_orco;
      modifier_ctx.parent_keys = psys->childcache ? psys->childcache[p - totpart] : NULL;

      do_child_modifiers(&modifier_ctx, mat, state, t);

      if (psys->lattice_deform_data) {
        calc_latt_deform(psys->lattice_deform_data, state->co, psys->lattice_strength);
      }
    }
    else {
      if (pa->state.time == cfra || ELEM(part->phystype, PART_PHYS_NO, PART_PHYS_KEYED)) {
        copy_particle_key(state, &pa->state, 1);
      }
      else if (pa->prev_state.time == cfra) {
        copy_particle_key(state, &pa->prev_state, 1);
      }
      else {
        float dfra, frs_sec = sim->scene->r.frs_sec;
        /* let's interpolate to try to be as accurate as possible */
        if (pa->state.time + 2.f >= state->time && pa->prev_state.time - 2.f <= state->time) {
          if (pa->prev_state.time >= pa->state.time || pa->prev_state.time < 0.f) {
            /* prev_state is wrong so let's not use it,
             * this can happen at frames 1, 0 or particle birth. */
            dfra = state->time - pa->state.time;

            copy_particle_key(state, &pa->state, 1);

            madd_v3_v3v3fl(state->co, state->co, state->vel, dfra / frs_sec);
          }
          else {
            ParticleKey keys[4];
            float keytime;

            copy_particle_key(keys + 1, &pa->prev_state, 1);
            copy_particle_key(keys + 2, &pa->state, 1);

            dfra = keys[2].time - keys[1].time;

            keytime = (state->time - keys[1].time) / dfra;

            /* convert velocity to timestep size */
            mul_v3_fl(keys[1].vel, dfra * timestep);
            mul_v3_fl(keys[2].vel, dfra * timestep);

            psys_interpolate_particle(-1, keys, keytime, state, 1);

            /* convert back to real velocity */
            mul_v3_fl(state->vel, 1.f / (dfra * timestep));

            interp_v3_v3v3(state->ave, keys[1].ave, keys[2].ave, keytime);
            interp_qt_qtqt(state->rot, keys[1].rot, keys[2].rot, keytime);
          }
        }
        else if (pa->state.time + 1.f >= state->time && pa->state.time - 1.f <= state->time) {
          /* linear interpolation using only pa->state */

          dfra = state->time - pa->state.time;

          copy_particle_key(state, &pa->state, 1);

          madd_v3_v3v3fl(state->co, state->co, state->vel, dfra / frs_sec);
        }
        else {
          /* Extrapolating over big ranges is not accurate
           * so let's just give something close to reasonable back. */
          copy_particle_key(state, &pa->state, 0);
        }
      }

      if (sim->psys->lattice_deform_data) {
        calc_latt_deform(sim->psys->lattice_deform_data, state->co, psys->lattice_strength);
      }
    }

    return 1;
  }
}

void psys_get_dupli_texture(ParticleSystem *psys,
                            ParticleSettings *part,
                            ParticleSystemModifierData *psmd,
                            ParticleData *pa,
                            ChildParticle *cpa,
                            float uv[2],
                            float orco[3])
{
  MFace *mface;
  MTFace *mtface;
  float loc[3];
  int num;

  /* XXX: on checking '(psmd->dm != NULL)'
   * This is incorrect but needed for metaball evaluation.
   * Ideally this would be calculated via the depsgraph, however with metaballs,
   * the entire scenes dupli's are scanned, which also looks into uncalculated data.
   *
   * For now just include this workaround as an alternative to crashing,
   * but longer term metaballs should behave in a more manageable way, see: T46622. */

  uv[0] = uv[1] = 0.f;

  /* Grid distribution doesn't support UV or emit from vertex mode */
  bool is_grid = (part->distr == PART_DISTR_GRID && part->from != PART_FROM_VERT);

  Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(psmd);

  if (cpa) {
    if ((part->childtype == PART_CHILD_FACES) && (mesh_final != NULL)) {
      CustomData *mtf_data = &mesh_final->fdata;
      const int uv_idx = CustomData_get_render_layer(mtf_data, CD_MTFACE);
      mtface = CustomData_get_layer_n(mtf_data, CD_MTFACE, uv_idx);

      if (mtface && !is_grid) {
        mface = CustomData_get(&mesh_final->fdata, cpa->num, CD_MFACE);
        mtface += cpa->num;
        psys_interpolate_uvs(mtface, mface->v4, cpa->fuv, uv);
      }

      psys_particle_on_emitter(psmd,
                               PART_FROM_FACE,
                               cpa->num,
                               DMCACHE_ISCHILD,
                               cpa->fuv,
                               cpa->foffset,
                               loc,
                               0,
                               0,
                               0,
                               orco);
      return;
    }
    else {
      pa = psys->particles + cpa->pa[0];
    }
  }

  if ((part->from == PART_FROM_FACE) && (mesh_final != NULL) && !is_grid) {
    CustomData *mtf_data = &mesh_final->fdata;
    const int uv_idx = CustomData_get_render_layer(mtf_data, CD_MTFACE);
    mtface = CustomData_get_layer_n(mtf_data, CD_MTFACE, uv_idx);

    num = pa->num_dmcache;

    if (num == DMCACHE_NOTFOUND) {
      num = pa->num;
    }

    if (num >= mesh_final->totface) {
      /* happens when simplify is enabled
       * gives invalid coords but would crash otherwise */
      num = DMCACHE_NOTFOUND;
    }

    if (mtface && !ELEM(num, DMCACHE_NOTFOUND, DMCACHE_ISCHILD)) {
      mface = CustomData_get(&mesh_final->fdata, num, CD_MFACE);
      mtface += num;
      psys_interpolate_uvs(mtface, mface->v4, pa->fuv, uv);
    }
  }

  psys_particle_on_emitter(
      psmd, part->from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, loc, 0, 0, 0, orco);
}

void psys_get_dupli_path_transform(ParticleSimulationData *sim,
                                   ParticleData *pa,
                                   ChildParticle *cpa,
                                   ParticleCacheKey *cache,
                                   float mat[4][4],
                                   float *scale)
{
  Object *ob = sim->ob;
  ParticleSystem *psys = sim->psys;
  ParticleSystemModifierData *psmd = sim->psmd;
  float loc[3], nor[3], vec[3], side[3], len;
  float xvec[3] = {-1.0, 0.0, 0.0}, nmat[3][3];

  sub_v3_v3v3(vec, (cache + cache->segments)->co, cache->co);
  len = normalize_v3(vec);

  if (pa == NULL && psys->part->childflat != PART_CHILD_FACES) {
    pa = psys->particles + cpa->pa[0];
  }

  if (pa) {
    psys_particle_on_emitter(psmd,
                             sim->psys->part->from,
                             pa->num,
                             pa->num_dmcache,
                             pa->fuv,
                             pa->foffset,
                             loc,
                             nor,
                             0,
                             0,
                             0);
  }
  else {
    psys_particle_on_emitter(psmd,
                             PART_FROM_FACE,
                             cpa->num,
                             DMCACHE_ISCHILD,
                             cpa->fuv,
                             cpa->foffset,
                             loc,
                             nor,
                             0,
                             0,
                             0);
  }

  if (psys->part->rotmode == PART_ROT_VEL) {
    transpose_m3_m4(nmat, ob->imat);
    mul_m3_v3(nmat, nor);
    normalize_v3(nor);

    /* make sure that we get a proper side vector */
    if (fabsf(dot_v3v3(nor, vec)) > 0.999999f) {
      if (fabsf(dot_v3v3(nor, xvec)) > 0.999999f) {
        nor[0] = 0.0f;
        nor[1] = 1.0f;
        nor[2] = 0.0f;
      }
      else {
        nor[0] = 1.0f;
        nor[1] = 0.0f;
        nor[2] = 0.0f;
      }
    }
    cross_v3_v3v3(side, nor, vec);
    normalize_v3(side);

    /* rotate side vector around vec */
    if (psys->part->phasefac != 0) {
      float q_phase[4];
      float phasefac = psys->part->phasefac;
      if (psys->part->randphasefac != 0.0f) {
        phasefac += psys->part->randphasefac * psys_frand(psys, (pa - psys->particles) + 20);
      }
      axis_angle_to_quat(q_phase, vec, phasefac * (float)M_PI);

      mul_qt_v3(q_phase, side);
    }

    cross_v3_v3v3(nor, vec, side);

    unit_m4(mat);
    copy_v3_v3(mat[0], vec);
    copy_v3_v3(mat[1], side);
    copy_v3_v3(mat[2], nor);
  }
  else {
    quat_to_mat4(mat, pa->state.rot);
  }

  *scale = len;
}

void psys_apply_hair_lattice(Depsgraph *depsgraph, Scene *scene, Object *ob, ParticleSystem *psys)
{
  ParticleSimulationData sim = {0};
  sim.depsgraph = depsgraph;
  sim.scene = scene;
  sim.ob = ob;
  sim.psys = psys;
  sim.psmd = psys_get_modifier(ob, psys);

  psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

  if (psys->lattice_deform_data) {
    ParticleData *pa = psys->particles;
    HairKey *hkey;
    int p, h;
    float hairmat[4][4], imat[4][4];

    Mesh *mesh_final = BKE_particle_modifier_mesh_final_get(sim.psmd);

    for (p = 0; p < psys->totpart; p++, pa++) {
      psys_mat_hair_to_global(sim.ob, mesh_final, psys->part->from, pa, hairmat);
      invert_m4_m4(imat, hairmat);

      hkey = pa->hair;
      for (h = 0; h < pa->totkey; h++, hkey++) {
        mul_m4_v3(hairmat, hkey->co);
        calc_latt_deform(psys->lattice_deform_data, hkey->co, psys->lattice_strength);
        mul_m4_v3(imat, hkey->co);
      }
    }

    end_latt_deform(psys->lattice_deform_data);
    psys->lattice_deform_data = NULL;

    /* protect the applied shape */
    psys->flag |= PSYS_EDITED;
  }
}

/* Draw Engine */
void (*BKE_particle_batch_cache_dirty_tag_cb)(ParticleSystem *psys, int mode) = NULL;
void (*BKE_particle_batch_cache_free_cb)(ParticleSystem *psys) = NULL;

void BKE_particle_batch_cache_dirty_tag(ParticleSystem *psys, int mode)
{
  if (psys->batch_cache) {
    BKE_particle_batch_cache_dirty_tag_cb(psys, mode);
  }
}
void BKE_particle_batch_cache_free(ParticleSystem *psys)
{
  if (psys->batch_cache) {
    BKE_particle_batch_cache_free_cb(psys);
  }
}

/* **** Particle system modifier helpers.  **** */

Mesh *BKE_particle_modifier_mesh_final_get(ParticleSystemModifierData *psmd)
{
  if (psmd == NULL) {
    return NULL;
  }
  ParticleSystemModifierDataRuntime *runtime = psmd->modifier.runtime;
  if (runtime == NULL) {
    return NULL;
  }
  return runtime->mesh_final;
}

Mesh *BKE_particle_modifier_mesh_original_get(ParticleSystemModifierData *psmd)
{
  if (psmd == NULL) {
    return NULL;
  }
  ParticleSystemModifierDataRuntime *runtime = psmd->modifier.runtime;
  if (runtime == NULL) {
    return NULL;
  }
  return runtime->mesh_original;
}

ParticleSystemModifierDataRuntime *BKE_particle_modifier_runtime_ensure(
    ParticleSystemModifierData *psmd)
{
  BLI_assert(psmd != NULL);
  if (psmd->modifier.runtime == NULL) {
    psmd->modifier.runtime = MEM_callocN(sizeof(ParticleSystemModifierDataRuntime),
                                         "psmd runtime");
  }
  return psmd->modifier.runtime;
}
