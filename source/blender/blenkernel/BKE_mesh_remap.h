/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
#  include "BLI_offset_indices.hh"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct CustomData;
struct CustomData_MeshMasks;
struct MemArena;
struct Mesh;

/* Generic ways to map some geometry elements from a source mesh to a destination one. */

typedef struct MeshPairRemapItem {
  int sources_num;
  int *indices_src;   /* NULL if no source found. */
  float *weights_src; /* NULL if no source found, else, always normalized! */
  /* UNUSED (at the moment). */
  // float  hit_dist;     /* FLT_MAX if irrelevant or no source found. */
  int island; /* For loops only. */
} MeshPairRemapItem;

/* All mapping computing func return this. */
typedef struct MeshPairRemap {
  int items_num;
  MeshPairRemapItem *items; /* Array, one item per destination element. */

  struct MemArena *mem; /* memory arena, internal use only. */
} MeshPairRemap;

/* Helpers! */
void BKE_mesh_remap_init(MeshPairRemap *map, int items_num);
void BKE_mesh_remap_free(MeshPairRemap *map);

void BKE_mesh_remap_item_define_invalid(MeshPairRemap *map, int index);

/* TODO:
 * Add other 'from/to' mapping sources, like e.g. using a UVMap, etc.
 * https://blenderartists.org/t/619105
 *
 * We could also use similar topology mappings inside a same mesh
 * (cf. Campbell's 'select face islands from similar topology' WIP work).
 * Also, users will have to check, whether we can get rid of some modes here,
 * not sure all will be useful!
 */
enum {
  MREMAP_USE_VERT = 1 << 4,
  MREMAP_USE_EDGE = 1 << 5,
  MREMAP_USE_LOOP = 1 << 6,
  MREMAP_USE_POLY = 1 << 7,

  MREMAP_USE_NEAREST = 1 << 8,
  MREMAP_USE_NORPROJ = 1 << 9,
  MREMAP_USE_INTERP = 1 << 10,
  MREMAP_USE_NORMAL = 1 << 11,

  /* ***** Target's vertices ***** */
  MREMAP_MODE_VERT = 1 << 24,
  /* Nearest source vert. */
  MREMAP_MODE_VERT_NEAREST = MREMAP_MODE_VERT | MREMAP_USE_VERT | MREMAP_USE_NEAREST,

  /* Nearest vertex of nearest edge. */
  MREMAP_MODE_VERT_EDGE_NEAREST = MREMAP_MODE_VERT | MREMAP_USE_EDGE | MREMAP_USE_NEAREST,
  /* This one uses two verts of selected edge (weighted interpolation). */
  /* Nearest point on nearest edge. */
  MREMAP_MODE_VERT_EDGEINTERP_NEAREST = MREMAP_MODE_VERT | MREMAP_USE_EDGE | MREMAP_USE_NEAREST |
                                        MREMAP_USE_INTERP,

  /* Nearest vertex of nearest poly. */
  MREMAP_MODE_VERT_POLY_NEAREST = MREMAP_MODE_VERT | MREMAP_USE_POLY | MREMAP_USE_NEAREST,
  /* Those two use all verts of selected poly (weighted interpolation). */
  /* Nearest point on nearest poly. */
  MREMAP_MODE_VERT_POLYINTERP_NEAREST = MREMAP_MODE_VERT | MREMAP_USE_POLY | MREMAP_USE_NEAREST |
                                        MREMAP_USE_INTERP,
  /* Point on nearest face hit by ray from target vertex's normal. */
  MREMAP_MODE_VERT_POLYINTERP_VNORPROJ = MREMAP_MODE_VERT | MREMAP_USE_POLY | MREMAP_USE_NORPROJ |
                                         MREMAP_USE_INTERP,

  /* ***** Target's edges ***** */
  MREMAP_MODE_EDGE = 1 << 25,

  /* Source edge which both vertices are nearest of destination ones. */
  MREMAP_MODE_EDGE_VERT_NEAREST = MREMAP_MODE_EDGE | MREMAP_USE_VERT | MREMAP_USE_NEAREST,

  /* Nearest source edge (using mid-point). */
  MREMAP_MODE_EDGE_NEAREST = MREMAP_MODE_EDGE | MREMAP_USE_EDGE | MREMAP_USE_NEAREST,

  /* Nearest edge of nearest poly (using mid-point). */
  MREMAP_MODE_EDGE_POLY_NEAREST = MREMAP_MODE_EDGE | MREMAP_USE_POLY | MREMAP_USE_NEAREST,

  /* Cast a set of rays from along destination edge,
   * interpolating its vertices' normals, and use hit source edges. */
  MREMAP_MODE_EDGE_EDGEINTERP_VNORPROJ = MREMAP_MODE_EDGE | MREMAP_USE_VERT | MREMAP_USE_NORPROJ |
                                         MREMAP_USE_INTERP,

  /* ***** Target's loops ***** */
  /* NOTE: when islands are given to loop mapping func,
   * all loops from the same destination face will always be mapped
   * to loops of source faces within a same island, regardless of mapping mode. */
  MREMAP_MODE_LOOP = 1 << 26,

