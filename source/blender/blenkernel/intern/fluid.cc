/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"

#include "BLI_fileops.h"
#include "BLI_hash.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_colorband_types.h"
#include "DNA_defaults.h"
#include "DNA_fluid_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_texture_types.h"

#include "BKE_attribute.hh"
#include "BKE_effect.h"
#include "BKE_fluid.h"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_modifier.hh"
#include "BKE_pointcache.h"

#ifdef WITH_FLUID

#  include <algorithm>
#  include <cfloat>
#  include <cmath>
#  include <cstdio>
#  include <cstring> /* memset */

#  include "DNA_customdata_types.h"
#  include "DNA_light_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_particle_types.h"
#  include "DNA_scene_types.h"

#  include "BLI_kdopbvh.hh"
#  include "BLI_kdtree.h"
#  include "BLI_math_vector.hh"
#  include "BLI_mutex.hh"
#  include "BLI_threads.h"
#  include "BLI_voxel.h"

#  include "BKE_bvhutils.hh"
#  include "BKE_collision.h"
#  include "BKE_customdata.hh"
#  include "BKE_deform.hh"
#  include "BKE_mesh.hh"
#  include "BKE_mesh_runtime.hh"
#  include "BKE_object.hh"
#  include "BKE_particle.h"
#  include "BKE_scene.hh"
#  include "BKE_texture.h"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_query.hh"

#  include "RE_texture.h"

#  include "CLG_log.h"

#  include "manta_fluid_API.h"

#endif /* WITH_FLUID */

/** Time step default value for nice appearance. */
#define DT_DEFAULT 0.1f

/** Max value for phi initialization */
#define PHI_MAX 9999.0f

static void fluid_modifier_reset_ex(FluidModifierData *fmd, bool need_lock);

#ifdef WITH_FLUID
// #define DEBUG_PRINT

static CLG_LogRef LOG = {"physics.fluid"};

/* -------------------------------------------------------------------- */
/** \name Fluid API
 * \{ */

static blender::Mutex object_update_lock;

#  define ADD_IF_LOWER_POS(a, b) min_ff((a) + (b), max_ff((a), (b)))
#  define ADD_IF_LOWER_NEG(a, b) max_ff((a) + (b), min_ff((a), (b)))
#  define ADD_IF_LOWER(a, b) (((b) > 0) ? ADD_IF_LOWER_POS((a), (b)) : ADD_IF_LOWER_NEG((a), (b)))

bool BKE_fluid_reallocate_fluid(FluidDomainSettings *fds, int res[3], int free_old)
{
  if (free_old && fds->fluid) {
    manta_free(fds->fluid);
  }
  if (!min_iii(res[0], res[1], res[2])) {
    fds->fluid = nullptr;
  }
  else {
    fds->fluid = manta_init(res, fds->fmd);

    fds->res_noise[0] = res[0] * fds->noise_scale;
    fds->res_noise[1] = res[1] * fds->noise_scale;
    fds->res_noise[2] = res[2] * fds->noise_scale;
  }

  return (fds->fluid != nullptr);
}

void BKE_fluid_reallocate_copy_fluid(FluidDomainSettings *fds,
                                     int o_res[3],
                                     int n_res[3],
                                     const int o_min[3],
                                     const int n_min[3],
                                     const int o_max[3],
                                     int o_shift[3],
                                     int n_shift[3])
{
  MANTA *fluid_old = fds->fluid;
  const int block_size = fds->noise_scale;
  int new_shift[3] = {0};
  sub_v3_v3v3_int(new_shift, n_shift, o_shift);

  /* Allocate new fluid data. */
  BKE_fluid_reallocate_fluid(fds, n_res, 0);

  int o_total_cells = o_res[0] * o_res[1] * o_res[2];
  int n_total_cells = n_res[0] * n_res[1] * n_res[2];

  /* Copy values from old fluid to new fluid object. */
  if (o_total_cells > 1 && n_total_cells > 1) {
    float *o_dens = manta_smoke_get_density(fluid_old);
    float *o_react = manta_smoke_get_react(fluid_old);
    float *o_flame = manta_smoke_get_flame(fluid_old);
    float *o_fuel = manta_smoke_get_fuel(fluid_old);
    float *o_heat = manta_smoke_get_heat(fluid_old);
    float *o_vx = manta_get_velocity_x(fluid_old);
    float *o_vy = manta_get_velocity_y(fluid_old);
    float *o_vz = manta_get_velocity_z(fluid_old);
    float *o_r = manta_smoke_get_color_r(fluid_old);
    float *o_g = manta_smoke_get_color_g(fluid_old);
    float *o_b = manta_smoke_get_color_b(fluid_old);

    float *n_dens = manta_smoke_get_density(fds->fluid);
    float *n_react = manta_smoke_get_react(fds->fluid);
    float *n_flame = manta_smoke_get_flame(fds->fluid);
    float *n_fuel = manta_smoke_get_fuel(fds->fluid);
    float *n_heat = manta_smoke_get_heat(fds->fluid);
    float *n_vx = manta_get_velocity_x(fds->fluid);
    float *n_vy = manta_get_velocity_y(fds->fluid);
    float *n_vz = manta_get_velocity_z(fds->fluid);
    float *n_r = manta_smoke_get_color_r(fds->fluid);
    float *n_g = manta_smoke_get_color_g(fds->fluid);
    float *n_b = manta_smoke_get_color_b(fds->fluid);

    /* Noise smoke fields. */
    float *o_wt_dens = manta_noise_get_density(fluid_old);
    float *o_wt_react = manta_noise_get_react(fluid_old);
    float *o_wt_flame = manta_noise_get_flame(fluid_old);
    float *o_wt_fuel = manta_noise_get_fuel(fluid_old);
    float *o_wt_r = manta_noise_get_color_r(fluid_old);
    float *o_wt_g = manta_noise_get_color_g(fluid_old);
    float *o_wt_b = manta_noise_get_color_b(fluid_old);
    float *o_wt_tcu = manta_noise_get_texture_u(fluid_old);
    float *o_wt_tcv = manta_noise_get_texture_v(fluid_old);
    float *o_wt_tcw = manta_noise_get_texture_w(fluid_old);
    float *o_wt_tcu2 = manta_noise_get_texture_u2(fluid_old);
    float *o_wt_tcv2 = manta_noise_get_texture_v2(fluid_old);
    float *o_wt_tcw2 = manta_noise_get_texture_w2(fluid_old);

    float *n_wt_dens = manta_noise_get_density(fds->fluid);
    float *n_wt_react = manta_noise_get_react(fds->fluid);
    float *n_wt_flame = manta_noise_get_flame(fds->fluid);
    float *n_wt_fuel = manta_noise_get_fuel(fds->fluid);
    float *n_wt_r = manta_noise_get_color_r(fds->fluid);
    float *n_wt_g = manta_noise_get_color_g(fds->fluid);
    float *n_wt_b = manta_noise_get_color_b(fds->fluid);
    float *n_wt_tcu = manta_noise_get_texture_u(fds->fluid);
    float *n_wt_tcv = manta_noise_get_texture_v(fds->fluid);
    float *n_wt_tcw = manta_noise_get_texture_w(fds->fluid);
    float *n_wt_tcu2 = manta_noise_get_texture_u2(fds->fluid);
    float *n_wt_tcv2 = manta_noise_get_texture_v2(fds->fluid);
    float *n_wt_tcw2 = manta_noise_get_texture_w2(fds->fluid);

    int wt_res_old[3];
    manta_noise_get_res(fluid_old, wt_res_old);

    for (int z = o_min[2]; z < o_max[2]; z++) {
      for (int y = o_min[1]; y < o_max[1]; y++) {
        for (int x = o_min[0]; x < o_max[0]; x++) {
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

          /* Skip if outside new domain. */
          if (xn < 0 || xn >= n_res[0] || yn < 0 || yn >= n_res[1] || zn < 0 || zn >= n_res[2]) {
            continue;
          }
#  if 0
          /* Note (sebbas):
           * Disabling this "skip section" as not copying borders results in weird cut-off effects.
           * It is possible that this cutting off is the reason for line effects as seen in #74559.
           * Since domain borders will be handled on the simulation side anyways,
           * copying border values should not be an issue. */

          /* boundary cells will be skipped when copying data */
          int bwidth = fds->boundary_width;

          /* Skip if trying to copy from old boundary cell. */
          if (xo < bwidth || yo < bwidth || zo < bwidth || xo >= o_res[0] - bwidth ||
              yo >= o_res[1] - bwidth || zo >= o_res[2] - bwidth)
          {
            continue;
          }
          /* Skip if trying to copy into new boundary cell. */
          if (xn < bwidth || yn < bwidth || zn < bwidth || xn >= n_res[0] - bwidth ||
              yn >= n_res[1] - bwidth || zn >= n_res[2] - bwidth)
          {
            continue;
          }
#  endif

          /* copy data */
          if (fds->flags & FLUID_DOMAIN_USE_NOISE) {
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
                      xx_n + i, fds->res_noise[0], yy_n + j, fds->res_noise[1], zz_n + k);
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

void BKE_fluid_cache_free_all(FluidDomainSettings *fds, Object *ob)
{
  int cache_map = (FLUID_DOMAIN_OUTDATED_DATA | FLUID_DOMAIN_OUTDATED_NOISE |
                   FLUID_DOMAIN_OUTDATED_MESH | FLUID_DOMAIN_OUTDATED_PARTICLES |
                   FLUID_DOMAIN_OUTDATED_GUIDE);
  BKE_fluid_cache_free(fds, ob, cache_map);
}

void BKE_fluid_cache_free(FluidDomainSettings *fds, Object *ob, int cache_map)
{
  char temp_dir[FILE_MAX];
  int flags = fds->cache_flag;
  const char *relbase = BKE_modifier_path_relbase_from_global(ob);

  if (cache_map & FLUID_DOMAIN_OUTDATED_DATA) {
    flags &= ~(FLUID_DOMAIN_BAKING_DATA | FLUID_DOMAIN_BAKED_DATA | FLUID_DOMAIN_OUTDATED_DATA);
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_CONFIG);
    BLI_path_abs(temp_dir, relbase);
    if (BLI_exists(temp_dir)) {
      BLI_delete(temp_dir, true, true);
    }
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_DATA);
    BLI_path_abs(temp_dir, relbase);
    if (BLI_exists(temp_dir)) {
      BLI_delete(temp_dir, true, true);
    }
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_SCRIPT);
    BLI_path_abs(temp_dir, relbase);
    if (BLI_exists(temp_dir)) {
      BLI_delete(temp_dir, true, true);
    }
    fds->cache_frame_pause_data = 0;
  }
  if (cache_map & FLUID_DOMAIN_OUTDATED_NOISE) {
    flags &= ~(FLUID_DOMAIN_BAKING_NOISE | FLUID_DOMAIN_BAKED_NOISE | FLUID_DOMAIN_OUTDATED_NOISE);
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_NOISE);
    BLI_path_abs(temp_dir, relbase);
    if (BLI_exists(temp_dir)) {
      BLI_delete(temp_dir, true, true);
    }
    fds->cache_frame_pause_noise = 0;
  }
  if (cache_map & FLUID_DOMAIN_OUTDATED_MESH) {
    flags &= ~(FLUID_DOMAIN_BAKING_MESH | FLUID_DOMAIN_BAKED_MESH | FLUID_DOMAIN_OUTDATED_MESH);
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_MESH);
    BLI_path_abs(temp_dir, relbase);
    if (BLI_exists(temp_dir)) {
      BLI_delete(temp_dir, true, true);
    }
    fds->cache_frame_pause_mesh = 0;
  }
  if (cache_map & FLUID_DOMAIN_OUTDATED_PARTICLES) {
    flags &= ~(FLUID_DOMAIN_BAKING_PARTICLES | FLUID_DOMAIN_BAKED_PARTICLES |
               FLUID_DOMAIN_OUTDATED_PARTICLES);
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_PARTICLES);
    BLI_path_abs(temp_dir, relbase);
    if (BLI_exists(temp_dir)) {
      BLI_delete(temp_dir, true, true);
    }
    fds->cache_frame_pause_particles = 0;
  }
  if (cache_map & FLUID_DOMAIN_OUTDATED_GUIDE) {
    flags &= ~(FLUID_DOMAIN_BAKING_GUIDE | FLUID_DOMAIN_BAKED_GUIDE | FLUID_DOMAIN_OUTDATED_GUIDE);
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_GUIDE);
    BLI_path_abs(temp_dir, relbase);
    if (BLI_exists(temp_dir)) {
      BLI_delete(temp_dir, true, true);
    }
    fds->cache_frame_pause_guide = 0;
  }
  fds->cache_flag = flags;
}

/* convert global position to domain cell space */
static void manta_pos_to_cell(FluidDomainSettings *fds, float pos[3])
{
  mul_m4_v3(fds->imat, pos);
  sub_v3_v3(pos, fds->p0);
  pos[0] *= 1.0f / fds->cell_size[0];
  pos[1] *= 1.0f / fds->cell_size[1];
  pos[2] *= 1.0f / fds->cell_size[2];
}

/* Set domain transformations and base resolution from object mesh. */
static void manta_set_domain_from_mesh(FluidDomainSettings *fds,
                                       Object *ob,
                                       Mesh *mesh,
                                       bool init_resolution)
{
  size_t i;
  float min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  float size[3];

  blender::MutableSpan<blender::float3> positions = mesh->vert_positions_for_write();
  float scale = 0.0;
  int res;

  res = fds->maxres;

  /* Set minimum and maximum coordinates of BB. */
  for (i = 0; i < mesh->verts_num; i++) {
    minmax_v3v3_v3(min, max, positions[i]);
  }

  /* Set domain bounds. */
  copy_v3_v3(fds->p0, min);
  copy_v3_v3(fds->p1, max);
  fds->dx = 1.0f / res;

  /* Calculate domain dimensions. */
  sub_v3_v3v3(size, max, min);
  if (init_resolution) {
    zero_v3_int(fds->base_res);
    copy_v3_v3(fds->cell_size, size);
  }
  /* Apply object scale. */
  for (i = 0; i < 3; i++) {
    size[i] = fabsf(size[i] * ob->scale[i]);
  }
  copy_v3_v3(fds->global_size, size);
  copy_v3_v3(fds->dp0, min);

  invert_m4_m4(fds->imat, ob->object_to_world().ptr());

  /* Prevent crash when initializing a plane as domain. */
  if (!init_resolution || (size[0] < FLT_EPSILON) || (size[1] < FLT_EPSILON) ||
      (size[2] < FLT_EPSILON))
  {
    return;
  }

  /* Define grid resolutions from longest domain side. */
  if (size[0] >= std::max(size[1], size[2])) {
    scale = res / size[0];
    fds->scale = size[0] / fabsf(ob->scale[0]);
    fds->base_res[0] = res;
    fds->base_res[1] = max_ii(int(size[1] * scale + 0.5f), 4);
    fds->base_res[2] = max_ii(int(size[2] * scale + 0.5f), 4);
  }
  else if (size[1] >= std::max(size[0], size[2])) {
    scale = res / size[1];
    fds->scale = size[1] / fabsf(ob->scale[1]);
    fds->base_res[0] = max_ii(int(size[0] * scale + 0.5f), 4);
    fds->base_res[1] = res;
    fds->base_res[2] = max_ii(int(size[2] * scale + 0.5f), 4);
  }
  else {
    scale = res / size[2];
    fds->scale = size[2] / fabsf(ob->scale[2]);
    fds->base_res[0] = max_ii(int(size[0] * scale + 0.5f), 4);
    fds->base_res[1] = max_ii(int(size[1] * scale + 0.5f), 4);
    fds->base_res[2] = res;
  }

  /* Set cell size. */
  fds->cell_size[0] /= float(fds->base_res[0]);
  fds->cell_size[1] /= float(fds->base_res[1]);
  fds->cell_size[2] /= float(fds->base_res[2]);
}

static void update_final_gravity(FluidDomainSettings *fds, Scene *scene)
{
  if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
    copy_v3_v3(fds->gravity_final, scene->physics_settings.gravity);
  }
  else {
    copy_v3_v3(fds->gravity_final, fds->gravity);
  }
  mul_v3_fl(fds->gravity_final, fds->effector_weights->global_gravity);
}

static bool fluid_modifier_init(
    FluidModifierData *fmd, Depsgraph *depsgraph, Object *ob, Scene *scene, Mesh *mesh)
{
  int scene_framenr = int(DEG_get_ctime(depsgraph));

  if ((fmd->type & MOD_FLUID_TYPE_DOMAIN) && fmd->domain && !fmd->domain->fluid) {
    FluidDomainSettings *fds = fmd->domain;
    int res[3];
    /* Set domain dimensions from mesh. */
    manta_set_domain_from_mesh(fds, ob, mesh, true);
    /* Set domain gravity, use global gravity if enabled. */
    update_final_gravity(fds, scene);
    /* Reset domain values. */
    zero_v3_int(fds->shift);
    zero_v3(fds->shift_f);
    add_v3_fl(fds->shift_f, 0.5f);
    zero_v3(fds->prev_loc);
    mul_m4_v3(ob->object_to_world().ptr(), fds->prev_loc);
    copy_m4_m4(fds->obmat, ob->object_to_world().ptr());

    /* Set resolutions. */
    if (fmd->domain->type == FLUID_DOMAIN_TYPE_GAS &&
        fmd->domain->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN)
    {
      res[0] = res[1] = res[2] = 1; /* Use minimum res for adaptive init. */
    }
    else {
      copy_v3_v3_int(res, fds->base_res);
    }
    copy_v3_v3_int(fds->res, res);
    fds->total_cells = fds->res[0] * fds->res[1] * fds->res[2];
    fds->res_min[0] = fds->res_min[1] = fds->res_min[2] = 0;
    copy_v3_v3_int(fds->res_max, res);

    /* Set time, frame length = 0.1 is at 25fps. */
    fds->frame_length = DT_DEFAULT * (25.0f / scene->frames_per_second()) * fds->time_scale;
    /* Initially dt is equal to frame length (dt can change with adaptive-time stepping though). */
    fds->dt = fds->frame_length;
    fds->time_per_frame = 0;

    fmd->time = scene_framenr;

    /* Allocate fluid. */
    return BKE_fluid_reallocate_fluid(fds, fds->res, 0);
  }
  if (fmd->type & MOD_FLUID_TYPE_FLOW) {
    if (!fmd->flow) {
      BKE_fluid_modifier_create_type_data(fmd);
    }
    fmd->time = scene_framenr;
    return true;
  }
  if (fmd->type & MOD_FLUID_TYPE_EFFEC) {
    if (!fmd->effector) {
      BKE_fluid_modifier_create_type_data(fmd);
    }
    fmd->time = scene_framenr;
    return true;
  }
  return false;
}

/* Forward declarations. */
static void manta_smoke_calc_transparency(FluidDomainSettings *fds,
                                          Scene *scene,
                                          ViewLayer *view_layer);
static float calc_voxel_transp(
    float *result, const float *input, int res[3], int *pixel, float *t_ray, float correct);
static void update_distances(int index,
                             float *distance_map,
                             blender::bke::BVHTreeFromMesh *tree_data,
                             const float ray_start[3],
                             float surface_thickness,
                             bool use_plane_init);

static int get_light(Scene *scene, ViewLayer *view_layer, float *light)
{
  int found_light = 0;

  /* Try to find a lamp, preferably local. */
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base_tmp, BKE_view_layer_object_bases_get(view_layer)) {
    if (base_tmp->object->type == OB_LAMP) {
      Light *la = static_cast<Light *>(base_tmp->object->data);

      if (la->type == LA_LOCAL) {
        copy_v3_v3(light, base_tmp->object->object_to_world().location());
        return 1;
      }
      if (!found_light) {
        copy_v3_v3(light, base_tmp->object->object_to_world().location());
        found_light = 1;
      }
    }
  }

  return found_light;
}

