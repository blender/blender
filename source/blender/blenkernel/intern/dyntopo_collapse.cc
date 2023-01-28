#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_array.hh"
#include "BLI_asan.h"
#include "BLI_buffer.h"
#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BLI_alloca.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"
#include "atomic_ops.h"

#include "BKE_customdata.h"
#include "BKE_dyntopo.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "bmesh.h"
#include "bmesh_log.h"

#include "dyntopo_intern.hh"
#include "pbvh_intern.h"

#include <stdio.h>

using blender::float2;
using blender::float3;
using blender::float4;
using blender::IndexRange;
using blender::Map;
using blender::Set;
using blender::Vector;

namespace blender::dyntopo {

// copied from decimate modifier code
inline bool bm_edge_collapse_is_degenerate_topology(BMEdge *e_first)
{
  /* simply check that there is no overlap between faces and edges of each vert,
   * (excluding the 2 faces attached to 'e' and 'e' its self) */

  BMEdge *e_iter;

  /* clear flags on both disks */
  e_iter = e_first;
  do {
    if (!bm_edge_is_manifold_or_boundary(e_iter->l)) {
      return true;
    }
    bm_edge_tag_disable(e_iter);
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, e_first->v1)) != e_first);

  e_iter = e_first;
  do {
    if (!bm_edge_is_manifold_or_boundary(e_iter->l)) {
      return true;
    }
    bm_edge_tag_disable(e_iter);
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, e_first->v2)) != e_first);

  /* now enable one side... */
  e_iter = e_first;
  do {
    bm_edge_tag_enable(e_iter);
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, e_first->v1)) != e_first);

  /* ... except for the edge we will collapse, we know that's shared,
   * disable this to avoid false positive. We could be smart and never enable these
   * face/edge tags in the first place but easier to do this */
  // bm_edge_tag_disable(e_first);
  /* do inline... */
  {
#if 0
    BMIter iter;
    BMIter liter;
    BMLoop *l;
    BMVert *v;
    BM_ITER_ELEM (l, &liter, e_first, BM_LOOPS_OF_EDGE) {
      BM_elem_flag_disable(l->f, BM_ELEM_TAG);
      BM_ITER_ELEM (v, &iter, l->f, BM_VERTS_OF_FACE) {
        BM_elem_flag_disable(v, BM_ELEM_TAG);
      }
    }
#else
    /* we know each face is a triangle, no looping/iterators needed here */

    BMLoop *l_radial;
    BMLoop *l_face;

    l_radial = e_first->l;
    l_face = l_radial;
    BLI_assert(l_face->f->len == 3);
    BM_elem_flag_disable(l_face->f, BM_ELEM_TAG);
    BM_elem_flag_disable((l_face = l_radial)->v, BM_ELEM_TAG);
    BM_elem_flag_disable((l_face = l_face->next)->v, BM_ELEM_TAG);
    BM_elem_flag_disable((l_face->next)->v, BM_ELEM_TAG);
    l_face = l_radial->radial_next;
    if (l_radial != l_face) {
      BLI_assert(l_face->f->len == 3);
      BM_elem_flag_disable(l_face->f, BM_ELEM_TAG);
      BM_elem_flag_disable((l_face = l_radial->radial_next)->v, BM_ELEM_TAG);
      BM_elem_flag_disable((l_face = l_face->next)->v, BM_ELEM_TAG);
      BM_elem_flag_disable((l_face->next)->v, BM_ELEM_TAG);
    }
#endif
  }

  /* and check for overlap */
  e_iter = e_first;
  do {
    if (bm_edge_tag_test(e_iter)) {
      return true;
    }
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, e_first->v2)) != e_first);

  return false;
}

typedef struct TraceData {
  PBVH *pbvh;
  SmallHash visit;
  blender::Set<void *> visit2;
  BMEdge *e;
} TraceData;

ATTR_NO_OPT void col_on_vert_kill(BMesh *bm, BMVert *v, void *userdata)
{
  TraceData *data = (TraceData *)userdata;
  PBVH *pbvh = data->pbvh;

  if (BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
    // printf("vert pbvh remove!\n");
    blender::dyntopo::pbvh_bmesh_vert_remove(pbvh, v);
  }

  if (!BLI_smallhash_haskey(&data->visit, (uintptr_t)v)) {
    // printf("vert kill!\n");
    BM_log_vert_pre(pbvh->bm_log, v);
    BLI_smallhash_insert(&data->visit, (uintptr_t)v, nullptr);
#ifdef USE_NEW_IDMAP
    BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(v), true);
#endif
  }
}

