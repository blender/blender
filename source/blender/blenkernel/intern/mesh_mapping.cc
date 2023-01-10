/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions for accessing mesh connectivity data.
 * eg: polys connected to verts, UV's connected to verts.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_vec_types.h"

#include "BLI_array.hh"
#include "BLI_bitmap.h"
#include "BLI_buffer.h"
#include "BLI_math.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_mesh_mapping.h"
#include "BLI_memarena.h"

#include "BLI_strict_flags.h"

/* -------------------------------------------------------------------- */
/** \name Mesh Connectivity Mapping
 * \{ */

/* ngon version wip, based on BM_uv_vert_map_create */
UvVertMap *BKE_mesh_uv_vert_map_create(const MPoly *mpoly,
                                       const bool *hide_poly,
                                       const bool *select_poly,
                                       const MLoop *mloop,
                                       const float (*mloopuv)[2],
                                       uint totpoly,
                                       uint totvert,
                                       const float limit[2],
                                       const bool selected,
                                       const bool use_winding)
{
  UvVertMap *vmap;
  UvMapVert *buf;
  const MPoly *mp;
  uint a;
  int i, totuv, nverts;

  bool *winding = nullptr;
  BLI_buffer_declare_static(vec2f, tf_uv_buf, BLI_BUFFER_NOP, 32);

  totuv = 0;

  /* generate UvMapVert array */
  mp = mpoly;
  for (a = 0; a < totpoly; a++, mp++) {
    if (!selected || (!(hide_poly && hide_poly[a]) && (select_poly && select_poly[a]))) {
      totuv += mp->totloop;
    }
  }

  if (totuv == 0) {
    return nullptr;
  }

  vmap = (UvVertMap *)MEM_callocN(sizeof(*vmap), "UvVertMap");
  buf = vmap->buf = (UvMapVert *)MEM_callocN(sizeof(*vmap->buf) * size_t(totuv), "UvMapVert");
  vmap->vert = (UvMapVert **)MEM_callocN(sizeof(*vmap->vert) * totvert, "UvMapVert*");
  if (use_winding) {
    winding = static_cast<bool *>(MEM_callocN(sizeof(*winding) * totpoly, "winding"));
  }

  if (!vmap->vert || !vmap->buf) {
    BKE_mesh_uv_vert_map_free(vmap);
    return nullptr;
  }

  mp = mpoly;
  for (a = 0; a < totpoly; a++, mp++) {
    if (!selected || (!(hide_poly && hide_poly[a]) && (select_poly && select_poly[a]))) {
      float(*tf_uv)[2] = nullptr;

      if (use_winding) {
        tf_uv = (float(*)[2])BLI_buffer_reinit_data(&tf_uv_buf, vec2f, size_t(mp->totloop));
      }

      nverts = mp->totloop;

      for (i = 0; i < nverts; i++) {
        buf->loop_of_poly_index = ushort(i);
        buf->poly_index = a;
        buf->separate = false;
        buf->next = vmap->vert[mloop[mp->loopstart + i].v];
        vmap->vert[mloop[mp->loopstart + i].v] = buf;

        if (use_winding) {
          copy_v2_v2(tf_uv[i], mloopuv[mpoly[a].loopstart + i]);
        }

        buf++;
      }

      if (use_winding) {
        winding[a] = cross_poly_v2(tf_uv, uint(nverts)) > 0;
      }
    }
  }

  /* sort individual uvs for each vert */
  for (a = 0; a < totvert; a++) {
    UvMapVert *newvlist = nullptr, *vlist = vmap->vert[a];
    UvMapVert *iterv, *v, *lastv, *next;
    const float *uv, *uv2;
    float uvdiff[2];

    while (vlist) {
      v = vlist;
      vlist = vlist->next;
      v->next = newvlist;
      newvlist = v;

      uv = mloopuv[mpoly[v->poly_index].loopstart + v->loop_of_poly_index];
      lastv = nullptr;
      iterv = vlist;

      while (iterv) {
        next = iterv->next;

        uv2 = mloopuv[mpoly[iterv->poly_index].loopstart + iterv->loop_of_poly_index];
        sub_v2_v2v2(uvdiff, uv2, uv);

        if (fabsf(uv[0] - uv2[0]) < limit[0] && fabsf(uv[1] - uv2[1]) < limit[1] &&
            (!use_winding || winding[iterv->poly_index] == winding[v->poly_index])) {
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
                                              const MPoly *mpoly,
                                              const MLoop *mloop,
                                              int totvert,
                                              int totpoly,
                                              int totloop,
                                              const bool do_loops)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totvert), __func__);
  int *indices, *index_iter;
  int i, j;

  indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(totloop), __func__));
  index_iter = indices;

  /* Count number of polys for each vertex */
  for (i = 0; i < totpoly; i++) {
    const MPoly *p = &mpoly[i];

    for (j = 0; j < p->totloop; j++) {
      map[mloop[p->loopstart + j].v].count++;
    }
  }

  /* Assign indices mem */
  for (i = 0; i < totvert; i++) {
    map[i].indices = index_iter;
    index_iter += map[i].count;

    /* Reset 'count' for use as index in last loop */
    map[i].count = 0;
  }

  /* Find the users */
  for (i = 0; i < totpoly; i++) {
    const MPoly *p = &mpoly[i];

    for (j = 0; j < p->totloop; j++) {
      uint v = mloop[p->loopstart + j].v;

      map[v].indices[map[v].count] = do_loops ? p->loopstart + j : i;
      map[v].count++;
    }
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_vert_poly_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const MPoly *mpoly,
                                   const MLoop *mloop,
                                   int totvert,
                                   int totpoly,
                                   int totloop)
{
  mesh_vert_poly_or_loop_map_create(r_map, r_mem, mpoly, mloop, totvert, totpoly, totloop, false);
}

