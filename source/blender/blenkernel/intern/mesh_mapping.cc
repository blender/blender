/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions for accessing mesh connectivity data.
 * eg: polys connected to verts, UVs connected to verts.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_vec_types.h"

#include "BLI_array.hh"
#include "BLI_bitmap.h"
#include "BLI_buffer.h"
#include "BLI_function_ref.hh"
#include "BLI_math.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BLI_memarena.h"

#include "BLI_strict_flags.h"

/* -------------------------------------------------------------------- */
/** \name Mesh Connectivity Mapping
 * \{ */

UvVertMap *BKE_mesh_uv_vert_map_create(const blender::OffsetIndices<int> polys,
                                       const bool *hide_poly,
                                       const bool *select_poly,
                                       const int *corner_verts,
                                       const float (*mloopuv)[2],
                                       uint totvert,
                                       const float limit[2],
                                       const bool selected,
                                       const bool use_winding)
{
  /* NOTE: N-gon version WIP, based on #BM_uv_vert_map_create. */

  UvVertMap *vmap;
  UvMapVert *buf;
  int i, totuv, nverts;

  BLI_buffer_declare_static(vec2f, tf_uv_buf, BLI_BUFFER_NOP, 32);

  totuv = 0;

  /* generate UvMapVert array */
  for (const int64_t a : polys.index_range()) {
    if (!selected || (!(hide_poly && hide_poly[a]) && (select_poly && select_poly[a]))) {
      totuv += int(polys[a].size());
    }
  }

  if (totuv == 0) {
    return nullptr;
  }

  vmap = (UvVertMap *)MEM_callocN(sizeof(*vmap), "UvVertMap");
  buf = vmap->buf = (UvMapVert *)MEM_callocN(sizeof(*vmap->buf) * size_t(totuv), "UvMapVert");
  vmap->vert = (UvMapVert **)MEM_callocN(sizeof(*vmap->vert) * totvert, "UvMapVert*");

  if (!vmap->vert || !vmap->buf) {
    BKE_mesh_uv_vert_map_free(vmap);
    return nullptr;
  }

  bool *winding = nullptr;
  if (use_winding) {
    winding = static_cast<bool *>(
        MEM_calloc_arrayN(sizeof(*winding), size_t(polys.size()), "winding"));
  }

  for (const int64_t a : polys.index_range()) {
    const blender::IndexRange poly = polys[a];
    if (!selected || (!(hide_poly && hide_poly[a]) && (select_poly && select_poly[a]))) {
      float(*tf_uv)[2] = nullptr;

      if (use_winding) {
        tf_uv = (float(*)[2])BLI_buffer_reinit_data(&tf_uv_buf, vec2f, size_t(poly.size()));
      }

      nverts = int(poly.size());

      for (i = 0; i < nverts; i++) {
        buf->loop_of_poly_index = ushort(i);
        buf->poly_index = uint(a);
        buf->separate = false;
        buf->next = vmap->vert[corner_verts[poly[i]]];
        vmap->vert[corner_verts[poly[i]]] = buf;

        if (use_winding) {
          copy_v2_v2(tf_uv[i], mloopuv[poly[i]]);
        }

        buf++;
      }

      if (use_winding) {
        winding[a] = cross_poly_v2(tf_uv, uint(nverts)) > 0;
      }
    }
  }

  /* sort individual uvs for each vert */
  for (uint a = 0; a < totvert; a++) {
    UvMapVert *newvlist = nullptr, *vlist = vmap->vert[a];
    UvMapVert *iterv, *v, *lastv, *next;
    const float *uv, *uv2;
    float uvdiff[2];

    while (vlist) {
      v = vlist;
      vlist = vlist->next;
      v->next = newvlist;
      newvlist = v;

      uv = mloopuv[polys[v->poly_index].start() + v->loop_of_poly_index];
      lastv = nullptr;
      iterv = vlist;

      while (iterv) {
        next = iterv->next;

        uv2 = mloopuv[polys[iterv->poly_index].start() + iterv->loop_of_poly_index];
        sub_v2_v2v2(uvdiff, uv2, uv);

        if (fabsf(uv[0] - uv2[0]) < limit[0] && fabsf(uv[1] - uv2[1]) < limit[1] &&
            (!use_winding || winding[iterv->poly_index] == winding[v->poly_index]))
        {
          if (lastv) {
            lastv->next = next;
          }
          else {
            vlist = next;
          }
          iterv->next = newvlist;
          newvlist = iterv;
        }
        else {
          lastv = iterv;
        }

        iterv = next;
      }

      newvlist->separate = true;
    }

    vmap->vert[a] = newvlist;
  }

  if (use_winding) {
    MEM_freeN(winding);
  }

  BLI_buffer_free(&tf_uv_buf);

  return vmap;
}

