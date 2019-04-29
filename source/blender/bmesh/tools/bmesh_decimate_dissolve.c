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

/** \file
 * \ingroup bmesh
 *
 * BMesh decimator that dissolves flat areas into polygons (ngons).
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_heap.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "bmesh_decimate.h" /* own include */

/* check that collapsing a vertex between 2 edges doesn't cause a degenerate face. */
#define USE_DEGENERATE_CHECK

#define COST_INVALID FLT_MAX

/* multiply vertex edge angle by face angle
 * this means we are not left with sharp corners between _almost_ planer faces
 * convert angles [0-PI/2] -> [0-1], multiply together, then convert back to radians. */
static float bm_vert_edge_face_angle(BMVert *v)
{
#define UNIT_TO_ANGLE DEG2RADF(90.0f)
#define ANGLE_TO_UNIT (1.0f / UNIT_TO_ANGLE)

  const float angle = BM_vert_calc_edge_angle(v);
  /* note: could be either edge, it doesn't matter */
  if (v->e && BM_edge_is_manifold(v->e)) {
    return ((angle * ANGLE_TO_UNIT) * (BM_edge_calc_face_angle(v->e) * ANGLE_TO_UNIT)) *
           UNIT_TO_ANGLE;
  }
  else {
    return angle;
  }

#undef UNIT_TO_ANGLE
#undef ANGLE_TO_UNIT
}

struct DelimitData {
  int cd_loop_type;
  int cd_loop_size;
  int cd_loop_offset;
  int cd_loop_offset_end;
};

static bool bm_edge_is_contiguous_loop_cd_all(const BMEdge *e,
                                              const struct DelimitData *delimit_data)
{
  int cd_loop_offset;
  for (cd_loop_offset = delimit_data->cd_loop_offset;
       cd_loop_offset < delimit_data->cd_loop_offset_end;
       cd_loop_offset += delimit_data->cd_loop_size) {
    if (BM_edge_is_contiguous_loop_cd(e, delimit_data->cd_loop_type, cd_loop_offset) == false) {
      return false;
    }
  }

  return true;
}

static float bm_edge_calc_dissolve_error(const BMEdge *e,
                                         const BMO_Delimit delimit,
                                         const struct DelimitData *delimit_data)
{
  if (!BM_edge_is_manifold(e)) {
    goto fail;
  }

  if ((delimit & BMO_DELIM_SEAM) && (BM_elem_flag_test(e, BM_ELEM_SEAM))) {
    goto fail;
  }

  if ((delimit & BMO_DELIM_SHARP) && (BM_elem_flag_test(e, BM_ELEM_SMOOTH) == 0)) {
    goto fail;
  }

  if ((delimit & BMO_DELIM_MATERIAL) && (e->l->f->mat_nr != e->l->radial_next->f->mat_nr)) {
    goto fail;
  }

  const bool is_contig = BM_edge_is_contiguous(e);

  if ((delimit & BMO_DELIM_NORMAL) && (is_contig == false)) {
    goto fail;
  }

  if ((delimit & BMO_DELIM_UV) && (bm_edge_is_contiguous_loop_cd_all(e, delimit_data) == 0)) {
    goto fail;
  }

  float angle_cos_neg = dot_v3v3(e->l->f->no, e->l->radial_next->f->no);
  if (is_contig) {
    angle_cos_neg *= -1;
  }

  return angle_cos_neg;

fail:
  return COST_INVALID;
}

#ifdef USE_DEGENERATE_CHECK

static void mul_v2_m3v3_center(float r[2], float m[3][3], const float a[3], const float center[3])
{
  BLI_assert(r != a);
  BLI_assert(r != center);

  float co[3];
  sub_v3_v3v3(co, a, center);

  r[0] = m[0][0] * co[0] + m[1][0] * co[1] + m[2][0] * co[2];
  r[1] = m[0][1] * co[0] + m[1][1] * co[1] + m[2][1] * co[2];
}