static void clamp_bounds_in_domain(FluidDomainSettings *fds,
                                   int min[3],
                                   int max[3],
                                   const float *min_vel,
                                   const float *max_vel,
                                   int margin,
                                   float dt)
{
  for (int i = 0; i < 3; i++) {
    int adapt = (fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) ? fds->adapt_res : 0;
    /* Add some margin. */
    min[i] -= margin;
    max[i] += margin;

    /* Adapt to velocity. */
    if (min_vel && min_vel[i] < 0.0f) {
      min[i] += int(floor(min_vel[i] * dt));
    }
    if (max_vel && max_vel[i] > 0.0f) {
      max[i] += int(ceil(max_vel[i] * dt));
    }

    /* Clamp within domain max size. */
    CLAMP(min[i], -adapt, fds->base_res[i] + adapt);
    CLAMP(max[i], -adapt, fds->base_res[i] + adapt);
  }
}

static bool is_static_object(Object *ob)
{
  /* Check if the object has modifiers that might make the object "dynamic". */
  VirtualModifierData virtual_modifier_data;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data);
  for (; md; md = md->next) {
    if (ELEM(md->type,
             eModifierType_Cloth,
             eModifierType_DynamicPaint,
             eModifierType_Explode,
             eModifierType_Ocean,
             eModifierType_ShapeKey,
             eModifierType_Softbody,
             eModifierType_Nodes))
    {
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

struct FluidObjectBB {
  float *influence;
  float *velocity;
  float *distances;
  float *numobjs;
  int min[3], max[3], res[3];
  int hmin[3], hmax[3], hres[3];
  int total_cells, valid;
};

static void bb_boundInsert(FluidObjectBB *bb, const float point[3])
{
  int i = 0;
  if (!bb->valid) {
    for (; i < 3; i++) {
      bb->min[i] = int(floor(point[i]));
      bb->max[i] = int(ceil(point[i]));
    }
    bb->valid = 1;
  }
  else {
    for (; i < 3; i++) {
      if (point[i] < bb->min[i]) {
        bb->min[i] = int(floor(point[i]));
      }
      if (point[i] > bb->max[i]) {
        bb->max[i] = int(ceil(point[i]));
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

  bb->numobjs = MEM_calloc_arrayN<float>(bb->total_cells, "fluid_bb_numobjs");
  if (use_influence) {
    bb->influence = MEM_calloc_arrayN<float>(bb->total_cells, "fluid_bb_influence");
  }
  if (use_velocity) {
    bb->velocity = MEM_calloc_arrayN<float>(3 * size_t(bb->total_cells), "fluid_bb_velocity");
  }

  bb->distances = MEM_malloc_arrayN<float>(size_t(bb->total_cells), "fluid_bb_distances");
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

  /* Copy-fill input 1 struct and clear output for new allocation. */
  FluidObjectBB bb1;
  memcpy(&bb1, output, sizeof(FluidObjectBB));
  memset(output, 0, sizeof(FluidObjectBB));

  for (i = 0; i < 3; i++) {
    if (bb1.valid) {
      output->min[i] = std::min(bb1.min[i], bb2->min[i]);
      output->max[i] = std::max(bb1.max[i], bb2->max[i]);
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
            z >= bb1.min[2] && z < bb1.max[2])
        {
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
            z >= bb2->min[2] && z < bb2->max[2])
        {
          int index_in = manta_get_index(
              x - bb2->min[0], bb2->res[0], y - bb2->min[1], bb2->res[1], z - bb2->min[2]);

          /* Values. */
          output->numobjs[index_out] = std::max(bb2->numobjs[index_in],
                                                output->numobjs[index_out]);
          if (output->influence && bb2->influence) {
            if (additive) {
              output->influence[index_out] += bb2->influence[index_in] * sample_size;
            }
            else {
              output->influence[index_out] = std::max(bb2->influence[index_in],
                                                      output->influence[index_out]);
            }
          }
          output->distances[index_out] = std::min(bb2->distances[index_in],
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

BLI_INLINE void apply_effector_fields(FluidEffectorSettings * /*fes*/,
                                      int index,
                                      float src_distance_value,
                                      float *dest_phi_in,
                                      float src_numobjs_value,
                                      float *dest_numobjs,
                                      float const src_vel_value[3],
                                      float *dest_vel_x,
                                      float *dest_vel_y,
                                      float *dest_vel_z)
{
  /* Ensure that distance value is "joined" into the levelset. */
  if (dest_phi_in) {
    dest_phi_in[index] = std::min(src_distance_value, dest_phi_in[index]);
  }

  /* Accumulate effector object count (important once effector object overlap). */
  if (dest_numobjs && src_numobjs_value > 0) {
    dest_numobjs[index] += 1;
  }

  /* Accumulate effector velocities for each cell. */
  if (dest_vel_x && src_numobjs_value > 0) {
    dest_vel_x[index] += src_vel_value[0];
    dest_vel_y[index] += src_vel_value[1];
    dest_vel_z[index] += src_vel_value[2];
  }
}

static void update_velocities(FluidEffectorSettings *fes,
                              const blender::Span<blender::float3> vert_positions,
                              const int *corner_verts,
                              const blender::int3 *corner_tris,
                              float *velocity_map,
                              int index,
                              blender::bke::BVHTreeFromMesh *tree_data,
                              const float ray_start[3],
                              const float *vert_vel,
                              bool has_velocity)
{
  BVHTreeNearest nearest = {0};
  nearest.index = -1;

  /* Distance between two opposing vertices in a unit cube.
   * I.e. the unit cube diagonal or `sqrt(3)`.
   * This value is our nearest neighbor search distance. */
  const float surface_distance = 1.732;
  /* find_nearest uses squared distance */
  nearest.dist_sq = surface_distance * surface_distance;

  /* Find the nearest point on the mesh. */
  if (has_velocity &&
      BLI_bvhtree_find_nearest(
          tree_data->tree, ray_start, &nearest, tree_data->nearest_callback, tree_data) != -1)
  {
    float weights[3];
    int v1, v2, v3, tri_i = nearest.index;

    /* Calculate barycentric weights for nearest point. */
    v1 = corner_verts[corner_tris[tri_i][0]];
    v2 = corner_verts[corner_tris[tri_i][1]];
    v3 = corner_verts[corner_tris[tri_i][2]];
    interp_weights_tri_v3(
        weights, vert_positions[v1], vert_positions[v2], vert_positions[v3], nearest.co);

    /* Apply object velocity. */
    float hit_vel[3];
    interp_v3_v3v3v3(hit_vel, &vert_vel[v1 * 3], &vert_vel[v2 * 3], &vert_vel[v3 * 3], weights);

    /* Guiding has additional velocity multiplier */
    if (fes->type == FLUID_EFFECTOR_TYPE_GUIDE) {
      mul_v3_fl(hit_vel, fes->vel_multi);

      /* Absolute representation of new object velocity. */
      blender::float3 abs_hit_vel = blender::math::abs(blender::float3(hit_vel));

      /* Absolute representation of current object velocity. */
      blender::float3 abs_vel = blender::math::abs(blender::float3(&velocity_map[index * 3]));

      switch (fes->guide_mode) {
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
          velocity_map[index * 3] = std::min(abs_hit_vel[0], abs_vel[0]);
          velocity_map[index * 3 + 1] = std::min(abs_hit_vel[1], abs_vel[1]);
          velocity_map[index * 3 + 2] = std::min(abs_hit_vel[2], abs_vel[2]);
          break;
        case FLUID_EFFECTOR_GUIDE_MAX:
        default:
          velocity_map[index * 3] = std::max(abs_hit_vel[0], abs_vel[0]);
          velocity_map[index * 3 + 1] = std::max(abs_hit_vel[1], abs_vel[1]);
          velocity_map[index * 3 + 2] = std::max(abs_hit_vel[2], abs_vel[2]);
          break;
      }
    }
    else if (fes->type == FLUID_EFFECTOR_TYPE_COLLISION) {
      velocity_map[index * 3] = hit_vel[0];
      velocity_map[index * 3 + 1] = hit_vel[1];
      velocity_map[index * 3 + 2] = hit_vel[2];
#  ifdef DEBUG_PRINT
      /* Debugging: Print object velocities. */
      printf("setting effector object vel: [%f, %f, %f]\n", hit_vel[0], hit_vel[1], hit_vel[2]);
#  endif
    }
    else {
      /* Should never reach this block. */
      BLI_assert_unreachable();
    }
  }
  else {
    /* Clear velocities at cells that are not moving. */
    copy_v3_fl(velocity_map, 0.0);
  }
}

struct ObstaclesFromDMData {
  FluidEffectorSettings *fes;

  blender::Span<blender::float3> vert_positions;
  blender::Span<int> corner_verts;
  blender::Span<blender::int3> corner_tris;

  blender::bke::BVHTreeFromMesh *tree;
  FluidObjectBB *bb;

  bool has_velocity;
  float *vert_vel;
  int *min, *max, *res;
};

static void obstacles_from_mesh_task_cb(void *__restrict userdata,
                                        const int z,
                                        const TaskParallelTLS *__restrict /*tls*/)
{
  ObstaclesFromDMData *data = static_cast<ObstaclesFromDMData *>(userdata);
  FluidObjectBB *bb = data->bb;

  for (int x = data->min[0]; x < data->max[0]; x++) {
    for (int y = data->min[1]; y < data->max[1]; y++) {
      const int index = manta_get_index(
          x - bb->min[0], bb->res[0], y - bb->min[1], bb->res[1], z - bb->min[2]);
      const float ray_start[3] = {float(x) + 0.5f, float(y) + 0.5f, float(z) + 0.5f};

      /* Calculate levelset values from meshes. Result in bb->distances. */
      update_distances(index,
                       bb->distances,
                       data->tree,
                       ray_start,
                       data->fes->surface_distance,
                       data->fes->flags & FLUID_EFFECTOR_USE_PLANE_INIT);

      /* Calculate object velocities. Result in bb->velocity. */
      update_velocities(data->fes,
                        data->vert_positions,
                        data->corner_verts.data(),
                        data->corner_tris.data(),
                        bb->velocity,
                        index,
                        data->tree,
                        ray_start,
                        data->vert_vel,
                        data->has_velocity);

      /* Increase obstacle count inside of moving obstacles. */
      if (bb->distances[index] < 0) {
        bb->numobjs[index]++;
      }
    }
  }
}

static void obstacles_from_mesh(Object *coll_ob,
                                FluidDomainSettings *fds,
                                FluidEffectorSettings *fes,
                                FluidObjectBB *bb,
                                float dt)
{
  if (fes->mesh) {
    int numverts, i;

    float *vert_vel = nullptr;
    bool has_velocity = false;

    Mesh *mesh = BKE_mesh_copy_for_eval(*fes->mesh);
    blender::MutableSpan<blender::float3> positions = mesh->vert_positions_for_write();

    int min[3], max[3], res[3];

    const blender::Span<int> corner_verts = mesh->corner_verts();
    const blender::Span<blender::int3> corner_tris = mesh->corner_tris();
    numverts = mesh->verts_num;

    /* TODO(sebbas): Make initialization of vertex velocities optional? */
    {
      vert_vel = MEM_calloc_arrayN<float>(3 * size_t(numverts), "manta_obs_velocity");

      if (fes->numverts != numverts || !fes->verts_old) {
        if (fes->verts_old) {
          MEM_freeN(fes->verts_old);
        }

        fes->verts_old = MEM_calloc_arrayN<float>(3 * size_t(numverts), "manta_obs_verts_old");
        fes->numverts = numverts;
      }
      else {
        has_velocity = true;
      }
    }

    /* Transform mesh vertices to domain grid space for fast lookups.
     * This is valid because the mesh is copied above. */
    for (i = 0; i < numverts; i++) {
      float co[3];

      /* Vertex position. */
      mul_m4_v3(coll_ob->object_to_world().ptr(), positions[i]);
      manta_pos_to_cell(fds, positions[i]);

      /* Vertex velocity. */
      add_v3fl_v3fl_v3i(co, positions[i], fds->shift);
      if (has_velocity) {
        sub_v3_v3v3(&vert_vel[i * 3], co, &fes->verts_old[i * 3]);
        mul_v3_fl(&vert_vel[i * 3], 1.0f / dt);
      }
      copy_v3_v3(&fes->verts_old[i * 3], co);

      /* Calculate emission map bounds. */
      bb_boundInsert(bb, positions[i]);
    }

    mesh->tag_positions_changed();

    /* Set emission map.
     * Use 3 cell diagonals as margin (3 * 1.732 = 5.196). */
    int bounds_margin = int(ceil(5.196));
    clamp_bounds_in_domain(fds, bb->min, bb->max, nullptr, nullptr, bounds_margin, dt);
    bb_allocateData(bb, true, false);

    /* Setup loop bounds. */
    for (i = 0; i < 3; i++) {
      min[i] = bb->min[i];
      max[i] = bb->max[i];
      res[i] = bb->res[i];
    }

    /* Skip effector sampling loop if object has disabled effector. */
    bool use_effector = fes->flags & FLUID_EFFECTOR_USE_EFFEC;
    blender::bke::BVHTreeFromMesh tree_data = mesh->bvh_corner_tris();
    if (use_effector && tree_data.tree != nullptr) {
      ObstaclesFromDMData data{};
      data.fes = fes;
      data.vert_positions = positions;
      data.corner_verts = corner_verts;
      data.corner_tris = corner_tris;
      data.tree = &tree_data;
      data.bb = bb;
      data.has_velocity = has_velocity;
      data.vert_vel = vert_vel;
      data.min = min;
      data.max = max;
      data.res = res;

      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.min_iter_per_thread = 2;
      BLI_task_parallel_range(min[2], max[2], &data, obstacles_from_mesh_task_cb, &settings);
    }

    if (vert_vel) {
      MEM_freeN(vert_vel);
    }
    BKE_id_free(nullptr, mesh);
  }
}

static void ensure_obstaclefields(FluidDomainSettings *fds)
{
  if (fds->active_fields & FLUID_DOMAIN_ACTIVE_OBSTACLE) {
    manta_ensure_obstacle(fds->fluid, fds->fmd);
  }
  if (fds->active_fields & FLUID_DOMAIN_ACTIVE_GUIDE) {
    manta_ensure_guiding(fds->fluid, fds->fmd);
  }
  manta_update_pointers(fds->fluid, fds->fmd, false);
}

static void update_obstacleflags(FluidDomainSettings *fds,
                                 Object **coll_ob_array,
                                 int coll_ob_array_len)
{
  int active_fields = fds->active_fields;
  uint coll_index;

  /* First, remove all flags that we want to update. */
  int prev_flags = (FLUID_DOMAIN_ACTIVE_OBSTACLE | FLUID_DOMAIN_ACTIVE_GUIDE);
  active_fields &= ~prev_flags;

  /* Monitor active fields based on flow settings */
  for (coll_index = 0; coll_index < coll_ob_array_len; coll_index++) {
    Object *coll_ob = coll_ob_array[coll_index];
    FluidModifierData *fmd2 = (FluidModifierData *)BKE_modifiers_findby_type(coll_ob,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!fmd2) {
      continue;
    }

    if ((fmd2->type & MOD_FLUID_TYPE_EFFEC) && fmd2->effector) {
      FluidEffectorSettings *fes = fmd2->effector;
      if (!fes) {
        break;
      }
      if (fes->flags & FLUID_EFFECTOR_NEEDS_UPDATE) {
        fes->flags &= ~FLUID_EFFECTOR_NEEDS_UPDATE;
        fds->cache_flag |= FLUID_DOMAIN_OUTDATED_DATA;
      }
      if (fes->type == FLUID_EFFECTOR_TYPE_COLLISION) {
        active_fields |= FLUID_DOMAIN_ACTIVE_OBSTACLE;
      }
      if (fes->type == FLUID_EFFECTOR_TYPE_GUIDE) {
        active_fields |= FLUID_DOMAIN_ACTIVE_GUIDE;
      }
    }
  }
  fds->active_fields = active_fields;
}

static bool escape_effectorobject(Object *flowobj,
                                  FluidDomainSettings *fds,
                                  FluidEffectorSettings * /*fes*/,
                                  int frame)
{
  bool is_static = is_static_object(flowobj);

  bool is_resume = (fds->cache_frame_pause_data == frame);
  bool is_adaptive = (fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN);
  bool is_first_frame = (frame == fds->cache_frame_start);

  /* Cannot use static mode with adaptive domain.
   * The adaptive domain might expand and only later discover the static object. */
  if (is_adaptive) {
    is_static = false;
  }
  /* Skip static effector objects after initial frame. */
  if (is_static && !is_first_frame && !is_resume) {
    return true;
  }
  return false;
}

static void compute_obstaclesemission(Scene *scene,
                                      FluidObjectBB *bb_maps,
                                      Depsgraph *depsgraph,
                                      float dt,
                                      Object **effecobjs,
                                      int frame,
                                      float frame_length,
                                      FluidDomainSettings *fds,
                                      uint numeffecobjs,
                                      float time_per_frame)
{
  bool is_first_frame = (frame == fds->cache_frame_start);

  /* Prepare effector maps. */
  for (int effec_index = 0; effec_index < numeffecobjs; effec_index++) {
    Object *effecobj = effecobjs[effec_index];
    FluidModifierData *fmd2 = (FluidModifierData *)BKE_modifiers_findby_type(effecobj,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!fmd2) {
      continue;
    }

    /* Check for initialized effector object. */
    if ((fmd2->type & MOD_FLUID_TYPE_EFFEC) && fmd2->effector) {
      FluidEffectorSettings *fes = fmd2->effector;
      int subframes = fes->subframes;
      FluidObjectBB *bb = &bb_maps[effec_index];

      /* Optimization: Skip this object under certain conditions. */
      if (escape_effectorobject(effecobj, fds, fes, frame)) {
        continue;
      }

      /* First frame cannot have any subframes because there is (obviously) no previous frame from
       * where subframes could come from. */
      if (is_first_frame) {
        subframes = 0;
      }

      /* More splitting because of emission subframe: If no subframes present, sample_size is 1. */
      float sample_size = 1.0f / float(subframes + 1);
      float subframe_dt = dt * sample_size;

      /* Emission loop. When not using subframes this will loop only once. */
      for (int subframe = 0; subframe <= subframes; subframe++) {

        /* Temporary emission map used when subframes are enabled, i.e. at least one subframe. */
        FluidObjectBB bb_temp = {nullptr};

        /* Set scene time */
        /* Handle emission subframe */
        if ((subframe < subframes || time_per_frame + dt + FLT_EPSILON < frame_length) &&
            !is_first_frame)
        {
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
         * TODO(sebbas): Using BKE_scene_ctime_get(scene) instead of new DEG_get_ctime(depsgraph)
         * as subframes don't work with the latter yet. */
        BKE_object_modifier_update_subframe(depsgraph,
                                            scene,
                                            effecobj,
                                            true,
                                            OBJECT_MODIFIER_UPDATE_SUBFRAME_RECURSION_DEFAULT,
                                            BKE_scene_ctime_get(scene),
                                            eModifierType_Fluid);

        if (subframes) {
          obstacles_from_mesh(effecobj, fds, fes, &bb_temp, subframe_dt);
        }
        else {
          obstacles_from_mesh(effecobj, fds, fes, bb, subframe_dt);
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
                             FluidDomainSettings *fds,
                             float time_per_frame,
                             float frame_length,
                             int frame,
                             float dt)
{
  FluidObjectBB *bb_maps = nullptr;
  Object **effecobjs = nullptr;
  uint numeffecobjs = 0;
  bool is_resume = (fds->cache_frame_pause_data == frame);
  bool is_first_frame = (frame == fds->cache_frame_start);

  effecobjs = BKE_collision_objects_create(
      depsgraph, ob, fds->effector_group, &numeffecobjs, eModifierType_Fluid);

  /* Update all effector related flags and ensure that corresponding grids get initialized. */
  update_obstacleflags(fds, effecobjs, numeffecobjs);
  ensure_obstaclefields(fds);

  /* Allocate effector map for each effector object. */
  bb_maps = MEM_calloc_arrayN<FluidObjectBB>(numeffecobjs, "fluid_effector_bb_maps");

  /* Initialize effector map for each effector object. */
  compute_obstaclesemission(scene,
                            bb_maps,
                            depsgraph,
                            dt,
                            effecobjs,
                            frame,
                            frame_length,
                            fds,
                            numeffecobjs,
                            time_per_frame);

  float *vel_x = manta_get_ob_velocity_x(fds->fluid);
  float *vel_y = manta_get_ob_velocity_y(fds->fluid);
  float *vel_z = manta_get_ob_velocity_z(fds->fluid);
  float *vel_x_guide = manta_get_guide_velocity_x(fds->fluid);
  float *vel_y_guide = manta_get_guide_velocity_y(fds->fluid);
  float *vel_z_guide = manta_get_guide_velocity_z(fds->fluid);
  float *phi_obs_in = manta_get_phiobs_in(fds->fluid);
  float *phi_obsstatic_in = manta_get_phiobsstatic_in(fds->fluid);
  float *phi_guide_in = manta_get_phiguide_in(fds->fluid);
  float *num_obstacles = manta_get_num_obstacle(fds->fluid);
  float *num_guides = manta_get_num_guide(fds->fluid);
  uint z;

  bool use_adaptivedomain = (fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN);

  /* Grid reset before writing again. */
  for (z = 0; z < fds->res[0] * fds->res[1] * fds->res[2]; z++) {

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
    FluidModifierData *fmd2 = (FluidModifierData *)BKE_modifiers_findby_type(effecobj,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!fmd2) {
      continue;
    }

    /* Cannot use static mode with adaptive domain.
     * The adaptive domain might expand and only later in the simulations discover the static
     * object. */
    bool is_static = is_static_object(effecobj) && !use_adaptivedomain;

    /* Check for initialized effector object. */
    if ((fmd2->type & MOD_FLUID_TYPE_EFFEC) && fmd2->effector) {
      FluidEffectorSettings *fes = fmd2->effector;

      /* Optimization: Skip effector objects with disabled effec flag. */
      if ((fes->flags & FLUID_EFFECTOR_USE_EFFEC) == 0) {
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
            dx = gx - fds->res_min[0];
            dy = gy - fds->res_min[1];
            dz = gz - fds->res_min[2];
            d_index = manta_get_index(dx, fds->res[0], dy, fds->res[1], dz);
            /* Make sure emission cell is inside the new domain boundary. */
            if (dx < 0 || dy < 0 || dz < 0 || dx >= fds->res[0] || dy >= fds->res[1] ||
                dz >= fds->res[2])
            {
              continue;
            }

            if (fes->type == FLUID_EFFECTOR_TYPE_COLLISION) {
              float *levelset = ((is_first_frame || is_resume) && is_static) ? phi_obsstatic_in :
                                                                               phi_obs_in;
              apply_effector_fields(fes,
                                    d_index,
                                    distance_map[e_index],
                                    levelset,
                                    numobjs_map[e_index],
                                    num_obstacles,
                                    &velocity_map[e_index * 3],
                                    vel_x,
                                    vel_y,
                                    vel_z);
            }
            if (fes->type == FLUID_EFFECTOR_TYPE_GUIDE) {
              apply_effector_fields(fes,
                                    d_index,
                                    distance_map[e_index],
                                    phi_guide_in,
                                    numobjs_map[e_index],
                                    num_guides,
                                    &velocity_map[e_index * 3],
                                    vel_x_guide,
                                    vel_y_guide,
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

struct EmitFromParticlesData {
  FluidFlowSettings *ffs;
  KDTree_3d *tree;

  FluidObjectBB *bb;
  float *particle_vel;
  int *min, *max, *res;

  float solid;
  float smooth;
};

static void emit_from_particles_task_cb(void *__restrict userdata,
                                        const int z,
                                        const TaskParallelTLS *__restrict /*tls*/)
{
  EmitFromParticlesData *data = static_cast<EmitFromParticlesData *>(userdata);
  FluidFlowSettings *ffs = data->ffs;
  FluidObjectBB *bb = data->bb;

  for (int x = data->min[0]; x < data->max[0]; x++) {
    for (int y = data->min[1]; y < data->max[1]; y++) {
      const int index = manta_get_index(
          x - bb->min[0], bb->res[0], y - bb->min[1], bb->res[1], z - bb->min[2]);
      const float ray_start[3] = {float(x) + 0.5f, float(y) + 0.5f, float(z) + 0.5f};

      /* Find particle distance from the kdtree. */
      KDTreeNearest_3d nearest;
      const float range = data->solid + data->smooth;
      BLI_kdtree_3d_find_nearest(data->tree, ray_start, &nearest);

      if (nearest.dist < range) {
        bb->influence[index] = (nearest.dist < data->solid) ?
                                   1.0f :
                                   (1.0f - (nearest.dist - data->solid) / data->smooth);
        /* Uses particle velocity as initial velocity for smoke. */
        if (ffs->flags & FLUID_FLOW_INITVELOCITY && (ffs->psys->part->phystype != PART_PHYS_NO)) {
          madd_v3_v3fl(
              &bb->velocity[index * 3], &data->particle_vel[nearest.index * 3], ffs->vel_multi);
        }
      }
    }
  }
}

static void emit_from_particles(Object *flow_ob,
                                FluidDomainSettings *fds,
                                FluidFlowSettings *ffs,
                                FluidObjectBB *bb,
                                Depsgraph *depsgraph,
                                Scene *scene,
                                float dt)
{
  if (ffs && ffs->psys && ffs->psys->part &&
      ELEM(ffs->psys->part->type, PART_EMITTER, PART_FLUID)) /* Is particle system selected. */
  {
    ParticleSimulationData sim;
    ParticleSystem *psys = ffs->psys;
    float *particle_pos;
    float *particle_vel;
    int totpart = psys->totpart, totchild;
    int p = 0;
    int valid_particles = 0;
    int bounds_margin = 1;

    /* radius based flow */
    const float solid = ffs->particle_size * 0.5f;
    const float smooth = 0.5f; /* add 0.5 cells of linear falloff to reduce aliasing */
    KDTree_3d *tree = nullptr;

    sim.depsgraph = depsgraph;
    sim.scene = scene;
    sim.ob = flow_ob;
    sim.psys = psys;

    psys_sim_data_init(&sim);

    /* initialize particle cache */
    if (psys->part->type == PART_HAIR) {
      /* TODO: PART_HAIR not supported whatsoever. */
      totchild = 0;
    }
    else {
      totchild = psys->totchild * psys->part->disp / 100;
    }

    particle_pos = MEM_calloc_arrayN<float>(3 * size_t(totpart + totchild),
                                            "manta_flow_particles_pos");
    particle_vel = MEM_calloc_arrayN<float>(3 * size_t(totpart + totchild),
                                            "manta_flow_particles_vel");

    /* setup particle radius emission if enabled */
    if (ffs->flags & FLUID_FLOW_USE_PART_SIZE) {
      tree = BLI_kdtree_3d_new(psys->totpart + psys->totchild);
      bounds_margin = int(ceil(solid + smooth));
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

      /* `DEG_get_ctime(depsgraph)` does not give sub-frame time. */
      state.time = BKE_scene_ctime_get(scene);

      if (psys_get_particle_state(&sim, p, &state, false) == 0) {
        continue;
      }

      /* location */
      pos = &particle_pos[valid_particles * 3];
      copy_v3_v3(pos, state.co);
      manta_pos_to_cell(fds, pos);

      /* velocity */
      vel = &particle_vel[valid_particles * 3];
      copy_v3_v3(vel, state.vel);
      mul_mat3_m4_v3(fds->imat, &particle_vel[valid_particles * 3]);

      if (ffs->flags & FLUID_FLOW_USE_PART_SIZE) {
        BLI_kdtree_3d_insert(tree, valid_particles, pos);
      }

      /* calculate emission map bounds */
      bb_boundInsert(bb, pos);
      valid_particles++;
    }

    /* set emission map */
    clamp_bounds_in_domain(fds, bb->min, bb->max, nullptr, nullptr, bounds_margin, dt);
    bb_allocateData(bb, ffs->flags & FLUID_FLOW_INITVELOCITY, true);

    if (!(ffs->flags & FLUID_FLOW_USE_PART_SIZE)) {
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
        if (ffs->flags & FLUID_FLOW_INITVELOCITY && (psys->part->phystype != PART_PHYS_NO)) {
          madd_v3_v3fl(&bb->velocity[index * 3], &particle_vel[p * 3], ffs->vel_multi);
        }
      } /* particles loop */
    }
    else if (valid_particles > 0) { /* #FLUID_FLOW_USE_PART_SIZE */
      int min[3], max[3], res[3];

      /* setup loop bounds */
      for (int i = 0; i < 3; i++) {
        min[i] = bb->min[i];
        max[i] = bb->max[i];
        res[i] = bb->res[i];
      }

      BLI_kdtree_3d_balance(tree);

      EmitFromParticlesData data{};
      data.ffs = ffs;
      data.tree = tree;
      data.bb = bb;
      data.particle_vel = particle_vel;
      data.min = min;
      data.max = max;
      data.res = res;
      data.solid = solid;
      data.smooth = smooth;

      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.min_iter_per_thread = 2;
      BLI_task_parallel_range(min[2], max[2], &data, emit_from_particles_task_cb, &settings);
    }

    if (ffs->flags & FLUID_FLOW_USE_PART_SIZE) {
      BLI_kdtree_3d_free(tree);
    }

    /* free data */
    if (particle_pos) {
      MEM_freeN(particle_pos);
    }
    if (particle_vel) {
      MEM_freeN(particle_vel);
    }

    psys_sim_data_free(&sim);
  }
}

/* Calculate map of (minimum) distances to flow/obstacle surface. Distances outside mesh are
 * positive, inside negative. */
static void update_distances(int index,
                             float *distance_map,
                             blender::bke::BVHTreeFromMesh *tree_data,
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
     * I.e. the unit cube diagonal or `sqrt(3)`.
     * This value is our nearest neighbor search distance. */
    const float surface_distance = 1.732;
    /* find_nearest uses squared distance. */
    nearest.dist_sq = surface_distance * surface_distance;

    /* Subtract optional surface thickness value and virtually increase the object size. */
    if (surface_thickness) {
      nearest.dist_sq += surface_thickness;
    }

    if (BLI_bvhtree_find_nearest(
            tree_data->tree, ray_start, &nearest, tree_data->nearest_callback, tree_data) != -1)
    {
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

    /* Count ray mesh misses (i.e. no face hit) and cases where the ray direction matches the face
     * normal direction. From this information it can be derived whether a cell is inside or
     * outside the mesh. */
    int miss_count = 0, dir_count = 0;

    for (int i = 0; i < ARRAY_SIZE(ray_dirs); i++) {
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
        miss_count++;
        /* Skip this ray since nothing was hit. */
        continue;
      }

      /* Ray and normal are pointing in opposite directions. */
      if (dot_v3v3(ray_dirs[i], hit_tree.no) <= 0) {
        dir_count++;
      }

      min_dist = std::min(hit_tree.dist, min_dist);
    }

    /* Point lies inside mesh. Use negative sign for distance value.
     * This "if statement" has 2 conditions that can be true for points outside mesh. */
    if (!(miss_count > 0 || dir_count == ARRAY_SIZE(ray_dirs))) {
      min_dist = (-1.0f) * fabsf(min_dist);
    }

    /* Subtract optional surface thickness value and virtually increase the object size. */
    if (surface_thickness) {
      min_dist -= surface_thickness;
    }
  }

  /* Update global distance array but ensure that older entries are not overridden. */
  distance_map[index] = std::min(distance_map[index], min_dist);

  /* Sanity check: Ensure that distances don't explode. */
  CLAMP(distance_map[index], -PHI_MAX, PHI_MAX);
}

static void sample_mesh(FluidFlowSettings *ffs,
                        blender::Span<blender::float3> vert_positions,
                        const blender::Span<blender::float3> vert_normals,
                        const int *corner_verts,
                        const blender::int3 *corner_tris,
                        blender::Span<blender::float2> uv_map,
                        float *influence_map,
                        float *velocity_map,
                        int index,
                        const int base_res[3],
                        const float global_size[3],
                        const float flow_center[3],
                        blender::bke::BVHTreeFromMesh *tree_data,
                        const float ray_start[3],
                        const float *vert_vel,
                        bool has_velocity,
                        int defgrp_index,
                        const MDeformVert *dvert,
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
   * I.e. the unit cube diagonal or `sqrt(3)`.
   * This value is our nearest neighbor search distance. */
  const float surface_distance = 1.732;
  /* find_nearest uses squared distance. */
  nearest.dist_sq = surface_distance * surface_distance;

  bool is_gas_flow = ELEM(
      ffs->type, FLUID_FLOW_TYPE_SMOKE, FLUID_FLOW_TYPE_FIRE, FLUID_FLOW_TYPE_SMOKEFIRE);

  /* Emission strength for gases will be computed below.
   * For liquids it's not needed. Just set to non zero value
   * to allow initial velocity computation. */
  float emission_strength = (is_gas_flow) ? 0.0f : 1.0f;

  /* Emission inside the flow object. */
  if (is_gas_flow && ffs->volume_density) {
    if (BLI_bvhtree_ray_cast(tree_data->tree,
                             ray_start,
                             ray_dir,
                             0.0f,
                             &hit,
                             tree_data->raycast_callback,
                             tree_data) != -1)
    {
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
          volume_factor = ffs->volume_density;
        }
      }
    }
  }

  /* Find the nearest point on the mesh. */
  if (BLI_bvhtree_find_nearest(
          tree_data->tree, ray_start, &nearest, tree_data->nearest_callback, tree_data) != -1)
  {
    float weights[3];
    int v1, v2, v3, tri_i = nearest.index;
    float hit_normal[3];

    /* Calculate barycentric weights for nearest point. */
    v1 = corner_verts[corner_tris[tri_i][0]];
    v2 = corner_verts[corner_tris[tri_i][1]];
    v3 = corner_verts[corner_tris[tri_i][2]];
    interp_weights_tri_v3(
        weights, vert_positions[v1], vert_positions[v2], vert_positions[v3], nearest.co);

    /* Compute emission strength for smoke flow. */
    if (is_gas_flow) {
      /* Emission from surface is based on UI configurable distance value. */
      if (ffs->surface_distance) {
        emission_strength = sqrtf(nearest.dist_sq) / ffs->surface_distance;
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
      if ((ffs->flags & FLUID_FLOW_TEXTUREEMIT) && ffs->noise_texture) {
        float tex_co[3] = {0};
        TexResult texres;

        if (ffs->texture_type == FLUID_FLOW_TEXTURE_MAP_AUTO) {
          tex_co[0] = ((x - flow_center[0]) / base_res[0]) / ffs->texture_size;
          tex_co[1] = ((y - flow_center[1]) / base_res[1]) / ffs->texture_size;
          tex_co[2] = ((z - flow_center[2]) / base_res[2] - ffs->texture_offset) /
                      ffs->texture_size;
        }
        else if (!uv_map.is_empty()) {
          const float *uv[3];
          uv[0] = uv_map[corner_tris[tri_i][0]];
          uv[1] = uv_map[corner_tris[tri_i][1]];
          uv[2] = uv_map[corner_tris[tri_i][2]];

          interp_v2_v2v2v2(tex_co, UNPACK3(uv), weights);

          /* Map texture coord between -1.0f and 1.0f. */
          tex_co[0] = tex_co[0] * 2.0f - 1.0f;
          tex_co[1] = tex_co[1] * 2.0f - 1.0f;
          tex_co[2] = ffs->texture_offset;
        }
        BKE_texture_get_value(ffs->noise_texture, tex_co, &texres, false);
        emission_strength *= texres.tin;
      }
    }

    /* Initial velocity of flow object. Only compute velocity if emission is present. */
    if (ffs->flags & FLUID_FLOW_INITVELOCITY && velocity_map && emission_strength != 0.0) {
      /* Apply normal directional velocity. */
      if (ffs->vel_normal) {
        /* Interpolate vertex normal vectors to get nearest point normal. */
        interp_v3_v3v3v3(
            hit_normal, vert_normals[v1], vert_normals[v2], vert_normals[v3], weights);
        normalize_v3(hit_normal);

        /* Apply normal directional velocity. */
        velocity_map[index * 3] += hit_normal[0] * ffs->vel_normal;
        velocity_map[index * 3 + 1] += hit_normal[1] * ffs->vel_normal;
        velocity_map[index * 3 + 2] += hit_normal[2] * ffs->vel_normal;
      }
      /* Apply object velocity. */
      if (has_velocity && ffs->vel_multi) {
        float hit_vel[3];
        interp_v3_v3v3v3(
            hit_vel, &vert_vel[v1 * 3], &vert_vel[v2 * 3], &vert_vel[v3 * 3], weights);
        velocity_map[index * 3] += hit_vel[0] * ffs->vel_multi;
        velocity_map[index * 3 + 1] += hit_vel[1] * ffs->vel_multi;
        velocity_map[index * 3 + 2] += hit_vel[2] * ffs->vel_multi;
#  ifdef DEBUG_PRINT
        /* Debugging: Print flow object velocities. */
        printf("adding flow object vel: [%f, %f, %f]\n", hit_vel[0], hit_vel[1], hit_vel[2]);
#  endif
      }
      /* Convert XYZ velocities flow settings from world to grid space. */
      float convert_vel[3];
      copy_v3_v3(convert_vel, ffs->vel_coord);
      float time_mult = 1.0 / (25.0f * DT_DEFAULT);
      float size_mult = std::max({base_res[0], base_res[1], base_res[2]}) /
                        std::max({global_size[0], global_size[1], global_size[2]});
      mul_v3_v3fl(convert_vel, ffs->vel_coord, size_mult * time_mult);

      velocity_map[index * 3] += convert_vel[0];
      velocity_map[index * 3 + 1] += convert_vel[1];
      velocity_map[index * 3 + 2] += convert_vel[2];
#  ifdef DEBUG_PRINT
      printf("initial vel: [%f, %f, %f]\n",
             velocity_map[index * 3],
             velocity_map[index * 3 + 1],
             velocity_map[index * 3 + 2]);
#  endif
    }
  }

  /* Apply final influence value but also consider volume initialization factor. */
  influence_map[index] = std::max(volume_factor, emission_strength);
}

struct EmitFromDMData {
  FluidDomainSettings *fds;
  FluidFlowSettings *ffs;

  blender::Span<blender::float3> vert_positions;
  blender::Span<blender::float3> vert_normals;
  blender::Span<int> corner_verts;
  blender::Span<blender::int3> corner_tris;
  blender::Span<blender::float2> uv_map;
  const MDeformVert *dvert;
  int defgrp_index;

  blender::bke::BVHTreeFromMesh *tree;
  FluidObjectBB *bb;

  bool has_velocity;
  float *vert_vel;
  float *flow_center;
  int *min, *max, *res;
};

static void emit_from_mesh_task_cb(void *__restrict userdata,
                                   const int z,
                                   const TaskParallelTLS *__restrict /*tls*/)
{
  EmitFromDMData *data = static_cast<EmitFromDMData *>(userdata);
  FluidObjectBB *bb = data->bb;

  for (int x = data->min[0]; x < data->max[0]; x++) {
    for (int y = data->min[1]; y < data->max[1]; y++) {
      const int index = manta_get_index(
          x - bb->min[0], bb->res[0], y - bb->min[1], bb->res[1], z - bb->min[2]);
      const float ray_start[3] = {float(x) + 0.5f, float(y) + 0.5f, float(z) + 0.5f};

      /* Compute emission only for flow objects that produce fluid (i.e. skip outflow objects).
       * Result in bb->influence. Also computes initial velocities. Result in bb->velocity. */
      if (ELEM(data->ffs->behavior, FLUID_FLOW_BEHAVIOR_GEOMETRY, FLUID_FLOW_BEHAVIOR_INFLOW)) {
        sample_mesh(data->ffs,
                    data->vert_positions,
                    data->vert_normals,
                    data->corner_verts.data(),
                    data->corner_tris.data(),
                    data->uv_map,
                    bb->influence,
                    bb->velocity,
                    index,
                    data->fds->base_res,
                    data->fds->global_size,
                    data->flow_center,
                    data->tree,
                    ray_start,
                    data->vert_vel,
                    data->has_velocity,
                    data->defgrp_index,
                    data->dvert,
                    float(x),
                    float(y),
                    float(z));
      }

      /* Calculate levelset values from meshes. Result in bb->distances. */
      update_distances(index,
                       bb->distances,
                       data->tree,
                       ray_start,
                       data->ffs->surface_distance,
                       data->ffs->flags & FLUID_FLOW_USE_PLANE_INIT);
    }
  }
}

static void emit_from_mesh(
    Object *flow_ob, FluidDomainSettings *fds, FluidFlowSettings *ffs, FluidObjectBB *bb, float dt)
{
  if (ffs->mesh) {
    int i;

    float *vert_vel = nullptr;
    bool has_velocity = false;

    int defgrp_index = ffs->vgroup_density - 1;
    float flow_center[3] = {0};
    int min[3], max[3], res[3];

    /* Copy mesh for thread safety as we modify it.
     * Main issue is its VertArray being modified, then replaced and freed. */
    Mesh *mesh = BKE_mesh_copy_for_eval(*ffs->mesh);
    blender::MutableSpan<blender::float3> positions = mesh->vert_positions_for_write();

    const blender::Span<int> corner_verts = mesh->corner_verts();
    const blender::Span<blender::int3> corner_tris = mesh->corner_tris();
    const int numverts = mesh->verts_num;
    const MDeformVert *dvert = mesh->deform_verts().data();
    const blender::bke::AttributeAccessor attributes = mesh->attributes();
    const blender::VArraySpan uv_map = *attributes.lookup<blender::float2>(
        ffs->uvlayer_name, blender::bke::AttrDomain::Corner);

    if (ffs->flags & FLUID_FLOW_INITVELOCITY) {
      vert_vel = MEM_calloc_arrayN<float>(3 * size_t(numverts), "manta_flow_velocity");

      if (ffs->numverts != numverts || !ffs->verts_old) {
        if (ffs->verts_old) {
          MEM_freeN(ffs->verts_old);
        }
        ffs->verts_old = MEM_calloc_arrayN<float>(3 * size_t(numverts), "manta_flow_verts_old");
        ffs->numverts = numverts;
      }
      else {
        has_velocity = true;
      }
    }

    /* Transform mesh vertices to domain grid space for fast lookups.
     * This is valid because the mesh is copied above. */
    for (i = 0; i < numverts; i++) {
      /* Vertex position. */
      mul_m4_v3(flow_ob->object_to_world().ptr(), positions[i]);
      manta_pos_to_cell(fds, positions[i]);

      /* Vertex velocity. */
      if (ffs->flags & FLUID_FLOW_INITVELOCITY) {
        float co[3];
        add_v3fl_v3fl_v3i(co, positions[i], fds->shift);
        if (has_velocity) {
          sub_v3_v3v3(&vert_vel[i * 3], co, &ffs->verts_old[i * 3]);
          mul_v3_fl(&vert_vel[i * 3], 1.0 / dt);
        }
        copy_v3_v3(&ffs->verts_old[i * 3], co);
      }

      /* Calculate emission map bounds. */
      bb_boundInsert(bb, positions[i]);
    }
    mesh->tag_positions_changed();
    mul_m4_v3(flow_ob->object_to_world().ptr(), flow_center);
    manta_pos_to_cell(fds, flow_center);

    /* Set emission map.
     * Use 3 cell diagonals as margin (3 * 1.732 = 5.196). */
    int bounds_margin = int(ceil(5.196));
    clamp_bounds_in_domain(fds, bb->min, bb->max, nullptr, nullptr, bounds_margin, dt);
    bb_allocateData(bb, ffs->flags & FLUID_FLOW_INITVELOCITY, true);

    /* Setup loop bounds. */
    for (i = 0; i < 3; i++) {
      min[i] = bb->min[i];
      max[i] = bb->max[i];
      res[i] = bb->res[i];
    }

    /* Skip flow sampling loop if object has disabled flow. */
    bool use_flow = ffs->flags & FLUID_FLOW_USE_INFLOW;
    blender::bke::BVHTreeFromMesh tree_data = mesh->bvh_corner_tris();
    if (use_flow && tree_data.tree != nullptr) {

      EmitFromDMData data{};
      data.fds = fds;
      data.ffs = ffs;
      data.vert_positions = positions;
      data.vert_normals = mesh->vert_normals();
      data.corner_verts = corner_verts;
      data.corner_tris = corner_tris;
      data.uv_map = uv_map;
      data.dvert = dvert;
      data.defgrp_index = defgrp_index;
      data.tree = &tree_data;
      data.bb = bb;
      data.has_velocity = has_velocity;
      data.vert_vel = vert_vel;
      data.flow_center = flow_center;
      data.min = min;
      data.max = max;
      data.res = res;

      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.min_iter_per_thread = 2;
      BLI_task_parallel_range(min[2], max[2], &data, emit_from_mesh_task_cb, &settings);
    }

    if (vert_vel) {
      MEM_freeN(vert_vel);
    }
    BKE_id_free(nullptr, mesh);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Fluid Step
 * \{ */

static void adaptive_domain_adjust(
    FluidDomainSettings *fds, Object *ob, FluidObjectBB *bb_maps, uint numflowobj, float dt)
{
  /* calculate domain shift for current frame */
  int new_shift[3] = {0};
  int total_shift[3];
  float frame_shift_f[3];
  float ob_loc[3] = {0};

  mul_m4_v3(ob->object_to_world().ptr(), ob_loc);

  sub_v3_v3v3(frame_shift_f, ob_loc, fds->prev_loc);
  copy_v3_v3(fds->prev_loc, ob_loc);
  /* convert global space shift to local "cell" space */
  mul_mat3_m4_v3(fds->imat, frame_shift_f);
  frame_shift_f[0] = frame_shift_f[0] / fds->cell_size[0];
  frame_shift_f[1] = frame_shift_f[1] / fds->cell_size[1];
  frame_shift_f[2] = frame_shift_f[2] / fds->cell_size[2];
  /* add to total shift */
  add_v3_v3(fds->shift_f, frame_shift_f);
  /* convert to integer */
  total_shift[0] = int(floorf(fds->shift_f[0]));
  total_shift[1] = int(floorf(fds->shift_f[1]));
  total_shift[2] = int(floorf(fds->shift_f[2]));
  int temp_shift[3];
  copy_v3_v3_int(temp_shift, fds->shift);
  sub_v3_v3v3_int(new_shift, total_shift, fds->shift);
  copy_v3_v3_int(fds->shift, total_shift);

  /* calculate new domain boundary points so that smoke doesn't slide on sub-cell movement */
  fds->p0[0] = fds->dp0[0] - fds->cell_size[0] * (fds->shift_f[0] - total_shift[0] - 0.5f);
  fds->p0[1] = fds->dp0[1] - fds->cell_size[1] * (fds->shift_f[1] - total_shift[1] - 0.5f);
  fds->p0[2] = fds->dp0[2] - fds->cell_size[2] * (fds->shift_f[2] - total_shift[2] - 0.5f);
  fds->p1[0] = fds->p0[0] + fds->cell_size[0] * fds->base_res[0];
  fds->p1[1] = fds->p0[1] + fds->cell_size[1] * fds->base_res[1];
  fds->p1[2] = fds->p0[2] + fds->cell_size[2] * fds->base_res[2];

  /* adjust domain resolution */
  const int block_size = fds->noise_scale;
  int min[3] = {32767, 32767, 32767}, max[3] = {-32767, -32767, -32767}, res[3];
  int total_cells = 1, res_changed = 0, shift_changed = 0;
  float min_vel[3], max_vel[3];
  int x, y, z;
  float *density = manta_smoke_get_density(fds->fluid);
  float *fuel = manta_smoke_get_fuel(fds->fluid);
  float *heat = manta_smoke_get_heat(fds->fluid);
  float *bigdensity = manta_noise_get_density(fds->fluid);
  float *bigfuel = manta_noise_get_fuel(fds->fluid);
  float *vx = manta_get_velocity_x(fds->fluid);
  float *vy = manta_get_velocity_y(fds->fluid);
  float *vz = manta_get_velocity_z(fds->fluid);
  int wt_res[3];

  if (fds->flags & FLUID_DOMAIN_USE_NOISE && fds->fluid) {
    manta_noise_get_res(fds->fluid, wt_res);
  }

  INIT_MINMAX(min_vel, max_vel);

  /* Calculate bounds for current domain content */
  for (x = fds->res_min[0]; x < fds->res_max[0]; x++) {
    for (y = fds->res_min[1]; y < fds->res_max[1]; y++) {
      for (z = fds->res_min[2]; z < fds->res_max[2]; z++) {
        int xn = x - new_shift[0];
        int yn = y - new_shift[1];
        int zn = z - new_shift[2];
        int index;
        float max_den;

        /* skip if cell already belongs to new area */
        if (xn >= min[0] && xn <= max[0] && yn >= min[1] && yn <= max[1] && zn >= min[2] &&
            zn <= max[2])
        {
          continue;
        }

        index = manta_get_index(x - fds->res_min[0],
                                fds->res[0],
                                y - fds->res_min[1],
                                fds->res[1],
                                z - fds->res_min[2]);
        max_den = (fuel) ? std::max(density[index], fuel[index]) : density[index];
        max_den = (heat) ? std::max(max_den, heat[index]) : max_den;

        /* Check high resolution bounds if max density isn't already high enough. */
        if (max_den < fds->adapt_threshold && fds->flags & FLUID_DOMAIN_USE_NOISE && fds->fluid) {
          int i, j, k;
          /* high res grid index */
          int xx = (x - fds->res_min[0]) * block_size;
          int yy = (y - fds->res_min[1]) * block_size;
          int zz = (z - fds->res_min[2]) * block_size;

          for (i = 0; i < block_size; i++) {
            for (j = 0; j < block_size; j++) {
              for (k = 0; k < block_size; k++) {
                int big_index = manta_get_index(xx + i, wt_res[0], yy + j, wt_res[1], zz + k);
                float den = (bigfuel) ? std::max(bigdensity[big_index], bigfuel[big_index]) :
                                        bigdensity[big_index];
                max_den = std::max(den, max_den);
              }
            }
          }
        }

        /* content bounds (use shifted coordinates) */
        if (max_den >= fds->adapt_threshold) {
          min[0] = std::min(min[0], xn);
          min[1] = std::min(min[1], yn);
          min[2] = std::min(min[2], zn);
          max[0] = std::max(max[0], xn);
          max[1] = std::max(max[1], yn);
          max[2] = std::max(max[2], zn);
        }

        /* velocity bounds */
        min_vel[0] = std::min(min_vel[0], vx[index]);
        min_vel[1] = std::min(min_vel[1], vy[index]);
        min_vel[2] = std::min(min_vel[2], vz[index]);
        max_vel[0] = std::max(max_vel[0], vx[index]);
        max_vel[1] = std::max(max_vel[1], vy[index]);
        max_vel[2] = std::max(max_vel[2], vz[index]);
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
          if (max_den >= fds->adapt_threshold) {
            min[0] = std::min(min[0], x);
            min[1] = std::min(min[1], y);
            min[2] = std::min(min[2], z);
            max[0] = std::max(max[0], x);
            max[1] = std::max(max[1], y);
            max[2] = std::max(max[2], z);
          }
        }
      }
    }
  }

  /* calculate new bounds based on these values */
  clamp_bounds_in_domain(fds, min, max, min_vel, max_vel, fds->adapt_margin + 1, dt);

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
    if (min[i] != fds->res_min[i] || max[i] != fds->res_max[i]) {
      res_changed = 1;
    }
  }

  if (res_changed || shift_changed) {
    BKE_fluid_reallocate_copy_fluid(
        fds, fds->res, res, fds->res_min, min, fds->res_max, temp_shift, total_shift);

    /* set new domain dimensions */
    copy_v3_v3_int(fds->res_min, min);
    copy_v3_v3_int(fds->res_max, max);
    copy_v3_v3_int(fds->res, res);
    fds->total_cells = total_cells;

    /* Redo adapt time step in manta to refresh solver state (ie time variables) */
    manta_adapt_timestep(fds->fluid);
  }
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
    phiout[index] = std::min(distance_value, phiout[index]);
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

BLI_INLINE void apply_inflow_fields(FluidFlowSettings *ffs,
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
    phi_in[index] = std::min(distance_value, phi_in[index]);
  }

  /* Set emission value for smoke inflow.
   * Ensure that emission value is "maximized". */
  if (emission_in) {
    emission_in[index] = std::max(emission_value, emission_in[index]);
  }

  /* Set inflow for smoke from here on. */
  int absolute_flow = (ffs->flags & FLUID_FLOW_ABSOLUTE);
  float dens_old = (density) ? density[index] : 0.0;
  // float fuel_old = (fuel) ? fuel[index] : 0.0f;  /* UNUSED */
  float dens_flow = (ffs->type == FLUID_FLOW_TYPE_FIRE) ? 0.0f : emission_value * ffs->density;
  float fuel_flow = (fuel) ? emission_value * ffs->fuel_amount : 0.0f;
  /* Set heat inflow. */
  if (heat && heat_in) {
    if (emission_value > 0.0f) {
      heat_in[index] = ADD_IF_LOWER(heat[index], ffs->temperature);
    }
  }

  /* Set density and fuel - absolute mode. */
  if (absolute_flow) {
    if (density && density_in) {
      if (ffs->type != FLUID_FLOW_TYPE_FIRE && dens_flow > density[index]) {
        /* Use std::max to preserve values from other emitters at this cell. */
        density_in[index] = std::max(dens_flow, density_in[index]);
      }
    }
    if (fuel && fuel_in) {
      if (ffs->type != FLUID_FLOW_TYPE_SMOKE && fuel_flow && fuel_flow > fuel[index]) {
        /* Use std::max to preserve values from other emitters at this cell. */
        fuel_in[index] = std::max(fuel_flow, fuel_in[index]);
      }
    }
  }
  /* Set density and fuel - additive mode. */
  else {
    if (density && density_in) {
      if (ffs->type != FLUID_FLOW_TYPE_FIRE) {
        density_in[index] += dens_flow;
        CLAMP(density_in[index], 0.0f, 1.0f);
      }
    }
    if (fuel && fuel_in) {
      if (ffs->type != FLUID_FLOW_TYPE_SMOKE && ffs->fuel_amount) {
        fuel_in[index] += fuel_flow;
        CLAMP(fuel_in[index], 0.0f, 10.0f);
      }
    }
  }

  /* Set color. */
  if (color_r && color_r_in) {
    if (dens_flow) {
      float total_dens = density[index] / (dens_old + dens_flow);
      color_r_in[index] = (color_r[index] + ffs->color[0] * dens_flow) * total_dens;
      color_g_in[index] = (color_g[index] + ffs->color[1] * dens_flow) * total_dens;
      color_b_in[index] = (color_b[index] + ffs->color[2] * dens_flow) * total_dens;
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

static void ensure_flowsfields(FluidDomainSettings *fds)
{
  if (fds->active_fields & FLUID_DOMAIN_ACTIVE_INVEL) {
    manta_ensure_invelocity(fds->fluid, fds->fmd);
  }
  if (fds->active_fields & FLUID_DOMAIN_ACTIVE_OUTFLOW) {
    manta_ensure_outflow(fds->fluid, fds->fmd);
  }
  if (fds->active_fields & FLUID_DOMAIN_ACTIVE_HEAT) {
    manta_smoke_ensure_heat(fds->fluid, fds->fmd);
  }
  if (fds->active_fields & FLUID_DOMAIN_ACTIVE_FIRE) {
    manta_smoke_ensure_fire(fds->fluid, fds->fmd);
  }
  if (fds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS) {
    /* Initialize all smoke with "active_color". */
    manta_smoke_ensure_colors(fds->fluid, fds->fmd);
  }
  if (fds->type == FLUID_DOMAIN_TYPE_LIQUID && (fds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY ||
                                                fds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM ||
                                                fds->particle_type & FLUID_DOMAIN_PARTICLE_TRACER))
  {
    manta_liquid_ensure_sndparts(fds->fluid, fds->fmd);
  }
  manta_update_pointers(fds->fluid, fds->fmd, false);
}

static void update_flowsflags(FluidDomainSettings *fds, Object **flowobjs, int numflowobj)
{
  int active_fields = fds->active_fields;
  uint flow_index;

  /* First, remove all flags that we want to update. */
  int prev_flags = (FLUID_DOMAIN_ACTIVE_INVEL | FLUID_DOMAIN_ACTIVE_OUTFLOW |
                    FLUID_DOMAIN_ACTIVE_HEAT | FLUID_DOMAIN_ACTIVE_FIRE);
  active_fields &= ~prev_flags;

  /* Monitor active fields based on flow settings. */
  for (flow_index = 0; flow_index < numflowobj; flow_index++) {
    Object *flow_ob = flowobjs[flow_index];
    FluidModifierData *fmd2 = (FluidModifierData *)BKE_modifiers_findby_type(flow_ob,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!fmd2) {
      continue;
    }

    /* Activate specific grids if at least one flow object requires this grid. */
    if ((fmd2->type & MOD_FLUID_TYPE_FLOW) && fmd2->flow) {
      FluidFlowSettings *ffs = fmd2->flow;
      if (!ffs) {
        break;
      }
      if (ffs->flags & FLUID_FLOW_NEEDS_UPDATE) {
        ffs->flags &= ~FLUID_FLOW_NEEDS_UPDATE;
        fds->cache_flag |= FLUID_DOMAIN_OUTDATED_DATA;
      }
      if (ffs->flags & FLUID_FLOW_INITVELOCITY) {
        active_fields |= FLUID_DOMAIN_ACTIVE_INVEL;
      }
      if (ffs->behavior == FLUID_FLOW_BEHAVIOR_OUTFLOW) {
        active_fields |= FLUID_DOMAIN_ACTIVE_OUTFLOW;
      }
      /* liquids done from here */
      if (fds->type == FLUID_DOMAIN_TYPE_LIQUID) {
        continue;
      }

      /* Activate heat field if a flow object produces any heat. */
      if (ffs->temperature != 0.0) {
        active_fields |= FLUID_DOMAIN_ACTIVE_HEAT;
      }
      /* Activate fuel field if a flow object is of fire type. */
      if (ffs->fuel_amount != 0.0 || ffs->type == FLUID_FLOW_TYPE_FIRE ||
          ffs->type == FLUID_FLOW_TYPE_SMOKEFIRE)
      {
        active_fields |= FLUID_DOMAIN_ACTIVE_FIRE;
      }
      /* Activate color field if flows add smoke with varying colors. */
      if (ffs->density != 0.0 && ELEM(ffs->type, FLUID_FLOW_TYPE_SMOKE, FLUID_FLOW_TYPE_SMOKEFIRE))
      {
        if (!(active_fields & FLUID_DOMAIN_ACTIVE_COLOR_SET)) {
          copy_v3_v3(fds->active_color, ffs->color);
          active_fields |= FLUID_DOMAIN_ACTIVE_COLOR_SET;
        }
        else if (!equals_v3v3(fds->active_color, ffs->color)) {
          copy_v3_v3(fds->active_color, ffs->color);
          active_fields |= FLUID_DOMAIN_ACTIVE_COLORS;
        }
      }
    }
  }
  /* Monitor active fields based on domain settings. */
  if (fds->type == FLUID_DOMAIN_TYPE_GAS && active_fields & FLUID_DOMAIN_ACTIVE_FIRE) {
    /* Heat is always needed for fire. */
    active_fields |= FLUID_DOMAIN_ACTIVE_HEAT;
    /* Also activate colors if domain smoke color differs from active color. */
    if (!(active_fields & FLUID_DOMAIN_ACTIVE_COLOR_SET)) {
      copy_v3_v3(fds->active_color, fds->flame_smoke_color);
      active_fields |= FLUID_DOMAIN_ACTIVE_COLOR_SET;
    }
    else if (!equals_v3v3(fds->active_color, fds->flame_smoke_color)) {
      copy_v3_v3(fds->active_color, fds->flame_smoke_color);
      active_fields |= FLUID_DOMAIN_ACTIVE_COLORS;
    }
  }
  fds->active_fields = active_fields;
}

static bool escape_flowsobject(Object *flowobj,
                               FluidDomainSettings *fds,
                               FluidFlowSettings *ffs,
                               int frame)
{
  bool use_velocity = (ffs->flags & FLUID_FLOW_INITVELOCITY);
  bool is_static = is_static_object(flowobj);

  bool liquid_flow = ffs->type == FLUID_FLOW_TYPE_LIQUID;
  bool gas_flow = ELEM(
      ffs->type, FLUID_FLOW_TYPE_SMOKE, FLUID_FLOW_TYPE_FIRE, FLUID_FLOW_TYPE_SMOKEFIRE);
  bool is_geometry = (ffs->behavior == FLUID_FLOW_BEHAVIOR_GEOMETRY);

  bool liquid_domain = fds->type == FLUID_DOMAIN_TYPE_LIQUID;
  bool gas_domain = fds->type == FLUID_DOMAIN_TYPE_GAS;
  bool is_adaptive = (fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN);
  bool is_resume = (fds->cache_frame_pause_data == frame);
  bool is_first_frame = (fds->cache_frame_start == frame);

  /* Cannot use static mode with adaptive domain.
   * The adaptive domain might expand and only later discover the static object. */
  if (is_adaptive) {
    is_static = false;
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
   * TODO(sebbas): Also do not use static mode if initial velocities are enabled. */
  if (liquid_flow && is_static && !is_first_frame && !is_resume && !use_velocity) {
    return true;
  }
  return false;
}

static void compute_flowsemission(Scene *scene,
                                  FluidObjectBB *bb_maps,
                                  Depsgraph *depsgraph,
                                  float dt,
                                  Object **flowobjs,
                                  int frame,
                                  float frame_length,
                                  FluidDomainSettings *fds,
                                  uint numflowobjs,
                                  float time_per_frame)
{
  bool is_first_frame = (frame == fds->cache_frame_start);

  /* Prepare flow emission maps. */
  for (int flow_index = 0; flow_index < numflowobjs; flow_index++) {
    Object *flowobj = flowobjs[flow_index];
    FluidModifierData *fmd2 = (FluidModifierData *)BKE_modifiers_findby_type(flowobj,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!fmd2) {
      continue;
    }

    /* Check for initialized flow object. */
    if ((fmd2->type & MOD_FLUID_TYPE_FLOW) && fmd2->flow) {
      FluidFlowSettings *ffs = fmd2->flow;
      int subframes = ffs->subframes;
      FluidObjectBB *bb = &bb_maps[flow_index];

      /* Optimization: Skip this object under certain conditions. */
      if (escape_flowsobject(flowobj, fds, ffs, frame)) {
        continue;
      }

      /* First frame cannot have any subframes because there is (obviously) no previous frame from
       * where subframes could come from. */
      if (is_first_frame) {
        subframes = 0;
      }

      /* More splitting because of emission subframe: If no subframes present, sample_size is 1. */
      float sample_size = 1.0f / float(subframes + 1);
      float subframe_dt = dt * sample_size;

      /* Emission loop. When not using subframes this will loop only once. */
      for (int subframe = 0; subframe <= subframes; subframe++) {
        /* Temporary emission map used when subframes are enabled, i.e. at least one subframe. */
        FluidObjectBB bb_temp = {nullptr};

        /* Set scene time */
        if ((subframe < subframes || time_per_frame + dt + FLT_EPSILON < frame_length) &&
            !is_first_frame)
        {
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
         * TODO(sebbas): Using BKE_scene_ctime_get(scene) instead of new DEG_get_ctime(depsgraph)
         * as subframes don't work with the latter yet. */
        BKE_object_modifier_update_subframe(depsgraph,
                                            scene,
                                            flowobj,
                                            true,
                                            OBJECT_MODIFIER_UPDATE_SUBFRAME_RECURSION_DEFAULT,
                                            BKE_scene_ctime_get(scene),
                                            eModifierType_Fluid);

        /* Emission from particles. */
        if (ffs->source == FLUID_FLOW_SOURCE_PARTICLES) {
          if (subframes) {
            emit_from_particles(flowobj, fds, ffs, &bb_temp, depsgraph, scene, subframe_dt);
          }
          else {
            emit_from_particles(flowobj, fds, ffs, bb, depsgraph, scene, subframe_dt);
          }
        }
        /* Emission from mesh. */
        else if (ffs->source == FLUID_FLOW_SOURCE_MESH) {
          if (subframes) {
            emit_from_mesh(flowobj, fds, ffs, &bb_temp, subframe_dt);
          }
          else {
            emit_from_mesh(flowobj, fds, ffs, bb, subframe_dt);
          }
        }
        else {
          printf("Error: unknown flow emission source\n");
        }

        /* If this we emitted with temp emission map in this loop (subframe emission), we combine
         * the temp map with the original emission map. */
        if (subframes) {
          /* Combine emission maps. */
          bb_combineMaps(bb, &bb_temp, !(ffs->flags & FLUID_FLOW_ABSOLUTE), sample_size);
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

static void update_flowsfluids(Depsgraph *depsgraph,
                               Scene *scene,
                               Object *ob,
                               FluidDomainSettings *fds,
                               float time_per_frame,
                               float frame_length,
                               int frame,
                               float dt)
{
  FluidObjectBB *bb_maps = nullptr;
  Object **flowobjs = nullptr;
  uint numflowobjs = 0;
  bool is_resume = (fds->cache_frame_pause_data == frame);
  bool is_first_frame = (fds->cache_frame_start == frame);

  flowobjs = BKE_collision_objects_create(
      depsgraph, ob, fds->fluid_group, &numflowobjs, eModifierType_Fluid);

  /* Update all flow related flags and ensure that corresponding grids get initialized. */
  update_flowsflags(fds, flowobjs, numflowobjs);
  ensure_flowsfields(fds);

  /* Allocate emission map for each flow object. */
  bb_maps = MEM_calloc_arrayN<FluidObjectBB>(numflowobjs, "fluid_flow_bb_maps");

  /* Initialize emission map for each flow object. */
  compute_flowsemission(scene,
                        bb_maps,
                        depsgraph,
                        dt,
                        flowobjs,
                        frame,
                        frame_length,
                        fds,
                        numflowobjs,
                        time_per_frame);

  /* Adjust domain size if needed. Only do this once for every frame. */
  if (fds->type == FLUID_DOMAIN_TYPE_GAS && fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) {
    adaptive_domain_adjust(fds, ob, bb_maps, numflowobjs, dt);
  }

  float *phi_in = manta_get_phi_in(fds->fluid);
  float *phistatic_in = manta_get_phistatic_in(fds->fluid);
  float *phiout_in = manta_get_phiout_in(fds->fluid);
  float *phioutstatic_in = manta_get_phioutstatic_in(fds->fluid);

  float *density = manta_smoke_get_density(fds->fluid);
  float *color_r = manta_smoke_get_color_r(fds->fluid);
  float *color_g = manta_smoke_get_color_g(fds->fluid);
  float *color_b = manta_smoke_get_color_b(fds->fluid);
  float *fuel = manta_smoke_get_fuel(fds->fluid);
  float *heat = manta_smoke_get_heat(fds->fluid);
  float *react = manta_smoke_get_react(fds->fluid);

  float *density_in = manta_smoke_get_density_in(fds->fluid);
  float *heat_in = manta_smoke_get_heat_in(fds->fluid);
  float *color_r_in = manta_smoke_get_color_r_in(fds->fluid);
  float *color_g_in = manta_smoke_get_color_g_in(fds->fluid);
  float *color_b_in = manta_smoke_get_color_b_in(fds->fluid);
  float *fuel_in = manta_smoke_get_fuel_in(fds->fluid);
  float *react_in = manta_smoke_get_react_in(fds->fluid);
  float *emission_in = manta_smoke_get_emission_in(fds->fluid);

  float *velx_initial = manta_get_in_velocity_x(fds->fluid);
  float *vely_initial = manta_get_in_velocity_y(fds->fluid);
  float *velz_initial = manta_get_in_velocity_z(fds->fluid);

  float *forcex = manta_get_force_x(fds->fluid);
  float *forcey = manta_get_force_y(fds->fluid);
  float *forcez = manta_get_force_z(fds->fluid);

  BLI_assert(forcex && forcey && forcez);

  /* Either all or no components have to exist. */
  BLI_assert((color_r && color_g && color_b) || (!color_r && !color_g && !color_b));
  BLI_assert((color_r_in && color_g_in && color_b_in) ||
             (!color_r_in && !color_g_in && !color_b_in));
  BLI_assert((velx_initial && vely_initial && velz_initial) ||
             (!velx_initial && !vely_initial && !velz_initial));

  uint z;
  /* Grid reset before writing again. */
  for (z = 0; z < fds->res[0] * fds->res[1] * fds->res[2]; z++) {
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
    if (color_r_in && color_g_in && color_b_in) {
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
    if (velx_initial && vely_initial && velz_initial) {
      velx_initial[z] = 0.0f;
      vely_initial[z] = 0.0f;
      velz_initial[z] = 0.0f;
    }
    /* Reset forces here as update_effectors() is skipped when no external forces are present. */
    forcex[z] = 0.0f;
    forcey[z] = 0.0f;
    forcez[z] = 0.0f;
  }

  /* Apply emission data for every flow object. */
  for (int flow_index = 0; flow_index < numflowobjs; flow_index++) {
    Object *flowobj = flowobjs[flow_index];
    FluidModifierData *fmd2 = (FluidModifierData *)BKE_modifiers_findby_type(flowobj,
                                                                             eModifierType_Fluid);

    /* Sanity check. */
    if (!fmd2) {
      continue;
    }

    /* Check for initialized flow object. */
    if ((fmd2->type & MOD_FLUID_TYPE_FLOW) && fmd2->flow) {
      FluidFlowSettings *ffs = fmd2->flow;

      bool is_inflow = (ffs->behavior == FLUID_FLOW_BEHAVIOR_INFLOW);
      bool is_geometry = (ffs->behavior == FLUID_FLOW_BEHAVIOR_GEOMETRY);
      bool is_outflow = (ffs->behavior == FLUID_FLOW_BEHAVIOR_OUTFLOW);
      bool is_static = is_static_object(flowobj) &&
                       ((fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) == 0);

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
            dx = gx - fds->res_min[0];
            dy = gy - fds->res_min[1];
            dz = gz - fds->res_min[2];
            d_index = manta_get_index(dx, fds->res[0], dy, fds->res[1], dz);
            /* Make sure emission cell is inside the new domain boundary. */
            if (dx < 0 || dy < 0 || dz < 0 || dx >= fds->res[0] || dy >= fds->res[1] ||
                dz >= fds->res[2])
            {
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
              apply_inflow_fields(ffs,
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
              apply_inflow_fields(ffs,
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
              if (ffs->flags & FLUID_FLOW_INITVELOCITY) {
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

struct UpdateEffectorsData {
  Scene *scene;
  FluidDomainSettings *fds;
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
};

static void update_effectors_task_cb(void *__restrict userdata,
                                     const int x,
                                     const TaskParallelTLS *__restrict /*tls*/)
{
  UpdateEffectorsData *data = static_cast<UpdateEffectorsData *>(userdata);
  FluidDomainSettings *fds = data->fds;

  for (int y = 0; y < fds->res[1]; y++) {
    for (int z = 0; z < fds->res[2]; z++) {
      EffectedPoint epoint;
      float mag;
      float voxel_center[3] = {0, 0, 0}, vel[3] = {0, 0, 0}, retvel[3] = {0, 0, 0};
      const uint index = manta_get_index(x, fds->res[0], y, fds->res[1], z);

      if ((data->fuel && std::max(data->density[index], data->fuel[index]) < FLT_EPSILON) ||
          (!data->fuel && data->density && data->density[index] < FLT_EPSILON) ||
          (data->phi_obs_in && data->phi_obs_in[index] < 0.0f) ||
          data->flags[index] & 2) /* Manta-flow convention: `2 == FlagObstacle`. */
      {
        continue;
      }

      /* Get velocities from manta grid space and convert to blender units. */
      vel[0] = data->velocity_x[index];
      vel[1] = data->velocity_y[index];
      vel[2] = data->velocity_z[index];
      mul_v3_fl(vel, fds->dx);

      /* Convert vel to global space. */
      mag = len_v3(vel);
      mul_mat3_m4_v3(fds->obmat, vel);
      normalize_v3(vel);
      mul_v3_fl(vel, mag);

      voxel_center[0] = fds->p0[0] + fds->cell_size[0] * (float(x + fds->res_min[0]) + 0.5f);
      voxel_center[1] = fds->p0[1] + fds->cell_size[1] * (float(y + fds->res_min[1]) + 0.5f);
      voxel_center[2] = fds->p0[2] + fds->cell_size[2] * (float(z + fds->res_min[2]) + 0.5f);
      mul_m4_v3(fds->obmat, voxel_center);

      /* Do effectors. */
      pd_point_from_loc(data->scene, voxel_center, vel, index, &epoint);
      BKE_effectors_apply(
          data->effectors, nullptr, fds->effector_weights, &epoint, retvel, nullptr, nullptr);

      /* Convert retvel to local space. */
      mag = len_v3(retvel);
      mul_mat3_m4_v3(fds->imat, retvel);
      normalize_v3(retvel);
      mul_v3_fl(retvel, mag);

      /* Copy computed force to fluid solver forces. */
      mul_v3_fl(retvel, 0.2f);       /* Factor from 0e6820cc5d62. */
      clamp_v3(retvel, -1.0f, 1.0f); /* Restrict forces to +-1 interval. */
      data->force_x[index] = retvel[0];
      data->force_y[index] = retvel[1];
      data->force_z[index] = retvel[2];

#  ifdef DEBUG_PRINT
      /* Debugging: Print forces. */
      printf("setting force: [%f, %f, %f]\n",
             data->force_x[index],
             data->force_y[index],
             data->force_z[index]);
#  endif
    }
  }
}

static void update_effectors(
    Depsgraph *depsgraph, Scene *scene, Object *ob, FluidDomainSettings *fds, float /*dt*/)
{
  ListBase *effectors;
  /* make sure smoke flow influence is 0.0f */
  fds->effector_weights->weight[PFIELD_FLUIDFLOW] = 0.0f;
  effectors = BKE_effectors_create(depsgraph, ob, nullptr, fds->effector_weights, false);

  if (effectors) {
    /* Precalculate wind forces. */
    UpdateEffectorsData data;
    data.scene = scene;
    data.fds = fds;
    data.effectors = effectors;
    data.density = manta_smoke_get_density(fds->fluid);
    data.fuel = manta_smoke_get_fuel(fds->fluid);
    data.force_x = manta_get_force_x(fds->fluid);
    data.force_y = manta_get_force_y(fds->fluid);
    data.force_z = manta_get_force_z(fds->fluid);
    data.velocity_x = manta_get_velocity_x(fds->fluid);
    data.velocity_y = manta_get_velocity_y(fds->fluid);
    data.velocity_z = manta_get_velocity_z(fds->fluid);
    data.flags = manta_smoke_get_flags(fds->fluid);
    data.phi_obs_in = manta_get_phiobs_in(fds->fluid);

    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = 2;
    BLI_task_parallel_range(0, fds->res[0], &data, update_effectors_task_cb, &settings);
  }

  BKE_effectors_free(effectors);
}

static Mesh *create_liquid_geometry(FluidDomainSettings *fds,
                                    Scene *scene,
                                    Mesh *orgmesh,
                                    Object *ob)
{
  using namespace blender;
  using namespace blender::bke;
  Mesh *mesh;
  float min[3];
  float max[3];
  float size[3];
  float cell_size_scaled[3];

  const AttributeAccessor orig_attributes = orgmesh->attributes();
  const VArraySpan orig_material_indices = *orig_attributes.lookup<int>("material_index",
                                                                        AttrDomain::Face);
  const short mp_mat_nr = orig_material_indices.is_empty() ? 0 : orig_material_indices[0];

  int i;
  int num_verts, num_faces;

  if (!fds->fluid) {
    return nullptr;
  }

  num_verts = manta_liquid_get_num_verts(fds->fluid);
  num_faces = manta_liquid_get_num_triangles(fds->fluid);

#  ifdef DEBUG_PRINT
  /* Debugging: Print number of vertices, normals, and faces. */
  printf("num_verts: %d, num_faces: %d\n", num_verts, num_faces);
#  endif

  if (!num_verts || !num_faces) {
    return nullptr;
  }

  mesh = BKE_mesh_new_nomain(num_verts, 0, num_faces, num_faces * 3);
  if (!mesh) {
    return nullptr;
  }
  blender::MutableSpan<blender::float3> positions = mesh->vert_positions_for_write();
  blender::MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  blender::MutableSpan<int> corner_verts = mesh->corner_verts_for_write();

  if (orgmesh->attributes().domain_size(AttrDomain::Face) > 0) {
    const bool is_sharp = orgmesh->attributes()
                              .lookup_or_default<bool>("sharp_face", AttrDomain::Face, false)
                              .varray[0];
    mesh_smooth_set(*mesh, !is_sharp);
  }

  /* Get size (dimension) but considering scaling. */
  copy_v3_v3(cell_size_scaled, fds->cell_size);
  mul_v3_v3(cell_size_scaled, ob->scale);
  madd_v3fl_v3fl_v3fl_v3i(min, fds->p0, cell_size_scaled, fds->res_min);
  madd_v3fl_v3fl_v3fl_v3i(max, fds->p0, cell_size_scaled, fds->res_max);
  sub_v3_v3v3(size, max, min);

  /* Biggest dimension will be used for up-scaling. */
  float max_size = std::max({size[0], size[1], size[2]});

  float co_scale[3];
  co_scale[0] = max_size / ob->scale[0];
  co_scale[1] = max_size / ob->scale[1];
  co_scale[2] = max_size / ob->scale[2];

  float co_offset[3];
  co_offset[0] = (fds->p0[0] + fds->p1[0]) / 2.0f;
  co_offset[1] = (fds->p0[1] + fds->p1[1]) / 2.0f;
  co_offset[2] = (fds->p0[2] + fds->p1[2]) / 2.0f;

  /* Velocities. */
  /* If needed, vertex velocities will be read too. */
  bool use_speedvectors = fds->flags & FLUID_DOMAIN_USE_SPEED_VECTORS;
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  SpanAttributeWriter<float3> velocities;
  float time_mult = fds->dx / (DT_DEFAULT * (25.0f / scene->frames_per_second()));

  if (use_speedvectors) {
    velocities = attributes.lookup_or_add_for_write_only_span<float3>("velocity",
                                                                      AttrDomain::Point);
  }

  /* Loop for vertices and normals. */
  for (i = 0; i < num_verts; i++) {

    /* Vertices (data is normalized cube around domain origin). */
    positions[i][0] = manta_liquid_get_vertex_x_at(fds->fluid, i);
    positions[i][1] = manta_liquid_get_vertex_y_at(fds->fluid, i);
    positions[i][2] = manta_liquid_get_vertex_z_at(fds->fluid, i);

    /* Adjust coordinates from Mantaflow to match viewport scaling. */
    float tmp[3] = {float(fds->res[0]), float(fds->res[1]), float(fds->res[2])};
    /* Scale to unit cube around 0. */
    mul_v3_fl(tmp, fds->mesh_scale * 0.5f);
    sub_v3_v3(positions[i], tmp);
    /* Apply scaling of domain object. */
    mul_v3_fl(positions[i], fds->dx / fds->mesh_scale);

    mul_v3_v3(positions[i], co_scale);
    add_v3_v3(positions[i], co_offset);

#  ifdef DEBUG_PRINT
    /* Debugging: Print coordinates of vertices. */
    printf("positions[i][0]: %f, positions[i][1]: %f, positions[i][2]: %f\n",
           positions[i][0],
           positions[i][1],
           positions[i][2]);
#  endif

#  ifdef DEBUG_PRINT
    /* Debugging: Print coordinates of normals. */
    printf("no_s[0]: %d, no_s[1]: %d, no_s[2]: %d\n", no_s[0], no_s[1], no_s[2]);
#  endif

    if (use_speedvectors) {
      velocities.span[i].x = manta_liquid_get_vertvel_x_at(fds->fluid, i) * time_mult;
      velocities.span[i].y = manta_liquid_get_vertvel_y_at(fds->fluid, i) * time_mult;
      velocities.span[i].z = manta_liquid_get_vertvel_z_at(fds->fluid, i) * time_mult;
#  ifdef DEBUG_PRINT
      /* Debugging: Print velocities of vertices. */
      printf("velocities[%d].x: %f, velocities[%d].y: %f, velocities[%d].z: %f\n",
             i,
             velocities.span[i].x,
             i,
             velocities.span[i].y,
             i,
             velocities.span[i].z);
#  endif
    }
  }

  bke::SpanAttributeWriter material_indices = attributes.lookup_or_add_for_write_span<int>(
      "material_index", AttrDomain::Face);

  /* Loop for triangles. */
  for (const int i : face_offsets.index_range().drop_back(1)) {
    /* Initialize from existing face. */
    material_indices.span[i] = mp_mat_nr;

    face_offsets[i] = i * 3;

    corner_verts[i * 3 + 0] = manta_liquid_get_triangle_x_at(fds->fluid, i);
    corner_verts[i * 3 + 1] = manta_liquid_get_triangle_y_at(fds->fluid, i);
    corner_verts[i * 3 + 2] = manta_liquid_get_triangle_z_at(fds->fluid, i);
#  ifdef DEBUG_PRINT
    /* Debugging: Print mesh faces. */
    printf("mloops[0].v: %d, mloops[1].v: %d, mloops[2].v: %d\n",
           mloops[0].v,
           mloops[1].v,
           mloops[2].v);
#  endif
  }

  velocities.finish();
  material_indices.finish();

  mesh_calc_edges(*mesh, false, false);

  return mesh;
}

static Mesh *create_smoke_geometry(FluidDomainSettings *fds, Mesh *orgmesh, Object *ob)
{
  using namespace blender;
  using namespace blender::bke;
  Mesh *result;
  float min[3];
  float max[3];
  float *co;
  int *corner_vert;

  int num_verts = 8;
  int num_faces = 6;
  float ob_loc[3] = {0};
  float ob_cache_loc[3] = {0};

  /* Just copy existing mesh if there is no content or if the adaptive domain is not being used. */
  if (fds->total_cells <= 1 || (fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) == 0) {
    return BKE_mesh_copy_for_eval(*orgmesh);
  }

  result = BKE_mesh_new_nomain(num_verts, 0, num_faces, num_faces * 4);
  blender::MutableSpan<blender::float3> positions = result->vert_positions_for_write();
  blender::MutableSpan<int> face_offsets = result->face_offsets_for_write();
  blender::MutableSpan<int> corner_verts = result->corner_verts_for_write();

  if (num_verts) {
    /* Volume bounds. */
    madd_v3fl_v3fl_v3fl_v3i(min, fds->p0, fds->cell_size, fds->res_min);
    madd_v3fl_v3fl_v3fl_v3i(max, fds->p0, fds->cell_size, fds->res_max);

    /* Set vertices of smoke BB. Especially important, when BB changes (adaptive domain). */
    /* Top slab */
    co = positions[0];
    co[0] = min[0];
    co[1] = min[1];
    co[2] = max[2];
    co = positions[1];
    co[0] = max[0];
    co[1] = min[1];
    co[2] = max[2];
    co = positions[2];
    co[0] = max[0];
    co[1] = max[1];
    co[2] = max[2];
    co = positions[3];
    co[0] = min[0];
    co[1] = max[1];
    co[2] = max[2];
    /* Bottom slab. */
    co = positions[4];
    co[0] = min[0];
    co[1] = min[1];
    co[2] = min[2];
    co = positions[5];
    co[0] = max[0];
    co[1] = min[1];
    co[2] = min[2];
    co = positions[6];
    co[0] = max[0];
    co[1] = max[1];
    co[2] = min[2];
    co = positions[7];
    co[0] = min[0];
    co[1] = max[1];
    co[2] = min[2];

    face_offsets.fill(4);
    blender::offset_indices::accumulate_counts_to_offsets(face_offsets);

    /* Create faces. */
    /* Top side. */
    corner_vert = &corner_verts[0 * 4];
    corner_vert[0] = 0;
    corner_vert[1] = 1;
    corner_vert[2] = 2;
    corner_vert[3] = 3;
    /* Right side. */
    corner_vert = &corner_verts[1 * 4];
    corner_vert[0] = 2;
    corner_vert[1] = 1;
    corner_vert[2] = 5;
    corner_vert[3] = 6;
    /* Bottom side. */
    corner_vert = &corner_verts[2 * 4];
    corner_vert[0] = 7;
    corner_vert[1] = 6;
    corner_vert[2] = 5;
    corner_vert[3] = 4;
    /* Left side. */
    corner_vert = &corner_verts[3 * 4];
    corner_vert[0] = 0;
    corner_vert[1] = 3;
    corner_vert[2] = 7;
    corner_vert[3] = 4;
    /* Front side. */
    corner_vert = &corner_verts[4 * 4];
    corner_vert[0] = 3;
    corner_vert[1] = 2;
    corner_vert[2] = 6;
    corner_vert[3] = 7;
    /* Back side. */
    corner_vert = &corner_verts[5 * 4];
    corner_vert[0] = 1;
    corner_vert[1] = 0;
    corner_vert[2] = 4;
    corner_vert[3] = 5;

    /* Calculate required shift to match domain's global position
     * it was originally simulated at (if object moves without manta step). */
    invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
    mul_m4_v3(ob->object_to_world().ptr(), ob_loc);
    mul_m4_v3(fds->obmat, ob_cache_loc);
    sub_v3_v3v3(fds->obj_shift_f, ob_cache_loc, ob_loc);
    /* Convert shift to local space and apply to vertices. */
    mul_mat3_m4_v3(ob->world_to_object().ptr(), fds->obj_shift_f);
    /* Apply shift to vertices. */
    for (int i = 0; i < num_verts; i++) {
      add_v3_v3(positions[i], fds->obj_shift_f);
    }
  }

  mesh_calc_edges(*result, false, false);
  return result;
}

static int manta_step(
    Depsgraph *depsgraph, Scene *scene, Object *ob, Mesh *mesh, FluidModifierData *fmd, int frame)
{
  FluidDomainSettings *fds = fmd->domain;
  float dt, frame_length, time_total, time_total_old;
  float time_per_frame;
  bool init_resolution = true;

  /* Store baking success - bake might be aborted anytime by user. */
  int result = 1;
  int mode = fds->cache_type;
  bool mode_replay = (mode == FLUID_DOMAIN_CACHE_REPLAY);

  /* Update object state. */
  invert_m4_m4(fds->imat, ob->object_to_world().ptr());
  copy_m4_m4(fds->obmat, ob->object_to_world().ptr());

  /* Gas domain might use adaptive domain. */
  if (fds->type == FLUID_DOMAIN_TYPE_GAS) {
    init_resolution = (fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) != 0;
  }
  manta_set_domain_from_mesh(fds, ob, mesh, init_resolution);

  /* Use local variables for adaptive loop, dt can change. */
  frame_length = fds->frame_length;
  dt = fds->dt;
  time_per_frame = 0;
  time_total = fds->time_total;
  /* Keep track of original total time to correct small errors at end of step. */
  time_total_old = fds->time_total;

  std::scoped_lock lock(object_update_lock);

  /* Loop as long as time_per_frame (sum of sub dt's) does not exceed actual frame-length. */
  while (time_per_frame + FLT_EPSILON < frame_length) {
    manta_adapt_timestep(fds->fluid);
    dt = manta_get_timestep(fds->fluid);

    /* Save adapted dt so that MANTA object can access it (important when adaptive domain creates
     * new MANTA object). */
    fds->dt = dt;

    /* Calculate inflow geometry. */
    update_flowsfluids(depsgraph, scene, ob, fds, time_per_frame, frame_length, frame, dt);

    /* If user requested stop, quit baking */
    if (G.is_break && !mode_replay) {
      result = 0;
      break;
    }

    manta_update_variables(fds->fluid, fmd);

    /* Calculate obstacle geometry. */
    update_obstacles(depsgraph, scene, ob, fds, time_per_frame, frame_length, frame, dt);

    /* If user requested stop, quit baking */
    if (G.is_break && !mode_replay) {
      result = 0;
      break;
    }

    /* Only bake if the domain is bigger than one cell (important for adaptive domain). */
    if (fds->total_cells > 1) {
      update_effectors(depsgraph, scene, ob, fds, dt);
      manta_bake_data(fds->fluid, fmd, frame);
    }

    /* Count for how long this while loop is running. */
    time_per_frame += dt;
    time_total += dt;

    fds->time_per_frame = time_per_frame;
    fds->time_total = time_total;
  }

  /* Total time must not exceed frame-count times frame-length. Correct tiny errors here. */
  CLAMP_MAX(fds->time_total, time_total_old + fds->frame_length);

  /* Compute shadow grid for gas simulations. Make sure to skip if bake job was canceled early. */
  if (fds->type == FLUID_DOMAIN_TYPE_GAS && result) {
    manta_smoke_calc_transparency(
        fds, DEG_get_evaluated_scene(depsgraph), DEG_get_evaluated_view_layer(depsgraph));
  }

  return result;
}

static void manta_guiding(
    Depsgraph *depsgraph, Scene *scene, Object *ob, FluidModifierData *fmd, int frame)
{
  FluidDomainSettings *fds = fmd->domain;
  float dt = DT_DEFAULT * (25.0f / scene->frames_per_second()) * fds->time_scale;

  std::scoped_lock lock(object_update_lock);

  update_obstacles(depsgraph, scene, ob, fds, dt, dt, frame, dt);
  manta_bake_guiding(fds->fluid, fmd, frame);
}

static void fluid_modifier_processFlow(FluidModifierData *fmd,
                                       Depsgraph *depsgraph,
                                       Scene *scene,
                                       Object *ob,
                                       Mesh *mesh,
                                       const int scene_framenr)
{
  if (scene_framenr >= fmd->time) {
    fluid_modifier_init(fmd, depsgraph, ob, scene, mesh);
  }

  if (fmd->flow) {
    if (fmd->flow->mesh) {
      BKE_id_free(nullptr, fmd->flow->mesh);
    }
    fmd->flow->mesh = BKE_mesh_copy_for_eval(*mesh);
  }

  if (scene_framenr > fmd->time) {
    fmd->time = scene_framenr;
  }
  else if (scene_framenr < fmd->time) {
    fmd->time = scene_framenr;
    fluid_modifier_reset_ex(fmd, false);
  }
}

static void fluid_modifier_processEffector(FluidModifierData *fmd,
                                           Depsgraph *depsgraph,
                                           Scene *scene,
                                           Object *ob,
                                           Mesh *mesh,
                                           const int scene_framenr)
{
  if (scene_framenr >= fmd->time) {
    fluid_modifier_init(fmd, depsgraph, ob, scene, mesh);
  }

  if (fmd->effector) {
    if (fmd->effector->mesh) {
      BKE_id_free(nullptr, fmd->effector->mesh);
    }
    fmd->effector->mesh = BKE_mesh_copy_for_eval(*mesh);
  }

  if (scene_framenr > fmd->time) {
    fmd->time = scene_framenr;
  }
  else if (scene_framenr < fmd->time) {
    fmd->time = scene_framenr;
    fluid_modifier_reset_ex(fmd, false);
  }
}

static void fluid_modifier_processDomain(FluidModifierData *fmd,
                                         Depsgraph *depsgraph,
                                         Scene *scene,
                                         Object *ob,
                                         Mesh *mesh,
                                         const int scene_framenr)
{
  FluidDomainSettings *fds = fmd->domain;
  Object *guide_parent = nullptr;
  Object **objs = nullptr;
  uint numobj = 0;
  FluidModifierData *fmd_parent = nullptr;

  bool is_startframe, has_advanced;
  is_startframe = (scene_framenr == fds->cache_frame_start);
  has_advanced = (scene_framenr == fmd->time + 1);
  int mode = fds->cache_type;

  /* Do not process modifier if current frame is out of cache range. */
  bool escape = false;
  switch (mode) {
    case FLUID_DOMAIN_CACHE_ALL:
    case FLUID_DOMAIN_CACHE_MODULAR:
      if (fds->cache_frame_offset > 0) {
        if (scene_framenr < fds->cache_frame_start ||
            scene_framenr > fds->cache_frame_end + fds->cache_frame_offset)
        {
          escape = true;
        }
      }
      else {
        if (scene_framenr < fds->cache_frame_start + fds->cache_frame_offset ||
            scene_framenr > fds->cache_frame_end)
        {
          escape = true;
        }
      }
      break;
    case FLUID_DOMAIN_CACHE_REPLAY:
    default:
      if (scene_framenr < fds->cache_frame_start || scene_framenr > fds->cache_frame_end) {
        escape = true;
      }
      break;
  }
  /* If modifier will not be processed, update/flush pointers from (old) fluid object once more. */
  if (escape && fds->fluid) {
    manta_update_pointers(fds->fluid, fmd, true);
    return;
  }

  /* Reset fluid if no fluid present. Also resets active fields. */
  if (!fds->fluid) {
    fluid_modifier_reset_ex(fmd, false);
  }

  /* Ensure cache directory is not relative. */
  const char *relbase = BKE_modifier_path_relbase_from_global(ob);
  BLI_path_abs(fds->cache_directory, relbase);

  /* If 'outdated', reset the cache here. */
  if (is_startframe && mode == FLUID_DOMAIN_CACHE_REPLAY) {
    PTCacheID pid;
    BKE_ptcache_id_from_smoke(&pid, ob, fmd);
    if (pid.cache->flag & PTCACHE_OUTDATED) {
      BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
      BKE_fluid_cache_free_all(fds, ob);
      fluid_modifier_reset_ex(fmd, false);
    }
  }

  /* Ensure that all flags are up to date before doing any baking and/or cache reading. */
  objs = BKE_collision_objects_create(
      depsgraph, ob, fds->fluid_group, &numobj, eModifierType_Fluid);
  update_flowsflags(fds, objs, numobj);
  if (objs) {
    MEM_freeN(objs);
  }
  objs = BKE_collision_objects_create(
      depsgraph, ob, fds->effector_group, &numobj, eModifierType_Fluid);
  update_obstacleflags(fds, objs, numobj);
  if (objs) {
    MEM_freeN(objs);
  }

  /* Fluid domain init must not fail in order to continue modifier evaluation. */
  if (!fds->fluid && !fluid_modifier_init(fmd, depsgraph, ob, scene, mesh)) {
    CLOG_ERROR(&LOG, "Fluid initialization failed. Should not happen!");
    return;
  }
  BLI_assert(fds->fluid);

  /* Guiding parent res pointer needs initialization. */
  guide_parent = fds->guide_parent;
  if (guide_parent) {
    fmd_parent = (FluidModifierData *)BKE_modifiers_findby_type(guide_parent, eModifierType_Fluid);
    if (fmd_parent && fmd_parent->domain) {
      copy_v3_v3_int(fds->guide_res, fmd_parent->domain->res);
    }
  }

  /* Adaptive domain needs to know about current state, so save it here. */
  int o_res[3], o_min[3], o_max[3], o_shift[3];
  copy_v3_v3_int(o_res, fds->res);
  copy_v3_v3_int(o_min, fds->res_min);
  copy_v3_v3_int(o_max, fds->res_max);
  copy_v3_v3_int(o_shift, fds->shift);

  /* Ensure that time parameters are initialized correctly before every step. */
  fds->frame_length = DT_DEFAULT * (25.0f / scene->frames_per_second()) * fds->time_scale;
  fds->dt = fds->frame_length;
  fds->time_per_frame = 0;

  /* Ensure that gravity is copied over every frame (could be keyframed). */
  update_final_gravity(fds, scene);

  int next_frame = scene_framenr + 1;
  int prev_frame = scene_framenr - 1;
  /* Ensure positive of previous frame. */
  CLAMP_MIN(prev_frame, fds->cache_frame_start);

  int data_frame = scene_framenr, noise_frame = scene_framenr;
  int mesh_frame = scene_framenr, particles_frame = scene_framenr, guide_frame = scene_framenr;

  bool with_smoke, with_liquid;
  with_smoke = fds->type == FLUID_DOMAIN_TYPE_GAS;
  with_liquid = fds->type == FLUID_DOMAIN_TYPE_LIQUID;

  bool drops, bubble, floater;
  drops = fds->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY;
  bubble = fds->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE;
  floater = fds->particle_type & FLUID_DOMAIN_PARTICLE_FOAM;

  bool with_resumable_cache = fds->flags & FLUID_DOMAIN_USE_RESUMABLE_CACHE;
  bool with_script, with_noise, with_mesh, with_particles, with_guide;
  with_script = fds->flags & FLUID_DOMAIN_EXPORT_MANTA_SCRIPT;
  with_noise = fds->flags & FLUID_DOMAIN_USE_NOISE;
  with_mesh = fds->flags & FLUID_DOMAIN_USE_MESH;
  with_guide = fds->flags & FLUID_DOMAIN_USE_GUIDE;
  with_particles = drops || bubble || floater;

  bool has_data, has_noise, has_mesh, has_particles, has_guide, has_config;
  has_data = manta_has_data(fds->fluid, fmd, scene_framenr);
  has_noise = manta_has_noise(fds->fluid, fmd, scene_framenr);
  has_mesh = manta_has_mesh(fds->fluid, fmd, scene_framenr);
  has_particles = manta_has_particles(fds->fluid, fmd, scene_framenr);
  has_guide = manta_has_guiding(fds->fluid, fmd, scene_framenr, guide_parent);
  has_config = manta_read_config(fds->fluid, fmd, scene_framenr);

  /* When reading data from cache (has_config == true) ensure that active fields are allocated.
   * update_flowsflags() and update_obstacleflags() will not find flow sources hidden from renders.
   * See also: #72192. */
  if (has_config) {
    ensure_flowsfields(fds);
    ensure_obstaclefields(fds);
  }

  bool baking_data, baking_noise, baking_mesh, baking_particles, baking_guide;
  baking_data = fds->cache_flag & FLUID_DOMAIN_BAKING_DATA;
  baking_noise = fds->cache_flag & FLUID_DOMAIN_BAKING_NOISE;
  baking_mesh = fds->cache_flag & FLUID_DOMAIN_BAKING_MESH;
  baking_particles = fds->cache_flag & FLUID_DOMAIN_BAKING_PARTICLES;
  baking_guide = fds->cache_flag & FLUID_DOMAIN_BAKING_GUIDE;

  bool resume_data, resume_noise, resume_mesh, resume_particles, resume_guide;
  resume_data = (!is_startframe) && (fds->cache_frame_pause_data == scene_framenr);
  resume_noise = (!is_startframe) && (fds->cache_frame_pause_noise == scene_framenr);
  resume_mesh = (!is_startframe) && (fds->cache_frame_pause_mesh == scene_framenr);
  resume_particles = (!is_startframe) && (fds->cache_frame_pause_particles == scene_framenr);
  resume_guide = (!is_startframe) && (fds->cache_frame_pause_guide == scene_framenr);

  bool read_cache, bake_cache;
  read_cache = false;
  bake_cache = baking_data || baking_noise || baking_mesh || baking_particles || baking_guide;

  bool next_data, next_noise, next_mesh, next_particles, next_guide;
  next_data = manta_has_data(fds->fluid, fmd, next_frame);
  next_noise = manta_has_noise(fds->fluid, fmd, next_frame);
  next_mesh = manta_has_mesh(fds->fluid, fmd, next_frame);
  next_particles = manta_has_particles(fds->fluid, fmd, next_frame);
  next_guide = manta_has_guiding(fds->fluid, fmd, next_frame, guide_parent);

  bool prev_data, prev_noise, prev_mesh, prev_particles, prev_guide;
  prev_data = manta_has_data(fds->fluid, fmd, prev_frame);
  prev_noise = manta_has_noise(fds->fluid, fmd, prev_frame);
  prev_mesh = manta_has_mesh(fds->fluid, fmd, prev_frame);
  prev_particles = manta_has_particles(fds->fluid, fmd, prev_frame);
  prev_guide = manta_has_guiding(fds->fluid, fmd, prev_frame, guide_parent);

  /* Unused for now. */
  UNUSED_VARS(next_mesh, next_guide);

  bool with_gdomain;
  with_gdomain = (fds->guide_source == FLUID_DOMAIN_GUIDE_SRC_DOMAIN);

  /* Cache mode specific settings. */
  switch (mode) {
    case FLUID_DOMAIN_CACHE_ALL:
    case FLUID_DOMAIN_CACHE_MODULAR:
      /* Just load the data that has already been baked */
      if (!baking_data && !baking_noise && !baking_mesh && !baking_particles && !baking_guide) {
        read_cache = true;
        bake_cache = false;

        /* Apply frame offset. */
        data_frame -= fmd->domain->cache_frame_offset;
        noise_frame -= fmd->domain->cache_frame_offset;
        mesh_frame -= fmd->domain->cache_frame_offset;
        particles_frame -= fmd->domain->cache_frame_offset;
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
      CLAMP_MAX(noise_frame, data_frame);
      CLAMP_MAX(mesh_frame, data_frame);
      CLAMP_MAX(particles_frame, data_frame);
      CLAMP_MAX(guide_frame, fds->cache_frame_end);

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

  bool read_partial = false, read_all = false;
  bool grid_display = fds->use_coba;

  /* Try to read from cache and keep track of read success. */
  if (read_cache) {

    /* Read mesh cache. */
    if (with_liquid && with_mesh) {
      if (mesh_frame != scene_framenr) {
        has_config = manta_read_config(fds->fluid, fmd, mesh_frame);
      }

      /* Only load the mesh at the resolution it ways originally simulated at.
       * The mesh files don't have a header, i.e. the don't store the grid resolution. */
      if (!manta_needs_realloc(fds->fluid, fmd)) {
        has_mesh = manta_read_mesh(fds->fluid, fmd, mesh_frame);
      }
    }

    /* Read particles cache. */
    if (with_liquid && with_particles) {
      if (particles_frame != scene_framenr) {
        has_config = manta_read_config(fds->fluid, fmd, particles_frame);
      }

      read_partial = !baking_data && !baking_particles && next_particles;
      read_all = !read_partial && with_resumable_cache;
      has_particles = manta_read_particles(fds->fluid, fmd, particles_frame, read_all);
    }

    /* Read guide cache. */
    if (with_guide) {
      FluidModifierData *fmd2 = (with_gdomain) ? fmd_parent : fmd;
      has_guide = manta_read_guiding(fds->fluid, fmd2, scene_framenr, with_gdomain);
    }

    /* Read noise and data cache */
    if (with_smoke && with_noise) {
      if (noise_frame != scene_framenr) {
        has_config = manta_read_config(fds->fluid, fmd, noise_frame);
      }

      /* Only reallocate when just reading cache or when resuming during bake. */
      if (has_data && has_config && manta_needs_realloc(fds->fluid, fmd)) {
        BKE_fluid_reallocate_copy_fluid(
            fds, o_res, fds->res, o_min, fds->res_min, o_max, o_shift, fds->shift);
      }

      read_partial = !baking_data && !baking_noise && next_noise;
      read_all = !read_partial && with_resumable_cache;
      has_noise = manta_read_noise(fds->fluid, fmd, noise_frame, read_all);

      read_partial = !baking_data && !baking_noise && next_data && next_noise;
      read_all = !read_partial && with_resumable_cache;
      has_data = manta_read_data(fds->fluid, fmd, data_frame, read_all);
    }
    /* Read data cache only */
    else {
      if (data_frame != scene_framenr) {
        has_config = manta_read_config(fds->fluid, fmd, data_frame);
      }

      if (with_smoke || with_liquid) {
        /* Read config and realloc fluid object if needed. */
        if (has_config && manta_needs_realloc(fds->fluid, fmd)) {
          BKE_fluid_reallocate_fluid(fds, fds->res, 1);
        }
      }

      read_partial = !baking_data && !baking_particles && !baking_mesh && next_data &&
                     !grid_display;
      read_all = !read_partial && with_resumable_cache;
      has_data = manta_read_data(fds->fluid, fmd, data_frame, read_all);
    }
  }

  /* Cache mode specific settings */
  switch (mode) {
    case FLUID_DOMAIN_CACHE_ALL:
    case FLUID_DOMAIN_CACHE_MODULAR:
      if (!baking_data && !baking_noise && !baking_mesh && !baking_particles && !baking_guide) {
        bake_cache = false;
      }
      break;
    case FLUID_DOMAIN_CACHE_REPLAY:
    default:
      if (with_guide) {
        baking_guide = !has_guide && (is_startframe || prev_guide);
      }
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
    manta_update_variables(fds->fluid, fmd);

    /* Export mantaflow python script on first frame (once only) and for any bake type */
    if (with_script && is_startframe) {
      if (with_smoke) {
        manta_smoke_export_script(fmd->domain->fluid, fmd);
      }
      if (with_liquid) {
        manta_liquid_export_script(fmd->domain->fluid, fmd);
      }
    }

    if (baking_guide && with_guide) {
      manta_guiding(depsgraph, scene, ob, fmd, scene_framenr);
    }
    if (baking_data) {
      /* Only save baked data if all of it completed successfully. */
      if (manta_step(depsgraph, scene, ob, mesh, fmd, scene_framenr)) {
        manta_write_config(fds->fluid, fmd, scene_framenr);
        manta_write_data(fds->fluid, fmd, scene_framenr);
      }
    }
    if (has_data || baking_data) {
      if (baking_noise && with_smoke && with_noise) {
        /* Ensure that no bake occurs if domain was minimized by adaptive domain. */
        if (fds->total_cells > 1) {
          manta_bake_noise(fds->fluid, fmd, scene_framenr);
        }
        manta_write_noise(fds->fluid, fmd, scene_framenr);
      }
      if (baking_mesh && with_liquid && with_mesh) {
        manta_bake_mesh(fds->fluid, fmd, scene_framenr);
      }
      if (baking_particles && with_liquid && with_particles) {
        manta_bake_particles(fds->fluid, fmd, scene_framenr);
      }
    }
  }

  /* Ensure that fluid pointers are always up to date at the end of modifier processing. */
  manta_update_pointers(fds->fluid, fmd, false);

  fds->flags &= ~FLUID_DOMAIN_FILE_LOAD;
  fmd->time = scene_framenr;
}

static void fluid_modifier_process(
    FluidModifierData *fmd, Depsgraph *depsgraph, Scene *scene, Object *ob, Mesh *mesh)
{
  const int scene_framenr = int(DEG_get_ctime(depsgraph));

  if (fmd->type & MOD_FLUID_TYPE_FLOW) {
    fluid_modifier_processFlow(fmd, depsgraph, scene, ob, mesh, scene_framenr);
  }
  else if (fmd->type & MOD_FLUID_TYPE_EFFEC) {
    fluid_modifier_processEffector(fmd, depsgraph, scene, ob, mesh, scene_framenr);
  }
  else if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
    fluid_modifier_processDomain(fmd, depsgraph, scene, ob, mesh, scene_framenr);
  }
}

Mesh *BKE_fluid_modifier_do(
    FluidModifierData *fmd, Depsgraph *depsgraph, Scene *scene, Object *ob, Mesh *mesh)
{
  /* Optimization: Do not update viewport during bakes (except in replay mode)
   * Reason: UI is locked and updated liquid / smoke geometry is not visible anyways. */
  bool needs_viewport_update = false;

  /* Optimization: Only process modifier if object is not being altered. */
  if (!G.moving) {
    /* Lock so preview render does not read smoke data while it gets modified. */
    if ((fmd->type & MOD_FLUID_TYPE_DOMAIN) && fmd->domain) {
      BLI_rw_mutex_lock(static_cast<ThreadRWMutex *>(fmd->domain->fluid_mutex), THREAD_LOCK_WRITE);
    }

    fluid_modifier_process(fmd, depsgraph, scene, ob, mesh);

    if ((fmd->type & MOD_FLUID_TYPE_DOMAIN) && fmd->domain) {
      BLI_rw_mutex_unlock(static_cast<ThreadRWMutex *>(fmd->domain->fluid_mutex));
    }

    if (fmd->domain) {
      FluidDomainSettings *fds = fmd->domain;

      /* Always update viewport in cache replay mode. */
      if (fds->cache_type == FLUID_DOMAIN_CACHE_REPLAY ||
          fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN)
      {
        needs_viewport_update = true;
      }
      /* In other cache modes, only update the viewport when no bake is going on. */
      else {
        bool with_mesh;
        with_mesh = fds->flags & FLUID_DOMAIN_USE_MESH;
        bool baking_data, baking_noise, baking_mesh, baking_particles, baking_guide;
        baking_data = fds->cache_flag & FLUID_DOMAIN_BAKING_DATA;
        baking_noise = fds->cache_flag & FLUID_DOMAIN_BAKING_NOISE;
        baking_mesh = fds->cache_flag & FLUID_DOMAIN_BAKING_MESH;
        baking_particles = fds->cache_flag & FLUID_DOMAIN_BAKING_PARTICLES;
        baking_guide = fds->cache_flag & FLUID_DOMAIN_BAKING_GUIDE;

        if (with_mesh && !baking_data && !baking_noise && !baking_mesh && !baking_particles &&
            !baking_guide)
        {
          needs_viewport_update = true;
        }
      }
    }
  }

  Mesh *result = nullptr;
  if (fmd->type & MOD_FLUID_TYPE_DOMAIN && fmd->domain) {
    if (needs_viewport_update) {
      /* Return generated geometry depending on domain type. */
      if (fmd->domain->type == FLUID_DOMAIN_TYPE_LIQUID) {
        result = create_liquid_geometry(fmd->domain, scene, mesh, ob);
      }
      if (fmd->domain->type == FLUID_DOMAIN_TYPE_GAS) {
        result = create_smoke_geometry(fmd->domain, mesh, ob);
      }
    }

    /* Clear flag outside of locked block (above). */
    fmd->domain->cache_flag &= ~FLUID_DOMAIN_OUTDATED_DATA;
    fmd->domain->cache_flag &= ~FLUID_DOMAIN_OUTDATED_NOISE;
    fmd->domain->cache_flag &= ~FLUID_DOMAIN_OUTDATED_MESH;
    fmd->domain->cache_flag &= ~FLUID_DOMAIN_OUTDATED_PARTICLES;
    fmd->domain->cache_flag &= ~FLUID_DOMAIN_OUTDATED_GUIDE;
  }

  if (!result) {
    result = BKE_mesh_copy_for_eval(*mesh);
  }
  else {
    BKE_mesh_copy_parameters_for_eval(result, mesh);
  }

  /* Liquid simulation has a texture space that based on the bounds of the fluid mesh.
   * This does not seem particularly useful, but it's backwards compatible.
   *
   * Smoke simulation needs a texture space relative to the adaptive domain bounds, not the
   * original mesh. So recompute it at this point in the modifier stack. See #58492. */
  BKE_mesh_texspace_calc(result);

  return result;
}

static float calc_voxel_transp(
    float *result, const float *input, int res[3], int *pixel, float *t_ray, float correct)
{
  const size_t index = manta_get_index(pixel[0], res[0], pixel[1], res[1], pixel[2]);

  /* `T_ray *= T_vox`. */
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

static void manta_smoke_calc_transparency(FluidDomainSettings *fds,
                                          Scene *scene,
                                          ViewLayer *view_layer)
{
  float bv[6] = {0};
  float light[3];
  int slabsize = fds->res[0] * fds->res[1];
  float *density = manta_smoke_get_density(fds->fluid);
  float *shadow = manta_smoke_get_shadow(fds->fluid);
  float correct = -7.0f * fds->dx;

  if (!get_light(scene, view_layer, light)) {
    return;
  }

  /* Convert light pos to sim cell space. */
  mul_m4_v3(fds->imat, light);
  light[0] = (light[0] - fds->p0[0]) / fds->cell_size[0] - 0.5f - float(fds->res_min[0]);
  light[1] = (light[1] - fds->p0[1]) / fds->cell_size[1] - 0.5f - float(fds->res_min[1]);
  light[2] = (light[2] - fds->p0[2]) / fds->cell_size[2] - 0.5f - float(fds->res_min[2]);

  /* Calculate domain bounds in sim cell space. */
  /* 0,2,4 = 0.0f */
  bv[1] = float(fds->res[0]); /* X */
  bv[3] = float(fds->res[1]); /* Y */
  bv[5] = float(fds->res[2]); /* Z */

  for (int z = 0; z < fds->res[2]; z++) {
    size_t index = z * slabsize;

    for (int y = 0; y < fds->res[1]; y++) {
      for (int x = 0; x < fds->res[0]; x++, index++) {
        float voxel_center[3];
        float pos[3];
        int cell[3];
        float t_ray = 1.0;

        /* Reset shadow value. */
        shadow[index] = -1.0f;

        voxel_center[0] = float(x);
        voxel_center[1] = float(y);
        voxel_center[2] = float(z);

        /* Get starting cell (light pos). */
        if (BLI_bvhtree_bb_raycast(bv, light, voxel_center, pos) > FLT_EPSILON) {
          /* We're outside -> use point on side of domain. */
          cell[0] = int(floor(pos[0]));
          cell[1] = int(floor(pos[1]));
          cell[2] = int(floor(pos[2]));
        }
        else {
          /* We're inside -> use light itself. */
          cell[0] = int(floor(light[0]));
          cell[1] = int(floor(light[1]));
          cell[2] = int(floor(light[2]));
        }
        /* Clamp within grid bounds */
        CLAMP(cell[0], 0, fds->res[0] - 1);
        CLAMP(cell[1], 0, fds->res[1] - 1);
        CLAMP(cell[2], 0, fds->res[2] - 1);

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
                           fds->res,
                           correct);

        /* Convention -> from a RGBA float array, use G value for t_ray. */
        shadow[index] = t_ray;
      }
    }
  }
}

float BKE_fluid_get_velocity_at(Object *ob, float position[3], float velocity[3])
{
  FluidModifierData *fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  zero_v3(velocity);

  if (fmd && (fmd->type & MOD_FLUID_TYPE_DOMAIN) && fmd->domain && fmd->domain->fluid) {
    FluidDomainSettings *fds = fmd->domain;
    float time_mult = 25.0f * DT_DEFAULT;
    float size_mult = std::max({fds->global_size[0], fds->global_size[1], fds->global_size[2]}) /
                      std::max({fds->base_res[0], fds->base_res[1], fds->base_res[2]});
    float vel_mag;
    float density = 0.0f, fuel = 0.0f;
    float pos[3];
    copy_v3_v3(pos, position);
    manta_pos_to_cell(fds, pos);

    /* Check if position is outside domain max bounds. */
    if (pos[0] < fds->res_min[0] || pos[1] < fds->res_min[1] || pos[2] < fds->res_min[2]) {
      return -1.0f;
    }
    if (pos[0] > fds->res_max[0] || pos[1] > fds->res_max[1] || pos[2] > fds->res_max[2]) {
      return -1.0f;
    }

    /* map pos between 0.0 - 1.0 */
    pos[0] = (pos[0] - fds->res_min[0]) / float(fds->res[0]);
    pos[1] = (pos[1] - fds->res_min[1]) / float(fds->res[1]);
    pos[2] = (pos[2] - fds->res_min[2]) / float(fds->res[2]);

    /* Check if position is outside active area. */
    if (fds->type == FLUID_DOMAIN_TYPE_GAS && fds->flags & FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN) {
      if (pos[0] < 0.0f || pos[1] < 0.0f || pos[2] < 0.0f) {
        return 0.0f;
      }
      if (pos[0] > 1.0f || pos[1] > 1.0f || pos[2] > 1.0f) {
        return 0.0f;
      }
    }

    /* Get interpolated velocity at given position. */
    velocity[0] = BLI_voxel_sample_trilinear(manta_get_velocity_x(fds->fluid), fds->res, pos);
    velocity[1] = BLI_voxel_sample_trilinear(manta_get_velocity_y(fds->fluid), fds->res, pos);
    velocity[2] = BLI_voxel_sample_trilinear(manta_get_velocity_z(fds->fluid), fds->res, pos);

    /* Convert simulation units to Blender units. */
    mul_v3_fl(velocity, size_mult);
    mul_v3_fl(velocity, time_mult);

    /* Convert velocity direction to global space. */
    vel_mag = len_v3(velocity);
    mul_mat3_m4_v3(fds->obmat, velocity);
    normalize_v3(velocity);
    mul_v3_fl(velocity, vel_mag);

    /* Use max value of fuel or smoke density. */
    density = BLI_voxel_sample_trilinear(manta_smoke_get_density(fds->fluid), fds->res, pos);
    if (manta_smoke_has_fuel(fds->fluid)) {
      fuel = BLI_voxel_sample_trilinear(manta_smoke_get_fuel(fds->fluid), fds->res, pos);
    }
    return std::max(density, fuel);
  }
  return -1.0f;
}

int BKE_fluid_get_data_flags(FluidDomainSettings *fds)
{
  int flags = 0;

  if (fds->fluid) {
    if (manta_smoke_has_heat(fds->fluid)) {
      flags |= FLUID_DOMAIN_ACTIVE_HEAT;
    }
    if (manta_smoke_has_fuel(fds->fluid)) {
      flags |= FLUID_DOMAIN_ACTIVE_FIRE;
    }
    if (manta_smoke_has_colors(fds->fluid)) {
      flags |= FLUID_DOMAIN_ACTIVE_COLORS;
    }
  }

  return flags;
}

void BKE_fluid_particle_system_create(Main *bmain,
                                      Object *ob,
                                      const char *pset_name,
                                      const char *parts_name,
                                      const char *psys_name,
                                      const int psys_type)
{
  ParticleSystem *psys;
  ParticleSettings *part;
  ParticleSystemModifierData *pfmd;

  /* add particle system */
  part = BKE_particlesettings_add(bmain, pset_name);
  psys = MEM_callocN<ParticleSystem>(__func__);

  part->type = psys_type;
  part->totpart = 0;
  part->draw_size = 0.01f; /* Make fluid particles more subtle in viewport. */
  part->draw_col = PART_DRAW_COL_VEL;

  /* Use different shape and color for fluid particles to be able to find issues in Viewport */
  if (psys_type == PART_FLUID_BUBBLE) {
    part->draw_as = PART_DRAW_CIRC;
  }
  if (psys_type == PART_FLUID_FOAM) {
    part->draw_as = PART_DRAW_CROSS;
  }

  part->phystype = PART_PHYS_NO; /* No physics needed, part system only used to display data. */
  psys->part = part;
  psys->pointcache = BKE_ptcache_add(&psys->ptcaches);
  STRNCPY(psys->name, parts_name);
  BLI_addtail(&ob->particlesystem, psys);

  /* add modifier */
  pfmd = (ParticleSystemModifierData *)BKE_modifier_new(eModifierType_ParticleSystem);
  STRNCPY(pfmd->modifier.name, psys_name);
  pfmd->psys = psys;
  BLI_addtail(&ob->modifiers, pfmd);
  BKE_modifier_unique_name(&ob->modifiers, (ModifierData *)pfmd);
  BKE_modifiers_persistent_uid_init(*ob, pfmd->modifier);
}

void BKE_fluid_particle_system_destroy(Object *ob, const int particle_type)
{
  ParticleSystemModifierData *pfmd;
  ParticleSystem *psys, *next_psys;

  for (psys = static_cast<ParticleSystem *>(ob->particlesystem.first); psys; psys = next_psys) {
    next_psys = psys->next;
    if (psys->part->type == particle_type) {
      /* clear modifier */
      pfmd = psys_get_modifier(ob, psys);
      BKE_modifier_remove_from_list(ob, (ModifierData *)pfmd);
      BKE_modifier_free((ModifierData *)pfmd);

      /* clear particle system */
      BLI_remlink(&ob->particlesystem, psys);
      psys_free(ob, psys);
    }
  }
}

/** \} */

#endif /* WITH_FLUID */

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
  /* Set values for border collision:
   * Liquids should have a closed domain, smoke domains should be open. */
  if (type == FLUID_DOMAIN_TYPE_GAS) {
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_FRONT, true);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_BACK, true);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_RIGHT, true);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_LEFT, true);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_TOP, true);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_BOTTOM, true);
    object->dt = OB_WIRE;
  }
  else if (type == FLUID_DOMAIN_TYPE_LIQUID) {
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_FRONT, false);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_BACK, false);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_RIGHT, false);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_LEFT, false);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_TOP, false);
    BKE_fluid_collisionextents_set(settings, FLUID_DOMAIN_BORDER_BOTTOM, false);
    object->dt = OB_SOLID;
  }

  /* Set actual domain type. */
  settings->type = type;
}

void BKE_fluid_flow_behavior_set(Object * /*object*/, FluidFlowSettings *settings, int behavior)
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

void BKE_fluid_effector_type_set(Object * /*object*/, FluidEffectorSettings *settings, int type)
{
  settings->type = type;
}

void BKE_fluid_fields_sanitize(FluidDomainSettings *settings)
{
  /* Based on the domain type, certain fields are defaulted accordingly if the selected field
   * is unsupported. */
  const char coba_field = settings->coba_field;
  const char data_depth = settings->openvdb_data_depth;

  if (settings->type == FLUID_DOMAIN_TYPE_GAS) {
    if (ELEM(coba_field,
             FLUID_DOMAIN_FIELD_PHI,
             FLUID_DOMAIN_FIELD_PHI_IN,
             FLUID_DOMAIN_FIELD_PHI_OUT,
             FLUID_DOMAIN_FIELD_PHI_OBSTACLE))
    {
      /* Defaulted to density for gas domain. */
      settings->coba_field = FLUID_DOMAIN_FIELD_DENSITY;
    }

    /* Gas domains do not support VDB mini precision. */
    if (data_depth == VDB_PRECISION_MINI_FLOAT) {
      settings->openvdb_data_depth = VDB_PRECISION_HALF_FLOAT;
    }
  }
  else if (settings->type == FLUID_DOMAIN_TYPE_LIQUID) {
    if (ELEM(coba_field,
             FLUID_DOMAIN_FIELD_COLOR_R,
             FLUID_DOMAIN_FIELD_COLOR_G,
             FLUID_DOMAIN_FIELD_COLOR_B,
             FLUID_DOMAIN_FIELD_DENSITY,
             FLUID_DOMAIN_FIELD_FLAME,
             FLUID_DOMAIN_FIELD_FUEL,
             FLUID_DOMAIN_FIELD_HEAT))
    {
      /* Defaulted to phi for liquid domain. */
      settings->coba_field = FLUID_DOMAIN_FIELD_PHI;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Modifier API
 *
 * Use for versioning, even when fluids are disabled.
 * \{ */

static void fluid_modifier_freeDomain(FluidModifierData *fmd)
{
  if (fmd->domain) {
    if (fmd->domain->fluid) {
#ifdef WITH_FLUID
      manta_free(fmd->domain->fluid);
#endif
    }

    if (fmd->domain->fluid_mutex) {
      BLI_rw_mutex_free(static_cast<ThreadRWMutex *>(fmd->domain->fluid_mutex));
    }

    MEM_SAFE_FREE(fmd->domain->effector_weights);

    if (!(fmd->modifier.flag & eModifierFlag_SharedCaches)) {
      BKE_ptcache_free_list(&(fmd->domain->ptcaches[0]));
      fmd->domain->point_cache[0] = nullptr;
    }

    if (fmd->domain->coba) {
      MEM_freeN(fmd->domain->coba);
    }

    MEM_freeN(fmd->domain);
    fmd->domain = nullptr;
  }
}

static void fluid_modifier_freeFlow(FluidModifierData *fmd)
{
  if (fmd->flow) {
    if (fmd->flow->mesh) {
      BKE_id_free(nullptr, fmd->flow->mesh);
    }
    fmd->flow->mesh = nullptr;

    MEM_SAFE_FREE(fmd->flow->verts_old);
    fmd->flow->numverts = 0;
    fmd->flow->flags &= ~FLUID_FLOW_NEEDS_UPDATE;

    MEM_freeN(fmd->flow);
    fmd->flow = nullptr;
  }
}

static void fluid_modifier_freeEffector(FluidModifierData *fmd)
{
  if (fmd->effector) {
    if (fmd->effector->mesh) {
      BKE_id_free(nullptr, fmd->effector->mesh);
    }
    fmd->effector->mesh = nullptr;

    MEM_SAFE_FREE(fmd->effector->verts_old);
    fmd->effector->numverts = 0;
    fmd->effector->flags &= ~FLUID_EFFECTOR_NEEDS_UPDATE;

    MEM_freeN(fmd->effector);
    fmd->effector = nullptr;
  }
}

static void fluid_modifier_reset_ex(FluidModifierData *fmd, bool need_lock)
{
  if (!fmd) {
    return;
  }

  if (fmd->domain) {
    if (fmd->domain->fluid) {
      if (need_lock) {
        BLI_rw_mutex_lock(static_cast<ThreadRWMutex *>(fmd->domain->fluid_mutex),
                          THREAD_LOCK_WRITE);
      }

#ifdef WITH_FLUID
      manta_free(fmd->domain->fluid);
#endif
      fmd->domain->fluid = nullptr;

      if (need_lock) {
        BLI_rw_mutex_unlock(static_cast<ThreadRWMutex *>(fmd->domain->fluid_mutex));
      }
    }

    fmd->time = -1;
    fmd->domain->total_cells = 0;
    fmd->domain->active_fields = 0;
  }
  else if (fmd->flow) {
    MEM_SAFE_FREE(fmd->flow->verts_old);
    fmd->flow->numverts = 0;
    fmd->flow->flags &= ~FLUID_FLOW_NEEDS_UPDATE;
  }
  else if (fmd->effector) {
    MEM_SAFE_FREE(fmd->effector->verts_old);
    fmd->effector->numverts = 0;
    fmd->effector->flags &= ~FLUID_EFFECTOR_NEEDS_UPDATE;
  }
}

void BKE_fluid_modifier_reset(FluidModifierData *fmd)
{
  fluid_modifier_reset_ex(fmd, true);
}

void BKE_fluid_modifier_free(FluidModifierData *fmd)
{
  if (!fmd) {
    return;
  }

  fluid_modifier_freeDomain(fmd);
  fluid_modifier_freeFlow(fmd);
  fluid_modifier_freeEffector(fmd);
}

void BKE_fluid_modifier_create_type_data(FluidModifierData *fmd)
{
  if (!fmd) {
    return;
  }

  if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
    if (fmd->domain) {
      fluid_modifier_freeDomain(fmd);
    }

    fmd->domain = DNA_struct_default_alloc(FluidDomainSettings);
    fmd->domain->fmd = fmd;

    /* Turn off incompatible options. */
#ifndef WITH_OPENVDB
    fmd->domain->cache_data_format = FLUID_DOMAIN_FILE_UNI;
    fmd->domain->cache_particle_format = FLUID_DOMAIN_FILE_UNI;
    fmd->domain->cache_noise_format = FLUID_DOMAIN_FILE_UNI;
#endif
#ifndef WITH_OPENVDB_BLOSC
    fmd->domain->openvdb_compression = VDB_COMPRESSION_ZIP;
#endif

    fmd->domain->effector_weights = BKE_effector_add_weights(nullptr);
    fmd->domain->fluid_mutex = BLI_rw_mutex_alloc();

    char cache_name[64];
    BKE_fluid_cache_new_name_for_current_session(sizeof(cache_name), cache_name);
    BKE_modifier_path_init(
        fmd->domain->cache_directory, sizeof(fmd->domain->cache_directory), cache_name);

    /* pointcache options */
    fmd->domain->point_cache[0] = BKE_ptcache_add(&(fmd->domain->ptcaches[0]));
    fmd->domain->point_cache[0]->flag |= PTCACHE_DISK_CACHE;
    fmd->domain->point_cache[0]->step = 1;
    fmd->domain->point_cache[1] = nullptr; /* Deprecated */
  }
  else if (fmd->type & MOD_FLUID_TYPE_FLOW) {
    if (fmd->flow) {
      fluid_modifier_freeFlow(fmd);
    }

    fmd->flow = DNA_struct_default_alloc(FluidFlowSettings);
    fmd->flow->fmd = fmd;
  }
  else if (fmd->type & MOD_FLUID_TYPE_EFFEC) {
    if (fmd->effector) {
      fluid_modifier_freeEffector(fmd);
    }

    fmd->effector = DNA_struct_default_alloc(FluidEffectorSettings);
    fmd->effector->fmd = fmd;
  }
}

void BKE_fluid_modifier_copy(const FluidModifierData *fmd, FluidModifierData *tfmd, const int flag)
{
  tfmd->type = fmd->type;
  tfmd->time = fmd->time;

  BKE_fluid_modifier_create_type_data(tfmd);

  if (tfmd->domain) {
    FluidDomainSettings *tfds = tfmd->domain;
    FluidDomainSettings *fds = fmd->domain;

    /* domain object data */
    tfds->fluid_group = fds->fluid_group;
    tfds->force_group = fds->force_group;
    tfds->effector_group = fds->effector_group;
    if (tfds->effector_weights) {
      MEM_freeN(tfds->effector_weights);
    }
    tfds->effector_weights = static_cast<EffectorWeights *>(MEM_dupallocN(fds->effector_weights));

    /* adaptive domain options */
    tfds->adapt_margin = fds->adapt_margin;
    tfds->adapt_res = fds->adapt_res;
    tfds->adapt_threshold = fds->adapt_threshold;

    /* fluid domain options */
    tfds->maxres = fds->maxres;
    tfds->solver_res = fds->solver_res;
    tfds->border_collisions = fds->border_collisions;
    tfds->flags = fds->flags;
    tfds->gravity[0] = fds->gravity[0];
    tfds->gravity[1] = fds->gravity[1];
    tfds->gravity[2] = fds->gravity[2];
    tfds->active_fields = fds->active_fields;
    tfds->type = fds->type;
    tfds->boundary_width = fds->boundary_width;

    /* smoke domain options */
    tfds->alpha = fds->alpha;
    tfds->beta = fds->beta;
    tfds->diss_speed = fds->diss_speed;
    tfds->vorticity = fds->vorticity;
    tfds->highres_sampling = fds->highres_sampling;

    /* flame options */
    tfds->burning_rate = fds->burning_rate;
    tfds->flame_smoke = fds->flame_smoke;
    tfds->flame_vorticity = fds->flame_vorticity;
    tfds->flame_ignition = fds->flame_ignition;
    tfds->flame_max_temp = fds->flame_max_temp;
    copy_v3_v3(tfds->flame_smoke_color, fds->flame_smoke_color);

    /* noise options */
    tfds->noise_strength = fds->noise_strength;
    tfds->noise_pos_scale = fds->noise_pos_scale;
    tfds->noise_time_anim = fds->noise_time_anim;
    tfds->noise_scale = fds->noise_scale;

    /* liquid domain options */
    tfds->flip_ratio = fds->flip_ratio;
    tfds->particle_randomness = fds->particle_randomness;
    tfds->particle_number = fds->particle_number;
    tfds->particle_minimum = fds->particle_minimum;
    tfds->particle_maximum = fds->particle_maximum;
    tfds->particle_radius = fds->particle_radius;
    tfds->particle_band_width = fds->particle_band_width;
    tfds->fractions_threshold = fds->fractions_threshold;
    tfds->fractions_distance = fds->fractions_distance;
    tfds->sys_particle_maximum = fds->sys_particle_maximum;
    tfds->simulation_method = fds->simulation_method;

    /* viscosity options */
    tfds->viscosity_value = fds->viscosity_value;

    /* Diffusion options. */
    tfds->surface_tension = fds->surface_tension;
    tfds->viscosity_base = fds->viscosity_base;
    tfds->viscosity_exponent = fds->viscosity_exponent;

    /* mesh options */
    tfds->mesh_concave_upper = fds->mesh_concave_upper;
    tfds->mesh_concave_lower = fds->mesh_concave_lower;
    tfds->mesh_particle_radius = fds->mesh_particle_radius;
    tfds->mesh_smoothen_pos = fds->mesh_smoothen_pos;
    tfds->mesh_smoothen_neg = fds->mesh_smoothen_neg;
    tfds->mesh_scale = fds->mesh_scale;
    tfds->mesh_generator = fds->mesh_generator;

    /* secondary particle options */
    tfds->sndparticle_k_b = fds->sndparticle_k_b;
    tfds->sndparticle_k_d = fds->sndparticle_k_d;
    tfds->sndparticle_k_ta = fds->sndparticle_k_ta;
    tfds->sndparticle_k_wc = fds->sndparticle_k_wc;
    tfds->sndparticle_l_max = fds->sndparticle_l_max;
    tfds->sndparticle_l_min = fds->sndparticle_l_min;
    tfds->sndparticle_tau_max_k = fds->sndparticle_tau_max_k;
    tfds->sndparticle_tau_max_ta = fds->sndparticle_tau_max_ta;
    tfds->sndparticle_tau_max_wc = fds->sndparticle_tau_max_wc;
    tfds->sndparticle_tau_min_k = fds->sndparticle_tau_min_k;
    tfds->sndparticle_tau_min_ta = fds->sndparticle_tau_min_ta;
    tfds->sndparticle_tau_min_wc = fds->sndparticle_tau_min_wc;
    tfds->sndparticle_boundary = fds->sndparticle_boundary;
    tfds->sndparticle_combined_export = fds->sndparticle_combined_export;
    tfds->sndparticle_potential_radius = fds->sndparticle_potential_radius;
    tfds->sndparticle_update_radius = fds->sndparticle_update_radius;
    tfds->particle_type = fds->particle_type;
    tfds->particle_scale = fds->particle_scale;

    /* fluid guide options */
    tfds->guide_parent = fds->guide_parent;
    tfds->guide_alpha = fds->guide_alpha;
    tfds->guide_beta = fds->guide_beta;
    tfds->guide_vel_factor = fds->guide_vel_factor;
    copy_v3_v3_int(tfds->guide_res, fds->guide_res);
    tfds->guide_source = fds->guide_source;

    /* cache options */
    tfds->cache_frame_start = fds->cache_frame_start;
    tfds->cache_frame_end = fds->cache_frame_end;
    tfds->cache_frame_pause_data = fds->cache_frame_pause_data;
    tfds->cache_frame_pause_noise = fds->cache_frame_pause_noise;
    tfds->cache_frame_pause_mesh = fds->cache_frame_pause_mesh;
    tfds->cache_frame_pause_particles = fds->cache_frame_pause_particles;
    tfds->cache_frame_pause_guide = fds->cache_frame_pause_guide;
    tfds->cache_frame_offset = fds->cache_frame_offset;
    tfds->cache_flag = fds->cache_flag;
    tfds->cache_type = fds->cache_type;
    tfds->cache_mesh_format = fds->cache_mesh_format;
    tfds->cache_data_format = fds->cache_data_format;
    tfds->cache_particle_format = fds->cache_particle_format;
    tfds->cache_noise_format = fds->cache_noise_format;
    STRNCPY(tfds->cache_directory, fds->cache_directory);

    /* time options */
    tfds->time_scale = fds->time_scale;
    tfds->cfl_condition = fds->cfl_condition;
    tfds->timesteps_minimum = fds->timesteps_minimum;
    tfds->timesteps_maximum = fds->timesteps_maximum;

    /* display options */
    tfds->axis_slice_method = fds->axis_slice_method;
    tfds->slice_axis = fds->slice_axis;
    tfds->interp_method = fds->interp_method;
    tfds->draw_velocity = fds->draw_velocity;
    tfds->slice_per_voxel = fds->slice_per_voxel;
    tfds->slice_depth = fds->slice_depth;
    tfds->display_thickness = fds->display_thickness;
    tfds->show_gridlines = fds->show_gridlines;
    if (fds->coba) {
      tfds->coba = static_cast<ColorBand *>(MEM_dupallocN(fds->coba));
    }
    tfds->vector_scale = fds->vector_scale;
    tfds->vector_draw_type = fds->vector_draw_type;
    tfds->vector_field = fds->vector_field;
    tfds->vector_scale_with_magnitude = fds->vector_scale_with_magnitude;
    tfds->vector_draw_mac_components = fds->vector_draw_mac_components;
    tfds->use_coba = fds->use_coba;
    tfds->coba_field = fds->coba_field;
    tfds->grid_scale = fds->grid_scale;
    tfds->gridlines_color_field = fds->gridlines_color_field;
    tfds->gridlines_lower_bound = fds->gridlines_lower_bound;
    tfds->gridlines_upper_bound = fds->gridlines_upper_bound;
    copy_v4_v4(tfds->gridlines_range_color, fds->gridlines_range_color);
    tfds->gridlines_cell_filter = fds->gridlines_cell_filter;

    /* -- Deprecated / unused options (below)-- */

    /* pointcache options */
    BKE_ptcache_free_list(&(tfds->ptcaches[0]));
    if (flag & LIB_ID_COPY_SET_COPIED_ON_WRITE) {
      /* Share the cache with the original object's modifier. */
      tfmd->modifier.flag |= eModifierFlag_SharedCaches;
      tfds->point_cache[0] = fds->point_cache[0];
      tfds->ptcaches[0] = fds->ptcaches[0];
    }
    else {
      tfds->point_cache[0] = BKE_ptcache_copy_list(
          &(tfds->ptcaches[0]), &(fds->ptcaches[0]), flag);
    }

    /* OpenVDB cache options */
    tfds->openvdb_compression = fds->openvdb_compression;
    tfds->clipping = fds->clipping;
    tfds->openvdb_data_depth = fds->openvdb_data_depth;

    /* Render options. */
    tfds->velocity_scale = fds->velocity_scale;
  }
  else if (tfmd->flow) {
    FluidFlowSettings *tffs = tfmd->flow;
    FluidFlowSettings *ffs = fmd->flow;

    /* NOTE: This is dangerous, as it will generate invalid data in case we are copying between
     * different objects. Extra external code has to be called to ensure proper remapping of
     * that pointer. See e.g. `BKE_object_copy_particlesystems` or `BKE_object_copy_modifier`. */
    tffs->psys = ffs->psys;
    tffs->noise_texture = ffs->noise_texture;

    /* initial velocity */
    tffs->vel_multi = ffs->vel_multi;
    tffs->vel_normal = ffs->vel_normal;
    tffs->vel_random = ffs->vel_random;
    tffs->vel_coord[0] = ffs->vel_coord[0];
    tffs->vel_coord[1] = ffs->vel_coord[1];
    tffs->vel_coord[2] = ffs->vel_coord[2];

    /* emission */
    tffs->density = ffs->density;
    copy_v3_v3(tffs->color, ffs->color);
    tffs->fuel_amount = ffs->fuel_amount;
    tffs->temperature = ffs->temperature;
    tffs->volume_density = ffs->volume_density;
    tffs->surface_distance = ffs->surface_distance;
    tffs->particle_size = ffs->particle_size;
    tffs->subframes = ffs->subframes;

    /* texture control */
    tffs->texture_size = ffs->texture_size;
    tffs->texture_offset = ffs->texture_offset;
    STRNCPY(tffs->uvlayer_name, ffs->uvlayer_name);
    tffs->vgroup_density = ffs->vgroup_density;

    tffs->type = ffs->type;
    tffs->behavior = ffs->behavior;
    tffs->source = ffs->source;
    tffs->texture_type = ffs->texture_type;
    tffs->flags = ffs->flags;
  }
  else if (tfmd->effector) {
    FluidEffectorSettings *tfes = tfmd->effector;
    FluidEffectorSettings *fes = fmd->effector;

    tfes->surface_distance = fes->surface_distance;
    tfes->type = fes->type;
    tfes->flags = fes->flags;
    tfes->subframes = fes->subframes;

    /* guide options */
    tfes->guide_mode = fes->guide_mode;
    tfes->vel_multi = fes->vel_multi;
  }
}

void BKE_fluid_cache_new_name_for_current_session(int maxlen, char *r_name)
{
  static int counter = 1;
  BLI_snprintf(r_name, maxlen, FLUID_DOMAIN_DIR_DEFAULT "_%x", BLI_hash_int(counter));
  counter++;
}

/** \} */