UvMapVert *BKE_mesh_uv_vert_map_get_vert(UvVertMap *vmap, uint v)
{
  return vmap->vert[v];
}

void BKE_mesh_uv_vert_map_free(UvVertMap *vmap)
{
  if (vmap) {
    if (vmap->vert) {
      MEM_freeN(vmap->vert);
    }
    if (vmap->buf) {
      MEM_freeN(vmap->buf);
    }
    MEM_freeN(vmap);
  }
}

/**
 * Generates a map where the key is the vertex and the value is a list
 * of polys or loops that use that vertex as a corner. The lists are allocated
 * from one memory pool.
 *
 * Wrapped by #BKE_mesh_vert_poly_map_create & BKE_mesh_vert_loop_map_create
 */
static void mesh_vert_poly_or_loop_map_create(MeshElemMap **r_map,
                                              int **r_mem,
                                              const blender::OffsetIndices<int> polys,
                                              const int *corner_verts,
                                              int totvert,
                                              const bool do_loops)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totvert), __func__);
  int *indices, *index_iter;

  indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(polys.total_size()), __func__));
  index_iter = indices;

  /* Count number of polys for each vertex */
  for (const int64_t i : polys.index_range()) {
    for (const int64_t corner : polys[i]) {
      map[corner_verts[corner]].count++;
    }
  }

  /* Assign indices mem */
  for (int64_t i = 0; i < totvert; i++) {
    map[i].indices = index_iter;
    index_iter += map[i].count;

    /* Reset 'count' for use as index in last loop */
    map[i].count = 0;
  }

  /* Find the users */
  for (const int64_t i : polys.index_range()) {
    for (const int64_t corner : polys[i]) {
      const int v = corner_verts[corner];

      map[v].indices[map[v].count] = do_loops ? int(corner) : int(i);
      map[v].count++;
    }
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_vert_poly_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const blender::OffsetIndices<int> polys,
                                   const int *corner_verts,
                                   int totvert)
{
  mesh_vert_poly_or_loop_map_create(r_map, r_mem, polys, corner_verts, totvert, false);
}

void BKE_mesh_vert_loop_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const blender::OffsetIndices<int> polys,
                                   const int *corner_verts,
                                   int totvert)
{
  mesh_vert_poly_or_loop_map_create(r_map, r_mem, polys, corner_verts, totvert, true);
}

