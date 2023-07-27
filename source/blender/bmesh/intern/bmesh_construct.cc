/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BM construction functions.
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_sort_utils.h"

#include "BKE_customdata.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

bool BM_verts_from_edges(BMVert **vert_arr, BMEdge **edge_arr, const int len)
{
  int i, i_prev = len - 1;
  for (i = 0; i < len; i++) {
    vert_arr[i] = BM_edge_share_vert(edge_arr[i_prev], edge_arr[i]);
    if (vert_arr[i] == nullptr) {
      return false;
    }
    i_prev = i;
  }
  return true;
}

bool BM_edges_from_verts(BMEdge **edge_arr, BMVert **vert_arr, const int len)
{
  int i, i_prev = len - 1;
  for (i = 0; i < len; i++) {
    edge_arr[i_prev] = BM_edge_exists(vert_arr[i_prev], vert_arr[i]);
    if (edge_arr[i_prev] == nullptr) {
      return false;
    }
    i_prev = i;
  }
  return true;
}

void BM_edges_from_verts_ensure(BMesh *bm, BMEdge **edge_arr, BMVert **vert_arr, const int len)
{
  int i, i_prev = len - 1;
  for (i = 0; i < len; i++) {
    edge_arr[i_prev] = BM_edge_create(
        bm, vert_arr[i_prev], vert_arr[i], nullptr, BM_CREATE_NO_DOUBLE);
    i_prev = i;
  }
}

