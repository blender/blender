/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"

struct BMLoop;
struct MemArena;

/* UvVertMap */
#define STD_UV_CONNECT_LIMIT 0.0001f

struct UvMapVert {
  UvMapVert *next;
  unsigned int face_index;
  unsigned short loop_of_face_index;
  bool separate;
};

/** Map from UV vertex to face. Used by select linked, UV subdivision-surface and obj exporter. */
struct UvVertMap {
  UvMapVert **vert;
  UvMapVert *buf;
};

/**
 * UvElement stores per uv information so that we can quickly access information for a uv.
 * it is actually an improved UvMapVert, including an island and a direct pointer to the face
 * to avoid initializing face arrays.
 */
struct UvElement {
  /** Next UvElement corresponding to same vertex */
  UvElement *next;
  /** Face the element belongs to */
  BMLoop *l;
  /** Index in loop. */
  unsigned short loop_of_face_index;
  /** Whether this element is the first of coincident elements */
  bool separate;
  /** general use flag. */
  unsigned char flag;
  /** If generating element map with island sorting, this stores the island index */
  unsigned int island;
};

/**
 * UvElementMap is a container for UvElements of a BMesh.
 *
 * It simplifies access to UV information and ensures the
 * different UV selection modes are respected.
 *
 * If islands are calculated, it also stores UvElements
 * belonging to the same uv island in sequence and
 * the number of uvs per island.
 *
 * \note in C++, #head_table and #unique_index_table would
 * be `mutable`, as they are created on demand, and never
 * changed after creation.
 */
struct UvElementMap {
  /** UvElement Storage. */
  UvElement *storage;
  /** Total number of UVs. */
  int total_uvs;
  /** Total number of unique UVs. */
  int total_unique_uvs;

  /** If Non-NULL, address UvElements by `BM_elem_index_get(BMVert*)`. */
  UvElement **vertex;

  /** If Non-NULL, pointer to local head of each unique UV. */
  UvElement **head_table;

  /** If Non-NULL, pointer to index of each unique UV. */
  int *unique_index_table;

  /** Number of islands, or zero if not calculated. */
  int total_islands;
  /** Array of starting index in #storage where each island begins. */
  int *island_indices;
  /** Array of number of UVs in each island. */
  int *island_total_uvs;
  /** Array of number of unique UVs in each island. */
  int *island_total_unique_uvs;
};

/* Connectivity data */
struct MeshElemMap {
  int *indices;
  int count;
};

/* mapping */

UvVertMap *BKE_mesh_uv_vert_map_create(blender::OffsetIndices<int> faces,
                                       const bool *hide_poly,
                                       const bool *select_poly,
                                       const int *corner_verts,
                                       const float (*mloopuv)[2],
                                       unsigned int totvert,
                                       const float limit[2],
                                       bool selected,
                                       bool use_winding);

UvMapVert *BKE_mesh_uv_vert_map_get_vert(UvVertMap *vmap, unsigned int v);
void BKE_mesh_uv_vert_map_free(UvVertMap *vmap);

/**
 * Generates a map where the key is the edge and the value
 * is a list of corner_tris that use that edge.
 * The lists are allocated from one memory pool.
 */
void BKE_mesh_vert_corner_tri_map_create(MeshElemMap **r_map,
                                         int **r_mem,
                                         int totvert,
                                         const blender::int3 *corner_tris,
                                         int tris_num,
                                         const int *corner_verts,
                                         int corners_num);
/**
 * This function creates a map so the source-data (vert/edge/loop/face)
 * can loop over the destination data (using the destination arrays origindex).
 *
 * This has the advantage that it can operate on any data-types.
 *
 * \param totsource: The total number of elements that \a final_origindex points to.
 * \param totfinal: The size of \a final_origindex
 * \param final_origindex: The size of the final array.
 *
 * \note `totsource` could be `faces_num`,
 *       `totfinal` could be `tottessface` and `final_origindex` its ORIGINDEX custom-data.
 *       This would allow a face to loop over its tessfaces.
 */
void BKE_mesh_origindex_map_create(
    MeshElemMap **r_map, int **r_mem, int totsource, const int *final_origindex, int totfinal);
/**
 * A version of #BKE_mesh_origindex_map_create that takes a corner tri array.
 * Making a face -> corner tri map.
 */
void BKE_mesh_origindex_map_create_corner_tri(MeshElemMap **r_map,
                                              int **r_mem,
                                              blender::OffsetIndices<int> faces,
                                              const int *corner_tri_faces,
                                              int corner_tris_num);

/* islands */

/* Loop islands data helpers. */
enum {
  MISLAND_TYPE_NONE = 0,
  MISLAND_TYPE_VERT = 1,
  MISLAND_TYPE_EDGE = 2,
  MISLAND_TYPE_POLY = 3,
  MISLAND_TYPE_LOOP = 4,
};

struct MeshIslandStore {
  short item_type;     /* MISLAND_TYPE_... */
  short island_type;   /* MISLAND_TYPE_... */
  short innercut_type; /* MISLAND_TYPE_... */