void BKE_mesh_vert_looptri_map_create(MeshElemMap **r_map,
                                      int **r_mem,
                                      const int totvert,
                                      const MLoopTri *mlooptri,
                                      const int totlooptri,
                                      const int *corner_verts,
                                      const int /*totloop*/)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totvert), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(totlooptri) * 3, __func__));
  int *index_step;
  const MLoopTri *mlt;
  int i;

  /* count face users */
  for (i = 0, mlt = mlooptri; i < totlooptri; mlt++, i++) {
    for (int j = 3; j--;) {
      map[corner_verts[mlt->tri[j]]].count++;
    }
  }

  /* create offsets */
  index_step = indices;
  for (i = 0; i < totvert; i++) {
    map[i].indices = index_step;
    index_step += map[i].count;

    /* re-count, using this as an index below */
    map[i].count = 0;
  }

  /* assign looptri-edge users */
  for (i = 0, mlt = mlooptri; i < totlooptri; mlt++, i++) {
    for (int j = 3; j--;) {
      MeshElemMap *map_ele = &map[corner_verts[mlt->tri[j]]];
      map_ele->indices[map_ele->count++] = i;
    }
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_vert_edge_map_create(
    MeshElemMap **r_map, int **r_mem, const blender::int2 *edges, int totvert, int totedge)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totvert), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int[2]) * size_t(totedge), __func__));
  int *i_pt = indices;

  int i;

  /* Count number of edges for each vertex */
  for (i = 0; i < totedge; i++) {
    map[edges[i][0]].count++;
    map[edges[i][1]].count++;
  }

  /* Assign indices mem */
  for (i = 0; i < totvert; i++) {
    map[i].indices = i_pt;
    i_pt += map[i].count;

    /* Reset 'count' for use as index in last loop */
    map[i].count = 0;
  }

  /* Find the users */
  for (i = 0; i < totedge; i++) {
    const int v[2] = {edges[i][0], edges[i][1]};

    map[v[0]].indices[map[v[0]].count] = i;
    map[v[1]].indices[map[v[1]].count] = i;

    map[v[0]].count++;
    map[v[1]].count++;
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_vert_edge_vert_map_create(
    MeshElemMap **r_map, int **r_mem, const blender::int2 *edges, int totvert, int totedge)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totvert), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int[2]) * size_t(totedge), __func__));
  int *i_pt = indices;

  int i;

  /* Count number of edges for each vertex */
  for (i = 0; i < totedge; i++) {
    map[edges[i][0]].count++;
    map[edges[i][1]].count++;
  }

  /* Assign indices mem */
  for (i = 0; i < totvert; i++) {
    map[i].indices = i_pt;
    i_pt += map[i].count;

    /* Reset 'count' for use as index in last loop */
    map[i].count = 0;
  }

  /* Find the users */
  for (i = 0; i < totedge; i++) {
    const int v[2] = {edges[i][0], edges[i][1]};

    map[v[0]].indices[map[v[0]].count] = int(v[1]);
    map[v[1]].indices[map[v[1]].count] = int(v[0]);

    map[v[0]].count++;
    map[v[1]].count++;
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_edge_loop_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const int totedge,
                                   const blender::OffsetIndices<int> polys,
                                   const int *corner_edges,
                                   const int totloop)
{
  using namespace blender;
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totedge), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(totloop) * 2, __func__));
  int *index_step;

  /* count face users */
  for (const int64_t i : IndexRange(totloop)) {
    map[corner_edges[i]].count += 2;
  }

  /* create offsets */
  index_step = indices;
  for (int i = 0; i < totedge; i++) {
    map[i].indices = index_step;
    index_step += map[i].count;

    /* re-count, using this as an index below */
    map[i].count = 0;
  }

  /* assign loop-edge users */
  for (const int64_t i : polys.index_range()) {
    MeshElemMap *map_ele;
    for (const int64_t corner : polys[i]) {
      map_ele = &map[corner_edges[corner]];
      map_ele->indices[map_ele->count++] = int(corner);
      map_ele->indices[map_ele->count++] = int(corner) + 1;
    }
    /* last edge/loop of poly, must point back to first loop! */
    map_ele->indices[map_ele->count - 1] = int(polys[i].start());
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_edge_poly_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const int totedge,
                                   const blender::OffsetIndices<int> polys,
                                   const int *corner_edges,
                                   const int totloop)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totedge), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(totloop), __func__));
  int *index_step;

  /* count face users */
  for (const int64_t i : blender::IndexRange(totloop)) {
    map[corner_edges[i]].count++;
  }

  /* create offsets */
  index_step = indices;
  for (int i = 0; i < totedge; i++) {
    map[i].indices = index_step;
    index_step += map[i].count;

    /* re-count, using this as an index below */
    map[i].count = 0;
  }

  /* assign poly-edge users */
  for (const int64_t i : polys.index_range()) {
    for (const int64_t corner : polys[i]) {
      const int edge_i = corner_edges[corner];
      MeshElemMap *map_ele = &map[edge_i];
      map_ele->indices[map_ele->count++] = int(i);
    }
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_origindex_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const int totsource,
                                   const int *final_origindex,
                                   const int totfinal)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totsource), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(totfinal), __func__));
  int *index_step;
  int i;

  /* count face users */
  for (i = 0; i < totfinal; i++) {
    if (final_origindex[i] != ORIGINDEX_NONE) {
      BLI_assert(final_origindex[i] < totsource);
      map[final_origindex[i]].count++;
    }
  }

  /* create offsets */
  index_step = indices;
  for (i = 0; i < totsource; i++) {
    map[i].indices = index_step;
    index_step += map[i].count;

    /* re-count, using this as an index below */
    map[i].count = 0;
  }

  /* assign poly-tessface users */
  for (i = 0; i < totfinal; i++) {
    if (final_origindex[i] != ORIGINDEX_NONE) {
      MeshElemMap *map_ele = &map[final_origindex[i]];
      map_ele->indices[map_ele->count++] = i;
    }
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_origindex_map_create_looptri(MeshElemMap **r_map,
                                           int **r_mem,
                                           const blender::OffsetIndices<int> polys,
                                           const int *looptri_polys,
                                           const int looptri_num)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(polys.size()), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(looptri_num), __func__));
  int *index_step;

  /* create offsets */
  index_step = indices;
  for (const int64_t i : polys.index_range()) {
    map[i].indices = index_step;
    index_step += ME_POLY_TRI_TOT(polys[i].size());
  }

  /* assign poly-tessface users */
  for (int i = 0; i < looptri_num; i++) {
    MeshElemMap *map_ele = &map[looptri_polys[i]];
    map_ele->indices[map_ele->count++] = i;
  }

  *r_map = map;
  *r_mem = indices;
}

