/* SPDX-FileCopyrightText: 2007 by Janne Karhu. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_jitter_2d.h"
#include "BLI_kdtree.h"
#include "BLI_math_geom.h"
#include "BLI_rand.h"
#include "BLI_sort.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_particle.h"

#include "DEG_depsgraph_query.hh"

static void alloc_child_particles(ParticleSystem *psys, int tot)
{
  if (psys->child) {
    /* only re-allocate if we have to */
    if (psys->part->childtype && psys->totchild == tot) {
      std::fill_n(psys->child, tot, ChildParticle{});
      return;
    }

    MEM_freeN(psys->child);
    psys->child = nullptr;
    psys->totchild = 0;
  }

  if (psys->part->childtype) {
    psys->totchild = tot;
    if (psys->totchild) {
      psys->child = MEM_calloc_arrayN<ChildParticle>(psys->totchild, "child_particles");
    }
  }
}

static void distribute_simple_children(Scene *scene,
                                       Object *ob,
                                       Mesh *final_mesh,
                                       Mesh *deform_mesh,
                                       ParticleSystem *psys,
                                       const bool use_render_params)
{
  ChildParticle *cpa = nullptr;
  int i, p;
  const int child_num = psys_get_child_number(scene, psys, use_render_params);
  const int totpart = psys_get_tot_child(scene, psys, use_render_params);
  RNG *rng = BLI_rng_new_srandom(31415926 + psys->seed + psys->child_seed);

  alloc_child_particles(psys, totpart);

  cpa = psys->child;
  for (i = 0; i < child_num; i++) {
    for (p = 0; p < psys->totpart; p++, cpa++) {
      float length = 2.0;
      cpa->parent = p;

      /* create even spherical distribution inside unit sphere */
      while (length >= 1.0f) {
        cpa->fuv[0] = 2.0f * BLI_rng_get_float(rng) - 1.0f;
        cpa->fuv[1] = 2.0f * BLI_rng_get_float(rng) - 1.0f;
        cpa->fuv[2] = 2.0f * BLI_rng_get_float(rng) - 1.0f;
        length = len_v3(cpa->fuv);
      }

      cpa->num = -1;
    }
  }
  /* dmcache must be updated for parent particles if children from faces is used */
  psys_calc_dmcache(ob, final_mesh, deform_mesh, psys);

  BLI_rng_free(rng);
}
static void distribute_grid(Mesh *mesh, ParticleSystem *psys)
{
  ParticleData *pa = nullptr;
  float min[3], max[3], delta[3], d;
  const blender::Span<blender::float3> positions = mesh->vert_positions();
  int totvert = mesh->verts_num, from = psys->part->from;
  int i, j, k, p, res = psys->part->grid_res, size[3], axis;

  /* find bounding box of dm */
  if (totvert > 0) {
    INIT_MINMAX(min, max);
    for (i = 1; i < totvert; i++) {
      minmax_v3v3_v3(min, max, positions[i]);
    }
  }
  else {
    zero_v3(min);
    zero_v3(max);
  }

  sub_v3_v3v3(delta, max, min);

  /* determine major axis */
  axis = axis_dominant_v3_single(delta);

  d = delta[axis] / float(res);

  size[axis] = res;
  size[(axis + 1) % 3] = int(ceil(delta[(axis + 1) % 3] / d));
  size[(axis + 2) % 3] = int(ceil(delta[(axis + 2) % 3] / d));

  /* float errors grrr. */
  size[(axis + 1) % 3] = std::min(size[(axis + 1) % 3], res);
  size[(axis + 2) % 3] = std::min(size[(axis + 2) % 3], res);

  size[0] = std::max(size[0], 1);
  size[1] = std::max(size[1], 1);
  size[2] = std::max(size[2], 1);

  /* no full offset for flat/thin objects */
  min[0] += d < delta[0] ? d / 2.0f : delta[0] / 2.0f;
  min[1] += d < delta[1] ? d / 2.0f : delta[1] / 2.0f;
  min[2] += d < delta[2] ? d / 2.0f : delta[2] / 2.0f;

  for (i = 0, p = 0, pa = psys->particles; i < res; i++) {
    for (j = 0; j < res; j++) {
      for (k = 0; k < res; k++, p++, pa++) {
        pa->fuv[0] = min[0] + float(i) * d;
        pa->fuv[1] = min[1] + float(j) * d;
        pa->fuv[2] = min[2] + float(k) * d;
        pa->flag |= PARS_UNEXIST;
        pa->hair_index = 0; /* abused in volume calculation */
      }
    }
  }

  /* enable particles near verts/edges/faces/inside surface */
  if (from == PART_FROM_VERT) {
    float vec[3];

    pa = psys->particles;

    min[0] -= d / 2.0f;
    min[1] -= d / 2.0f;
    min[2] -= d / 2.0f;

    for (i = 0; i < totvert; i++) {
      sub_v3_v3v3(vec, positions[i], min);
      vec[0] /= delta[0];
      vec[1] /= delta[1];
      vec[2] /= delta[2];
      pa[(int(vec[0] * (size[0] - 1)) * res + int(vec[1] * (size[1] - 1))) * res +
         int(vec[2] * (size[2] - 1))]
          .flag &= ~PARS_UNEXIST;
    }
  }
  else if (ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
    float co1[3], co2[3];

    MFace *mface = nullptr, *mface_array;
    float v1[3], v2[3], v3[3], v4[4], lambda;
    int a, a1, a2, a0mul, a1mul, a2mul, totface;
    int amax = from == PART_FROM_FACE ? 3 : 1;

    totface = mesh->totface_legacy;
    mface = mface_array = static_cast<MFace *>(
        CustomData_get_layer_for_write(&mesh->fdata_legacy, CD_MFACE, mesh->totface_legacy));

    for (a = 0; a < amax; a++) {
      if (a == 0) {
        a0mul = res * res;
        a1mul = res;
        a2mul = 1;
      }
      else if (a == 1) {
        a0mul = res;
        a1mul = 1;
        a2mul = res * res;
      }
      else {
        a0mul = 1;
        a1mul = res * res;
        a2mul = res;
      }

      for (a1 = 0; a1 < size[(a + 1) % 3]; a1++) {
        for (a2 = 0; a2 < size[(a + 2) % 3]; a2++) {
          mface = mface_array;

          pa = psys->particles + a1 * a1mul + a2 * a2mul;
          copy_v3_v3(co1, pa->fuv);
          co1[a] -= d < delta[a] ? d / 2.0f : delta[a] / 2.0f;
          copy_v3_v3(co2, co1);
          co2[a] += delta[a] + 0.001f * d;
          co1[a] -= 0.001f * d;

          IsectRayPrecalc isect_precalc;
          float ray_direction[3];
          sub_v3_v3v3(ray_direction, co2, co1);
          isect_ray_tri_watertight_v3_precalc(&isect_precalc, ray_direction);

          /* lets intersect the faces */
          for (i = 0; i < totface; i++, mface++) {
            ParticleData *pa1 = nullptr, *pa2 = nullptr;

            copy_v3_v3(v1, positions[mface->v1]);
            copy_v3_v3(v2, positions[mface->v2]);
            copy_v3_v3(v3, positions[mface->v3]);

            bool intersects_tri = isect_ray_tri_watertight_v3(
                co1, &isect_precalc, v1, v2, v3, &lambda, nullptr);
            if (intersects_tri) {
              pa1 = (pa + int(lambda * size[a]) * a0mul);
            }

            if (mface->v4 && (!intersects_tri || from == PART_FROM_VOLUME)) {
              copy_v3_v3(v4, positions[mface->v4]);

              if (isect_ray_tri_watertight_v3(co1, &isect_precalc, v1, v3, v4, &lambda, nullptr)) {
                pa2 = (pa + int(lambda * size[a]) * a0mul);
              }
            }

            if (pa1) {
              if (from == PART_FROM_FACE) {
                pa1->flag &= ~PARS_UNEXIST;
              }
              else { /* store number of intersections */
                pa1->hair_index++;
              }
            }

            if (pa2 && pa2 != pa1) {
              if (from == PART_FROM_FACE) {
                pa2->flag &= ~PARS_UNEXIST;
              }
              else { /* store number of intersections */
                pa2->hair_index++;
              }
            }
          }

          if (from == PART_FROM_VOLUME) {
            int in = pa->hair_index % 2;
            if (in) {
              pa->hair_index++;
            }
            for (i = 0; i < size[0]; i++) {
              if (in || (pa + i * a0mul)->hair_index % 2) {
                (pa + i * a0mul)->flag &= ~PARS_UNEXIST;
              }
              /* odd intersections == in->out / out->in */
              /* even intersections -> in stays same */
              in = (in + (pa + i * a0mul)->hair_index) % 2;
            }
          }
        }
      }
    }
  }

  if (psys->part->flag & PART_GRID_HEXAGONAL) {
    for (i = 0, p = 0, pa = psys->particles; i < res; i++) {
      for (j = 0; j < res; j++) {
        for (k = 0; k < res; k++, p++, pa++) {
          if (j % 2) {
            pa->fuv[0] += d / 2.0f;
          }

          if (k % 2) {
            pa->fuv[0] += d / 2.0f;
            pa->fuv[1] += d / 2.0f;
          }
        }
      }
    }
  }

  if (psys->part->flag & PART_GRID_INVERT) {
    for (i = 0; i < size[0]; i++) {
      for (j = 0; j < size[1]; j++) {
        pa = psys->particles + res * (i * res + j);
        for (k = 0; k < size[2]; k++, pa++) {
          pa->flag ^= PARS_UNEXIST;
        }
      }
    }
  }

  if (psys->part->grid_rand > 0.0f) {
    float rfac = d * psys->part->grid_rand;
    for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++) {
      if (pa->flag & PARS_UNEXIST) {
        continue;
      }

      pa->fuv[0] += rfac * (psys_frand(psys, p + 31) - 0.5f);
      pa->fuv[1] += rfac * (psys_frand(psys, p + 32) - 0.5f);
      pa->fuv[2] += rfac * (psys_frand(psys, p + 33) - 0.5f);
    }
  }
}

