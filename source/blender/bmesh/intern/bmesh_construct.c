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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bmesh
 *
 * BM construction functions.
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_sort_utils.h"

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

#include "range_tree.h"

#define SELECT 1

#ifdef WITH_BM_ID_FREELIST
static uint bm_id_freelist_pop(BMesh *bm)
{
  if (bm->idmap.freelist_len > 0) {
    return bm->idmap.freelist[--bm->idmap.freelist_len];
  }

  return 0;
}

void bm_free_ids_check(BMesh *bm, uint id)
{
  if (id >> 2UL >= (uint)bm->idmap.free_ids_size) {
    size_t size = (size_t)(id >> 2) + 2ULL;
    size += size >> 1ULL;

    if (!bm->idmap.free_ids) {
      bm->idmap.free_ids = MEM_callocN(sizeof(int) * size, "free_ids");
    }
    else {
      bm->idmap.free_ids = MEM_recallocN(bm->idmap.free_ids, sizeof(int) * size);
    }

    bm->idmap.free_ids_size = (uint)size;
  }
}

static void bm_id_freelist_take(BMesh *bm, uint id)
{
  bm_free_ids_check(bm, id);

  if (!bm->idmap.free_ids || !BLI_BITMAP_TEST(bm->idmap.free_ids, id)) {
    return;
  }

  BLI_BITMAP_ENABLE(bm->idmap.free_ids, id);

  for (int i = 0; i < bm->idmap.freelist_len; i++) {
    if (bm->idmap.freelist[i] == id) {
      // swap with end
      bm->idmap.freelist[i] = bm->idmap.freelist[bm->idmap.freelist_len - 1];
      bm->idmap.freelist_len--;
    }
  }
}

static bool bm_id_freelist_has(BMesh *bm, uint id)
{
  if (!bm->idmap.free_ids) {
    return false;
  }

  return id < bm->idmap.free_ids_size && BLI_BITMAP_TEST(bm->idmap.free_ids, id);
}

void bm_id_freelist_push(BMesh *bm, uint id)
{
  bm_free_ids_check(bm, id);

  bm->idmap.freelist_len++;

  if (bm->idmap.freelist_len >= bm->idmap.freelist_size) {
    int size = 2 + bm->idmap.freelist_size + (bm->idmap.freelist_size >> 1);

    uint *newlist;

    if (bm->idmap.freelist) {
      newlist = MEM_reallocN(bm->idmap.freelist, size * sizeof(uint));
      memcpy((void *)newlist, (void *)bm->idmap.freelist, bm->idmap.freelist_size);
    }
    else {
      newlist = MEM_malloc_arrayN(size, sizeof(uint), "bm->idmap.freelist");
    }

    bm->idmap.freelist_size = size;
    bm->idmap.freelist = newlist;
  }

  bm->idmap.freelist[bm->idmap.freelist_len - 1] = id;
  BLI_BITMAP_ENABLE(bm->idmap.free_ids, id);
}
#endif

// static const int _typemap[] = {0, 0, 1, 0, 2, 0, 0, 0, 3};

void bm_assign_id_intern(BMesh *bm, BMElem *elem, uint id)
{
  // CustomData *cdata = &bm->vdata + _typemap[elem->head.htype];
  // int cd_id_off = cdata->layers[cdata->typemap[CD_MESH_ID]].offset;

  BM_ELEM_CD_SET_INT(elem, bm->idmap.cd_id_off[elem->head.htype], id);
  bm->idmap.maxid = MAX2(bm->idmap.maxid, id);

  if (bm->idmap.flag & BM_HAS_ID_MAP) {
    if (!(bm->idmap.flag & BM_NO_REUSE_IDS)) {
      if (!bm->idmap.map || bm->idmap.map_size <= (int)bm->idmap.maxid) {
        int size = 2 + bm->idmap.maxid + (bm->idmap.maxid >> 1);

        BMElem **idmap = MEM_callocN(sizeof(void *) * size, "bmesh idmap");

        if (bm->idmap.map) {
          memcpy((void *)idmap, (void *)bm->idmap.map, sizeof(void *) * bm->idmap.map_size);
          MEM_freeN(bm->idmap.map);
        }

        bm->idmap.map = idmap;
        bm->idmap.map_size = size;
      }

      bm->idmap.map[id] = elem;
    }
    else {
      void **val = NULL;

      BLI_ghash_ensure_p(bm->idmap.ghash, POINTER_FROM_UINT(id), &val);
      *val = (void *)elem;
    }
  }
}

void bm_assign_id(BMesh *bm, BMElem *elem, uint id, bool check_unqiue)
{
  if (check_unqiue && (bm->idmap.flag & BM_HAS_ID_MAP)) {
    if (BM_ELEM_FROM_ID(bm, id)) {

      printf("had to alloc a new id in bm_assign_id for %p; old id: %d\n", elem, (int)id);
    }
  }

#ifdef WITH_BM_ID_FREELIST
  bm_id_freelist_take(bm, id);
#else
  range_tree_uint_retake(bm->idmap.idtree, id);
#endif
  bm_assign_id_intern(bm, elem, id);
}

void bm_alloc_id(BMesh *bm, BMElem *elem)
{
  if ((bm->idmap.flag & (elem->head.htype | BM_HAS_IDS)) != (elem->head.htype | BM_HAS_IDS)) {
    return;
  }

#ifdef WITH_BM_ID_FREELIST
  uint id;

  if (bm->idmap.freelist_len > 0) {
    id = bm_id_freelist_pop(bm);
  }
  else {
    id = bm->idmap.maxid + 1;
  }
#else
  uint id = range_tree_uint_take_any(bm->idmap.idtree);
#endif

  bm_assign_id_intern(bm, elem, id);
}

