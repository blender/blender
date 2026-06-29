/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BMesh decimator that uses a grid un-subdivide method.
 */

#include <algorithm>
#include <optional>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"

#include "bmesh.hh"
#include "bmesh_decimate.hh" /* own include */

namespace blender {

/**
 * Vertices with more than this number of connected edges/faces are ignored,
 * they don't need to be classified/dissolved.
 */
constexpr int VERT_DISSOLVE_MAX = 4;

/** Classify the kind of dissolve the vertex may use. */
enum class VertDissolveMethod {
  /**
   * Surrounded by 3/4 manifold edges (each with 2 connected faces).
   *
   * Dissolves by clipping ears of connected faces into triangles, then joining them.
   */
  InteriorFan,
  /**
   * Surrounded by 2 edges, each connected to the same 2 faces.
   *
   * Dissolves by removing the vertex and connecting the two adjacent edges into a single edge.
   */
  InteriorChain,
  /**
   * Surrounded by 1 manifold edge (2 connected faces), and 2 boundary edges.
   *
   * Dissolves by clipping ears of connected faces into triangles, then joining them.
   */
  BoundaryFan,
  /**
   * Surrounded by 2 edges, not connected to any faces.
   *
   * Dissolves by removing the vertex and connecting the two adjacent edges into a single edge.
   */
  WireChain,
};

static int bm_vert_loops_from_vert_edges(const BMVert *v,
                                         BMEdge **v_edges,
                                         const int v_edges_num,
                                         BMLoop *v_loops[VERT_DISSOLVE_MAX])
{
  int v_loops_num = 0;
  for (int i = 0; i < v_edges_num; i++) {
    const BMEdge *e = v_edges[i];
    if (e->l != nullptr) {
      BMLoop *l_radial_iter = e->l;
      do {
        if (l_radial_iter->v == v) {
          /* Shouldn't be exceeded, more of a guard against corrupt topology. */
          if (v_loops_num == VERT_DISSOLVE_MAX) [[unlikely]] {
            BLI_assert_unreachable();
            return v_loops_num;
          }
          v_loops[v_loops_num++] = l_radial_iter;
        }
      } while ((l_radial_iter = l_radial_iter->radial_next) != e->l);
    }
  }
  return v_loops_num;
}

static bool bm_vert_dissolve_fan_makes_double_whole_face(BMLoop **v_loops_all,
                                                         const int v_loops_all_num)
{
  BMEdge *edges_from_loop_ear[VERT_DISSOLVE_MAX];
  uint edges_from_loop_ear_num = 0;

  for (int i = 0; i < v_loops_all_num; i++) {
    BMLoop *l = v_loops_all[i];
    BMEdge *e_opposite;
    if (l->f->len > 3) {
      e_opposite = BM_edge_exists(l->prev->v, l->next->v);
      if (e_opposite == nullptr) {
        return false;
      }
      if (edges_from_loop_ear_num == VERT_DISSOLVE_MAX) {
        return false;
      }
    }
    else {
      e_opposite = l->next->e;
    }
    edges_from_loop_ear[edges_from_loop_ear_num++] = e_opposite;
  }

  if (edges_from_loop_ear_num >= 3) {
    if (BM_face_exists_multi_edge(edges_from_loop_ear, edges_from_loop_ear_num)) {
      return true;
    }
  }

  return false;
}

/**
 * Check if splitting all triangle "ears", then dissolving the vertex would create duplicate faces.
 * Only possible duplicates with *existing* faces are accounted for.
 * See #bm_vert_dissolve_fan_makes_double_with_self which accounts for the creation of duplicates.
 *
 * \param v_loops_all: One loop per face around the vertex (each `l->v` is that vertex),
 * as gathered for triangulation.
 * \return true if the dissolve would create a duplicate face.
 */
static bool bm_vert_dissolve_fan_makes_double_with_existing(BMLoop **v_loops_all,
                                                            const int v_loops_all_num)
{
  for (int i = 0; i < v_loops_all_num; i++) {
    BMLoop *l = v_loops_all[i];
    if (l->f->len > 3) {
      if (BM_face_split_check_double_face(l->prev, l->next, 3)) {
        return true;
      }
    }
  }
  return false;
}

/**
 * Check if splitting all triangle "ears", then dissolving the vertex would create duplicate faces.
 *
 * Note that this is a little more specialized than it may seem because
 * we already check if splitting a face would create a duplicate face: *this* checks
 * the faces connected to the vertex don't create a duplicate against *each other*.
 *
 * The topology that does this ends up being fairly messy in practice - but legal.
 * The simplest case is 4 vertices storing 3 quads (one typical quad, and 2x bow-tie quads),
 * in this case collapsing one vertex would create 3x overlapping triangles.
 *
 * \param v_loops_all: One loop per face around the vertex (each `l->v` is that vertex),
 * as gathered for triangulation.
 * \return true if the dissolve would create a duplicate face.
 */
static bool bm_vert_dissolve_fan_makes_double_with_self(BMLoop **v_loops_all,
                                                        const int v_loops_all_num)
{
  BLI_assert(v_loops_all_num <= VERT_DISSOLVE_MAX);
  if (v_loops_all_num < 2) {
    return false;
  }

  BMLoop *v_loops[VERT_DISSOLVE_MAX];
  int v_loops_num = 0;
  for (int i = 0; i < v_loops_all_num; i++) {
    BLI_assert(v_loops_all[i]->v == v_loops_all[0]->v);
    BMLoop *l = v_loops_all[i];
    if (l->f->len > 3) {
      v_loops[v_loops_num++] = l;
    }
  }
  if (v_loops_num < 2) {
    return false;
  }

  std::ranges::sort(v_loops, v_loops + v_loops_num, {}, [](const BMLoop *l) { return l->f->len; });

  using VertArray = Array<BMVert *, BM_DEFAULT_NGON_STACK_SIZE>;

  /* Return the vertices left forming a face when the central vertex is collapsed out,
   * canonicalized so two faces that collapse to the same loop compare equal. */
  auto verts_canonical_collapsed_fn = [](const BMLoop *l_vert) -> VertArray {
    const int verts_num = l_vert->f->len - 1;
    VertArray verts(verts_num, NoInitialization());
    int verts_index = 0;
    for (const BMLoop *l = l_vert->next; l != l_vert; l = l->next) {
      verts[verts_index++] = l->v;
    }
    BLI_assert(verts_index == verts_num);

    /* Canonicalize using the minimum pointer, with the order
     * defined by the next adjacent minimum. */
    int i_min = 0;
    for (int i = 1; i < verts_num; i++) {
      if (verts[i] < verts[i_min]) {
        i_min = i;
      }
    }
    const int i_next = (i_min + 1 == verts_num) ? 0 : i_min + 1;
    const int i_prev = (i_min == 0) ? verts_num - 1 : i_min - 1;
    /* A step of `verts_num - 1` moves one place backward & wraps. */
    const int step = (verts[i_next] < verts[i_prev]) ? 1 : (verts_num - 1);

    VertArray verts_ordered(verts_num, NoInitialization());
    for (int i = 0, j = i_min; i < verts_num; i++) {
      verts_ordered[i] = verts[j];
      j += step;
      if (j >= verts_num) {
        j -= verts_num;
      }
    }
    return verts_ordered;
  };

  /* Only perform the relatively expensive unique face checks for
   * groups of faces with matching numbers of sides. */
  for (int group_beg = 0; group_beg < v_loops_num;) {
    const int group_f_len = v_loops[group_beg]->f->len;
    int group_end = group_beg + 1;
    while (group_end < v_loops_num && v_loops[group_end]->f->len == group_f_len) {
      group_end++;
    }

    const int group_len = group_end - group_beg;
    if (group_len >= 2) {
      VertArray collapsed[VERT_DISSOLVE_MAX];
      for (int i = 0; i < group_len; i++) {
        collapsed[i] = verts_canonical_collapsed_fn(v_loops[group_beg + i]);
      }
      for (int i = 0; i + 1 < group_len; i++) {
        for (int j = i + 1; j < group_len; j++) {
          if (std::ranges::equal(collapsed[i], collapsed[j])) {
            return true;
          }
        }
      }
    }
    group_beg = group_end;
  }
  return false;
}

/**
 * Detect if `v` can be dissolved as a fan or a chain based on its topology.
 *
 * \param check_for_duplicates: When true, extensive tests are performed that ensure
 * dissolving won't create duplicate faces.
 * Otherwise perform a simple topology check.
 * Avoid the expensive check on the initial pass to mark vertices as candidates to dissolve
 * because it's slow and the exact topology will change as vertices begin to dissolve.
 * \param v_loops, v_loops_num_p: Surrounding loops, set when `check_for_duplicates`
 * is true and the return value isn't `nullopt`.
 *
 * \note It's important this *only* dissolves `v`, otherwise
 * the main dissolve loop may iterate over freed vertices.
 * Enabling `check_for_duplicates` ensures this.
 */
static std::optional<VertDissolveMethod> bm_vert_dissolve_fan_or_chain_test(
    BMVert *v,
    const bool check_for_duplicates,
    BMLoop *v_loops[VERT_DISSOLVE_MAX],
    int *v_loops_num_p)
{
  BMIter iter;
  BMEdge *e;

  /* All edges connected to `v`. */
  BMEdge *v_edges[VERT_DISSOLVE_MAX];

  /* Boundary edges, only check for 2 because cases with more than 2 are ignored. */
  BMEdge *e_boundary[2];
  uint e_boundary_num = 0;

  uint tot_edge = 0;
  uint tot_edge_boundary = 0;
  uint tot_edge_manifold = 0;
  uint tot_edge_wire = 0;

  /* In this loop we try to bail out early and gather data to be used later.
   * Avoid expensive queries up front. */
  BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
    if (BM_edge_is_boundary(e)) {
      if (e_boundary_num < ARRAY_SIZE(e_boundary)) {
        e_boundary[e_boundary_num] = e;
      }
      e_boundary_num++;
      tot_edge_boundary++;
    }
    else if (BM_edge_is_manifold(e)) {
      tot_edge_manifold++;
    }
    else if (BM_edge_is_wire(e)) {
      tot_edge_wire++;
    }
    else {
      return std::nullopt;
    }

    if (tot_edge == VERT_DISSOLVE_MAX) {
      return std::nullopt;
    }

    v_edges[tot_edge] = e;
    tot_edge++;
  }