static void hammersley_create(float *out, int n, int seed, float amount)
{
  /* This code is originally from a modified copy from `rayshade.c`
   * (a file that's no longer included). */
  RNG *rng;

  double ofs[2], t;

  rng = BLI_rng_new(31415926 + n + seed);
  ofs[0] = BLI_rng_get_double(rng) + double(amount);
  ofs[1] = BLI_rng_get_double(rng) + double(amount);
  BLI_rng_free(rng);

  for (int k = 0; k < n; k++) {
    BLI_hammersley_1d(k, &t);

    out[2 * k + 0] = fmod(double(k) / double(n) + ofs[0], 1.0);
    out[2 * k + 1] = fmod(t + ofs[1], 1.0);
  }
}

/* almost exact copy of BLI_jitter_init */
static void init_mv_jit(float *jit, int num, int seed2, float amount)
{
  RNG *rng;
  float *jit2, x, rad1, rad2, rad3;
  int i, num2;

  if (num == 0) {
    return;
  }

  rad1 = (1.0f / sqrtf(float(num)));
  rad2 = (1.0f / float(num));
  rad3 = (sqrtf(float(num)) / float(num));

  rng = BLI_rng_new(31415926 + num + seed2);
  x = 0;
  num2 = 2 * num;
  for (i = 0; i < num2; i += 2) {

    jit[i] = x + amount * rad1 * (0.5f - BLI_rng_get_float(rng));
    jit[i + 1] = i / (2.0f * num) + amount * rad1 * (0.5f - BLI_rng_get_float(rng));

    jit[i] -= floor(jit[i]);
    jit[i + 1] -= floor(jit[i + 1]);

    x += rad3;
    x -= floor(x);
  }

  /* FIXME: The `+ 3` number of items does not seem to be required? */
  jit2 = MEM_malloc_arrayN<float>(3 + 2 * size_t(num), "initjit");

  for (i = 0; i < 4; i++) {
    BLI_jitterate1((float (*)[2])jit, (float (*)[2])jit2, num, rad1);
    BLI_jitterate1((float (*)[2])jit, (float (*)[2])jit2, num, rad1);
    BLI_jitterate2((float (*)[2])jit, (float (*)[2])jit2, num, rad2);
  }
  MEM_freeN(jit2);
  BLI_rng_free(rng);
}