void BKE_mesh_vert_loop_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const MPoly *mpoly,
                                   const MLoop *mloop,
                                   int totvert,
                                   int totpoly,
                                   int totloop)
{
  mesh_vert_poly_or_loop_map_create(r_map, r_mem, mpoly, mloop, totvert, totpoly, totloop, true);
}

void BKE_mesh_vert_looptri_map_create(MeshElemMap **r_map,
                                      int **r_mem,
                                      const int totvert,
                                      const MLoopTri *mlooptri,
                                      const int totlooptri,
                                      const MLoop *mloop,
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
      map[mloop[mlt->tri[j]].v].count++;
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
      MeshElemMap *map_ele = &map[mloop[mlt->tri[j]].v];
      map_ele->indices[map_ele->count++] = i;
    }
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_vert_edge_map_create(
    MeshElemMap **r_map, int **r_mem, const MEdge *medge, int totvert, int totedge)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totvert), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int[2]) * size_t(totedge), __func__));
  int *i_pt = indices;

  int i;

  /* Count number of edges for each vertex */
  for (i = 0; i < totedge; i++) {
    map[medge[i].v1].count++;
    map[medge[i].v2].count++;
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
    const uint v[2] = {medge[i].v1, medge[i].v2};

    map[v[0]].indices[map[v[0]].count] = i;
    map[v[1]].indices[map[v[1]].count] = i;

    map[v[0]].count++;
    map[v[1]].count++;
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_vert_edge_vert_map_create(
    MeshElemMap **r_map, int **r_mem, const MEdge *medge, int totvert, int totedge)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totvert), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int[2]) * size_t(totedge), __func__));
  int *i_pt = indices;

  int i;

  /* Count number of edges for each vertex */
  for (i = 0; i < totedge; i++) {
    map[medge[i].v1].count++;
    map[medge[i].v2].count++;
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
    const uint v[2] = {medge[i].v1, medge[i].v2};

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
                                   const MEdge * /*medge*/,
                                   const int totedge,
                                   const MPoly *mpoly,
                                   const int totpoly,
                                   const MLoop *mloop,
                                   const int totloop)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totedge), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(totloop) * 2, __func__));
  int *index_step;
  const MPoly *mp;
  int i;

  /* count face users */
  for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
    const MLoop *ml;
    int j = mp->totloop;
    for (ml = &mloop[mp->loopstart]; j--; ml++) {
      map[ml->e].count += 2;
    }
  }

  /* create offsets */
  index_step = indices;
  for (i = 0; i < totedge; i++) {
    map[i].indices = index_step;
    index_step += map[i].count;

    /* re-count, using this as an index below */
    map[i].count = 0;
  }

  /* assign loop-edge users */
  for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
    const MLoop *ml;
    MeshElemMap *map_ele;
    const int max_loop = mp->loopstart + mp->totloop;
    int j = mp->loopstart;
    for (ml = &mloop[j]; j < max_loop; j++, ml++) {
      map_ele = &map[ml->e];
      map_ele->indices[map_ele->count++] = j;
      map_ele->indices[map_ele->count++] = j + 1;
    }
    /* last edge/loop of poly, must point back to first loop! */
    map_ele->indices[map_ele->count - 1] = mp->loopstart;
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_edge_poly_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const MEdge * /*medge*/,
                                   const int totedge,
                                   const MPoly *mpoly,
                                   const int totpoly,
                                   const MLoop *mloop,
                                   const int totloop)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(totedge), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(totloop), __func__));
  int *index_step;
  const MPoly *mp;
  int i;

  /* count face users */
  for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
    const MLoop *ml;
    int j = mp->totloop;
    for (ml = &mloop[mp->loopstart]; j--; ml++) {
      map[ml->e].count++;
    }
  }

  /* create offsets */
  index_step = indices;
  for (i = 0; i < totedge; i++) {
    map[i].indices = index_step;
    index_step += map[i].count;

    /* re-count, using this as an index below */
    map[i].count = 0;
  }

  /* assign poly-edge users */
  for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
    const MLoop *ml;
    int j = mp->totloop;
    for (ml = &mloop[mp->loopstart]; j--; ml++) {
      MeshElemMap *map_ele = &map[ml->e];
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
                                           const MPoly *mpoly,
                                           const int mpoly_num,
                                           const MLoopTri *looptri,
                                           const int looptri_num)
{
  MeshElemMap *map = MEM_cnew_array<MeshElemMap>(size_t(mpoly_num), __func__);
  int *indices = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(looptri_num), __func__));
  int *index_step;
  int i;

  /* create offsets */
  index_step = indices;
  for (i = 0; i < mpoly_num; i++) {
    map[i].indices = index_step;
    index_step += ME_POLY_TRI_TOT(&mpoly[i]);
  }

  /* assign poly-tessface users */
  for (i = 0; i < looptri_num; i++) {
    MeshElemMap *map_ele = &map[looptri[i].poly];
    map_ele->indices[map_ele->count++] = i;
  }

  *r_map = map;
  *r_mem = indices;
}

