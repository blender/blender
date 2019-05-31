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
 * \ingroup render
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_noise.h"
#include "BLI_kdopbvh.h"
#include "BLI_utildefines.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_texture_types.h"

#include "BKE_colorband.h"
#include "BKE_deform.h"
#include "BKE_lattice.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_colortools.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "render_types.h"
#include "texture.h"

#include "RE_render_ext.h"
#include "RE_shader_ext.h"

static ThreadMutex sample_mutex = PTHREAD_MUTEX_INITIALIZER;

static int point_data_used(PointDensity *pd)
{
  int pd_bitflag = 0;

  if (pd->source == TEX_PD_PSYS) {
    if ((pd->falloff_type == TEX_PD_FALLOFF_PARTICLE_VEL) ||
        (pd->color_source == TEX_PD_COLOR_PARTVEL) ||
        (pd->color_source == TEX_PD_COLOR_PARTSPEED)) {
      pd_bitflag |= POINT_DATA_VEL;
    }
    if ((pd->color_source == TEX_PD_COLOR_PARTAGE) ||
        (pd->falloff_type == TEX_PD_FALLOFF_PARTICLE_AGE)) {
      pd_bitflag |= POINT_DATA_LIFE;
    }
  }
  else if (pd->source == TEX_PD_OBJECT) {
    if (ELEM(pd->ob_color_source,
             TEX_PD_COLOR_VERTCOL,
             TEX_PD_COLOR_VERTWEIGHT,
             TEX_PD_COLOR_VERTNOR)) {
      pd_bitflag |= POINT_DATA_COLOR;
    }
  }

  return pd_bitflag;
}

static void point_data_pointers(PointDensity *pd,
                                float **r_data_velocity,
                                float **r_data_life,
                                float **r_data_color)
{
  const int data_used = point_data_used(pd);
  const int totpoint = pd->totpoints;
  float *data = pd->point_data;
  int offset = 0;

  if (data_used & POINT_DATA_VEL) {
    if (r_data_velocity) {
      *r_data_velocity = data + offset;
    }
    offset += 3 * totpoint;
  }
  else {
    if (r_data_velocity) {
      *r_data_velocity = NULL;
    }
  }

  if (data_used & POINT_DATA_LIFE) {
    if (r_data_life) {
      *r_data_life = data + offset;
    }
    offset += totpoint;
  }
  else {
    if (r_data_life) {
      *r_data_life = NULL;
    }
  }

  if (data_used & POINT_DATA_COLOR) {
    if (r_data_color) {
      *r_data_color = data + offset;
    }
    offset += 3 * totpoint;
  }
  else {
    if (r_data_color) {
      *r_data_color = NULL;
    }
  }
}

/* additional data stored alongside the point density BVH,
 * accessible by point index number to retrieve other information
 * such as particle velocity or lifetime */
static void alloc_point_data(PointDensity *pd)
{
  const int totpoints = pd->totpoints;
  int data_used = point_data_used(pd);
  int data_size = 0;

  if (data_used & POINT_DATA_VEL) {
    /* store 3 channels of velocity data */
    data_size += 3;
  }
  if (data_used & POINT_DATA_LIFE) {
    /* store 1 channel of lifetime data */
    data_size += 1;
  }
  if (data_used & POINT_DATA_COLOR) {
    /* store 3 channels of RGB data */
    data_size += 3;
  }

  if (data_size) {
    pd->point_data = MEM_callocN(sizeof(float) * data_size * totpoints, "particle point data");
  }
}