ATTR_NO_OPT void col_on_edge_kill(BMesh *bm, BMEdge *e, void *userdata)
{
  TraceData *data = (TraceData *)userdata;
  PBVH *pbvh = data->pbvh;

  if (!BLI_smallhash_haskey(&data->visit, (uintptr_t)e)) {
    // printf("edge kill!\n");
    BM_log_edge_pre(pbvh->bm_log, e);
    BLI_smallhash_insert(&data->visit, (uintptr_t)e, nullptr);
#ifdef USE_NEW_IDMAP
    BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(e), true);
#endif
  }
}

ATTR_NO_OPT void col_on_face_kill(BMesh *bm, BMFace *f, void *userdata)
{
  TraceData *data = (TraceData *)userdata;
  PBVH *pbvh = data->pbvh;

  if (BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset) != DYNTOPO_NODE_NONE) {
    pbvh_bmesh_face_remove(pbvh, f, false, false, false);
  }

  if (!BLI_smallhash_haskey(&data->visit, (uintptr_t)f)) {
    BM_log_face_pre(pbvh->bm_log, f);
    BLI_smallhash_insert(&data->visit, (uintptr_t)f, nullptr);
#ifdef USE_NEW_IDMAP
    BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(f), true);
#endif
  }
}

ATTR_NO_OPT static void collapse_restore_id(BMIdMap *idmap, BMElem *elem)
{
  int id = BM_idmap_get_id(idmap, elem);

  if (id < 0 || id >= idmap->map_size || idmap->map[id]) {
    BM_idmap_alloc(idmap, elem);
  }
  else {
    BM_idmap_assign(idmap, elem, id);
  }
}

ATTR_NO_OPT void col_on_vert_add(BMesh *bm, BMVert *v, void *userdata)
{
  TraceData *data = (TraceData *)userdata;
  PBVH *pbvh = data->pbvh;

  if (!data->visit2.add(static_cast<void *>(v))) {
    // return;
  }

  pbvh_boundary_update_bmesh(pbvh, v);

  MSculptVert *mv = (MSculptVert *)BM_ELEM_CD_GET_VOID_P(v, data->pbvh->cd_sculpt_vert);
  mv->flag |= SCULPTVERT_NEED_VALENCE | SCULPTVERT_NEED_DISK_SORT;

  collapse_restore_id(pbvh->bm_idmap, (BMElem *)v);
  BM_log_vert_post(pbvh->bm_log, v);
}

ATTR_NO_OPT void col_on_edge_add(BMesh *bm, BMEdge *e, void *userdata)
{
  TraceData *data = (TraceData *)userdata;
  PBVH *pbvh = data->pbvh;

  if (!data->visit2.add(static_cast<void *>(e))) {
    // return;
  }

  collapse_restore_id(pbvh->bm_idmap, (BMElem *)e);
  BM_log_edge_post(pbvh->bm_log, e);
}

ATTR_NO_OPT void col_on_face_add(BMesh *bm, BMFace *f, void *userdata)
{
  TraceData *data = (TraceData *)userdata;
  PBVH *pbvh = data->pbvh;

  if (!data->visit2.add(static_cast<void *>(f))) {
    // return;
  }

  if (bm_elem_is_free((BMElem *)f, BM_FACE)) {
    printf("%s: error, f was freed!\n", __func__);
    return;
  }

  if (BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset) != DYNTOPO_NODE_NONE) {
    pbvh_bmesh_face_remove(pbvh, f, false, false, false);
  }

  collapse_restore_id(pbvh->bm_idmap, (BMElem *)f);
  BM_log_face_post(pbvh->bm_log, f);
  BKE_pbvh_bmesh_add_face(pbvh, f, false, false);
}

/* Faces *outside* the ring region are tagged with facetag, used to detect
 * border edges.
 */
