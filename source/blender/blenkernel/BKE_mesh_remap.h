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
 */

#ifndef __BKE_MESH_REMAP_H__
#define __BKE_MESH_REMAP_H__

/** \file
 * \ingroup bke
 */

struct CustomData;
struct CustomData_MeshMasks;
struct MVert;
struct MemArena;
struct Mesh;

/* Generic ways to map some geometry elements from a source mesh to a dest one. */

typedef struct MeshPairRemapItem {
  int sources_num;
  int *indices_src;   /* NULL if no source found. */
  float *weights_src; /* NULL if no source found, else, always normalized! */
  /* UNUSED (at the moment)*/
  // float  hit_dist;     /* FLT_MAX if irrelevant or no source found. */
  int island; /* For loops only. */
} MeshPairRemapItem;

/* All mapping computing func return this. */
typedef struct MeshPairRemap {
  int items_num;
  MeshPairRemapItem *items; /* array, one item per dest element. */

  struct MemArena *mem; /* memory arena, internal use only. */
} MeshPairRemap;

/* Helpers! */
void BKE_mesh_remap_init(MeshPairRemap *map, const int items_num);
void BKE_mesh_remap_free(MeshPairRemap *map);

void BKE_mesh_remap_item_define_invalid(MeshPairRemap *map, const int index);

/* TODO:
 * Add other 'from/to' mapping sources, like e.g. using an UVMap, etc.
 * https://blenderartists.org/t/619105
 *
 * We could also use similar topology mappings inside a same mesh
 * (cf. Campbell's 'select face islands from similar topology' wip work).
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

  /* Source edge which both vertices are nearest of dest ones. */
  MREMAP_MODE_EDGE_VERT_NEAREST = MREMAP_MODE_EDGE | MREMAP_USE_VERT | MREMAP_USE_NEAREST,

  /* Nearest source edge (using mid-point). */
  MREMAP_MODE_EDGE_NEAREST = MREMAP_MODE_EDGE | MREMAP_USE_EDGE | MREMAP_USE_NEAREST,

  /* Nearest edge of nearest poly (using mid-point). */
  MREMAP_MODE_EDGE_POLY_NEAREST = MREMAP_MODE_EDGE | MREMAP_USE_POLY | MREMAP_USE_NEAREST,

  /* Cast a set of rays from along dest edge,
   * interpolating its vertices' normals, and use hit source edges. */
  MREMAP_MODE_EDGE_EDGEINTERP_VNORPROJ = MREMAP_MODE_EDGE | MREMAP_USE_VERT | MREMAP_USE_NORPROJ |
                                         MREMAP_USE_INTERP,

  /* ***** Target's loops ***** */
  /* Note: when islands are given to loop mapping func,
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
  /* Source poly from best normal-matching dest poly. */
  MREMAP_MODE_POLY_NOR = MREMAP_MODE_POLY | MREMAP_USE_POLY | MREMAP_USE_NORMAL,

  /* Project dest poly onto source mesh using its normal,
   * and use interpolation of all intersecting source polys. */
  MREMAP_MODE_POLY_POLYINTERP_PNORPROJ = MREMAP_MODE_POLY | MREMAP_USE_POLY | MREMAP_USE_NORPROJ |
                                         MREMAP_USE_INTERP,

  /* ***** Same topology, applies to all four elements types. ***** */
  MREMAP_MODE_TOPOLOGY = MREMAP_MODE_VERT | MREMAP_MODE_EDGE | MREMAP_MODE_LOOP | MREMAP_MODE_POLY,
};

void BKE_mesh_remap_calc_source_cddata_masks_from_map_modes(
    const int vert_mode,
    const int edge_mode,
    const int loop_mode,
    const int poly_mode,
    struct CustomData_MeshMasks *cddata_mask);

float BKE_mesh_remap_calc_difference_from_mesh(const struct SpaceTransform *space_transform,
                                               const struct MVert *verts_dst,
                                               const int numverts_dst,
                                               struct Mesh *me_src);

void BKE_mesh_remap_find_best_match_from_mesh(const struct MVert *verts_dst,
                                              const int numverts_dst,
                                              struct Mesh *me_src,
                                              struct SpaceTransform *r_space_transform);

void BKE_mesh_remap_calc_verts_from_mesh(const int mode,
                                         const struct SpaceTransform *space_transform,
                                         const float max_dist,
                                         const float ray_radius,
                                         const struct MVert *verts_dst,
                                         const int numverts_dst,
                                         const bool dirty_nors_dst,
                                         struct Mesh *me_src,
                                         MeshPairRemap *r_map);

void BKE_mesh_remap_calc_edges_from_mesh(const int mode,
                                         const struct SpaceTransform *space_transform,
                                         const float max_dist,
                                         const float ray_radius,
                                         const struct MVert *verts_dst,
                                         const int numverts_dst,
                                         const struct MEdge *edges_dst,
                                         const int numedges_dst,
                                         const bool dirty_nors_dst,
                                         struct Mesh *me_src,
                                         MeshPairRemap *r_map);

void BKE_mesh_remap_calc_loops_from_mesh(const int mode,
                                         const struct SpaceTransform *space_transform,
                                         const float max_dist,
                                         const float ray_radius,
                                         struct MVert *verts_dst,
                                         const int numverts_dst,
                                         struct MEdge *edges_dst,
                                         const int numedges_dst,
                                         struct MLoop *loops_dst,
                                         const int numloops_dst,
                                         struct MPoly *polys_dst,
                                         const int numpolys_dst,
                                         struct CustomData *ldata_dst,
                                         struct CustomData *pdata_dst,
                                         const bool use_split_nors_dst,
                                         const float split_angle_dst,
                                         const bool dirty_nors_dst,
                                         struct Mesh *me_src,
                                         MeshRemapIslandsCalc gen_islands_src,
                                         const float islands_precision_src,
                                         struct MeshPairRemap *r_map);

void BKE_mesh_remap_calc_polys_from_mesh(const int mode,
                                         const struct SpaceTransform *space_transform,
                                         const float max_dist,
                                         const float ray_radius,
                                         struct MVert *verts_dst,
                                         const int numverts_dst,
                                         struct MLoop *loops_dst,
                                         const int numloops_dst,
                                         struct MPoly *polys_dst,
                                         const int numpolys_dst,
                                         struct CustomData *pdata_dst,
                                         const bool dirty_nors_dst,
                                         struct Mesh *me_src,
                                         struct MeshPairRemap *r_map);

#endif /* __BKE_MESH_REMAP_H__ */