static void pointdensity_cache_psys(
    Depsgraph *depsgraph, Scene *scene, PointDensity *pd, Object *ob, ParticleSystem *psys)
{
  ParticleKey state;
  ParticleCacheKey *cache;
  ParticleSimulationData sim = {NULL};
  ParticleData *pa = NULL;
  float cfra = BKE_scene_frame_get(scene);
  int i /*, childexists*/ /* UNUSED */;
  int total_particles;
  int data_used;
  float *data_vel, *data_life;
  float partco[3];
  const bool use_render_params = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);

  data_used = point_data_used(pd);

  if (!psys_check_enabled(ob, psys, use_render_params)) {
    return;
  }

  sim.depsgraph = depsgraph;
  sim.scene = scene;
  sim.ob = ob;
  sim.psys = psys;
  sim.psmd = psys_get_modifier(ob, psys);

  /* in case ob->imat isn't up-to-date */
  invert_m4_m4(ob->imat, ob->obmat);

  total_particles = psys->totpart + psys->totchild;
  psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

  pd->point_tree = BLI_bvhtree_new(total_particles, 0.0, 4, 6);
  pd->totpoints = total_particles;
  alloc_point_data(pd);
  point_data_pointers(pd, &data_vel, &data_life, NULL);

#if 0 /* UNUSED */
  if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT)) {
    childexists = 1;
  }
#endif

  for (i = 0, pa = psys->particles; i < total_particles; i++, pa++) {

    if (psys->part->type == PART_HAIR) {
      /* hair particles */
      if (i < psys->totpart && psys->pathcache) {
        cache = psys->pathcache[i];
      }
      else if (i >= psys->totpart && psys->childcache) {
        cache = psys->childcache[i - psys->totpart];
      }
      else {
        continue;
      }

      cache += cache->segments; /* use endpoint */

      copy_v3_v3(state.co, cache->co);
      zero_v3(state.vel);
      state.time = 0.0f;
    }
    else {
      /* emitter particles */
      state.time = cfra;

      if (!psys_get_particle_state(&sim, i, &state, 0)) {
        continue;
      }

      if (data_used & POINT_DATA_LIFE) {
        if (i < psys->totpart) {
          state.time = (cfra - pa->time) / pa->lifetime;
        }
        else {
          ChildParticle *cpa = (psys->child + i) - psys->totpart;
          float pa_birthtime, pa_dietime;

          state.time = psys_get_child_time(psys, cpa, cfra, &pa_birthtime, &pa_dietime);
        }
      }
    }

    copy_v3_v3(partco, state.co);

    if (pd->psys_cache_space == TEX_PD_OBJECTSPACE) {
      mul_m4_v3(ob->imat, partco);
    }
    else if (pd->psys_cache_space == TEX_PD_OBJECTLOC) {
      sub_v3_v3(partco, ob->loc);
    }
    else {
      /* TEX_PD_WORLDSPACE */
    }

    BLI_bvhtree_insert(pd->point_tree, i, partco, 1);

    if (data_vel) {
      data_vel[i * 3 + 0] = state.vel[0];
      data_vel[i * 3 + 1] = state.vel[1];
      data_vel[i * 3 + 2] = state.vel[2];
    }
    if (data_life) {
      data_life[i] = state.time;
    }
  }

  BLI_bvhtree_balance(pd->point_tree);

  if (psys->lattice_deform_data) {
    end_latt_deform(psys->lattice_deform_data);
    psys->lattice_deform_data = NULL;
  }
}

static void pointdensity_cache_vertex_color(PointDensity *pd,
                                            Object *UNUSED(ob),
                                            Mesh *mesh,
                                            float *data_color)
{
  const MLoop *mloop = mesh->mloop;
  const int totloop = mesh->totloop;
  const MLoopCol *mcol;
  char layername[MAX_CUSTOMDATA_LAYER_NAME];
  int i;

  BLI_assert(data_color);

  if (!CustomData_has_layer(&mesh->ldata, CD_MLOOPCOL)) {
    return;
  }
  CustomData_validate_layer_name(&mesh->ldata, CD_MLOOPCOL, pd->vertex_attribute_name, layername);
  mcol = CustomData_get_layer_named(&mesh->ldata, CD_MLOOPCOL, layername);
  if (!mcol) {
    return;
  }

  /* Stores the number of MLoops using the same vertex, so we can normalize colors. */
  int *mcorners = MEM_callocN(sizeof(int) * pd->totpoints, "point density corner count");

  for (i = 0; i < totloop; i++) {
    int v = mloop[i].v;

    if (mcorners[v] == 0) {
      rgb_uchar_to_float(&data_color[v * 3], &mcol[i].r);
    }
    else {
      float col[3];
      rgb_uchar_to_float(col, &mcol[i].r);
      add_v3_v3(&data_color[v * 3], col);
    }

    ++mcorners[v];
  }

  /* Normalize colors by averaging over mcorners.
   * All the corners share the same vertex, ie. occupy the same point in space.
   */
  for (i = 0; i < pd->totpoints; i++) {
    if (mcorners[i] > 0) {
      mul_v3_fl(&data_color[i * 3], 1.0f / mcorners[i]);
    }
  }

  MEM_freeN(mcorners);
}