namespace blender::bke::mesh_topology {

Array<int> build_loop_to_poly_map(const OffsetIndices<int> polys)
{
  Array<int> map(polys.total_size());
  offset_indices::build_reverse_map(polys, map);
  return map;
}

Array<Vector<int>> build_vert_to_edge_map(const Span<int2> edges, const int verts_num)
{
  Array<Vector<int>> map(verts_num);
  for (const int64_t i : edges.index_range()) {
    map[edges[i][0]].append(int(i));
    map[edges[i][1]].append(int(i));
  }
  return map;
}

Array<Vector<int>> build_vert_to_poly_map(const OffsetIndices<int> polys,
                                          const Span<int> corner_verts,
                                          int verts_num)
{
  Array<Vector<int>> map(verts_num);
  for (const int64_t i : polys.index_range()) {
    for (const int64_t vert_i : corner_verts.slice(polys[i])) {
      map[int(vert_i)].append(int(i));
    }
  }
  return map;
}

Array<Vector<int>> build_vert_to_loop_map(const Span<int> corner_verts, const int verts_num)
{
  Array<Vector<int>> map(verts_num);
  for (const int64_t i : corner_verts.index_range()) {
    map[corner_verts[i]].append(int(i));
  }
  return map;
}

Array<Vector<int>> build_edge_to_loop_map(const Span<int> corner_edges, const int edges_num)
{
  Array<Vector<int>> map(edges_num);
  for (const int64_t i : corner_edges.index_range()) {
    map[corner_edges[i]].append(int(i));
  }
  return map;
}

Array<Vector<int, 2>> build_edge_to_poly_map(const OffsetIndices<int> polys,
                                             const Span<int> corner_edges,
                                             const int edges_num)
{
  Array<Vector<int, 2>> map(edges_num);
  for (const int64_t i : polys.index_range()) {
    for (const int edge : corner_edges.slice(polys[i])) {
      map[edge].append(int(i));
    }
  }
  return map;
}

Vector<Vector<int>> build_edge_to_loop_map_resizable(const Span<int> corner_edges,
                                                     const int edges_num)

{
  Vector<Vector<int>> map(edges_num);
  for (const int64_t i : corner_edges.index_range()) {
    map[corner_edges[i]].append(int(i));
  }
  return map;
}

}  // namespace blender::bke::mesh_topology

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh loops/poly islands.
 * Used currently for UVs and 'smooth groups'.
 * \{ */

/**
 * Callback deciding whether the given poly/loop/edge define an island boundary or not.
 */
using MeshRemap_CheckIslandBoundary =
    blender::FunctionRef<bool(int poly_index,
                              int loop_index,
                              int edge_index,
                              int edge_user_count,
                              const MeshElemMap &edge_poly_map_elem)>;

static void poly_edge_loop_islands_calc(const int totedge,
                                        const blender::OffsetIndices<int> polys,
                                        const blender::Span<int> corner_edges,
                                        MeshElemMap *edge_poly_map,
                                        const bool use_bitflags,
                                        MeshRemap_CheckIslandBoundary edge_boundary_check,
                                        int **r_poly_groups,
                                        int *r_totgroup,
                                        BLI_bitmap **r_edge_borders,
                                        int *r_totedgeborder)
{
  int *poly_groups;
  int *poly_stack;

  BLI_bitmap *edge_borders = nullptr;
  int num_edgeborders = 0;

  int poly_prev = 0;
  const int temp_poly_group_id = 3; /* Placeholder value. */

  /* Group we could not find any available bit, will be reset to 0 at end. */
  const int poly_group_id_overflowed = 5;

  int tot_group = 0;
  bool group_id_overflow = false;

  /* map vars */
  int *edge_poly_mem = nullptr;

  if (polys.size() == 0) {
    *r_totgroup = 0;
    *r_poly_groups = nullptr;
    if (r_edge_borders) {
      *r_edge_borders = nullptr;
      *r_totedgeborder = 0;
    }
    return;
  }

  if (r_edge_borders) {
    edge_borders = BLI_BITMAP_NEW(totedge, __func__);
    *r_totedgeborder = 0;
  }

  if (!edge_poly_map) {
    BKE_mesh_edge_poly_map_create(&edge_poly_map,
                                  &edge_poly_mem,
                                  totedge,
                                  polys,
                                  corner_edges.data(),
                                  int(corner_edges.size()));
  }

  poly_groups = static_cast<int *>(MEM_callocN(sizeof(int) * size_t(polys.size()), __func__));
  poly_stack = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(polys.size()), __func__));

  while (true) {
    int poly;
    int bit_poly_group_mask = 0;
    int poly_group_id;
    int ps_curr_idx = 0, ps_end_idx = 0; /* stack indices */

    for (poly = poly_prev; poly < int(polys.size()); poly++) {
      if (poly_groups[poly] == 0) {
        break;
      }
    }

    if (poly == int(polys.size())) {
      /* all done */
      break;
    }

    poly_group_id = use_bitflags ? temp_poly_group_id : ++tot_group;

    /* start searching from here next time */
    poly_prev = poly + 1;

    poly_groups[poly] = poly_group_id;
    poly_stack[ps_end_idx++] = poly;

    while (ps_curr_idx != ps_end_idx) {
      poly = poly_stack[ps_curr_idx++];
      BLI_assert(poly_groups[poly] == poly_group_id);

      for (const int64_t loop : polys[poly]) {
        const int edge = corner_edges[loop];
        /* loop over poly users */
        const MeshElemMap &map_ele = edge_poly_map[edge];
        const int *p = map_ele.indices;
        int i = map_ele.count;
        if (!edge_boundary_check(poly, int(loop), edge, i, map_ele)) {
          for (; i--; p++) {
            /* if we meet other non initialized its a bug */
            BLI_assert(ELEM(poly_groups[*p], 0, poly_group_id));

            if (poly_groups[*p] == 0) {
              poly_groups[*p] = poly_group_id;
              poly_stack[ps_end_idx++] = *p;
            }
          }
        }
        else {
          if (edge_borders && !BLI_BITMAP_TEST(edge_borders, edge)) {
            BLI_BITMAP_ENABLE(edge_borders, edge);
            num_edgeborders++;
          }
          if (use_bitflags) {
            /* Find contiguous smooth groups already assigned,
             * these are the values we can't reuse! */
            for (; i--; p++) {
              int bit = poly_groups[*p];
              if (!ELEM(bit, 0, poly_group_id, poly_group_id_overflowed) &&
                  !(bit_poly_group_mask & bit)) {
                bit_poly_group_mask |= bit;
              }
            }
          }
        }
      }
    }
    /* And now, we have all our poly from current group in poly_stack
     * (from 0 to (ps_end_idx - 1)),
     * as well as all smoothgroups bits we can't use in bit_poly_group_mask.
     */
    if (use_bitflags) {
      int i, *p, gid_bit = 0;
      poly_group_id = 1;

      /* Find first bit available! */
      for (; (poly_group_id & bit_poly_group_mask) && (gid_bit < 32); gid_bit++) {
        poly_group_id <<= 1; /* will 'overflow' on last possible iteration. */
      }
      if (UNLIKELY(gid_bit > 31)) {
        /* All bits used in contiguous smooth groups, we can't do much!
         * NOTE: this is *very* unlikely - theoretically, four groups are enough,
         *       I don't think we can reach this goal with such a simple algorithm,
         *       but I don't think either we'll never need all 32 groups!
         */
        printf(
            "Warning, could not find an available id for current smooth group, faces will me "
            "marked "
            "as out of any smooth group...\n");

        /* Can't use 0, will have to set them to this value later. */
        poly_group_id = poly_group_id_overflowed;

        group_id_overflow = true;
      }
      if (gid_bit > tot_group) {
        tot_group = gid_bit;
      }
      /* And assign the final smooth group id to that poly group! */
      for (i = ps_end_idx, p = poly_stack; i--; p++) {
        poly_groups[*p] = poly_group_id;
      }
    }
  }

  if (use_bitflags) {
    /* used bits are zero-based. */
    tot_group++;
  }

  if (UNLIKELY(group_id_overflow)) {
    int i = int(polys.size()), *gid = poly_groups;
    for (; i--; gid++) {
      if (*gid == poly_group_id_overflowed) {
        *gid = 0;
      }
    }
    /* Using 0 as group id adds one more group! */
    tot_group++;
  }

  if (edge_poly_mem) {
    MEM_freeN(edge_poly_map);
    MEM_freeN(edge_poly_mem);
  }
  MEM_freeN(poly_stack);

  *r_totgroup = tot_group;
  *r_poly_groups = poly_groups;
  if (r_edge_borders) {
    *r_edge_borders = edge_borders;
    *r_totedgeborder = num_edgeborders;
  }
}