  if (((tot_edge == 4) && (tot_edge_boundary == 0) && (tot_edge_manifold == 4)) ||
      ((tot_edge == 3) && (tot_edge_boundary == 0) && (tot_edge_manifold == 3)))
  {
    if (check_for_duplicates) {
      const int v_loops_num = bm_vert_loops_from_vert_edges(v, v_edges, tot_edge, v_loops);

      /* Check the corners around `v` don't already form a face. */
      if (bm_vert_dissolve_fan_makes_double_whole_face(v_loops, v_loops_num)) {
        return std::nullopt;
      }

      /* Check if splits would create a duplicate (with existing or between each other). */
      if (bm_vert_dissolve_fan_makes_double_with_existing(v_loops, v_loops_num) ||
          bm_vert_dissolve_fan_makes_double_with_self(v_loops, v_loops_num))
      {
        return std::nullopt;
      }

      *v_loops_num_p = v_loops_num;
    }
    return VertDissolveMethod::InteriorFan;
  }
  if ((tot_edge == 3) && (tot_edge_boundary == 2) && (tot_edge_manifold == 1)) {
    if (check_for_duplicates) {
      BLI_assert(e_boundary_num == 2);

      if (const BMEdge *e_span = BM_edge_exists(BM_edge_other_vert(e_boundary[0], v),
                                                BM_edge_other_vert(e_boundary[1], v)))
      {
        if (e_span->l) {
          /* Boundary dissolving connects the two boundary edges into `e_span`.
           * When `e_span` exists with a face, the dissolve may join across it.
           * In isolation that is "ok" - however it means the dissolve may remove vertices
           * other than `v`, causing the main un-subdivide loop to operate on freed geometry.
           *
           * NOTE(@ideasman42): If this was an important use case it's possible to inspect the
           * topology and only reject dissolving when it would cause problems.
           * However the situations where this occurs are not so important to support
           * as they tend to be caused by overlapping/bow-tie faces.
           * So it's simpler to reject them. */
          return std::nullopt;
        }
      }

      const int v_loops_num = bm_vert_loops_from_vert_edges(v, v_edges, tot_edge, v_loops);

      /* Check if splits would create a duplicate (with existing or between each other). */
      if (bm_vert_dissolve_fan_makes_double_with_existing(v_loops, v_loops_num) ||
          bm_vert_dissolve_fan_makes_double_with_self(v_loops, v_loops_num))
      {
        return std::nullopt;
      }

      *v_loops_num_p = v_loops_num;
    }
    return VertDissolveMethod::BoundaryFan;
  }
  if ((tot_edge == 2) && (tot_edge_wire == 2)) {
    return VertDissolveMethod::WireChain;
  }
  if ((tot_edge == 2) && (tot_edge_manifold == 2)) {
    return VertDissolveMethod::InteriorChain;
  }
  return std::nullopt;
}