void bm_free_id(BMesh *bm, BMElem *elem)
{
  if ((bm->idmap.flag & (elem->head.htype | BM_HAS_IDS)) != (elem->head.htype | BM_HAS_IDS)) {
    return;
  }

  uint id = (uint)BM_ELEM_CD_GET_INT(elem, bm->idmap.cd_id_off[elem->head.htype]);

#ifndef WITH_BM_ID_FREELIST

  if (!(bm->idmap.flag & BM_NO_REUSE_IDS) && !range_tree_uint_has(bm->idmap.idtree, id)) {
    range_tree_uint_release(bm->idmap.idtree, id);
  }
#else

#endif

  if ((bm->idmap.flag & BM_HAS_ID_MAP)) {
    if (!(bm->idmap.flag & BM_NO_REUSE_IDS) && bm->idmap.map && (int)id < bm->idmap.map_size) {
      bm->idmap.map[id] = NULL;
    }
    else if (bm->idmap.flag & BM_NO_REUSE_IDS) {
      BLI_ghash_remove(bm->idmap.ghash, POINTER_FROM_UINT(id), NULL, NULL);
    }
  }
}

/**
 * Fill in a vertex array from an edge array.
 *
 * \returns false if any verts aren't found.
 */
bool BM_verts_from_edges(BMVert **vert_arr, BMEdge **edge_arr, const int len)
{
  int i, i_prev = len - 1;
  for (i = 0; i < len; i++) {
    vert_arr[i] = BM_edge_share_vert(edge_arr[i_prev], edge_arr[i]);
    if (vert_arr[i] == NULL) {
      return false;
    }
    i_prev = i;
  }
  return true;
}

/**
 * Fill in an edge array from a vertex array (connected polygon loop).
 *
 * \returns false if any edges aren't found.
 */
bool BM_edges_from_verts(BMEdge **edge_arr, BMVert **vert_arr, const int len)
{
  int i, i_prev = len - 1;
  for (i = 0; i < len; i++) {
    edge_arr[i_prev] = BM_edge_exists(vert_arr[i_prev], vert_arr[i]);
    if (edge_arr[i_prev] == NULL) {
      return false;
    }
    i_prev = i;
  }
  return true;
}

/**
 * Fill in an edge array from a vertex array (connected polygon loop).
 * Creating edges as-needed.
 */
void BM_edges_from_verts_ensure(BMesh *bm, BMEdge **edge_arr, BMVert **vert_arr, const int len)
{
  int i, i_prev = len - 1;
  for (i = 0; i < len; i++) {
    edge_arr[i_prev] = BM_edge_create(
        bm, vert_arr[i_prev], vert_arr[i], NULL, BM_CREATE_NO_DOUBLE);
    i_prev = i;
  }
}

/* prototypes */
static void bm_loop_attrs_copy(
    BMesh *bm_src, BMesh *bm_dst, const BMLoop *l_src, BMLoop *l_dst, CustomDataMask mask_exclude);

/**
 * \brief Make Quad/Triangle
 *
 * Creates a new quad or triangle from a list of 3 or 4 vertices.
 * If \a no_double is true, then a check is done to see if a face
 * with these vertices already exists and returns it instead.
 *
 * If a pointer to an example face is provided, its custom data
 * and properties will be copied to the new face.
 *
 * \note The winding of the face is determined by the order
 * of the vertices in the vertex array.
 */

BMFace *BM_face_create_quad_tri(BMesh *bm,
                                BMVert *v1,
                                BMVert *v2,
                                BMVert *v3,
                                BMVert *v4,
                                const BMFace *f_example,
                                const eBMCreateFlag create_flag)
{
  BMVert *vtar[4] = {v1, v2, v3, v4};
  return BM_face_create_verts(bm, vtar, v4 ? 4 : 3, f_example, create_flag, true);
}

/**
 * \brief copies face loop data from shared adjacent faces.
 *
 * \param filter_fn: A function that filters the source loops before copying
 * (don't always want to copy all).
 *
 * \note when a matching edge is found, both loops of that edge are copied
 * this is done since the face may not be completely surrounded by faces,
 * this way: a quad with 2 connected quads on either side will still get all 4 loops updated
 */
void BM_face_copy_shared(BMesh *bm, BMFace *f, BMLoopFilterFunc filter_fn, void *user_data)
{
  BMLoop *l_first;
  BMLoop *l_iter;

#ifdef DEBUG
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    BLI_assert(BM_ELEM_API_FLAG_TEST(l_iter, _FLAG_OVERLAP) == 0);
  } while ((l_iter = l_iter->next) != l_first);
#endif

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    BMLoop *l_other = l_iter->radial_next;

    if (l_other && l_other != l_iter) {
      BMLoop *l_src[2];
      BMLoop *l_dst[2] = {l_iter, l_iter->next};
      uint j;

      if (l_other->v == l_iter->v) {
        l_src[0] = l_other;
        l_src[1] = l_other->next;
      }
      else {
        l_src[0] = l_other->next;
        l_src[1] = l_other;
      }

      for (j = 0; j < 2; j++) {
        BLI_assert(l_dst[j]->v == l_src[j]->v);
        if (BM_ELEM_API_FLAG_TEST(l_dst[j], _FLAG_OVERLAP) == 0) {
          if ((filter_fn == NULL) || filter_fn(l_src[j], user_data)) {
            bm_loop_attrs_copy(bm, bm, l_src[j], l_dst[j], 0x0);
            BM_ELEM_API_FLAG_ENABLE(l_dst[j], _FLAG_OVERLAP);
          }
        }
      }
    }
  } while ((l_iter = l_iter->next) != l_first);

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    BM_ELEM_API_FLAG_DISABLE(l_iter, _FLAG_OVERLAP);
  } while ((l_iter = l_iter->next) != l_first);
}

/**
 * Given an array of edges,
 * order them using the winding defined by \a v1 & \a v2
 * into \a edges_sort & \a verts_sort.
 *
 * All arrays must be \a len long.
 */