namespace blender::bke::mesh_topology {

Array<int> build_loop_to_poly_map(const Span<MPoly> polys, const int loops_num)
{
  Array<int> map(loops_num);
  threading::parallel_for(polys.index_range(), 1024, [&](IndexRange range) {
    for (const int64_t poly_i : range) {
      const MPoly &poly = polys[poly_i];
      map.as_mutable_span().slice(poly.loopstart, poly.totloop).fill(int(poly_i));
    }
  });
  return map;
}

Array<Vector<int>> build_vert_to_edge_map(const Span<MEdge> edges, const int verts_num)
{
  Array<Vector<int>> map(verts_num);
  for (const int64_t i : edges.index_range()) {
    map[edges[i].v1].append(int(i));
    map[edges[i].v2].append(int(i));
  }
  return map;
}

Array<Vector<int>> build_vert_to_poly_map(const Span<MPoly> polys,
                                          const Span<MLoop> loops,
                                          int verts_num)
{
  Array<Vector<int>> map(verts_num);
  for (const int64_t i : polys.index_range()) {
    const MPoly &poly = polys[i];
    for (const MLoop &loop : loops.slice(poly.loopstart, poly.totloop)) {
      map[loop.v].append(int(i));
    }
  }
  return map;
}

Array<Vector<int>> build_vert_to_loop_map(const Span<MLoop> loops, const int verts_num)
{
  Array<Vector<int>> map(verts_num);
  for (const int64_t i : loops.index_range()) {
    map[loops[i].v].append(int(i));
  }
  return map;
}

Array<Vector<int>> build_edge_to_loop_map(const Span<MLoop> loops, const int edges_num)
{
  Array<Vector<int>> map(edges_num);
  for (const int64_t i : loops.index_range()) {
    map[loops[i].e].append(int(i));
  }
  return map;
}

Vector<Vector<int>> build_edge_to_loop_map_resizable(const Span<MLoop> loops, const int edges_num)
{
  Vector<Vector<int>> map(edges_num);
  for (const int64_t i : loops.index_range()) {
    map[loops[i].e].append(int(i));
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
using MeshRemap_CheckIslandBoundary = bool (*)(const MPoly *mpoly,
                                               const MLoop *mloop,
                                               const MEdge *medge,
                                               const int edge_user_count,
                                               const MPoly *mpoly_array,
                                               const MeshElemMap *edge_poly_map,
                                               void *user_data);

static void poly_edge_loop_islands_calc(const MEdge *medge,
                                        const int totedge,
                                        const MPoly *mpoly,
                                        const int totpoly,
                                        const MLoop *mloop,
                                        const int totloop,
                                        MeshElemMap *edge_poly_map,
                                        const bool use_bitflags,
                                        MeshRemap_CheckIslandBoundary edge_boundary_check,
                                        void *edge_boundary_check_data,
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

  if (totpoly == 0) {
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
    BKE_mesh_edge_poly_map_create(
        &edge_poly_map, &edge_poly_mem, medge, totedge, mpoly, totpoly, mloop, totloop);
  }

  poly_groups = static_cast<int *>(MEM_callocN(sizeof(int) * size_t(totpoly), __func__));
  poly_stack = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(totpoly), __func__));

  while (true) {
    int poly;
    int bit_poly_group_mask = 0;
    int poly_group_id;
    int ps_curr_idx = 0, ps_end_idx = 0; /* stack indices */

    for (poly = poly_prev; poly < totpoly; poly++) {
      if (poly_groups[poly] == 0) {
        break;
      }
    }

    if (poly == totpoly) {
      /* all done */
      break;
    }

    poly_group_id = use_bitflags ? temp_poly_group_id : ++tot_group;

    /* start searching from here next time */
    poly_prev = poly + 1;

    poly_groups[poly] = poly_group_id;
    poly_stack[ps_end_idx++] = poly;

    while (ps_curr_idx != ps_end_idx) {
      const MPoly *mp;
      const MLoop *ml;
      int j;

      poly = poly_stack[ps_curr_idx++];
      BLI_assert(poly_groups[poly] == poly_group_id);

      mp = &mpoly[poly];
      for (ml = &mloop[mp->loopstart], j = mp->totloop; j--; ml++) {
        /* loop over poly users */
        const int me_idx = int(ml->e);
        const MEdge *me = &medge[me_idx];
        const MeshElemMap *map_ele = &edge_poly_map[me_idx];
        const int *p = map_ele->indices;
        int i = map_ele->count;
        if (!edge_boundary_check(mp, ml, me, i, mpoly, map_ele, edge_boundary_check_data)) {
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
          if (edge_borders && !BLI_BITMAP_TEST(edge_borders, me_idx)) {
            BLI_BITMAP_ENABLE(edge_borders, me_idx);
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
    int i = totpoly, *gid = poly_groups;
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

static bool poly_is_island_boundary_smooth_cb(const MPoly *mp,
                                              const MLoop * /*ml*/,
                                              const MEdge *me,
                                              const int edge_user_count,
                                              const MPoly *mpoly_array,
                                              const MeshElemMap *edge_poly_map,
                                              void * /*user_data*/)
{
  /* Edge is sharp if one of its polys is flat, or edge itself is sharp,
   * or edge is not used by exactly two polygons. */
  if ((mp->flag & ME_SMOOTH) && !(me->flag & ME_SHARP) && (edge_user_count == 2)) {
    /* In that case, edge appears to be smooth, but we need to check its other poly too. */
    const MPoly *mp_other = (mp == &mpoly_array[edge_poly_map->indices[0]]) ?
                                &mpoly_array[edge_poly_map->indices[1]] :
                                &mpoly_array[edge_poly_map->indices[0]];
    return (mp_other->flag & ME_SMOOTH) == 0;
  }
  return true;
}

int *BKE_mesh_calc_smoothgroups(const MEdge *medge,
                                const int totedge,
                                const MPoly *mpoly,
                                const int totpoly,
                                const MLoop *mloop,
                                const int totloop,
                                int *r_totgroup,
                                const bool use_bitflags)
{
  int *poly_groups = nullptr;

  poly_edge_loop_islands_calc(medge,
                              totedge,
                              mpoly,
                              totpoly,
                              mloop,
                              totloop,
                              nullptr,
                              use_bitflags,
                              poly_is_island_boundary_smooth_cb,
                              nullptr,
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

/* TODO: I'm not sure edge seam flag is enough to define UV islands?
 *       Maybe we should also consider UV-maps values
 *       themselves (i.e. different UV-edges for a same mesh-edge => boundary edge too?).
 *       Would make things much more complex though,
 *       and each UVMap would then need its own mesh mapping, not sure we want that at all!
 */
struct MeshCheckIslandBoundaryUv {
  const MLoop *loops;
  const float (*luvs)[2];
  const MeshElemMap *edge_loop_map;
};

static bool mesh_check_island_boundary_uv(const MPoly * /*mp*/,
                                          const MLoop *ml,
                                          const MEdge *me,
                                          const int /*edge_user_count*/,
                                          const MPoly * /*mpoly_array*/,
                                          const MeshElemMap * /*edge_poly_map*/,
                                          void *user_data)
{
  if (user_data) {
    const MeshCheckIslandBoundaryUv *data = static_cast<const MeshCheckIslandBoundaryUv *>(
        user_data);
    const MLoop *loops = data->loops;
    const float(*luvs)[2] = data->luvs;
    const MeshElemMap *edge_to_loops = &data->edge_loop_map[ml->e];

    BLI_assert(edge_to_loops->count >= 2 && (edge_to_loops->count % 2) == 0);

    const uint v1 = loops[edge_to_loops->indices[0]].v;
    const uint v2 = loops[edge_to_loops->indices[1]].v;
    const float *uvco_v1 = luvs[edge_to_loops->indices[0]];
    const float *uvco_v2 = luvs[edge_to_loops->indices[1]];
    for (int i = 2; i < edge_to_loops->count; i += 2) {
      if (loops[edge_to_loops->indices[i]].v == v1) {
        if (!equals_v2v2(uvco_v1, luvs[edge_to_loops->indices[i]]) ||
            !equals_v2v2(uvco_v2, luvs[edge_to_loops->indices[i + 1]])) {
          return true;
        }
      }
      else {
        BLI_assert(loops[edge_to_loops->indices[i]].v == v2);
        UNUSED_VARS_NDEBUG(v2);
        if (!equals_v2v2(uvco_v2, luvs[edge_to_loops->indices[i]]) ||
            !equals_v2v2(uvco_v1, luvs[edge_to_loops->indices[i + 1]])) {
          return true;
        }
      }
    }
    return false;
  }

  /* Edge is UV boundary if tagged as seam. */
  return (me->flag & ME_SEAM) != 0;
}

static bool mesh_calc_islands_loop_poly_uv(const MEdge *edges,
                                           const int totedge,
                                           const MPoly *polys,
                                           const int totpoly,
                                           const MLoop *loops,
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

  MeshCheckIslandBoundaryUv edge_boundary_check_data;

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

  int grp_idx, p_idx, pl_idx, l_idx;

  BKE_mesh_loop_islands_clear(r_island_store);
  BKE_mesh_loop_islands_init(
      r_island_store, MISLAND_TYPE_LOOP, totloop, MISLAND_TYPE_POLY, MISLAND_TYPE_EDGE);

  BKE_mesh_edge_poly_map_create(
      &edge_poly_map, &edge_poly_mem, edges, totedge, polys, totpoly, loops, totloop);

  if (luvs) {
    BKE_mesh_edge_loop_map_create(
        &edge_loop_map, &edge_loop_mem, edges, totedge, polys, totpoly, loops, totloop);
    edge_boundary_check_data.loops = loops;
    edge_boundary_check_data.luvs = luvs;
    edge_boundary_check_data.edge_loop_map = edge_loop_map;
  }

  poly_edge_loop_islands_calc(edges,
                              totedge,
                              polys,
                              totpoly,
                              loops,
                              totloop,
                              edge_poly_map,
                              false,
                              mesh_check_island_boundary_uv,
                              luvs ? &edge_boundary_check_data : nullptr,
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
      MEM_mallocN(sizeof(*poly_indices) * size_t(totpoly), __func__));
  loop_indices = static_cast<int *>(
      MEM_mallocN(sizeof(*loop_indices) * size_t(totloop), __func__));

  /* NOTE: here we ignore '0' invalid group - this should *never* happen in this case anyway? */
  for (grp_idx = 1; grp_idx <= num_poly_groups; grp_idx++) {
    num_pidx = num_lidx = 0;
    if (num_edge_borders) {
      num_einnercuts = 0;
      memset(edge_border_count, 0, sizeof(*edge_border_count) * size_t(totedge));
    }

    for (p_idx = 0; p_idx < totpoly; p_idx++) {
      if (poly_groups[p_idx] != grp_idx) {
        continue;
      }
      const MPoly *mp = &polys[p_idx];
      poly_indices[num_pidx++] = p_idx;
      for (l_idx = mp->loopstart, pl_idx = 0; pl_idx < mp->totloop; l_idx++, pl_idx++) {
        const MLoop *ml = &loops[l_idx];
        loop_indices[num_lidx++] = l_idx;
        if (num_edge_borders && BLI_BITMAP_TEST(edge_borders, ml->e) &&
            (edge_border_count[ml->e] < 2)) {
          edge_border_count[ml->e]++;
          if (edge_border_count[ml->e] == 2) {
            edge_innercut_indices[num_einnercuts++] = int(ml->e);
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
                                              const MEdge *edges,
                                              const int totedge,
                                              const MPoly *polys,
                                              const int totpoly,
                                              const MLoop *loops,
                                              const int totloop,
                                              MeshIslandStore *r_island_store)
{
  UNUSED_VARS(vert_positions, totvert);
  return mesh_calc_islands_loop_poly_uv(
      edges, totedge, polys, totpoly, loops, totloop, nullptr, r_island_store);
}

bool BKE_mesh_calc_islands_loop_poly_uvmap(float (*vert_positions)[3],
                                           const int totvert,
                                           MEdge *edges,
                                           const int totedge,
                                           MPoly *polys,
                                           const int totpoly,
                                           MLoop *loops,
                                           const int totloop,
                                           const float (*luvs)[2],
                                           MeshIslandStore *r_island_store)
{
  UNUSED_VARS(vert_positions, totvert);
  BLI_assert(luvs != nullptr);
  return mesh_calc_islands_loop_poly_uv(
      edges, totedge, polys, totpoly, loops, totloop, luvs, r_island_store);
}

/** \} */