/**
 * Dissolve `v` (and only `v`).
 */
static bool bm_vert_dissolve_fan_or_chain(BMesh *bm, BMVert *v)
{
  /* Re-test if dissolve is possible.
   * The surrounding geometry may have changed since the first check,
   * the first call also skips expensive degeneracy checks for this very reason. */
  BMLoop *v_loops[VERT_DISSOLVE_MAX];
  int v_loops_num = 0;
  const std::optional<VertDissolveMethod> dissolve = bm_vert_dissolve_fan_or_chain_test(
      v, true, v_loops, &v_loops_num);
  if (!dissolve) {
    return false;
  }

  switch (*dissolve) {
    case VertDissolveMethod::WireChain:
    case VertDissolveMethod::InteriorChain: {
      return (BM_vert_collapse_edge(bm, v->e, v, true, true, true) != nullptr);
    }
    case VertDissolveMethod::BoundaryFan:
    case VertDissolveMethod::InteriorFan: {
      for (int i = 0; i < v_loops_num; i++) {
        BMLoop *l = v_loops[i];
        if (l->f->len > 3) {
          BMLoop *l_new;
          BLI_assert(l->prev->v != l->next->v);
          /* Must have been rejected by #bm_vert_dissolve_fan_or_chain_test.
           * NOTE(@ideasman42): Assert because it's theoretically possible an edit
           * in this loop creates a duplicate (although incredibly unlikely). */
          BLI_assert(!BM_face_split_check_double_face(l->prev, l->next, 3));
          BM_face_split(bm, l->f, l->prev, l->next, &l_new, nullptr, true);
          BM_elem_flag_merge_into(l_new->e, l->e, l->prev->e);
        }
      }
      return BM_vert_dissolve(bm, v);
    }
  }

  return false;
}