static bool bm_edges_sort_winding(BMVert *v1,
                                  BMVert *v2,
                                  BMEdge **edges,
                                  const int len,
                                  BMEdge **edges_sort,
                                  BMVert **verts_sort)
{
  BMEdge *e_iter, *e_first;
  BMVert *v_iter;
  int i;

  /* all flags _must_ be cleared on exit! */
  for (i = 0; i < len; i++) {
    BM_ELEM_API_FLAG_ENABLE(edges[i], _FLAG_MF);
    BM_ELEM_API_FLAG_ENABLE(edges[i]->v1, _FLAG_MV);
    BM_ELEM_API_FLAG_ENABLE(edges[i]->v2, _FLAG_MV);
  }

  /* find first edge */
  i = 0;
  v_iter = v1;
  e_iter = e_first = v1->e;
  do {
    if (BM_ELEM_API_FLAG_TEST(e_iter, _FLAG_MF) && (BM_edge_other_vert(e_iter, v_iter) == v2)) {
      i = 1;
      break;
    }
  } while ((e_iter = bmesh_disk_edge_next(e_iter, v_iter)) != e_first);
  if (i == 0) {
    goto error;
  }

  i = 0;
  do {
    /* entering loop will always succeed */
    if (BM_ELEM_API_FLAG_TEST(e_iter, _FLAG_MF)) {
      if (UNLIKELY(BM_ELEM_API_FLAG_TEST(v_iter, _FLAG_MV) == false)) {
        /* vert is in loop multiple times */
        goto error;
      }

      BM_ELEM_API_FLAG_DISABLE(e_iter, _FLAG_MF);
      edges_sort[i] = e_iter;

      BM_ELEM_API_FLAG_DISABLE(v_iter, _FLAG_MV);
      verts_sort[i] = v_iter;

      i += 1;

      /* walk onto the next vertex */
      v_iter = BM_edge_other_vert(e_iter, v_iter);
      if (i == len) {
        if (UNLIKELY(v_iter != verts_sort[0])) {
          goto error;
        }
        break;
      }

      e_first = e_iter;
    }
  } while ((e_iter = bmesh_disk_edge_next(e_iter, v_iter)) != e_first);

  if (i == len) {
    return true;
  }

error:
  for (i = 0; i < len; i++) {
    BM_ELEM_API_FLAG_DISABLE(edges[i], _FLAG_MF);
    BM_ELEM_API_FLAG_DISABLE(edges[i]->v1, _FLAG_MV);
    BM_ELEM_API_FLAG_DISABLE(edges[i]->v2, _FLAG_MV);
  }

  return false;
}

/**
 * \brief Make NGon
 *
 * Makes an ngon from an unordered list of edges.
 * Verts \a v1 and \a v2 define the winding of the new face.
 *
 * \a edges are not required to be ordered, simply to form
 * a single closed loop as a whole.
 *
 * \note While this function will work fine when the edges
 * are already sorted, if the edges are always going to be sorted,
 * #BM_face_create should be considered over this function as it
 * avoids some unnecessary work.
 */
BMFace *BM_face_create_ngon(BMesh *bm,
                            BMVert *v1,
                            BMVert *v2,
                            BMEdge **edges,
                            const int len,
                            const BMFace *f_example,
                            const eBMCreateFlag create_flag)
{
  BMEdge **edges_sort = BLI_array_alloca(edges_sort, len);
  BMVert **verts_sort = BLI_array_alloca(verts_sort, len);

  BLI_assert(len && v1 && v2 && edges && bm);

  if (bm_edges_sort_winding(v1, v2, edges, len, edges_sort, verts_sort)) {
    return BM_face_create(bm, verts_sort, edges_sort, len, f_example, create_flag);
  }

  return NULL;
}

/**
 * Create an ngon from an array of sorted verts
 *
 * Special features this has over other functions.
 * - Optionally calculate winding based on surrounding edges.
 * - Optionally create edges between vertices.
 * - Uses verts so no need to find edges (handy when you only have verts)
 */
BMFace *BM_face_create_ngon_verts(BMesh *bm,
                                  BMVert **vert_arr,
                                  const int len,
                                  const BMFace *f_example,
                                  const eBMCreateFlag create_flag,
                                  const bool calc_winding,
                                  const bool create_edges)
{
  BMEdge **edge_arr = BLI_array_alloca(edge_arr, len);
  uint winding[2] = {0, 0};
  int i, i_prev = len - 1;
  BMVert *v_winding[2] = {vert_arr[i_prev], vert_arr[0]};

  BLI_assert(len > 2);

  for (i = 0; i < len; i++) {
    if (create_edges) {
      edge_arr[i] = BM_edge_create(bm, vert_arr[i_prev], vert_arr[i], NULL, BM_CREATE_NO_DOUBLE);
    }
    else {
      edge_arr[i] = BM_edge_exists(vert_arr[i_prev], vert_arr[i]);
      if (edge_arr[i] == NULL) {
        return NULL;
      }
    }

    if (calc_winding) {
      /* the edge may exist already and be attached to a face
       * in this case we can find the best winding to use for the new face */
      if (edge_arr[i]->l) {
        BMVert *test_v1, *test_v2;
        /* we want to use the reverse winding to the existing order */
        BM_edge_ordered_verts(edge_arr[i], &test_v2, &test_v1);
        winding[(vert_arr[i_prev] == test_v2)]++;
        BLI_assert(ELEM(vert_arr[i_prev], test_v2, test_v1));
      }
    }

    i_prev = i;
  }

  /* --- */

  if (calc_winding) {
    if (winding[0] < winding[1]) {
      winding[0] = 1;
      winding[1] = 0;
    }
    else {
      winding[0] = 0;
      winding[1] = 1;
    }
  }
  else {
    winding[0] = 0;
    winding[1] = 1;
  }

  /* --- */

  /* create the face */
  return BM_face_create_ngon(
      bm, v_winding[winding[0]], v_winding[winding[1]], edge_arr, len, f_example, create_flag);
}

/**
 * Makes an NGon from an un-ordered set of verts
 *
 * assumes...
 * - that verts are only once in the list.
 * - that the verts have roughly planer bounds
 * - that the verts are roughly circular
 * there can be concave areas but overlapping folds from the center point will fail.
 *
 * a brief explanation of the method used
 * - find the center point
 * - find the normal of the vcloud
 * - order the verts around the face based on their angle to the normal vector at the center point.
 *
 * \note Since this is a vcloud there is no direction.
 */
