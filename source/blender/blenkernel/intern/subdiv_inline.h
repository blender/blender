/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_assert.h"
#include "BLI_compiler_compat.h"

#include "BKE_subdiv.h"

BLI_INLINE void BKE_subdiv_ptex_face_uv_to_grid_uv(const float ptex_u,
                                                   const float ptex_v,
                                                   float *r_grid_u,
                                                   float *r_grid_v)
{
  *r_grid_u = 1.0f - ptex_v;
  *r_grid_v = 1.0f - ptex_u;
}

BLI_INLINE void BKE_subdiv_grid_uv_to_ptex_face_uv(const float grid_u,
                                                   const float grid_v,
                                                   float *r_ptex_u,
                                                   float *r_ptex_v)
{
  *r_ptex_u = 1.0f - grid_v;
  *r_ptex_v = 1.0f - grid_u;
}

BLI_INLINE int BKE_subdiv_grid_size_from_level(const int level)
{
  return (1 << (level - 1)) + 1;
}

BLI_INLINE int BKE_subdiv_rotate_quad_to_corner(const float quad_u,
                                                const float quad_v,
                                                float *r_corner_u,
                                                float *r_corner_v)
{
  int corner;
  if (quad_u <= 0.5f && quad_v <= 0.5f) {
    corner = 0;
    *r_corner_u = 2.0f * quad_u;
    *r_corner_v = 2.0f * quad_v;
  }
  else if (quad_u > 0.5f && quad_v <= 0.5f) {
    corner = 1;
    *r_corner_u = 2.0f * quad_v;
    *r_corner_v = 2.0f * (1.0f - quad_u);
  }
  else if (quad_u > 0.5f && quad_v > 0.5f) {
    corner = 2;
    *r_corner_u = 2.0f * (1.0f - quad_u);
    *r_corner_v = 2.0f * (1.0f - quad_v);
  }
  else {
    BLI_assert(quad_u <= 0.5f && quad_v >= 0.5f);
    corner = 3;
    *r_corner_u = 2.0f * (1.0f - quad_v);
    *r_corner_v = 2.0f * quad_u;
  }
  return corner;
}

BLI_INLINE void BKE_subdiv_rotate_grid_to_quad(
    const int corner, const float grid_u, const float grid_v, float *r_quad_u, float *r_quad_v)
{
  if (corner == 0) {
    *r_quad_u = 0.5f - grid_v * 0.5f;
    *r_quad_v = 0.5f - grid_u * 0.5f;
  }
  else if (corner == 1) {
    *r_quad_u = 0.5f + grid_u * 0.5f;
    *r_quad_v = 0.5f - grid_v * 0.5f;
  }
  else if (corner == 2) {
    *r_quad_u = 0.5f + grid_v * 0.5f;
    *r_quad_v = 0.5f + grid_u * 0.5f;
  }
  else {
    BLI_assert(corner == 3);
    *r_quad_u = 0.5f - grid_u * 0.5f;
    *r_quad_v = 0.5f + grid_v * 0.5f;
  }
}

BLI_INLINE float BKE_subdiv_crease_to_sharpness_f(float edge_crease)
{
  return edge_crease * edge_crease * 10.0f;
}