static void pointdensity_cache_vertex_weight(PointDensity *pd,
                                             Object *ob,
                                             Mesh *mesh,
                                             float *data_color)
{
  const int totvert = mesh->totvert;
  const MDeformVert *mdef, *dv;
  int mdef_index;
  int i;

  BLI_assert(data_color);

  mdef = CustomData_get_layer(&mesh->vdata, CD_MDEFORMVERT);
  if (!mdef) {
    return;
  }
  mdef_index = defgroup_name_index(ob, pd->vertex_attribute_name);
  if (mdef_index < 0) {
    mdef_index = ob->actdef - 1;
  }
  if (mdef_index < 0) {
    return;
  }

  for (i = 0, dv = mdef; i < totvert; ++i, ++dv, data_color += 3) {
    MDeformWeight *dw;
    int j;

    for (j = 0, dw = dv->dw; j < dv->totweight; ++j, ++dw) {
      if (dw->def_nr == mdef_index) {
        copy_v3_fl(data_color, dw->weight);
        break;
      }
    }
  }
}

static void pointdensity_cache_vertex_normal(PointDensity *pd,
                                             Object *UNUSED(ob),
                                             Mesh *mesh,
                                             float *data_color)
{
  MVert *mvert = mesh->mvert, *mv;
  int i;

  BLI_assert(data_color);

  for (i = 0, mv = mvert; i < pd->totpoints; i++, mv++, data_color += 3) {
    normal_short_to_float_v3(data_color, mv->no);
  }
}

static void pointdensity_cache_object(PointDensity *pd, Object *ob)
{
  float *data_color;
  int i;
  MVert *mvert = NULL, *mv;
  Mesh *mesh = ob->data;

#if 0 /* UNUSED */
  CustomData_MeshMasks mask = CD_MASK_BAREMESH;
  mask.fmask |= CD_MASK_MTFACE | CD_MASK_MCOL;
  switch (pd->ob_color_source) {
    case TEX_PD_COLOR_VERTCOL:
      mask.lmask |= CD_MASK_MLOOPCOL;
      break;
    case TEX_PD_COLOR_VERTWEIGHT:
      mask.vmask |= CD_MASK_MDEFORMVERT;
      break;
  }
#endif

  mvert = mesh->mvert; /* local object space */
  pd->totpoints = mesh->totvert;
  if (pd->totpoints == 0) {
    return;
  }

  pd->point_tree = BLI_bvhtree_new(pd->totpoints, 0.0, 4, 6);
  alloc_point_data(pd);
  point_data_pointers(pd, NULL, NULL, &data_color);

  for (i = 0, mv = mvert; i < pd->totpoints; i++, mv++) {
    float co[3];

    copy_v3_v3(co, mv->co);

    switch (pd->ob_cache_space) {
      case TEX_PD_OBJECTSPACE:
        break;
      case TEX_PD_OBJECTLOC:
        mul_m4_v3(ob->obmat, co);
        sub_v3_v3(co, ob->loc);
        break;
      case TEX_PD_WORLDSPACE:
      default:
        mul_m4_v3(ob->obmat, co);
        break;
    }

    BLI_bvhtree_insert(pd->point_tree, i, co, 1);
  }

  switch (pd->ob_color_source) {
    case TEX_PD_COLOR_VERTCOL:
      pointdensity_cache_vertex_color(pd, ob, mesh, data_color);
      break;
    case TEX_PD_COLOR_VERTWEIGHT:
      pointdensity_cache_vertex_weight(pd, ob, mesh, data_color);
      break;
    case TEX_PD_COLOR_VERTNOR:
      pointdensity_cache_vertex_normal(pd, ob, mesh, data_color);
      break;
  }

  BLI_bvhtree_balance(pd->point_tree);
}