static void psys_uv_to_w(float u, float v, int quad, float *w)
{
  float vert[4][3], co[3];

  if (!quad) {
    if (u + v > 1.0f) {
      v = 1.0f - v;
    }
    else {
      u = 1.0f - u;
    }
  }

  vert[0][0] = 0.0f;
  vert[0][1] = 0.0f;
  vert[0][2] = 0.0f;
  vert[1][0] = 1.0f;
  vert[1][1] = 0.0f;
  vert[1][2] = 0.0f;
  vert[2][0] = 1.0f;
  vert[2][1] = 1.0f;
  vert[2][2] = 0.0f;

  co[0] = u;
  co[1] = v;
  co[2] = 0.0f;

  if (quad) {
    vert[3][0] = 0.0f;
    vert[3][1] = 1.0f;
    vert[3][2] = 0.0f;
    interp_weights_poly_v3(w, vert, 4, co);
  }
  else {
    interp_weights_poly_v3(w, vert, 3, co);
    w[3] = 0.0f;
  }
}

/* Find the index in "sum" array before "value" is crossed. */
static int distribute_binary_search(const float *sum, int n, float value)
{
  int mid, low = 0, high = n - 1;

  if (high == low) {
    return low;
  }

  if (sum[low] >= value) {
    return low;
  }

  if (sum[high - 1] < value) {
    return high;
  }

  while (low < high) {
    mid = (low + high) / 2;

    if ((sum[mid] >= value) && (sum[mid - 1] < value)) {
      return mid;
    }

    if (sum[mid] > value) {
      high = mid - 1;
    }
    else {
      low = mid + 1;
    }
  }

  return low;
}

/* the max number if calls to rng_* functions within psys_thread_distribute_particle
 * be sure to keep up to date if this changes */
#define PSYS_RND_DIST_SKIP 3

/* NOTE: this function must be thread safe, for `from == PART_FROM_CHILD`. */
#define ONLY_WORKING_WITH_PA_VERTS 0
static void distribute_from_verts_exec(ParticleTask *thread, ParticleData *pa, int p)
{
  ParticleThreadContext *ctx = thread->ctx;
  MFace *mface;

  mface = static_cast<MFace *>(CustomData_get_layer_for_write(
      &ctx->mesh->fdata_legacy, CD_MFACE, ctx->mesh->totface_legacy));

  int rng_skip_tot = PSYS_RND_DIST_SKIP; /* count how many rng_* calls won't need skipping */

  /* TODO_PARTICLE - use original index */
  pa->num = ctx->index[p];

  zero_v4(pa->fuv);

  if (pa->num != DMCACHE_NOTFOUND && pa->num < ctx->mesh->verts_num) {

    /* This finds the first face to contain the emitting vertex,
     * this is not ideal, but is mostly fine as UV seams generally
     * map to equal-colored parts of a texture */
    for (int i = 0; i < ctx->mesh->totface_legacy; i++, mface++) {
      if (ELEM(pa->num, mface->v1, mface->v2, mface->v3, mface->v4)) {
        uint *vert = &mface->v1;

        for (int j = 0; j < 4; j++, vert++) {
          if (*vert == pa->num) {
            pa->fuv[j] = 1.0f;
            break;
          }
        }

        break;
      }
    }
  }

#if ONLY_WORKING_WITH_PA_VERTS
  if (ctx->tree) {
    KDTreeNearest_3d ptn[3];
    int w, maxw;

    psys_particle_on_dm(
        ctx->mesh, from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, co1, 0, 0, 0, orco1, 0);
    BKE_mesh_orco_verts_transform(ob->data, &orco1, 1, true);
    maxw = BLI_kdtree_3d_find_nearest_n(ctx->tree, orco1, ptn, 3);

    for (w = 0; w < maxw; w++) {
      pa->verts[w] = ptn->num;
    }
  }
#endif

  BLI_assert(rng_skip_tot >= 0); /* should never be below zero */
  if (rng_skip_tot > 0) {
    BLI_rng_skip(thread->rng, rng_skip_tot);
  }
}

static void distribute_from_faces_exec(ParticleTask *thread, ParticleData *pa, int p)
{
  ParticleThreadContext *ctx = thread->ctx;
  Mesh *mesh = ctx->mesh;
  float randu, randv;
  int distr = ctx->distr;
  int i;
  int rng_skip_tot = PSYS_RND_DIST_SKIP; /* count how many rng_* calls won't need skipping */

  MFace *mfaces = (MFace *)CustomData_get_layer_for_write(
      &mesh->fdata_legacy, CD_MFACE, mesh->totface_legacy);
  MFace *mface;

  pa->num = i = ctx->index[p];
  mface = &mfaces[i];

  switch (distr) {
    case PART_DISTR_JIT:
      if (ctx->jitlevel == 1) {
        if (mface->v4) {
          psys_uv_to_w(0.5f, 0.5f, mface->v4, pa->fuv);
        }
        else {
          psys_uv_to_w(1.0f / 3.0f, 1.0f / 3.0f, mface->v4, pa->fuv);
        }
      }
      else {
        float offset = fmod(ctx->jitoff[i] + float(p), float(ctx->jitlevel));
        if (!isnan(offset)) {
          psys_uv_to_w(
              ctx->jit[2 * int(offset)], ctx->jit[2 * int(offset) + 1], mface->v4, pa->fuv);
        }
      }
      break;
    case PART_DISTR_RAND:
      randu = BLI_rng_get_float(thread->rng);
      randv = BLI_rng_get_float(thread->rng);
      rng_skip_tot -= 2;

      psys_uv_to_w(randu, randv, mface->v4, pa->fuv);
      break;
  }
  pa->foffset = 0.0f;

  BLI_assert(rng_skip_tot >= 0); /* should never be below zero */
  if (rng_skip_tot > 0) {
    BLI_rng_skip(thread->rng, rng_skip_tot);
  }
}