static bool bm_loop_collapse_is_degenerate(BMLoop *l_ear)
{
  /* calculate relative to the centeral vertex for higher precision */
  const float *center = l_ear->v->co;

  float tri_2d[3][2];
  float axis_mat[3][3];

  axis_dominant_v3_to_m3(axis_mat, l_ear->f->no);

  {
    mul_v2_m3v3_center(tri_2d[0], axis_mat, l_ear->prev->v->co, center);
#  if 0
    mul_v2_m3v3_center(tri_2d[1], axis_mat, l_ear->v->co, center);
#  else
    zero_v2(tri_2d[1]);
#  endif
    mul_v2_m3v3_center(tri_2d[2], axis_mat, l_ear->next->v->co, center);
  }

  /* check we're not flipping face corners before or after the ear */
  {
    float adjacent_2d[2];

    if (!BM_vert_is_edge_pair(l_ear->prev->v)) {
      mul_v2_m3v3_center(adjacent_2d, axis_mat, l_ear->prev->prev->v->co, center);
      if (signum_i(cross_tri_v2(adjacent_2d, tri_2d[0], tri_2d[1])) !=
          signum_i(cross_tri_v2(adjacent_2d, tri_2d[0], tri_2d[2]))) {
        return true;
      }
    }

    if (!BM_vert_is_edge_pair(l_ear->next->v)) {
      mul_v2_m3v3_center(adjacent_2d, axis_mat, l_ear->next->next->v->co, center);
      if (signum_i(cross_tri_v2(adjacent_2d, tri_2d[2], tri_2d[1])) !=
          signum_i(cross_tri_v2(adjacent_2d, tri_2d[2], tri_2d[0]))) {
        return true;
      }
    }
  }

  /* check no existing verts are inside the triangle */
  {
    /* triangle may be concave, if so - flip so we can use clockwise check */
    if (cross_tri_v2(UNPACK3(tri_2d)) < 0.0f) {
      swap_v2_v2(tri_2d[1], tri_2d[2]);
    }

    /* skip l_ear and adjacent verts */
    BMLoop *l_iter, *l_first;

    l_iter = l_ear->next->next;
    l_first = l_ear->prev;
    do {
      float co_2d[2];
      mul_v2_m3v3_center(co_2d, axis_mat, l_iter->v->co, center);
      if (isect_point_tri_v2_cw(co_2d, tri_2d[0], tri_2d[1], tri_2d[2])) {
        return true;
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  return false;
}

static bool bm_vert_collapse_is_degenerate(BMVert *v)
{
  BMEdge *e_pair[2];
  BMVert *v_pair[2];

  if (BM_vert_edge_pair(v, &e_pair[0], &e_pair[1])) {

    /* allow wire edges */
    if (BM_edge_is_wire(e_pair[0]) || BM_edge_is_wire(e_pair[1])) {
      return false;
    }

    v_pair[0] = BM_edge_other_vert(e_pair[0], v);
    v_pair[1] = BM_edge_other_vert(e_pair[1], v);

    if (fabsf(cos_v3v3v3(v_pair[0]->co, v->co, v_pair[1]->co)) < (1.0f - FLT_EPSILON)) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = e_pair[1]->l;
      do {
        if (l_iter->f->len > 3) {
          BMLoop *l_pivot = (l_iter->v == v ? l_iter : l_iter->next);
          BLI_assert(v == l_pivot->v);
          if (bm_loop_collapse_is_degenerate(l_pivot)) {
            return true;
          }
        }
      } while ((l_iter = l_iter->radial_next) != l_first);
    }
    return false;
  }
  else {
    return true;
  }
}
#endif /* USE_DEGENERATE_CHECK */

void BM_mesh_decimate_dissolve_ex(BMesh *bm,
                                  const float angle_limit,
                                  const bool do_dissolve_boundaries,
                                  BMO_Delimit delimit,
                                  BMVert **vinput_arr,
                                  const int vinput_len,
                                  BMEdge **einput_arr,
                                  const int einput_len,
                                  const short oflag_out)
{
  const float angle_limit_cos_neg = -cosf(angle_limit);
  struct DelimitData delimit_data = {0};
  const int eheap_table_len = do_dissolve_boundaries ? einput_len : max_ii(einput_len, vinput_len);
  void *_heap_table = MEM_mallocN(sizeof(HeapNode *) * eheap_table_len, __func__);

  int i;

  if (delimit & BMO_DELIM_UV) {
    const int layer_len = CustomData_number_of_layers(&bm->ldata, CD_MLOOPUV);
    if (layer_len == 0) {
      delimit &= ~BMO_DELIM_UV;
    }
    else {
      delimit_data.cd_loop_type = CD_MLOOPUV;
      delimit_data.cd_loop_size = CustomData_sizeof(delimit_data.cd_loop_type);
      delimit_data.cd_loop_offset = CustomData_get_n_offset(&bm->ldata, CD_MLOOPUV, 0);
      delimit_data.cd_loop_offset_end = delimit_data.cd_loop_size * layer_len;
    }
  }

  /* --- first edges --- */
  if (1) {
    BMEdge **earray;
    Heap *eheap;
    HeapNode **eheap_table = _heap_table;
    HeapNode *enode_top;
    int *vert_reverse_lookup;
    BMIter iter;
    BMEdge *e_iter;

    /* --- setup heap --- */
    eheap = BLI_heap_new_ex(einput_len);

    /* wire -> tag */
    BM_ITER_MESH (e_iter, &iter, bm, BM_EDGES_OF_MESH) {
      BM_elem_flag_set(e_iter, BM_ELEM_TAG, BM_edge_is_wire(e_iter));
      BM_elem_index_set(e_iter, -1); /* set dirty */
    }
    bm->elem_index_dirty |= BM_EDGE;

    /* build heap */
    for (i = 0; i < einput_len; i++) {
      BMEdge *e = einput_arr[i];
      const float cost = bm_edge_calc_dissolve_error(e, delimit, &delimit_data);
      eheap_table[i] = BLI_heap_insert(eheap, cost, e);
      BM_elem_index_set(e, i); /* set dirty */
    }

    while ((BLI_heap_is_empty(eheap) == false) &&
           (BLI_heap_node_value((enode_top = BLI_heap_top(eheap))) < angle_limit_cos_neg)) {
      BMFace *f_new = NULL;
      BMEdge *e;

      e = BLI_heap_node_ptr(enode_top);
      i = BM_elem_index_get(e);

      if (BM_edge_is_manifold(e)) {
        f_new = BM_faces_join_pair(bm, e->l, e->l->radial_next, false);

        if (f_new) {
          BMLoop *l_first, *l_iter;

          BLI_heap_remove(eheap, enode_top);
          eheap_table[i] = NULL;

          /* update normal */
          BM_face_normal_update(f_new);
          if (oflag_out) {
            BMO_face_flag_enable(bm, f_new, oflag_out);
          }

          /* re-calculate costs */
          l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
          do {
            const int j = BM_elem_index_get(l_iter->e);
            if (j != -1 && eheap_table[j]) {
              const float cost = bm_edge_calc_dissolve_error(l_iter->e, delimit, &delimit_data);
              BLI_heap_node_value_update(eheap, eheap_table[j], cost);
            }
          } while ((l_iter = l_iter->next) != l_first);
        }
        else {
          BMO_error_clear(bm);
        }
      }

      if (UNLIKELY(f_new == NULL)) {
        BLI_heap_node_value_update(eheap, enode_top, COST_INVALID);
      }
    }

    /* prepare for cleanup */
    BM_mesh_elem_index_ensure(bm, BM_VERT);
    vert_reverse_lookup = MEM_mallocN(sizeof(int) * bm->totvert, __func__);
    copy_vn_i(vert_reverse_lookup, bm->totvert, -1);
    for (i = 0; i < vinput_len; i++) {
      BMVert *v = vinput_arr[i];
      vert_reverse_lookup[BM_elem_index_get(v)] = i;
    }

    /* --- cleanup --- */
    earray = MEM_mallocN(sizeof(BMEdge *) * bm->totedge, __func__);
    BM_ITER_MESH_INDEX (e_iter, &iter, bm, BM_EDGES_OF_MESH, i) {
      earray[i] = e_iter;
    }
    /* Remove all edges/verts left behind from dissolving,
     * NULL'ing the vertex array so we dont re-use. */
    for (i = bm->totedge - 1; i != -1; i--) {
      e_iter = earray[i];

      if (BM_edge_is_wire(e_iter) && (BM_elem_flag_test(e_iter, BM_ELEM_TAG) == false)) {
        /* edge has become wire */
        int vidx_reverse;
        BMVert *v1 = e_iter->v1;
        BMVert *v2 = e_iter->v2;
        BM_edge_kill(bm, e_iter);
        if (v1->e == NULL) {
          vidx_reverse = vert_reverse_lookup[BM_elem_index_get(v1)];
          if (vidx_reverse != -1) {
            vinput_arr[vidx_reverse] = NULL;
          }
          BM_vert_kill(bm, v1);
        }
        if (v2->e == NULL) {
          vidx_reverse = vert_reverse_lookup[BM_elem_index_get(v2)];
          if (vidx_reverse != -1) {
            vinput_arr[vidx_reverse] = NULL;
          }
          BM_vert_kill(bm, v2);
        }
      }
    }
    MEM_freeN(vert_reverse_lookup);
    MEM_freeN(earray);

    BLI_heap_free(eheap, NULL);
  }

  /* --- second verts --- */
  if (do_dissolve_boundaries) {
    /* simple version of the branch below, since we will dissolve _all_ verts that use 2 edges */
    for (i = 0; i < vinput_len; i++) {
      BMVert *v = vinput_arr[i];
      if (LIKELY(v != NULL) && BM_vert_is_edge_pair(v)) {
        BM_vert_collapse_edge(bm, v->e, v, true, true); /* join edges */
      }
    }
  }
  else {
    Heap *vheap;
    HeapNode **vheap_table = _heap_table;
    HeapNode *vnode_top;

    BMVert *v_iter;
    BMIter iter;

    BM_ITER_MESH (v_iter, &iter, bm, BM_VERTS_OF_MESH) {
      BM_elem_index_set(v_iter, -1); /* set dirty */
    }
    bm->elem_index_dirty |= BM_VERT;

    vheap = BLI_heap_new_ex(vinput_len);

    for (i = 0; i < vinput_len; i++) {
      BMVert *v = vinput_arr[i];
      if (LIKELY(v != NULL)) {
        const float cost = bm_vert_edge_face_angle(v);
        vheap_table[i] = BLI_heap_insert(vheap, cost, v);
        BM_elem_index_set(v, i); /* set dirty */
      }
    }

    while ((BLI_heap_is_empty(vheap) == false) &&
           (BLI_heap_node_value((vnode_top = BLI_heap_top(vheap))) < angle_limit)) {
      BMEdge *e_new = NULL;
      BMVert *v;

      v = BLI_heap_node_ptr(vnode_top);
      i = BM_elem_index_get(v);

      if (
#ifdef USE_DEGENERATE_CHECK
          !bm_vert_collapse_is_degenerate(v)
#else
          BM_vert_is_edge_pair(v)
#endif
      ) {
        e_new = BM_vert_collapse_edge(bm, v->e, v, true, true); /* join edges */

        if (e_new) {

          BLI_heap_remove(vheap, vnode_top);
          vheap_table[i] = NULL;

          /* update normal */
          if (e_new->l) {
            BMLoop *l_first, *l_iter;
            l_iter = l_first = e_new->l;
            do {
              BM_face_normal_update(l_iter->f);
            } while ((l_iter = l_iter->radial_next) != l_first);
          }

          /* re-calculate costs */
          BM_ITER_ELEM (v_iter, &iter, e_new, BM_VERTS_OF_EDGE) {
            const int j = BM_elem_index_get(v_iter);
            if (j != -1 && vheap_table[j]) {
              const float cost = bm_vert_edge_face_angle(v_iter);
              BLI_heap_node_value_update(vheap, vheap_table[j], cost);
            }
          }

#ifdef USE_DEGENERATE_CHECK
          /* dissolving a vertex may mean vertices we previously weren't able to dissolve
           * can now be re-evaluated. */
          if (e_new->l) {
            BMLoop *l_first, *l_iter;
            l_iter = l_first = e_new->l;
            do {
              /* skip vertices part of this edge, evaluated above */
              BMLoop *l_cycle_first, *l_cycle_iter;
              l_cycle_iter = l_iter->next->next;
              l_cycle_first = l_iter->prev;
              do {
                const int j = BM_elem_index_get(l_cycle_iter->v);
                if (j != -1 && vheap_table[j] &&
                    (BLI_heap_node_value(vheap_table[j]) == COST_INVALID)) {
                  const float cost = bm_vert_edge_face_angle(l_cycle_iter->v);
                  BLI_heap_node_value_update(vheap, vheap_table[j], cost);
                }
              } while ((l_cycle_iter = l_cycle_iter->next) != l_cycle_first);

            } while ((l_iter = l_iter->radial_next) != l_first);
          }
#endif /* USE_DEGENERATE_CHECK */
        }
      }

      if (UNLIKELY(e_new == NULL)) {
        BLI_heap_node_value_update(vheap, vnode_top, COST_INVALID);
      }
    }

    BLI_heap_free(vheap, NULL);
  }

  MEM_freeN(_heap_table);
}

void BM_mesh_decimate_dissolve(BMesh *bm,
                               const float angle_limit,
                               const bool do_dissolve_boundaries,
                               const BMO_Delimit delimit)
{
  int vinput_len;
  int einput_len;

  BMVert **vinput_arr = BM_iter_as_arrayN(bm, BM_VERTS_OF_MESH, NULL, &vinput_len, NULL, 0);
  BMEdge **einput_arr = BM_iter_as_arrayN(bm, BM_EDGES_OF_MESH, NULL, &einput_len, NULL, 0);

  BM_mesh_decimate_dissolve_ex(bm,
                               angle_limit,
                               do_dissolve_boundaries,
                               delimit,
                               vinput_arr,
                               vinput_len,
                               einput_arr,
                               einput_len,
                               0);

  MEM_freeN(vinput_arr);
  MEM_freeN(einput_arr);
}