  /* Best normal-matching loop from nearest vert. */
  MREMAP_MODE_LOOP_NEAREST_LOOPNOR = MREMAP_MODE_LOOP | MREMAP_USE_LOOP | MREMAP_USE_VERT |
                                     MREMAP_USE_NEAREST | MREMAP_USE_NORMAL,
  /* Loop from best normal-matching poly from nearest vert. */
  MREMAP_MODE_LOOP_NEAREST_POLYNOR = MREMAP_MODE_LOOP | MREMAP_USE_POLY | MREMAP_USE_VERT |
                                     MREMAP_USE_NEAREST | MREMAP_USE_NORMAL,

  /* Loop from nearest vertex of nearest poly. */
  MREMAP_MODE_LOOP_POLY_NEAREST = MREMAP_MODE_LOOP | MREMAP_USE_POLY | MREMAP_USE_NEAREST,
  /* Those two use all verts of selected poly (weighted interpolation). */
  /* Nearest point on nearest poly. */
  MREMAP_MODE_LOOP_POLYINTERP_NEAREST = MREMAP_MODE_LOOP | MREMAP_USE_POLY | MREMAP_USE_NEAREST |
                                        MREMAP_USE_INTERP,
  /* Point on nearest face hit by ray from target loop's normal. */
  MREMAP_MODE_LOOP_POLYINTERP_LNORPROJ = MREMAP_MODE_LOOP | MREMAP_USE_POLY | MREMAP_USE_NORPROJ |
                                         MREMAP_USE_INTERP,

  /* ***** Target's polygons ***** */
  MREMAP_MODE_POLY = 1 << 27,

  /* Nearest source poly. */
  MREMAP_MODE_POLY_NEAREST = MREMAP_MODE_POLY | MREMAP_USE_POLY | MREMAP_USE_NEAREST,
  /* Source poly from best normal-matching destination poly. */
  MREMAP_MODE_POLY_NOR = MREMAP_MODE_POLY | MREMAP_USE_POLY | MREMAP_USE_NORMAL,

  /* Project destination poly onto source mesh using its normal,
   * and use interpolation of all intersecting source polys. */
  MREMAP_MODE_POLY_POLYINTERP_PNORPROJ = MREMAP_MODE_POLY | MREMAP_USE_POLY | MREMAP_USE_NORPROJ |
                                         MREMAP_USE_INTERP,

  /* ***** Same topology, applies to all four elements types. ***** */
  MREMAP_MODE_TOPOLOGY = MREMAP_MODE_VERT | MREMAP_MODE_EDGE | MREMAP_MODE_LOOP | MREMAP_MODE_POLY,
};

void BKE_mesh_remap_calc_source_cddata_masks_from_map_modes(
    int vert_mode,
    int edge_mode,
    int loop_mode,
    int poly_mode,
    struct CustomData_MeshMasks *cddata_mask);

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
float BKE_mesh_remap_calc_difference_from_mesh(const struct SpaceTransform *space_transform,
                                               const float (*vert_positions_dst)[3],
                                               int numverts_dst,
                                               const struct Mesh *me_src);

/**
 * Set r_space_transform so that best bbox of dst matches best bbox of src.
 */
void BKE_mesh_remap_find_best_match_from_mesh(const float (*vert_positions_dst)[3],
                                              int numverts_dst,
                                              const struct Mesh *me_src,
                                              struct SpaceTransform *r_space_transform);

void BKE_mesh_remap_calc_verts_from_mesh(int mode,
                                         const struct SpaceTransform *space_transform,
                                         float max_dist,
                                         float ray_radius,
                                         const float (*vert_positions_dst)[3],
                                         int numverts_dst,
                                         bool dirty_nors_dst,
                                         const struct Mesh *me_src,
                                         struct Mesh *me_dst,
                                         MeshPairRemap *r_map);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

void BKE_mesh_remap_calc_edges_from_mesh(int mode,
                                         const struct SpaceTransform *space_transform,
                                         float max_dist,
                                         float ray_radius,
                                         const float (*vert_positions_dst)[3],
                                         int numverts_dst,
                                         const blender::int2 *edges_dst,
                                         int numedges_dst,
                                         bool dirty_nors_dst,
                                         const struct Mesh *me_src,
                                         struct Mesh *me_dst,
                                         MeshPairRemap *r_map);

void BKE_mesh_remap_calc_loops_from_mesh(int mode,
                                         const struct SpaceTransform *space_transform,
                                         float max_dist,
                                         float ray_radius,
                                         struct Mesh *mesh_dst,
                                         const float (*vert_positions_dst)[3],
                                         int numverts_dst,
                                         const blender::int2 *edges_dst,
                                         int numedges_dst,
                                         const int *corner_verts_dst,
                                         const int *corner_edges_dst,
                                         int numloops_dst,
                                         const blender::OffsetIndices<int> polys_dst,
                                         struct CustomData *ldata_dst,
                                         bool use_split_nors_dst,
                                         float split_angle_dst,
                                         bool dirty_nors_dst,
                                         const struct Mesh *me_src,
                                         MeshRemapIslandsCalc gen_islands_src,
                                         float islands_precision_src,
                                         struct MeshPairRemap *r_map);

void BKE_mesh_remap_calc_polys_from_mesh(int mode,
                                         const struct SpaceTransform *space_transform,
                                         float max_dist,
                                         float ray_radius,
                                         const struct Mesh *mesh_dst,
                                         const float (*vert_positions_dst)[3],
                                         int numverts_dst,
                                         const int *corner_verts,
                                         const blender::OffsetIndices<int> polys_dst,
                                         const struct Mesh *me_src,
                                         struct MeshPairRemap *r_map);

#endif