ATTR_NO_OPT static void vert_ring_do_tag(BMVert *v, int tag, int facetag, int depth)
{

  BMEdge *e = v->e;
  do {
    BMVert *v2 = BM_edge_other_vert(e, v);

    if (depth > 0) {
      vert_ring_do_tag(v2, tag, facetag, depth - 1);
    }

    e->head.hflag |= tag;
    v2->head.hflag |= tag;

    if (!e->l) {
      continue;
    }

    BMLoop *l = e->l;
    do {
      l->f->head.hflag |= tag;

      BMLoop *l2 = l;
      do {
        l2->v->head.hflag |= tag;
        l2->e->head.hflag |= tag;
        l2->f->head.hflag |= tag;

        /*set up face tags for faces outside this region*/
        BMLoop *l3 = l2->radial_next;

        do {
          l3->f->head.hflag |= facetag;
        } while ((l3 = l3->radial_next) != l2);

      } while ((l2 = l2->next) != l);
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
}

ATTR_NO_OPT static void vert_ring_untag_inner_faces(BMVert *v, int tag, int facetag, int depth)
{
  if (!v->e) {
    return;
  }

  BMEdge *e = v->e;

  /* untag faces inside this region with facetag */
  do {
    BMLoop *l = e->l;

    if (depth > 0) {
      BMVert *v2 = BM_edge_other_vert(e, v);
      vert_ring_untag_inner_faces(v2, tag, facetag, depth - 1);
    }

    if (!l) {
      continue;
    }

    do {
      l->f->head.hflag &= ~facetag;
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
}

ATTR_NO_OPT void vert_ring_do_apply(BMVert *v,
                                    void (*callback)(BMElem *elem, void *userdata),
                                    void *userdata,
                                    int tag,
                                    int facetag,
                                    int depth)
{
  BMEdge *e = v->e;

  callback((BMElem *)v, userdata);
  v->head.hflag &= ~tag;

  e = v->e;
  do {
    BMVert *v2 = BM_edge_other_vert(e, v);

    if (depth > 0) {
      vert_ring_do_apply(v2, callback, userdata, tag, facetag, depth - 1);
    }

    if (v2->head.hflag & tag) {
      v2->head.hflag &= ~tag;
      callback((BMElem *)v2, userdata);
    }
    if (e->head.hflag & tag) {
      e->head.hflag &= ~tag;
      callback((BMElem *)e, userdata);
    }

    if (!e->l) {
      continue;
    }

    BMLoop *l = e->l;
    do {
      BMLoop *l2 = l;

      do {
        if (l2->v->head.hflag & tag) {
          l2->v->head.hflag &= ~tag;
          callback((BMElem *)l2->v, userdata);
        }

        if (l2->e->head.hflag & tag) {
          l2->e->head.hflag &= ~tag;
          callback((BMElem *)l2->e, userdata);
        }

        if (l2->f->head.hflag & tag) {
          l2->f->head.hflag &= ~tag;
          callback((BMElem *)l2->f, userdata);
        }
      } while ((l2 = l2->next) != l);
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
}

const int COLLAPSE_TAG = BM_ELEM_INTERNAL_TAG;
const int COLLAPSE_FACE_TAG = BM_ELEM_TAG_ALT;

ATTR_NO_OPT static void vert_ring_do(BMVert *v,
                                     void (*callback)(BMElem *elem, void *userdata),
                                     void *userdata,
                                     int tag,
                                     int facetag,
                                     int depth)
{
  if (!v->e) {
    v->head.hflag &= ~tag;
    callback((BMElem *)v, userdata);
    return;
  }

  vert_ring_do_tag(v, tag, facetag, depth);
  vert_ring_untag_inner_faces(v, tag, facetag, depth);
  vert_ring_do_apply(v, callback, userdata, tag, facetag, depth);
}

static void edge_ring_do(BMEdge *e,
                         void (*callback)(BMElem *elem, void *userdata),
                         void *userdata,
                         int tag,
                         int facetag,
                         int depth)
{

  vert_ring_do_tag(e->v1, tag, facetag, depth);
  vert_ring_do_tag(e->v2, tag, facetag, depth);

  vert_ring_untag_inner_faces(e->v1, tag, facetag, depth);
  vert_ring_untag_inner_faces(e->v2, tag, facetag, depth);

  vert_ring_do_apply(e->v1, callback, userdata, tag, facetag, depth);
  vert_ring_do_apply(e->v2, callback, userdata, tag, facetag, depth);

  return;
  for (int i = 0; i < 2; i++) {
    BMVert *v2 = i ? e->v2 : e->v1;
    BMEdge *e2 = v2->e;

    do {
      e2->head.hflag |= tag;
      e2->v1->head.hflag |= tag;
      e2->v2->head.hflag |= tag;

      if (!e2->l) {
        continue;
      }

      BMLoop *l = e2->l;

      do {
        BMLoop *l2 = l;
        do {
          l2->v->head.hflag |= tag;
          l2->e->head.hflag |= tag;
          l2->f->head.hflag |= tag;
        } while ((l2 = l2->next) != l);
      } while ((l = l->radial_next) != e2->l);
    } while ((e2 = BM_DISK_EDGE_NEXT(e2, v2)) != v2->e);
  }

  for (int i = 0; i < 2; i++) {
    BMVert *v2 = i ? e->v2 : e->v1;
    BMEdge *e2 = v2->e;

    if (v2->head.hflag & tag) {
      v2->head.hflag &= ~tag;
      callback((BMElem *)v2, userdata);
    }

    do {
      if (e2->head.hflag & tag) {
        e2->head.hflag &= ~tag;
        callback((BMElem *)e2, userdata);
      }

      if (!e2->l) {
        continue;
      }

      BMLoop *l = e2->l;

      do {
        BMLoop *l2 = l;
        do {
          if (l2->v->head.hflag & tag) {
            callback((BMElem *)l2->v, userdata);
            l2->v->head.hflag &= ~tag;
          }

          if (l2->e->head.hflag & tag) {
            callback((BMElem *)l2->e, userdata);
            l2->e->head.hflag &= ~tag;
          }

          if (l2->f->head.hflag & tag) {
            callback((BMElem *)l2->f, userdata);
            l2->f->head.hflag &= ~tag;
          }
        } while ((l2 = l2->next) != l);
      } while ((l = l->radial_next) != e2->l);
    } while ((e2 = BM_DISK_EDGE_NEXT(e2, v2)) != v2->e);
  }
}

/*
 * This function is rather complicated.  It has to
 * snap UVs, log geometry and free ids.
 */
ATTR_NO_OPT BMVert *pbvh_bmesh_collapse_edge(PBVH *pbvh,
                                             BMEdge *e,
                                             BMVert *v1,
                                             BMVert *v2,
                                             GHash *deleted_verts,
                                             BLI_Buffer *deleted_faces,
                                             EdgeQueueContext *eq_ctx)
{
  bm_logstack_push();

  BMVert *v_del, *v_conn;

  if (pbvh->dyntopo_stop) {
    bm_logstack_pop();
    return nullptr;
  }

  pbvh_check_vert_boundary(pbvh, v1);
  pbvh_check_vert_boundary(pbvh, v2);

  TraceData tdata;
  BLI_smallhash_init(&tdata.visit);

  tdata.pbvh = pbvh;
  tdata.e = e;

  const int mupdateflag = SCULPTVERT_NEED_VALENCE | SCULPTVERT_NEED_DISK_SORT;
  // updateflag |= SCULPTVERT_NEED_TRIANGULATE;  // to check for non-manifold flaps

  validate_edge(pbvh, pbvh->header.bm, e, true, true);

  check_vert_fan_are_tris(pbvh, e->v1);
  check_vert_fan_are_tris(pbvh, e->v2);

  MSculptVert *mv1 = BKE_PBVH_SCULPTVERT(pbvh->cd_sculpt_vert, v1);
  MSculptVert *mv2 = BKE_PBVH_SCULPTVERT(pbvh->cd_sculpt_vert, v2);
  int boundflag1 = BM_ELEM_CD_GET_INT(v1, pbvh->cd_boundary_flag);
  int boundflag2 = BM_ELEM_CD_GET_INT(v2, pbvh->cd_boundary_flag);

  /* one of the two vertices may be masked, select the correct one for deletion */
  if (!(boundflag1 & SCULPTVERT_ALL_CORNER) || DYNTOPO_MASK(eq_ctx->cd_vert_mask_offset, v1) <
                                                   DYNTOPO_MASK(eq_ctx->cd_vert_mask_offset, v2)) {
    v_del = v1;
    v_conn = v2;
  }
  else {
    v_del = v2;
    v_conn = v1;

    SWAP(MSculptVert *, mv1, mv2);
    SWAP(int, boundflag1, boundflag2);
  }

  if ((boundflag1 & SCULPTVERT_ALL_CORNER) ||
      (boundflag1 & SCULPTVERT_ALL_BOUNDARY) != (boundflag2 & SCULPTVERT_ALL_BOUNDARY)) {
    bm_logstack_pop();
    return nullptr;
  }

  int uvidx = pbvh->header.bm->ldata.typemap[CD_PROP_FLOAT2];
  CustomDataLayer *uv_layer = nullptr;
  int totuv = 0;

  if (uvidx >= 0) {
    uv_layer = pbvh->header.bm->ldata.layers + uvidx;
    totuv = 0;

    while (uvidx < pbvh->header.bm->ldata.totlayer &&
           pbvh->header.bm->ldata.layers[uvidx].type == CD_PROP_FLOAT2) {
      uvidx++;
      totuv++;
    }
  }

  /*have to check edge flags directly, vertex flag test above isn't specific enough and
    can sometimes let bad edges through*/
  if ((boundflag1 & SCULPT_BOUNDARY_SHARP) && (e->head.hflag & BM_ELEM_SMOOTH)) {
    bm_logstack_pop();
    return nullptr;
  }
  if ((boundflag1 & SCULPT_BOUNDARY_SEAM) && !(e->head.hflag & BM_ELEM_SEAM)) {
    bm_logstack_pop();
    return nullptr;
  }

  bool snap = !(boundflag2 & SCULPTVERT_ALL_CORNER);

  /* snap customdata */
  if (snap) {
    int ni_conn = BM_ELEM_CD_GET_INT(v_conn, pbvh->cd_vert_node_offset);

    const float v_ws[2] = {0.5f, 0.5f};
    const void *v_blocks[2] = {v_del->head.data, v_conn->head.data};

    CustomData_bmesh_interp(
        &pbvh->header.bm->vdata, v_blocks, v_ws, nullptr, 2, v_conn->head.data);
    BM_ELEM_CD_SET_INT(v_conn, pbvh->cd_vert_node_offset, ni_conn);
  }

  // deal with UVs
  if (e->l) {
    BMLoop *l = e->l;

    for (int step = 0; step < 2; step++) {
      BMVert *v = step ? e->v2 : e->v1;
      BMEdge *e2 = v->e;

      if (!e2) {
        continue;
      }

      do {
        BMLoop *l2 = e2->l;

        if (!l2) {
          continue;
        }

        do {
          BMLoop *l3 = l2->v != v ? l2->next : l2;

          /* store visit bits for each uv layer in l3->head.index */
          l3->head.index = 0;
        } while ((l2 = l2->radial_next) != e2->l);
      } while ((e2 = BM_DISK_EDGE_NEXT(e2, v)) != v->e);
    }

    float(*uv)[2] = BLI_array_alloca(uv, 4 * totuv);

    do {
      const void *ls2[2] = {l->head.data, l->next->head.data};
      float ws2[2] = {0.5f, 0.5f};

      if (!snap) {
        const int axis = l->v == v_del ? 0 : 1;

        ws2[axis] = 0.0f;
        ws2[axis ^ 1] = 1.0f;
      }

      for (int step = 0; uv_layer && step < 2; step++) {
        BMLoop *l1 = step ? l : l->next;

        for (int k = 0; k < totuv; k++) {
          float *luv = (float *)BM_ELEM_CD_GET_VOID_P(l1, uv_layer[k].offset);

          copy_v2_v2(uv[k * 2 + step], luv);
        }
      }

      CustomData_bmesh_interp(&pbvh->header.bm->ldata, ls2, ws2, nullptr, 2, l->head.data);
      CustomData_bmesh_copy_data(
          &pbvh->header.bm->ldata, &pbvh->header.bm->ldata, l->head.data, &l->next->head.data);

      for (int step = 0; totuv >= 0 && step < 2; step++) {
        BMVert *v = step ? l->next->v : l->v;
        BMLoop *l1 = step ? l->next : l;
        BMEdge *e2 = v->e;

        do {
          BMLoop *l2 = e2->l;

          if (!l2) {
            continue;
          }

          do {
            BMLoop *l3 = l2->v != v ? l2->next : l2;

            if (!l3 || l3 == l1 || l3 == l || l3 == l->next) {
              continue;
            }

            for (int k = 0; k < totuv; k++) {
              const int flag = 1 << k;

              if (l3->head.index & flag) {
                continue;
              }

              const int cd_uv = uv_layer[k].offset;

              float *luv1 = (float *)BM_ELEM_CD_GET_VOID_P(l1, cd_uv);
              float *luv2 = (float *)BM_ELEM_CD_GET_VOID_P(l3, cd_uv);

              float dx = luv2[0] - uv[k * 2 + step][0];
              float dy = luv2[1] - uv[k * 2 + step][1];

              float delta = dx * dx + dy * dy;

              if (delta < 0.001) {
                l3->head.index |= flag;
                copy_v2_v2(luv2, luv1);
              }
            }
          } while ((l2 = l2->radial_next) != e2->l);
        } while ((e2 = BM_DISK_EDGE_NEXT(e2, v)) != v->e);
      }
    } while ((l = l->radial_next) != e->l);
  }

  validate_vert_faces(pbvh, pbvh->header.bm, v_conn, false, true);

  BMEdge *e2;

  const int tag = COLLAPSE_TAG;
  const int facetag = COLLAPSE_FACE_TAG;
  const int log_rings = 1;

  // edge_ring_do(e, collapse_ring_callback_pre, &tdata, tag, facetag, log_rings - 1);

  blender::dyntopo::pbvh_bmesh_vert_remove(pbvh, v_del);

  BM_log_edge_pre(pbvh->bm_log, e);
  BLI_smallhash_reinsert(&tdata.visit, (uintptr_t)e, nullptr);
#ifdef USE_NEW_IDMAP
  BM_idmap_release(pbvh->bm_idmap, (BMElem *)e, true);
#endif

  BM_log_vert_removed(pbvh->bm_log, v_del, pbvh->cd_vert_mask_offset);
  BLI_smallhash_reinsert(&tdata.visit, (uintptr_t)v_del, nullptr);
#ifdef USE_NEW_IDMAP
  BM_idmap_release(pbvh->bm_idmap, (BMElem *)v_del, true);
#endif

  // edge_ring_do(e, collapse_ring_callback_pre2, &tdata, tag, facetag, log_rings - 1);

  if (deleted_verts) {
    BLI_ghash_insert(deleted_verts, (void *)v_del, nullptr);
  }

  pbvh_bmesh_check_nodes(pbvh);
  validate_vert_faces(pbvh, pbvh->header.bm, v_conn, false, true);

  BMTracer tracer;
  BM_empty_tracer(&tracer, &tdata);

  tracer.on_vert_kill = col_on_vert_kill;
  tracer.on_edge_kill = col_on_edge_kill;
  tracer.on_face_kill = col_on_face_kill;

  tracer.on_vert_create = col_on_vert_add;
  tracer.on_edge_create = col_on_edge_add;
  tracer.on_face_create = col_on_face_add;

  if (!snap) {
    float co[3];

    copy_v3_v3(co, v_conn->co);

    // full non-manifold collapse
    BM_edge_collapse(pbvh->header.bm, e, v_del, true, true, true, true, &tracer);
    copy_v3_v3(v_conn->co, co);
  }
  else {
    float co[3];

    add_v3_v3v3(co, v_del->co, v_conn->co);
    mul_v3_fl(co, 0.5f);

    // full non-manifold collapse
    BM_edge_collapse(pbvh->header.bm, e, v_del, true, true, true, true, &tracer);
    copy_v3_v3(v_conn->co, co);
  }

  if (!v_conn->e) {
    printf("%s: pbvh error, v_conn->e was null\n", __func__);
    return v_conn;
  }

  validate_vert_faces(pbvh, pbvh->header.bm, v_conn, false, true);

  e2 = v_conn->e;
  do {
    BMLoop *l = e2->l;

    if (!l) {
      continue;
    }

    do {
      BMLoop *l2 = l->f->l_first;
      do {
        pbvh_boundary_update_bmesh(pbvh, l2->v);

        MSculptVert *mv = BKE_PBVH_SCULPTVERT(pbvh->cd_sculpt_vert, l2->v);
        mv->flag |= mupdateflag;
      } while ((l2 = l2->next) != l->f->l_first);
    } while ((l = l->radial_next) != e2->l);
  } while ((e2 = BM_DISK_EDGE_NEXT(e2, v_conn)) != v_conn->e);

  pbvh_bmesh_check_nodes(pbvh);

  if (!v_conn) {
    bm_logstack_pop();
    return nullptr;
  }

  MSculptVert *mv_conn = BKE_PBVH_SCULPTVERT(pbvh->cd_sculpt_vert, v_conn);
  pbvh_boundary_update_bmesh(pbvh, v_conn);

  MV_ADD_FLAG(mv_conn, mupdateflag);

#if 0
  e2 = v_conn->e;
  BMEdge *enext;
  do {
    if (!e2) {
      break;
    }

    enext = BM_DISK_EDGE_NEXT(e2, v_conn);

    // kill wire edge
    if (!e2->l) {
      BM_log_edge_pre(pbvh->bm_log, e2);
      BM_idmap_release(pbvh->bm_idmap, (BMElem *)e2, true);
      BM_edge_kill(pbvh->header.bm, e2);
    }
  } while (v_conn->e && (e2 = enext) != v_conn->e);
#endif

  if (0) {
    BMElem *elem = nullptr;
    SmallHashIter siter;
    void **val = BLI_smallhash_iternew_p(&tdata.visit, &siter, (uintptr_t *)&elem);

    for (; val; val = BLI_smallhash_iternext_p(&siter, (uintptr_t *)&elem)) {
      if (bm_elem_is_free(elem, BM_EDGE) && bm_elem_is_free(elem, BM_VERT) &&
          bm_elem_is_free(elem, BM_FACE)) {
        continue;
      }

      switch (elem->head.htype) {
        case BM_VERT:
          if (!BM_log_has_vert_post(pbvh->bm_log, (BMVert *)elem)) {
            BM_log_vert_added(pbvh->bm_log, (BMVert *)elem, -1);
          }
          break;
        case BM_EDGE:
          if (!BM_log_has_edge_post(pbvh->bm_log, (BMEdge *)elem)) {
            BM_log_edge_added(pbvh->bm_log, (BMEdge *)elem);
          }
          break;
        case BM_FACE:
          if (!BM_log_has_face_post(pbvh->bm_log, (BMFace *)elem)) {
            BM_log_face_added(pbvh->bm_log, (BMFace *)elem);
          }
          break;
      }
    }
  }

  MSculptVert *mv3 = BKE_PBVH_SCULPTVERT(pbvh->cd_sculpt_vert, v_conn);
  pbvh_boundary_update_bmesh(pbvh, v_conn);

  MV_ADD_FLAG(mv3, mupdateflag);

  if (!v_conn->e) {
    // delete isolated vertex
    if (BM_ELEM_CD_GET_INT(v_conn, pbvh->cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
      blender::dyntopo::pbvh_bmesh_vert_remove(pbvh, v_conn);
    }

    // if (!BLI_smallhash_lookup(&tdata.visit, (intptr_t)v_conn)) {
    BM_log_vert_removed(pbvh->bm_log, v_conn, 0);
    //}

#ifdef USE_NEW_IDMAP
    BM_idmap_release(pbvh->bm_idmap, (BMElem *)v_conn, true);
#endif
    BM_vert_kill(pbvh->header.bm, v_conn);

    bm_logstack_pop();
    return nullptr;
  }

  if (BM_ELEM_CD_GET_INT(v_conn, pbvh->cd_vert_node_offset) == DYNTOPO_NODE_NONE) {
    printf("%s: error: failed to remove vert from pbvh?\n", __func__);
  }

#if 0

  e = v_conn->e;
  if (e) {
    do {
      enext = BM_DISK_EDGE_NEXT(e, v_conn);
      BMVert *v2 = BM_edge_other_vert(e, v_conn);

      MSculptVert *mv4 = BKE_PBVH_SCULPTVERT(pbvh->cd_sculpt_vert, v2);

    } while ((e = enext) != v_conn->e);
  }
#endif

  if (v_conn) {
    check_for_fins(pbvh, v_conn);
  }

  validate_vert_faces(pbvh, pbvh->header.bm, v_conn, false, true);

  bm_logstack_pop();
  PBVH_CHECK_NAN(v_conn->co);

  return v_conn;
}
}  // namespace blender::dyntopo