int *BKE_mesh_calc_smoothgroups(const int totedge,
                                const int *poly_offsets,
                                const int totpoly,
                                const int *corner_edges,
                                const int totloop,
                                const bool *sharp_edges,
                                const bool *sharp_faces,
                                int *r_totgroup,
                                const bool use_bitflags)
{
  int *poly_groups = nullptr;

  auto poly_is_smooth = [&](const int i) { return !(sharp_faces && sharp_faces[i]); };

  auto poly_is_island_boundary_smooth = [&](const int poly_index,
                                            const int /*loop_index*/,
                                            const int edge_index,
                                            const int edge_user_count,
                                            const MeshElemMap &edge_poly_map_elem) {
    /* Edge is sharp if one of its polys is flat, or edge itself is sharp,
     * or edge is not used by exactly two polygons. */
    if (poly_is_smooth(poly_index) && !(sharp_edges && sharp_edges[edge_index]) &&
        (edge_user_count == 2))
    {
      /* In that case, edge appears to be smooth, but we need to check its other poly too. */
      const int other_poly_index = (poly_index == edge_poly_map_elem.indices[0]) ?
                                       edge_poly_map_elem.indices[1] :
                                       edge_poly_map_elem.indices[0];
      return !poly_is_smooth(other_poly_index);
    }
    return true;
  };

  poly_edge_loop_islands_calc(totedge,
                              blender::Span(poly_offsets, totpoly + 1),
                              {corner_edges, totloop},
                              nullptr,
                              use_bitflags,
                              poly_is_island_boundary_smooth,
                              &poly_groups,
                              r_totgroup,
                              nullptr,
                              nullptr);

  return poly_groups;
}