static void distribute_from_volume_exec(ParticleTask *thread, ParticleData *pa, int p)
{
  ParticleThreadContext *ctx = thread->ctx;
  Mesh *mesh = ctx->mesh;
  const float *v1, *v2, *v3, *v4;
  float nor[3], co[3];
  float cur_d, min_d, randu, randv;
  int distr = ctx->distr;
  int i, intersect, tot;
  int rng_skip_tot = PSYS_RND_DIST_SKIP; /* count how many rng_* calls won't need skipping */

  MFace *mface;
  const blender::Span<blender::float3> positions = mesh->vert_positions();

  pa->num = i = ctx->index[p];
  MFace *mfaces = (MFace *)CustomData_get_layer_for_write(
      &mesh->fdata_legacy, CD_MFACE, mesh->totface_legacy);
  mface = &mfaces[i];

  switch (distr) {
    case PART_DISTR_JIT:
      if (ctx->jitlevel == 1) {
        if (mface->v4) {
          psys_uv_to_w(0.5f, 0.5f, mface->v4, pa->fuv);
        }
        else {
          psys_uv_to_w(1.0f / 3.0f, 1.0f / 3.0f, mface->v4, pa->fuv);
        }
      }
      else {
        float offset = fmod(ctx->jitoff[i] + float(p), float(ctx->jitlevel));
        if (!isnan(offset)) {
          psys_uv_to_w(
              ctx->jit[2 * int(offset)], ctx->jit[2 * int(offset) + 1], mface->v4, pa->fuv);
        }
      }
      break;
    case PART_DISTR_RAND:
      randu = BLI_rng_get_float(thread->rng);
      randv = BLI_rng_get_float(thread->rng);
      rng_skip_tot -= 2;

      psys_uv_to_w(randu, randv, mface->v4, pa->fuv);
      break;
  }
  pa->foffset = 0.0f;

  /* experimental */
  tot = mesh->totface_legacy;

  psys_interpolate_face(mesh,
                        reinterpret_cast<const float (*)[3]>(positions.data()),
                        reinterpret_cast<const float (*)[3]>(mesh->vert_normals().data()),
                        mface,
                        nullptr,
                        nullptr,
                        pa->fuv,
                        co,
                        nor,
                        nullptr,
                        nullptr,
                        nullptr);

  normalize_v3(nor);
  negate_v3(nor);

  min_d = FLT_MAX;
  intersect = 0;
  mface = static_cast<MFace *>(
      CustomData_get_layer_for_write(&mesh->fdata_legacy, CD_MFACE, mesh->totface_legacy));
  for (i = 0; i < tot; i++, mface++) {
    if (i == pa->num) {
      continue;
    }

    v1 = positions[mface->v1];
    v2 = positions[mface->v2];
    v3 = positions[mface->v3];

    if (isect_ray_tri_v3(co, nor, v2, v3, v1, &cur_d, nullptr)) {
      if (cur_d < min_d) {
        min_d = cur_d;
        pa->foffset = cur_d * 0.5f; /* to the middle of volume */
        intersect = 1;
      }
    }
    if (mface->v4) {
      v4 = positions[mface->v4];

      if (isect_ray_tri_v3(co, nor, v4, v1, v3, &cur_d, nullptr)) {
        if (cur_d < min_d) {
          min_d = cur_d;
          pa->foffset = cur_d * 0.5f; /* to the middle of volume */
          intersect = 1;
        }
      }
    }
  }
  if (intersect == 0) {
    pa->foffset = 0.0;
  }
  else {
    switch (distr) {
      case PART_DISTR_JIT:
        pa->foffset *= ctx->jit[p % (2 * ctx->jitlevel)];
        break;
      case PART_DISTR_RAND:
        pa->foffset *= BLI_rng_get_float(thread->rng);
        rng_skip_tot--;
        break;
    }
  }

  BLI_assert(rng_skip_tot >= 0); /* should never be below zero */
  if (rng_skip_tot > 0) {
    BLI_rng_skip(thread->rng, rng_skip_tot);
  }
}

