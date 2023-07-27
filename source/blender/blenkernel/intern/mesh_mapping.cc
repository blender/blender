/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions for accessing mesh connectivity data.
 * eg: faces connected to verts, UVs connected to verts.
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

UvVertMap *BKE_mesh_uv_vert_map_create(const blender::OffsetIndices<int> faces,
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
  for (const int64_t a : faces.index_range()) {
    if (!selected || (!(hide_poly && hide_poly[a]) && (select_poly && select_poly[a]))) {
      totuv += int(faces[a].size());
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
        MEM_calloc_arrayN(sizeof(*winding), size_t(faces.size()), "winding"));
  }

  for (const int64_t a : faces.index_range()) {
    const blender::IndexRange face = faces[a];
    if (!selected || (!(hide_poly && hide_poly[a]) && (select_poly && select_poly[a]))) {
      float(*tf_uv)[2] = nullptr;

      if (use_winding) {
        tf_uv = (float(*)[2])BLI_buffer_reinit_data(&tf_uv_buf, vec2f, size_t(face.size()));
      }

      nverts = int(face.size());

      for (i = 0; i < nverts; i++) {
        buf->loop_of_face_index = ushort(i);
        buf->face_index = uint(a);
        buf->separate = false;
        buf->next = vmap->vert[corner_verts[face[i]]];
        vmap->vert[corner_verts[face[i]]] = buf;

        if (use_winding) {
          copy_v2_v2(tf_uv[i], mloopuv[face[i]]);
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

      uv = mloopuv[faces[v->face_index].start() + v->loop_of_face_index];
      lastv = nullptr;
      iterv = vlist;

      while (iterv) {
        next = iterv->next;

        uv2 = mloopuv[faces[iterv->face_index].start() + iterv->loop_of_face_index];
        sub_v2_v2v2(uvdiff, uv2, uv);

        if (fabsf(uv[0] - uv2[0]) < limit[0] && fabsf(uv[1] - uv2[1]) < limit[1] &&
            (!use_winding || winding[iterv->face_index] == winding[v->face_index]))
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

  /* Assign face-tessellation users. */
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
                                           const blender::OffsetIndices<int> faces,
                                           const int *looptri_faces,
                                           const int looptri_num)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(faces.size()), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(looptri_num), __func__));
  int *index_step;

  /* create offsets */
  index_step = indices;
  for (const int64_t i : faces.index_range()) {
    map[i].indices = index_step;
    index_step += ME_FACE_TRI_TOT(faces[i].size());
  }

  /* Assign face-tessellation users. */
  for (int i = 0; i < looptri_num; i++) {
    MeshElemMap *map_ele = &map[looptri_faces[i]];
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

Array<int> build_loop_to_face_map(const OffsetIndices<int> faces)
{
  Array<int> map(faces.total_size());
  offset_indices::build_reverse_map(faces, map);
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

GroupedSpan<int> build_vert_to_face_map(const OffsetIndices<int> faces,
                                        const Span<int> corner_verts,
                                        const int verts_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(corner_verts, verts_num);
  r_indices.reinitialize(r_offsets.last());
  Array<int> counts(verts_num, 0);

  for (const int64_t face_i : faces.index_range()) {
    for (const int vert : corner_verts.slice(faces[face_i])) {
      r_indices[r_offsets[vert] + counts[vert]] = int(face_i);
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

GroupedSpan<int> build_edge_to_face_map(const OffsetIndices<int> faces,
                                        const Span<int> corner_edges,
                                        const int edges_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(corner_edges, edges_num);
  r_indices.reinitialize(r_offsets.last());
  Array<int> counts(edges_num, 0);

  for (const int64_t face_i : faces.index_range()) {
    for (const int edge : corner_edges.slice(faces[face_i])) {
      r_indices[r_offsets[edge] + counts[edge]] = int(face_i);
      counts[edge]++;
    }
  }
  return {OffsetIndices<int>(r_offsets), r_indices};
}

}  // namespace blender::bke::mesh

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh loops/face islands.
 * Used currently for UVs and 'smooth groups'.
 * \{ */

/**
 * Callback deciding whether the given face/loop/edge define an island boundary or not.
 */
using MeshRemap_CheckIslandBoundary =
    blender::FunctionRef<bool(int face_index,
                              int loop_index,
                              int edge_index,
                              int edge_user_count,
                              const blender::Span<int> edge_face_map_elem)>;

static void face_edge_loop_islands_calc(const int totedge,
                                        const blender::OffsetIndices<int> faces,
                                        const blender::Span<int> corner_edges,
                                        blender::GroupedSpan<int> edge_face_map,
                                        const bool use_bitflags,
                                        MeshRemap_CheckIslandBoundary edge_boundary_check,
                                        int **r_face_groups,
                                        int *r_totgroup,
                                        BLI_bitmap **r_edge_borders,
                                        int *r_totedgeborder)
{
  int *face_groups;
  int *face_stack;

  BLI_bitmap *edge_borders = nullptr;
  int num_edgeborders = 0;

  int face_prev = 0;
  const int temp_face_group_id = 3; /* Placeholder value. */

  /* Group we could not find any available bit, will be reset to 0 at end. */
  const int face_group_id_overflowed = 5;

  int tot_group = 0;
  bool group_id_overflow = false;

  if (faces.size() == 0) {
    *r_totgroup = 0;
    *r_face_groups = nullptr;
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

  blender::Array<int> edge_to_face_src_offsets;
  blender::Array<int> edge_to_face_src_indices;
  if (edge_face_map.is_empty()) {
    edge_face_map = blender::bke::mesh::build_edge_to_face_map(
        faces, corner_edges, totedge, edge_to_face_src_offsets, edge_to_face_src_indices);
  }

  face_groups = static_cast<int *>(MEM_callocN(sizeof(int) * size_t(faces.size()), __func__));
  face_stack = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(faces.size()), __func__));

  while (true) {
    int face;
    int bit_face_group_mask = 0;
    int face_group_id;
    int ps_curr_idx = 0, ps_end_idx = 0; /* stack indices */

    for (face = face_prev; face < int(faces.size()); face++) {
      if (face_groups[face] == 0) {
        break;
      }
    }

    if (face == int(faces.size())) {
      /* all done */
      break;
    }

    face_group_id = use_bitflags ? temp_face_group_id : ++tot_group;

    /* start searching from here next time */
    face_prev = face + 1;

    face_groups[face] = face_group_id;
    face_stack[ps_end_idx++] = face;

    while (ps_curr_idx != ps_end_idx) {
      face = face_stack[ps_curr_idx++];
      BLI_assert(face_groups[face] == face_group_id);

      for (const int64_t loop : faces[face]) {
        const int edge = corner_edges[loop];
        /* loop over face users */
        const blender::Span<int> map_ele = edge_face_map[edge];
        const int *p = map_ele.data();
        int i = int(map_ele.size());
        if (!edge_boundary_check(face, int(loop), edge, i, map_ele)) {
          for (; i--; p++) {
            /* if we meet other non initialized its a bug */
            BLI_assert(ELEM(face_groups[*p], 0, face_group_id));

            if (face_groups[*p] == 0) {
              face_groups[*p] = face_group_id;
              face_stack[ps_end_idx++] = *p;
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
              int bit = face_groups[*p];
              if (!ELEM(bit, 0, face_group_id, face_group_id_overflowed) &&
                  !(bit_face_group_mask & bit)) {
                bit_face_group_mask |= bit;
              }
            }
          }
        }
      }
    }
    /* And now, we have all our face from current group in face_stack
     * (from 0 to (ps_end_idx - 1)),
     * as well as all smoothgroups bits we can't use in bit_face_group_mask.
     */
    if (use_bitflags) {
      int i, *p, gid_bit = 0;
      face_group_id = 1;

      /* Find first bit available! */
      for (; (face_group_id & bit_face_group_mask) && (gid_bit < 32); gid_bit++) {
        face_group_id <<= 1; /* will 'overflow' on last possible iteration. */
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
        face_group_id = face_group_id_overflowed;

        group_id_overflow = true;
      }
      if (gid_bit > tot_group) {
        tot_group = gid_bit;
      }
      /* And assign the final smooth group id to that face group! */
      for (i = ps_end_idx, p = face_stack; i--; p++) {
        face_groups[*p] = face_group_id;
      }
    }
  }

  if (use_bitflags) {
    /* used bits are zero-based. */
    tot_group++;
  }

  if (UNLIKELY(group_id_overflow)) {
    int i = int(faces.size()), *gid = face_groups;
    for (; i--; gid++) {
      if (*gid == face_group_id_overflowed) {
        *gid = 0;
      }
    }
    /* Using 0 as group id adds one more group! */
    tot_group++;
  }

  MEM_freeN(face_stack);

  *r_totgroup = tot_group;
  *r_face_groups = face_groups;
  if (r_edge_borders) {
    *r_edge_borders = edge_borders;
    *r_totedgeborder = num_edgeborders;
  }
}

int *BKE_mesh_calc_smoothgroups(int edges_num,
                                const blender::OffsetIndices<int> faces,
                                const blender::Span<int> corner_edges,
                                const bool *sharp_edges,
                                const bool *sharp_faces,
                                int *r_totgroup,
                                bool use_bitflags)
{
  int *face_groups = nullptr;

  auto face_is_smooth = [&](const int i) { return !(sharp_faces && sharp_faces[i]); };

  auto face_is_island_boundary_smooth = [&](const int face_index,
                                            const int /*loop_index*/,
                                            const int edge_index,
                                            const int edge_user_count,
                                            const blender::Span<int> edge_face_map_elem) {
    /* Edge is sharp if one of its faces is flat, or edge itself is sharp,
     * or edge is not used by exactly two faces. */
    if (face_is_smooth(face_index) && !(sharp_edges && sharp_edges[edge_index]) &&
        (edge_user_count == 2))
    {
      /* In that case, edge appears to be smooth, but we need to check its other face too. */
      const int other_face_index = (face_index == edge_face_map_elem[0]) ? edge_face_map_elem[1] :
                                                                           edge_face_map_elem[0];
      return !face_is_smooth(other_face_index);
    }
    return true;
  };

  face_edge_loop_islands_calc(edges_num,
                              faces,
                              corner_edges,
                              {},
                              use_bitflags,
                              face_is_island_boundary_smooth,
                              &face_groups,
                              r_totgroup,
                              nullptr,
                              nullptr);

  return face_groups;
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

static bool mesh_calc_islands_loop_face_uv(const int totedge,
                                           const bool *uv_seams,
                                           const blender::OffsetIndices<int> faces,
                                           const int *corner_verts,
                                           const int *corner_edges,
                                           const int totloop,
                                           const float (*luvs)[2],
                                           MeshIslandStore *r_island_store)
{
  using namespace blender;
  int *face_groups = nullptr;
  int num_face_groups;

  int *face_indices;
  int *loop_indices;
  int num_pidx, num_lidx;

  /* Those are used to detect 'inner cuts', i.e. edges that are borders,
   * and yet have two or more faces of a same group using them
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

  Array<int> edge_to_face_offsets;
  Array<int> edge_to_face_indices;
  const GroupedSpan<int> edge_to_face_map = bke::mesh::build_edge_to_face_map(
      faces, {corner_edges, totloop}, totedge, edge_to_face_offsets, edge_to_face_indices);

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
  auto mesh_check_island_boundary_uv = [&](const int /*face_index*/,
                                           const int loop_index,
                                           const int edge_index,
                                           const int /*edge_user_count*/,
                                           const Span<int> /*edge_face_map_elem*/) -> bool {
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

  face_edge_loop_islands_calc(totedge,
                              faces,
                              {corner_edges, totloop},
                              edge_to_face_map,
                              false,
                              mesh_check_island_boundary_uv,
                              &face_groups,
                              &num_face_groups,
                              &edge_borders,
                              &num_edge_borders);

  if (!num_face_groups) {
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

  face_indices = static_cast<int *>(
      MEM_mallocN(sizeof(*face_indices) * size_t(faces.size()), __func__));
  loop_indices = static_cast<int *>(
      MEM_mallocN(sizeof(*loop_indices) * size_t(totloop), __func__));

  /* NOTE: here we ignore '0' invalid group - this should *never* happen in this case anyway? */
  for (grp_idx = 1; grp_idx <= num_face_groups; grp_idx++) {
    num_pidx = num_lidx = 0;
    if (num_edge_borders) {
      num_einnercuts = 0;
      memset(edge_border_count, 0, sizeof(*edge_border_count) * size_t(totedge));
    }

    for (const int64_t p_idx : faces.index_range()) {
      if (face_groups[p_idx] != grp_idx) {
        continue;
      }
      face_indices[num_pidx++] = int(p_idx);
      for (const int64_t corner : faces[p_idx]) {
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
                              face_indices,
                              num_einnercuts,
                              edge_innercut_indices);
  }

  MEM_freeN(face_indices);
  MEM_freeN(loop_indices);
  MEM_freeN(face_groups);

  if (edge_borders) {
    MEM_freeN(edge_borders);
  }

  if (num_edge_borders) {
    MEM_freeN(edge_border_count);
    MEM_freeN(edge_innercut_indices);
  }
  return true;
}

bool BKE_mesh_calc_islands_loop_face_edgeseam(const float (*vert_positions)[3],
                                              const int totvert,
                                              const blender::int2 *edges,
                                              const int totedge,
                                              const bool *uv_seams,
                                              const blender::OffsetIndices<int> faces,
                                              const int *corner_verts,
                                              const int *corner_edges,
                                              const int totloop,
                                              MeshIslandStore *r_island_store)
{
  UNUSED_VARS(vert_positions, totvert, edges);
  return mesh_calc_islands_loop_face_uv(
      totedge, uv_seams, faces, corner_verts, corner_edges, totloop, nullptr, r_island_store);
}

bool BKE_mesh_calc_islands_loop_face_uvmap(float (*vert_positions)[3],
                                           const int totvert,
                                           blender::int2 *edges,
                                           const int totedge,
                                           const bool *uv_seams,
                                           const blender::OffsetIndices<int> faces,
                                           const int *corner_verts,
                                           const int *corner_edges,
                                           const int totloop,
                                           const float (*luvs)[2],
                                           MeshIslandStore *r_island_store)
{
  UNUSED_VARS(vert_positions, totvert, edges);
  BLI_assert(luvs != nullptr);
  return mesh_calc_islands_loop_face_uv(
      totedge, uv_seams, faces, corner_verts, corner_edges, totloop, luvs, r_island_store);
}

/** \} */