void BM_verts_sort_radial_plane(BMVert **vert_arr, int len)
{
  struct SortIntByFloat *vang = BLI_array_alloca(vang, len);
  BMVert **vert_arr_map = BLI_array_alloca(vert_arr_map, len);

  float nor[3], cent[3];
  int index_tangent = 0;
  BM_verts_calc_normal_from_cloud_ex(vert_arr, len, nor, cent, &index_tangent);
  const float *far = vert_arr[index_tangent]->co;

  /* Now calculate every points angle around the normal (signed). */
  for (int i = 0; i < len; i++) {
    vang[i].sort_value = angle_signed_on_axis_v3v3v3_v3(far, cent, vert_arr[i]->co, nor);
    vang[i].data = i;
    vert_arr_map[i] = vert_arr[i];
  }

  /* sort by angle and magic! - we have our ngon */
  qsort(vang, len, sizeof(*vang), BLI_sortutil_cmp_float);

  /* --- */

  for (int i = 0; i < len; i++) {
    vert_arr[i] = vert_arr_map[vang[i].data];
  }
}

void BM_sort_disk_cycle(BMVert *v)
{
  BMVert **vs = NULL;
  BLI_array_staticdeclare(vs, 64);
  BMEdge **es = NULL;
  BLI_array_staticdeclare(es, 64);

  if (!v->e) {
    return;
  }

  BMEdge *e = v->e;
  do {
    BMVert *v2 = BM_edge_other_vert(e, v);

    BLI_array_append(es, e);
    BLI_array_append(vs, v2);

    e = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;
  } while (e != v->e);

  if (BLI_array_len(vs) < 2) {
    return;
  }

  int totvert = BLI_array_len(vs);

  struct SortIntByFloat *vang = BLI_array_alloca(vang, totvert);
  BMVert **vert_arr_map = BLI_array_alloca(vert_arr_map, totvert);

  float nor[3], cent[3];
  int index_tangent = 0;
  BM_verts_calc_normal_from_cloud_ex(vs, totvert, nor, cent, &index_tangent);
  const float *far = vs[index_tangent]->co;

  /* Now calculate every points angle around the normal (signed). */
  for (int i = 0; i < totvert; i++) {
    vang[i].sort_value = angle_signed_on_axis_v3v3v3_v3(far, cent, vs[i]->co, nor);
    vang[i].data = i;
    vert_arr_map[i] = vs[i];
  }

  /* sort by angle and magic! - we have our ngon */
  qsort(vang, totvert, sizeof(*vang), BLI_sortutil_cmp_float);

  BMEdge **es2 = BLI_array_alloca(es2, totvert);

  /* --- */

  for (int i = 0; i < totvert; i++) {
    es2[i] = es[vang[i].data];
  }

  // rebuild disk cycle
  for (int i = 0; i < totvert; i++) {
    int prev = (i + totvert - 1) % totvert;
    int next = (i + 1) % totvert;
    BMEdge *e = es2[i];

    if (e->v1 == v) {
      e->v1_disk_link.prev = es2[prev];
      e->v1_disk_link.next = es2[next];
    }
    else {
      e->v2_disk_link.prev = es2[prev];
      e->v2_disk_link.next = es2[next];
    }
  }

  BLI_array_free(es);
  BLI_array_free(vs);
}

/*************************************************************/

static void bm_vert_attrs_copy(
    BMesh *bm_src, BMesh *bm_dst, const BMVert *v_src, BMVert *v_dst, CustomDataMask mask_exclude)
{
  if ((bm_src == bm_dst) && (v_src == v_dst)) {
    BLI_assert_msg(0, "BMVert: source and target match");
    return;
  }
  if ((mask_exclude & CD_MASK_NORMAL) == 0) {
    copy_v3_v3(v_dst->no, v_src->no);
  }

  int id = bm_save_id(bm_dst, (BMElem *)v_dst);

  CustomData_bmesh_free_block_data_exclude_by_type(&bm_dst->vdata, v_dst->head.data, mask_exclude);
  CustomData_bmesh_copy_data_exclude_by_type(
      &bm_src->vdata, &bm_dst->vdata, v_src->head.data, &v_dst->head.data, mask_exclude);

  bm_restore_id(bm_dst, (BMElem *)v_dst, id);
}

static void bm_edge_attrs_copy(
    BMesh *bm_src, BMesh *bm_dst, const BMEdge *e_src, BMEdge *e_dst, CustomDataMask mask_exclude)
{
  if ((bm_src == bm_dst) && (e_src == e_dst)) {
    BLI_assert_msg(0, "BMEdge: source and target match");
    return;
  }

  int id = bm_save_id(bm_dst, (BMElem *)e_dst);

  CustomData_bmesh_free_block_data_exclude_by_type(&bm_dst->edata, e_dst->head.data, mask_exclude);
  CustomData_bmesh_copy_data_exclude_by_type(
      &bm_src->edata, &bm_dst->edata, e_src->head.data, &e_dst->head.data, mask_exclude);

  bm_restore_id(bm_dst, (BMElem *)e_dst, id);
}

static void bm_loop_attrs_copy(
    BMesh *bm_src, BMesh *bm_dst, const BMLoop *l_src, BMLoop *l_dst, CustomDataMask mask_exclude)
{
  if ((bm_src == bm_dst) && (l_src == l_dst)) {
    BLI_assert_msg(0, "BMLoop: source and target match");
    return;
  }

  int id = bm_save_id(bm_dst, (BMElem *)l_dst);

  CustomData_bmesh_free_block_data_exclude_by_type(&bm_dst->ldata, l_dst->head.data, mask_exclude);
  CustomData_bmesh_copy_data_exclude_by_type(
      &bm_src->ldata, &bm_dst->ldata, l_src->head.data, &l_dst->head.data, mask_exclude);

  bm_restore_id(bm_dst, (BMElem *)l_dst, id);
}

