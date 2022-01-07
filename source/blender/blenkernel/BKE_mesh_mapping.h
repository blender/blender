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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct MEdge;
struct MLoop;
struct MLoopTri;
struct MLoopUV;
struct MPoly;
struct MVert;

/* map from uv vertex to face (for select linked, stitch, uv suburf) */

/* UvVertMap */
#define STD_UV_CONNECT_LIMIT 0.0001f

typedef struct UvVertMap {
  struct UvMapVert **vert;
  struct UvMapVert *buf;
} UvVertMap;

typedef struct UvMapVert {
  struct UvMapVert *next;
  unsigned int poly_index;
  unsigned short loop_of_poly_index;
  bool separate;
} UvMapVert;

/* UvElement stores per uv information so that we can quickly access information for a uv.
 * it is actually an improved UvMapVert, including an island and a direct pointer to the face
 * to avoid initializing face arrays */
typedef struct UvElement {
  /* Next UvElement corresponding to same vertex */
  struct UvElement *next;
  /* Face the element belongs to */
  struct BMLoop *l;
  /* index in loop. */
  unsigned short loop_of_poly_index;
  /* Whether this element is the first of coincident elements */
  bool separate;
  /* general use flag */
  unsigned char flag;
  /* If generating element map with island sorting, this stores the island index */
  unsigned int island;
} UvElement;

/* UvElementMap is a container for UvElements of a mesh. It stores some UvElements belonging to the
 * same uv island in sequence and the number of uvs per island so it is possible to access all uvs
 * belonging to an island directly by iterating through the buffer.
 */
typedef struct UvElementMap {
  /* address UvElements by their vertex */
  struct UvElement **vert;
  /* UvElement Store */
  struct UvElement *buf;
  /* Total number of UVs in the layer. Useful to know */
  int totalUVs;
  /* Number of Islands in the mesh */
  int totalIslands;
  /* Stores the starting index in buf where each island begins */
  int *islandIndices;
} UvElementMap;

#define INVALID_ISLAND ((unsigned int)-1)

/* Connectivity data */
typedef struct MeshElemMap {
  int *indices;
  int count;
} MeshElemMap;

/* mapping */
UvVertMap *BKE_mesh_uv_vert_map_create(const struct MPoly *mpoly,
                                       const struct MLoop *mloop,
                                       const struct MLoopUV *mloopuv,
                                       unsigned int totpoly,
                                       unsigned int totvert,
                                       const float limit[2],
                                       bool selected,
                                       bool use_winding);
UvMapVert *BKE_mesh_uv_vert_map_get_vert(UvVertMap *vmap, unsigned int v);
void BKE_mesh_uv_vert_map_free(UvVertMap *vmap);

/**
 * Generates a map where the key is the vertex and the value
 * is a list of polys that use that vertex as a corner.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_vert_poly_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const struct MPoly *mpoly,
                                   const struct MLoop *mloop,
                                   int totvert,
                                   int totpoly,
                                   int totloop);
/**
 * Generates a map where the key is the vertex and the value
 * is a list of loops that use that vertex as a corner.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_vert_loop_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const struct MPoly *mpoly,
                                   const struct MLoop *mloop,
                                   int totvert,
                                   int totpoly,
                                   int totloop);
/**
 * Generates a map where the key is the edge and the value
 * is a list of looptris that use that edge.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_vert_looptri_map_create(MeshElemMap **r_map,
                                      int **r_mem,
                                      const struct MVert *mvert,
                                      int totvert,
                                      const struct MLoopTri *mlooptri,
                                      int totlooptri,
                                      const struct MLoop *mloop,
                                      int totloop);
/**
 * Generates a map where the key is the vertex and the value
 * is a list of edges that use that vertex as an endpoint.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_vert_edge_map_create(
    MeshElemMap **r_map, int **r_mem, const struct MEdge *medge, int totvert, int totedge);
/**
 * A version of #BKE_mesh_vert_edge_map_create that references connected vertices directly
 * (not their edges).
 */
void BKE_mesh_vert_edge_vert_map_create(
    MeshElemMap **r_map, int **r_mem, const struct MEdge *medge, int totvert, int totedge);
/**
 * Generates a map where the key is the edge and the value is a list of loops that use that edge.
 * Loops indices of a same poly are contiguous and in winding order.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_edge_loop_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const struct MEdge *medge,
                                   int totedge,
                                   const struct MPoly *mpoly,
                                   int totpoly,
                                   const struct MLoop *mloop,
                                   int totloop);
/**
 * Generates a map where the key is the edge and the value
 * is a list of polygons that use that edge.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_edge_poly_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const struct MEdge *medge,
                                   int totedge,
                                   const struct MPoly *mpoly,
                                   int totpoly,
                                   const struct MLoop *mloop,
                                   int totloop);
/**
 * This function creates a map so the source-data (vert/edge/loop/poly)
 * can loop over the destination data (using the destination arrays origindex).
 *
 * This has the advantage that it can operate on any data-types.
 *
 * \param totsource: The total number of elements that \a final_origindex points to.
 * \param totfinal: The size of \a final_origindex
 * \param final_origindex: The size of the final array.
 *
 * \note `totsource` could be `totpoly`,
 *       `totfinal` could be `tottessface` and `final_origindex` its ORIGINDEX custom-data.
 *       This would allow an MPoly to loop over its tessfaces.
 */