/**
 * Note that #bm_tag_untagged_neighbors requires #VERT_INDEX_DO_COLLAPSE & #VERT_INDEX_IGNORE
 * are equal magnitude, opposite sign.
 */
enum {
  VERT_INDEX_DO_COLLAPSE = -1,
  VERT_INDEX_INIT = 0,
  VERT_INDEX_IGNORE = 1,
};

/**
 * Given a set of starting verts, find all the currently-untagged neighbors of those verts, tag
 * them with the specified value, and return an array specifying all the newly-tagged verts.
 *
 * By using two arrays and two tag values, repeated alternating calls will expand the selection in
 * an alternating tagging pattern. Dissolving one of the two tags will then reduce the density of
 * the mesh, by half, in a regular diamond pattern.
 *
 * \param verts_start: The array of starting verts whose neighbors should be tagged.
 * \param verts_start_num: The number of verts in the verts_start array.
 * \param desired_tag: The value to set as a tag, on any currently-untagged neighbors.
 * \param r_verts_tagged: Returned array of all the verts which were tagged in this call.
 * \param r_verts_tagged_num: Returned number of verts in the r_verts_tagged array.
 */
static void bm_tag_untagged_neighbors(BMVert *verts_start[],
                                      const uint verts_start_num,
                                      const int desired_tag,
                                      BMVert *r_verts_tagged[],
                                      uint &r_verts_tagged_num)
{

  BMEdge *e;
  BMIter iter;
  r_verts_tagged_num = 0;

  for (int i = 0; i < verts_start_num; i++) {
    BMVert *v = verts_start[i];

    /* Since DO_COLLAPSE and IGNORE are -1 and +1, inverting the sign finds the other. */
    BLI_assert(BM_elem_index_get(v) == -desired_tag);

    BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
      BMVert *v_other = BM_edge_other_vert(e, v);
      if (BM_elem_index_get(v_other) == VERT_INDEX_INIT) {
        BM_elem_index_set(v_other, desired_tag); /* set_dirty! */
        r_verts_tagged[r_verts_tagged_num++] = v_other;
      }
    }
  }
}