static void bm_face_attrs_copy(
    BMesh *bm_src, BMesh *bm_dst, const BMFace *f_src, BMFace *f_dst, CustomDataMask mask_exclude)
{
  if ((bm_src == bm_dst) && (f_src == f_dst)) {
    BLI_assert_msg(0, "BMFace: source and target match");
    return;
  }
  if ((mask_exclude & CD_MASK_NORMAL) == 0) {
    copy_v3_v3(f_dst->no, f_src->no);
  }

  int id = bm_save_id(bm_dst, (BMElem *)f_dst);

  CustomData_bmesh_free_block_data_exclude_by_type(&bm_dst->pdata, f_dst->head.data, mask_exclude);
  CustomData_bmesh_copy_data_exclude_by_type(
      &bm_src->pdata, &bm_dst->pdata, f_src->head.data, &f_dst->head.data, mask_exclude);
  f_dst->mat_nr = f_src->mat_nr;

  bm_restore_id(bm_dst, (BMElem *)f_dst, id);
}

/* BMESH_TODO: Special handling for hide flags? */
/* BMESH_TODO: swap src/dst args, everywhere else in bmesh does other way round */

/**
 * Copies attributes, e.g. customdata, header flags, etc, from one element
 * to another of the same type.
 */
void BM_elem_attrs_copy_ex(BMesh *bm_src,
                           BMesh *bm_dst,
                           const void *ele_src_v,
                           void *ele_dst_v,
                           const char hflag_mask,
                           const uint64_t cd_mask_exclude)
{
  const BMHeader *ele_src = ele_src_v;
  BMHeader *ele_dst = ele_dst_v;

  BLI_assert(ele_src->htype == ele_dst->htype);
  BLI_assert(ele_src != ele_dst);

  if ((hflag_mask & BM_ELEM_SELECT) == 0) {
    /* First we copy select */
    if (BM_elem_flag_test((BMElem *)ele_src, BM_ELEM_SELECT)) {
      BM_elem_select_set(bm_dst, (BMElem *)ele_dst, true);
    }
  }

  /* Now we copy flags */
  if (hflag_mask == 0) {
    ele_dst->hflag = ele_src->hflag;
  }
  else if (hflag_mask == 0xff) {
    /* pass */
  }
  else {
    ele_dst->hflag = ((ele_dst->hflag & hflag_mask) | (ele_src->hflag & ~hflag_mask));
  }

  /* Copy specific attributes */
  switch (ele_dst->htype) {
    case BM_VERT:
      bm_vert_attrs_copy(bm_src,
                         bm_dst,
                         (const BMVert *)ele_src,
                         (BMVert *)ele_dst,
                         cd_mask_exclude | CD_MASK_MESH_ID);
      break;
    case BM_EDGE:
      bm_edge_attrs_copy(bm_src,
                         bm_dst,
                         (const BMEdge *)ele_src,
                         (BMEdge *)ele_dst,
                         cd_mask_exclude | CD_MASK_MESH_ID);
      break;
    case BM_LOOP:
      bm_loop_attrs_copy(bm_src,
                         bm_dst,
                         (const BMLoop *)ele_src,
                         (BMLoop *)ele_dst,
                         cd_mask_exclude | CD_MASK_MESH_ID);
      break;
    case BM_FACE:
      bm_face_attrs_copy(bm_src,
                         bm_dst,
                         (const BMFace *)ele_src,
                         (BMFace *)ele_dst,
                         cd_mask_exclude | CD_MASK_MESH_ID);
      break;
    default:
      BLI_assert(0);
      break;
  }
}

void BM_elem_attrs_copy(BMesh *bm_src, BMesh *bm_dst, const void *ele_src, void *ele_dst)
{
  /* BMESH_TODO, default 'use_flags' to false */
  BM_elem_attrs_copy_ex(bm_src, bm_dst, ele_src, ele_dst, BM_ELEM_SELECT, 0x0);
}

void BM_elem_select_copy(BMesh *bm_dst, void *ele_dst_v, const void *ele_src_v)
{
  BMHeader *ele_dst = ele_dst_v;
  const BMHeader *ele_src = ele_src_v;

  BLI_assert(ele_src->htype == ele_dst->htype);

  if ((ele_src->hflag & BM_ELEM_SELECT) != (ele_dst->hflag & BM_ELEM_SELECT)) {
    BM_elem_select_set(bm_dst, (BMElem *)ele_dst, (ele_src->hflag & BM_ELEM_SELECT) != 0);
  }
}

/* helper function for 'BM_mesh_copy' */
static BMFace *bm_mesh_copy_new_face(
    BMesh *bm_new, BMesh *bm_old, BMVert **vtable, BMEdge **etable, BMFace *f)
{
  BMLoop **loops = BLI_array_alloca(loops, f->len);
  BMVert **verts = BLI_array_alloca(verts, f->len);
  BMEdge **edges = BLI_array_alloca(edges, f->len);

  BMFace *f_new;
  BMLoop *l_iter, *l_first;
  int j;

  j = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    loops[j] = l_iter;
    verts[j] = vtable[BM_elem_index_get(l_iter->v)];
    edges[j] = etable[BM_elem_index_get(l_iter->e)];
    j++;
  } while ((l_iter = l_iter->next) != l_first);

  f_new = BM_face_create(
      bm_new, verts, edges, f->len, NULL, BM_CREATE_SKIP_CD | BM_CREATE_SKIP_ID);

  if (UNLIKELY(f_new == NULL)) {
    return NULL;
  }

  /* use totface in case adding some faces fails */
  BM_elem_index_set(f_new, (bm_new->totface - 1)); /* set_inline */

  BM_elem_attrs_copy_ex(bm_old, bm_new, f, f_new, 0xff, 0x0);
  f_new->head.hflag = f->head.hflag; /* low level! don't do this for normal api use */

  j = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
  do {
    BM_elem_attrs_copy(bm_old, bm_new, loops[j], l_iter);
    j++;
  } while ((l_iter = l_iter->next) != l_first);

  return f_new;
}