#define MISLAND_DEFAULT_BUFSIZE 64

void BKE_mesh_loop_islands_init(MeshIslandStore *island_store,
                                const short item_type,
                                const int items_num,
                                const short island_type,
                                const short innercut_type)
{
  MemArena *mem = island_store->mem;

  if (mem == nullptr) {
    mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    island_store->mem = mem;
  }
  /* else memarena should be cleared */

  BLI_assert(
      ELEM(item_type, MISLAND_TYPE_VERT, MISLAND_TYPE_EDGE, MISLAND_TYPE_POLY, MISLAND_TYPE_LOOP));
  BLI_assert(ELEM(
      island_type, MISLAND_TYPE_VERT, MISLAND_TYPE_EDGE, MISLAND_TYPE_POLY, MISLAND_TYPE_LOOP));

  island_store->item_type = item_type;
  island_store->items_to_islands_num = items_num;
  island_store->items_to_islands = static_cast<int *>(
      BLI_memarena_alloc(mem, sizeof(*island_store->items_to_islands) * size_t(items_num)));

  island_store->island_type = island_type;
  island_store->islands_num_alloc = MISLAND_DEFAULT_BUFSIZE;
  island_store->islands = static_cast<MeshElemMap **>(
      BLI_memarena_alloc(mem, sizeof(*island_store->islands) * island_store->islands_num_alloc));

  island_store->innercut_type = innercut_type;
  island_store->innercuts = static_cast<MeshElemMap **>(
      BLI_memarena_alloc(mem, sizeof(*island_store->innercuts) * island_store->islands_num_alloc));
}

void BKE_mesh_loop_islands_clear(MeshIslandStore *island_store)
{
  island_store->item_type = MISLAND_TYPE_NONE;
  island_store->items_to_islands_num = 0;
  island_store->items_to_islands = nullptr;

  island_store->island_type = MISLAND_TYPE_NONE;
  island_store->islands_num = 0;
  island_store->islands = nullptr;

  island_store->innercut_type = MISLAND_TYPE_NONE;
  island_store->innercuts = nullptr;

  if (island_store->mem) {
    BLI_memarena_clear(island_store->mem);
  }

  island_store->islands_num_alloc = 0;
}

void BKE_mesh_loop_islands_free(MeshIslandStore *island_store)
{
  if (island_store->mem) {
    BLI_memarena_free(island_store->mem);
    island_store->mem = nullptr;
  }
}