static void cache_pointdensity(Depsgraph *depsgraph, Scene *scene, PointDensity *pd)
{
  if (pd == NULL) {
    return;
  }

  if (pd->point_tree) {
    BLI_bvhtree_free(pd->point_tree);
    pd->point_tree = NULL;
  }

  if (pd->source == TEX_PD_PSYS) {
    Object *ob = pd->object;
    ParticleSystem *psys;

    if (!ob || !pd->psys) {
      return;
    }

    psys = BLI_findlink(&ob->particlesystem, pd->psys - 1);
    if (!psys) {
      return;
    }

    pointdensity_cache_psys(depsgraph, scene, pd, ob, psys);
  }
  else if (pd->source == TEX_PD_OBJECT) {
    Object *ob = pd->object;
    if (ob && ob->type == OB_MESH) {
      pointdensity_cache_object(pd, ob);
    }
  }
}

static void free_pointdensity(PointDensity *pd)
{
  if (pd == NULL) {
    return;
  }

  if (pd->point_tree) {
    BLI_bvhtree_free(pd->point_tree);
    pd->point_tree = NULL;
  }

  if (pd->point_data) {
    MEM_freeN(pd->point_data);
    pd->point_data = NULL;
  }
  pd->totpoints = 0;
}

typedef struct PointDensityRangeData {
  float *density;
  float squared_radius;
  float *point_data_life;
  float *point_data_velocity;
  float *point_data_color;
  float *vec;
  float *col;
  float softness;
  short falloff_type;
  short noise_influence;
  float *age;
  struct CurveMapping *density_curve;
  float velscale;
} PointDensityRangeData;

static float density_falloff(PointDensityRangeData *pdr, int index, float squared_dist)
{
  const float dist = (pdr->squared_radius - squared_dist) / pdr->squared_radius * 0.5f;
  float density = 0.0f;

  switch (pdr->falloff_type) {
    case TEX_PD_FALLOFF_STD:
      density = dist;
      break;
    case TEX_PD_FALLOFF_SMOOTH:
      density = 3.0f * dist * dist - 2.0f * dist * dist * dist;
      break;
    case TEX_PD_FALLOFF_SOFT:
      density = pow(dist, pdr->softness);
      break;
    case TEX_PD_FALLOFF_CONSTANT:
      density = pdr->squared_radius;
      break;
    case TEX_PD_FALLOFF_ROOT:
      density = sqrtf(dist);
      break;
    case TEX_PD_FALLOFF_PARTICLE_AGE:
      if (pdr->point_data_life) {
        density = dist * MIN2(pdr->point_data_life[index], 1.0f);
      }
      else {
        density = dist;
      }
      break;
    case TEX_PD_FALLOFF_PARTICLE_VEL:
      if (pdr->point_data_velocity) {
        density = dist * len_v3(&pdr->point_data_velocity[index * 3]) * pdr->velscale;
      }
      else {
        density = dist;
      }
      break;
  }

  if (pdr->density_curve && dist != 0.0f) {
    curvemapping_initialize(pdr->density_curve);
    density = curvemapping_evaluateF(pdr->density_curve, 0, density / dist) * dist;
  }

  return density;
}

static void accum_density(void *userdata, int index, const float co[3], float squared_dist)
{
  PointDensityRangeData *pdr = (PointDensityRangeData *)userdata;
  float density = 0.0f;

  UNUSED_VARS(co);

  if (pdr->point_data_velocity) {
    pdr->vec[0] += pdr->point_data_velocity[index * 3 + 0];  // * density;
    pdr->vec[1] += pdr->point_data_velocity[index * 3 + 1];  // * density;
    pdr->vec[2] += pdr->point_data_velocity[index * 3 + 2];  // * density;
  }
  if (pdr->point_data_life) {
    *pdr->age += pdr->point_data_life[index];  // * density;
  }
  if (pdr->point_data_color) {
    add_v3_v3(pdr->col, &pdr->point_data_color[index * 3]);  // * density;
  }

  density = density_falloff(pdr, index, squared_dist);

  *pdr->density += density;
}