/* prototypes */
static void bm_loop_attrs_copy(BMesh *bm_src,
                               BMesh *bm_dst,
                               const BMLoop *l_src,
                               BMLoop *l_dst,
                               eCustomDataMask mask_exclude);

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
          if ((filter_fn == nullptr) || filter_fn(l_src[j], user_data)) {
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

  return nullptr;
}

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
      edge_arr[i] = BM_edge_create(
          bm, vert_arr[i_prev], vert_arr[i], nullptr, BM_CREATE_NO_DOUBLE);
    }
    else {
      edge_arr[i] = BM_edge_exists(vert_arr[i_prev], vert_arr[i]);
      if (edge_arr[i] == nullptr) {
        return nullptr;
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

void BM_verts_sort_radial_plane(BMVert **vert_arr, int len)
{
  SortIntByFloat *vang = BLI_array_alloca(vang, len);
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

/*************************************************************/

static void bm_vert_attrs_copy(
    BMesh *bm_src, BMesh *bm_dst, const BMVert *v_src, BMVert *v_dst, eCustomDataMask mask_exclude)
{
  if ((bm_src == bm_dst) && (v_src == v_dst)) {
    BLI_assert_msg(0, "BMVert: source and target match");
    return;
  }
  if ((mask_exclude & CD_MASK_NORMAL) == 0) {
    copy_v3_v3(v_dst->no, v_src->no);
  }
  CustomData_bmesh_free_block_data_exclude_by_type(&bm_dst->vdata, v_dst->head.data, mask_exclude);
  CustomData_bmesh_copy_data_exclude_by_type(
      &bm_src->vdata, &bm_dst->vdata, v_src->head.data, &v_dst->head.data, mask_exclude);
}

static void bm_edge_attrs_copy(
    BMesh *bm_src, BMesh *bm_dst, const BMEdge *e_src, BMEdge *e_dst, eCustomDataMask mask_exclude)
{
  if ((bm_src == bm_dst) && (e_src == e_dst)) {
    BLI_assert_msg(0, "BMEdge: source and target match");
    return;
  }
  CustomData_bmesh_free_block_data_exclude_by_type(&bm_dst->edata, e_dst->head.data, mask_exclude);
  CustomData_bmesh_copy_data_exclude_by_type(
      &bm_src->edata, &bm_dst->edata, e_src->head.data, &e_dst->head.data, mask_exclude);
}

static void bm_loop_attrs_copy(
    BMesh *bm_src, BMesh *bm_dst, const BMLoop *l_src, BMLoop *l_dst, eCustomDataMask mask_exclude)
{
  if ((bm_src == bm_dst) && (l_src == l_dst)) {
    BLI_assert_msg(0, "BMLoop: source and target match");
    return;
  }
  CustomData_bmesh_free_block_data_exclude_by_type(&bm_dst->ldata, l_dst->head.data, mask_exclude);
  CustomData_bmesh_copy_data_exclude_by_type(
      &bm_src->ldata, &bm_dst->ldata, l_src->head.data, &l_dst->head.data, mask_exclude);
}

static void bm_face_attrs_copy(
    BMesh *bm_src, BMesh *bm_dst, const BMFace *f_src, BMFace *f_dst, eCustomDataMask mask_exclude)
{
  if ((bm_src == bm_dst) && (f_src == f_dst)) {
    BLI_assert_msg(0, "BMFace: source and target match");
    return;
  }
  if ((mask_exclude & CD_MASK_NORMAL) == 0) {
    copy_v3_v3(f_dst->no, f_src->no);
  }
  CustomData_bmesh_free_block_data_exclude_by_type(&bm_dst->pdata, f_dst->head.data, mask_exclude);
  CustomData_bmesh_copy_data_exclude_by_type(
      &bm_src->pdata, &bm_dst->pdata, f_src->head.data, &f_dst->head.data, mask_exclude);
  f_dst->mat_nr = f_src->mat_nr;
}

void BM_elem_attrs_copy_ex(BMesh *bm_src,
                           BMesh *bm_dst,
                           const void *ele_src_v,
                           void *ele_dst_v,
                           const char hflag_mask,
                           const uint64_t cd_mask_exclude)
{
  /* TODO: Special handling for hide flags? */
  /* TODO: swap src/dst args, everywhere else in bmesh does other way round. */

  const BMHeader *ele_src = static_cast<const BMHeader *>(ele_src_v);
  BMHeader *ele_dst = static_cast<BMHeader *>(ele_dst_v);

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
      bm_vert_attrs_copy(
          bm_src, bm_dst, (const BMVert *)ele_src, (BMVert *)ele_dst, cd_mask_exclude);
      break;
    case BM_EDGE:
      bm_edge_attrs_copy(
          bm_src, bm_dst, (const BMEdge *)ele_src, (BMEdge *)ele_dst, cd_mask_exclude);
      break;
    case BM_LOOP:
      bm_loop_attrs_copy(
          bm_src, bm_dst, (const BMLoop *)ele_src, (BMLoop *)ele_dst, cd_mask_exclude);
      break;
    case BM_FACE:
      bm_face_attrs_copy(
          bm_src, bm_dst, (const BMFace *)ele_src, (BMFace *)ele_dst, cd_mask_exclude);
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
  BMHeader *ele_dst = static_cast<BMHeader *>(ele_dst_v);
  const BMHeader *ele_src = static_cast<const BMHeader *>(ele_src_v);

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

  f_new = BM_face_create(bm_new, verts, edges, f->len, nullptr, BM_CREATE_SKIP_CD);

  if (UNLIKELY(f_new == nullptr)) {
    return nullptr;
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

void BM_mesh_copy_init_customdata_from_mesh_array(BMesh *bm_dst,
                                                  const Mesh *me_src_array[],
                                                  const int me_src_array_len,
                                                  const BMAllocTemplate *allocsize)

{
  if (allocsize == nullptr) {
    allocsize = &bm_mesh_allocsize_default;
  }

  for (int i = 0; i < me_src_array_len; i++) {
    const Mesh *me_src = me_src_array[i];
    CustomData mesh_vdata = CustomData_shallow_copy_remove_non_bmesh_attributes(
        &me_src->vert_data, CD_MASK_BMESH.vmask);
    CustomData mesh_edata = CustomData_shallow_copy_remove_non_bmesh_attributes(
        &me_src->edge_data, CD_MASK_BMESH.emask);
    CustomData mesh_pdata = CustomData_shallow_copy_remove_non_bmesh_attributes(
        &me_src->face_data, CD_MASK_BMESH.lmask);
    CustomData mesh_ldata = CustomData_shallow_copy_remove_non_bmesh_attributes(
        &me_src->loop_data, CD_MASK_BMESH.pmask);

    if (i == 0) {
      CustomData_copy_layout(&mesh_vdata, &bm_dst->vdata, CD_MASK_BMESH.vmask, CD_SET_DEFAULT, 0);
      CustomData_copy_layout(&mesh_edata, &bm_dst->edata, CD_MASK_BMESH.emask, CD_SET_DEFAULT, 0);
      CustomData_copy_layout(&mesh_pdata, &bm_dst->pdata, CD_MASK_BMESH.pmask, CD_SET_DEFAULT, 0);
      CustomData_copy_layout(&mesh_ldata, &bm_dst->ldata, CD_MASK_BMESH.lmask, CD_SET_DEFAULT, 0);
    }
    else {
      CustomData_merge_layout(&mesh_vdata, &bm_dst->vdata, CD_MASK_BMESH.vmask, CD_SET_DEFAULT, 0);
      CustomData_merge_layout(&mesh_edata, &bm_dst->edata, CD_MASK_BMESH.emask, CD_SET_DEFAULT, 0);
      CustomData_merge_layout(&mesh_pdata, &bm_dst->pdata, CD_MASK_BMESH.pmask, CD_SET_DEFAULT, 0);
      CustomData_merge_layout(&mesh_ldata, &bm_dst->ldata, CD_MASK_BMESH.lmask, CD_SET_DEFAULT, 0);
    }

    MEM_SAFE_FREE(mesh_vdata.layers);
    MEM_SAFE_FREE(mesh_edata.layers);
    MEM_SAFE_FREE(mesh_pdata.layers);
    MEM_SAFE_FREE(mesh_ldata.layers);
  }

  CustomData_bmesh_init_pool(&bm_dst->vdata, allocsize->totvert, BM_VERT);
  CustomData_bmesh_init_pool(&bm_dst->edata, allocsize->totedge, BM_EDGE);
  CustomData_bmesh_init_pool(&bm_dst->ldata, allocsize->totloop, BM_LOOP);
  CustomData_bmesh_init_pool(&bm_dst->pdata, allocsize->totface, BM_FACE);
}

void BM_mesh_copy_init_customdata_from_mesh(BMesh *bm_dst,
                                            const Mesh *me_src,
                                            const BMAllocTemplate *allocsize)
{
  BM_mesh_copy_init_customdata_from_mesh_array(bm_dst, &me_src, 1, allocsize);
}

void BM_mesh_copy_init_customdata(BMesh *bm_dst, BMesh *bm_src, const BMAllocTemplate *allocsize)
{
  if (allocsize == nullptr) {
    allocsize = &bm_mesh_allocsize_default;
  }

  CustomData_copy_layout(&bm_src->vdata, &bm_dst->vdata, CD_MASK_BMESH.vmask, CD_SET_DEFAULT, 0);
  CustomData_copy_layout(&bm_src->edata, &bm_dst->edata, CD_MASK_BMESH.emask, CD_SET_DEFAULT, 0);
  CustomData_copy_layout(&bm_src->ldata, &bm_dst->ldata, CD_MASK_BMESH.lmask, CD_SET_DEFAULT, 0);
  CustomData_copy_layout(&bm_src->pdata, &bm_dst->pdata, CD_MASK_BMESH.pmask, CD_SET_DEFAULT, 0);

  CustomData_bmesh_init_pool(&bm_dst->vdata, allocsize->totvert, BM_VERT);
  CustomData_bmesh_init_pool(&bm_dst->edata, allocsize->totedge, BM_EDGE);
  CustomData_bmesh_init_pool(&bm_dst->ldata, allocsize->totloop, BM_LOOP);
  CustomData_bmesh_init_pool(&bm_dst->pdata, allocsize->totface, BM_FACE);
}

void BM_mesh_copy_init_customdata_all_layers(BMesh *bm_dst,
                                             BMesh *bm_src,
                                             const char htype,
                                             const BMAllocTemplate *allocsize)
{
  if (allocsize == nullptr) {
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
          dst, eCustomDataType(src->layers[l].type), CD_SET_DEFAULT, 0, src->layers[l].name);
    }
    CustomData_bmesh_init_pool(dst, size, htypes[i]);
  }
}

BMesh *BM_mesh_copy(BMesh *bm_old)
{
  BMesh *bm_new;
  BMVert *v, *v_new, **vtable = nullptr;
  BMEdge *e, *e_new, **etable = nullptr;
  BMFace *f, *f_new, **ftable = nullptr;
  BMElem **eletable;
  BMEditSelection *ese;
  BMIter iter;
  int i;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_BM(bm_old);

  /* allocate a bmesh */
  BMeshCreateParams params{};
  params.use_toolflags = bm_old->use_toolflags;
  bm_new = BM_mesh_create(&allocsize, &params);

  BM_mesh_copy_init_customdata(bm_new, bm_old, &allocsize);

  vtable = static_cast<BMVert **>(
      MEM_mallocN(sizeof(BMVert *) * bm_old->totvert, "BM_mesh_copy vtable"));
  etable = static_cast<BMEdge **>(
      MEM_mallocN(sizeof(BMEdge *) * bm_old->totedge, "BM_mesh_copy etable"));
  ftable = static_cast<BMFace **>(
      MEM_mallocN(sizeof(BMFace *) * bm_old->totface, "BM_mesh_copy ftable"));

  BM_ITER_MESH_INDEX (v, &iter, bm_old, BM_VERTS_OF_MESH, i) {
    /* copy between meshes so can't use 'example' argument */
    v_new = BM_vert_create(bm_new, v->co, nullptr, BM_CREATE_SKIP_CD);
    BM_elem_attrs_copy_ex(bm_old, bm_new, v, v_new, 0xff, 0x0);
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
                           BM_CREATE_SKIP_CD);

    BM_elem_attrs_copy_ex(bm_old, bm_new, e, e_new, 0xff, 0x0);
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
  for (ese = static_cast<BMEditSelection *>(bm_old->selected.first); ese; ese = ese->next) {
    BMElem *ele = nullptr;

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
        eletable = nullptr;
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
