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

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_texture_types.h"

#include "BKE_effect.h"
}

#include "implicit.h"
#include "eigen_utils.h"

/* ================ Volumetric Hair Interaction ================
 * adapted from
 *
 * Volumetric Methods for Simulation and Rendering of Hair
 *     (Petrovic, Henne, Anderson, Pixar Technical Memo #06-08, Pixar Animation Studios)
 *
 * as well as
 *
 * "Detail Preserving Continuum Simulation of Straight Hair"
 *     (McAdams, Selle 2009)
 */

/* Note about array indexing:
 * Generally the arrays here are one-dimensional.
 * The relation between 3D indices and the array offset is
 *   offset = x + res_x * y + res_x * res_y * z
 */

static float I[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

BLI_INLINE int floor_int(float value)
{
  return value > 0.0f ? (int)value : ((int)value) - 1;
}

BLI_INLINE float floor_mod(float value)
{
  return value - floorf(value);
}

BLI_INLINE int hair_grid_size(const int res[3])
{
  return res[0] * res[1] * res[2];
}

typedef struct HairGridVert {
  int samples;
  float velocity[3];
  float density;

  float velocity_smooth[3];
} HairGridVert;

typedef struct HairGrid {
  HairGridVert *verts;
  int res[3];
  float gmin[3], gmax[3];
  float cellsize, inv_cellsize;
} HairGrid;

#define HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, axis) \
  (min_ii(max_ii((int)((vec[axis] - gmin[axis]) * scale), 0), res[axis] - 2))

BLI_INLINE int hair_grid_offset(const float vec[3],
                                const int res[3],
                                const float gmin[3],
                                float scale)
{
  int i, j, k;
  i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
  j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
  k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
  return i + (j + k * res[1]) * res[0];
}

BLI_INLINE int hair_grid_interp_weights(
    const int res[3], const float gmin[3], float scale, const float vec[3], float uvw[3])
{
  int i, j, k, offset;

  i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
  j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
  k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
  offset = i + (j + k * res[1]) * res[0];

  uvw[0] = (vec[0] - gmin[0]) * scale - (float)i;
  uvw[1] = (vec[1] - gmin[1]) * scale - (float)j;
  uvw[2] = (vec[2] - gmin[2]) * scale - (float)k;

  //  BLI_assert(0.0f <= uvw[0] && uvw[0] <= 1.0001f);
  //  BLI_assert(0.0f <= uvw[1] && uvw[1] <= 1.0001f);
  //  BLI_assert(0.0f <= uvw[2] && uvw[2] <= 1.0001f);

  return offset;
}

BLI_INLINE void hair_grid_interpolate(const HairGridVert *grid,
                                      const int res[3],
                                      const float gmin[3],
                                      float scale,
                                      const float vec[3],
                                      float *density,
                                      float velocity[3],
                                      float vel_smooth[3],
                                      float density_gradient[3],
                                      float velocity_gradient[3][3])
{
  HairGridVert data[8];
  float uvw[3], muvw[3];
  int res2 = res[1] * res[0];
  int offset;

  offset = hair_grid_interp_weights(res, gmin, scale, vec, uvw);
  muvw[0] = 1.0f - uvw[0];
  muvw[1] = 1.0f - uvw[1];
  muvw[2] = 1.0f - uvw[2];

  data[0] = grid[offset];
  data[1] = grid[offset + 1];
  data[2] = grid[offset + res[0]];
  data[3] = grid[offset + res[0] + 1];
  data[4] = grid[offset + res2];
  data[5] = grid[offset + res2 + 1];
  data[6] = grid[offset + res2 + res[0]];
  data[7] = grid[offset + res2 + res[0] + 1];

  if (density) {
    *density = muvw[2] * (muvw[1] * (muvw[0] * data[0].density + uvw[0] * data[1].density) +
                          uvw[1] * (muvw[0] * data[2].density + uvw[0] * data[3].density)) +
               uvw[2] * (muvw[1] * (muvw[0] * data[4].density + uvw[0] * data[5].density) +
                         uvw[1] * (muvw[0] * data[6].density + uvw[0] * data[7].density));
  }

  if (velocity) {
    int k;
    for (k = 0; k < 3; ++k) {
      velocity[k] = muvw[2] *
                        (muvw[1] * (muvw[0] * data[0].velocity[k] + uvw[0] * data[1].velocity[k]) +
                         uvw[1] * (muvw[0] * data[2].velocity[k] + uvw[0] * data[3].velocity[k])) +
                    uvw[2] *
                        (muvw[1] * (muvw[0] * data[4].velocity[k] + uvw[0] * data[5].velocity[k]) +
                         uvw[1] * (muvw[0] * data[6].velocity[k] + uvw[0] * data[7].velocity[k]));
    }
  }

  if (vel_smooth) {
    int k;
    for (k = 0; k < 3; ++k) {
      vel_smooth[k] = muvw[2] * (muvw[1] * (muvw[0] * data[0].velocity_smooth[k] +
                                            uvw[0] * data[1].velocity_smooth[k]) +
                                 uvw[1] * (muvw[0] * data[2].velocity_smooth[k] +
                                           uvw[0] * data[3].velocity_smooth[k])) +
                      uvw[2] * (muvw[1] * (muvw[0] * data[4].velocity_smooth[k] +
                                           uvw[0] * data[5].velocity_smooth[k]) +
                                uvw[1] * (muvw[0] * data[6].velocity_smooth[k] +
                                          uvw[0] * data[7].velocity_smooth[k]));
    }
  }

  if (density_gradient) {
    density_gradient[0] = muvw[1] * muvw[2] * (data[0].density - data[1].density) +
                          uvw[1] * muvw[2] * (data[2].density - data[3].density) +
                          muvw[1] * uvw[2] * (data[4].density - data[5].density) +
                          uvw[1] * uvw[2] * (data[6].density - data[7].density);

    density_gradient[1] = muvw[2] * muvw[0] * (data[0].density - data[2].density) +
                          uvw[2] * muvw[0] * (data[4].density - data[6].density) +
                          muvw[2] * uvw[0] * (data[1].density - data[3].density) +
                          uvw[2] * uvw[0] * (data[5].density - data[7].density);

    density_gradient[2] = muvw[2] * muvw[0] * (data[0].density - data[4].density) +
                          uvw[2] * muvw[0] * (data[1].density - data[5].density) +
                          muvw[2] * uvw[0] * (data[2].density - data[6].density) +
                          uvw[2] * uvw[0] * (data[3].density - data[7].density);
  }

  if (velocity_gradient) {
    /* XXX TODO */
    zero_m3(velocity_gradient);
  }
}

void BPH_hair_volume_vertex_grid_forces(HairGrid *grid,
                                        const float x[3],
                                        const float v[3],
                                        float smoothfac,
                                        float pressurefac,
                                        float minpressure,
                                        float f[3],
                                        float dfdx[3][3],
                                        float dfdv[3][3])
{
  float gdensity, gvelocity[3], ggrad[3], gvelgrad[3][3], gradlen;

  hair_grid_interpolate(grid->verts,
                        grid->res,
                        grid->gmin,
                        grid->inv_cellsize,
                        x,
                        &gdensity,
                        gvelocity,
                        NULL,
                        ggrad,
                        gvelgrad);

  zero_v3(f);
  sub_v3_v3(gvelocity, v);
  mul_v3_v3fl(f, gvelocity, smoothfac);

  gradlen = normalize_v3(ggrad) - minpressure;
  if (gradlen > 0.0f) {
    mul_v3_fl(ggrad, gradlen);
    madd_v3_v3fl(f, ggrad, pressurefac);
  }

  zero_m3(dfdx);

  sub_m3_m3m3(dfdv, gvelgrad, I);
  mul_m3_fl(dfdv, smoothfac);
}

void BPH_hair_volume_grid_interpolate(HairGrid *grid,
                                      const float x[3],
                                      float *density,
                                      float velocity[3],
                                      float velocity_smooth[3],
                                      float density_gradient[3],
                                      float velocity_gradient[3][3])
{
  hair_grid_interpolate(grid->verts,
                        grid->res,
                        grid->gmin,
                        grid->inv_cellsize,
                        x,
                        density,
                        velocity,
                        velocity_smooth,
                        density_gradient,
                        velocity_gradient);
}

void BPH_hair_volume_grid_velocity(
    HairGrid *grid, const float x[3], const float v[3], float fluid_factor, float r_v[3])
{
  float gdensity, gvelocity[3], gvel_smooth[3], ggrad[3], gvelgrad[3][3];
  float v_pic[3], v_flip[3];

  hair_grid_interpolate(grid->verts,
                        grid->res,
                        grid->gmin,
                        grid->inv_cellsize,
                        x,
                        &gdensity,
                        gvelocity,
                        gvel_smooth,
                        ggrad,
                        gvelgrad);

  /* velocity according to PIC method (Particle-in-Cell) */
  copy_v3_v3(v_pic, gvel_smooth);

  /* velocity according to FLIP method (Fluid-Implicit-Particle) */
  sub_v3_v3v3(v_flip, gvel_smooth, gvelocity);
  add_v3_v3(v_flip, v);

  interp_v3_v3v3(r_v, v_pic, v_flip, fluid_factor);
}

void BPH_hair_volume_grid_clear(HairGrid *grid)
{
  const int size = hair_grid_size(grid->res);
  int i;
  for (i = 0; i < size; ++i) {
    zero_v3(grid->verts[i].velocity);
    zero_v3(grid->verts[i].velocity_smooth);
    grid->verts[i].density = 0.0f;
    grid->verts[i].samples = 0;
  }
}

BLI_INLINE bool hair_grid_point_valid(const float vec[3], float gmin[3], float gmax[3])
{
  return !(vec[0] < gmin[0] || vec[1] < gmin[1] || vec[2] < gmin[2] || vec[0] > gmax[0] ||
           vec[1] > gmax[1] || vec[2] > gmax[2]);
}

BLI_INLINE float dist_tent_v3f3(const float a[3], float x, float y, float z)
{
  float w = (1.0f - fabsf(a[0] - x)) * (1.0f - fabsf(a[1] - y)) * (1.0f - fabsf(a[2] - z));
  return w;
}

BLI_INLINE float weights_sum(const float weights[8])
{
  float totweight = 0.0f;
  int i;
  for (i = 0; i < 8; ++i)
    totweight += weights[i];
  return totweight;
}

/* returns the grid array offset as well to avoid redundant calculation */
BLI_INLINE int hair_grid_weights(
    const int res[3], const float gmin[3], float scale, const float vec[3], float weights[8])
{
  int i, j, k, offset;
  float uvw[3];

  i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
  j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
  k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
  offset = i + (j + k * res[1]) * res[0];

  uvw[0] = (vec[0] - gmin[0]) * scale;
  uvw[1] = (vec[1] - gmin[1]) * scale;
  uvw[2] = (vec[2] - gmin[2]) * scale;

  weights[0] = dist_tent_v3f3(uvw, (float)i, (float)j, (float)k);
  weights[1] = dist_tent_v3f3(uvw, (float)(i + 1), (float)j, (float)k);
  weights[2] = dist_tent_v3f3(uvw, (float)i, (float)(j + 1), (float)k);
  weights[3] = dist_tent_v3f3(uvw, (float)(i + 1), (float)(j + 1), (float)k);
  weights[4] = dist_tent_v3f3(uvw, (float)i, (float)j, (float)(k + 1));
  weights[5] = dist_tent_v3f3(uvw, (float)(i + 1), (float)j, (float)(k + 1));
  weights[6] = dist_tent_v3f3(uvw, (float)i, (float)(j + 1), (float)(k + 1));
  weights[7] = dist_tent_v3f3(uvw, (float)(i + 1), (float)(j + 1), (float)(k + 1));

  //  BLI_assert(fabsf(weights_sum(weights) - 1.0f) < 0.0001f);

  return offset;
}

BLI_INLINE void grid_to_world(HairGrid *grid, float vecw[3], const float vec[3])
{
  copy_v3_v3(vecw, vec);
  mul_v3_fl(vecw, grid->cellsize);
  add_v3_v3(vecw, grid->gmin);
}

void BPH_hair_volume_add_vertex(HairGrid *grid, const float x[3], const float v[3])
{
  const int res[3] = {grid->res[0], grid->res[1], grid->res[2]};
  float weights[8];
  int di, dj, dk;
  int offset;

  if (!hair_grid_point_valid(x, grid->gmin, grid->gmax))
    return;

  offset = hair_grid_weights(res, grid->gmin, grid->inv_cellsize, x, weights);

  for (di = 0; di < 2; ++di) {
    for (dj = 0; dj < 2; ++dj) {
      for (dk = 0; dk < 2; ++dk) {
        int voffset = offset + di + (dj + dk * res[1]) * res[0];
        int iw = di + dj * 2 + dk * 4;

        grid->verts[voffset].density += weights[iw];
        madd_v3_v3fl(grid->verts[voffset].velocity, v, weights[iw]);
      }
    }
  }
}

#if 0
BLI_INLINE void hair_volume_eval_grid_vertex(HairGridVert *vert,
                                             const float loc[3],
                                             float radius,
                                             float dist_scale,
                                             const float x2[3],
                                             const float v2[3],
                                             const float x3[3],
                                             const float v3[3])
{
  float closest[3], lambda, dist, weight;

  lambda = closest_to_line_v3(closest, loc, x2, x3);
  dist = len_v3v3(closest, loc);

  weight = (radius - dist) * dist_scale;

  if (weight > 0.0f) {
    float vel[3];

    interp_v3_v3v3(vel, v2, v3, lambda);
    madd_v3_v3fl(vert->velocity, vel, weight);
    vert->density += weight;
    vert->samples += 1;
  }
}

BLI_INLINE int major_axis_v3(const float v[3])
{
  const float a = fabsf(v[0]);
  const float b = fabsf(v[1]);
  const float c = fabsf(v[2]);
  return a > b ? (a > c ? 0 : 2) : (b > c ? 1 : 2);
}

BLI_INLINE void hair_volume_add_segment_2D(HairGrid *grid,
                                           const float UNUSED(x1[3]),
                                           const float UNUSED(v1[3]),
                                           const float x2[3],
                                           const float v2[3],
                                           const float x3[3],
                                           const float v3[3],
                                           const float UNUSED(x4[3]),
                                           const float UNUSED(v4[3]),
                                           const float UNUSED(dir1[3]),
                                           const float dir2[3],
                                           const float UNUSED(dir3[3]),
                                           int resj,
                                           int resk,
                                           int jmin,
                                           int jmax,
                                           int kmin,
                                           int kmax,
                                           HairGridVert *vert,
                                           int stride_j,
                                           int stride_k,
                                           const float loc[3],
                                           int axis_j,
                                           int axis_k,
                                           int debug_i)
{
  const float radius = 1.5f;
  const float dist_scale = grid->inv_cellsize;

  int j, k;

  /* boundary checks to be safe */
  CLAMP_MIN(jmin, 0);
  CLAMP_MAX(jmax, resj - 1);
  CLAMP_MIN(kmin, 0);
  CLAMP_MAX(kmax, resk - 1);

  HairGridVert *vert_j = vert + jmin * stride_j;
  float loc_j[3] = {loc[0], loc[1], loc[2]};
  loc_j[axis_j] += (float)jmin;
  for (j = jmin; j <= jmax; ++j, vert_j += stride_j, loc_j[axis_j] += 1.0f) {

    HairGridVert *vert_k = vert_j + kmin * stride_k;
    float loc_k[3] = {loc_j[0], loc_j[1], loc_j[2]};
    loc_k[axis_k] += (float)kmin;
    for (k = kmin; k <= kmax; ++k, vert_k += stride_k, loc_k[axis_k] += 1.0f) {

      hair_volume_eval_grid_vertex(vert_k, loc_k, radius, dist_scale, x2, v2, x3, v3);

#  if 0
      {
        float wloc[3], x2w[3], x3w[3];
        grid_to_world(grid, wloc, loc_k);
        grid_to_world(grid, x2w, x2);
        grid_to_world(grid, x3w, x3);

        if (vert_k->samples > 0)
          BKE_sim_debug_data_add_circle(wloc, 0.01f, 1.0, 1.0, 0.3, "grid", 2525, debug_i, j, k);

        if (grid->debug_value) {
          BKE_sim_debug_data_add_dot(wloc, 1, 0, 0, "grid", 93, debug_i, j, k);
          BKE_sim_debug_data_add_dot(x2w, 0.1, 0.1, 0.7, "grid", 649, debug_i, j, k);
          BKE_sim_debug_data_add_line(wloc, x2w, 0.3, 0.8, 0.3, "grid", 253, debug_i, j, k);
          BKE_sim_debug_data_add_line(wloc, x3w, 0.8, 0.3, 0.3, "grid", 254, debug_i, j, k);
          // BKE_sim_debug_data_add_circle(
          //     x2w, len_v3v3(wloc, x2w), 0.2, 0.7, 0.2,
          //     "grid", 255, i, j, k);
        }
      }
#  endif
    }
  }
}

/* Uses a variation of Bresenham's algorithm for rasterizing a 3D grid with a line segment.
 *
 * The radius of influence around a segment is assumed to be at most 2*cellsize,
 * i.e. only cells containing the segment and their direct neighbors are examined.
 */
void BPH_hair_volume_add_segment(HairGrid *grid,
                                 const float x1[3],
                                 const float v1[3],
                                 const float x2[3],
                                 const float v2[3],
                                 const float x3[3],
                                 const float v3[3],
                                 const float x4[3],
                                 const float v4[3],
                                 const float dir1[3],
                                 const float dir2[3],
                                 const float dir3[3])
{
  const int res[3] = {grid->res[0], grid->res[1], grid->res[2]};

  /* find the primary direction from the major axis of the direction vector */
  const int axis0 = major_axis_v3(dir2);
  const int axis1 = (axis0 + 1) % 3;
  const int axis2 = (axis0 + 2) % 3;

  /* vertex buffer offset factors along cardinal axes */
  const int strides[3] = {1, res[0], res[0] * res[1]};
  const int stride0 = strides[axis0];
  const int stride1 = strides[axis1];
  const int stride2 = strides[axis2];

  /* increment of secondary directions per step in the primary direction
   * note: we always go in the positive direction along axis0, so the sign can be inverted
   */
  const float inc1 = dir2[axis1] / dir2[axis0];
  const float inc2 = dir2[axis2] / dir2[axis0];

  /* start/end points, so increment along axis0 is always positive */
  const float *start = x2[axis0] < x3[axis0] ? x2 : x3;
  const float *end = x2[axis0] < x3[axis0] ? x3 : x2;
  const float start0 = start[axis0], start1 = start[axis1], start2 = start[axis2];
  const float end0 = end[axis0];

  /* range along primary direction */
  const int imin = max_ii(floor_int(start[axis0]) - 1, 0);
  const int imax = min_ii(floor_int(end[axis0]) + 2, res[axis0] - 1);

  float h = 0.0f;
  HairGridVert *vert0;
  float loc0[3];
  int j0, k0, j0_prev, k0_prev;
  int i;

  for (i = imin; i <= imax; ++i) {
    float shift1, shift2; /* fraction of a full cell shift [0.0, 1.0) */
    int jmin, jmax, kmin, kmax;

    h = CLAMPIS((float)i, start0, end0);

    shift1 = start1 + (h - start0) * inc1;
    shift2 = start2 + (h - start0) * inc2;

    j0_prev = j0;
    j0 = floor_int(shift1);

    k0_prev = k0;
    k0 = floor_int(shift2);

    if (i > imin) {
      jmin = min_ii(j0, j0_prev);
      jmax = max_ii(j0, j0_prev);
      kmin = min_ii(k0, k0_prev);
      kmax = max_ii(k0, k0_prev);
    }
    else {
      jmin = jmax = j0;
      kmin = kmax = k0;
    }

    vert0 = grid->verts + i * stride0;
    loc0[axis0] = (float)i;
    loc0[axis1] = 0.0f;
    loc0[axis2] = 0.0f;

    hair_volume_add_segment_2D(grid,
                               x1,
                               v1,
                               x2,
                               v2,
                               x3,
                               v3,
                               x4,
                               v4,
                               dir1,
                               dir2,
                               dir3,
                               res[axis1],
                               res[axis2],
                               jmin - 1,
                               jmax + 2,
                               kmin - 1,
                               kmax + 2,
                               vert0,
                               stride1,
                               stride2,
                               loc0,
                               axis1,
                               axis2,
                               i);
  }
}
#else
BLI_INLINE void hair_volume_eval_grid_vertex_sample(HairGridVert *vert,
                                                    const float loc[3],
                                                    float radius,
                                                    float dist_scale,
                                                    const float x[3],
                                                    const float v[3])
{
  float dist, weight;

  dist = len_v3v3(x, loc);

  weight = (radius - dist) * dist_scale;

  if (weight > 0.0f) {
    madd_v3_v3fl(vert->velocity, v, weight);
    vert->density += weight;
    vert->samples += 1;
  }
}

/* XXX simplified test implementation using a series of discrete sample along the segment,
 * instead of finding the closest point for all affected grid vertices.
 */
void BPH_hair_volume_add_segment(HairGrid *grid,
                                 const float UNUSED(x1[3]),
                                 const float UNUSED(v1[3]),
                                 const float x2[3],
                                 const float v2[3],
                                 const float x3[3],
                                 const float v3[3],
                                 const float UNUSED(x4[3]),
                                 const float UNUSED(v4[3]),
                                 const float UNUSED(dir1[3]),
                                 const float UNUSED(dir2[3]),
                                 const float UNUSED(dir3[3]))
{
  const float radius = 1.5f;
  const float dist_scale = grid->inv_cellsize;

  const int res[3] = {grid->res[0], grid->res[1], grid->res[2]};
  const int stride[3] = {1, res[0], res[0] * res[1]};
  const int num_samples = 10;

  int s;

  for (s = 0; s < num_samples; ++s) {
    float x[3], v[3];
    int i, j, k;

    float f = (float)s / (float)(num_samples - 1);
    interp_v3_v3v3(x, x2, x3, f);
    interp_v3_v3v3(v, v2, v3, f);

    int imin = max_ii(floor_int(x[0]) - 2, 0);
    int imax = min_ii(floor_int(x[0]) + 2, res[0] - 1);
    int jmin = max_ii(floor_int(x[1]) - 2, 0);
    int jmax = min_ii(floor_int(x[1]) + 2, res[1] - 1);
    int kmin = max_ii(floor_int(x[2]) - 2, 0);
    int kmax = min_ii(floor_int(x[2]) + 2, res[2] - 1);

    for (k = kmin; k <= kmax; ++k) {
      for (j = jmin; j <= jmax; ++j) {
        for (i = imin; i <= imax; ++i) {
          float loc[3] = {(float)i, (float)j, (float)k};
          HairGridVert *vert = grid->verts + i * stride[0] + j * stride[1] + k * stride[2];

          hair_volume_eval_grid_vertex_sample(vert, loc, radius, dist_scale, x, v);
        }
      }
    }
  }
}
#endif

void BPH_hair_volume_normalize_vertex_grid(HairGrid *grid)
{
  int i, size = hair_grid_size(grid->res);
  /* divide velocity with density */
  for (i = 0; i < size; i++) {
    float density = grid->verts[i].density;
    if (density > 0.0f)
      mul_v3_fl(grid->verts[i].velocity, 1.0f / density);
  }
}

static const float density_threshold =
    0.001f; /* cells with density below this are considered empty */

/* Contribution of target density pressure to the laplacian in the pressure poisson equation.
 * This is based on the model found in
 * "Two-way Coupled SPH and Particle Level Set Fluid Simulation" (Losasso et al., 2008)
 */
BLI_INLINE float hair_volume_density_divergence(float density,
                                                float target_density,
                                                float strength)
{
  if (density > density_threshold && density > target_density)
    return strength * logf(target_density / density);
  else
    return 0.0f;
}

bool BPH_hair_volume_solve_divergence(HairGrid *grid,
                                      float /*dt*/,
                                      float target_density,
                                      float target_strength)
{
  const float flowfac = grid->cellsize;
  const float inv_flowfac = 1.0f / grid->cellsize;

  /*const int num_cells = hair_grid_size(grid->res);*/
  const int res[3] = {grid->res[0], grid->res[1], grid->res[2]};
  const int resA[3] = {grid->res[0] + 2, grid->res[1] + 2, grid->res[2] + 2};

  const int stride0 = 1;
  const int stride1 = grid->res[0];
  const int stride2 = grid->res[1] * grid->res[0];
  const int strideA0 = 1;
  const int strideA1 = grid->res[0] + 2;
  const int strideA2 = (grid->res[1] + 2) * (grid->res[0] + 2);

  const int num_cells = res[0] * res[1] * res[2];
  const int num_cellsA = (res[0] + 2) * (res[1] + 2) * (res[2] + 2);

  HairGridVert *vert_start = grid->verts - (stride0 + stride1 + stride2);
  HairGridVert *vert;
  int i, j, k;

#define MARGIN_i0 (i < 1)
#define MARGIN_j0 (j < 1)
#define MARGIN_k0 (k < 1)
#define MARGIN_i1 (i >= resA[0] - 1)
#define MARGIN_j1 (j >= resA[1] - 1)
#define MARGIN_k1 (k >= resA[2] - 1)

#define NEIGHBOR_MARGIN_i0 (i < 2)
#define NEIGHBOR_MARGIN_j0 (j < 2)
#define NEIGHBOR_MARGIN_k0 (k < 2)
#define NEIGHBOR_MARGIN_i1 (i >= resA[0] - 2)
#define NEIGHBOR_MARGIN_j1 (j >= resA[1] - 2)
#define NEIGHBOR_MARGIN_k1 (k >= resA[2] - 2)

  BLI_assert(num_cells >= 1);

  /* Calculate divergence */
  lVector B(num_cellsA);
  for (k = 0; k < resA[2]; ++k) {
    for (j = 0; j < resA[1]; ++j) {
      for (i = 0; i < resA[0]; ++i) {
        int u = i * strideA0 + j * strideA1 + k * strideA2;
        bool is_margin = MARGIN_i0 || MARGIN_i1 || MARGIN_j0 || MARGIN_j1 || MARGIN_k0 ||
                         MARGIN_k1;

        if (is_margin) {
          B[u] = 0.0f;
          continue;
        }

        vert = vert_start + i * stride0 + j * stride1 + k * stride2;

        const float *v0 = vert->velocity;
        float dx = 0.0f, dy = 0.0f, dz = 0.0f;
        if (!NEIGHBOR_MARGIN_i0)
          dx += v0[0] - (vert - stride0)->velocity[0];
        if (!NEIGHBOR_MARGIN_i1)
          dx += (vert + stride0)->velocity[0] - v0[0];
        if (!NEIGHBOR_MARGIN_j0)
          dy += v0[1] - (vert - stride1)->velocity[1];
        if (!NEIGHBOR_MARGIN_j1)
          dy += (vert + stride1)->velocity[1] - v0[1];
        if (!NEIGHBOR_MARGIN_k0)
          dz += v0[2] - (vert - stride2)->velocity[2];
        if (!NEIGHBOR_MARGIN_k1)
          dz += (vert + stride2)->velocity[2] - v0[2];

        float divergence = -0.5f * flowfac * (dx + dy + dz);

        /* adjustment term for target density */
        float target = hair_volume_density_divergence(
            vert->density, target_density, target_strength);

        /* B vector contains the finite difference approximation of the velocity divergence.
         * Note: according to the discretized Navier-Stokes equation the rhs vector
         * and resulting pressure gradient should be multiplied by the (inverse) density;
         * however, this is already included in the weighting of hair velocities on the grid!
         */
        B[u] = divergence - target;

#if 0
        {
          float wloc[3], loc[3];
          float col0[3] = {0.0, 0.0, 0.0};
          float colp[3] = {0.0, 1.0, 1.0};
          float coln[3] = {1.0, 0.0, 1.0};
          float col[3];
          float fac;

          loc[0] = (float)(i - 1);
          loc[1] = (float)(j - 1);
          loc[2] = (float)(k - 1);
          grid_to_world(grid, wloc, loc);

          if (divergence > 0.0f) {
            fac = CLAMPIS(divergence * target_strength, 0.0, 1.0);
            interp_v3_v3v3(col, col0, colp, fac);
          }
          else {
            fac = CLAMPIS(-divergence * target_strength, 0.0, 1.0);
            interp_v3_v3v3(col, col0, coln, fac);
          }
          if (fac > 0.05f)
            BKE_sim_debug_data_add_circle(
                grid->debug_data, wloc, 0.01f, col[0], col[1], col[2], "grid", 5522, i, j, k);
        }
#endif
      }
    }
  }

  /* Main Poisson equation system:
   * This is derived from the discretezation of the Poisson equation
   *   div(grad(p)) = div(v)
   *
   * The finite difference approximation yields the linear equation system described here:
   * https://en.wikipedia.org/wiki/Discrete_Poisson_equation
   */
  lMatrix A(num_cellsA, num_cellsA);
  /* Reserve space for the base equation system (without boundary conditions).
   * Each column contains a factor 6 on the diagonal
   * and up to 6 factors -1 on other places.
   */
  A.reserve(Eigen::VectorXi::Constant(num_cellsA, 7));

  for (k = 0; k < resA[2]; ++k) {
    for (j = 0; j < resA[1]; ++j) {
      for (i = 0; i < resA[0]; ++i) {
        int u = i * strideA0 + j * strideA1 + k * strideA2;
        bool is_margin = MARGIN_i0 || MARGIN_i1 || MARGIN_j0 || MARGIN_j1 || MARGIN_k0 ||
                         MARGIN_k1;

        vert = vert_start + i * stride0 + j * stride1 + k * stride2;
        if (!is_margin && vert->density > density_threshold) {
          int neighbors_lo = 0;
          int neighbors_hi = 0;
          int non_solid_neighbors = 0;
          int neighbor_lo_index[3];
          int neighbor_hi_index[3];
          int n;

          /* check for upper bounds in advance
           * to get the correct number of neighbors,
           * needed for the diagonal element
           */
          if (!NEIGHBOR_MARGIN_k0 && (vert - stride2)->density > density_threshold)
            neighbor_lo_index[neighbors_lo++] = u - strideA2;
          if (!NEIGHBOR_MARGIN_j0 && (vert - stride1)->density > density_threshold)
            neighbor_lo_index[neighbors_lo++] = u - strideA1;
          if (!NEIGHBOR_MARGIN_i0 && (vert - stride0)->density > density_threshold)
            neighbor_lo_index[neighbors_lo++] = u - strideA0;
          if (!NEIGHBOR_MARGIN_i1 && (vert + stride0)->density > density_threshold)
            neighbor_hi_index[neighbors_hi++] = u + strideA0;
          if (!NEIGHBOR_MARGIN_j1 && (vert + stride1)->density > density_threshold)
            neighbor_hi_index[neighbors_hi++] = u + strideA1;
          if (!NEIGHBOR_MARGIN_k1 && (vert + stride2)->density > density_threshold)
            neighbor_hi_index[neighbors_hi++] = u + strideA2;

          /*int liquid_neighbors = neighbors_lo + neighbors_hi;*/
          non_solid_neighbors = 6;

          for (n = 0; n < neighbors_lo; ++n)
            A.insert(neighbor_lo_index[n], u) = -1.0f;
          A.insert(u, u) = (float)non_solid_neighbors;
          for (n = 0; n < neighbors_hi; ++n)
            A.insert(neighbor_hi_index[n], u) = -1.0f;
        }
        else {
          A.insert(u, u) = 1.0f;
        }
      }
    }
  }

  ConjugateGradient cg;
  cg.setMaxIterations(100);
  cg.setTolerance(0.01f);

  cg.compute(A);

  lVector p = cg.solve(B);

  if (cg.info() == Eigen::Success) {
    /* Calculate velocity = grad(p) */
    for (k = 0; k < resA[2]; ++k) {
      for (j = 0; j < resA[1]; ++j) {
        for (i = 0; i < resA[0]; ++i) {
          int u = i * strideA0 + j * strideA1 + k * strideA2;
          bool is_margin = MARGIN_i0 || MARGIN_i1 || MARGIN_j0 || MARGIN_j1 || MARGIN_k0 ||
                           MARGIN_k1;
          if (is_margin)
            continue;

          vert = vert_start + i * stride0 + j * stride1 + k * stride2;
          if (vert->density > density_threshold) {
            float p_left = p[u - strideA0];
            float p_right = p[u + strideA0];
            float p_down = p[u - strideA1];
            float p_up = p[u + strideA1];
            float p_bottom = p[u - strideA2];
            float p_top = p[u + strideA2];

            /* finite difference estimate of pressure gradient */
            float dvel[3];
            dvel[0] = p_right - p_left;
            dvel[1] = p_up - p_down;
            dvel[2] = p_top - p_bottom;
            mul_v3_fl(dvel, -0.5f * inv_flowfac);

            /* pressure gradient describes velocity delta */
            add_v3_v3v3(vert->velocity_smooth, vert->velocity, dvel);
          }
          else {
            zero_v3(vert->velocity_smooth);
          }
        }
      }
    }

#if 0
    {
      int axis = 0;
      float offset = 0.0f;

      int slice = (offset - grid->gmin[axis]) / grid->cellsize;

      for (k = 0; k < resA[2]; ++k) {
        for (j = 0; j < resA[1]; ++j) {
          for (i = 0; i < resA[0]; ++i) {
            int u = i * strideA0 + j * strideA1 + k * strideA2;
            bool is_margin = MARGIN_i0 || MARGIN_i1 || MARGIN_j0 || MARGIN_j1 || MARGIN_k0 ||
                             MARGIN_k1;
            if (i != slice)
              continue;

            vert = vert_start + i * stride0 + j * stride1 + k * stride2;

            float wloc[3], loc[3];
            float col0[3] = {0.0, 0.0, 0.0};
            float colp[3] = {0.0, 1.0, 1.0};
            float coln[3] = {1.0, 0.0, 1.0};
            float col[3];
            float fac;

            loc[0] = (float)(i - 1);
            loc[1] = (float)(j - 1);
            loc[2] = (float)(k - 1);
            grid_to_world(grid, wloc, loc);

            float pressure = p[u];
            if (pressure > 0.0f) {
              fac = CLAMPIS(pressure * grid->debug1, 0.0, 1.0);
              interp_v3_v3v3(col, col0, colp, fac);
            }
            else {
              fac = CLAMPIS(-pressure * grid->debug1, 0.0, 1.0);
              interp_v3_v3v3(col, col0, coln, fac);
            }
            if (fac > 0.05f)
              BKE_sim_debug_data_add_circle(
                  grid->debug_data, wloc, 0.01f, col[0], col[1], col[2], "grid", 5533, i, j, k);

            if (!is_margin) {
              float dvel[3];
              sub_v3_v3v3(dvel, vert->velocity_smooth, vert->velocity);
              // BKE_sim_debug_data_add_vector(
              //     grid->debug_data, wloc, dvel, 1, 1, 1,
              //     "grid", 5566, i, j, k);
            }

            if (!is_margin) {
              float d = CLAMPIS(vert->density * grid->debug2, 0.0f, 1.0f);
              float col0[3] = {0.3, 0.3, 0.3};
              float colp[3] = {0.0, 0.0, 1.0};
              float col[3];

              interp_v3_v3v3(col, col0, colp, d);
              // if (d > 0.05f) {
              // BKE_sim_debug_data_add_dot(
              //     grid->debug_data, wloc, col[0], col[1], col[2],
              //     "grid", 5544, i, j, k);
              // }
            }
          }
        }
      }
    }
#endif

    return true;
  }
  else {
    /* Clear result in case of error */
    for (i = 0, vert = grid->verts; i < num_cells; ++i, ++vert) {
      zero_v3(vert->velocity_smooth);
    }

    return false;
  }
}

#if 0 /* XXX weighting is incorrect, disabled for now */
/* Velocity filter kernel
 * See https://en.wikipedia.org/wiki/Filter_%28large_eddy_simulation%29
 */

BLI_INLINE void hair_volume_filter_box_convolute(
    HairVertexGrid *grid, float invD, const int kernel_size[3], int i, int j, int k)
{
  int res = grid->res;
  int p, q, r;
  int minp = max_ii(i - kernel_size[0], 0), maxp = min_ii(i + kernel_size[0], res - 1);
  int minq = max_ii(j - kernel_size[1], 0), maxq = min_ii(j + kernel_size[1], res - 1);
  int minr = max_ii(k - kernel_size[2], 0), maxr = min_ii(k + kernel_size[2], res - 1);
  int offset, kernel_offset, kernel_dq, kernel_dr;
  HairGridVert *verts;
  float *vel_smooth;

  offset = i + (j + k * res) * res;
  verts = grid->verts;
  vel_smooth = verts[offset].velocity_smooth;

  kernel_offset = minp + (minq + minr * res) * res;
  kernel_dq = res;
  kernel_dr = res * res;
  for (r = minr; r <= maxr; ++r) {
    for (q = minq; q <= maxq; ++q) {
      for (p = minp; p <= maxp; ++p) {

        madd_v3_v3fl(vel_smooth, verts[kernel_offset].velocity, invD);

        kernel_offset += 1;
      }
      kernel_offset += kernel_dq;
    }
    kernel_offset += kernel_dr;
  }
}

void BPH_hair_volume_vertex_grid_filter_box(HairVertexGrid *grid, int kernel_size)
{
  int size = hair_grid_size(grid->res);
  int kernel_sizev[3] = {kernel_size, kernel_size, kernel_size};
  int tot;
  float invD;
  int i, j, k;

  if (kernel_size <= 0)
    return;

  tot = kernel_size * 2 + 1;
  invD = 1.0f / (float)(tot * tot * tot);

  /* clear values for convolution */
  for (i = 0; i < size; ++i) {
    zero_v3(grid->verts[i].velocity_smooth);
  }

  for (i = 0; i < grid->res; ++i) {
    for (j = 0; j < grid->res; ++j) {
      for (k = 0; k < grid->res; ++k) {
        hair_volume_filter_box_convolute(grid, invD, kernel_sizev, i, j, k);
      }
    }
  }

  /* apply as new velocity */
  for (i = 0; i < size; ++i) {
    copy_v3_v3(grid->verts[i].velocity, grid->verts[i].velocity_smooth);
  }
}
#endif

HairGrid *BPH_hair_volume_create_vertex_grid(float cellsize,
                                             const float gmin[3],
                                             const float gmax[3])
{
  float scale;
  float extent[3];
  int resmin[3], resmax[3], res[3];
  float gmin_margin[3], gmax_margin[3];
  int size;
  HairGrid *grid;
  int i;

  /* sanity check */
  if (cellsize <= 0.0f)
    cellsize = 1.0f;
  scale = 1.0f / cellsize;

  sub_v3_v3v3(extent, gmax, gmin);
  for (i = 0; i < 3; ++i) {
    resmin[i] = floor_int(gmin[i] * scale);
    resmax[i] = floor_int(gmax[i] * scale) + 1;

    /* add margin of 1 cell */
    resmin[i] -= 1;
    resmax[i] += 1;

    res[i] = resmax[i] - resmin[i] + 1;
    /* sanity check: avoid null-sized grid */
    if (res[i] < 4) {
      res[i] = 4;
      resmax[i] = resmin[i] + 4;
    }
    /* sanity check: avoid too large grid size */
    if (res[i] > MAX_HAIR_GRID_RES) {
      res[i] = MAX_HAIR_GRID_RES;
      resmax[i] = resmin[i] + MAX_HAIR_GRID_RES;
    }

    gmin_margin[i] = (float)resmin[i] * cellsize;
    gmax_margin[i] = (float)resmax[i] * cellsize;
  }
  size = hair_grid_size(res);

  grid = (HairGrid *)MEM_callocN(sizeof(HairGrid), "hair grid");
  grid->res[0] = res[0];
  grid->res[1] = res[1];
  grid->res[2] = res[2];
  copy_v3_v3(grid->gmin, gmin_margin);
  copy_v3_v3(grid->gmax, gmax_margin);
  grid->cellsize = cellsize;
  grid->inv_cellsize = scale;
  grid->verts = (HairGridVert *)MEM_callocN(sizeof(HairGridVert) * size, "hair voxel data");

  return grid;
}

void BPH_hair_volume_free_vertex_grid(HairGrid *grid)
{
  if (grid) {
    if (grid->verts)
      MEM_freeN(grid->verts);
    MEM_freeN(grid);
  }
}

void BPH_hair_volume_grid_geometry(
    HairGrid *grid, float *cellsize, int res[3], float gmin[3], float gmax[3])
{
  if (cellsize)
    *cellsize = grid->cellsize;
  if (res)
    copy_v3_v3_int(res, grid->res);
  if (gmin)
    copy_v3_v3(gmin, grid->gmin);
  if (gmax)
    copy_v3_v3(gmax, grid->gmax);
}

#if 0
static HairGridVert *hair_volume_create_collision_grid(ClothModifierData *clmd,
                                                       lfVector *lX,
                                                       unsigned int numverts)
{
  int res = hair_grid_res;
  int size = hair_grid_size(res);
  HairGridVert *collgrid;
  ListBase *colliders;
  ColliderCache *col = NULL;
  float gmin[3], gmax[3], scale[3];
  /* 2.0f is an experimental value that seems to give good results */
  float collfac = 2.0f * clmd->sim_parms->collider_friction;
  unsigned int v = 0;
  int i = 0;

  hair_volume_get_boundbox(lX, numverts, gmin, gmax);
  hair_grid_get_scale(res, gmin, gmax, scale);

  collgrid = MEM_mallocN(sizeof(HairGridVert) * size, "hair collider voxel data");

  /* initialize grid */
  for (i = 0; i < size; ++i) {
    zero_v3(collgrid[i].velocity);
    collgrid[i].density = 0.0f;
  }

  /* gather colliders */
  colliders = BKE_collider_cache_create(depsgraph, NULL, NULL);
  if (colliders && collfac > 0.0f) {
    for (col = colliders->first; col; col = col->next) {
      MVert *loc0 = col->collmd->x;
      MVert *loc1 = col->collmd->xnew;
      float vel[3];
      float weights[8];
      int di, dj, dk;

      for (v = 0; v < col->collmd->numverts; v++, loc0++, loc1++) {
        int offset;

        if (!hair_grid_point_valid(loc1->co, gmin, gmax))
          continue;

        offset = hair_grid_weights(res, gmin, scale, lX[v], weights);

        sub_v3_v3v3(vel, loc1->co, loc0->co);

        for (di = 0; di < 2; ++di) {
          for (dj = 0; dj < 2; ++dj) {
            for (dk = 0; dk < 2; ++dk) {
              int voffset = offset + di + (dj + dk * res) * res;
              int iw = di + dj * 2 + dk * 4;

              collgrid[voffset].density += weights[iw];
              madd_v3_v3fl(collgrid[voffset].velocity, vel, weights[iw]);
            }
          }
        }
      }
    }
  }
  BKE_collider_cache_free(&colliders);

  /* divide velocity with density */
  for (i = 0; i < size; i++) {
    float density = collgrid[i].density;
    if (density > 0.0f)
      mul_v3_fl(collgrid[i].velocity, 1.0f / density);
  }

  return collgrid;
}
#endif
