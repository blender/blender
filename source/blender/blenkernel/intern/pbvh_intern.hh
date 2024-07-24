/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_pbvh.hh"

/** \file
 * \ingroup bke
 */

/* pbvh.cc */

namespace blender::bke::pbvh {

bool ray_face_intersection_quad(const float ray_start[3],
                                IsectRayPrecalc *isect_precalc,
                                const float t0[3],
                                const float t1[3],
                                const float t2[3],
                                const float t3[3],
                                float *depth);
bool ray_face_intersection_tri(const float ray_start[3],
                               IsectRayPrecalc *isect_precalc,
                               const float t0[3],
                               const float t1[3],
                               const float t2[3],
                               float *depth);

bool ray_face_nearest_quad(const float ray_start[3],
                           const float ray_normal[3],
                           const float t0[3],
                           const float t1[3],
                           const float t2[3],
                           const float t3[3],
                           float *r_depth,
                           float *r_dist_sq);
bool ray_face_nearest_tri(const float ray_start[3],
                          const float ray_normal[3],
                          const float t0[3],
                          const float t1[3],
                          const float t2[3],
                          float *r_depth,
                          float *r_dist_sq);

/* pbvh_bmesh.cc */

bool bmesh_node_raycast(blender::bke::pbvh::Node &node,
                        const float ray_start[3],
                        const float ray_normal[3],
                        IsectRayPrecalc *isect_precalc,
                        float *dist,
                        bool use_original,
                        PBVHVertRef *r_active_vertex,
                        float *r_face_normal);
bool bmesh_node_nearest_to_ray(blender::bke::pbvh::Node &node,
                               const float ray_start[3],
                               const float ray_normal[3],
                               float *depth,
                               float *dist_sq,
                               bool use_original);

void bmesh_normals_update(Span<blender::bke::pbvh::Node *> nodes);

/* pbvh_pixels.hh */

void node_pixels_free(blender::bke::pbvh::Node *node);
void pixels_free(blender::bke::pbvh::Tree *pbvh);
void free_draw_buffers(blender::bke::pbvh::Tree &pbvh, blender::bke::pbvh::Node *node);

}  // namespace blender::bke::pbvh