/* - BMVert.flag & BM_ELEM_TAG:  shows we touched this vert
 * - BMVert.index == -1:         shows we will remove this vert
 */

void BM_mesh_decimate_unsubdivide_ex(BMesh *bm, const int iterations, const bool tag_only)
{
  /* NOTE: while #BMWalker seems like a logical choice, it results in uneven geometry. */

  BMVert **verts_collapse = MEM_new_array_uninitialized<BMVert *>(bm->totvert, __func__);
  BMVert **verts_ignore = MEM_new_array_uninitialized<BMVert *>(bm->totvert, __func__);
  uint verts_collapse_num = 0;
  uint verts_ignore_num = 0;

  BMIter iter;

  int iter_step;

  /* if tag_only is set, we assume the caller knows what verts to tag
   * needed for the operator */
  if (tag_only == false) {
    BMVert *v;
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      BM_elem_flag_enable(v, BM_ELEM_TAG);
    }
  }

  /* Perform the number of iteration steps which the user requested. */
  for (iter_step = 0; iter_step < iterations; iter_step++) {
    BMVert *v, *v_next;
    bool verts_were_marked_for_dissolve = false;

    /* Tag all verts which are eligible to be dissolved on this iteration. */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG) &&
          bm_vert_dissolve_fan_or_chain_test(v, false, nullptr, nullptr))
      {
        BM_elem_index_set(v, VERT_INDEX_INIT); /* set_dirty! */
      }
      else {
        BM_elem_index_set(v, VERT_INDEX_IGNORE); /* set_dirty! */
      }
    }

    /* main loop, keep tagging until we can't tag any more islands */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {

      /* Only process verts which are eligible for dissolve and which have not yet been tagged. */
      if (!(BM_elem_index_get(v) == VERT_INDEX_INIT)) {
        continue;
      }

      /* Set the first #VERT_INDEX_INIT vert as the first starting vert. */
      BM_elem_index_set(v, VERT_INDEX_IGNORE); /* set_dirty! */
      verts_ignore[0] = v;
      verts_ignore_num = 1;

      /* Starting at v, expand outwards, tagging any currently untagged neighbors.
       * verts will be alternately tagged for collapse or ignore.
       * Stop when there are no neighbors left to expand to. */
      while (true) {

        bm_tag_untagged_neighbors(verts_ignore,
                                  verts_ignore_num,
                                  VERT_INDEX_DO_COLLAPSE, /* set_dirty! */
                                  verts_collapse,
                                  verts_collapse_num);
        if (verts_collapse_num == 0) {
          break;
        }
        verts_were_marked_for_dissolve = true;

        bm_tag_untagged_neighbors(verts_collapse,
                                  verts_collapse_num,
                                  VERT_INDEX_IGNORE, /* set_dirty! */
                                  verts_ignore,
                                  verts_ignore_num);
        if (verts_ignore_num == 0) {
          break;
        }
      }
    }

    /* At high iteration levels, later steps can run out of verts that are eligible for dissolve.
     * If this occurs, stop. Future iterations won't find any verts that this iteration didn't. */
    if (!verts_were_marked_for_dissolve) {
      break;
    }

    /* Remove all verts tagged for removal. */
    BM_ITER_MESH_MUTABLE (v, v_next, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_index_get(v) == VERT_INDEX_DO_COLLAPSE) {
        bm_vert_dissolve_fan_or_chain(bm, v);
      }
    }
  }

  /* Ensure the vert index values will be recomputed. */
  bm->elem_index_dirty |= BM_VERT;

  MEM_delete(verts_collapse);
  MEM_delete(verts_ignore);
}

void BM_mesh_decimate_unsubdivide(BMesh *bm, const int iterations)
{
  BM_mesh_decimate_unsubdivide_ex(bm, iterations, false);
}

}  // namespace blender