static void init_pointdensityrangedata(PointDensity *pd,
                                       PointDensityRangeData *pdr,
                                       float *density,
                                       float *vec,
                                       float *age,
                                       float *col,
                                       struct CurveMapping *density_curve,
                                       float velscale)
{
  pdr->squared_radius = pd->radius * pd->radius;
  pdr->density = density;
  pdr->falloff_type = pd->falloff_type;
  pdr->vec = vec;
  pdr->age = age;
  pdr->col = col;
  pdr->softness = pd->falloff_softness;
  pdr->noise_influence = pd->noise_influence;
  point_data_pointers(
      pd, &pdr->point_data_velocity, &pdr->point_data_life, &pdr->point_data_color);
  pdr->density_curve = density_curve;
  pdr->velscale = velscale;
}

static int pointdensity(PointDensity *pd,
                        const float texvec[3],
                        TexResult *texres,
                        float r_vec[3],
                        float *r_age,
                        float r_col[3])
{
  int retval = TEX_INT;
  PointDensityRangeData pdr;
  float density = 0.0f, age = 0.0f;
  float vec[3] = {0.0f, 0.0f, 0.0f}, col[3] = {0.0f, 0.0f, 0.0f}, co[3];
  float turb, noise_fac;
  int num = 0;

  texres->tin = 0.0f;

  init_pointdensityrangedata(pd,
                             &pdr,
                             &density,
                             vec,
                             &age,
                             col,
                             (pd->flag & TEX_PD_FALLOFF_CURVE ? pd->falloff_curve : NULL),
                             pd->falloff_speed_scale * 0.001f);
  noise_fac = pd->noise_fac * 0.5f; /* better default */

  copy_v3_v3(co, texvec);

  if (point_data_used(pd)) {
    /* does a BVH lookup to find accumulated density and additional point data *
     * stores particle velocity vector in 'vec', and particle lifetime in 'time' */
    num = BLI_bvhtree_range_query(pd->point_tree, co, pd->radius, accum_density, &pdr);
    if (num > 0) {
      age /= num;
      mul_v3_fl(vec, 1.0f / num);
      mul_v3_fl(col, 1.0f / num);
    }

    /* reset */
    density = 0.0f;
    zero_v3(vec);
    zero_v3(col);
  }

  if (pd->flag & TEX_PD_TURBULENCE) {
    turb = BLI_gTurbulence(pd->noise_size,
                           texvec[0] + vec[0],
                           texvec[1] + vec[1],
                           texvec[2] + vec[2],
                           pd->noise_depth,
                           0,
                           pd->noise_basis);

    turb -= 0.5f; /* re-center 0.0-1.0 range around 0 to prevent offsetting result */

    /* now we have an offset coordinate to use for the density lookup */
    co[0] = texvec[0] + noise_fac * turb;
    co[1] = texvec[1] + noise_fac * turb;
    co[2] = texvec[2] + noise_fac * turb;
  }

  /* BVH query with the potentially perturbed coordinates */
  num = BLI_bvhtree_range_query(pd->point_tree, co, pd->radius, accum_density, &pdr);
  if (num > 0) {
    age /= num;
    mul_v3_fl(vec, 1.0f / num);
    mul_v3_fl(col, 1.0f / num);
  }

  texres->tin = density;
  if (r_age != NULL) {
    *r_age = age;
  }
  if (r_vec != NULL) {
    copy_v3_v3(r_vec, vec);
  }
  if (r_col != NULL) {
    copy_v3_v3(r_col, col);
  }

  return retval;
}