void BM_mesh_copy_init_customdata(BMesh *bm_dst, BMesh *bm_src, const BMAllocTemplate *allocsize)
{
  if (allocsize == NULL) {
    allocsize = &bm_mesh_allocsize_default;
  }

  // forcibly copy mesh_id layers
  CustomData *srcdatas[4] = {&bm_src->vdata, &bm_src->edata, &bm_src->ldata, &bm_src->pdata};
  CustomData *dstdatas[4] = {&bm_dst->vdata, &bm_dst->edata, &bm_dst->ldata, &bm_dst->pdata};

  for (int i = 0; i < 4; i++) {
    CustomData *cdata = srcdatas[i];

    if (CustomData_has_layer(cdata, CD_MESH_ID)) {
      int idx = CustomData_get_layer_index(cdata, CD_MESH_ID);

      cdata->layers[idx].flag &= ~(CD_FLAG_TEMPORARY | CD_FLAG_ELEM_NOCOPY);
    }
  }

  CustomData_copy(
      &bm_src->vdata, &bm_dst->vdata, CD_MASK_BMESH.vmask | CD_MASK_MESH_ID, CD_CALLOC, 0);
  CustomData_copy(
      &bm_src->edata, &bm_dst->edata, CD_MASK_BMESH.emask | CD_MASK_MESH_ID, CD_CALLOC, 0);
  CustomData_copy(
      &bm_src->ldata, &bm_dst->ldata, CD_MASK_BMESH.lmask | CD_MASK_MESH_ID, CD_CALLOC, 0);
  CustomData_copy(
      &bm_src->pdata, &bm_dst->pdata, CD_MASK_BMESH.pmask | CD_MASK_MESH_ID, CD_CALLOC, 0);

  CustomData_bmesh_init_pool(&bm_dst->vdata, allocsize->totvert, BM_VERT);
  CustomData_bmesh_init_pool(&bm_dst->edata, allocsize->totedge, BM_EDGE);
  CustomData_bmesh_init_pool(&bm_dst->ldata, allocsize->totloop, BM_LOOP);
  CustomData_bmesh_init_pool(&bm_dst->pdata, allocsize->totface, BM_FACE);

  // flag mesh id layer as temporary
  if (!(bm_dst->idmap.flag & BM_PERMANENT_IDS)) {
    for (int i = 0; i < 4; i++) {
      CustomData *cdata = dstdatas[i];

      if (CustomData_has_layer(cdata, CD_MESH_ID)) {
        int idx = CustomData_get_layer_index(cdata, CD_MESH_ID);

        cdata->layers[idx].flag |= CD_FLAG_TEMPORARY | CD_FLAG_ELEM_NOCOPY;
      }
    }
  }
}

/**
 * Similar to #BM_mesh_copy_init_customdata but copies all layers ignoring
 * flags like #CD_FLAG_NOCOPY.
 *
 * \param bm_dst: BMesh whose custom-data layers will be added.
 * \param bm_src: BMesh whose custom-data layers will be copied.
 * \param htype: Specifies which custom-data layers will be initiated.
 * \param allocsize: Initialize the memory-pool before use (may be an estimate).
 */
void BM_mesh_copy_init_customdata_all_layers(BMesh *bm_dst,
                                             BMesh *bm_src,
                                             const char htype,
                                             const BMAllocTemplate *allocsize)
{
  if (allocsize == NULL) {
    allocsize = &bm_mesh_allocsize_default;
  }

  const char htypes[4] = {BM_VERT, BM_EDGE, BM_LOOP, BM_FACE};
  BLI_assert(((&bm_dst->vdata + 1) == &bm_dst->edata) &&
             ((&bm_dst->vdata + 2) == &bm_dst->ldata) && ((&bm_dst->vdata + 3) == &bm_dst->pdata));

  BLI_assert(((&allocsize->totvert + 1) == &allocsize->totedge) &&
             ((&allocsize->totvert + 2) == &allocsize->totloop) &&
             ((&allocsize->totvert + 3) == &allocsize->totface));

  for (int i = 0; i < 4; i++) {
    if (!(htypes[i] & htype)) {
      continue;
    }
    CustomData *dst = &bm_dst->vdata + i;
    CustomData *src = &bm_src->vdata + i;
    const int size = *(&allocsize->totvert + i);

    for (int l = 0; l < src->totlayer; l++) {
      CustomData_add_layer_named(
          dst, src->layers[l].type, CD_CALLOC, NULL, 0, src->layers[l].name);
    }
    CustomData_bmesh_init_pool(dst, size, htypes[i]);
  }

  bm_update_idmap_cdlayers(bm_dst);
}