void BKE_mesh_loop_islands_add(MeshIslandStore *island_store,
                               const int item_num,
                               const int *items_indices,
                               const int num_island_items,
                               int *island_item_indices,
                               const int num_innercut_items,
                               int *innercut_item_indices)
{
  MemArena *mem = island_store->mem;

  MeshElemMap *isld, *innrcut;
  const int curr_island_idx = island_store->islands_num++;
  const size_t curr_num_islands = size_t(island_store->islands_num);
  int i = item_num;

  while (i--) {
    island_store->items_to_islands[items_indices[i]] = curr_island_idx;
  }

  if (UNLIKELY(curr_num_islands > island_store->islands_num_alloc)) {
    MeshElemMap **islds, **innrcuts;

    island_store->islands_num_alloc *= 2;
    islds = static_cast<MeshElemMap **>(
        BLI_memarena_alloc(mem, sizeof(*islds) * island_store->islands_num_alloc));
    memcpy(islds, island_store->islands, sizeof(*islds) * (curr_num_islands - 1));
    island_store->islands = islds;

    innrcuts = static_cast<MeshElemMap **>(
        BLI_memarena_alloc(mem, sizeof(*innrcuts) * island_store->islands_num_alloc));
    memcpy(innrcuts, island_store->innercuts, sizeof(*innrcuts) * (curr_num_islands - 1));
    island_store->innercuts = innrcuts;
  }

  island_store->islands[curr_island_idx] = isld = static_cast<MeshElemMap *>(
      BLI_memarena_alloc(mem, sizeof(*isld)));
  isld->count = num_island_items;
  isld->indices = static_cast<int *>(
      BLI_memarena_alloc(mem, sizeof(*isld->indices) * size_t(num_island_items)));
  memcpy(isld->indices, island_item_indices, sizeof(*isld->indices) * size_t(num_island_items));

  island_store->innercuts[curr_island_idx] = innrcut = static_cast<MeshElemMap *>(
      BLI_memarena_alloc(mem, sizeof(*innrcut)));
  innrcut->count = num_innercut_items;
  innrcut->indices = static_cast<int *>(
      BLI_memarena_alloc(mem, sizeof(*innrcut->indices) * size_t(num_innercut_items)));
  memcpy(innrcut->indices,
         innercut_item_indices,
         sizeof(*innrcut->indices) * size_t(num_innercut_items));
}

static bool mesh_calc_islands_loop_poly_uv(const int totedge,
                                           const bool *uv_seams,
                                           const blender::OffsetIndices<int> polys,
                                           const int *corner_verts,
                                           const int *corner_edges,
                                           const int totloop,
                                           const float (*luvs)[2],
                                           MeshIslandStore *r_island_store)
{
  int *poly_groups = nullptr;
  int num_poly_groups;

  /* map vars */
  MeshElemMap *edge_poly_map;
  int *edge_poly_mem;

  MeshElemMap *edge_loop_map;
  int *edge_loop_mem;

  int *poly_indices;
  int *loop_indices;
  int num_pidx, num_lidx;

  /* Those are used to detect 'inner cuts', i.e. edges that are borders,
   * and yet have two or more polys of a same group using them
   * (typical case: seam used to unwrap properly a cylinder). */
  BLI_bitmap *edge_borders = nullptr;
  int num_edge_borders = 0;
  char *edge_border_count = nullptr;
  int *edge_innercut_indices = nullptr;
  int num_einnercuts = 0;

  int grp_idx;

  BKE_mesh_loop_islands_clear(r_island_store);
  BKE_mesh_loop_islands_init(
      r_island_store, MISLAND_TYPE_LOOP, totloop, MISLAND_TYPE_POLY, MISLAND_TYPE_EDGE);

  BKE_mesh_edge_poly_map_create(
      &edge_poly_map, &edge_poly_mem, totedge, polys, corner_edges, totloop);

  if (luvs) {
    BKE_mesh_edge_loop_map_create(
        &edge_loop_map, &edge_loop_mem, totedge, polys, corner_edges, totloop);
  }

  /* TODO: I'm not sure edge seam flag is enough to define UV islands?
   *       Maybe we should also consider UV-maps values
   *       themselves (i.e. different UV-edges for a same mesh-edge => boundary edge too?).
   *       Would make things much more complex though,
   *       and each UVMap would then need its own mesh mapping, not sure we want that at all!
   */
  auto mesh_check_island_boundary_uv = [&](const int /*poly_index*/,
                                           const int loop_index,
                                           const int edge_index,
                                           const int /*edge_user_count*/,
                                           const MeshElemMap & /*edge_poly_map_elem*/) -> bool {
    if (luvs) {
      const MeshElemMap &edge_to_loops = edge_loop_map[corner_edges[loop_index]];

      BLI_assert(edge_to_loops.count >= 2 && (edge_to_loops.count % 2) == 0);

      const int v1 = corner_verts[edge_to_loops.indices[0]];
      const int v2 = corner_verts[edge_to_loops.indices[1]];
      const float *uvco_v1 = luvs[edge_to_loops.indices[0]];
      const float *uvco_v2 = luvs[edge_to_loops.indices[1]];
      for (int i = 2; i < edge_to_loops.count; i += 2) {
        if (corner_verts[edge_to_loops.indices[i]] == v1) {
          if (!equals_v2v2(uvco_v1, luvs[edge_to_loops.indices[i]]) ||
              !equals_v2v2(uvco_v2, luvs[edge_to_loops.indices[i + 1]]))
          {
            return true;
          }
        }
        else {
          BLI_assert(corner_verts[edge_to_loops.indices[i]] == v2);
          UNUSED_VARS_NDEBUG(v2);
          if (!equals_v2v2(uvco_v2, luvs[edge_to_loops.indices[i]]) ||
              !equals_v2v2(uvco_v1, luvs[edge_to_loops.indices[i + 1]]))
          {
            return true;
          }
        }
      }
      return false;
    }

    /* Edge is UV boundary if tagged as seam. */
    return uv_seams && uv_seams[edge_index];
  };

  poly_edge_loop_islands_calc(totedge,
                              polys,
                              {corner_edges, totloop},
                              edge_poly_map,
                              false,
                              mesh_check_island_boundary_uv,
                              &poly_groups,
                              &num_poly_groups,
                              &edge_borders,
                              &num_edge_borders);

  if (!num_poly_groups) {
    /* Should never happen... */
    MEM_freeN(edge_poly_map);
    MEM_freeN(edge_poly_mem);

    if (edge_borders) {
      MEM_freeN(edge_borders);
    }
    return false;
  }

  if (num_edge_borders) {
    edge_border_count = static_cast<char *>(
        MEM_mallocN(sizeof(*edge_border_count) * size_t(totedge), __func__));
    edge_innercut_indices = static_cast<int *>(
        MEM_mallocN(sizeof(*edge_innercut_indices) * size_t(num_edge_borders), __func__));
  }

  poly_indices = static_cast<int *>(
      MEM_mallocN(sizeof(*poly_indices) * size_t(polys.size()), __func__));
  loop_indices = static_cast<int *>(
      MEM_mallocN(sizeof(*loop_indices) * size_t(totloop), __func__));

  /* NOTE: here we ignore '0' invalid group - this should *never* happen in this case anyway? */
  for (grp_idx = 1; grp_idx <= num_poly_groups; grp_idx++) {
    num_pidx = num_lidx = 0;
    if (num_edge_borders) {
      num_einnercuts = 0;
      memset(edge_border_count, 0, sizeof(*edge_border_count) * size_t(totedge));
    }

    for (const int64_t p_idx : polys.index_range()) {
      if (poly_groups[p_idx] != grp_idx) {
        continue;
      }
      poly_indices[num_pidx++] = int(p_idx);
      for (const int64_t corner : polys[p_idx]) {
        const int edge_i = corner_edges[corner];
        loop_indices[num_lidx++] = int(corner);
        if (num_edge_borders && BLI_BITMAP_TEST(edge_borders, edge_i) &&
            (edge_border_count[edge_i] < 2)) {
          edge_border_count[edge_i]++;
          if (edge_border_count[edge_i] == 2) {
            edge_innercut_indices[num_einnercuts++] = edge_i;
          }
        }
      }
    }

    BKE_mesh_loop_islands_add(r_island_store,
                              num_lidx,
                              loop_indices,
                              num_pidx,
                              poly_indices,
                              num_einnercuts,
                              edge_innercut_indices);
  }

  MEM_freeN(edge_poly_map);
  MEM_freeN(edge_poly_mem);

  if (luvs) {
    MEM_freeN(edge_loop_map);
    MEM_freeN(edge_loop_mem);
  }

  MEM_freeN(poly_indices);
  MEM_freeN(loop_indices);
  MEM_freeN(poly_groups);

  if (edge_borders) {
    MEM_freeN(edge_borders);
  }

  if (num_edge_borders) {
    MEM_freeN(edge_border_count);
    MEM_freeN(edge_innercut_indices);
  }
  return true;
}