static void pointdensity_color(
    PointDensity *pd, TexResult *texres, float age, const float vec[3], const float col[3])
{
  texres->tr = texres->tg = texres->tb = texres->ta = 1.0f;

  if (pd->source == TEX_PD_PSYS) {
    float rgba[4];

    switch (pd->color_source) {
      case TEX_PD_COLOR_PARTAGE:
        if (pd->coba) {
          if (BKE_colorband_evaluate(pd->coba, age, rgba)) {
            texres->talpha = true;
            copy_v3_v3(&texres->tr, rgba);
            texres->tin *= rgba[3];
            texres->ta = texres->tin;
          }
        }
        break;
      case TEX_PD_COLOR_PARTSPEED: {
        float speed = len_v3(vec) * pd->speed_scale;

        if (pd->coba) {
          if (BKE_colorband_evaluate(pd->coba, speed, rgba)) {
            texres->talpha = true;
            copy_v3_v3(&texres->tr, rgba);
            texres->tin *= rgba[3];
            texres->ta = texres->tin;
          }
        }
        break;
      }
      case TEX_PD_COLOR_PARTVEL:
        texres->talpha = true;
        mul_v3_v3fl(&texres->tr, vec, pd->speed_scale);
        texres->ta = texres->tin;
        break;
      case TEX_PD_COLOR_CONSTANT:
      default:
        break;
    }
  }
  else {
    float rgba[4];

    switch (pd->ob_color_source) {
      case TEX_PD_COLOR_VERTCOL:
        texres->talpha = true;
        copy_v3_v3(&texres->tr, col);
        texres->ta = texres->tin;
        break;
      case TEX_PD_COLOR_VERTWEIGHT:
        texres->talpha = true;
        if (pd->coba && BKE_colorband_evaluate(pd->coba, col[0], rgba)) {
          copy_v3_v3(&texres->tr, rgba);
          texres->tin *= rgba[3];
        }
        else {
          copy_v3_v3(&texres->tr, col);
        }
        texres->ta = texres->tin;
        break;
      case TEX_PD_COLOR_VERTNOR:
        texres->talpha = true;
        copy_v3_v3(&texres->tr, col);
        texres->ta = texres->tin;
        break;
      case TEX_PD_COLOR_CONSTANT:
      default:
        break;
    }
  }
}

static void sample_dummy_point_density(int resolution, float *values)
{
  memset(values, 0, sizeof(float) * 4 * resolution * resolution * resolution);
}

static void particle_system_minmax(Depsgraph *depsgraph,
                                   Scene *scene,
                                   Object *object,
                                   ParticleSystem *psys,
                                   float radius,
                                   float min[3],
                                   float max[3])
{
  const float size[3] = {radius, radius, radius};
  const float cfra = BKE_scene_frame_get(scene);
  ParticleSettings *part = psys->part;
  ParticleSimulationData sim = {NULL};
  ParticleData *pa = NULL;
  int i;
  int total_particles;
  float mat[4][4], imat[4][4];

  INIT_MINMAX(min, max);
  if (part->type == PART_HAIR) {
    /* TOOD(sergey): Not supported currently. */
    return;
  }

  unit_m4(mat);

  sim.depsgraph = depsgraph;
  sim.scene = scene;
  sim.ob = object;
  sim.psys = psys;
  sim.psmd = psys_get_modifier(object, psys);

  invert_m4_m4(imat, object->obmat);
  total_particles = psys->totpart + psys->totchild;
  psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

  for (i = 0, pa = psys->particles; i < total_particles; i++, pa++) {
    float co_object[3], co_min[3], co_max[3];
    ParticleKey state;
    state.time = cfra;
    if (!psys_get_particle_state(&sim, i, &state, 0)) {
      continue;
    }
    mul_v3_m4v3(co_object, imat, state.co);
    sub_v3_v3v3(co_min, co_object, size);
    add_v3_v3v3(co_max, co_object, size);
    minmax_v3v3_v3(min, max, co_min);
    minmax_v3v3_v3(min, max, co_max);
  }

  if (psys->lattice_deform_data) {
    end_latt_deform(psys->lattice_deform_data);
    psys->lattice_deform_data = NULL;
  }
}

void RE_point_density_cache(struct Depsgraph *depsgraph, PointDensity *pd)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);

  /* Same matricies/resolution as dupli_render_particle_set(). */
  BLI_mutex_lock(&sample_mutex);
  cache_pointdensity(depsgraph, scene, pd);
  BLI_mutex_unlock(&sample_mutex);
}

