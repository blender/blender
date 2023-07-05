/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

namespace blender::bke::mesh {

static Array<int> create_reverse_offsets(const Span<int> indices, const int items_num)
{
  Array<int> offsets(items_num + 1, 0);
  offset_indices::build_reverse_offsets(indices, offsets);
  return offsets;
}

Array<int> build_loop_to_poly_map(const OffsetIndices<int> polys)
{
  Array<int> map(polys.total_size());
  offset_indices::build_reverse_map(polys, map);
  return map;
}

GroupedSpan<int> build_vert_to_edge_map(const Span<int2> edges,
                                        const int verts_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(edges.cast<int>(), verts_num);
  r_indices.reinitialize(r_offsets.last());
  Array<int> counts(verts_num, 0);

  for (const int64_t edge_i : edges.index_range()) {
    for (const int vert : {edges[edge_i][0], edges[edge_i][1]}) {
      r_indices[r_offsets[vert] + counts[vert]] = int(edge_i);
      counts[vert]++;
    }
  }
  return {OffsetIndices<int>(r_offsets), r_indices};
}

GroupedSpan<int> build_vert_to_poly_map(const OffsetIndices<int> polys,
                                        const Span<int> corner_verts,
                                        const int verts_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(corner_verts, verts_num);
  r_indices.reinitialize(r_offsets.last());
  Array<int> counts(verts_num, 0);

  for (const int64_t poly_i : polys.index_range()) {
    for (const int vert : corner_verts.slice(polys[poly_i])) {
      r_indices[r_offsets[vert] + counts[vert]] = int(poly_i);
      counts[vert]++;
    }
  }
  return {OffsetIndices<int>(r_offsets), r_indices};
}

GroupedSpan<int> build_vert_to_loop_map(const Span<int> corner_verts,
                                        const int verts_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(corner_verts, verts_num);
  r_indices.reinitialize(r_offsets.last());
  Array<int> counts(verts_num, 0);

  for (const int64_t corner : corner_verts.index_range()) {
    const int vert = corner_verts[corner];
    r_indices[r_offsets[vert] + counts[vert]] = int(corner);
    counts[vert]++;
  }
  return {OffsetIndices<int>(r_offsets), r_indices};
}

GroupedSpan<int> build_edge_to_loop_map(const Span<int> corner_edges,
                                        const int edges_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(corner_edges, edges_num);
  r_indices.reinitialize(r_offsets.last());
  Array<int> counts(edges_num, 0);

  for (const int64_t corner : corner_edges.index_range()) {
    const int edge = corner_edges[corner];
    r_indices[r_offsets[edge] + counts[edge]] = int(corner);
    counts[edge]++;
  }
  return {OffsetIndices<int>(r_offsets), r_indices};
}

GroupedSpan<int> build_edge_to_poly_map(const OffsetIndices<int> polys,
                                        const Span<int> corner_edges,
                                        const int edges_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(corner_edges, edges_num);
  r_indices.reinitialize(r_offsets.last());
  Array<int> counts(edges_num, 0);

  for (const int64_t poly_i : polys.index_range()) {
    for (const int edge : corner_edges.slice(polys[poly_i])) {
      r_indices[r_offsets[edge] + counts[edge]] = int(poly_i);
      counts[edge]++;
    }
  }
  return {OffsetIndices<int>(r_offsets), r_indices};
}

}  // namespace blender::bke::mesh

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
                              const blender::Span<int> edge_poly_map_elem)>;

static void poly_edge_loop_islands_calc(const int totedge,
                                        const blender::OffsetIndices<int> polys,
                                        const blender::Span<int> corner_edges,
                                        blender::GroupedSpan<int> edge_poly_map,
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

  blender::Array<int> edge_to_poly_src_offsets;
  blender::Array<int> edge_to_poly_src_indices;
  if (edge_poly_map.is_empty()) {
    edge_poly_map = blender::bke::mesh::build_edge_to_poly_map(
        polys, corner_edges, totedge, edge_to_poly_src_offsets, edge_to_poly_src_indices);
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
        const blender::Span<int> map_ele = edge_poly_map[edge];
        const int *p = map_ele.data();
        int i = int(map_ele.size());
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
                                            const blender::Span<int> edge_poly_map_elem) {
    /* Edge is sharp if one of its polys is flat, or edge itself is sharp,
     * or edge is not used by exactly two polygons. */
    if (poly_is_smooth(poly_index) && !(sharp_edges && sharp_edges[edge_index]) &&
        (edge_user_count == 2))
    {
      /* In that case, edge appears to be smooth, but we need to check its other poly too. */
      const int other_poly_index = (poly_index == edge_poly_map_elem[0]) ? edge_poly_map_elem[1] :
                                                                           edge_poly_map_elem[0];
      return !poly_is_smooth(other_poly_index);
    }
    return true;
  };

  poly_edge_loop_islands_calc(totedge,
                              blender::Span(poly_offsets, totpoly + 1),
                              {corner_edges, totloop},
                              {},
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
  using namespace blender;
  int *poly_groups = nullptr;
  int num_poly_groups;

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

  Array<int> edge_to_poly_offsets;
  Array<int> edge_to_poly_indices;
  const GroupedSpan<int> edge_to_poly_map = bke::mesh::build_edge_to_poly_map(
      polys, {corner_edges, totloop}, totedge, edge_to_poly_offsets, edge_to_poly_indices);

  Array<int> edge_to_loop_offsets;
  Array<int> edge_to_loop_indices;
  GroupedSpan<int> edge_to_loop_map;
  if (luvs) {
    edge_to_loop_map = bke::mesh::build_edge_to_loop_map(
        {corner_edges, totloop}, totedge, edge_to_loop_offsets, edge_to_loop_indices);
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
                                           const Span<int> /*edge_poly_map_elem*/) -> bool {
    if (luvs) {
      const Span<int> edge_to_loops = edge_to_loop_map[corner_edges[loop_index]];

      BLI_assert(edge_to_loops.size() >= 2 && (edge_to_loops.size() % 2) == 0);

      const int v1 = corner_verts[edge_to_loops[0]];
      const int v2 = corner_verts[edge_to_loops[1]];
      const float *uvco_v1 = luvs[edge_to_loops[0]];
      const float *uvco_v2 = luvs[edge_to_loops[1]];
      for (int i = 2; i < edge_to_loops.size(); i += 2) {
        if (corner_verts[edge_to_loops[i]] == v1) {
          if (!equals_v2v2(uvco_v1, luvs[edge_to_loops[i]]) ||
              !equals_v2v2(uvco_v2, luvs[edge_to_loops[i + 1]]))
          {
            return true;
          }
        }
        else {
          BLI_assert(corner_verts[edge_to_loops[i]] == v2);
          UNUSED_VARS_NDEBUG(v2);
          if (!equals_v2v2(uvco_v2, luvs[edge_to_loops[i]]) ||
              !equals_v2v2(uvco_v1, luvs[edge_to_loops[i + 1]]))
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
                              edge_to_poly_map,
                              false,
                              mesh_check_island_boundary_uv,
                              &poly_groups,
                              &num_poly_groups,
                              &edge_borders,
                              &num_edge_borders);

  if (!num_poly_groups) {
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