  int items_to_islands_num;
  int *items_to_islands; /* map the item to the island index */

  int islands_num;
  size_t islands_num_alloc;
  MeshElemMap **islands;   /* Array of pointers, one item per island. */
  MeshElemMap **innercuts; /* Array of pointers, one item per island. */

  MemArena *mem; /* Memory arena, internal use only. */
};

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

using MeshRemapIslandsCalc = bool (*)(const float (*vert_positions)[3],
                                      int totvert,
                                      const blender::int2 *edges,
                                      int totedge,
                                      const bool *uv_seams,
                                      blender::OffsetIndices<int> faces,
                                      const int *corner_verts,
                                      const int *corner_edges,
                                      int corners_num,
                                      MeshIslandStore *r_island_store);

/* Above vert/UV mapping stuff does not do what we need here, but does things we do not need here.
 * So better keep them separated for now, I think. */

/**
 * Calculate 'generic' UV islands, i.e. based only on actual geometry data (edge seams),
 * not some UV layers coordinates.
 */
bool BKE_mesh_calc_islands_loop_face_edgeseam(const float (*vert_positions)[3],
                                              int totvert,
                                              const blender::int2 *edges,
                                              int totedge,
                                              const bool *uv_seams,
                                              blender::OffsetIndices<int> faces,
                                              const int *corner_verts,
                                              const int *corner_edges,
                                              int corners_num,
                                              MeshIslandStore *r_island_store);

/**
 * Calculate UV islands.
 *
 * \note If no UV layer is passed, we only consider edges tagged as seams as UV boundaries.
 * This has the advantages of simplicity, and being valid/common to all UV maps.
 * However, it means actual UV islands without matching UV seams will not be handled correctly.
 * If a valid UV layer is passed as \a luvs parameter,
 * UV coordinates are also used to detect islands boundaries.
 *
 * \note All this could be optimized.
 * Not sure it would be worth the more complex code, though,
 * those loops are supposed to be really quick to do.
 */
bool BKE_mesh_calc_islands_loop_face_uvmap(float (*vert_positions)[3],
                                           int totvert,
                                           blender::int2 *edges,
                                           int totedge,
                                           const bool *uv_seams,
                                           blender::OffsetIndices<int> faces,
                                           const int *corner_verts,
                                           const int *corner_edges,
                                           int corners_num,
                                           const float (*luvs)[2],
                                           MeshIslandStore *r_island_store);

/**
 * Calculate smooth groups from sharp edges.
 *
 * \param sharp_edges: Optional (possibly empty) span.
 * \param sharp_faces: Optional (possibly empty) span.
 * \param r_totgroup: The total number of groups, 1 or more.
 * \return Polygon aligned array of group index values (bitflags if use_bitflags is true),
 * starting at 1 (0 being used as 'invalid' flag).
 * Note it's callers's responsibility to MEM_freeN returned array.
 */
int *BKE_mesh_calc_smoothgroups(int edges_num,
                                blender::OffsetIndices<int> faces,
                                blender::Span<int> corner_edges,
                                blender::Span<bool> sharp_edges,
                                blender::Span<bool> sharp_faces,
                                int *r_totgroup,
                                bool use_bitflags);

/* Use on corner_tri vertex values. */
#define BKE_MESH_TESSTRI_VINDEX_ORDER(_tri, _v) \
  ((CHECK_TYPE_ANY( \
        _tri, unsigned int *, int *, int[3], const unsigned int *, const int *, const int[3]), \
    CHECK_TYPE_ANY(_v, unsigned int, const unsigned int, int, const int)), \
   (((_tri)[0] == _v) ? 0 : \
    ((_tri)[1] == _v) ? 1 : \
    ((_tri)[2] == _v) ? 2 : \
                        -1))

namespace blender::bke::mesh {

Array<int> build_corner_to_face_map(OffsetIndices<int> faces);

GroupedSpan<int> build_vert_to_edge_map(Span<int2> edges,
                                        int verts_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices);

void build_vert_to_face_indices(OffsetIndices<int> faces,
                                Span<int> corner_verts,
                                OffsetIndices<int> offsets,
                                MutableSpan<int> face_indices);
GroupedSpan<int> build_vert_to_face_map(OffsetIndices<int> faces,
                                        Span<int> corner_verts,
                                        int verts_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices);

Array<int> build_vert_to_corner_indices(Span<int> corner_verts, OffsetIndices<int> offsets);
GroupedSpan<int> build_vert_to_corner_map(Span<int> corner_verts,
                                          int verts_num,
                                          Array<int> &r_offsets,
                                          Array<int> &r_indices);

GroupedSpan<int> build_edge_to_corner_map(Span<int> corner_edges,
                                          int edges_num,
                                          Array<int> &r_offsets,
                                          Array<int> &r_indices);

GroupedSpan<int> build_edge_to_face_map(OffsetIndices<int> faces,
                                        Span<int> corner_edges,
                                        int edges_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices);

}  // namespace blender::bke::mesh