BMesh *BM_mesh_copy_ex(BMesh *bm_old, struct BMeshCreateParams *params)
{
  BMesh *bm_new;
  BMVert *v, *v_new, **vtable = NULL;
  BMEdge *e, *e_new, **etable = NULL;
  BMFace *f, *f_new, **ftable = NULL;
  BMElem **eletable;
  BMEditSelection *ese;
  BMIter iter;
  int i;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_BM(bm_old);
  struct BMeshCreateParams _params;

  if (!params) {
    _params = ((struct BMeshCreateParams){
        .use_toolflags = bm_old->use_toolflags,
        .id_elem_mask = bm_old->idmap.flag & (BM_VERT | BM_EDGE | BM_LOOP | BM_FACE),
        .create_unique_ids = !!(bm_old->idmap.flag & BM_HAS_IDS),
        .id_map = !!(bm_old->idmap.flag & BM_HAS_ID_MAP),
        .temporary_ids = !(bm_old->idmap.flag & BM_PERMANENT_IDS),
        .no_reuse_ids = !!(bm_old->idmap.flag & BM_NO_REUSE_IDS)});
    params = &_params;
  }

  /* allocate a bmesh */
  bm_new = BM_mesh_create(&allocsize, params);

  if (params->copy_all_layers) {
    BM_mesh_copy_init_customdata_all_layers(
        bm_new, bm_old, BM_VERT | BM_EDGE | BM_LOOP | BM_FACE, &allocsize);
  }
  else {
    BM_mesh_copy_init_customdata(bm_new, bm_old, &allocsize);
  }

  if (bm_old->idmap.flag & BM_HAS_IDS) {
    MEM_SAFE_FREE(bm_new->idmap.map);

    if ((bm_old->idmap.flag & BM_HAS_ID_MAP)) {
      if (!(bm_old->idmap.flag & BM_NO_REUSE_IDS)) {
        bm_new->idmap.map_size = bm_old->idmap.map_size;
        bm_new->idmap.flag = bm_old->idmap.flag;

        if (bm_new->idmap.map_size) {
          bm_new->idmap.map = MEM_callocN(sizeof(void *) * bm_old->idmap.map_size, "bm idmap");
        }
        else {
          bm_new->idmap.map = NULL;
        }
      }
      else {
        BLI_ghash_free(bm_new->idmap.ghash, NULL, NULL);
        bm_new->idmap.ghash = BLI_ghash_ptr_new_ex(
            "idmap.ghash", bm_old->totvert + bm_old->totedge + bm_old->totface);
      }
    }

    bm_init_idmap_cdlayers(bm_new);
  }

  vtable = MEM_mallocN(sizeof(BMVert *) * bm_old->totvert, "BM_mesh_copy vtable");
  etable = MEM_mallocN(sizeof(BMEdge *) * bm_old->totedge, "BM_mesh_copy etable");
  ftable = MEM_mallocN(sizeof(BMFace *) * bm_old->totface, "BM_mesh_copy ftable");

  BM_ITER_MESH_INDEX (v, &iter, bm_old, BM_VERTS_OF_MESH, i) {
    /* copy between meshes so can't use 'example' argument */
    v_new = BM_vert_create(bm_new, v->co, NULL, BM_CREATE_SKIP_CD | BM_CREATE_SKIP_ID);

    BM_elem_attrs_copy_ex(bm_old, bm_new, v, v_new, 0xff, 0x0);
    bm_alloc_id(bm_new, (BMElem *)v_new);

    v_new->head.hflag = v->head.hflag; /* low level! don't do this for normal api use */
    vtable[i] = v_new;
    BM_elem_index_set(v, i);     /* set_inline */
    BM_elem_index_set(v_new, i); /* set_inline */
  }
  bm_old->elem_index_dirty &= ~BM_VERT;
  bm_new->elem_index_dirty &= ~BM_VERT;

  /* safety check */
  BLI_assert(i == bm_old->totvert);

  BM_ITER_MESH_INDEX (e, &iter, bm_old, BM_EDGES_OF_MESH, i) {
    e_new = BM_edge_create(bm_new,
                           vtable[BM_elem_index_get(e->v1)],
                           vtable[BM_elem_index_get(e->v2)],
                           e,
                           BM_CREATE_SKIP_CD | BM_CREATE_SKIP_ID);

    BM_elem_attrs_copy_ex(bm_old, bm_new, e, e_new, 0xff, 0x0);
    bm_alloc_id(bm_new, (BMElem *)e_new);

    e_new->head.hflag = e->head.hflag; /* low level! don't do this for normal api use */
    etable[i] = e_new;
    BM_elem_index_set(e, i);     /* set_inline */
    BM_elem_index_set(e_new, i); /* set_inline */
  }
  bm_old->elem_index_dirty &= ~BM_EDGE;
  bm_new->elem_index_dirty &= ~BM_EDGE;

  /* safety check */
  BLI_assert(i == bm_old->totedge);

  BM_ITER_MESH_INDEX (f, &iter, bm_old, BM_FACES_OF_MESH, i) {
    BM_elem_index_set(f, i); /* set_inline */

    f_new = bm_mesh_copy_new_face(bm_new, bm_old, vtable, etable, f);
    bm_alloc_id(bm_new, (BMElem *)f_new);

    if (bm_new->idmap.flag & BM_LOOP) {
      BMLoop *l_new = f_new->l_first;

      do {
        bm_alloc_id(bm_new, (BMElem *)l_new);
        l_new = l_new->next;
      } while (l_new != f_new->l_first);
    }

    ftable[i] = f_new;

    if (f == bm_old->act_face) {
      bm_new->act_face = f_new;
    }
  }
  bm_old->elem_index_dirty &= ~BM_FACE;
  bm_new->elem_index_dirty &= ~BM_FACE;

  /* low level! don't do this for normal api use */
  bm_new->totvertsel = bm_old->totvertsel;
  bm_new->totedgesel = bm_old->totedgesel;
  bm_new->totfacesel = bm_old->totfacesel;

  /* safety check */
  BLI_assert(i == bm_old->totface);

  /* copy over edit selection history */
  for (ese = bm_old->selected.first; ese; ese = ese->next) {
    BMElem *ele = NULL;

    switch (ese->htype) {
      case BM_VERT:
        eletable = (BMElem **)vtable;
        break;
      case BM_EDGE:
        eletable = (BMElem **)etable;
        break;
      case BM_FACE:
        eletable = (BMElem **)ftable;
        break;
      default:
        eletable = NULL;
        break;
    }

    if (eletable) {
      ele = eletable[BM_elem_index_get(ese->ele)];
      if (ele) {
        BM_select_history_store(bm_new, ele);
      }
    }
  }

  MEM_freeN(etable);
  MEM_freeN(vtable);
  MEM_freeN(ftable);

  /* Copy various settings. */
  bm_new->shapenr = bm_old->shapenr;
  bm_new->selectmode = bm_old->selectmode;

  return bm_new;
}

BMesh *BM_mesh_copy(BMesh *bm_old)
{
  return BM_mesh_copy_ex(bm_old, NULL);
}

/* ME -> BM */
char BM_vert_flag_from_mflag(const char mflag)
{
  return (((mflag & SELECT) ? BM_ELEM_SELECT : 0) | ((mflag & ME_HIDE) ? BM_ELEM_HIDDEN : 0));
}
char BM_edge_flag_from_mflag(const short mflag)
{
  return (((mflag & SELECT) ? BM_ELEM_SELECT : 0) | ((mflag & ME_SEAM) ? BM_ELEM_SEAM : 0) |
          ((mflag & ME_EDGEDRAW) ? BM_ELEM_DRAW : 0) |
          ((mflag & ME_SHARP) == 0 ? BM_ELEM_SMOOTH : 0) | /* invert */
          ((mflag & ME_HIDE) ? BM_ELEM_HIDDEN : 0));
}
char BM_face_flag_from_mflag(const char mflag)
{
  return (((mflag & ME_FACE_SEL) ? BM_ELEM_SELECT : 0) |
          ((mflag & ME_SMOOTH) ? BM_ELEM_SMOOTH : 0) | ((mflag & ME_HIDE) ? BM_ELEM_HIDDEN : 0));
}