static void distribute_children_exec(ParticleTask *thread, ChildParticle *cpa, int p)
{
  ParticleThreadContext *ctx = thread->ctx;
  Object *ob = ctx->sim.ob;
  Mesh *mesh = ctx->mesh;
  float orco1[3], co1[3], nor1[3];
  float randu, randv;
  int cfrom = ctx->cfrom;
  int i;
  int rng_skip_tot = PSYS_RND_DIST_SKIP; /* count how many rng_* calls won't need skipping */

  MFace *mf;

  if (ctx->index[p] < 0) {
    cpa->num = 0;
    cpa->fuv[0] = cpa->fuv[1] = cpa->fuv[2] = cpa->fuv[3] = 0.0f;
    cpa->pa[0] = cpa->pa[1] = cpa->pa[2] = cpa->pa[3] = 0;
    return;
  }

  MFace *mfaces = (MFace *)CustomData_get_layer_for_write(
      &mesh->fdata_legacy, CD_MFACE, mesh->totface_legacy);
  mf = &mfaces[ctx->index[p]];

  randu = BLI_rng_get_float(thread->rng);
  randv = BLI_rng_get_float(thread->rng);
  rng_skip_tot -= 2;

  psys_uv_to_w(randu, randv, mf->v4, cpa->fuv);

  cpa->num = ctx->index[p];

  if (ctx->tree) {
    KDTreeNearest_3d ptn[10];
    int w, maxw;  //, do_seams;
    float maxd /*, mind,dd */, totw = 0.0f;
    int parent[10];
    float pweight[10];

    psys_particle_on_dm(mesh,
                        cfrom,
                        cpa->num,
                        DMCACHE_ISCHILD,
                        cpa->fuv,
                        cpa->foffset,
                        co1,
                        nor1,
                        nullptr,
                        nullptr,
                        orco1);
    BKE_mesh_orco_verts_transform(static_cast<Mesh *>(ob->data), &orco1, 1, true);
    maxw = BLI_kdtree_3d_find_nearest_n(ctx->tree, orco1, ptn, 3);

    maxd = ptn[maxw - 1].dist;
    // mind=ptn[0].dist; /* UNUSED */

    /* the weights here could be done better */
    for (w = 0; w < maxw; w++) {
      parent[w] = ptn[w].index;
      pweight[w] = float(pow(2.0, double(-6.0f * ptn[w].dist / maxd)));
    }
    for (; w < 10; w++) {
      parent[w] = -1;
      pweight[w] = 0.0f;
    }

    for (w = 0, i = 0; w < maxw && i < 4; w++) {
      if (parent[w] >= 0) {
        cpa->pa[i] = parent[w];
        cpa->w[i] = pweight[w];
        totw += pweight[w];
        i++;
      }
    }
    for (; i < 4; i++) {
      cpa->pa[i] = -1;
      cpa->w[i] = 0.0f;
    }

    if (totw > 0.0f) {
      for (w = 0; w < 4; w++) {
        cpa->w[w] /= totw;
      }
    }

    cpa->parent = cpa->pa[0];
  }

  if (rng_skip_tot > 0) { /* should never be below zero */
    BLI_rng_skip(thread->rng, rng_skip_tot);
  }
}

static void exec_distribute_parent(TaskPool *__restrict /*pool*/, void *taskdata)
{
  ParticleTask *task = static_cast<ParticleTask *>(taskdata);
  ParticleSystem *psys = task->ctx->sim.psys;
  ParticleData *pa;
  int p;

  BLI_rng_skip(task->rng, PSYS_RND_DIST_SKIP * task->begin);

  pa = psys->particles + task->begin;
  switch (psys->part->from) {
    case PART_FROM_FACE:
      for (p = task->begin; p < task->end; p++, pa++) {
        distribute_from_faces_exec(task, pa, p);
      }
      break;
    case PART_FROM_VOLUME:
      for (p = task->begin; p < task->end; p++, pa++) {
        distribute_from_volume_exec(task, pa, p);
      }
      break;
    case PART_FROM_VERT:
      for (p = task->begin; p < task->end; p++, pa++) {
        distribute_from_verts_exec(task, pa, p);
      }
      break;
  }
}

static void exec_distribute_child(TaskPool *__restrict /*pool*/, void *taskdata)
{
  ParticleTask *task = static_cast<ParticleTask *>(taskdata);
  ParticleSystem *psys = task->ctx->sim.psys;
  ChildParticle *cpa;
  int p;

  /* RNG skipping at the beginning */
  cpa = psys->child;
  for (p = 0; p < task->begin; p++, cpa++) {
    BLI_rng_skip(task->rng, PSYS_RND_DIST_SKIP);
  }

  for (; p < task->end; p++, cpa++) {
    distribute_children_exec(task, cpa, p);
  }
}

static int distribute_compare_orig_index(const void *p1, const void *p2, void *user_data)
{
  const int *orig_index = (const int *)user_data;
  int index1 = orig_index[*(const int *)p1];
  int index2 = orig_index[*(const int *)p2];

  if (index1 < index2) {
    return -1;
  }
  if (index1 == index2) {
    /* This pointer comparison appears to make #qsort stable for GLIBC,
     * and apparently on SOLARIS too, makes the renders reproducible. */
    if (p1 < p2) {
      return -1;
    }
    if (p1 == p2) {
      return 0;
    }

    return 1;
  }

  return 1;
}

static void distribute_invalid(ParticleSimulationData *sim, int from)
{
  Scene *scene = sim->scene;
  ParticleSystem *psys = sim->psys;
  const bool use_render_params = (DEG_get_mode(sim->depsgraph) == DAG_EVAL_RENDER);

  if (from == PART_FROM_CHILD) {
    ChildParticle *cpa;
    int p, totchild = psys_get_tot_child(scene, psys, use_render_params);

    if (psys->child && totchild) {
      for (p = 0, cpa = psys->child; p < totchild; p++, cpa++) {
        cpa->fuv[0] = cpa->fuv[1] = cpa->fuv[2] = cpa->fuv[3] = 0.0;
        cpa->foffset = 0.0f;
        cpa->parent = 0;
        cpa->pa[0] = cpa->pa[1] = cpa->pa[2] = cpa->pa[3] = 0;
        cpa->num = -1;
      }
    }
  }
  else {
    PARTICLE_P;
    LOOP_PARTICLES
    {
      pa->fuv[0] = pa->fuv[1] = pa->fuv[2] = pa->fuv[3] = 0.0;
      pa->foffset = 0.0f;
      pa->num = -1;
    }
  }
}