void BKE_mesh_origindex_map_create(
    MeshElemMap **r_map, int **r_mem, int totsource, const int *final_origindex, int totfinal);
/**
 * A version of #BKE_mesh_origindex_map_create that takes a looptri array.
 * Making a poly -> looptri map.
 */
void BKE_mesh_origindex_map_create_looptri(MeshElemMap **r_map,
                                           int **r_mem,
                                           const struct MPoly *mpoly,
                                           int mpoly_num,
                                           const struct MLoopTri *looptri,
                                           int looptri_num);

/* islands */

/* Loop islands data helpers. */
enum {
  MISLAND_TYPE_NONE = 0,
  MISLAND_TYPE_VERT = 1,
  MISLAND_TYPE_EDGE = 2,
  MISLAND_TYPE_POLY = 3,
  MISLAND_TYPE_LOOP = 4,
};

typedef struct MeshIslandStore {
  short item_type;     /* MISLAND_TYPE_... */
  short island_type;   /* MISLAND_TYPE_... */
  short innercut_type; /* MISLAND_TYPE_... */

  int items_to_islands_num;
  int *items_to_islands; /* map the item to the island index */

  int islands_num;
  size_t islands_num_alloc;
  struct MeshElemMap **islands;   /* Array of pointers, one item per island. */
  struct MeshElemMap **innercuts; /* Array of pointers, one item per island. */

  struct MemArena *mem; /* Memory arena, internal use only. */
} MeshIslandStore;

void BKE_mesh_loop_islands_init(MeshIslandStore *island_store,
                                short item_type,
                                int items_num,
                                short island_type,
                                short innercut_type);
void BKE_mesh_loop_islands_clear(MeshIslandStore *island_store);
void BKE_mesh_loop_islands_free(MeshIslandStore *island_store);
void BKE_mesh_loop_islands_add(MeshIslandStore *island_store,
                               int item_num,
                               const int *items_indices,
                               int num_island_items,
                               int *island_item_indices,
                               int num_innercut_items,
                               int *innercut_item_indices);

typedef bool (*MeshRemapIslandsCalc)(struct MVert *verts,
                                     int totvert,
                                     struct MEdge *edges,
                                     int totedge,
                                     struct MPoly *polys,
                                     int totpoly,
                                     struct MLoop *loops,
                                     int totloop,
                                     struct MeshIslandStore *r_island_store);

/* Above vert/UV mapping stuff does not do what we need here, but does things we do not need here.
 * So better keep them separated for now, I think. */

/**
 * Calculate 'generic' UV islands, i.e. based only on actual geometry data (edge seams),
 * not some UV layers coordinates.
 */
bool BKE_mesh_calc_islands_loop_poly_edgeseam(struct MVert *verts,
                                              int totvert,
                                              struct MEdge *edges,
                                              int totedge,
                                              struct MPoly *polys,
                                              int totpoly,
                                              struct MLoop *loops,
                                              int totloop,
                                              MeshIslandStore *r_island_store);

/**
 * Calculate UV islands.
 *
 * \note If no MLoopUV layer is passed, we only consider edges tagged as seams as UV boundaries.
 * This has the advantages of simplicity, and being valid/common to all UV maps.
 * However, it means actual UV islands without matching UV seams will not be handled correctly.
 * If a valid UV layer is passed as \a luvs parameter,
 * UV coordinates are also used to detect islands boundaries.
 *
 * \note All this could be optimized.
 * Not sure it would be worth the more complex code, though,
 * those loops are supposed to be really quick to do.
 */
bool BKE_mesh_calc_islands_loop_poly_uvmap(struct MVert *verts,
                                           int totvert,
                                           struct MEdge *edges,
                                           int totedge,
                                           struct MPoly *polys,
                                           int totpoly,
                                           struct MLoop *loops,
                                           int totloop,
                                           const struct MLoopUV *luvs,
                                           MeshIslandStore *r_island_store);

/**
 * Calculate smooth groups from sharp edges.
 *
 * \param r_totgroup: The total number of groups, 1 or more.
 * \return Polygon aligned array of group index values (bitflags if use_bitflags is true),
 * starting at 1 (0 being used as 'invalid' flag).
 * Note it's callers's responsibility to MEM_freeN returned array.
 */
int *BKE_mesh_calc_smoothgroups(const struct MEdge *medge,
                                int totedge,
                                const struct MPoly *mpoly,
                                int totpoly,
                                const struct MLoop *mloop,
                                int totloop,
                                int *r_totgroup,
                                bool use_bitflags);

/* use on looptri vertex values */
#define BKE_MESH_TESSTRI_VINDEX_ORDER(_tri, _v) \
  ((CHECK_TYPE_ANY( \
        _tri, unsigned int *, int *, int[3], const unsigned int *, const int *, const int[3]), \
    CHECK_TYPE_ANY(_v, unsigned int, const unsigned int, int, const int)), \
   (((_tri)[0] == _v) ? 0 : \
    ((_tri)[1] == _v) ? 1 : \
    ((_tri)[2] == _v) ? 2 : \
                        -1))

#ifdef __cplusplus
}
#endif