/* BM -> ME */
char BM_vert_flag_to_mflag(BMVert *v)
{
  const char hflag = v->head.hflag;

  return (((hflag & BM_ELEM_SELECT) ? SELECT : 0) | ((hflag & BM_ELEM_HIDDEN) ? ME_HIDE : 0));
}

short BM_edge_flag_to_mflag(BMEdge *e)
{
  const char hflag = e->head.hflag;

  return (((hflag & BM_ELEM_SELECT) ? SELECT : 0) | ((hflag & BM_ELEM_SEAM) ? ME_SEAM : 0) |
          ((hflag & BM_ELEM_DRAW) ? ME_EDGEDRAW : 0) |
          ((hflag & BM_ELEM_SMOOTH) == 0 ? ME_SHARP : 0) |
          ((hflag & BM_ELEM_HIDDEN) ? ME_HIDE : 0) |
          (BM_edge_is_wire(e) ? ME_LOOSEEDGE : 0) | /* not typical */
          ME_EDGERENDER);
}
char BM_face_flag_to_mflag(BMFace *f)
{
  const char hflag = f->head.hflag;

  return (((hflag & BM_ELEM_SELECT) ? ME_FACE_SEL : 0) |
          ((hflag & BM_ELEM_SMOOTH) ? ME_SMOOTH : 0) | ((hflag & BM_ELEM_HIDDEN) ? ME_HIDE : 0));
}

void bm_init_idmap_cdlayers(BMesh *bm)
{
  if (!(bm->idmap.flag & BM_HAS_IDS)) {
    return;
  }

  bool temp_ids = !(bm->idmap.flag & BM_PERMANENT_IDS);

  int types[4] = {BM_VERT, BM_EDGE, BM_LOOP, BM_FACE};
  CustomData *cdatas[4] = {&bm->vdata, &bm->edata, &bm->ldata, &bm->pdata};

  for (int i = 0; i < 4; i++) {
    CustomDataLayer *layer;

    if (!(bm->idmap.flag & types[i])) {
      continue;
    }

    if (!CustomData_has_layer(cdatas[i], CD_MESH_ID)) {
      BM_data_layer_add(bm, cdatas[i], CD_MESH_ID);
    }

    layer = cdatas[i]->layers + CustomData_get_layer_index(cdatas[i], CD_MESH_ID);
    layer->flag |= CD_FLAG_ELEM_NOCOPY;

    if (temp_ids) {
      layer->flag |= CD_FLAG_TEMPORARY;
    }
    else {
      layer->flag &= ~CD_FLAG_TEMPORARY;
    }
  }

  bm_update_idmap_cdlayers(bm);
}

void bm_update_idmap_cdlayers(BMesh *bm)
{
  if (!(bm->idmap.flag & BM_HAS_IDS)) {
    return;
  }

  bm->idmap.cd_id_off[BM_VERT] = CustomData_get_offset(&bm->vdata, CD_MESH_ID);
  bm->idmap.cd_id_off[BM_EDGE] = CustomData_get_offset(&bm->edata, CD_MESH_ID);
  bm->idmap.cd_id_off[BM_LOOP] = CustomData_get_offset(&bm->ldata, CD_MESH_ID);
  bm->idmap.cd_id_off[BM_FACE] = CustomData_get_offset(&bm->pdata, CD_MESH_ID);
}

ATTR_NO_OPT void bm_rebuild_idmap(BMesh *bm)
{
  CustomData *cdatas[4] = {
      &bm->vdata,
      &bm->edata,
      &bm->ldata,
      &bm->pdata,
  };

  if (bm->idmap.flag & BM_HAS_ID_MAP) {
    if (bm->idmap.flag & BM_NO_REUSE_IDS) {
      if (bm->idmap.ghash) {
        BLI_ghash_clear(bm->idmap.ghash, NULL, NULL);
      }
      else {
        bm->idmap.ghash = BLI_ghash_ptr_new("bm->idmap.ghash");
      }
    }
    else if (bm->idmap.map) {
      memset(bm->idmap.map, 0, sizeof(void *) * bm->idmap.map_size);
    }
  }

  for (int i = 0; i < 4; i++) {
    int type = 1 << i;

    if (!(bm->idmap.flag & type)) {
      continue;
    }

    int cd_off = bm->idmap.cd_id_off[type];
    cd_off = CustomData_get_offset(cdatas[i], CD_MESH_ID);

    if (bm->idmap.flag & BM_NO_REUSE_IDS) {
      BLI_mempool_iter iter;

      BLI_mempool_iternew((&bm->vpool)[i], &iter);
      BMElem *elem = (BMElem *)BLI_mempool_iterstep(&iter);
      for (; elem; elem = (BMElem *)BLI_mempool_iterstep(&iter)) {
        void **val;

        if (!BLI_ghash_ensure_p(bm->idmap.ghash, (void *)elem, &val)) {
          *val = POINTER_FROM_INT(BM_ELEM_CD_GET_INT(elem, cd_off));
        }
      }
    }
    else {
      BLI_mempool_iter iter;

      BLI_mempool_iternew((&bm->vpool)[i], &iter);
      BMElem *elem = (BMElem *)BLI_mempool_iterstep(&iter);
      for (; elem; elem = (BMElem *)BLI_mempool_iterstep(&iter)) {
        void **val;
        int id = BM_ELEM_CD_GET_INT(elem, cd_off);

        if (!bm->idmap.map || bm->idmap.map_size <= id) {
          int size = (2 + id);
          size += size >> 1;

          if (!bm->idmap.map) {
            bm->idmap.map = MEM_calloc_arrayN(size, sizeof(*bm->idmap.map), "bm->idmap.map");
          }
          else {
            bm->idmap.map = MEM_recallocN(bm->idmap.map, sizeof(void *) * size);
          }

          bm->idmap.map_size = size;
        }

        bm->idmap.map[BM_ELEM_CD_GET_INT(elem, cd_off)] = elem;
      }
    }
  }
}
