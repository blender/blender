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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"

#include "BLI_fileops.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_fluid_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"

#include "BKE_effect.h"
#include "BKE_fluid.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_modifier.h"
#include "BKE_pointcache.h"

#ifdef WITH_FLUID

#  include <float.h>
#  include <math.h>
#  include <stdio.h>
#  include <string.h> /* memset */

#  include "DNA_customdata_types.h"
#  include "DNA_light_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_particle_types.h"
#  include "DNA_scene_types.h"

#  include "BLI_kdopbvh.h"
#  include "BLI_kdtree.h"
#  include "BLI_threads.h"
#  include "BLI_voxel.h"

#  include "BKE_bvhutils.h"
#  include "BKE_collision.h"
#  include "BKE_colortools.h"
#  include "BKE_customdata.h"
#  include "BKE_deform.h"
#  include "BKE_mesh.h"
#  include "BKE_mesh_runtime.h"
#  include "BKE_object.h"
#  include "BKE_particle.h"
#  include "BKE_scene.h"
#  include "BKE_texture.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_query.h"

#  include "RE_shader_ext.h"

#  include "CLG_log.h"

#  include "manta_fluid_API.h"

#endif /* WITH_FLUID */

/** Time step default value for nice appearance. */
#define DT_DEFAULT 0.1f

/** Max value for phi initialization */
#define PHI_MAX 9999.0f

static void BKE_fluid_modifier_reset_ex(struct FluidModifierData *mmd, bool need_lock);

#ifdef WITH_FLUID
// #define DEBUG_PRINT

static CLG_LogRef LOG = {"bke.fluid"};

/* -------------------------------------------------------------------- */
/** \name Fluid API
 * \{ */

static ThreadMutex object_update_lock = BLI_MUTEX_INITIALIZER;

struct FluidModifierData;
struct Mesh;
struct Object;
struct Scene;

#  define ADD_IF_LOWER_POS(a, b) (min_ff((a) + (b), max_ff((a), (b))))
#  define ADD_IF_LOWER_NEG(a, b) (max_ff((a) + (b), min_ff((a), (b))))
#  define ADD_IF_LOWER(a, b) (((b) > 0) ? ADD_IF_LOWER_POS((a), (b)) : ADD_IF_LOWER_NEG((a), (b)))

bool BKE_fluid_reallocate_fluid(FluidDomainSettings *mds, int res[3], int free_old)
{
  if (free_old && mds->fluid) {
    manta_free(mds->fluid);
  }
  if (!min_iii(res[0], res[1], res[2])) {
    mds->fluid = NULL;
  }
  else {
    mds->fluid = manta_init(res, mds->mmd);

    mds->res_noise[0] = res[0] * mds->noise_scale;
    mds->res_noise[1] = res[1] * mds->noise_scale;
    mds->res_noise[2] = res[2] * mds->noise_scale;
  }

  return (mds->fluid != NULL);
}

void BKE_fluid_reallocate_copy_fluid(FluidDomainSettings *mds,
                                     int o_res[3],
                                     int n_res[3],
                                     int o_min[3],
                                     int n_min[3],
                                     int o_max[3],
                                     int o_shift[3],
                                     int n_shift[3])
{
  int x, y, z;
  struct MANTA *fluid_old = mds->fluid;
  const int block_size = mds->noise_scale;
  int new_shift[3] = {0};
  sub_v3_v3v3_int(new_shift, n_shift, o_shift);

  /* allocate new fluid data */
  BKE_fluid_reallocate_fluid(mds, n_res, 0);

  int o_total_cells = o_res[0] * o_res[1] * o_res[2];
  int n_total_cells = n_res[0] * n_res[1] * n_res[2];

  /* boundary cells will be skipped when copying data */
  int bwidth = mds->boundary_width;

  /* copy values from old fluid to new */
  if (o_total_cells > 1 && n_total_cells > 1) {
    /* base smoke */
    float *o_dens, *o_react, *o_flame, *o_fuel, *o_heat, *o_vx, *o_vy, *o_vz, *o_r, *o_g, *o_b;
    float *n_dens, *n_react, *n_flame, *n_fuel, *n_heat, *n_vx, *n_vy, *n_vz, *n_r, *n_g, *n_b;
    float dummy, *dummy_s;
    int *dummy_p;
    /* noise smoke */
    int wt_res_old[3];
    float *o_wt_dens, *o_wt_react, *o_wt_flame, *o_wt_fuel, *o_wt_tcu, *o_wt_tcv, *o_wt_tcw,
        *o_wt_tcu2, *o_wt_tcv2, *o_wt_tcw2, *o_wt_r, *o_wt_g, *o_wt_b;
    float *n_wt_dens, *n_wt_react, *n_wt_flame, *n_wt_fuel, *n_wt_tcu, *n_wt_tcv, *n_wt_tcw,
        *n_wt_tcu2, *n_wt_tcv2, *n_wt_tcw2, *n_wt_r, *n_wt_g, *n_wt_b;

    if (mds->flags & FLUID_DOMAIN_USE_NOISE) {
      manta_smoke_turbulence_export(fluid_old,
                                    &o_wt_dens,
                                    &o_wt_react,
                                    &o_wt_flame,
                                    &o_wt_fuel,
                                    &o_wt_r,
                                    &o_wt_g,
                                    &o_wt_b,
                                    &o_wt_tcu,
                                    &o_wt_tcv,
                                    &o_wt_tcw,
                                    &o_wt_tcu2,
                                    &o_wt_tcv2,
                                    &o_wt_tcw2);
      manta_smoke_turbulence_get_res(fluid_old, wt_res_old);
      manta_smoke_turbulence_export(mds->fluid,
                                    &n_wt_dens,
                                    &n_wt_react,
                                    &n_wt_flame,
                                    &n_wt_fuel,
                                    &n_wt_r,
                                    &n_wt_g,
                                    &n_wt_b,
                                    &n_wt_tcu,
                                    &n_wt_tcv,
                                    &n_wt_tcw,
                                    &n_wt_tcu2,
                                    &n_wt_tcv2,
                                    &n_wt_tcw2);
    }

    manta_smoke_export(fluid_old,
                       &dummy,
                       &dummy,
                       &o_dens,
                       &o_react,
                       &o_flame,
                       &o_fuel,
                       &o_heat,
                       &o_vx,
                       &o_vy,
                       &o_vz,
                       &o_r,
                       &o_g,
                       &o_b,
                       &dummy_p,
                       &dummy_s);
    manta_smoke_export(mds->fluid,
                       &dummy,
                       &dummy,
                       &n_dens,
                       &n_react,
                       &n_flame,
                       &n_fuel,
                       &n_heat,
                       &n_vx,
                       &n_vy,
                       &n_vz,
                       &n_r,
                       &n_g,
                       &n_b,
                       &dummy_p,
                       &dummy_s);

    for (x = o_min[0]; x < o_max[0]; x++) {
      for (y = o_min[1]; y < o_max[1]; y++) {
        for (z = o_min[2]; z < o_max[2]; z++) {
          /* old grid index */
          int xo = x - o_min[0];
          int yo = y - o_min[1];
          int zo = z - o_min[2];
          int index_old = manta_get_index(xo, o_res[0], yo, o_res[1], zo);
          /* new grid index */
          int xn = x - n_min[0] - new_shift[0];
          int yn = y - n_min[1] - new_shift[1];
          int zn = z - n_min[2] - new_shift[2];
          int index_new = manta_get_index(xn, n_res[0], yn, n_res[1], zn);

          /* skip if outside new domain */
          if (xn < 0 || xn >= n_res[0] || yn < 0 || yn >= n_res[1] || zn < 0 || zn >= n_res[2]) {
            continue;
          }
          /* skip if trying to copy from old boundary cell */
          if (xo < bwidth || yo < bwidth || zo < bwidth || xo >= o_res[0] - bwidth ||
              yo >= o_res[1] - bwidth || zo >= o_res[2] - bwidth) {
            continue;
          }
          /* skip if trying to copy into new boundary cell */
          if (xn < bwidth || yn < bwidth || zn < bwidth || xn >= n_res[0] - bwidth ||
              yn >= n_res[1] - bwidth || zn >= n_res[2] - bwidth) {
            continue;
          }

          /* copy data */
          if (mds->flags & FLUID_DOMAIN_USE_NOISE) {
            int i, j, k;
            /* old grid index */
            int xx_o = xo * block_size;
            int yy_o = yo * block_size;
            int zz_o = zo * block_size;
            /* new grid index */
            int xx_n = xn * block_size;
            int yy_n = yn * block_size;
            int zz_n = zn * block_size;

            /* insert old texture values into new texture grids */
            n_wt_tcu[index_new] = o_wt_tcu[index_old];
            n_wt_tcv[index_new] = o_wt_tcv[index_old];
            n_wt_tcw[index_new] = o_wt_tcw[index_old];

            n_wt_tcu2[index_new] = o_wt_tcu2[index_old];
            n_wt_tcv2[index_new] = o_wt_tcv2[index_old];
            n_wt_tcw2[index_new] = o_wt_tcw2[index_old];

            for (i = 0; i < block_size; i++) {
              for (j = 0; j < block_size; j++) {
                for (k = 0; k < block_size; k++) {
                  int big_index_old = manta_get_index(
                      xx_o + i, wt_res_old[0], yy_o + j, wt_res_old[1], zz_o + k);
                  int big_index_new = manta_get_index(
                      xx_n + i, mds->res_noise[0], yy_n + j, mds->res_noise[1], zz_n + k);
                  /* copy data */
                  n_wt_dens[big_index_new] = o_wt_dens[big_index_old];
                  if (n_wt_flame && o_wt_flame) {
                    n_wt_flame[big_index_new] = o_wt_flame[big_index_old];
                    n_wt_fuel[big_index_new] = o_wt_fuel[big_index_old];
                    n_wt_react[big_index_new] = o_wt_react[big_index_old];
                  }
                  if (n_wt_r && o_wt_r) {
                    n_wt_r[big_index_new] = o_wt_r[big_index_old];
                    n_wt_g[big_index_new] = o_wt_g[big_index_old];
                    n_wt_b[big_index_new] = o_wt_b[big_index_old];
                  }
                }
              }
            }
          }

          n_dens[index_new] = o_dens[index_old];
          /* heat */
          if (n_heat && o_heat) {
            n_heat[index_new] = o_heat[index_old];
          }
          /* fuel */
          if (n_fuel && o_fuel) {
            n_flame[index_new] = o_flame[index_old];
            n_fuel[index_new] = o_fuel[index_old];
            n_react[index_new] = o_react[index_old];
          }
          /* color */
          if (o_r && n_r) {
            n_r[index_new] = o_r[index_old];
            n_g[index_new] = o_g[index_old];
            n_b[index_new] = o_b[index_old];
          }
          n_vx[index_new] = o_vx[index_old];
          n_vy[index_new] = o_vy[index_old];
          n_vz[index_new] = o_vz[index_old];
        }
      }
    }
  }
  manta_free(fluid_old);
}

void BKE_fluid_cache_free_all(FluidDomainSettings *mds, Object *ob)
{
  int cache_map = (FLUID_DOMAIN_OUTDATED_DATA | FLUID_DOMAIN_OUTDATED_NOISE |
                   FLUID_DOMAIN_OUTDATED_MESH | FLUID_DOMAIN_OUTDATED_PARTICLES |
                   FLUID_DOMAIN_OUTDATED_GUIDE);
  BKE_fluid_cache_free(mds, ob, cache_map);
}

void BKE_fluid_cache_free(FluidDomainSettings *mds, Object *ob, int cache_map)
{
  char temp_dir[FILE_MAX];
  int flags = mds->cache_flag;
  const char *relbase = BKE_modifier_path_relbase_from_global(ob);

  if (cache_map & FLUID_DOMAIN_OUTDATED_DATA) {
    flags &= ~(FLUID_DOMAIN_BAKING_DATA | FLUID_DOMAIN_BAKED_DATA | FLUID_DOMAIN_OUTDATED_DATA);
    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_CONFIG, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_delete(temp_dir, true, true); /* BLI_exists(filepath) is implicit */

    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_DATA, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_delete(temp_dir, true, true); /* BLI_exists(filepath) is implicit */

    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_SCRIPT, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_delete(temp_dir, true, true); /* BLI_exists(filepath) is implicit */

    mds->cache_frame_pause_data = 0;
  }
  if (cache_map & FLUID_DOMAIN_OUTDATED_NOISE) {
    flags &= ~(FLUID_DOMAIN_BAKING_NOISE | FLUID_DOMAIN_BAKED_NOISE | FLUID_DOMAIN_OUTDATED_NOISE);
    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_NOISE, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_delete(temp_dir, true, true); /* BLI_exists(filepath) is implicit */

    mds->cache_frame_pause_noise = 0;
  }
  if (cache_map & FLUID_DOMAIN_OUTDATED_MESH) {
    flags &= ~(FLUID_DOMAIN_BAKING_MESH | FLUID_DOMAIN_BAKED_MESH | FLUID_DOMAIN_OUTDATED_MESH);
    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_MESH, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_delete(temp_dir, true, true); /* BLI_exists(filepath) is implicit */

    mds->cache_frame_pause_mesh = 0;
  }
  if (cache_map & FLUID_DOMAIN_OUTDATED_PARTICLES) {
    flags &= ~(FLUID_DOMAIN_BAKING_PARTICLES | FLUID_DOMAIN_BAKED_PARTICLES |
               FLUID_DOMAIN_OUTDATED_PARTICLES);
    BLI_path_join(
        temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_PARTICLES, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_delete(temp_dir, true, true); /* BLI_exists(filepath) is implicit */

    mds->cache_frame_pause_particles = 0;
  }

  if (cache_map & FLUID_DOMAIN_OUTDATED_GUIDE) {
    flags &= ~(FLUID_DOMAIN_BAKING_GUIDE | FLUID_DOMAIN_BAKED_GUIDE | FLUID_DOMAIN_OUTDATED_GUIDE);
    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_GUIDE, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_delete(temp_dir, true, true); /* BLI_exists(filepath) is implicit */

    mds->cache_frame_pause_guide = 0;
  }
  mds->cache_flag = flags;
}

/* convert global position to domain cell space */
static void manta_pos_to_cell(FluidDomainSettings *mds, float pos[3])
{
  mul_m4_v3(mds->imat, pos);
  sub_v3_v3(pos, mds->p0);
  pos[0] *= 1.0f / mds->cell_size[0];
  pos[1] *= 1.0f / mds->cell_size[1];
  pos[2] *= 1.0f / mds->cell_size[2];
}

/* Set domain transformations and base resolution from object mesh. */
static void manta_set_domain_from_mesh(FluidDomainSettings *mds,
                                       Object *ob,
                                       Mesh *me,
                                       bool init_resolution)
{
  size_t i;
  float min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  float size[3];
  MVert *verts = me->mvert;
  float scale = 0.0;
  int res;

  res = mds->maxres;

  /* Set minimum and maximum coordinates of BB. */
  for (i = 0; i < me->totvert; i++) {
    minmax_v3v3_v3(min, max, verts[i].co);
  }

  /* Set domain bounds. */
  copy_v3_v3(mds->p0, min);
  copy_v3_v3(mds->p1, max);
  mds->dx = 1.0f / res;

  /* Calculate domain dimensions. */
  sub_v3_v3v3(size, max, min);
  if (init_resolution) {
    zero_v3_int(mds->base_res);
    copy_v3_v3(mds->cell_size, size);
  }
  /* Apply object scale. */
  for (i = 0; i < 3; i++) {
    size[i] = fabsf(size[i] * ob->scale[i]);
  }
  copy_v3_v3(mds->global_size, size);
  copy_v3_v3(mds->dp0, min);

  invert_m4_m4(mds->imat, ob->obmat);

  /* Prevent crash when initializing a plane as domain. */
  if (!init_resolution || (size[0] < FLT_EPSILON) || (size[1] < FLT_EPSILON) ||
      (size[2] < FLT_EPSILON)) {
    return;
  }

  /* Define grid resolutions from longest domain side. */
  if (size[0] >= MAX2(size[1], size[2])) {
    scale = res / size[0];
    mds->scale = size[0] / fabsf(ob->scale[0]);
    mds->base_res[0] = res;
    mds->base_res[1] = max_ii((int)(size[1] * scale + 0.5f), 4);
    mds->base_res[2] = max_ii((int)(size[2] * scale + 0.5f), 4);
  }
  else if (size[1] >= MAX2(size[0], size[2])) {
    scale = res / size[1];
    mds->scale = size[1] / fabsf(ob->scale[1]);
    mds->base_res[0] = max_ii((int)(size[0] * scale + 0.5f), 4);
    mds->base_res[1] = res;
    mds->base_res[2] = max_ii((int)(size[2] * scale + 0.5f), 4);
  }
  else {
    scale = res / size[2];
    mds->scale = size[2] / fabsf(ob->scale[2]);
    mds->base_res[0] = max_ii((int)(size[0] * scale + 0.5f), 4);
    mds->base_res[1] = max_ii((int)(size[1] * scale + 0.5f), 4);
    mds->base_res[2] = res;
  }

  /* Set cell size. */
  mds->cell_size[0] /= (float)mds->base_res[0];
  mds->cell_size[1] /= (float)mds->base_res[1];
  mds->cell_size[2] /= (float)mds->base_res[2];
}

static bool BKE_fluid_modifier_init(
    FluidModifierData *mmd, Depsgraph *depsgraph, Object *ob, Scene *scene, Mesh *me)
{
  int scene_framenr = (int)DEG_get_ctime(depsgraph);

  if ((mmd->type & MOD_FLUID_TYPE_DOMAIN) && mmd->domain && !mmd->domain->fluid) {
    FluidDomainSettings *mds = mmd->domain;
    int res[3];
    /* Set domain dimensions from mesh. */
    manta_set_domain_from_mesh(mds, ob, me, true);
    /* Set domain gravity, use global gravity if enabled. */
    if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
      copy_v3_v3(mds->gravity, scene->physics_settings.gravity);
    }
    mul_v3_fl(mds->gravity, mds->effector_weights->global_gravity);
    /* Reset domain values. */
    zero_v3_int(mds->shift);
    zero_v3(mds->shift_f);
    add_v3_fl(mds->shift_f, 0.5f);
    zero_v3(mds->prev_loc);
    mul_m4_v3(ob->obmat, mds->prev_loc);
    copy_m4_m4(mds->obmat, ob->obmat);

    /* Set resolutions. */
    if (mmd->domain->type == FLUID_DOMAIN_TYPE_GAS &&
        mmd->domain->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) {
      res[0] = res[1] = res[2] = 1; /* Use minimum res for adaptive init. */
    }
    else {
      copy_v3_v3_int(res, mds->base_res);
    }
    copy_v3_v3_int(mds->res, res);
    mds->total_cells = mds->res[0] * mds->res[1] * mds->res[2];
    mds->res_min[0] = mds->res_min[1] = mds->res_min[2] = 0;
    copy_v3_v3_int(mds->res_max, res);

    /* Set time, frame length = 0.1 is at 25fps. */
    float fps = scene->r.frs_sec / scene->r.frs_sec_base;
    mds->frame_length = DT_DEFAULT * (25.0f / fps) * mds->time_scale;
    /* Initially dt is equal to frame length (dt can change with adaptive-time stepping though). */
    mds->dt = mds->frame_length;
    mds->time_per_frame = 0;

    mmd->time = scene_framenr;

    /* Allocate fluid. */
    return BKE_fluid_reallocate_fluid(mds, mds->res, 0);
  }
  else if (mmd->type & MOD_FLUID_TYPE_FLOW) {
    if (!mmd->flow) {
      BKE_fluid_modifier_create_type_data(mmd);
    }
    mmd->time = scene_framenr;
    return true;
  }
  else if (mmd->type & MOD_FLUID_TYPE_EFFEC) {
    if (!mmd->effector) {
      BKE_fluid_modifier_create_type_data(mmd);
    }
    mmd->time = scene_framenr;
    return true;
  }
  return false;
}

// forward declaration
static void manta_smoke_calc_transparency(FluidDomainSettings *mds, ViewLayer *view_layer);
static float calc_voxel_transp(
    float *result, float *input, int res[3], int *pixel, float *t_ray, float correct);
static void update_distances(int index,
                             float *mesh_distances,
                             BVHTreeFromMesh *tree_data,
                             const float ray_start[3],
                             float surface_thickness,
                             bool use_plane_init);

static int get_light(ViewLayer *view_layer, float *light)
{
  Base *base_tmp = NULL;
  int found_light = 0;

  /* Try to find a lamp, preferably local. */
  for (base_tmp = FIRSTBASE(view_layer); base_tmp; base_tmp = base_tmp->next) {
    if (base_tmp->object->type == OB_LAMP) {
      Light *la = base_tmp->object->data;

      if (la->type == LA_LOCAL) {
        copy_v3_v3(light, base_tmp->object->obmat[3]);
        return 1;
      }
      else if (!found_light) {
        copy_v3_v3(light, base_tmp->object->obmat[3]);
        found_light = 1;
      }
    }
  }

  return found_light;
}

static void clamp_bounds_in_domain(FluidDomainSettings *mds,
                                   int min[3],
                                   int max[3],
                                   float *min_vel,
                                   float *max_vel,
                                   int margin,
                                   float dt)
{
  int i;
  for (i = 0; i < 3; i++) {
    int adapt = (mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) ? mds->adapt_res : 0;
    /* Add some margin. */
    min[i] -= margin;
    max[i] += margin;

    /* Adapt to velocity. */
    if (min_vel && min_vel[i] < 0.0f) {
      min[i] += (int)floor(min_vel[i] * dt);
    }
    if (max_vel && max_vel[i] > 0.0f) {
      max[i] += (int)ceil(max_vel[i] * dt);
    }

    /* Clamp within domain max size. */
    CLAMP(min[i], -adapt, mds->base_res[i] + adapt);
    CLAMP(max[i], -adapt, mds->base_res[i] + adapt);
  }
}