/* Creates a distribution of coordinates on a Mesh */
static int psys_thread_context_init_distribute(ParticleThreadContext *ctx,
                                               ParticleSimulationData *sim,
                                               int from)
{
  Scene *scene = sim->scene;
  Mesh *final_mesh = sim->psmd->mesh_final;
  Object *ob = sim->ob;
  ParticleSystem *psys = sim->psys;
  ParticleData *pa = nullptr, *tpars = nullptr;
  ParticleSettings *part;
  ParticleSeam *seams = nullptr;
  KDTree_3d *tree = nullptr;
  Mesh *mesh = nullptr;
  float *jit = nullptr;
  int i, p = 0;
  int cfrom = 0;
  int totelem = 0, totpart, *particle_element = nullptr, children = 0, totseam = 0;
  int jitlevel = 1, distr;
  float *element_weight = nullptr, *jitter_offset = nullptr, *vweight = nullptr;
  float cur, maxweight = 0.0, tweight, totweight, inv_totweight, co[3], nor[3], orco[3];
  RNG *rng = nullptr;

  if (ELEM(nullptr, ob, psys, psys->part)) {
    return 0;
  }

  part = psys->part;
  totpart = psys->totpart;
  if (totpart == 0) {
    return 0;
  }

  if (!final_mesh->runtime->deformed_only &&
      !CustomData_get_layer(&final_mesh->fdata_legacy, CD_ORIGINDEX))
  {
    printf(
        "Can't create particles with the current modifier stack, disable destructive modifiers\n");
    // XXX error("Can't paint with the current modifier stack, disable destructive modifiers");
    return 0;
  }

  /* XXX This distribution code is totally broken in case from == PART_FROM_CHILD,
   *     it's always using `final_mesh` even if use_modifier_stack is unset...
   *     But making things consistent here break all existing edited
   *     hair systems, so better wait for complete rewrite. */

  psys_thread_context_init(ctx, sim);

  const bool use_render_params = (DEG_get_mode(sim->depsgraph) == DAG_EVAL_RENDER);

  /* First handle special cases */
  if (from == PART_FROM_CHILD) {
    /* Simple children */
    if (part->childtype != PART_CHILD_FACES) {
      distribute_simple_children(
          scene, ob, final_mesh, sim->psmd->mesh_original, psys, use_render_params);
      return 0;
    }
  }
  else {
    /* Grid distribution */
    if (part->distr == PART_DISTR_GRID && from != PART_FROM_VERT) {
      if (psys->part->use_modifier_stack) {
        mesh = final_mesh;
      }
      else {
        mesh = (Mesh *)BKE_id_copy_ex(
            nullptr, static_cast<const ID *>(ob->data), nullptr, LIB_ID_COPY_LOCALIZE);
      }
      BKE_mesh_tessface_ensure(mesh);

      distribute_grid(mesh, psys);

      if (mesh != final_mesh) {
        BKE_id_free(nullptr, mesh);
      }

      return 0;
    }
  }

  /* After this #BKE_mesh_orco_verts_transform can be used safely from multiple threads. */
  BKE_mesh_texspace_ensure(final_mesh);

  /* Create trees and original coordinates if needed */
  if (from == PART_FROM_CHILD) {
    distr = PART_DISTR_RAND;
    rng = BLI_rng_new_srandom(31415926 + psys->seed + psys->child_seed);
    mesh = final_mesh;

    /* BMESH ONLY */
    BKE_mesh_tessface_ensure(mesh);

    children = 1;

    tree = BLI_kdtree_3d_new(totpart);

    for (p = 0, pa = psys->particles; p < totpart; p++, pa++) {
      psys_particle_on_dm(mesh,
                          part->from,
                          pa->num,
                          pa->num_dmcache,
                          pa->fuv,
                          pa->foffset,
                          co,
                          nor,
                          nullptr,
                          nullptr,
                          orco);
      BKE_mesh_orco_verts_transform(static_cast<Mesh *>(ob->data), &orco, 1, true);
      BLI_kdtree_3d_insert(tree, p, orco);
    }

    BLI_kdtree_3d_balance(tree);

    totpart = psys_get_tot_child(scene, psys, use_render_params);
    cfrom = from = PART_FROM_FACE;
  }
  else {
    distr = part->distr;

    rng = BLI_rng_new_srandom(31415926 + psys->seed);

    if (psys->part->use_modifier_stack) {
      mesh = final_mesh;
    }
    else {
      mesh = (Mesh *)BKE_id_copy_ex(
          nullptr, static_cast<const ID *>(ob->data), nullptr, LIB_ID_COPY_LOCALIZE);
    }

    BKE_mesh_tessface_ensure(mesh);

    /* we need orco for consistent distributions */
    BKE_mesh_orco_ensure(ob, mesh);

    if (from == PART_FROM_VERT) {
      const blender::Span<blender::float3> positions = mesh->vert_positions();
      const float (*orcodata)[3] = static_cast<const float (*)[3]>(
          CustomData_get_layer(&mesh->vert_data, CD_ORCO));
      int totvert = mesh->verts_num;

      tree = BLI_kdtree_3d_new(totvert);

      for (p = 0; p < totvert; p++) {
        if (orcodata) {
          copy_v3_v3(co, orcodata[p]);
          BKE_mesh_orco_verts_transform(static_cast<Mesh *>(ob->data), &co, 1, true);
        }
        else {
          copy_v3_v3(co, positions[p]);
        }
        BLI_kdtree_3d_insert(tree, p, co);
      }

      BLI_kdtree_3d_balance(tree);
    }
  }

  /* Get total number of emission elements and allocate needed arrays */
  totelem = (from == PART_FROM_VERT) ? mesh->verts_num : mesh->totface_legacy;

  if (totelem == 0) {
    distribute_invalid(sim, children ? PART_FROM_CHILD : 0);

    if (G.debug & G_DEBUG) {
      fprintf(stderr, "Particle distribution error: Nothing to emit from!\n");
    }

    if (mesh != final_mesh) {
      BKE_id_free(nullptr, mesh);
    }

    BLI_kdtree_3d_free(tree);
    BLI_rng_free(rng);

    return 0;
  }

  element_weight = MEM_calloc_arrayN<float>(totelem, "particle_distribution_weights");
  particle_element = MEM_calloc_arrayN<int>(totpart, "particle_distribution_indexes");
  jitter_offset = MEM_calloc_arrayN<float>(totelem, "particle_distribution_jitoff");

  /* Calculate weights from face areas */
  if ((part->flag & PART_EDISTR || children) && from != PART_FROM_VERT) {
    float totarea = 0.0f, co1[3], co2[3], co3[3], co4[3];
    const float (*orcodata)[3];

    orcodata = static_cast<const float (*)[3]>(CustomData_get_layer(&mesh->vert_data, CD_ORCO));

    MFace *mfaces = (MFace *)CustomData_get_layer_for_write(
        &mesh->fdata_legacy, CD_MFACE, mesh->totface_legacy);
    for (i = 0; i < totelem; i++) {
      MFace *mf = &mfaces[i];

      if (orcodata) {
        /* Transform orcos from normalized 0..1 to object space. */
        copy_v3_v3(co1, orcodata[mf->v1]);
        copy_v3_v3(co2, orcodata[mf->v2]);
        copy_v3_v3(co3, orcodata[mf->v3]);
        BKE_mesh_orco_verts_transform(static_cast<Mesh *>(ob->data), &co1, 1, true);
        BKE_mesh_orco_verts_transform(static_cast<Mesh *>(ob->data), &co2, 1, true);
        BKE_mesh_orco_verts_transform(static_cast<Mesh *>(ob->data), &co3, 1, true);
        if (mf->v4) {
          copy_v3_v3(co4, orcodata[mf->v4]);
          BKE_mesh_orco_verts_transform(static_cast<Mesh *>(ob->data), &co4, 1, true);
        }
      }
      else {
        blender::MutableSpan<blender::float3> positions = mesh->vert_positions_for_write();
        copy_v3_v3(co1, positions[mf->v1]);
        copy_v3_v3(co2, positions[mf->v2]);
        copy_v3_v3(co3, positions[mf->v3]);
        if (mf->v4) {
          copy_v3_v3(co4, positions[mf->v4]);
        }
      }

      cur = mf->v4 ? area_quad_v3(co1, co2, co3, co4) : area_tri_v3(co1, co2, co3);

      maxweight = std::max(cur, maxweight);

      element_weight[i] = cur;
      totarea += cur;
    }

    for (i = 0; i < totelem; i++) {
      element_weight[i] /= totarea;
    }

    maxweight /= totarea;
  }
  else {
    float min = 1.0f / float(std::min(totelem, totpart));
    for (i = 0; i < totelem; i++) {
      element_weight[i] = min;
    }
    maxweight = min;
  }

  /* Calculate weights from vgroup */
  vweight = psys_cache_vgroup(mesh, psys, PSYS_VG_DENSITY);

  if (vweight) {
    if (from == PART_FROM_VERT) {
      for (i = 0; i < totelem; i++) {
        element_weight[i] *= vweight[i];
      }
    }
    else { /* PART_FROM_FACE / PART_FROM_VOLUME */
      MFace *mfaces = (MFace *)CustomData_get_layer_for_write(
          &mesh->fdata_legacy, CD_MFACE, mesh->totface_legacy);
      for (i = 0; i < totelem; i++) {
        MFace *mf = &mfaces[i];
        tweight = vweight[mf->v1] + vweight[mf->v2] + vweight[mf->v3];

        if (mf->v4) {
          tweight += vweight[mf->v4];
          tweight /= 4.0f;
        }
        else {
          tweight /= 3.0f;
        }

        element_weight[i] *= tweight;
      }
    }
    MEM_freeN(vweight);
  }

  /* Calculate total weight of all elements */
  int totmapped = 0;
  totweight = 0.0f;
  for (i = 0; i < totelem; i++) {
    if (element_weight[i] > 0.0f) {
      totmapped++;
      totweight += element_weight[i];
    }
  }

  if (totmapped == 0) {
    /* We are not allowed to distribute particles anywhere... */
    if (mesh != final_mesh) {
      BKE_id_free(nullptr, mesh);
    }
    BLI_kdtree_3d_free(tree);
    BLI_rng_free(rng);
    MEM_freeN(element_weight);
    MEM_freeN(particle_element);
    MEM_freeN(jitter_offset);
    return 0;
  }

  inv_totweight = 1.0f / totweight;

  /* Calculate cumulative weights.
   * We remove all null-weighted elements from element_sum, and create a new mapping
   * 'activ'_elem_index -> orig_elem_index.
   * This simplifies greatly the filtering of zero-weighted items - and can be much more efficient
   * especially in random case (reducing a lot the size of binary-searched array)...
   */
  float *element_sum = MEM_malloc_arrayN<float>(size_t(totmapped), __func__);
  int *element_map = MEM_malloc_arrayN<int>(size_t(totmapped), __func__);
  int i_mapped = 0;

  for (i = 0; i < totelem && element_weight[i] == 0.0f; i++) {
    /* pass */
  }
  element_sum[i_mapped] = element_weight[i] * inv_totweight;
  element_map[i_mapped] = i;
  i_mapped++;
  for (i++; i < totelem; i++) {
    if (element_weight[i] > 0.0f) {
      element_sum[i_mapped] = element_sum[i_mapped - 1] + element_weight[i] * inv_totweight;
      /* Skip elements which weight is so small that it does not affect the sum. */
      if (element_sum[i_mapped] > element_sum[i_mapped - 1]) {
        element_map[i_mapped] = i;
        i_mapped++;
      }
    }
  }
  totmapped = i_mapped;

  /* Finally assign elements to particles */
  if (part->flag & PART_TRAND) {
    for (p = 0; p < totpart; p++) {
      /* In theory element_sum[totmapped - 1] should be 1.0,
       * but due to float errors this is not necessarily always true, so scale pos accordingly. */
      const float pos = BLI_rng_get_float(rng) * element_sum[totmapped - 1];
      const int eidx = distribute_binary_search(element_sum, totmapped, pos);
      particle_element[p] = element_map[eidx];
      BLI_assert(pos <= element_sum[eidx]);
      BLI_assert(eidx ? (pos > element_sum[eidx - 1]) : (pos >= 0.0f));
      jitter_offset[particle_element[p]] = pos;
    }
  }
  else {
    double step, pos;

    step = (totpart < 2) ? 0.5 : 1.0 / double(totpart);
    /* This is to address tricky issues with vertex-emitting when user tries
     * (and expects) exact 1-1 vert/part distribution (see #47983 and its two example files).
     * It allows us to consider pos as 'midpoint between v and v+1'
     * (or 'p and p+1', depending whether we have more vertices than particles or not),
     * and avoid stumbling over float impression in element_sum.
     * NOTE: moved face and volume distribution to this as well (instead of starting at zero),
     * for the same reasons, see #52682. */
    pos = (totpart < totmapped) ? 0.5 / double(totmapped) :
                                  step * 0.5; /* We choose the smaller step. */

    for (i = 0, p = 0; p < totpart; p++, pos += step) {
      for (; (i < totmapped - 1) && (pos > double(element_sum[i])); i++) {
        /* pass */
      }

      particle_element[p] = element_map[i];

      jitter_offset[particle_element[p]] = pos;
    }
  }

  MEM_freeN(element_sum);
  MEM_freeN(element_map);

  /* For hair, sort by #CD_ORIGINDEX (allows optimization's in rendering),
   * however with virtual parents the children need to be in random order. */
  if (part->type == PART_HAIR && !(part->childtype == PART_CHILD_FACES && part->parents != 0.0f)) {
    const int *orig_index = nullptr;

    if (from == PART_FROM_VERT) {
      if (mesh->verts_num) {
        orig_index = static_cast<const int *>(
            CustomData_get_layer(&mesh->vert_data, CD_ORIGINDEX));
      }
    }
    else {
      if (mesh->totface_legacy) {
        orig_index = static_cast<const int *>(
            CustomData_get_layer(&mesh->fdata_legacy, CD_ORIGINDEX));
      }
    }

    if (orig_index) {
      BLI_qsort_r(particle_element,
                  totpart,
                  sizeof(int),
                  distribute_compare_orig_index,
                  (void *)orig_index);
    }
  }

  /* Create jittering if needed */
  if (distr == PART_DISTR_JIT && ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
    jitlevel = part->userjit;

    if (jitlevel == 0) {
      jitlevel = totpart / totelem;
      if (part->flag & PART_EDISTR) {
        jitlevel *= 2; /* looks better in general, not very scientific */
      }
      jitlevel = std::max(jitlevel, 3);
    }

    jit = MEM_calloc_arrayN<float>(2 + size_t(jitlevel * 2), "jit");

    /* for small amounts of particles we use regular jitter since it looks
     * a bit better, for larger amounts we switch to hammersley sequence
     * because it is much faster */
    if (jitlevel < 25) {
      init_mv_jit(jit, jitlevel, psys->seed, part->jitfac);
    }
    else {
      hammersley_create(jit, jitlevel + 1, psys->seed, part->jitfac);
    }
    BLI_array_randomize(
        jit, sizeof(float[2]), jitlevel, psys->seed); /* for custom jit or even distribution */
  }

  /* Setup things for threaded distribution */
  ctx->tree = tree;
  ctx->seams = seams;
  ctx->totseam = totseam;
  ctx->sim.psys = psys;
  ctx->index = particle_element;
  ctx->jit = jit;
  ctx->jitlevel = jitlevel;
  ctx->jitoff = jitter_offset;
  ctx->weight = element_weight;
  ctx->maxweight = maxweight;
  ctx->cfrom = cfrom;
  ctx->distr = distr;
  ctx->mesh = mesh;
  ctx->tpars = tpars;

  if (children) {
    alloc_child_particles(psys, totpart);
  }

  BLI_rng_free(rng);

  return 1;
}