bool BKE_mesh_calc_islands_loop_poly_edgeseam(const float (*vert_positions)[3],
                                              const int totvert,
                                              const blender::int2 *edges,
                                              const int totedge,
                                              const bool *uv_seams,
                                              const blender::OffsetIndices<int> polys,
                                              const int *corner_verts,
                                              const int *corner_edges,
                                              const int totloop,
                                              MeshIslandStore *r_island_store)
{
  UNUSED_VARS(vert_positions, totvert, edges);
  return mesh_calc_islands_loop_poly_uv(
      totedge, uv_seams, polys, corner_verts, corner_edges, totloop, nullptr, r_island_store);
}

bool BKE_mesh_calc_islands_loop_poly_uvmap(float (*vert_positions)[3],
                                           const int totvert,
                                           blender::int2 *edges,
                                           const int totedge,
                                           const bool *uv_seams,
                                           const blender::OffsetIndices<int> polys,
                                           const int *corner_verts,
                                           const int *corner_edges,
                                           const int totloop,
                                           const float (*luvs)[2],
                                           MeshIslandStore *r_island_store)
{
  UNUSED_VARS(vert_positions, totvert, edges);
  BLI_assert(luvs != nullptr);
  return mesh_calc_islands_loop_poly_uv(
      totedge, uv_seams, polys, corner_verts, corner_edges, totloop, luvs, r_island_store);
}

/** \} */