void RE_point_density_minmax(struct Depsgraph *depsgraph,
                             struct PointDensity *pd,
                             float r_min[3],
                             float r_max[3])
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  Object *object = pd->object;
  if (object == NULL) {
    zero_v3(r_min);
    zero_v3(r_max);
    return;
  }
  if (pd->source == TEX_PD_PSYS) {
    ParticleSystem *psys;

    if (pd->psys == 0) {
      zero_v3(r_min);
      zero_v3(r_max);
      return;
    }
    psys = BLI_findlink(&object->particlesystem, pd->psys - 1);
    if (psys == NULL) {
      zero_v3(r_min);
      zero_v3(r_max);
      return;
    }

    particle_system_minmax(depsgraph, scene, object, psys, pd->radius, r_min, r_max);
  }
  else {
    float radius[3] = {pd->radius, pd->radius, pd->radius};
    BoundBox *bb = BKE_object_boundbox_get(object);

    if (bb != NULL) {
      BLI_assert((bb->flag & BOUNDBOX_DIRTY) == 0);
      copy_v3_v3(r_min, bb->vec[0]);
      copy_v3_v3(r_max, bb->vec[6]);
      /* Adjust texture space to include density points on the boundaries. */
      sub_v3_v3(r_min, radius);
      add_v3_v3(r_max, radius);
    }
    else {
      zero_v3(r_min);
      zero_v3(r_max);
    }
  }
}

typedef struct SampleCallbackData {
  PointDensity *pd;
  int resolution;
  float *min, *dim;
  float *values;
} SampleCallbackData;

static void point_density_sample_func(void *__restrict data_v,
                                      const int iter,
                                      const ParallelRangeTLS *__restrict UNUSED(tls))
{
  SampleCallbackData *data = (SampleCallbackData *)data_v;

  const int resolution = data->resolution;
  const int resolution2 = resolution * resolution;
  const float *min = data->min, *dim = data->dim;
  PointDensity *pd = data->pd;
  float *values = data->values;

  if (!pd || !pd->point_tree) {
    return;
  }

  size_t z = (size_t)iter;
  for (size_t y = 0; y < resolution; ++y) {
    for (size_t x = 0; x < resolution; ++x) {
      size_t index = z * resolution2 + y * resolution + x;
      float texvec[3];
      float age, vec[3], col[3];
      TexResult texres;

      copy_v3_v3(texvec, min);
      texvec[0] += dim[0] * (float)x / (float)resolution;
      texvec[1] += dim[1] * (float)y / (float)resolution;
      texvec[2] += dim[2] * (float)z / (float)resolution;

      pointdensity(pd, texvec, &texres, vec, &age, col);
      pointdensity_color(pd, &texres, age, vec, col);

      copy_v3_v3(&values[index * 4 + 0], &texres.tr);
      values[index * 4 + 3] = texres.tin;
    }
  }
}

/* NOTE 1: Requires RE_point_density_cache() to be called first.
 * NOTE 2: Frees point density structure after sampling.
 */
void RE_point_density_sample(Depsgraph *depsgraph,
                             PointDensity *pd,
                             const int resolution,
                             float *values)
{
  Object *object = pd->object;
  float min[3], max[3], dim[3];

  /* TODO(sergey): Implement some sort of assert() that point density
   * was cached already.
   */

  if (object == NULL) {
    sample_dummy_point_density(resolution, values);
    return;
  }

  BLI_mutex_lock(&sample_mutex);
  RE_point_density_minmax(depsgraph, pd, min, max);
  BLI_mutex_unlock(&sample_mutex);
  sub_v3_v3v3(dim, max, min);
  if (dim[0] <= 0.0f || dim[1] <= 0.0f || dim[2] <= 0.0f) {
    sample_dummy_point_density(resolution, values);
    return;
  }

  SampleCallbackData data;
  data.pd = pd;
  data.resolution = resolution;
  data.min = min;
  data.dim = dim;
  data.values = values;
  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (resolution > 32);
  BLI_task_parallel_range(0, resolution, &data, point_density_sample_func, &settings);

  free_pointdensity(pd);
}

void RE_point_density_free(struct PointDensity *pd)
{
  free_pointdensity(pd);
}

void RE_point_density_fix_linking(void)
{
}
