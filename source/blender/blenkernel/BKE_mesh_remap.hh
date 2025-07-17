/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_math_matrix.h"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"

#include "BKE_mesh_mapping.hh"

struct MemArena;
struct Mesh;

/* Generic ways to map some geometry elements from a source mesh to a destination one. */

struct MeshPairRemapItem {
  int sources_num;
  int *indices_src;   /* NULL if no source found. */
  float *weights_src; /* NULL if no source found, else, always normalized! */
  /* UNUSED (at the moment). */
  // float  hit_dist;     /* FLT_MAX if irrelevant or no source found. */
  int island; /* For loops only. */
};

/* All mapping computing func return this. */
struct MeshPairRemap {
  int items_num;
  MeshPairRemapItem *items; /* Array, one item per destination element. */

  MemArena *mem; /* memory arena, internal use only. */
};

/* Helpers! */
void BKE_mesh_remap_init(MeshPairRemap *map, int items_num);
void BKE_mesh_remap_free(MeshPairRemap *map);

void BKE_mesh_remap_item_define_invalid(MeshPairRemap *map, int index);

/**
 * Compute a value of the difference between both given meshes.
 * The smaller the result, the better the match.
 *
 * We return the inverse of the average of the inversed
 * shortest distance from each dst vertex to src ones.
 * In other words, beyond a certain (relatively small) distance, all differences have more or less
 * the same weight in final result, which allows to reduce influence of a few high differences,
 * in favor of a global good matching.
 */
float BKE_mesh_remap_calc_difference_from_mesh(const SpaceTransform *space_transform,
                                               blender::Span<blender::float3> vert_positions_dst,
                                               const Mesh *me_src);

/**
 * Set r_space_transform so that best bbox of dst matches best bbox of src.
 */
void BKE_mesh_remap_find_best_match_from_mesh(blender::Span<blender::float3> vert_positions_dst,
                                              const Mesh *me_src,
                                              SpaceTransform *r_space_transform);

void BKE_mesh_remap_calc_verts_from_mesh(int mode,
                                         const SpaceTransform *space_transform,
                                         float max_dist,
                                         float ray_radius,
                                         blender::Span<blender::float3> vert_positions_dst,
                                         const Mesh *me_src,
                                         Mesh *me_dst,
                                         MeshPairRemap *r_map);

void BKE_mesh_remap_calc_edges_from_mesh(int mode,
                                         const SpaceTransform *space_transform,
                                         float max_dist,
                                         float ray_radius,
                                         blender::Span<blender::float3> vert_positions_dst,
                                         blender::Span<blender::int2> edges_dst,
                                         const Mesh *me_src,
                                         Mesh *me_dst,
                                         MeshPairRemap *r_map);

void BKE_mesh_remap_calc_loops_from_mesh(int mode,
                                         const SpaceTransform *space_transform,
                                         float max_dist,
                                         float ray_radius,
                                         const Mesh *mesh_dst,
                                         blender::Span<blender::float3> vert_positions_dst,
                                         blender::Span<int> corner_verts_dst,
                                         const blender::OffsetIndices<int> faces_dst,
                                         const Mesh *me_src,
                                         MeshRemapIslandsCalc gen_islands_src,
                                         float islands_precision_src,
                                         MeshPairRemap *r_map);

void BKE_mesh_remap_calc_faces_from_mesh(int mode,
                                         const SpaceTransform *space_transform,
                                         float max_dist,
                                         float ray_radius,
                                         const Mesh *mesh_dst,
                                         blender::Span<blender::float3> vert_positions_dst,
                                         blender::Span<int> corner_verts,
                                         const blender::OffsetIndices<int> faces_dst,
                                         const Mesh *me_src,
                                         MeshPairRemap *r_map);