static bool is_static_object(Object *ob)
{
  /* Check if the object has modifiers that might make the object "dynamic". */
  ModifierData *md = ob->modifiers.first;
  for (; md; md = md->next) {
    if (ELEM(md->type,
             eModifierType_Cloth,
             eModifierType_DynamicPaint,
             eModifierType_Explode,
             eModifierType_Ocean,
             eModifierType_ShapeKey,
             eModifierType_Softbody)) {
      return false;
    }
  }

  /* Active rigid body objects considered to be dynamic fluid objects. */
  if (ob->rigidbody_object && ob->rigidbody_object->type == RBO_TYPE_ACTIVE) {
    return false;
  }

  /* Finally, check if the object has animation data. If so, it is considered dynamic. */
  return !BKE_object_moves_in_time(ob, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bounding Box
 * \{ */

typedef struct FluidObjectBB {
  float *influence;
  float *velocity;
  float *distances;
  float *numobjs;
  int min[3], max[3], res[3];
  int hmin[3], hmax[3], hres[3];
  int total_cells, valid;
} FluidObjectBB;

static void bb_boundInsert(FluidObjectBB *bb, float point[3])
{
  int i = 0;
  if (!bb->valid) {
    for (; i < 3; i++) {
      bb->min[i] = (int)floor(point[i]);
      bb->max[i] = (int)ceil(point[i]);
    }
    bb->valid = 1;
  }
  else {
    for (; i < 3; i++) {
      if (point[i] < bb->min[i]) {
        bb->min[i] = (int)floor(point[i]);
      }
      if (point[i] > bb->max[i]) {
        bb->max[i] = (int)ceil(point[i]);
      }
    }
  }
}

static void bb_allocateData(FluidObjectBB *bb, bool use_velocity, bool use_influence)
{
  int i, res[3];

  for (i = 0; i < 3; i++) {
    res[i] = bb->max[i] - bb->min[i];
    if (res[i] <= 0) {
      return;
    }
  }
  bb->total_cells = res[0] * res[1] * res[2];
  copy_v3_v3_int(bb->res, res);

  bb->numobjs = MEM_calloc_arrayN(bb->total_cells, sizeof(float), "fluid_bb_numobjs");
  if (use_influence) {
    bb->influence = MEM_calloc_arrayN(bb->total_cells, sizeof(float), "fluid_bb_influence");
  }
  if (use_velocity) {
    bb->velocity = MEM_calloc_arrayN(bb->total_cells * 3, sizeof(float), "fluid_bb_velocity");
  }

  bb->distances = MEM_malloc_arrayN(bb->total_cells, sizeof(float), "fluid_bb_distances");
  copy_vn_fl(bb->distances, bb->total_cells, FLT_MAX);

  bb->valid = true;
}

static void bb_freeData(FluidObjectBB *bb)
{
  if (bb->numobjs) {
    MEM_freeN(bb->numobjs);
  }
  if (bb->influence) {
    MEM_freeN(bb->influence);
  }
  if (bb->velocity) {
    MEM_freeN(bb->velocity);
  }
  if (bb->distances) {
    MEM_freeN(bb->distances);
  }
}

static void bb_combineMaps(FluidObjectBB *output,
                           FluidObjectBB *bb2,
                           int additive,
                           float sample_size)
{
  int i, x, y, z;

  /* Copyfill input 1 struct and clear output for new allocation. */
  FluidObjectBB bb1;
  memcpy(&bb1, output, sizeof(FluidObjectBB));
  memset(output, 0, sizeof(FluidObjectBB));

  for (i = 0; i < 3; i++) {
    if (bb1.valid) {
      output->min[i] = MIN2(bb1.min[i], bb2->min[i]);
      output->max[i] = MAX2(bb1.max[i], bb2->max[i]);
    }
    else {
      output->min[i] = bb2->min[i];
      output->max[i] = bb2->max[i];
    }
  }
  /* Allocate output map. */
  bb_allocateData(output, (bb1.velocity || bb2->velocity), (bb1.influence || bb2->influence));

  /* Low through bounding box */
  for (x = output->min[0]; x < output->max[0]; x++) {
    for (y = output->min[1]; y < output->max[1]; y++) {
      for (z = output->min[2]; z < output->max[2]; z++) {
        int index_out = manta_get_index(x - output->min[0],
                                        output->res[0],
                                        y - output->min[1],
                                        output->res[1],
                                        z - output->min[2]);

        /* Initialize with first input if in range. */
        if (x >= bb1.min[0] && x < bb1.max[0] && y >= bb1.min[1] && y < bb1.max[1] &&
            z >= bb1.min[2] && z < bb1.max[2]) {
          int index_in = manta_get_index(
              x - bb1.min[0], bb1.res[0], y - bb1.min[1], bb1.res[1], z - bb1.min[2]);

          /* Values. */
          output->numobjs[index_out] = bb1.numobjs[index_in];
          if (output->influence && bb1.influence) {
            output->influence[index_out] = bb1.influence[index_in];
          }
          output->distances[index_out] = bb1.distances[index_in];
          if (output->velocity && bb1.velocity) {
            copy_v3_v3(&output->velocity[index_out * 3], &bb1.velocity[index_in * 3]);
          }
        }

        /* Apply second input if in range. */
        if (x >= bb2->min[0] && x < bb2->max[0] && y >= bb2->min[1] && y < bb2->max[1] &&
            z >= bb2->min[2] && z < bb2->max[2]) {
          int index_in = manta_get_index(
              x - bb2->min[0], bb2->res[0], y - bb2->min[1], bb2->res[1], z - bb2->min[2]);

          /* Values. */
          output->numobjs[index_out] = MAX2(bb2->numobjs[index_in], output->numobjs[index_out]);
          if (output->influence && bb2->influence) {
            if (additive) {
              output->influence[index_out] += bb2->influence[index_in] * sample_size;
            }
            else {
              output->influence[index_out] = MAX2(bb2->influence[index_in],
                                                  output->influence[index_out]);
            }
          }
          output->distances[index_out] = MIN2(bb2->distances[index_in],
                                              output->distances[index_out]);
          if (output->velocity && bb2->velocity) {
            /* Last sample replaces the velocity. */
            output->velocity[index_out * 3] = ADD_IF_LOWER(output->velocity[index_out * 3],
                                                           bb2->velocity[index_in * 3]);
            output->velocity[index_out * 3 + 1] = ADD_IF_LOWER(output->velocity[index_out * 3 + 1],
                                                               bb2->velocity[index_in * 3 + 1]);
            output->velocity[index_out * 3 + 2] = ADD_IF_LOWER(output->velocity[index_out * 3 + 2],
                                                               bb2->velocity[index_in * 3 + 2]);
          }
        }
      } /* Low res loop. */
    }
  }

  /* Free original data. */
  bb_freeData(&bb1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Effectors
 * \{ */

BLI_INLINE void apply_effector_fields(FluidEffectorSettings *UNUSED(mes),
                                      int index,
                                      float distance_value,
                                      float *phi_in,
                                      float numobjs_value,
                                      float *numobjs,
                                      float vel_x_value,
                                      float *vel_x,
                                      float vel_y_value,
                                      float *vel_y,
                                      float vel_z_value,
                                      float *vel_z)
{
  /* Ensure that distance value is "joined" into the levelset. */
  if (phi_in) {
    phi_in[index] = MIN2(distance_value, phi_in[index]);
  }

  /* Accumulate effector object count (important once effector object overlap). */
  if (numobjs && numobjs_value > 0) {
    numobjs[index] += 1;
  }

  if (vel_x) {
    vel_x[index] = vel_x_value;
    vel_y[index] = vel_y_value;
    vel_z[index] = vel_z_value;
  }
}

static void sample_effector(FluidEffectorSettings *mes,
                            const MVert *mvert,
                            const MLoop *mloop,
                            const MLoopTri *mlooptri,
                            float *velocity_map,
                            int index,
                            BVHTreeFromMesh *tree_data,
                            const float ray_start[3],
                            const float *vert_vel,
                            bool has_velocity)
{
  BVHTreeNearest nearest = {0};
  nearest.index = -1;

  /* Distance between two opposing vertices in a unit cube.
   * I.e. the unit cube diagonal or sqrt(3).
   * This value is our nearest neighbor search distance. */
  const float surface_distance = 1.732;
  nearest.dist_sq = surface_distance * surface_distance; /* find_nearest uses squared distance */

  /* Find the nearest point on the mesh. */
  if (BLI_bvhtree_find_nearest(
          tree_data->tree, ray_start, &nearest, tree_data->nearest_callback, tree_data) != -1) {
    float weights[3];
    int v1, v2, v3, f_index = nearest.index;

    /* Calculate barycentric weights for nearest point. */
    v1 = mloop[mlooptri[f_index].tri[0]].v;
    v2 = mloop[mlooptri[f_index].tri[1]].v;
    v3 = mloop[mlooptri[f_index].tri[2]].v;
    interp_weights_tri_v3(weights, mvert[v1].co, mvert[v2].co, mvert[v3].co, nearest.co);

    if (has_velocity) {

      /* Apply object velocity. */
      float hit_vel[3];
      interp_v3_v3v3v3(hit_vel, &vert_vel[v1 * 3], &vert_vel[v2 * 3], &vert_vel[v3 * 3], weights);

      /* Guiding has additional velocity multiplier */
      if (mes->type == FLUID_EFFECTOR_TYPE_GUIDE) {
        mul_v3_fl(hit_vel, mes->vel_multi);

        switch (mes->guide_mode) {
          case FLUID_EFFECTOR_GUIDE_AVERAGED:
            velocity_map[index * 3] = (velocity_map[index * 3] + hit_vel[0]) * 0.5f;
            velocity_map[index * 3 + 1] = (velocity_map[index * 3 + 1] + hit_vel[1]) * 0.5f;
            velocity_map[index * 3 + 2] = (velocity_map[index * 3 + 2] + hit_vel[2]) * 0.5f;
            break;
          case FLUID_EFFECTOR_GUIDE_OVERRIDE:
            velocity_map[index * 3] = hit_vel[0];
            velocity_map[index * 3 + 1] = hit_vel[1];
            velocity_map[index * 3 + 2] = hit_vel[2];
            break;
          case FLUID_EFFECTOR_GUIDE_MIN:
            velocity_map[index * 3] = MIN2(fabsf(hit_vel[0]), fabsf(velocity_map[index * 3]));
            velocity_map[index * 3 + 1] = MIN2(fabsf(hit_vel[1]),
                                               fabsf(velocity_map[index * 3 + 1]));
            velocity_map[index * 3 + 2] = MIN2(fabsf(hit_vel[2]),
                                               fabsf(velocity_map[index * 3 + 2]));
            break;
          case FLUID_EFFECTOR_GUIDE_MAX:
          default:
            velocity_map[index * 3] = MAX2(fabsf(hit_vel[0]), fabsf(velocity_map[index * 3]));
            velocity_map[index * 3 + 1] = MAX2(fabsf(hit_vel[1]),
                                               fabsf(velocity_map[index * 3 + 1]));
            velocity_map[index * 3 + 2] = MAX2(fabsf(hit_vel[2]),
                                               fabsf(velocity_map[index * 3 + 2]));
            break;
        }
      }
      else {
        /* Apply (i.e. add) effector object velocity */
        velocity_map[index * 3] += hit_vel[0];
        velocity_map[index * 3 + 1] += hit_vel[1];
        velocity_map[index * 3 + 2] += hit_vel[2];
#  ifdef DEBUG_PRINT
        /* Debugging: Print object velocities. */
        printf("adding effector object vel: [%f, %f, %f]\n", hit_vel[0], hit_vel[1], hit_vel[2]);
#  endif
      }
    }
  }
}

typedef struct ObstaclesFromDMData {
  FluidEffectorSettings *mes;

  const MVert *mvert;
  const MLoop *mloop;
  const MLoopTri *mlooptri;

  BVHTreeFromMesh *tree;
  FluidObjectBB *bb;

  bool has_velocity;
  float *vert_vel;
  int *min, *max, *res;
} ObstaclesFromDMData;

static void obstacles_from_mesh_task_cb(void *__restrict userdata,
                                        const int z,
                                        const TaskParallelTLS *__restrict UNUSED(tls))
{
  ObstaclesFromDMData *data = userdata;
  FluidObjectBB *bb = data->bb;

  for (int x = data->min[0]; x < data->max[0]; x++) {
    for (int y = data->min[1]; y < data->max[1]; y++) {
      const int index = manta_get_index(
          x - bb->min[0], bb->res[0], y - bb->min[1], bb->res[1], z - bb->min[2]);
      float ray_start[3] = {(float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f};

      /* Calculate object velocities. Result in bb->velocity. */
      sample_effector(data->mes,
                      data->mvert,
                      data->mloop,
                      data->mlooptri,
                      bb->velocity,
                      index,
                      data->tree,
                      ray_start,
                      data->vert_vel,
                      data->has_velocity);

      /* Calculate levelset values from meshes. Result in bb->distances. */
      update_distances(index,
                       bb->distances,
                       data->tree,
                       ray_start,
                       data->mes->surface_distance,
                       data->mes->flags & FLUID_EFFECTOR_USE_PLANE_INIT);

      /* Ensure that num objects are also counted inside object.
       * But don't count twice (see object inc for nearest point). */
      if (bb->distances[index] < 0) {
        bb->numobjs[index]++;
      }
    }
  }
}

static void obstacles_from_mesh(Object *coll_ob,
                                FluidDomainSettings *mds,
                                FluidEffectorSettings *mes,
                                FluidObjectBB *bb,
                                float dt)
{
  if (mes->mesh) {
    Mesh *me = NULL;
    MVert *mvert = NULL;
    const MLoopTri *looptri;
    const MLoop *mloop;
    BVHTreeFromMesh tree_data = {NULL};
    int numverts, i;

    float *vert_vel = NULL;
    bool has_velocity = false;

    me = BKE_mesh_copy_for_eval(mes->mesh, true);

    int min[3], max[3], res[3];

    /* Duplicate vertices to modify. */
    if (me->mvert) {
      me->mvert = MEM_dupallocN(me->mvert);
      CustomData_set_layer(&me->vdata, CD_MVERT, me->mvert);
    }

    BKE_mesh_ensure_normals(me);
    mvert = me->mvert;
    mloop = me->mloop;
    looptri = BKE_mesh_runtime_looptri_ensure(me);
    numverts = me->totvert;

    /* TODO (sebbas): Make initialization of vertex velocities optional? */
    {
      vert_vel = MEM_callocN(sizeof(float) * numverts * 3, "manta_obs_velocity");

      if (mes->numverts != numverts || !mes->verts_old) {
        if (mes->verts_old) {
          MEM_freeN(mes->verts_old);
        }

        mes->verts_old = MEM_callocN(sizeof(float) * numverts * 3, "manta_obs_verts_old");
        mes->numverts = numverts;
      }
      else {
        has_velocity = true;
      }
    }

    /* Transform mesh vertices to domain grid space for fast lookups */
    for (i = 0; i < numverts; i++) {
      float n[3];
      float co[3];

      /* Vertex position. */
      mul_m4_v3(coll_ob->obmat, mvert[i].co);
      manta_pos_to_cell(mds, mvert[i].co);

      /* Vertex normal. */
      normal_short_to_float_v3(n, mvert[i].no);
      mul_mat3_m4_v3(coll_ob->obmat, n);
      mul_mat3_m4_v3(mds->imat, n);
      normalize_v3(n);
      normal_float_to_short_v3(mvert[i].no, n);

      /* Vertex velocity. */
      add_v3fl_v3fl_v3i(co, mvert[i].co, mds->shift);
      if (has_velocity) {
        sub_v3_v3v3(&vert_vel[i * 3], co, &mes->verts_old[i * 3]);
        mul_v3_fl(&vert_vel[i * 3], mds->dx / dt);
      }
      copy_v3_v3(&mes->verts_old[i * 3], co);

      /* Calculate emission map bounds. */
      bb_boundInsert(bb, mvert[i].co);
    }

    /* Set emission map.
     * Use 3 cell diagonals as margin (3 * 1.732 = 5.196). */
    int bounds_margin = (int)ceil(5.196);
    clamp_bounds_in_domain(mds, bb->min, bb->max, NULL, NULL, bounds_margin, dt);
    bb_allocateData(bb, true, false);

    /* Setup loop bounds. */
    for (i = 0; i < 3; i++) {
      min[i] = bb->min[i];
      max[i] = bb->max[i];
      res[i] = bb->res[i];
    }

    if (BKE_bvhtree_from_mesh_get(&tree_data, me, BVHTREE_FROM_LOOPTRI, 4)) {

      ObstaclesFromDMData data = {
          .mes = mes,
          .mvert = mvert,
          .mloop = mloop,
          .mlooptri = looptri,
          .tree = &tree_data,
          .bb = bb,
          .has_velocity = has_velocity,
          .vert_vel = vert_vel,
          .min = min,
          .max = max,
          .res = res,
      };

      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.min_iter_per_thread = 2;
      BLI_task_parallel_range(min[2], max[2], &data, obstacles_from_mesh_task_cb, &settings);
    }
    /* Free bvh tree. */
    free_bvhtree_from_mesh(&tree_data);

    if (vert_vel) {
      MEM_freeN(vert_vel);
    }
    if (me->mvert) {
      MEM_freeN(me->mvert);
    }
    BKE_id_free(NULL, me);
  }
}

static void ensure_obstaclefields(FluidDomainSettings *mds)
{
  if (mds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE) {
    manta_ensure_obstacle(mds->fluid, mds->mmd);
  }
  if (mds->active_fields & FLUID_DOMAIN_ACTIVE_GUIDE) {
    manta_ensure_guiding(mds->fluid, mds->mmd);
  }
}

static void update_obstacleflags(FluidDomainSettings *mds,
                                 Object **coll_ob_array,
                                 int coll_ob_array_len)
{
  int active_fields = mds->active_fields;
  uint coll_index;

  /* First, remove all flags that we want to update. */
  int prev_flags = (FLUID_DOMAIN_ACTIVE_OBSTACLE | FLUID_DOMAIN_ACTIVE_GUIDE);
  active_fields &= ~prev_flags;

  /* Monitor active fields based on flow settings */
  for (coll_index = 0; coll_index < coll_ob_array_len; coll_index++) {
    Object *coll_ob = coll_ob_array[coll_index];
    FluidModifierData *mmd2 = (FluidModifierData *)BKE_modifiers_findby_type(coll_ob,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!mmd2) {
      continue;
    }

    if ((mmd2->type & MOD_FLUID_TYPE_EFFEC) && mmd2->effector) {
      FluidEffectorSettings *mes = mmd2->effector;
      if (!mes) {
        break;
      }
      if (mes->flags & FLUID_EFFECTOR_NEEDS_UPDATE) {
        mes->flags &= ~FLUID_EFFECTOR_NEEDS_UPDATE;
        mds->cache_flag |= FLUID_DOMAIN_OUTDATED_DATA;
      }
      if (mes->type == FLUID_EFFECTOR_TYPE_COLLISION) {
        active_fields |= FLUID_DOMAIN_ACTIVE_OBSTACLE;
      }
      if (mes->type == FLUID_EFFECTOR_TYPE_GUIDE) {
        active_fields |= FLUID_DOMAIN_ACTIVE_GUIDE;
      }
    }
  }
  mds->active_fields = active_fields;
}

static bool escape_effectorobject(Object *flowobj,
                                  FluidDomainSettings *mds,
                                  FluidEffectorSettings *mes,
                                  int frame)
{
  bool is_static = is_static_object(flowobj);

  bool use_effector = (mes->flags & FLUID_EFFECTOR_USE_EFFEC);

  bool is_resume = (mds->cache_frame_pause_data == frame);
  bool is_adaptive = (mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN);
  bool is_first_frame = (frame == mds->cache_frame_start);

  /* Cannot use static mode with adaptive domain.
   * The adaptive domain might expand and only later discover the static object. */
  if (is_adaptive) {
    is_static = false;
  }
  /* Skip flow objects with disabled inflow flag. */
  if (!use_effector) {
    return true;
  }
  /* Skip static effector objects after initial frame. */
  if (is_static && !is_first_frame && !is_resume) {
    return true;
  }
  return false;
}

static void compute_obstaclesemission(Scene *scene,
                                      FluidObjectBB *bb_maps,
                                      struct Depsgraph *depsgraph,
                                      float dt,
                                      Object **effecobjs,
                                      int frame,
                                      float frame_length,
                                      FluidDomainSettings *mds,
                                      uint numeffecobjs,
                                      float time_per_frame)
{
  bool is_first_frame = (frame == mds->cache_frame_start);

  /* Prepare effector maps. */
  for (int effec_index = 0; effec_index < numeffecobjs; effec_index++) {
    Object *effecobj = effecobjs[effec_index];
    FluidModifierData *mmd2 = (FluidModifierData *)BKE_modifiers_findby_type(effecobj,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!mmd2) {
      continue;
    }

    /* Check for initialized effector object. */
    if ((mmd2->type & MOD_FLUID_TYPE_EFFEC) && mmd2->effector) {
      FluidEffectorSettings *mes = mmd2->effector;
      int subframes = mes->subframes;
      FluidObjectBB *bb = &bb_maps[effec_index];

      /* Optimization: Skip this object under certain conditions. */
      if (escape_effectorobject(effecobj, mds, mes, frame)) {
        continue;
      }

      /* First frame cannot have any subframes because there is (obviously) no previous frame from
       * where subframes could come from. */
      if (is_first_frame) {
        subframes = 0;
      }

      /* More splitting because of emission subframe: If no subframes present, sample_size is 1. */
      float sample_size = 1.0f / (float)(subframes + 1);
      float subframe_dt = dt * sample_size;

      /* Emission loop. When not using subframes this will loop only once. */
      for (int subframe = 0; subframe <= subframes; subframe++) {

        /* Temporary emission map used when subframes are enabled, i.e. at least one subframe. */
        FluidObjectBB bb_temp = {NULL};

        /* Set scene time */
        /* Handle emission subframe */
        if ((subframe < subframes || time_per_frame + dt + FLT_EPSILON < frame_length) &&
            !is_first_frame) {
          scene->r.subframe = (time_per_frame + (subframe + 1.0f) * subframe_dt) / frame_length;
          scene->r.cfra = frame - 1;
        }
        else {
          scene->r.subframe = 0.0f;
          scene->r.cfra = frame;
        }
        /* Sanity check: subframe portion must be between 0 and 1. */
        CLAMP(scene->r.subframe, 0.0f, 1.0f);
#  ifdef DEBUG_PRINT
        /* Debugging: Print subframe information. */
        printf(
            "effector: frame (is first: %d): %d // scene current frame: %d // scene current "
            "subframe: "
            "%f\n",
            is_first_frame,
            frame,
            scene->r.cfra,
            scene->r.subframe);
#  endif
        /* Update frame time, this is considering current subframe fraction
         * BLI_mutex_lock() called in manta_step(), so safe to update subframe here
         * TODO (sebbas): Using BKE_scene_frame_get(scene) instead of new DEG_get_ctime(depsgraph)
         * as subframes don't work with the latter yet. */
        BKE_object_modifier_update_subframe(
            depsgraph, scene, effecobj, true, 5, BKE_scene_frame_get(scene), eModifierType_Fluid);

        if (subframes) {
          obstacles_from_mesh(effecobj, mds, mes, &bb_temp, subframe_dt);
        }
        else {
          obstacles_from_mesh(effecobj, mds, mes, bb, subframe_dt);
        }

        /* If this we emitted with temp emission map in this loop (subframe emission), we combine
         * the temp map with the original emission map. */
        if (subframes) {
          /* Combine emission maps. */
          bb_combineMaps(bb, &bb_temp, 0, 0.0f);
          bb_freeData(&bb_temp);
        }
      }
    }
  }
}

static void update_obstacles(Depsgraph *depsgraph,
                             Scene *scene,
                             Object *ob,
                             FluidDomainSettings *mds,
                             float time_per_frame,
                             float frame_length,
                             int frame,
                             float dt)
{
  FluidObjectBB *bb_maps = NULL;
  Object **effecobjs = NULL;
  uint numeffecobjs = 0;
  bool is_resume = (mds->cache_frame_pause_data == frame);
  bool is_first_frame = (frame == mds->cache_frame_start);

  effecobjs = BKE_collision_objects_create(
      depsgraph, ob, mds->effector_group, &numeffecobjs, eModifierType_Fluid);

  /* Update all effector related flags and ensure that corresponding grids get initialized. */
  update_obstacleflags(mds, effecobjs, numeffecobjs);
  ensure_obstaclefields(mds);

  /* Allocate effector map for each effector object. */
  bb_maps = MEM_callocN(sizeof(struct FluidObjectBB) * numeffecobjs, "fluid_effector_bb_maps");

  /* Initialize effector map for each effector object. */
  compute_obstaclesemission(scene,
                            bb_maps,
                            depsgraph,
                            dt,
                            effecobjs,
                            frame,
                            frame_length,
                            mds,
                            numeffecobjs,
                            time_per_frame);

  float *vel_x = manta_get_ob_velocity_x(mds->fluid);
  float *vel_y = manta_get_ob_velocity_y(mds->fluid);
  float *vel_z = manta_get_ob_velocity_z(mds->fluid);
  float *vel_x_guide = manta_get_guide_velocity_x(mds->fluid);
  float *vel_y_guide = manta_get_guide_velocity_y(mds->fluid);
  float *vel_z_guide = manta_get_guide_velocity_z(mds->fluid);
  float *phi_obs_in = manta_get_phiobs_in(mds->fluid);
  float *phi_obsstatic_in = manta_get_phiobsstatic_in(mds->fluid);
  float *phi_guide_in = manta_get_phiguide_in(mds->fluid);
  float *num_obstacles = manta_get_num_obstacle(mds->fluid);
  float *num_guides = manta_get_num_guide(mds->fluid);
  uint z;

  bool use_adaptivedomain = (mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN);

  /* Grid reset before writing again. */
  for (z = 0; z < mds->res[0] * mds->res[1] * mds->res[2]; z++) {

    /* Use big value that's not inf to initialize levelset grids. */
    if (phi_obs_in) {
      phi_obs_in[z] = PHI_MAX;
    }
    /* Only reset static effectors on first frame. Only use static effectors without adaptive
     * domains. */
    if (phi_obsstatic_in && (is_first_frame || use_adaptivedomain)) {
      phi_obsstatic_in[z] = PHI_MAX;
    }
    if (phi_guide_in) {
      phi_guide_in[z] = PHI_MAX;
    }
    if (num_obstacles) {
      num_obstacles[z] = 0;
    }
    if (num_guides) {
      num_guides[z] = 0;
    }
    if (vel_x && vel_y && vel_z) {
      vel_x[z] = 0.0f;
      vel_y[z] = 0.0f;
      vel_z[z] = 0.0f;
    }
    if (vel_x_guide && vel_y_guide && vel_z_guide) {
      vel_x_guide[z] = 0.0f;
      vel_y_guide[z] = 0.0f;
      vel_z_guide[z] = 0.0f;
    }
  }

  /* Prepare grids from effector objects. */
  for (int effec_index = 0; effec_index < numeffecobjs; effec_index++) {
    Object *effecobj = effecobjs[effec_index];
    FluidModifierData *mmd2 = (FluidModifierData *)BKE_modifiers_findby_type(effecobj,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!mmd2) {
      continue;
    }

    /* Cannot use static mode with adaptive domain.
     * The adaptive domain might expand and only later in the simulations discover the static
     * object. */
    bool is_static = is_static_object(effecobj) &&
                     ((mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) == 0);

    /* Check for initialized effector object. */
    if ((mmd2->type & MOD_FLUID_TYPE_EFFEC) && mmd2->effector) {
      FluidEffectorSettings *mes = mmd2->effector;

      /* Optimization: Skip effector objects with disabled effec flag. */
      if ((mes->flags & FLUID_EFFECTOR_USE_EFFEC) == 0) {
        continue;
      }

      FluidObjectBB *bb = &bb_maps[effec_index];
      float *velocity_map = bb->velocity;
      float *numobjs_map = bb->numobjs;
      float *distance_map = bb->distances;

      int gx, gy, gz, ex, ey, ez, dx, dy, dz;
      size_t e_index, d_index;

      /* Loop through every emission map cell. */
      for (gx = bb->min[0]; gx < bb->max[0]; gx++) {
        for (gy = bb->min[1]; gy < bb->max[1]; gy++) {
          for (gz = bb->min[2]; gz < bb->max[2]; gz++) {
            /* Compute emission map index. */
            ex = gx - bb->min[0];
            ey = gy - bb->min[1];
            ez = gz - bb->min[2];
            e_index = manta_get_index(ex, bb->res[0], ey, bb->res[1], ez);

            /* Get domain index. */
            dx = gx - mds->res_min[0];
            dy = gy - mds->res_min[1];
            dz = gz - mds->res_min[2];
            d_index = manta_get_index(dx, mds->res[0], dy, mds->res[1], dz);
            /* Make sure emission cell is inside the new domain boundary. */
            if (dx < 0 || dy < 0 || dz < 0 || dx >= mds->res[0] || dy >= mds->res[1] ||
                dz >= mds->res[2]) {
              continue;
            }

            if (mes->type == FLUID_EFFECTOR_TYPE_COLLISION) {
              float *levelset = ((is_first_frame || is_resume) && is_static) ? phi_obsstatic_in :
                                                                               phi_obs_in;
              apply_effector_fields(mes,
                                    d_index,
                                    distance_map[e_index],
                                    levelset,
                                    numobjs_map[e_index],
                                    num_obstacles,
                                    velocity_map[e_index * 3],
                                    vel_x,
                                    velocity_map[e_index * 3 + 1],
                                    vel_y,
                                    velocity_map[e_index * 3 + 2],
                                    vel_z);
            }
            if (mes->type == FLUID_EFFECTOR_TYPE_GUIDE) {
              apply_effector_fields(mes,
                                    d_index,
                                    distance_map[e_index],
                                    phi_guide_in,
                                    numobjs_map[e_index],
                                    num_guides,
                                    velocity_map[e_index * 3],
                                    vel_x_guide,
                                    velocity_map[e_index * 3 + 1],
                                    vel_y_guide,
                                    velocity_map[e_index * 3 + 2],
                                    vel_z_guide);
            }
          }
        }
      } /* End of effector map loop. */
      bb_freeData(bb);
    } /* End of effector object loop. */
  }

  BKE_collision_objects_free(effecobjs);
  if (bb_maps) {
    MEM_freeN(bb_maps);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flow
 * \{ */

typedef struct EmitFromParticlesData {
  FluidFlowSettings *mfs;
  KDTree_3d *tree;

  FluidObjectBB *bb;
  float *particle_vel;
  int *min, *max, *res;

  float solid;
  float smooth;
} EmitFromParticlesData;

static void emit_from_particles_task_cb(void *__restrict userdata,
                                        const int z,
                                        const TaskParallelTLS *__restrict UNUSED(tls))
{
  EmitFromParticlesData *data = userdata;
  FluidFlowSettings *mfs = data->mfs;
  FluidObjectBB *bb = data->bb;

  for (int x = data->min[0]; x < data->max[0]; x++) {
    for (int y = data->min[1]; y < data->max[1]; y++) {
      const int index = manta_get_index(
          x - bb->min[0], bb->res[0], y - bb->min[1], bb->res[1], z - bb->min[2]);
      const float ray_start[3] = {((float)x) + 0.5f, ((float)y) + 0.5f, ((float)z) + 0.5f};

      /* Find particle distance from the kdtree. */
      KDTreeNearest_3d nearest;
      const float range = data->solid + data->smooth;
      BLI_kdtree_3d_find_nearest(data->tree, ray_start, &nearest);

      if (nearest.dist < range) {
        bb->influence[index] = (nearest.dist < data->solid) ?
                                   1.0f :
                                   (1.0f - (nearest.dist - data->solid) / data->smooth);
        /* Uses particle velocity as initial velocity for smoke. */
        if (mfs->flags & FLUID_FLOW_INITVELOCITY && (mfs->psys->part->phystype != PART_PHYS_NO)) {
          madd_v3_v3fl(
              &bb->velocity[index * 3], &data->particle_vel[nearest.index * 3], mfs->vel_multi);
        }
      }
    }
  }
}

static void emit_from_particles(Object *flow_ob,
                                FluidDomainSettings *mds,
                                FluidFlowSettings *mfs,
                                FluidObjectBB *bb,
                                Depsgraph *depsgraph,
                                Scene *scene,
                                float dt)
{
  if (mfs && mfs->psys && mfs->psys->part &&
      ELEM(mfs->psys->part->type, PART_EMITTER, PART_FLUID))  // is particle system selected
  {
    ParticleSimulationData sim;
    ParticleSystem *psys = mfs->psys;
    float *particle_pos;
    float *particle_vel;
    int totpart = psys->totpart, totchild;
    int p = 0;
    int valid_particles = 0;
    int bounds_margin = 1;

    /* radius based flow */
    const float solid = mfs->particle_size * 0.5f;
    const float smooth = 0.5f; /* add 0.5 cells of linear falloff to reduce aliasing */
    KDTree_3d *tree = NULL;

    sim.depsgraph = depsgraph;
    sim.scene = scene;
    sim.ob = flow_ob;
    sim.psys = psys;
    sim.psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

    /* prepare curvemapping tables */
    if ((psys->part->child_flag & PART_CHILD_USE_CLUMP_CURVE) && psys->part->clumpcurve) {
      BKE_curvemapping_changed_all(psys->part->clumpcurve);
    }
    if ((psys->part->child_flag & PART_CHILD_USE_ROUGH_CURVE) && psys->part->roughcurve) {
      BKE_curvemapping_changed_all(psys->part->roughcurve);
    }
    if ((psys->part->child_flag & PART_CHILD_USE_TWIST_CURVE) && psys->part->twistcurve) {
      BKE_curvemapping_changed_all(psys->part->twistcurve);
    }

    /* initialize particle cache */
    if (psys->part->type == PART_HAIR) {
      // TODO: PART_HAIR not supported whatsoever
      totchild = 0;
    }
    else {
      totchild = psys->totchild * psys->part->disp / 100;
    }

    particle_pos = MEM_callocN(sizeof(float) * (totpart + totchild) * 3,
                               "manta_flow_particles_pos");
    particle_vel = MEM_callocN(sizeof(float) * (totpart + totchild) * 3,
                               "manta_flow_particles_vel");

    /* setup particle radius emission if enabled */
    if (mfs->flags & FLUID_FLOW_USE_PART_SIZE) {
      tree = BLI_kdtree_3d_new(psys->totpart + psys->totchild);
      bounds_margin = (int)ceil(solid + smooth);
    }

    /* calculate local position for each particle */
    for (p = 0; p < totpart + totchild; p++) {
      ParticleKey state;
      float *pos, *vel;
      if (p < totpart) {
        if (psys->particles[p].flag & (PARS_NO_DISP | PARS_UNEXIST)) {
          continue;
        }
      }
      else {
        /* handle child particle */
        ChildParticle *cpa = &psys->child[p - totpart];
        if (psys->particles[cpa->parent].flag & (PARS_NO_DISP | PARS_UNEXIST)) {
          continue;
        }
      }

      state.time = BKE_scene_frame_get(
          scene); /* DEG_get_ctime(depsgraph) does not give subframe time */
      if (psys_get_particle_state(&sim, p, &state, 0) == 0) {
        continue;
      }

      /* location */
      pos = &particle_pos[valid_particles * 3];
      copy_v3_v3(pos, state.co);
      manta_pos_to_cell(mds, pos);

      /* velocity */
      vel = &particle_vel[valid_particles * 3];
      copy_v3_v3(vel, state.vel);
      mul_mat3_m4_v3(mds->imat, &particle_vel[valid_particles * 3]);

      if (mfs->flags & FLUID_FLOW_USE_PART_SIZE) {
        BLI_kdtree_3d_insert(tree, valid_particles, pos);
      }

      /* calculate emission map bounds */
      bb_boundInsert(bb, pos);
      valid_particles++;
    }

    /* set emission map */
    clamp_bounds_in_domain(mds, bb->min, bb->max, NULL, NULL, bounds_margin, dt);
    bb_allocateData(bb, mfs->flags & FLUID_FLOW_INITVELOCITY, true);

    if (!(mfs->flags & FLUID_FLOW_USE_PART_SIZE)) {
      for (p = 0; p < valid_particles; p++) {
        int cell[3];
        size_t i = 0;
        size_t index = 0;
        int badcell = 0;

        /* 1. get corresponding cell */
        cell[0] = floor(particle_pos[p * 3]) - bb->min[0];
        cell[1] = floor(particle_pos[p * 3 + 1]) - bb->min[1];
        cell[2] = floor(particle_pos[p * 3 + 2]) - bb->min[2];
        /* check if cell is valid (in the domain boundary) */
        for (i = 0; i < 3; i++) {
          if ((cell[i] > bb->res[i] - 1) || (cell[i] < 0)) {
            badcell = 1;
            break;
          }
        }
        if (badcell) {
          continue;
        }
        /* get cell index */
        index = manta_get_index(cell[0], bb->res[0], cell[1], bb->res[1], cell[2]);
        /* Add influence to emission map */
        bb->influence[index] = 1.0f;
        /* Uses particle velocity as initial velocity for smoke */
        if (mfs->flags & FLUID_FLOW_INITVELOCITY && (psys->part->phystype != PART_PHYS_NO)) {
          madd_v3_v3fl(&bb->velocity[index * 3], &particle_vel[p * 3], mfs->vel_multi);
        }
      }  // particles loop
    }
    else if (valid_particles > 0) {  // FLUID_FLOW_USE_PART_SIZE
      int min[3], max[3], res[3];

      /* setup loop bounds */
      for (int i = 0; i < 3; i++) {
        min[i] = bb->min[i];
        max[i] = bb->max[i];
        res[i] = bb->res[i];
      }

      BLI_kdtree_3d_balance(tree);

      EmitFromParticlesData data = {
          .mfs = mfs,
          .tree = tree,
          .bb = bb,
          .particle_vel = particle_vel,
          .min = min,
          .max = max,
          .res = res,
          .solid = solid,
          .smooth = smooth,
      };

      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.min_iter_per_thread = 2;
      BLI_task_parallel_range(min[2], max[2], &data, emit_from_particles_task_cb, &settings);
    }

    if (mfs->flags & FLUID_FLOW_USE_PART_SIZE) {
      BLI_kdtree_3d_free(tree);
    }

    /* free data */
    if (particle_pos) {
      MEM_freeN(particle_pos);
    }
    if (particle_vel) {
      MEM_freeN(particle_vel);
    }
  }
}

/* Calculate map of (minimum) distances to flow/obstacle surface. Distances outside mesh are
 * positive, inside negative. */
static void update_distances(int index,
                             float *distance_map,
                             BVHTreeFromMesh *tree_data,
                             const float ray_start[3],
                             float surface_thickness,
                             bool use_plane_init)
{
  float min_dist = PHI_MAX;

  /* Planar initialization: Find nearest cells around mesh. */
  if (use_plane_init) {
    BVHTreeNearest nearest = {0};
    nearest.index = -1;
    /* Distance between two opposing vertices in a unit cube.
     * I.e. the unit cube diagonal or sqrt(3).
     * This value is our nearest neighbor search distance. */
    const float surface_distance = 1.732;
    nearest.dist_sq = surface_distance *
                      surface_distance; /* find_nearest uses squared distance. */

    /* Subtract optional surface thickness value and virtually increase the object size. */
    if (surface_thickness) {
      nearest.dist_sq += surface_thickness;
    }

    if (BLI_bvhtree_find_nearest(
            tree_data->tree, ray_start, &nearest, tree_data->nearest_callback, tree_data) != -1) {
      float ray[3] = {0};
      sub_v3_v3v3(ray, ray_start, nearest.co);
      min_dist = len_v3(ray);
      min_dist = (-1.0f) * fabsf(min_dist);
    }
  }
  /* Volumetric initialization: Ray-casts around mesh object. */
  else {
    /* Ray-casts in 26 directions.
     * (6 main axis + 12 quadrant diagonals (2D) + 8 octant diagonals (3D)). */
    float ray_dirs[26][3] = {
        {1.0f, 0.0f, 0.0f},   {0.0f, 1.0f, 0.0f},   {0.0f, 0.0f, 1.0f},  {-1.0f, 0.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},  {0.0f, 0.0f, -1.0f},  {1.0f, 1.0f, 0.0f},  {1.0f, -1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f},  {-1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 1.0f},  {1.0f, 0.0f, -1.0f},
        {-1.0f, 0.0f, 1.0f},  {-1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 1.0f},  {0.0f, 1.0f, -1.0f},
        {0.0f, -1.0f, 1.0f},  {0.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f},  {1.0f, -1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},  {-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
        {-1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}};
    size_t ray_cnt = sizeof ray_dirs / sizeof ray_dirs[0];

    /* Count ray mesh misses (i.e. no face hit) and cases where the ray direction matches the face
     * normal direction. From this information it can be derived whether a cell is inside or
     * outside the mesh. */
    int miss_cnt = 0, dir_cnt = 0;

    for (int i = 0; i < ray_cnt; i++) {
      BVHTreeRayHit hit_tree = {0};
      hit_tree.index = -1;
      hit_tree.dist = PHI_MAX;

      normalize_v3(ray_dirs[i]);
      BLI_bvhtree_ray_cast(tree_data->tree,
                           ray_start,
                           ray_dirs[i],
                           0.0f,
                           &hit_tree,
                           tree_data->raycast_callback,
                           tree_data);

      /* Ray did not hit mesh.
       * Current point definitely not inside mesh. Inside mesh as all rays have to hit. */
      if (hit_tree.index == -1) {
        miss_cnt++;
        /* Skip this ray since nothing was hit. */
        continue;
      }

      /* Ray and normal are pointing in opposite directions. */
      if (dot_v3v3(ray_dirs[i], hit_tree.no) <= 0) {
        dir_cnt++;
      }

      if (hit_tree.dist < min_dist) {
        min_dist = hit_tree.dist;
      }
    }

    /* Point lies inside mesh. Use negative sign for distance value.
     * This "if statement" has 2 conditions that can be true for points outside mesh. */
    if (!(miss_cnt > 0 || dir_cnt == ray_cnt)) {
      min_dist = (-1.0f) * fabsf(min_dist);
    }

    /* Subtract optional surface thickness value and virtually increase the object size. */
    if (surface_thickness) {
      min_dist -= surface_thickness;
    }
  }

  /* Update global distance array but ensure that older entries are not overridden. */
  distance_map[index] = MIN2(distance_map[index], min_dist);

  /* Sanity check: Ensure that distances don't explode. */
  CLAMP(distance_map[index], -PHI_MAX, PHI_MAX);
}

static void sample_mesh(FluidFlowSettings *mfs,
                        const MVert *mvert,
                        const MLoop *mloop,
                        const MLoopTri *mlooptri,
                        const MLoopUV *mloopuv,
                        float *influence_map,
                        float *velocity_map,
                        int index,
                        const int base_res[3],
                        float flow_center[3],
                        BVHTreeFromMesh *tree_data,
                        const float ray_start[3],
                        const float *vert_vel,
                        bool has_velocity,
                        int defgrp_index,
                        MDeformVert *dvert,
                        float x,
                        float y,
                        float z)
{
  float ray_dir[3] = {1.0f, 0.0f, 0.0f};
  BVHTreeRayHit hit = {0};
  BVHTreeNearest nearest = {0};

  float volume_factor = 0.0f;

  hit.index = -1;
  hit.dist = PHI_MAX;
  nearest.index = -1;

  /* Distance between two opposing vertices in a unit cube.
   * I.e. the unit cube diagonal or sqrt(3).
   * This value is our nearest neighbor search distance. */
  const float surface_distance = 1.732;
  nearest.dist_sq = surface_distance * surface_distance; /* find_nearest uses squared distance. */

  bool is_gas_flow = (mfs->type == FLUID_FLOW_TYPE_SMOKE || mfs->type == FLUID_FLOW_TYPE_FIRE ||
                      mfs->type == FLUID_FLOW_TYPE_SMOKEFIRE);

  /* Emission strength for gases will be computed below.
   * For liquids it's not needed. Just set to non zero value
   * to allow initial velocity computation. */
  float emission_strength = (is_gas_flow) ? 0.0f : 1.0f;

  /* Emission inside the flow object. */
  if (is_gas_flow && mfs->volume_density) {
    if (BLI_bvhtree_ray_cast(tree_data->tree,
                             ray_start,
                             ray_dir,
                             0.0f,
                             &hit,
                             tree_data->raycast_callback,
                             tree_data) != -1) {
      float dot = ray_dir[0] * hit.no[0] + ray_dir[1] * hit.no[1] + ray_dir[2] * hit.no[2];
      /* If ray and hit face normal are facing same direction hit point is inside a closed mesh. */
      if (dot >= 0) {
        /* Also cast a ray in opposite direction to make sure point is at least surrounded by two
         * faces. */
        negate_v3(ray_dir);
        hit.index = -1;
        hit.dist = PHI_MAX;

        BLI_bvhtree_ray_cast(tree_data->tree,
                             ray_start,
                             ray_dir,
                             0.0f,
                             &hit,
                             tree_data->raycast_callback,
                             tree_data);
        if (hit.index != -1) {
          volume_factor = mfs->volume_density;
        }
      }
    }
  }

  /* Find the nearest point on the mesh. */
  if (BLI_bvhtree_find_nearest(
          tree_data->tree, ray_start, &nearest, tree_data->nearest_callback, tree_data) != -1) {
    float weights[3];
    int v1, v2, v3, f_index = nearest.index;
    float n1[3], n2[3], n3[3], hit_normal[3];

    /* Calculate barycentric weights for nearest point. */
    v1 = mloop[mlooptri[f_index].tri[0]].v;
    v2 = mloop[mlooptri[f_index].tri[1]].v;
    v3 = mloop[mlooptri[f_index].tri[2]].v;
    interp_weights_tri_v3(weights, mvert[v1].co, mvert[v2].co, mvert[v3].co, nearest.co);

    /* Compute emission strength for smoke flow. */
    if (is_gas_flow) {
      /* Emission from surface is based on UI configurable distance value. */
      if (mfs->surface_distance) {
        emission_strength = sqrtf(nearest.dist_sq) / mfs->surface_distance;
        CLAMP(emission_strength, 0.0f, 1.0f);
        emission_strength = pow(1.0f - emission_strength, 0.5f);
      }
      else {
        emission_strength = 0.0f;
      }

      /* Apply vertex group influence if it is being used. */
      if (defgrp_index != -1 && dvert) {
        float weight_mask = BKE_defvert_find_weight(&dvert[v1], defgrp_index) * weights[0] +
                            BKE_defvert_find_weight(&dvert[v2], defgrp_index) * weights[1] +
                            BKE_defvert_find_weight(&dvert[v3], defgrp_index) * weights[2];
        emission_strength *= weight_mask;
      }

      /* Apply emission texture. */
      if ((mfs->flags & FLUID_FLOW_TEXTUREEMIT) && mfs->noise_texture) {
        float tex_co[3] = {0};
        TexResult texres;

        if (mfs->texture_type == FLUID_FLOW_TEXTURE_MAP_AUTO) {
          tex_co[0] = ((x - flow_center[0]) / base_res[0]) / mfs->texture_size;
          tex_co[1] = ((y - flow_center[1]) / base_res[1]) / mfs->texture_size;
          tex_co[2] = ((z - flow_center[2]) / base_res[2] - mfs->texture_offset) /
                      mfs->texture_size;
        }
        else if (mloopuv) {
          const float *uv[3];
          uv[0] = mloopuv[mlooptri[f_index].tri[0]].uv;
          uv[1] = mloopuv[mlooptri[f_index].tri[1]].uv;
          uv[2] = mloopuv[mlooptri[f_index].tri[2]].uv;

          interp_v2_v2v2v2(tex_co, UNPACK3(uv), weights);

          /* Map texure coord between -1.0f and 1.0f. */
          tex_co[0] = tex_co[0] * 2.0f - 1.0f;
          tex_co[1] = tex_co[1] * 2.0f - 1.0f;
          tex_co[2] = mfs->texture_offset;
        }
        texres.nor = NULL;
        BKE_texture_get_value(NULL, mfs->noise_texture, tex_co, &texres, false);
        emission_strength *= texres.tin;
      }
    }

    /* Initial velocity of flow object. Only compute velocity if emission is present. */
    if (mfs->flags & FLUID_FLOW_INITVELOCITY && velocity_map && emission_strength != 0.0) {
      /* Apply normal directional velocity. */
      if (mfs->vel_normal) {
        /* Interpolate vertex normal vectors to get nearest point normal. */
        normal_short_to_float_v3(n1, mvert[v1].no);
        normal_short_to_float_v3(n2, mvert[v2].no);
        normal_short_to_float_v3(n3, mvert[v3].no);
        interp_v3_v3v3v3(hit_normal, n1, n2, n3, weights);
        normalize_v3(hit_normal);

        /* Apply normal directional velocity. */
        velocity_map[index * 3] += hit_normal[0] * mfs->vel_normal;
        velocity_map[index * 3 + 1] += hit_normal[1] * mfs->vel_normal;
        velocity_map[index * 3 + 2] += hit_normal[2] * mfs->vel_normal;
      }
      /* Apply object velocity. */
      if (has_velocity && mfs->vel_multi) {
        float hit_vel[3];
        interp_v3_v3v3v3(
            hit_vel, &vert_vel[v1 * 3], &vert_vel[v2 * 3], &vert_vel[v3 * 3], weights);
        velocity_map[index * 3] += hit_vel[0] * mfs->vel_multi;
        velocity_map[index * 3 + 1] += hit_vel[1] * mfs->vel_multi;
        velocity_map[index * 3 + 2] += hit_vel[2] * mfs->vel_multi;
#  ifdef DEBUG_PRINT
        /* Debugging: Print flow object velocities. */
        printf("adding flow object vel: [%f, %f, %f]\n", hit_vel[0], hit_vel[1], hit_vel[2]);
#  endif
      }
      velocity_map[index * 3] += mfs->vel_coord[0];
      velocity_map[index * 3 + 1] += mfs->vel_coord[1];
      velocity_map[index * 3 + 2] += mfs->vel_coord[2];
    }
  }

  /* Apply final influence value but also consider volume initialization factor. */
  influence_map[index] = MAX2(volume_factor, emission_strength);
}

typedef struct EmitFromDMData {
  FluidDomainSettings *mds;
  FluidFlowSettings *mfs;

  const MVert *mvert;
  const MLoop *mloop;
  const MLoopTri *mlooptri;
  const MLoopUV *mloopuv;
  MDeformVert *dvert;
  int defgrp_index;

  BVHTreeFromMesh *tree;
  FluidObjectBB *bb;

  bool has_velocity;
  float *vert_vel;
  float *flow_center;
  int *min, *max, *res;
} EmitFromDMData;

static void emit_from_mesh_task_cb(void *__restrict userdata,
                                   const int z,
                                   const TaskParallelTLS *__restrict UNUSED(tls))
{
  EmitFromDMData *data = userdata;
  FluidObjectBB *bb = data->bb;

  for (int x = data->min[0]; x < data->max[0]; x++) {
    for (int y = data->min[1]; y < data->max[1]; y++) {
      const int index = manta_get_index(
          x - bb->min[0], bb->res[0], y - bb->min[1], bb->res[1], z - bb->min[2]);
      const float ray_start[3] = {((float)x) + 0.5f, ((float)y) + 0.5f, ((float)z) + 0.5f};

      /* Compute emission only for flow objects that produce fluid (i.e. skip outflow objects).
       * Result in bb->influence. Also computes initial velocities. Result in bb->velocity. */
      if ((data->mfs->behavior == FLUID_FLOW_BEHAVIOR_GEOMETRY) ||
          (data->mfs->behavior == FLUID_FLOW_BEHAVIOR_INFLOW)) {
        sample_mesh(data->mfs,
                    data->mvert,
                    data->mloop,
                    data->mlooptri,
                    data->mloopuv,
                    bb->influence,
                    bb->velocity,
                    index,
                    data->mds->base_res,
                    data->flow_center,
                    data->tree,
                    ray_start,
                    data->vert_vel,
                    data->has_velocity,
                    data->defgrp_index,
                    data->dvert,
                    (float)x,
                    (float)y,
                    (float)z);
      }

      /* Calculate levelset values from meshes. Result in bb->distances. */
      update_distances(index,
                       bb->distances,
                       data->tree,
                       ray_start,
                       data->mfs->surface_distance,
                       data->mfs->flags & FLUID_FLOW_USE_PLANE_INIT);
    }
  }
}

static void emit_from_mesh(
    Object *flow_ob, FluidDomainSettings *mds, FluidFlowSettings *mfs, FluidObjectBB *bb, float dt)
{
  if (mfs->mesh) {
    Mesh *me = NULL;
    MVert *mvert = NULL;
    const MLoopTri *mlooptri = NULL;
    const MLoop *mloop = NULL;
    const MLoopUV *mloopuv = NULL;
    MDeformVert *dvert = NULL;
    BVHTreeFromMesh tree_data = {NULL};
    int numverts, i;

    float *vert_vel = NULL;
    bool has_velocity = false;

    int defgrp_index = mfs->vgroup_density - 1;
    float flow_center[3] = {0};
    int min[3], max[3], res[3];

    /* Copy mesh for thread safety as we modify it.
     * Main issue is its VertArray being modified, then replaced and freed. */
    me = BKE_mesh_copy_for_eval(mfs->mesh, true);

    /* Duplicate vertices to modify. */
    if (me->mvert) {
      me->mvert = MEM_dupallocN(me->mvert);
      CustomData_set_layer(&me->vdata, CD_MVERT, me->mvert);
    }

    BKE_mesh_ensure_normals(me);
    mvert = me->mvert;
    mloop = me->mloop;
    mlooptri = BKE_mesh_runtime_looptri_ensure(me);
    numverts = me->totvert;
    dvert = CustomData_get_layer(&me->vdata, CD_MDEFORMVERT);
    mloopuv = CustomData_get_layer_named(&me->ldata, CD_MLOOPUV, mfs->uvlayer_name);

    if (mfs->flags & FLUID_FLOW_INITVELOCITY) {
      vert_vel = MEM_callocN(sizeof(float) * numverts * 3, "manta_flow_velocity");

      if (mfs->numverts != numverts || !mfs->verts_old) {
        if (mfs->verts_old) {
          MEM_freeN(mfs->verts_old);
        }
        mfs->verts_old = MEM_callocN(sizeof(float) * numverts * 3, "manta_flow_verts_old");
        mfs->numverts = numverts;
      }
      else {
        has_velocity = true;
      }
    }

    /* Transform mesh vertices to domain grid space for fast lookups */
    for (i = 0; i < numverts; i++) {
      float n[3];

      /* Vertex position. */
      mul_m4_v3(flow_ob->obmat, mvert[i].co);
      manta_pos_to_cell(mds, mvert[i].co);

      /* Vertex normal. */
      normal_short_to_float_v3(n, mvert[i].no);
      mul_mat3_m4_v3(flow_ob->obmat, n);
      mul_mat3_m4_v3(mds->imat, n);
      normalize_v3(n);
      normal_float_to_short_v3(mvert[i].no, n);

      /* Vertex velocity. */
      if (mfs->flags & FLUID_FLOW_INITVELOCITY) {
        float co[3];
        add_v3fl_v3fl_v3i(co, mvert[i].co, mds->shift);
        if (has_velocity) {
          sub_v3_v3v3(&vert_vel[i * 3], co, &mfs->verts_old[i * 3]);
          mul_v3_fl(&vert_vel[i * 3], mds->dx / dt);
        }
        copy_v3_v3(&mfs->verts_old[i * 3], co);
      }

      /* Calculate emission map bounds. */
      bb_boundInsert(bb, mvert[i].co);
    }
    mul_m4_v3(flow_ob->obmat, flow_center);
    manta_pos_to_cell(mds, flow_center);

    /* Set emission map.
     * Use 3 cell diagonals as margin (3 * 1.732 = 5.196). */
    int bounds_margin = (int)ceil(5.196);
    clamp_bounds_in_domain(mds, bb->min, bb->max, NULL, NULL, bounds_margin, dt);
    bb_allocateData(bb, mfs->flags & FLUID_FLOW_INITVELOCITY, true);

    /* Setup loop bounds. */
    for (i = 0; i < 3; i++) {
      min[i] = bb->min[i];
      max[i] = bb->max[i];
      res[i] = bb->res[i];
    }

    if (BKE_bvhtree_from_mesh_get(&tree_data, me, BVHTREE_FROM_LOOPTRI, 4)) {

      EmitFromDMData data = {
          .mds = mds,
          .mfs = mfs,
          .mvert = mvert,
          .mloop = mloop,
          .mlooptri = mlooptri,
          .mloopuv = mloopuv,
          .dvert = dvert,
          .defgrp_index = defgrp_index,
          .tree = &tree_data,
          .bb = bb,
          .has_velocity = has_velocity,
          .vert_vel = vert_vel,
          .flow_center = flow_center,
          .min = min,
          .max = max,
          .res = res,
      };

      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.min_iter_per_thread = 2;
      BLI_task_parallel_range(min[2], max[2], &data, emit_from_mesh_task_cb, &settings);
    }
    /* Free bvh tree. */
    free_bvhtree_from_mesh(&tree_data);

    if (vert_vel) {
      MEM_freeN(vert_vel);
    }
    if (me->mvert) {
      MEM_freeN(me->mvert);
    }
    BKE_id_free(NULL, me);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Fluid Step
 * \{ */

static void adaptive_domain_adjust(
    FluidDomainSettings *mds, Object *ob, FluidObjectBB *bb_maps, uint numflowobj, float dt)
{
  /* calculate domain shift for current frame */
  int new_shift[3] = {0};
  int total_shift[3];
  float frame_shift_f[3];
  float ob_loc[3] = {0};

  mul_m4_v3(ob->obmat, ob_loc);

  sub_v3_v3v3(frame_shift_f, ob_loc, mds->prev_loc);
  copy_v3_v3(mds->prev_loc, ob_loc);
  /* convert global space shift to local "cell" space */
  mul_mat3_m4_v3(mds->imat, frame_shift_f);
  frame_shift_f[0] = frame_shift_f[0] / mds->cell_size[0];
  frame_shift_f[1] = frame_shift_f[1] / mds->cell_size[1];
  frame_shift_f[2] = frame_shift_f[2] / mds->cell_size[2];
  /* add to total shift */
  add_v3_v3(mds->shift_f, frame_shift_f);
  /* convert to integer */
  total_shift[0] = (int)(floorf(mds->shift_f[0]));
  total_shift[1] = (int)(floorf(mds->shift_f[1]));
  total_shift[2] = (int)(floorf(mds->shift_f[2]));
  int temp_shift[3];
  copy_v3_v3_int(temp_shift, mds->shift);
  sub_v3_v3v3_int(new_shift, total_shift, mds->shift);
  copy_v3_v3_int(mds->shift, total_shift);

  /* calculate new domain boundary points so that smoke doesn't slide on sub-cell movement */
  mds->p0[0] = mds->dp0[0] - mds->cell_size[0] * (mds->shift_f[0] - total_shift[0] - 0.5f);
  mds->p0[1] = mds->dp0[1] - mds->cell_size[1] * (mds->shift_f[1] - total_shift[1] - 0.5f);
  mds->p0[2] = mds->dp0[2] - mds->cell_size[2] * (mds->shift_f[2] - total_shift[2] - 0.5f);
  mds->p1[0] = mds->p0[0] + mds->cell_size[0] * mds->base_res[0];
  mds->p1[1] = mds->p0[1] + mds->cell_size[1] * mds->base_res[1];
  mds->p1[2] = mds->p0[2] + mds->cell_size[2] * mds->base_res[2];

  /* adjust domain resolution */
  const int block_size = mds->noise_scale;
  int min[3] = {32767, 32767, 32767}, max[3] = {-32767, -32767, -32767}, res[3];
  int total_cells = 1, res_changed = 0, shift_changed = 0;
  float min_vel[3], max_vel[3];
  int x, y, z;
  float *density = manta_smoke_get_density(mds->fluid);
  float *fuel = manta_smoke_get_fuel(mds->fluid);
  float *bigdensity = manta_smoke_turbulence_get_density(mds->fluid);
  float *bigfuel = manta_smoke_turbulence_get_fuel(mds->fluid);
  float *vx = manta_get_velocity_x(mds->fluid);
  float *vy = manta_get_velocity_y(mds->fluid);
  float *vz = manta_get_velocity_z(mds->fluid);
  int wt_res[3];

  if (mds->flags & FLUID_DOMAIN_USE_NOISE && mds->fluid) {
    manta_smoke_turbulence_get_res(mds->fluid, wt_res);
  }

  INIT_MINMAX(min_vel, max_vel);

  /* Calculate bounds for current domain content */
  for (x = mds->res_min[0]; x < mds->res_max[0]; x++) {
    for (y = mds->res_min[1]; y < mds->res_max[1]; y++) {
      for (z = mds->res_min[2]; z < mds->res_max[2]; z++) {
        int xn = x - new_shift[0];
        int yn = y - new_shift[1];
        int zn = z - new_shift[2];
        int index;
        float max_den;

        /* skip if cell already belongs to new area */
        if (xn >= min[0] && xn <= max[0] && yn >= min[1] && yn <= max[1] && zn >= min[2] &&
            zn <= max[2]) {
          continue;
        }

        index = manta_get_index(x - mds->res_min[0],
                                mds->res[0],
                                y - mds->res_min[1],
                                mds->res[1],
                                z - mds->res_min[2]);
        max_den = (fuel) ? MAX2(density[index], fuel[index]) : density[index];

        /* check high resolution bounds if max density isnt already high enough */
        if (max_den < mds->adapt_threshold && mds->flags & FLUID_DOMAIN_USE_NOISE && mds->fluid) {
          int i, j, k;
          /* high res grid index */
          int xx = (x - mds->res_min[0]) * block_size;
          int yy = (y - mds->res_min[1]) * block_size;
          int zz = (z - mds->res_min[2]) * block_size;

          for (i = 0; i < block_size; i++) {
            for (j = 0; j < block_size; j++) {
              for (k = 0; k < block_size; k++) {
                int big_index = manta_get_index(xx + i, wt_res[0], yy + j, wt_res[1], zz + k);
                float den = (bigfuel) ? MAX2(bigdensity[big_index], bigfuel[big_index]) :
                                        bigdensity[big_index];
                if (den > max_den) {
                  max_den = den;
                }
              }
            }
          }
        }

        /* content bounds (use shifted coordinates) */
        if (max_den >= mds->adapt_threshold) {
          if (min[0] > xn) {
            min[0] = xn;
          }
          if (min[1] > yn) {
            min[1] = yn;
          }
          if (min[2] > zn) {
            min[2] = zn;
          }
          if (max[0] < xn) {
            max[0] = xn;
          }
          if (max[1] < yn) {
            max[1] = yn;
          }
          if (max[2] < zn) {
            max[2] = zn;
          }
        }

        /* velocity bounds */
        if (min_vel[0] > vx[index]) {
          min_vel[0] = vx[index];
        }
        if (min_vel[1] > vy[index]) {
          min_vel[1] = vy[index];
        }
        if (min_vel[2] > vz[index]) {
          min_vel[2] = vz[index];
        }
        if (max_vel[0] < vx[index]) {
          max_vel[0] = vx[index];
        }
        if (max_vel[1] < vy[index]) {
          max_vel[1] = vy[index];
        }
        if (max_vel[2] < vz[index]) {
          max_vel[2] = vz[index];
        }
      }
    }
  }

  /* also apply emission maps */
  for (int i = 0; i < numflowobj; i++) {
    FluidObjectBB *bb = &bb_maps[i];

    for (x = bb->min[0]; x < bb->max[0]; x++) {
      for (y = bb->min[1]; y < bb->max[1]; y++) {
        for (z = bb->min[2]; z < bb->max[2]; z++) {
          int index = manta_get_index(
              x - bb->min[0], bb->res[0], y - bb->min[1], bb->res[1], z - bb->min[2]);
          float max_den = bb->influence[index];

          /* density bounds */
          if (max_den >= mds->adapt_threshold) {
            if (min[0] > x) {
              min[0] = x;
            }
            if (min[1] > y) {
              min[1] = y;
            }
            if (min[2] > z) {
              min[2] = z;
            }
            if (max[0] < x) {
              max[0] = x;
            }
            if (max[1] < y) {
              max[1] = y;
            }
            if (max[2] < z) {
              max[2] = z;
            }
          }
        }
      }
    }
  }

  /* calculate new bounds based on these values */
  clamp_bounds_in_domain(mds, min, max, min_vel, max_vel, mds->adapt_margin + 1, dt);

  for (int i = 0; i < 3; i++) {
    /* calculate new resolution */
    res[i] = max[i] - min[i];
    total_cells *= res[i];

    if (new_shift[i]) {
      shift_changed = 1;
    }

    /* if no content set minimum dimensions */
    if (res[i] <= 0) {
      int j;
      for (j = 0; j < 3; j++) {
        min[j] = 0;
        max[j] = 1;
        res[j] = 1;
      }
      res_changed = 1;
      total_cells = 1;
      break;
    }
    if (min[i] != mds->res_min[i] || max[i] != mds->res_max[i]) {
      res_changed = 1;
    }
  }

  if (res_changed || shift_changed) {
    BKE_fluid_reallocate_copy_fluid(
        mds, mds->res, res, mds->res_min, min, mds->res_max, temp_shift, total_shift);

    /* set new domain dimensions */
    copy_v3_v3_int(mds->res_min, min);
    copy_v3_v3_int(mds->res_max, max);
    copy_v3_v3_int(mds->res, res);
    mds->total_cells = total_cells;

    /* Redo adapt time step in manta to refresh solver state (ie time variables) */
    manta_adapt_timestep(mds->fluid);
  }

  /* update global size field with new bbox size */
  /* volume bounds */
  float minf[3], maxf[3], size[3];
  madd_v3fl_v3fl_v3fl_v3i(minf, mds->p0, mds->cell_size, mds->res_min);
  madd_v3fl_v3fl_v3fl_v3i(maxf, mds->p0, mds->cell_size, mds->res_max);
  /* calculate domain dimensions */
  sub_v3_v3v3(size, maxf, minf);
  /* apply object scale */
  for (int i = 0; i < 3; i++) {
    size[i] = fabsf(size[i] * ob->scale[i]);
  }
  copy_v3_v3(mds->global_size, size);
}

BLI_INLINE void apply_outflow_fields(int index,
                                     float distance_value,
                                     float *density,
                                     float *heat,
                                     float *fuel,
                                     float *react,
                                     float *color_r,
                                     float *color_g,
                                     float *color_b,
                                     float *phiout)
{
  /* Set levelset value for liquid inflow.
   * Ensure that distance value is "joined" into the levelset. */
  if (phiout) {
    phiout[index] = MIN2(distance_value, phiout[index]);
  }

  /* Set smoke outflow, i.e. reset cell to zero. */
  if (density) {
    density[index] = 0.0f;
  }
  if (heat) {
    heat[index] = 0.0f;
  }
  if (fuel) {
    fuel[index] = 0.0f;
    react[index] = 0.0f;
  }
  if (color_r) {
    color_r[index] = 0.0f;
    color_g[index] = 0.0f;
    color_b[index] = 0.0f;
  }
}

BLI_INLINE void apply_inflow_fields(FluidFlowSettings *mfs,
                                    float emission_value,
                                    float distance_value,
                                    int index,
                                    float *density_in,
                                    const float *density,
                                    float *heat_in,
                                    const float *heat,
                                    float *fuel_in,
                                    const float *fuel,
                                    float *react_in,
                                    const float *react,
                                    float *color_r_in,
                                    const float *color_r,
                                    float *color_g_in,
                                    const float *color_g,
                                    float *color_b_in,
                                    const float *color_b,
                                    float *phi_in,
                                    float *emission_in)
{
  /* Set levelset value for liquid inflow.
   * Ensure that distance value is "joined" into the levelset. */
  if (phi_in) {
    phi_in[index] = MIN2(distance_value, phi_in[index]);
  }

  /* Set emission value for smoke inflow.
   * Ensure that emission value is "maximized". */
  if (emission_in) {
    emission_in[index] = MAX2(emission_value, emission_in[index]);
  }

  /* Set inflow for smoke from here on. */
  int absolute_flow = (mfs->flags & FLUID_FLOW_ABSOLUTE);
  float dens_old = (density) ? density[index] : 0.0;
  // float fuel_old = (fuel) ? fuel[index] : 0.0f;  /* UNUSED */
  float dens_flow = (mfs->type == FLUID_FLOW_TYPE_FIRE) ? 0.0f : emission_value * mfs->density;
  float fuel_flow = (fuel) ? emission_value * mfs->fuel_amount : 0.0f;
  /* Set heat inflow. */
  if (heat && heat_in) {
    if (emission_value > 0.0f) {
      heat_in[index] = ADD_IF_LOWER(heat[index], mfs->temperature);
    }
  }

  /* Set density and fuel - absolute mode. */
  if (absolute_flow) {
    if (density && density_in) {
      if (mfs->type != FLUID_FLOW_TYPE_FIRE && dens_flow > density[index]) {
        /* Use MAX2 to preserve values from other emitters at this cell. */
        density_in[index] = MAX2(dens_flow, density_in[index]);
      }
    }
    if (fuel && fuel_in) {
      if (mfs->type != FLUID_FLOW_TYPE_SMOKE && fuel_flow && fuel_flow > fuel[index]) {
        /* Use MAX2 to preserve values from other emitters at this cell. */
        fuel_in[index] = MAX2(fuel_flow, fuel_in[index]);
      }
    }
  }
  /* Set density and fuel - additive mode. */
  else {
    if (density && density_in) {
      if (mfs->type != FLUID_FLOW_TYPE_FIRE) {
        density_in[index] += dens_flow;
        CLAMP(density_in[index], 0.0f, 1.0f);
      }
    }
    if (fuel && fuel_in) {
      if (mfs->type != FLUID_FLOW_TYPE_SMOKE && mfs->fuel_amount) {
        fuel_in[index] += fuel_flow;
        CLAMP(fuel_in[index], 0.0f, 10.0f);
      }
    }
  }

  /* Set color. */
  if (color_r && color_r_in) {
    if (dens_flow) {
      float total_dens = density[index] / (dens_old + dens_flow);
      color_r_in[index] = (color_r[index] + mfs->color[0] * dens_flow) * total_dens;
      color_g_in[index] = (color_g[index] + mfs->color[1] * dens_flow) * total_dens;
      color_b_in[index] = (color_b[index] + mfs->color[2] * dens_flow) * total_dens;
    }
  }

  /* Set fire reaction coordinate. */
  if (fuel && fuel_in) {
    /* Instead of using 1.0 for all new fuel add slight falloff to reduce flow blocky-ness. */
    float value = 1.0f - pow2f(1.0f - emission_value);

    if (fuel_in[index] > FLT_EPSILON && value > react[index]) {
      float f = fuel_flow / fuel_in[index];
      react_in[index] = value * f + (1.0f - f) * react[index];
      CLAMP(react_in[index], 0.0f, value);
    }
  }
}

static void ensure_flowsfields(FluidDomainSettings *mds)
{
  if (mds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL) {
    manta_ensure_invelocity(mds->fluid, mds->mmd);
  }
  if (mds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW) {
    manta_ensure_outflow(mds->fluid, mds->mmd);
  }
  if (mds->active_fields & FLUID_DOMAIN_ACTIVE_HEAT) {
    manta_smoke_ensure_heat(mds->fluid, mds->mmd);
  }
  if (mds->active_fields & FLUID_DOMAIN_ACTIVE_FIRE) {
    manta_smoke_ensure_fire(mds->fluid, mds->mmd);
  }
  if (mds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS) {
    /* initialize all smoke with "active_color" */
    manta_smoke_ensure_colors(mds->fluid, mds->mmd);
  }
  if (mds->type == FLUID_DOMAIN_TYPE_LIQUID &&
      (mds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY ||
       mds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM ||
       mds->particle_type & FLUID_DOMAIN_PARTICLE_TRACER)) {
    manta_liquid_ensure_sndparts(mds->fluid, mds->mmd);
  }
}

static void update_flowsflags(FluidDomainSettings *mds, Object **flowobjs, int numflowobj)
{
  int active_fields = mds->active_fields;
  uint flow_index;

  /* First, remove all flags that we want to update. */
  int prev_flags = (FLUID_DOMAIN_ACTIVE_INVEL | FLUID_DOMAIN_ACTIVE_OUTFLOW |
                    FLUID_DOMAIN_ACTIVE_HEAT | FLUID_DOMAIN_ACTIVE_FIRE);
  active_fields &= ~prev_flags;

  /* Monitor active fields based on flow settings */
  for (flow_index = 0; flow_index < numflowobj; flow_index++) {
    Object *flow_ob = flowobjs[flow_index];
    FluidModifierData *mmd2 = (FluidModifierData *)BKE_modifiers_findby_type(flow_ob,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!mmd2) {
      continue;
    }

    if ((mmd2->type & MOD_FLUID_TYPE_FLOW) && mmd2->flow) {
      FluidFlowSettings *mfs = mmd2->flow;
      if (!mfs) {
        break;
      }
      if (mfs->flags & FLUID_FLOW_NEEDS_UPDATE) {
        mfs->flags &= ~FLUID_FLOW_NEEDS_UPDATE;
        mds->cache_flag |= FLUID_DOMAIN_OUTDATED_DATA;
      }
      if (mfs->flags & FLUID_FLOW_INITVELOCITY) {
        active_fields |= FLUID_DOMAIN_ACTIVE_INVEL;
      }
      if (mfs->behavior == FLUID_FLOW_BEHAVIOR_OUTFLOW) {
        active_fields |= FLUID_DOMAIN_ACTIVE_OUTFLOW;
      }
      /* liquids done from here */
      if (mds->type == FLUID_DOMAIN_TYPE_LIQUID) {
        continue;
      }

      /* activate heat field if flow produces any heat */
      if (mfs->temperature) {
        active_fields |= FLUID_DOMAIN_ACTIVE_HEAT;
      }
      /* activate fuel field if flow adds any fuel */
      if (mfs->fuel_amount &&
          (mfs->type == FLUID_FLOW_TYPE_FIRE || mfs->type == FLUID_FLOW_TYPE_SMOKEFIRE)) {
        active_fields |= FLUID_DOMAIN_ACTIVE_FIRE;
      }
      /* activate color field if flows add smoke with varying colors */
      if (mfs->density &&
          (mfs->type == FLUID_FLOW_TYPE_SMOKE || mfs->type == FLUID_FLOW_TYPE_SMOKEFIRE)) {
        if (!(active_fields & FLUID_DOMAIN_ACTIVE_COLOR_SET)) {
          copy_v3_v3(mds->active_color, mfs->color);
          active_fields |= FLUID_DOMAIN_ACTIVE_COLOR_SET;
        }
        else if (!equals_v3v3(mds->active_color, mfs->color)) {
          copy_v3_v3(mds->active_color, mfs->color);
          active_fields |= FLUID_DOMAIN_ACTIVE_COLORS;
        }
      }
    }
  }
  /* Monitor active fields based on domain settings */
  if (mds->type == FLUID_DOMAIN_TYPE_GAS && active_fields & FLUID_DOMAIN_ACTIVE_FIRE) {
    /* heat is always needed for fire */
    active_fields |= FLUID_DOMAIN_ACTIVE_HEAT;
    /* also activate colors if domain smoke color differs from active color */
    if (!(active_fields & FLUID_DOMAIN_ACTIVE_COLOR_SET)) {
      copy_v3_v3(mds->active_color, mds->flame_smoke_color);
      active_fields |= FLUID_DOMAIN_ACTIVE_COLOR_SET;
    }
    else if (!equals_v3v3(mds->active_color, mds->flame_smoke_color)) {
      copy_v3_v3(mds->active_color, mds->flame_smoke_color);
      active_fields |= FLUID_DOMAIN_ACTIVE_COLORS;
    }
  }
  mds->active_fields = active_fields;
}

static bool escape_flowsobject(Object *flowobj,
                               FluidDomainSettings *mds,
                               FluidFlowSettings *mfs,
                               int frame)
{
  bool use_velocity = (mfs->flags & FLUID_FLOW_INITVELOCITY);
  bool is_static = is_static_object(flowobj);

  bool liquid_flow = mfs->type == FLUID_FLOW_TYPE_LIQUID;
  bool gas_flow = (mfs->type == FLUID_FLOW_TYPE_SMOKE || mfs->type == FLUID_FLOW_TYPE_FIRE ||
                   mfs->type == FLUID_FLOW_TYPE_SMOKEFIRE);
  bool is_geometry = (mfs->behavior == FLUID_FLOW_BEHAVIOR_GEOMETRY);
  bool is_inflow = (mfs->behavior == FLUID_FLOW_BEHAVIOR_INFLOW);
  bool is_outflow = (mfs->behavior == FLUID_FLOW_BEHAVIOR_OUTFLOW);
  bool use_flow = (mfs->flags & FLUID_FLOW_USE_INFLOW);

  bool liquid_domain = mds->type == FLUID_DOMAIN_TYPE_LIQUID;
  bool gas_domain = mds->type == FLUID_DOMAIN_TYPE_GAS;
  bool is_adaptive = (mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN);
  bool is_resume = (mds->cache_frame_pause_data == frame);
  bool is_first_frame = (mds->cache_frame_start == frame);

  /* Cannot use static mode with adaptive domain.
   * The adaptive domain might expand and only later discover the static object. */
  if (is_adaptive) {
    is_static = false;
  }
  /* Skip flow objects with disabled inflow flag. */
  if ((is_inflow || is_outflow) && !use_flow) {
    return true;
  }
  /* No need to compute emission value if it won't be applied. */
  if (liquid_flow && is_geometry && !is_first_frame) {
    return true;
  }
  /* Skip flow object if it does not "belong" to this domain type. */
  if ((liquid_flow && gas_domain) || (gas_flow && liquid_domain)) {
    return true;
  }
  /* Optimization: Static liquid flow objects don't need emission after first frame.
   * TODO (sebbas): Also do not use static mode if initial velocities are enabled. */
  if (liquid_flow && is_static && !is_first_frame && !is_resume && !use_velocity) {
    return true;
  }
  return false;
}

static void compute_flowsemission(Scene *scene,
                                  FluidObjectBB *bb_maps,
                                  struct Depsgraph *depsgraph,
                                  float dt,
                                  Object **flowobjs,
                                  int frame,
                                  float frame_length,
                                  FluidDomainSettings *mds,
                                  uint numflowobjs,
                                  float time_per_frame)
{
  bool is_first_frame = (frame == mds->cache_frame_start);

  /* Prepare flow emission maps. */
  for (int flow_index = 0; flow_index < numflowobjs; flow_index++) {
    Object *flowobj = flowobjs[flow_index];
    FluidModifierData *mmd2 = (FluidModifierData *)BKE_modifiers_findby_type(flowobj,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!mmd2) {
      continue;
    }

    /* Check for initialized flow object. */
    if ((mmd2->type & MOD_FLUID_TYPE_FLOW) && mmd2->flow) {
      FluidFlowSettings *mfs = mmd2->flow;
      int subframes = mfs->subframes;
      FluidObjectBB *bb = &bb_maps[flow_index];

      /* Optimization: Skip this object under certain conditions. */
      if (escape_flowsobject(flowobj, mds, mfs, frame)) {
        continue;
      }

      /* First frame cannot have any subframes because there is (obviously) no previous frame from
       * where subframes could come from. */
      if (is_first_frame) {
        subframes = 0;
      }

      /* More splitting because of emission subframe: If no subframes present, sample_size is 1. */
      float sample_size = 1.0f / (float)(subframes + 1);
      float subframe_dt = dt * sample_size;

      /* Emission loop. When not using subframes this will loop only once. */
      for (int subframe = 0; subframe <= subframes; subframe++) {
        /* Temporary emission map used when subframes are enabled, i.e. at least one subframe. */
        FluidObjectBB bb_temp = {NULL};

        /* Set scene time */
        if ((subframe < subframes || time_per_frame + dt + FLT_EPSILON < frame_length) &&
            !is_first_frame) {
          scene->r.subframe = (time_per_frame + (subframe + 1.0f) * subframe_dt) / frame_length;
          scene->r.cfra = frame - 1;
        }
        else {
          scene->r.subframe = 0.0f;
          scene->r.cfra = frame;
        }

        /* Sanity check: subframe portion must be between 0 and 1. */
        CLAMP(scene->r.subframe, 0.0f, 1.0f);
#  ifdef DEBUG_PRINT
        /* Debugging: Print subframe information. */
        printf(
            "flow: frame (is first: %d): %d // scene current frame: %d // scene current subframe: "
            "%f\n",
            is_first_frame,
            frame,
            scene->r.cfra,
            scene->r.subframe);
#  endif
        /* Update frame time, this is considering current subframe fraction
         * BLI_mutex_lock() called in manta_step(), so safe to update subframe here
         * TODO (sebbas): Using BKE_scene_frame_get(scene) instead of new DEG_get_ctime(depsgraph)
         * as subframes don't work with the latter yet. */
        BKE_object_modifier_update_subframe(
            depsgraph, scene, flowobj, true, 5, BKE_scene_frame_get(scene), eModifierType_Fluid);

        /* Emission from particles. */
        if (mfs->source == FLUID_FLOW_SOURCE_PARTICLES) {
          if (subframes) {
            emit_from_particles(flowobj, mds, mfs, &bb_temp, depsgraph, scene, subframe_dt);
          }
          else {
            emit_from_particles(flowobj, mds, mfs, bb, depsgraph, scene, subframe_dt);
          }
        }
        /* Emission from mesh. */
        else if (mfs->source == FLUID_FLOW_SOURCE_MESH) {
          if (subframes) {
            emit_from_mesh(flowobj, mds, mfs, &bb_temp, subframe_dt);
          }
          else {
            emit_from_mesh(flowobj, mds, mfs, bb, subframe_dt);
          }
        }
        else {
          printf("Error: unknown flow emission source\n");
        }

        /* If this we emitted with temp emission map in this loop (subframe emission), we combine
         * the temp map with the original emission map. */
        if (subframes) {
          /* Combine emission maps. */
          bb_combineMaps(bb, &bb_temp, !(mfs->flags & FLUID_FLOW_ABSOLUTE), sample_size);
          bb_freeData(&bb_temp);
        }
      }
    }
  }
#  ifdef DEBUG_PRINT
  /* Debugging: Print time information. */
  printf("flow: frame: %d // time per frame: %f // frame length: %f // dt: %f\n",
         frame,
         time_per_frame,
         frame_length,
         dt);
#  endif
}

static void update_flowsfluids(struct Depsgraph *depsgraph,
                               Scene *scene,
                               Object *ob,
                               FluidDomainSettings *mds,
                               float time_per_frame,
                               float frame_length,
                               int frame,
                               float dt)
{
  FluidObjectBB *bb_maps = NULL;
  Object **flowobjs = NULL;
  uint numflowobjs = 0;
  bool is_resume = (mds->cache_frame_pause_data == frame);
  bool is_first_frame = (mds->cache_frame_start == frame);

  flowobjs = BKE_collision_objects_create(
      depsgraph, ob, mds->fluid_group, &numflowobjs, eModifierType_Fluid);

  /* Update all flow related flags and ensure that corresponding grids get initialized. */
  update_flowsflags(mds, flowobjs, numflowobjs);
  ensure_flowsfields(mds);

  /* Allocate emission map for each flow object. */
  bb_maps = MEM_callocN(sizeof(struct FluidObjectBB) * numflowobjs, "fluid_flow_bb_maps");

  /* Initialize emission map for each flow object. */
  compute_flowsemission(scene,
                        bb_maps,
                        depsgraph,
                        dt,
                        flowobjs,
                        frame,
                        frame_length,
                        mds,
                        numflowobjs,
                        time_per_frame);

  /* Adjust domain size if needed. Only do this once for every frame. */
  if (mds->type == FLUID_DOMAIN_TYPE_GAS && mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) {
    adaptive_domain_adjust(mds, ob, bb_maps, numflowobjs, dt);
  }

  float *phi_in = manta_get_phi_in(mds->fluid);
  float *phistatic_in = manta_get_phistatic_in(mds->fluid);
  float *phiout_in = manta_get_phiout_in(mds->fluid);
  float *phioutstatic_in = manta_get_phioutstatic_in(mds->fluid);

  float *density = manta_smoke_get_density(mds->fluid);
  float *color_r = manta_smoke_get_color_r(mds->fluid);
  float *color_g = manta_smoke_get_color_g(mds->fluid);
  float *color_b = manta_smoke_get_color_b(mds->fluid);
  float *fuel = manta_smoke_get_fuel(mds->fluid);
  float *heat = manta_smoke_get_heat(mds->fluid);
  float *react = manta_smoke_get_react(mds->fluid);

  float *density_in = manta_smoke_get_density_in(mds->fluid);
  float *heat_in = manta_smoke_get_heat_in(mds->fluid);
  float *color_r_in = manta_smoke_get_color_r_in(mds->fluid);
  float *color_g_in = manta_smoke_get_color_g_in(mds->fluid);
  float *color_b_in = manta_smoke_get_color_b_in(mds->fluid);
  float *fuel_in = manta_smoke_get_fuel_in(mds->fluid);
  float *react_in = manta_smoke_get_react_in(mds->fluid);
  float *emission_in = manta_smoke_get_emission_in(mds->fluid);

  float *velx_initial = manta_get_in_velocity_x(mds->fluid);
  float *vely_initial = manta_get_in_velocity_y(mds->fluid);
  float *velz_initial = manta_get_in_velocity_z(mds->fluid);
  uint z;

  /* Grid reset before writing again. */
  for (z = 0; z < mds->res[0] * mds->res[1] * mds->res[2]; z++) {
    /* Only reset static phi on first frame, dynamic phi gets reset every time. */
    if (phistatic_in && is_first_frame) {
      phistatic_in[z] = PHI_MAX;
    }
    if (phi_in) {
      phi_in[z] = PHI_MAX;
    }
    /* Only reset static phi on first frame, dynamic phi gets reset every time. */
    if (phioutstatic_in && is_first_frame) {
      phioutstatic_in[z] = PHI_MAX;
    }
    if (phiout_in) {
      phiout_in[z] = PHI_MAX;
    }
    /* Sync smoke inflow grids with their counterparts (simulation grids). */
    if (density_in) {
      density_in[z] = density[z];
    }
    if (heat_in) {
      heat_in[z] = heat[z];
    }
    if (color_r_in) {
      color_r_in[z] = color_r[z];
      color_g_in[z] = color_b[z];
      color_b_in[z] = color_g[z];
    }
    if (fuel_in) {
      fuel_in[z] = fuel[z];
      react_in[z] = react[z];
    }
    if (emission_in) {
      emission_in[z] = 0.0f;
    }
    if (velx_initial) {
      velx_initial[z] = 0.0f;
      vely_initial[z] = 0.0f;
      velz_initial[z] = 0.0f;
    }
  }

  /* Apply emission data for every flow object. */
  for (int flow_index = 0; flow_index < numflowobjs; flow_index++) {
    Object *flowobj = flowobjs[flow_index];
    FluidModifierData *mmd2 = (FluidModifierData *)BKE_modifiers_findby_type(flowobj,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!mmd2) {
      continue;
    }

    /* Check for initialized flow object. */
    if ((mmd2->type & MOD_FLUID_TYPE_FLOW) && mmd2->flow) {
      FluidFlowSettings *mfs = mmd2->flow;

      bool is_inflow = (mfs->behavior == FLUID_FLOW_BEHAVIOR_INFLOW);
      bool is_geometry = (mfs->behavior == FLUID_FLOW_BEHAVIOR_GEOMETRY);
      bool is_outflow = (mfs->behavior == FLUID_FLOW_BEHAVIOR_OUTFLOW);
      bool is_static = is_static_object(flowobj) &&
                       ((mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) == 0);

      FluidObjectBB *bb = &bb_maps[flow_index];
      float *velocity_map = bb->velocity;
      float *emission_map = bb->influence;
      float *distance_map = bb->distances;

      int gx, gy, gz, ex, ey, ez, dx, dy, dz;
      size_t e_index, d_index;

      /* Loop through every emission map cell. */
      for (gx = bb->min[0]; gx < bb->max[0]; gx++) {
        for (gy = bb->min[1]; gy < bb->max[1]; gy++) {
          for (gz = bb->min[2]; gz < bb->max[2]; gz++) {
            /* Compute emission map index. */
            ex = gx - bb->min[0];
            ey = gy - bb->min[1];
            ez = gz - bb->min[2];
            e_index = manta_get_index(ex, bb->res[0], ey, bb->res[1], ez);

            /* Get domain index. */
            dx = gx - mds->res_min[0];
            dy = gy - mds->res_min[1];
            dz = gz - mds->res_min[2];
            d_index = manta_get_index(dx, mds->res[0], dy, mds->res[1], dz);
            /* Make sure emission cell is inside the new domain boundary. */
            if (dx < 0 || dy < 0 || dz < 0 || dx >= mds->res[0] || dy >= mds->res[1] ||
                dz >= mds->res[2]) {
              continue;
            }

            /* Delete fluid in outflow regions. */
            if (is_outflow) {
              float *levelset = ((is_first_frame || is_resume) && is_static) ? phioutstatic_in :
                                                                               phiout_in;
              apply_outflow_fields(d_index,
                                   distance_map[e_index],
                                   density_in,
                                   heat_in,
                                   fuel_in,
                                   react_in,
                                   color_r_in,
                                   color_g_in,
                                   color_b_in,
                                   levelset);
            }
            /* Do not apply inflow after the first frame when in geometry mode. */
            else if (is_geometry && !is_first_frame) {
              apply_inflow_fields(mfs,
                                  0.0f,
                                  PHI_MAX,
                                  d_index,
                                  density_in,
                                  density,
                                  heat_in,
                                  heat,
                                  fuel_in,
                                  fuel,
                                  react_in,
                                  react,
                                  color_r_in,
                                  color_r,
                                  color_g_in,
                                  color_g,
                                  color_b_in,
                                  color_b,
                                  phi_in,
                                  emission_in);
            }
            /* Main inflow application. */
            else if (is_geometry || is_inflow) {
              float *levelset = ((is_first_frame || is_resume) && is_static && !is_geometry) ?
                                    phistatic_in :
                                    phi_in;
              apply_inflow_fields(mfs,
                                  emission_map[e_index],
                                  distance_map[e_index],
                                  d_index,
                                  density_in,
                                  density,
                                  heat_in,
                                  heat,
                                  fuel_in,
                                  fuel,
                                  react_in,
                                  react,
                                  color_r_in,
                                  color_r,
                                  color_g_in,
                                  color_g,
                                  color_b_in,
                                  color_b,
                                  levelset,
                                  emission_in);
              if (mfs->flags & FLUID_FLOW_INITVELOCITY) {
                /* Use the initial velocity from the inflow object with the highest velocity for
                 * now. */
                float vel_initial[3];
                vel_initial[0] = velx_initial[d_index];
                vel_initial[1] = vely_initial[d_index];
                vel_initial[2] = velz_initial[d_index];
                float vel_initial_strength = len_squared_v3(vel_initial);
                float vel_map_strength = len_squared_v3(velocity_map + 3 * e_index);
                if (vel_map_strength > vel_initial_strength) {
                  velx_initial[d_index] = velocity_map[e_index * 3];
                  vely_initial[d_index] = velocity_map[e_index * 3 + 1];
                  velz_initial[d_index] = velocity_map[e_index * 3 + 2];
                }
              }
            }
          }
        }
      } /* End of flow emission map loop. */
      bb_freeData(bb);
    } /* End of flow object loop. */
  }

  BKE_collision_objects_free(flowobjs);
  if (bb_maps) {
    MEM_freeN(bb_maps);
  }
}

typedef struct UpdateEffectorsData {
  Scene *scene;
  FluidDomainSettings *mds;
  ListBase *effectors;

  float *density;
  float *fuel;
  float *force_x;
  float *force_y;
  float *force_z;
  float *velocity_x;
  float *velocity_y;
  float *velocity_z;
  int *flags;
  float *phi_obs_in;
} UpdateEffectorsData;

static void update_effectors_task_cb(void *__restrict userdata,
                                     const int x,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{
  UpdateEffectorsData *data = userdata;
  FluidDomainSettings *mds = data->mds;

  for (int y = 0; y < mds->res[1]; y++) {
    for (int z = 0; z < mds->res[2]; z++) {
      EffectedPoint epoint;
      float mag;
      float voxel_center[3] = {0, 0, 0}, vel[3] = {0, 0, 0}, retvel[3] = {0, 0, 0};
      const uint index = manta_get_index(x, mds->res[0], y, mds->res[1], z);

      if ((data->fuel && MAX2(data->density[index], data->fuel[index]) < FLT_EPSILON) ||
          (data->density && data->density[index] < FLT_EPSILON) ||
          (data->phi_obs_in && data->phi_obs_in[index] < 0.0f) ||
          data->flags[index] & 2)  // mantaflow convention: 2 == FlagObstacle
      {
        continue;
      }

      /* get velocities from manta grid space and convert to blender units */
      vel[0] = data->velocity_x[index];
      vel[1] = data->velocity_y[index];
      vel[2] = data->velocity_z[index];
      mul_v3_fl(vel, mds->dx);

      /* convert vel to global space */
      mag = len_v3(vel);
      mul_mat3_m4_v3(mds->obmat, vel);
      normalize_v3(vel);
      mul_v3_fl(vel, mag);

      voxel_center[0] = mds->p0[0] + mds->cell_size[0] * ((float)(x + mds->res_min[0]) + 0.5f);
      voxel_center[1] = mds->p0[1] + mds->cell_size[1] * ((float)(y + mds->res_min[1]) + 0.5f);
      voxel_center[2] = mds->p0[2] + mds->cell_size[2] * ((float)(z + mds->res_min[2]) + 0.5f);
      mul_m4_v3(mds->obmat, voxel_center);

      /* do effectors */
      pd_point_from_loc(data->scene, voxel_center, vel, index, &epoint);
      BKE_effectors_apply(
          data->effectors, NULL, mds->effector_weights, &epoint, retvel, NULL, NULL);

      /* convert retvel to local space */
      mag = len_v3(retvel);
      mul_mat3_m4_v3(mds->imat, retvel);
      normalize_v3(retvel);
      mul_v3_fl(retvel, mag);

      /* constrain forces to interval -1 to 1 */
      data->force_x[index] = min_ff(max_ff(-1.0f, retvel[0] * 0.2f), 1.0f);
      data->force_y[index] = min_ff(max_ff(-1.0f, retvel[1] * 0.2f), 1.0f);
      data->force_z[index] = min_ff(max_ff(-1.0f, retvel[2] * 0.2f), 1.0f);
    }
  }
}

static void update_effectors(
    Depsgraph *depsgraph, Scene *scene, Object *ob, FluidDomainSettings *mds, float UNUSED(dt))
{
  ListBase *effectors;
  /* make sure smoke flow influence is 0.0f */
  mds->effector_weights->weight[PFIELD_FLUIDFLOW] = 0.0f;
  effectors = BKE_effectors_create(depsgraph, ob, NULL, mds->effector_weights);

  if (effectors) {
    // precalculate wind forces
    UpdateEffectorsData data;
    data.scene = scene;
    data.mds = mds;
    data.effectors = effectors;
    data.density = manta_smoke_get_density(mds->fluid);
    data.fuel = manta_smoke_get_fuel(mds->fluid);
    data.force_x = manta_get_force_x(mds->fluid);
    data.force_y = manta_get_force_y(mds->fluid);
    data.force_z = manta_get_force_z(mds->fluid);
    data.velocity_x = manta_get_velocity_x(mds->fluid);
    data.velocity_y = manta_get_velocity_y(mds->fluid);
    data.velocity_z = manta_get_velocity_z(mds->fluid);
    data.flags = manta_smoke_get_flags(mds->fluid);
    data.phi_obs_in = manta_get_phiobs_in(mds->fluid);

    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = 2;
    BLI_task_parallel_range(0, mds->res[0], &data, update_effectors_task_cb, &settings);
  }

  BKE_effectors_free(effectors);
}

static Mesh *create_liquid_geometry(FluidDomainSettings *mds, Mesh *orgmesh, Object *ob)
{
  Mesh *me;
  MVert *mverts;
  MPoly *mpolys;
  MLoop *mloops;
  short *normals, *no_s;
  float no[3];
  float min[3];
  float max[3];
  float size[3];
  float cell_size_scaled[3];

  /* Assign material + flags to new mesh.
   * If there are no faces in original mesh, keep materials and flags unchanged. */
  MPoly *mpoly;
  MPoly mp_example = {0};
  mpoly = orgmesh->mpoly;
  if (mpoly) {
    mp_example = *mpoly;
  }

  const short mp_mat_nr = mp_example.mat_nr;
  const char mp_flag = mp_example.flag;

  int i;
  int num_verts, num_normals, num_faces;

  if (!mds->fluid) {
    return NULL;
  }

  num_verts = manta_liquid_get_num_verts(mds->fluid);
  num_normals = manta_liquid_get_num_normals(mds->fluid);
  num_faces = manta_liquid_get_num_triangles(mds->fluid);

#  ifdef DEBUG_PRINT
  /* Debugging: Print number of vertices, normals, and faces. */
  printf("num_verts: %d, num_normals: %d, num_faces: %d\n", num_verts, num_normals, num_faces);
#  endif

  if (!num_verts || !num_faces) {
    return NULL;
  }
  /* Normals are per vertex, so these must match. */
  BLI_assert(num_verts == num_normals);

  /* If needed, vertex velocities will be read too. */
  bool use_speedvectors = mds->flags & FLUID_DOMAIN_USE_SPEED_VECTORS;
  FluidDomainVertexVelocity *velarray = NULL;
  float time_mult = 25.f * DT_DEFAULT;

  if (use_speedvectors) {
    if (mds->mesh_velocities) {
      MEM_freeN(mds->mesh_velocities);
    }

    mds->mesh_velocities = MEM_calloc_arrayN(
        num_verts, sizeof(FluidDomainVertexVelocity), "fluid_mesh_vertvelocities");
    mds->totvert = num_verts;
    velarray = mds->mesh_velocities;
  }

  me = BKE_mesh_new_nomain(num_verts, 0, 0, num_faces * 3, num_faces);
  if (!me) {
    return NULL;
  }
  mverts = me->mvert;
  mpolys = me->mpoly;
  mloops = me->mloop;

  /* Get size (dimension) but considering scaling scaling. */
  copy_v3_v3(cell_size_scaled, mds->cell_size);
  mul_v3_v3(cell_size_scaled, ob->scale);
  madd_v3fl_v3fl_v3fl_v3i(min, mds->p0, cell_size_scaled, mds->res_min);
  madd_v3fl_v3fl_v3fl_v3i(max, mds->p0, cell_size_scaled, mds->res_max);
  sub_v3_v3v3(size, max, min);

  /* Biggest dimension will be used for upscaling. */
  float max_size = MAX3(size[0], size[1], size[2]);

  float co_scale[3];
  co_scale[0] = max_size / ob->scale[0];
  co_scale[1] = max_size / ob->scale[1];
  co_scale[2] = max_size / ob->scale[2];

  float co_offset[3];
  co_offset[0] = (mds->p0[0] + mds->p1[0]) / 2.0f;
  co_offset[1] = (mds->p0[1] + mds->p1[1]) / 2.0f;
  co_offset[2] = (mds->p0[2] + mds->p1[2]) / 2.0f;

  /* Normals. */
  normals = MEM_callocN(sizeof(short) * num_normals * 3, "Fluidmesh_tmp_normals");

  /* Loop for vertices and normals. */
  for (i = 0, no_s = normals; i < num_verts && i < num_normals; i++, mverts++, no_s += 3) {

    /* Vertices (data is normalized cube around domain origin). */
    mverts->co[0] = manta_liquid_get_vertex_x_at(mds->fluid, i);
    mverts->co[1] = manta_liquid_get_vertex_y_at(mds->fluid, i);
    mverts->co[2] = manta_liquid_get_vertex_z_at(mds->fluid, i);

    /* If reading raw data directly from manta, normalize now (e.g. during replay mode).
     * If reading data from files from disk, omit this normalization. */
    if (!manta_liquid_mesh_from_file(mds->fluid)) {
      // normalize to unit cube around 0
      mverts->co[0] -= ((float)mds->res[0] * mds->mesh_scale) * 0.5f;
      mverts->co[1] -= ((float)mds->res[1] * mds->mesh_scale) * 0.5f;
      mverts->co[2] -= ((float)mds->res[2] * mds->mesh_scale) * 0.5f;
      mverts->co[0] *= mds->dx / mds->mesh_scale;
      mverts->co[1] *= mds->dx / mds->mesh_scale;
      mverts->co[2] *= mds->dx / mds->mesh_scale;
    }

    mul_v3_v3(mverts->co, co_scale);
    add_v3_v3(mverts->co, co_offset);

#  ifdef DEBUG_PRINT
    /* Debugging: Print coordinates of vertices. */
    printf("mverts->co[0]: %f, mverts->co[1]: %f, mverts->co[2]: %f\n",
           mverts->co[0],
           mverts->co[1],
           mverts->co[2]);
#  endif

    /* Normals (data is normalized cube around domain origin). */
    no[0] = manta_liquid_get_normal_x_at(mds->fluid, i);
    no[1] = manta_liquid_get_normal_y_at(mds->fluid, i);
    no[2] = manta_liquid_get_normal_z_at(mds->fluid, i);

    normal_float_to_short_v3(no_s, no);
#  ifdef DEBUG_PRINT
    /* Debugging: Print coordinates of normals. */
    printf("no_s[0]: %d, no_s[1]: %d, no_s[2]: %d\n", no_s[0], no_s[1], no_s[2]);
#  endif

    if (use_speedvectors) {
      velarray[i].vel[0] = manta_liquid_get_vertvel_x_at(mds->fluid, i) * (mds->dx / time_mult);
      velarray[i].vel[1] = manta_liquid_get_vertvel_y_at(mds->fluid, i) * (mds->dx / time_mult);
      velarray[i].vel[2] = manta_liquid_get_vertvel_z_at(mds->fluid, i) * (mds->dx / time_mult);
#  ifdef DEBUG_PRINT
      /* Debugging: Print velocities of vertices. */
      printf("velarray[%d].vel[0]: %f, velarray[%d].vel[1]: %f, velarray[%d].vel[2]: %f\n",
             i,
             velarray[i].vel[0],
             i,
             velarray[i].vel[1],
             i,
             velarray[i].vel[2]);
#  endif
    }
  }

  /* Loop for triangles. */
  for (i = 0; i < num_faces; i++, mpolys++, mloops += 3) {
    /* Initialize from existing face. */
    mpolys->mat_nr = mp_mat_nr;
    mpolys->flag = mp_flag;

    mpolys->loopstart = i * 3;
    mpolys->totloop = 3;

    mloops[0].v = manta_liquid_get_triangle_x_at(mds->fluid, i);
    mloops[1].v = manta_liquid_get_triangle_y_at(mds->fluid, i);
    mloops[2].v = manta_liquid_get_triangle_z_at(mds->fluid, i);
#  ifdef DEBUG_PRINT
    /* Debugging: Print mesh faces. */
    printf("mloops[0].v: %d, mloops[1].v: %d, mloops[2].v: %d\n",
           mloops[0].v,
           mloops[1].v,
           mloops[2].v);
#  endif
  }

  BKE_mesh_ensure_normals(me);
  BKE_mesh_calc_edges(me, false, false);
  BKE_mesh_vert_normals_apply(me, (short(*)[3])normals);

  MEM_freeN(normals);

  return me;
}

static Mesh *create_smoke_geometry(FluidDomainSettings *mds, Mesh *orgmesh, Object *ob)
{
  Mesh *result;
  MVert *mverts;
  MPoly *mpolys;
  MLoop *mloops;
  float min[3];
  float max[3];
  float *co;
  MPoly *mp;
  MLoop *ml;

  int num_verts = 8;
  int num_faces = 6;
  int i;
  float ob_loc[3] = {0};
  float ob_cache_loc[3] = {0};

  /* Just copy existing mesh if there is no content or if the adaptive domain is not being used. */
  if (mds->total_cells <= 1 || (mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) == 0) {
    return BKE_mesh_copy_for_eval(orgmesh, false);
  }

  result = BKE_mesh_new_nomain(num_verts, 0, 0, num_faces * 4, num_faces);
  mverts = result->mvert;
  mpolys = result->mpoly;
  mloops = result->mloop;

  if (num_verts) {
    /* Volume bounds. */
    madd_v3fl_v3fl_v3fl_v3i(min, mds->p0, mds->cell_size, mds->res_min);
    madd_v3fl_v3fl_v3fl_v3i(max, mds->p0, mds->cell_size, mds->res_max);

    /* Set vertices of smoke BB. Especially important, when BB changes (adaptive domain). */
    /* Top slab */
    co = mverts[0].co;
    co[0] = min[0];
    co[1] = min[1];
    co[2] = max[2];
    co = mverts[1].co;
    co[0] = max[0];
    co[1] = min[1];
    co[2] = max[2];
    co = mverts[2].co;
    co[0] = max[0];
    co[1] = max[1];
    co[2] = max[2];
    co = mverts[3].co;
    co[0] = min[0];
    co[1] = max[1];
    co[2] = max[2];
    /* Bottom slab. */
    co = mverts[4].co;
    co[0] = min[0];
    co[1] = min[1];
    co[2] = min[2];
    co = mverts[5].co;
    co[0] = max[0];
    co[1] = min[1];
    co[2] = min[2];
    co = mverts[6].co;
    co[0] = max[0];
    co[1] = max[1];
    co[2] = min[2];
    co = mverts[7].co;
    co[0] = min[0];
    co[1] = max[1];
    co[2] = min[2];

    /* Create faces. */
    /* Top side. */
    mp = &mpolys[0];
    ml = &mloops[0 * 4];
    mp->loopstart = 0 * 4;
    mp->totloop = 4;
    ml[0].v = 0;
    ml[1].v = 1;
    ml[2].v = 2;
    ml[3].v = 3;
    /* Right side. */
    mp = &mpolys[1];
    ml = &mloops[1 * 4];
    mp->loopstart = 1 * 4;
    mp->totloop = 4;
    ml[0].v = 2;
    ml[1].v = 1;
    ml[2].v = 5;
    ml[3].v = 6;
    /* Bottom side. */
    mp = &mpolys[2];
    ml = &mloops[2 * 4];
    mp->loopstart = 2 * 4;
    mp->totloop = 4;
    ml[0].v = 7;
    ml[1].v = 6;
    ml[2].v = 5;
    ml[3].v = 4;
    /* Left side. */
    mp = &mpolys[3];
    ml = &mloops[3 * 4];
    mp->loopstart = 3 * 4;
    mp->totloop = 4;
    ml[0].v = 0;
    ml[1].v = 3;
    ml[2].v = 7;
    ml[3].v = 4;
    /* Front side. */
    mp = &mpolys[4];
    ml = &mloops[4 * 4];
    mp->loopstart = 4 * 4;
    mp->totloop = 4;
    ml[0].v = 3;
    ml[1].v = 2;
    ml[2].v = 6;
    ml[3].v = 7;
    /* Back side. */
    mp = &mpolys[5];
    ml = &mloops[5 * 4];
    mp->loopstart = 5 * 4;
    mp->totloop = 4;
    ml[0].v = 1;
    ml[1].v = 0;
    ml[2].v = 4;
    ml[3].v = 5;

    /* Calculate required shift to match domain's global position
     * it was originally simulated at (if object moves without manta step). */
    invert_m4_m4(ob->imat, ob->obmat);
    mul_m4_v3(ob->obmat, ob_loc);
    mul_m4_v3(mds->obmat, ob_cache_loc);
    sub_v3_v3v3(mds->obj_shift_f, ob_cache_loc, ob_loc);
    /* Convert shift to local space and apply to vertices. */
    mul_mat3_m4_v3(ob->imat, mds->obj_shift_f);
    /* Apply shift to vertices. */
    for (i = 0; i < num_verts; i++) {
      add_v3_v3(mverts[i].co, mds->obj_shift_f);
    }
  }

  BKE_mesh_calc_edges(result, false, false);
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  return result;
}

static int manta_step(
    Depsgraph *depsgraph, Scene *scene, Object *ob, Mesh *me, FluidModifierData *mmd, int frame)
{
  FluidDomainSettings *mds = mmd->domain;
  float dt, frame_length, time_total, time_total_old;
  float time_per_frame;
  bool init_resolution = true;

  /* Store baking success - bake might be aborted anytime by user. */
  int result = 1;
  int mode = mds->cache_type;
  bool mode_replay = (mode == FLUID_DOMAIN_CACHE_REPLAY);

  /* Update object state. */
  invert_m4_m4(mds->imat, ob->obmat);
  copy_m4_m4(mds->obmat, ob->obmat);

  /* Gas domain might use adaptive domain. */
  if (mds->type == FLUID_DOMAIN_TYPE_GAS) {
    init_resolution = (mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) != 0;
  }
  manta_set_domain_from_mesh(mds, ob, me, init_resolution);

  /* Use local variables for adaptive loop, dt can change. */
  frame_length = mds->frame_length;
  dt = mds->dt;
  time_per_frame = 0;
  time_total = mds->time_total;
  /* Keep track of original total time to correct small errors at end of step. */
  time_total_old = mds->time_total;

  BLI_mutex_lock(&object_update_lock);

  /* Loop as long as time_per_frame (sum of sub dt's) does not exceed actual framelength. */
  while (time_per_frame + FLT_EPSILON < frame_length) {
    manta_adapt_timestep(mds->fluid);
    dt = manta_get_timestep(mds->fluid);

    /* Save adapted dt so that MANTA object can access it (important when adaptive domain creates
     * new MANTA object). */
    mds->dt = dt;

    /* Calculate inflow geometry. */
    update_flowsfluids(depsgraph, scene, ob, mds, time_per_frame, frame_length, frame, dt);

    /* If user requested stop, quit baking */
    if (G.is_break && !mode_replay) {
      result = 0;
      break;
    }

    manta_update_variables(mds->fluid, mmd);

    /* Calculate obstacle geometry. */
    update_obstacles(depsgraph, scene, ob, mds, time_per_frame, frame_length, frame, dt);

    /* If user requested stop, quit baking */
    if (G.is_break && !mode_replay) {
      result = 0;
      break;
    }

    /* Only bake if the domain is bigger than one cell (important for adaptive domain). */
    if (mds->total_cells > 1) {
      update_effectors(depsgraph, scene, ob, mds, dt);
      manta_bake_data(mds->fluid, mmd, frame);
    }

    /* Count for how long this while loop is running. */
    time_per_frame += dt;
    time_total += dt;

    mds->time_per_frame = time_per_frame;
    mds->time_total = time_total;
  }
  /* Total time must not exceed framecount times framelength. Correct tiny errors here. */
  CLAMP(mds->time_total, mds->time_total, time_total_old + mds->frame_length);

  if (mds->type == FLUID_DOMAIN_TYPE_GAS && result) {
    manta_smoke_calc_transparency(mds, DEG_get_evaluated_view_layer(depsgraph));
  }
  BLI_mutex_unlock(&object_update_lock);

  return result;
}

static void manta_guiding(
    Depsgraph *depsgraph, Scene *scene, Object *ob, FluidModifierData *mmd, int frame)
{
  FluidDomainSettings *mds = mmd->domain;
  float fps = scene->r.frs_sec / scene->r.frs_sec_base;
  float dt = DT_DEFAULT * (25.0f / fps) * mds->time_scale;

  BLI_mutex_lock(&object_update_lock);

  update_obstacles(depsgraph, scene, ob, mds, dt, dt, frame, dt);
  manta_bake_guiding(mds->fluid, mmd, frame);

  BLI_mutex_unlock(&object_update_lock);
}

static void BKE_fluid_modifier_processFlow(FluidModifierData *mmd,
                                           Depsgraph *depsgraph,
                                           Scene *scene,
                                           Object *ob,
                                           Mesh *me,
                                           const int scene_framenr)
{
  if (scene_framenr >= mmd->time) {
    BKE_fluid_modifier_init(mmd, depsgraph, ob, scene, me);
  }

  if (mmd->flow) {
    if (mmd->flow->mesh) {
      BKE_id_free(NULL, mmd->flow->mesh);
    }
    mmd->flow->mesh = BKE_mesh_copy_for_eval(me, false);
  }

  if (scene_framenr > mmd->time) {
    mmd->time = scene_framenr;
  }
  else if (scene_framenr < mmd->time) {
    mmd->time = scene_framenr;
    BKE_fluid_modifier_reset_ex(mmd, false);
  }
}

static void BKE_fluid_modifier_processEffector(FluidModifierData *mmd,
                                               Depsgraph *depsgraph,
                                               Scene *scene,
                                               Object *ob,
                                               Mesh *me,
                                               const int scene_framenr)
{
  if (scene_framenr >= mmd->time) {
    BKE_fluid_modifier_init(mmd, depsgraph, ob, scene, me);
  }

  if (mmd->effector) {
    if (mmd->effector->mesh) {
      BKE_id_free(NULL, mmd->effector->mesh);
    }
    mmd->effector->mesh = BKE_mesh_copy_for_eval(me, false);
  }

  if (scene_framenr > mmd->time) {
    mmd->time = scene_framenr;
  }
  else if (scene_framenr < mmd->time) {
    mmd->time = scene_framenr;
    BKE_fluid_modifier_reset_ex(mmd, false);
  }
}

static void BKE_fluid_modifier_processDomain(FluidModifierData *mmd,
                                             Depsgraph *depsgraph,
                                             Scene *scene,
                                             Object *ob,
                                             Mesh *me,
                                             const int scene_framenr)
{
  FluidDomainSettings *mds = mmd->domain;
  Object *guide_parent = NULL;
  Object **objs = NULL;
  uint numobj = 0;
  FluidModifierData *mmd_parent = NULL;

  bool is_startframe, has_advanced;
  is_startframe = (scene_framenr == mds->cache_frame_start);
  has_advanced = (scene_framenr == mmd->time + 1);

  /* Do not process modifier if current frame is out of cache range. */
  if (scene_framenr < mds->cache_frame_start || scene_framenr > mds->cache_frame_end) {
    return;
  }

  /* Reset fluid if no fluid present. Also resets active fields. */
  if (!mds->fluid) {
    BKE_fluid_modifier_reset_ex(mmd, false);
  }

  /* Ensure cache directory is not relative. */
  const char *relbase = BKE_modifier_path_relbase_from_global(ob);
  BLI_path_abs(mds->cache_directory, relbase);

  /* Ensure that all flags are up to date before doing any baking and/or cache reading. */
  objs = BKE_collision_objects_create(
      depsgraph, ob, mds->fluid_group, &numobj, eModifierType_Fluid);
  update_flowsflags(mds, objs, numobj);
  if (objs) {
    MEM_freeN(objs);
  }
  objs = BKE_collision_objects_create(
      depsgraph, ob, mds->effector_group, &numobj, eModifierType_Fluid);
  update_obstacleflags(mds, objs, numobj);
  if (objs) {
    MEM_freeN(objs);
  }

  /* TODO (sebbas): Cache reset for when flow / effector object need update flag is set. */
#  if 0
  /* If the just updated flags now carry the 'outdated' flag, reset the cache here!
   * Plus sanity check: Do not clear cache on file load. */
  if (mds->cache_flag & FLUID_DOMAIN_OUTDATED_DATA &&
      ((mds->flags & FLUID_DOMAIN_FILE_LOAD) == 0)) {
    BKE_fluid_cache_free_all(mds, ob);
    BKE_fluid_modifier_reset_ex(mmd, false);
  }
#  endif

  /* Fluid domain init must not fail in order to continue modifier evaluation. */
  if (!mds->fluid && !BKE_fluid_modifier_init(mmd, depsgraph, ob, scene, me)) {
    CLOG_ERROR(&LOG, "Fluid initialization failed. Should not happen!");
    return;
  }
  BLI_assert(mds->fluid);

  /* Guiding parent res pointer needs initialization. */
  guide_parent = mds->guide_parent;
  if (guide_parent) {
    mmd_parent = (FluidModifierData *)BKE_modifiers_findby_type(guide_parent, eModifierType_Fluid);
    if (mmd_parent && mmd_parent->domain) {
      copy_v3_v3_int(mds->guide_res, mmd_parent->domain->res);
    }
  }

  /* Ensure that time parameters are initialized correctly before every step. */
  float fps = scene->r.frs_sec / scene->r.frs_sec_base;
  mds->frame_length = DT_DEFAULT * (25.0f / fps) * mds->time_scale;
  mds->dt = mds->frame_length;
  mds->time_per_frame = 0;

  /* Ensure that gravity is copied over every frame (could be keyframed). */
  if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
    copy_v3_v3(mds->gravity, scene->physics_settings.gravity);
    mul_v3_fl(mds->gravity, mds->effector_weights->global_gravity);
  }

  int next_frame = scene_framenr + 1;
  int prev_frame = scene_framenr - 1;
  /* Ensure positivity of previous frame. */
  CLAMP(prev_frame, mds->cache_frame_start, prev_frame);

  int data_frame = scene_framenr, noise_frame = scene_framenr;
  int mesh_frame = scene_framenr, particles_frame = scene_framenr, guide_frame = scene_framenr;

  bool with_smoke, with_liquid;
  with_smoke = mds->type == FLUID_DOMAIN_TYPE_GAS;
  with_liquid = mds->type == FLUID_DOMAIN_TYPE_LIQUID;

  bool drops, bubble, floater;
  drops = mds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY;
  bubble = mds->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE;
  floater = mds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM;

  bool with_script, with_adaptive, with_noise, with_mesh, with_particles, with_guide;
  with_script = mds->flags & FLUID_DOMAIN_EXPORT_MANTA_SCRIPT;
  with_adaptive = mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN;
  with_noise = mds->flags & FLUID_DOMAIN_USE_NOISE;
  with_mesh = mds->flags & FLUID_DOMAIN_USE_MESH;
  with_guide = mds->flags & FLUID_DOMAIN_USE_GUIDE;
  with_particles = drops || bubble || floater;

  bool has_data, has_noise, has_mesh, has_particles, has_guide, has_config;
  has_data = manta_has_data(mds->fluid, mmd, scene_framenr);
  has_noise = manta_has_noise(mds->fluid, mmd, scene_framenr);
  has_mesh = manta_has_mesh(mds->fluid, mmd, scene_framenr);
  has_particles = manta_has_particles(mds->fluid, mmd, scene_framenr);
  has_guide = manta_has_guiding(mds->fluid, mmd, scene_framenr, guide_parent);
  has_config = false;

  bool baking_data, baking_noise, baking_mesh, baking_particles, baking_guide;
  baking_data = mds->cache_flag & FLUID_DOMAIN_BAKING_DATA;
  baking_noise = mds->cache_flag & FLUID_DOMAIN_BAKING_NOISE;
  baking_mesh = mds->cache_flag & FLUID_DOMAIN_BAKING_MESH;
  baking_particles = mds->cache_flag & FLUID_DOMAIN_BAKING_PARTICLES;
  baking_guide = mds->cache_flag & FLUID_DOMAIN_BAKING_GUIDE;

  bool resume_data, resume_noise, resume_mesh, resume_particles, resume_guide;
  resume_data = (!is_startframe) && (mds->cache_frame_pause_data == scene_framenr);
  resume_noise = (!is_startframe) && (mds->cache_frame_pause_noise == scene_framenr);
  resume_mesh = (!is_startframe) && (mds->cache_frame_pause_mesh == scene_framenr);
  resume_particles = (!is_startframe) && (mds->cache_frame_pause_particles == scene_framenr);
  resume_guide = (!is_startframe) && (mds->cache_frame_pause_guide == scene_framenr);

  bool read_cache, bake_cache;
  read_cache = false;
  bake_cache = baking_data || baking_noise || baking_mesh || baking_particles || baking_guide;

  bool next_data, next_noise, next_mesh, next_particles, next_guide;
  next_data = manta_has_data(mds->fluid, mmd, next_frame);
  next_noise = manta_has_noise(mds->fluid, mmd, next_frame);
  next_mesh = manta_has_mesh(mds->fluid, mmd, next_frame);
  next_particles = manta_has_particles(mds->fluid, mmd, next_frame);
  next_guide = manta_has_guiding(mds->fluid, mmd, next_frame, guide_parent);

  bool prev_data, prev_noise, prev_mesh, prev_particles, prev_guide;
  prev_data = manta_has_data(mds->fluid, mmd, prev_frame);
  prev_noise = manta_has_noise(mds->fluid, mmd, prev_frame);
  prev_mesh = manta_has_mesh(mds->fluid, mmd, prev_frame);
  prev_particles = manta_has_particles(mds->fluid, mmd, prev_frame);
  prev_guide = manta_has_guiding(mds->fluid, mmd, prev_frame, guide_parent);

  /* Unused for now. */
  UNUSED_VARS(has_guide, prev_guide, next_mesh, next_guide);

  bool with_gdomain;
  with_gdomain = (mds->guide_source == FLUID_DOMAIN_GUIDE_SRC_DOMAIN);

  int o_res[3], o_min[3], o_max[3], o_shift[3];
  int mode = mds->cache_type;

  /* Cache mode specific settings. */
  switch (mode) {
    case FLUID_DOMAIN_CACHE_FINAL:
      /* Just load the data that has already been baked */
      if (!baking_data && !baking_noise && !baking_mesh && !baking_particles && !baking_guide) {
        read_cache = true;
        bake_cache = false;
      }
      break;
    case FLUID_DOMAIN_CACHE_MODULAR:
      /* Just load the data that has already been baked */
      if (!baking_data && !baking_noise && !baking_mesh && !baking_particles && !baking_guide) {
        read_cache = true;
        bake_cache = false;
        break;
      }

      /* Set to previous frame if the bake was resumed
       * ie don't read all of the already baked frames, just the one before bake resumes */
      if (baking_data && resume_data) {
        data_frame = prev_frame;
      }
      if (baking_noise && resume_noise) {
        noise_frame = prev_frame;
      }
      if (baking_mesh && resume_mesh) {
        mesh_frame = prev_frame;
      }
      if (baking_particles && resume_particles) {
        particles_frame = prev_frame;
      }
      if (baking_guide && resume_guide) {
        guide_frame = prev_frame;
      }

      /* Noise, mesh and particles can never be baked more than data. */
      CLAMP(noise_frame, noise_frame, data_frame);
      CLAMP(mesh_frame, mesh_frame, data_frame);
      CLAMP(particles_frame, particles_frame, data_frame);
      CLAMP(guide_frame, guide_frame, mds->cache_frame_end);

      /* Force to read cache as we're resuming the bake */
      read_cache = true;
      break;
    case FLUID_DOMAIN_CACHE_REPLAY:
    default:
      baking_data = !has_data && (is_startframe || prev_data);
      if (with_smoke && with_noise) {
        baking_noise = !has_noise && (is_startframe || prev_noise);
      }
      if (with_liquid && with_mesh) {
        baking_mesh = !has_mesh && (is_startframe || prev_mesh);
      }
      if (with_liquid && with_particles) {
        baking_particles = !has_particles && (is_startframe || prev_particles);
      }

      /* Always trying to read the cache in replay mode. */
      read_cache = true;
      bake_cache = false;
      break;
  }

  /* Try to read from cache and keep track of read success. */
  if (read_cache) {

    /* Read mesh cache. */
    if (with_liquid && with_mesh) {
      has_config = manta_read_config(mds->fluid, mmd, mesh_frame);

      /* Update mesh data from file is faster than via Python (manta_read_mesh()). */
      has_mesh = manta_update_mesh_structures(mds->fluid, mmd, mesh_frame);
    }

    /* Read particles cache. */
    if (with_liquid && with_particles) {
      has_config = manta_read_config(mds->fluid, mmd, particles_frame);

      if (!baking_data && !baking_particles && next_particles) {
        /* Update particle data from file is faster than via Python (manta_read_particles()). */
        has_particles = manta_update_particle_structures(mds->fluid, mmd, particles_frame);
      }
      else {
        has_particles = manta_read_particles(mds->fluid, mmd, particles_frame);
      }
    }

    /* Read guide cache. */
    if (with_guide) {
      FluidModifierData *mmd2 = (with_gdomain) ? mmd_parent : mmd;
      has_guide = manta_read_guiding(mds->fluid, mmd2, scene_framenr, with_gdomain);
    }

    /* Read noise and data cache */
    if (with_smoke && with_noise) {
      has_config = manta_read_config(mds->fluid, mmd, noise_frame);

      /* Only reallocate when just reading cache or when resuming during bake. */
      if ((!baking_noise || (baking_noise && resume_noise)) && has_config &&
          manta_needs_realloc(mds->fluid, mmd)) {
        BKE_fluid_reallocate_fluid(mds, mds->res, 1);
      }
      if (!baking_data && !baking_noise && next_noise) {
        has_noise = manta_update_noise_structures(mds->fluid, mmd, noise_frame);
      }
      else {
        has_noise = manta_read_noise(mds->fluid, mmd, noise_frame);
      }

      /* When using the adaptive domain, copy all data that was read to a new fluid object. */
      if (with_adaptive && baking_noise) {
        /* Adaptive domain needs to know about current state, so save it, then copy. */
        copy_v3_v3_int(o_res, mds->res);
        copy_v3_v3_int(o_min, mds->res_min);
        copy_v3_v3_int(o_max, mds->res_max);
        copy_v3_v3_int(o_shift, mds->shift);
        if (has_config && manta_needs_realloc(mds->fluid, mmd)) {
          BKE_fluid_reallocate_copy_fluid(
              mds, o_res, mds->res, o_min, mds->res_min, o_max, o_shift, mds->shift);
        }
      }
      if (!baking_data && !baking_noise && next_data && next_noise) {
        /* Nothing to do here since we already loaded noise grids. */
      }
      else {
        has_data = manta_read_data(mds->fluid, mmd, data_frame);
      }
    }
    /* Read data cache only */
    else {
      has_config = manta_read_config(mds->fluid, mmd, data_frame);

      if (with_smoke) {
        /* Read config and realloc fluid object if needed. */
        if (has_config && manta_needs_realloc(mds->fluid, mmd)) {
          BKE_fluid_reallocate_fluid(mds, mds->res, 1);
        }
        /* Read data cache */
        if (!baking_data && !baking_particles && !baking_mesh && next_data) {
          has_data = manta_update_smoke_structures(mds->fluid, mmd, data_frame);
        }
        else {
          has_data = manta_read_data(mds->fluid, mmd, data_frame);
        }
      }
      if (with_liquid) {
        if (!baking_data && !baking_particles && !baking_mesh && next_data) {
          has_data = manta_update_liquid_structures(mds->fluid, mmd, data_frame);
        }
        else {
          has_data = manta_read_data(mds->fluid, mmd, data_frame);
        }
      }
    }
  }

  /* Cache mode specific settings */
  switch (mode) {
    case FLUID_DOMAIN_CACHE_FINAL:
    case FLUID_DOMAIN_CACHE_MODULAR:
      if (!baking_data && !baking_noise && !baking_mesh && !baking_particles && !baking_guide) {
        bake_cache = false;
      }
      break;
    case FLUID_DOMAIN_CACHE_REPLAY:
    default:
      baking_data = !has_data && (is_startframe || prev_data);
      if (with_smoke && with_noise) {
        baking_noise = !has_noise && (is_startframe || prev_noise);
      }
      if (with_liquid && with_mesh) {
        baking_mesh = !has_mesh && (is_startframe || prev_mesh);
      }
      if (with_liquid && with_particles) {
        baking_particles = !has_particles && (is_startframe || prev_particles);
      }

      /* Only bake if time advanced by one frame. */
      if (is_startframe || has_advanced) {
        bake_cache = baking_data || baking_noise || baking_mesh || baking_particles;
      }
      break;
  }

  /* Trigger bake calls individually */
  if (bake_cache) {
    /* Ensure fresh variables at every animation step */
    manta_update_variables(mds->fluid, mmd);

    /* Export mantaflow python script on first frame (once only) and for any bake type */
    if (with_script && is_startframe) {
      if (with_smoke) {
        manta_smoke_export_script(mmd->domain->fluid, mmd);
      }
      if (with_liquid) {
        manta_liquid_export_script(mmd->domain->fluid, mmd);
      }
    }

    if (baking_guide && with_guide) {
      manta_guiding(depsgraph, scene, ob, mmd, scene_framenr);
    }
    if (baking_data) {
      /* Only save baked data if all of it completed successfully. */
      if (manta_step(depsgraph, scene, ob, me, mmd, scene_framenr)) {
        manta_write_config(mds->fluid, mmd, scene_framenr);
        manta_write_data(mds->fluid, mmd, scene_framenr);
      }
    }
    if (has_data || baking_data) {
      if (baking_noise && with_smoke && with_noise) {
        /* Ensure that no bake occurs if domain was minimized by adaptive domain. */
        if (mds->total_cells > 1) {
          manta_bake_noise(mds->fluid, mmd, scene_framenr);
        }
        manta_write_noise(mds->fluid, mmd, scene_framenr);
      }
      if (baking_mesh && with_liquid && with_mesh) {
        manta_bake_mesh(mds->fluid, mmd, scene_framenr);
      }
      if (baking_particles && with_liquid && with_particles) {
        manta_bake_particles(mds->fluid, mmd, scene_framenr);
      }
    }
  }

  mds->flags &= ~FLUID_DOMAIN_FILE_LOAD;
  mmd->time = scene_framenr;
}

static void BKE_fluid_modifier_process(
    FluidModifierData *mmd, Depsgraph *depsgraph, Scene *scene, Object *ob, Mesh *me)
{
  const int scene_framenr = (int)DEG_get_ctime(depsgraph);

  if ((mmd->type & MOD_FLUID_TYPE_FLOW)) {
    BKE_fluid_modifier_processFlow(mmd, depsgraph, scene, ob, me, scene_framenr);
  }
  else if (mmd->type & MOD_FLUID_TYPE_EFFEC) {
    BKE_fluid_modifier_processEffector(mmd, depsgraph, scene, ob, me, scene_framenr);
  }
  else if (mmd->type & MOD_FLUID_TYPE_DOMAIN) {
    BKE_fluid_modifier_processDomain(mmd, depsgraph, scene, ob, me, scene_framenr);
  }
}

struct Mesh *BKE_fluid_modifier_do(
    FluidModifierData *mmd, Depsgraph *depsgraph, Scene *scene, Object *ob, Mesh *me)
{
  /* Lock so preview render does not read smoke data while it gets modified. */
  if ((mmd->type & MOD_FLUID_TYPE_DOMAIN) && mmd->domain) {
    BLI_rw_mutex_lock(mmd->domain->fluid_mutex, THREAD_LOCK_WRITE);
  }

  BKE_fluid_modifier_process(mmd, depsgraph, scene, ob, me);

  if ((mmd->type & MOD_FLUID_TYPE_DOMAIN) && mmd->domain) {
    BLI_rw_mutex_unlock(mmd->domain->fluid_mutex);
  }

  /* Optimization: Do not update viewport during bakes (except in replay mode)
   * Reason: UI is locked and updated liquid / smoke geometry is not visible anyways. */
  bool needs_viewport_update = false;
  if (mmd->domain) {
    FluidDomainSettings *mds = mmd->domain;

    /* Always update viewport in cache replay mode. */
    if (mds->cache_type == FLUID_DOMAIN_CACHE_REPLAY ||
        mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) {
      needs_viewport_update = true;
    }
    /* In other cache modes, only update the viewport when no bake is going on. */
    else {
      bool with_mesh;
      with_mesh = mds->flags & FLUID_DOMAIN_USE_MESH;
      bool baking_data, baking_noise, baking_mesh, baking_particles, baking_guide;
      baking_data = mds->cache_flag & FLUID_DOMAIN_BAKING_DATA;
      baking_noise = mds->cache_flag & FLUID_DOMAIN_BAKING_NOISE;
      baking_mesh = mds->cache_flag & FLUID_DOMAIN_BAKING_MESH;
      baking_particles = mds->cache_flag & FLUID_DOMAIN_BAKING_PARTICLES;
      baking_guide = mds->cache_flag & FLUID_DOMAIN_BAKING_GUIDE;

      if (with_mesh && !baking_data && !baking_noise && !baking_mesh && !baking_particles &&
          !baking_guide) {
        needs_viewport_update = true;
      }
    }
  }

  Mesh *result = NULL;
  if (mmd->type & MOD_FLUID_TYPE_DOMAIN && mmd->domain) {
    if (needs_viewport_update) {
      /* Return generated geometry depending on domain type. */
      if (mmd->domain->type == FLUID_DOMAIN_TYPE_LIQUID) {
        result = create_liquid_geometry(mmd->domain, me, ob);
      }
      if (mmd->domain->type == FLUID_DOMAIN_TYPE_GAS) {
        result = create_smoke_geometry(mmd->domain, me, ob);
      }
    }

    /* Clear flag outside of locked block (above). */
    mmd->domain->cache_flag &= ~FLUID_DOMAIN_OUTDATED_DATA;
    mmd->domain->cache_flag &= ~FLUID_DOMAIN_OUTDATED_NOISE;
    mmd->domain->cache_flag &= ~FLUID_DOMAIN_OUTDATED_MESH;
    mmd->domain->cache_flag &= ~FLUID_DOMAIN_OUTDATED_PARTICLES;
    mmd->domain->cache_flag &= ~FLUID_DOMAIN_OUTDATED_GUIDE;
  }

  if (!result) {
    result = BKE_mesh_copy_for_eval(me, false);
  }
  else {
    BKE_mesh_copy_settings(result, me);
  }

  /* Liquid simulation has a texture space that based on the bounds of the fluid mesh.
   * This does not seem particularly useful, but it's backwards compatible.
   *
   * Smoke simulation needs a texture space relative to the adaptive domain bounds, not the
   * original mesh. So recompute it at this point in the modifier stack. See T58492. */
  BKE_mesh_texspace_calc(result);

  return result;
}

static float calc_voxel_transp(
    float *result, float *input, int res[3], int *pixel, float *t_ray, float correct)
{
  const size_t index = manta_get_index(pixel[0], res[0], pixel[1], res[1], pixel[2]);

  // T_ray *= T_vox
  *t_ray *= expf(input[index] * correct);

  if (result[index] < 0.0f) {
    result[index] = *t_ray;
  }

  return *t_ray;
}

static void bresenham_linie_3D(int x1,
                               int y1,
                               int z1,
                               int x2,
                               int y2,
                               int z2,
                               float *t_ray,
                               BKE_Fluid_BresenhamFn cb,
                               float *result,
                               float *input,
                               int res[3],
                               float correct)
{
  int dx, dy, dz, i, l, m, n, x_inc, y_inc, z_inc, err_1, err_2, dx2, dy2, dz2;
  int pixel[3];

  pixel[0] = x1;
  pixel[1] = y1;
  pixel[2] = z1;

  dx = x2 - x1;
  dy = y2 - y1;
  dz = z2 - z1;

  x_inc = (dx < 0) ? -1 : 1;
  l = abs(dx);
  y_inc = (dy < 0) ? -1 : 1;
  m = abs(dy);
  z_inc = (dz < 0) ? -1 : 1;
  n = abs(dz);
  dx2 = l << 1;
  dy2 = m << 1;
  dz2 = n << 1;

  if ((l >= m) && (l >= n)) {
    err_1 = dy2 - l;
    err_2 = dz2 - l;
    for (i = 0; i < l; i++) {
      if (cb(result, input, res, pixel, t_ray, correct) <= FLT_EPSILON) {
        break;
      }
      if (err_1 > 0) {
        pixel[1] += y_inc;
        err_1 -= dx2;
      }
      if (err_2 > 0) {
        pixel[2] += z_inc;
        err_2 -= dx2;
      }
      err_1 += dy2;
      err_2 += dz2;
      pixel[0] += x_inc;
    }
  }
  else if ((m >= l) && (m >= n)) {
    err_1 = dx2 - m;
    err_2 = dz2 - m;
    for (i = 0; i < m; i++) {
      if (cb(result, input, res, pixel, t_ray, correct) <= FLT_EPSILON) {
        break;
      }
      if (err_1 > 0) {
        pixel[0] += x_inc;
        err_1 -= dy2;
      }
      if (err_2 > 0) {
        pixel[2] += z_inc;
        err_2 -= dy2;
      }
      err_1 += dx2;
      err_2 += dz2;
      pixel[1] += y_inc;
    }
  }
  else {
    err_1 = dy2 - n;
    err_2 = dx2 - n;
    for (i = 0; i < n; i++) {
      if (cb(result, input, res, pixel, t_ray, correct) <= FLT_EPSILON) {
        break;
      }
      if (err_1 > 0) {
        pixel[1] += y_inc;
        err_1 -= dz2;
      }
      if (err_2 > 0) {
        pixel[0] += x_inc;
        err_2 -= dz2;
      }
      err_1 += dy2;
      err_2 += dx2;
      pixel[2] += z_inc;
    }
  }
  cb(result, input, res, pixel, t_ray, correct);
}

static void manta_smoke_calc_transparency(FluidDomainSettings *mds, ViewLayer *view_layer)
{
  float bv[6] = {0};
  float light[3];
  int a, z, slabsize = mds->res[0] * mds->res[1], size = mds->res[0] * mds->res[1] * mds->res[2];
  float *density = manta_smoke_get_density(mds->fluid);
  float *shadow = manta_smoke_get_shadow(mds->fluid);
  float correct = -7.0f * mds->dx;

  if (!get_light(view_layer, light)) {
    return;
  }

  /* convert light pos to sim cell space */
  mul_m4_v3(mds->imat, light);
  light[0] = (light[0] - mds->p0[0]) / mds->cell_size[0] - 0.5f - (float)mds->res_min[0];
  light[1] = (light[1] - mds->p0[1]) / mds->cell_size[1] - 0.5f - (float)mds->res_min[1];
  light[2] = (light[2] - mds->p0[2]) / mds->cell_size[2] - 0.5f - (float)mds->res_min[2];

  for (a = 0; a < size; a++) {
    shadow[a] = -1.0f;
  }

  /* calculate domain bounds in sim cell space */
  // 0,2,4 = 0.0f
  bv[1] = (float)mds->res[0];  // x
  bv[3] = (float)mds->res[1];  // y
  bv[5] = (float)mds->res[2];  // z

  for (z = 0; z < mds->res[2]; z++) {
    size_t index = z * slabsize;
    int x, y;

    for (y = 0; y < mds->res[1]; y++) {
      for (x = 0; x < mds->res[0]; x++, index++) {
        float voxel_center[3];
        float pos[3];
        int cell[3];
        float t_ray = 1.0;

        if (shadow[index] >= 0.0f) {
          continue;
        }
        voxel_center[0] = (float)x;
        voxel_center[1] = (float)y;
        voxel_center[2] = (float)z;

        // get starting cell (light pos)
        if (BLI_bvhtree_bb_raycast(bv, light, voxel_center, pos) > FLT_EPSILON) {
          // we're outside -> use point on side of domain
          cell[0] = (int)floor(pos[0]);
          cell[1] = (int)floor(pos[1]);
          cell[2] = (int)floor(pos[2]);
        }
        else {
          // we're inside -> use light itself
          cell[0] = (int)floor(light[0]);
          cell[1] = (int)floor(light[1]);
          cell[2] = (int)floor(light[2]);
        }
        /* clamp within grid bounds */
        CLAMP(cell[0], 0, mds->res[0] - 1);
        CLAMP(cell[1], 0, mds->res[1] - 1);
        CLAMP(cell[2], 0, mds->res[2] - 1);

        bresenham_linie_3D(cell[0],
                           cell[1],
                           cell[2],
                           x,
                           y,
                           z,
                           &t_ray,
                           calc_voxel_transp,
                           shadow,
                           density,
                           mds->res,
                           correct);

        // convention -> from a RGBA float array, use G value for t_ray
        shadow[index] = t_ray;
      }
    }
  }
}

/* Get fluid velocity and density at given coordinates
 * Returns fluid density or -1.0f if outside domain. */
float BKE_fluid_get_velocity_at(struct Object *ob, float position[3], float velocity[3])
{
  FluidModifierData *mmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  zero_v3(velocity);

  if (mmd && (mmd->type & MOD_FLUID_TYPE_DOMAIN) && mmd->domain && mmd->domain->fluid) {
    FluidDomainSettings *mds = mmd->domain;
    float time_mult = 25.f * DT_DEFAULT;
    float size_mult = MAX3(mds->global_size[0], mds->global_size[1], mds->global_size[2]) /
                      mds->maxres;
    float vel_mag;
    float density = 0.0f, fuel = 0.0f;
    float pos[3];
    copy_v3_v3(pos, position);
    manta_pos_to_cell(mds, pos);

    /* Check if position is outside domain max bounds. */
    if (pos[0] < mds->res_min[0] || pos[1] < mds->res_min[1] || pos[2] < mds->res_min[2]) {
      return -1.0f;
    }
    if (pos[0] > mds->res_max[0] || pos[1] > mds->res_max[1] || pos[2] > mds->res_max[2]) {
      return -1.0f;
    }

    /* map pos between 0.0 - 1.0 */
    pos[0] = (pos[0] - mds->res_min[0]) / ((float)mds->res[0]);
    pos[1] = (pos[1] - mds->res_min[1]) / ((float)mds->res[1]);
    pos[2] = (pos[2] - mds->res_min[2]) / ((float)mds->res[2]);

    /* Check if position is outside active area. */
    if (mds->type == FLUID_DOMAIN_TYPE_GAS && mds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) {
      if (pos[0] < 0.0f || pos[1] < 0.0f || pos[2] < 0.0f) {
        return 0.0f;
      }
      if (pos[0] > 1.0f || pos[1] > 1.0f || pos[2] > 1.0f) {
        return 0.0f;
      }
    }

    /* Get interpolated velocity at given position. */
    velocity[0] = BLI_voxel_sample_trilinear(manta_get_velocity_x(mds->fluid), mds->res, pos);
    velocity[1] = BLI_voxel_sample_trilinear(manta_get_velocity_y(mds->fluid), mds->res, pos);
    velocity[2] = BLI_voxel_sample_trilinear(manta_get_velocity_z(mds->fluid), mds->res, pos);

    /* Convert simulation units to Blender units. */
    mul_v3_fl(velocity, size_mult);
    mul_v3_fl(velocity, time_mult);

    /* Convert velocity direction to global space. */
    vel_mag = len_v3(velocity);
    mul_mat3_m4_v3(mds->obmat, velocity);
    normalize_v3(velocity);
    mul_v3_fl(velocity, vel_mag);

    /* Use max value of fuel or smoke density. */
    density = BLI_voxel_sample_trilinear(manta_smoke_get_density(mds->fluid), mds->res, pos);
    if (manta_smoke_has_fuel(mds->fluid)) {
      fuel = BLI_voxel_sample_trilinear(manta_smoke_get_fuel(mds->fluid), mds->res, pos);
    }
    return MAX2(density, fuel);
  }
  return -1.0f;
}

int BKE_fluid_get_data_flags(FluidDomainSettings *mds)
{
  int flags = 0;

  if (mds->fluid) {
    if (manta_smoke_has_heat(mds->fluid)) {
      flags |= FLUID_DOMAIN_ACTIVE_HEAT;
    }
    if (manta_smoke_has_fuel(mds->fluid)) {
      flags |= FLUID_DOMAIN_ACTIVE_FIRE;
    }
    if (manta_smoke_has_colors(mds->fluid)) {
      flags |= FLUID_DOMAIN_ACTIVE_COLORS;
    }
  }

  return flags;
}

void BKE_fluid_particle_system_create(struct Main *bmain,
                                      struct Object *ob,
                                      const char *pset_name,
                                      const char *parts_name,
                                      const char *psys_name,
                                      const int psys_type)
{
  ParticleSystem *psys;
  ParticleSettings *part;
  ParticleSystemModifierData *pmmd;

  /* add particle system */
  part = BKE_particlesettings_add(bmain, pset_name);
  psys = MEM_callocN(sizeof(ParticleSystem), "particle_system");

  part->type = psys_type;
  part->totpart = 0;
  part->draw_size = 0.01f; /* Make fluid particles more subtle in viewport. */
  part->draw_col = PART_DRAW_COL_VEL;
  psys->part = part;
  psys->pointcache = BKE_ptcache_add(&psys->ptcaches);
  BLI_strncpy(psys->name, parts_name, sizeof(psys->name));
  BLI_addtail(&ob->particlesystem, psys);

  /* add modifier */
  pmmd = (ParticleSystemModifierData *)BKE_modifier_new(eModifierType_ParticleSystem);
  BLI_strncpy(pmmd->modifier.name, psys_name, sizeof(pmmd->modifier.name));
  pmmd->psys = psys;
  BLI_addtail(&ob->modifiers, pmmd);
  BKE_modifier_unique_name(&ob->modifiers, (ModifierData *)pmmd);
}

void BKE_fluid_particle_system_destroy(struct Object *ob, const int particle_type)
{
  ParticleSystemModifierData *pmmd;
  ParticleSystem *psys, *next_psys;

  for (psys = ob->particlesystem.first; psys; psys = next_psys) {
    next_psys = psys->next;
    if (psys->part->type == particle_type) {
      /* clear modifier */
      pmmd = psys_get_modifier(ob, psys);
      BLI_remlink(&ob->modifiers, pmmd);
      BKE_modifier_free((ModifierData *)pmmd);

      /* clear particle system */
      BLI_remlink(&ob->particlesystem, psys);
      psys_free(ob, psys);
    }
  }
}

#endif /* WITH_FLUID */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Data Access API
 *
 * Use for versioning, even when fluids are disabled.
 * \{ */

void BKE_fluid_cache_startframe_set(FluidDomainSettings *settings, int value)
{
  settings->cache_frame_start = (value > settings->cache_frame_end) ? settings->cache_frame_end :
                                                                      value;
}

void BKE_fluid_cache_endframe_set(FluidDomainSettings *settings, int value)
{
  settings->cache_frame_end = (value < settings->cache_frame_start) ? settings->cache_frame_start :
                                                                      value;
}

void BKE_fluid_cachetype_mesh_set(FluidDomainSettings *settings, int cache_mesh_format)
{
  if (cache_mesh_format == settings->cache_mesh_format) {
    return;
  }
  /* TODO(sebbas): Clear old caches. */
  settings->cache_mesh_format = cache_mesh_format;
}

void BKE_fluid_cachetype_data_set(FluidDomainSettings *settings, int cache_data_format)
{
  if (cache_data_format == settings->cache_data_format) {
    return;
  }
  /* TODO(sebbas): Clear old caches. */
  settings->cache_data_format = cache_data_format;
}

void BKE_fluid_cachetype_particle_set(FluidDomainSettings *settings, int cache_particle_format)
{
  if (cache_particle_format == settings->cache_particle_format) {
    return;
  }
  /* TODO(sebbas): Clear old caches. */
  settings->cache_particle_format = cache_particle_format;
}

void BKE_fluid_cachetype_noise_set(FluidDomainSettings *settings, int cache_noise_format)
{
  if (cache_noise_format == settings->cache_noise_format) {
    return;
  }
  /* TODO(sebbas): Clear old caches. */
  settings->cache_noise_format = cache_noise_format;
}

void BKE_fluid_collisionextents_set(FluidDomainSettings *settings, int value, bool clear)
{
  if (clear) {
    settings->border_collisions &= value;
  }
  else {
    settings->border_collisions |= value;
  }
}

void BKE_fluid_particles_set(FluidDomainSettings *settings, int value, bool clear)
{
  if (clear) {
    settings->particle_type &= ~value;
  }
  else {
    settings->particle_type |= value;
  }
}

void BKE_fluid_domain_type_set(Object *object, FluidDomainSettings *settings, int type)
{
  /* Set common values for liquid/smoke domain: cache type,
   * border collision and viewport draw-type. */
  if (type == FLUID_DOMAIN_TYPE_GAS) {
    BKE_fluid_cachetype_mesh_set(settings, FLUID_DOMAIN_FILE_BIN_OBJECT);
    BKE_fluid_cachetype_data_set(settings, FLUID_DOMAIN_FILE_UNI);
    BKE_fluid_cachetype_particle_set(settings, FLUID_DOMAIN_FILE_UNI);
    BKE_fluid_cachetype_noise_set(settings, FLUID_DOMAIN_FILE_UNI);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_FRONT, 1);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_BACK, 1);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_RIGHT, 1);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_LEFT, 1);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_TOP, 1);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_BOTTOM, 1);
    object->dt = OB_WIRE;
  }
  else if (type == FLUID_DOMAIN_TYPE_LIQUID) {
    BKE_fluid_cachetype_mesh_set(settings, FLUID_DOMAIN_FILE_BIN_OBJECT);
    BKE_fluid_cachetype_data_set(settings, FLUID_DOMAIN_FILE_UNI);
    BKE_fluid_cachetype_particle_set(settings, FLUID_DOMAIN_FILE_UNI);
    BKE_fluid_cachetype_noise_set(settings, FLUID_DOMAIN_FILE_UNI);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_FRONT, 0);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_BACK, 0);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_RIGHT, 0);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_LEFT, 0);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_TOP, 0);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_BOTTOM, 0);
    object->dt = OB_SOLID;
  }

  /* Set actual domain type. */
  settings->type = type;
}

void BKE_fluid_flow_behavior_set(Object *UNUSED(object), FluidFlowSettings *settings, int behavior)
{
  settings->behavior = behavior;
}

void BKE_fluid_flow_type_set(Object *object, FluidFlowSettings *settings, int type)
{
  /* By default, liquid flow objects should behave like their geometry (geometry behavior),
   * gas flow objects should continuously produce smoke (inflow behavior). */
  if (type == FLUID_FLOW_TYPE_LIQUID) {
    BKE_fluid_flow_behavior_set(object, settings, FLUID_FLOW_BEHAVIOR_GEOMETRY);
  }
  else {
    BKE_fluid_flow_behavior_set(object, settings, FLUID_FLOW_BEHAVIOR_INFLOW);
  }

  /* Set actual flow type. */
  settings->type = type;
}

void BKE_fluid_effector_type_set(Object *UNUSED(object), FluidEffectorSettings *settings, int type)
{
  settings->type = type;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Modifier API
 *
 * Use for versioning, even when fluids are disabled.
 * \{ */

static void BKE_fluid_modifier_freeDomain(FluidModifierData *mmd)
{
  if (mmd->domain) {
    if (mmd->domain->fluid) {
#ifdef WITH_FLUID
      manta_free(mmd->domain->fluid);
#endif
    }

    if (mmd->domain->fluid_mutex) {
      BLI_rw_mutex_free(mmd->domain->fluid_mutex);
    }

    if (mmd->domain->effector_weights) {
      MEM_freeN(mmd->domain->effector_weights);
    }
    mmd->domain->effector_weights = NULL;

    if (!(mmd->modifier.flag & eModifierFlag_SharedCaches)) {
      BKE_ptcache_free_list(&(mmd->domain->ptcaches[0]));
      mmd->domain->point_cache[0] = NULL;
    }

    if (mmd->domain->mesh_velocities) {
      MEM_freeN(mmd->domain->mesh_velocities);
    }
    mmd->domain->mesh_velocities = NULL;

    if (mmd->domain->coba) {
      MEM_freeN(mmd->domain->coba);
    }

    MEM_freeN(mmd->domain);
    mmd->domain = NULL;
  }
}

static void BKE_fluid_modifier_freeFlow(FluidModifierData *mmd)
{
  if (mmd->flow) {
    if (mmd->flow->mesh) {
      BKE_id_free(NULL, mmd->flow->mesh);
    }
    mmd->flow->mesh = NULL;

    if (mmd->flow->verts_old) {
      MEM_freeN(mmd->flow->verts_old);
    }
    mmd->flow->verts_old = NULL;
    mmd->flow->numverts = 0;
    mmd->flow->flags &= ~FLUID_FLOW_NEEDS_UPDATE;

    MEM_freeN(mmd->flow);
    mmd->flow = NULL;
  }
}

static void BKE_fluid_modifier_freeEffector(FluidModifierData *mmd)
{
  if (mmd->effector) {
    if (mmd->effector->mesh) {
      BKE_id_free(NULL, mmd->effector->mesh);
    }
    mmd->effector->mesh = NULL;

    if (mmd->effector->verts_old) {
      MEM_freeN(mmd->effector->verts_old);
    }
    mmd->effector->verts_old = NULL;
    mmd->effector->numverts = 0;
    mmd->effector->flags &= ~FLUID_EFFECTOR_NEEDS_UPDATE;

    MEM_freeN(mmd->effector);
    mmd->effector = NULL;
  }
}

static void BKE_fluid_modifier_reset_ex(struct FluidModifierData *mmd, bool need_lock)
{
  if (!mmd) {
    return;
  }

  if (mmd->domain) {
    if (mmd->domain->fluid) {
      if (need_lock) {
        BLI_rw_mutex_lock(mmd->domain->fluid_mutex, THREAD_LOCK_WRITE);
      }

#ifdef WITH_FLUID
      manta_free(mmd->domain->fluid);
#endif
      mmd->domain->fluid = NULL;

      if (need_lock) {
        BLI_rw_mutex_unlock(mmd->domain->fluid_mutex);
      }
    }

    mmd->time = -1;
    mmd->domain->total_cells = 0;
    mmd->domain->active_fields = 0;
  }
  else if (mmd->flow) {
    if (mmd->flow->verts_old) {
      MEM_freeN(mmd->flow->verts_old);
    }
    mmd->flow->verts_old = NULL;
    mmd->flow->numverts = 0;
    mmd->flow->flags &= ~FLUID_FLOW_NEEDS_UPDATE;
  }
  else if (mmd->effector) {
    if (mmd->effector->verts_old) {
      MEM_freeN(mmd->effector->verts_old);
    }
    mmd->effector->verts_old = NULL;
    mmd->effector->numverts = 0;
    mmd->effector->flags &= ~FLUID_EFFECTOR_NEEDS_UPDATE;
  }
}

void BKE_fluid_modifier_reset(struct FluidModifierData *mmd)
{
  BKE_fluid_modifier_reset_ex(mmd, true);
}

void BKE_fluid_modifier_free(FluidModifierData *mmd)
{
  if (!mmd) {
    return;
  }

  BKE_fluid_modifier_freeDomain(mmd);
  BKE_fluid_modifier_freeFlow(mmd);
  BKE_fluid_modifier_freeEffector(mmd);
}

void BKE_fluid_modifier_create_type_data(struct FluidModifierData *mmd)
{
  if (!mmd) {
    return;
  }

  if (mmd->type & MOD_FLUID_TYPE_DOMAIN) {
    if (mmd->domain) {
      BKE_fluid_modifier_freeDomain(mmd);
    }

    /* domain object data */
    mmd->domain = MEM_callocN(sizeof(FluidDomainSettings), "FluidDomain");
    mmd->domain->mmd = mmd;
    mmd->domain->effector_weights = BKE_effector_add_weights(NULL);
    mmd->domain->fluid = NULL;
    mmd->domain->fluid_mutex = BLI_rw_mutex_alloc();
    mmd->domain->force_group = NULL;
    mmd->domain->fluid_group = NULL;
    mmd->domain->effector_group = NULL;

    /* adaptive domain options */
    mmd->domain->adapt_margin = 4;
    mmd->domain->adapt_res = 0;
    mmd->domain->adapt_threshold = 0.02f;

    /* fluid domain options */
    mmd->domain->maxres = 32;
    mmd->domain->solver_res = 3;
    mmd->domain->border_collisions = 0;  // open domain
    mmd->domain->flags = FLUID_DOMAIN_USE_DISSOLVE_LOG | FLUID_DOMAIN_USE_ADAPTIVE_TIME;
    mmd->domain->gravity[0] = 0.0f;
    mmd->domain->gravity[1] = 0.0f;
    mmd->domain->gravity[2] = -9.81f;
    mmd->domain->active_fields = 0;
    mmd->domain->type = FLUID_DOMAIN_TYPE_GAS;
    mmd->domain->boundary_width = 1;

    /* smoke domain options */
    mmd->domain->alpha = 1.0f;
    mmd->domain->beta = 1.0f;
    mmd->domain->diss_speed = 5;
    mmd->domain->vorticity = 0;
    mmd->domain->active_color[0] = 0.0f;
    mmd->domain->active_color[1] = 0.0f;
    mmd->domain->active_color[2] = 0.0f;
    mmd->domain->highres_sampling = SM_HRES_FULLSAMPLE;

    /* flame options */
    mmd->domain->burning_rate = 0.75f;
    mmd->domain->flame_smoke = 1.0f;
    mmd->domain->flame_vorticity = 0.5f;
    mmd->domain->flame_ignition = 1.5f;
    mmd->domain->flame_max_temp = 3.0f;
    mmd->domain->flame_smoke_color[0] = 0.7f;
    mmd->domain->flame_smoke_color[1] = 0.7f;
    mmd->domain->flame_smoke_color[2] = 0.7f;

    /* noise options */
    mmd->domain->noise_strength = 1.0;
    mmd->domain->noise_pos_scale = 2.0f;
    mmd->domain->noise_time_anim = 0.1f;
    mmd->domain->noise_scale = 2;
    mmd->domain->noise_type = FLUID_NOISE_TYPE_WAVELET;

    /* liquid domain options */
    mmd->domain->simulation_method = FLUID_DOMAIN_METHOD_FLIP;
    mmd->domain->flip_ratio = 0.97f;
    mmd->domain->particle_randomness = 0.1f;
    mmd->domain->particle_number = 2;
    mmd->domain->particle_minimum = 8;
    mmd->domain->particle_maximum = 16;
    mmd->domain->particle_radius = 1.0f;
    mmd->domain->particle_band_width = 3.0f;
    mmd->domain->fractions_threshold = 0.05f;

    /* diffusion options*/
    mmd->domain->surface_tension = 0.0f;
    mmd->domain->viscosity_base = 1.0f;
    mmd->domain->viscosity_exponent = 6.0f;

    /* mesh options */
    mmd->domain->mesh_velocities = NULL;
    mmd->domain->mesh_concave_upper = 3.5f;
    mmd->domain->mesh_concave_lower = 0.4f;
    mmd->domain->mesh_particle_radius = 2.0;
    mmd->domain->mesh_smoothen_pos = 1;
    mmd->domain->mesh_smoothen_neg = 1;
    mmd->domain->mesh_scale = 2;
    mmd->domain->totvert = 0;
    mmd->domain->mesh_generator = FLUID_DOMAIN_MESH_IMPROVED;

    /* secondary particle options */
    mmd->domain->sndparticle_tau_min_wc = 2.0;
    mmd->domain->sndparticle_tau_max_wc = 8.0;
    mmd->domain->sndparticle_tau_min_ta = 5.0;
    mmd->domain->sndparticle_tau_max_ta = 20.0;
    mmd->domain->sndparticle_tau_min_k = 1.0;
    mmd->domain->sndparticle_tau_max_k = 5.0;
    mmd->domain->sndparticle_k_wc = 200;
    mmd->domain->sndparticle_k_ta = 40;
    mmd->domain->sndparticle_k_b = 0.5;
    mmd->domain->sndparticle_k_d = 0.6;
    mmd->domain->sndparticle_l_min = 10.0;
    mmd->domain->sndparticle_l_max = 25.0;
    mmd->domain->sndparticle_boundary = SNDPARTICLE_BOUNDARY_DELETE;
    mmd->domain->sndparticle_combined_export = SNDPARTICLE_COMBINED_EXPORT_OFF;
    mmd->domain->sndparticle_potential_radius = 2;
    mmd->domain->sndparticle_update_radius = 2;
    mmd->domain->particle_type = 0;
    mmd->domain->particle_scale = 1;

    /* fluid guide options */
    mmd->domain->guide_parent = NULL;
    mmd->domain->guide_alpha = 2.0f;
    mmd->domain->guide_beta = 5;
    mmd->domain->guide_vel_factor = 2.0f;
    mmd->domain->guide_source = FLUID_DOMAIN_GUIDE_SRC_DOMAIN;

    /* cache options */
    mmd->domain->cache_frame_start = 1;
    mmd->domain->cache_frame_end = 250;
    mmd->domain->cache_frame_pause_data = 0;
    mmd->domain->cache_frame_pause_noise = 0;
    mmd->domain->cache_frame_pause_mesh = 0;
    mmd->domain->cache_frame_pause_particles = 0;
    mmd->domain->cache_frame_pause_guide = 0;
    mmd->domain->cache_flag = 0;
    mmd->domain->cache_type = FLUID_DOMAIN_CACHE_REPLAY;
    mmd->domain->cache_mesh_format = FLUID_DOMAIN_FILE_BIN_OBJECT;
#ifdef WITH_OPENVDB
    mmd->domain->cache_data_format = FLUID_DOMAIN_FILE_OPENVDB;
    mmd->domain->cache_particle_format = FLUID_DOMAIN_FILE_OPENVDB;
    mmd->domain->cache_noise_format = FLUID_DOMAIN_FILE_OPENVDB;
#else
    mmd->domain->cache_data_format = FLUID_DOMAIN_FILE_UNI;
    mmd->domain->cache_particle_format = FLUID_DOMAIN_FILE_UNI;
    mmd->domain->cache_noise_format = FLUID_DOMAIN_FILE_UNI;
#endif
    char cache_name[64];
    BKE_fluid_cache_new_name_for_current_session(sizeof(cache_name), cache_name);
    BKE_modifier_path_init(
        mmd->domain->cache_directory, sizeof(mmd->domain->cache_directory), cache_name);

    /* time options */
    mmd->domain->time_scale = 1.0;
    mmd->domain->cfl_condition = 4.0;
    mmd->domain->timesteps_minimum = 1;
    mmd->domain->timesteps_maximum = 4;

    /* display options */
    mmd->domain->slice_method = FLUID_DOMAIN_SLICE_VIEW_ALIGNED;
    mmd->domain->axis_slice_method = AXIS_SLICE_FULL;
    mmd->domain->slice_axis = 0;
    mmd->domain->interp_method = 0;
    mmd->domain->draw_velocity = false;
    mmd->domain->slice_per_voxel = 5.0f;
    mmd->domain->slice_depth = 0.5f;
    mmd->domain->display_thickness = 1.0f;
    mmd->domain->coba = NULL;
    mmd->domain->vector_scale = 1.0f;
    mmd->domain->vector_draw_type = VECTOR_DRAW_NEEDLE;
    mmd->domain->use_coba = false;
    mmd->domain->coba_field = FLUID_DOMAIN_FIELD_DENSITY;

    /* -- Deprecated / unsed options (below)-- */

    /* pointcache options */
    BLI_listbase_clear(&mmd->domain->ptcaches[1]);
    mmd->domain->point_cache[0] = BKE_ptcache_add(&(mmd->domain->ptcaches[0]));
    mmd->domain->point_cache[0]->flag |= PTCACHE_DISK_CACHE;
    mmd->domain->point_cache[0]->step = 1;
    mmd->domain->point_cache[1] = NULL; /* Deprecated */
    mmd->domain->cache_comp = SM_CACHE_LIGHT;
    mmd->domain->cache_high_comp = SM_CACHE_LIGHT;

    /* OpenVDB cache options */
#ifdef WITH_OPENVDB_BLOSC
    mmd->domain->openvdb_comp = VDB_COMPRESSION_BLOSC;
#else
    mmd->domain->openvdb_comp = VDB_COMPRESSION_ZIP;
#endif
    mmd->domain->clipping = 1e-6f;
    mmd->domain->data_depth = 0;
  }
  else if (mmd->type & MOD_FLUID_TYPE_FLOW) {
    if (mmd->flow) {
      BKE_fluid_modifier_freeFlow(mmd);
    }

    /* flow object data */
    mmd->flow = MEM_callocN(sizeof(FluidFlowSettings), "MantaFlow");
    mmd->flow->mmd = mmd;
    mmd->flow->mesh = NULL;
    mmd->flow->psys = NULL;
    mmd->flow->noise_texture = NULL;

    /* initial velocity */
    mmd->flow->verts_old = NULL;
    mmd->flow->numverts = 0;
    mmd->flow->vel_multi = 1.0f;
    mmd->flow->vel_normal = 0.0f;
    mmd->flow->vel_random = 0.0f;
    mmd->flow->vel_coord[0] = 0.0f;
    mmd->flow->vel_coord[1] = 0.0f;
    mmd->flow->vel_coord[2] = 0.0f;

    /* emission */
    mmd->flow->density = 1.0f;
    mmd->flow->color[0] = 0.7f;
    mmd->flow->color[1] = 0.7f;
    mmd->flow->color[2] = 0.7f;
    mmd->flow->fuel_amount = 1.0f;
    mmd->flow->temperature = 1.0f;
    mmd->flow->volume_density = 0.0f;
    mmd->flow->surface_distance = 1.5f;
    mmd->flow->particle_size = 1.0f;
    mmd->flow->subframes = 0;

    /* texture control */
    mmd->flow->source = FLUID_FLOW_SOURCE_MESH;
    mmd->flow->texture_size = 1.0f;

    mmd->flow->type = FLUID_FLOW_TYPE_SMOKE;
    mmd->flow->behavior = FLUID_FLOW_BEHAVIOR_GEOMETRY;
    mmd->flow->flags = FLUID_FLOW_ABSOLUTE | FLUID_FLOW_USE_PART_SIZE | FLUID_FLOW_USE_INFLOW;
  }
  else if (mmd->type & MOD_FLUID_TYPE_EFFEC) {
    if (mmd->effector) {
      BKE_fluid_modifier_freeEffector(mmd);
    }

    /* effector object data */
    mmd->effector = MEM_callocN(sizeof(FluidEffectorSettings), "MantaEffector");
    mmd->effector->mmd = mmd;
    mmd->effector->mesh = NULL;
    mmd->effector->verts_old = NULL;
    mmd->effector->numverts = 0;
    mmd->effector->surface_distance = 0.0f;
    mmd->effector->type = FLUID_EFFECTOR_TYPE_COLLISION;
    mmd->effector->flags = FLUID_EFFECTOR_USE_EFFEC;

    /* guide options */
    mmd->effector->guide_mode = FLUID_EFFECTOR_GUIDE_OVERRIDE;
    mmd->effector->vel_multi = 1.0f;
  }
}

void BKE_fluid_modifier_copy(const struct FluidModifierData *mmd,
                             struct FluidModifierData *tmmd,
                             const int flag)
{
  tmmd->type = mmd->type;
  tmmd->time = mmd->time;

  BKE_fluid_modifier_create_type_data(tmmd);

  if (tmmd->domain) {
    FluidDomainSettings *tmds = tmmd->domain;
    FluidDomainSettings *mds = mmd->domain;

    /* domain object data */
    tmds->fluid_group = mds->fluid_group;
    tmds->force_group = mds->force_group;
    tmds->effector_group = mds->effector_group;
    if (tmds->effector_weights) {
      MEM_freeN(tmds->effector_weights);
    }
    tmds->effector_weights = MEM_dupallocN(mds->effector_weights);

    /* adaptive domain options */
    tmds->adapt_margin = mds->adapt_margin;
    tmds->adapt_res = mds->adapt_res;
    tmds->adapt_threshold = mds->adapt_threshold;

    /* fluid domain options */
    tmds->maxres = mds->maxres;
    tmds->solver_res = mds->solver_res;
    tmds->border_collisions = mds->border_collisions;
    tmds->flags = mds->flags;
    tmds->gravity[0] = mds->gravity[0];
    tmds->gravity[1] = mds->gravity[1];
    tmds->gravity[2] = mds->gravity[2];
    tmds->active_fields = mds->active_fields;
    tmds->type = mds->type;
    tmds->boundary_width = mds->boundary_width;

    /* smoke domain options */
    tmds->alpha = mds->alpha;
    tmds->beta = mds->beta;
    tmds->diss_speed = mds->diss_speed;
    tmds->vorticity = mds->vorticity;
    tmds->highres_sampling = mds->highres_sampling;

    /* flame options */
    tmds->burning_rate = mds->burning_rate;
    tmds->flame_smoke = mds->flame_smoke;
    tmds->flame_vorticity = mds->flame_vorticity;
    tmds->flame_ignition = mds->flame_ignition;
    tmds->flame_max_temp = mds->flame_max_temp;
    copy_v3_v3(tmds->flame_smoke_color, mds->flame_smoke_color);

    /* noise options */
    tmds->noise_strength = mds->noise_strength;
    tmds->noise_pos_scale = mds->noise_pos_scale;
    tmds->noise_time_anim = mds->noise_time_anim;
    tmds->noise_scale = mds->noise_scale;
    tmds->noise_type = mds->noise_type;

    /* liquid domain options */
    tmds->flip_ratio = mds->flip_ratio;
    tmds->particle_randomness = mds->particle_randomness;
    tmds->particle_number = mds->particle_number;
    tmds->particle_minimum = mds->particle_minimum;
    tmds->particle_maximum = mds->particle_maximum;
    tmds->particle_radius = mds->particle_radius;
    tmds->particle_band_width = mds->particle_band_width;
    tmds->fractions_threshold = mds->fractions_threshold;

    /* diffusion options*/
    tmds->surface_tension = mds->surface_tension;
    tmds->viscosity_base = mds->viscosity_base;
    tmds->viscosity_exponent = mds->viscosity_exponent;

    /* mesh options */
    if (mds->mesh_velocities) {
      tmds->mesh_velocities = MEM_dupallocN(mds->mesh_velocities);
    }
    tmds->mesh_concave_upper = mds->mesh_concave_upper;
    tmds->mesh_concave_lower = mds->mesh_concave_lower;
    tmds->mesh_particle_radius = mds->mesh_particle_radius;
    tmds->mesh_smoothen_pos = mds->mesh_smoothen_pos;
    tmds->mesh_smoothen_neg = mds->mesh_smoothen_neg;
    tmds->mesh_scale = mds->mesh_scale;
    tmds->totvert = mds->totvert;
    tmds->mesh_generator = mds->mesh_generator;

    /* secondary particle options */
    tmds->sndparticle_k_b = mds->sndparticle_k_b;
    tmds->sndparticle_k_d = mds->sndparticle_k_d;
    tmds->sndparticle_k_ta = mds->sndparticle_k_ta;
    tmds->sndparticle_k_wc = mds->sndparticle_k_wc;
    tmds->sndparticle_l_max = mds->sndparticle_l_max;
    tmds->sndparticle_l_min = mds->sndparticle_l_min;
    tmds->sndparticle_tau_max_k = mds->sndparticle_tau_max_k;
    tmds->sndparticle_tau_max_ta = mds->sndparticle_tau_max_ta;
    tmds->sndparticle_tau_max_wc = mds->sndparticle_tau_max_wc;
    tmds->sndparticle_tau_min_k = mds->sndparticle_tau_min_k;
    tmds->sndparticle_tau_min_ta = mds->sndparticle_tau_min_ta;
    tmds->sndparticle_tau_min_wc = mds->sndparticle_tau_min_wc;
    tmds->sndparticle_boundary = mds->sndparticle_boundary;
    tmds->sndparticle_combined_export = mds->sndparticle_combined_export;
    tmds->sndparticle_potential_radius = mds->sndparticle_potential_radius;
    tmds->sndparticle_update_radius = mds->sndparticle_update_radius;
    tmds->particle_type = mds->particle_type;
    tmds->particle_scale = mds->particle_scale;

    /* fluid guide options */
    tmds->guide_parent = mds->guide_parent;
    tmds->guide_alpha = mds->guide_alpha;
    tmds->guide_beta = mds->guide_beta;
    tmds->guide_vel_factor = mds->guide_vel_factor;
    copy_v3_v3_int(tmds->guide_res, mds->guide_res);
    tmds->guide_source = mds->guide_source;

    /* cache options */
    tmds->cache_frame_start = mds->cache_frame_start;
    tmds->cache_frame_end = mds->cache_frame_end;
    tmds->cache_frame_pause_data = mds->cache_frame_pause_data;
    tmds->cache_frame_pause_noise = mds->cache_frame_pause_noise;
    tmds->cache_frame_pause_mesh = mds->cache_frame_pause_mesh;
    tmds->cache_frame_pause_particles = mds->cache_frame_pause_particles;
    tmds->cache_frame_pause_guide = mds->cache_frame_pause_guide;
    tmds->cache_flag = mds->cache_flag;
    tmds->cache_type = mds->cache_type;
    tmds->cache_mesh_format = mds->cache_mesh_format;
    tmds->cache_data_format = mds->cache_data_format;
    tmds->cache_particle_format = mds->cache_particle_format;
    tmds->cache_noise_format = mds->cache_noise_format;
    BLI_strncpy(tmds->cache_directory, mds->cache_directory, sizeof(tmds->cache_directory));

    /* time options */
    tmds->time_scale = mds->time_scale;
    tmds->cfl_condition = mds->cfl_condition;
    tmds->timesteps_minimum = mds->timesteps_minimum;
    tmds->timesteps_maximum = mds->timesteps_maximum;

    /* display options */
    tmds->slice_method = mds->slice_method;
    tmds->axis_slice_method = mds->axis_slice_method;
    tmds->slice_axis = mds->slice_axis;
    tmds->interp_method = mds->interp_method;
    tmds->draw_velocity = mds->draw_velocity;
    tmds->slice_per_voxel = mds->slice_per_voxel;
    tmds->slice_depth = mds->slice_depth;
    tmds->display_thickness = mds->display_thickness;
    if (mds->coba) {
      tmds->coba = MEM_dupallocN(mds->coba);
    }
    tmds->vector_scale = mds->vector_scale;
    tmds->vector_draw_type = mds->vector_draw_type;
    tmds->use_coba = mds->use_coba;
    tmds->coba_field = mds->coba_field;

    /* -- Deprecated / unsed options (below)-- */

    /* pointcache options */
    BKE_ptcache_free_list(&(tmds->ptcaches[0]));
    if (flag & LIB_ID_CREATE_NO_MAIN) {
      /* Share the cache with the original object's modifier. */
      tmmd->modifier.flag |= eModifierFlag_SharedCaches;
      tmds->point_cache[0] = mds->point_cache[0];
      tmds->ptcaches[0] = mds->ptcaches[0];
    }
    else {
      tmds->point_cache[0] = BKE_ptcache_copy_list(
          &(tmds->ptcaches[0]), &(mds->ptcaches[0]), flag);
    }

    /* OpenVDB cache options */
    tmds->openvdb_comp = mds->openvdb_comp;
    tmds->clipping = mds->clipping;
    tmds->data_depth = mds->data_depth;
  }
  else if (tmmd->flow) {
    FluidFlowSettings *tmfs = tmmd->flow;
    FluidFlowSettings *mfs = mmd->flow;

    tmfs->psys = mfs->psys;
    tmfs->noise_texture = mfs->noise_texture;

    /* initial velocity */
    tmfs->vel_multi = mfs->vel_multi;
    tmfs->vel_normal = mfs->vel_normal;
    tmfs->vel_random = mfs->vel_random;
    tmfs->vel_coord[0] = mfs->vel_coord[0];
    tmfs->vel_coord[1] = mfs->vel_coord[1];
    tmfs->vel_coord[2] = mfs->vel_coord[2];

    /* emission */
    tmfs->density = mfs->density;
    copy_v3_v3(tmfs->color, mfs->color);
    tmfs->fuel_amount = mfs->fuel_amount;
    tmfs->temperature = mfs->temperature;
    tmfs->volume_density = mfs->volume_density;
    tmfs->surface_distance = mfs->surface_distance;
    tmfs->particle_size = mfs->particle_size;
    tmfs->subframes = mfs->subframes;

    /* texture control */
    tmfs->texture_size = mfs->texture_size;
    tmfs->texture_offset = mfs->texture_offset;
    BLI_strncpy(tmfs->uvlayer_name, mfs->uvlayer_name, sizeof(tmfs->uvlayer_name));
    tmfs->vgroup_density = mfs->vgroup_density;

    tmfs->type = mfs->type;
    tmfs->behavior = mfs->behavior;
    tmfs->source = mfs->source;
    tmfs->texture_type = mfs->texture_type;
    tmfs->flags = mfs->flags;
  }
  else if (tmmd->effector) {
    FluidEffectorSettings *tmes = tmmd->effector;
    FluidEffectorSettings *mes = mmd->effector;

    tmes->surface_distance = mes->surface_distance;
    tmes->type = mes->type;
    tmes->flags = mes->flags;
    tmes->subframes = mes->subframes;

    /* guide options */
    tmes->guide_mode = mes->guide_mode;
    tmes->vel_multi = mes->vel_multi;
  }
}

void BKE_fluid_cache_new_name_for_current_session(int maxlen, char *r_name)
{
  static int counter = 1;
  BLI_snprintf(r_name, maxlen, FLUID_DOMAIN_DIR_DEFAULT "_%x", BLI_hash_int(counter));
  counter++;
}

/** \} */