static void psys_task_init_distribute(ParticleTask *task, ParticleSimulationData *sim)
{
  /* init random number generator */
  int seed = 31415926 + sim->psys->seed;

  task->rng = BLI_rng_new(seed);
}

static void distribute_particles_on_dm(ParticleSimulationData *sim, int from)
{
  ParticleThreadContext ctx;
  Mesh *final_mesh = sim->psmd->mesh_final;

  /* create a task pool for distribution tasks */
  if (!psys_thread_context_init_distribute(&ctx, sim, from)) {
    return;
  }

  TaskPool *task_pool = BLI_task_pool_create(&ctx, TASK_PRIORITY_HIGH);

  const int totpart = (from == PART_FROM_CHILD ? sim->psys->totchild : sim->psys->totpart);
  blender::Vector<ParticleTask> tasks = psys_tasks_create(&ctx, 0, totpart);
  for (ParticleTask &task : tasks) {
    psys_task_init_distribute(&task, sim);
    if (from == PART_FROM_CHILD) {
      BLI_task_pool_push(task_pool, exec_distribute_child, &task, false, nullptr);
    }
    else {
      BLI_task_pool_push(task_pool, exec_distribute_parent, &task, false, nullptr);
    }
  }
  BLI_task_pool_work_and_wait(task_pool);

  BLI_task_pool_free(task_pool);

  psys_calc_dmcache(sim->ob, final_mesh, sim->psmd->mesh_original, sim->psys);

  if (ctx.mesh != final_mesh) {
    BKE_id_free(nullptr, ctx.mesh);
  }

  psys_tasks_free(tasks);

  psys_thread_context_free(&ctx);
}

/* ready for future use, to emit particles without geometry */
static void distribute_particles_on_shape(ParticleSimulationData *sim, int /*from*/)
{
  distribute_invalid(sim, 0);

  fprintf(stderr, "Shape emission not yet possible!\n");
}

void distribute_particles(ParticleSimulationData *sim, int from)
{
  PARTICLE_PSMD;
  int distr_error = 0;

  if (psmd) {
    if (psmd->mesh_final) {
      distribute_particles_on_dm(sim, from);
    }
    else {
      distr_error = 1;
    }
  }
  else {
    distribute_particles_on_shape(sim, from);
  }

  if (distr_error) {
    distribute_invalid(sim, from);

    fprintf(stderr, "Particle distribution error!\n");
  }
}
