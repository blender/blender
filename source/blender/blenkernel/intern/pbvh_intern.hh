/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_paint_bvh.hh"

/** \file
 * \ingroup bke
 */

/* pbvh.cc */

namespace blender::bke::pbvh {

bool ray_face_intersection_quad(const float3 &ray_start,
                                const IsectRayPrecalc *isect_precalc,
                                const float3 &t0,
                                const float3 &t1,
                                const float3 &t2,
                                const float3 &t3,
                                float *depth);
bool ray_face_intersection_tri(const float3 &ray_start,
                               const IsectRayPrecalc *isect_precalc,
                               const float3 &t0,
                               const float3 &t1,
                               const float3 &t2,
                               float *depth);

bool ray_face_nearest_quad(const float3 &ray_start,
                           const float3 &ray_normal,
                           const float3 &t0,
                           const float3 &t1,
                           const float3 &t2,
                           const float3 &t3,
                           float *r_depth,
                           float *r_dist_sq);
bool ray_face_nearest_tri(const float3 &ray_start,
                          const float3 &ray_normal,
                          const float3 &t0,
                          const float3 &t1,
                          const float3 &t2,
                          float *r_depth,
                          float *r_dist_sq);

/* pbvh_bmesh.cc */

bool bmesh_node_nearest_to_ray(blender::bke::pbvh::BMeshNode &node,
                               const float3 &ray_start,
                               const float3 &ray_normal,
                               float *r_depth,
                               float *dist_sq,
                               bool use_original);

void bmesh_normals_update(Tree &pbvh, const IndexMask &nodes_to_update);

/* pbvh_pixels.hh */

void node_pixels_free(blender::bke::pbvh::Node *node);
void pixels_free(blender::bke::pbvh::Tree *pbvh);

}  // namespace blender::bke::pbvh
