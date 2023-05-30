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
#include "BKE_dyntopo.hh"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "bmesh.h"
#include "bmesh_log.h"

#include "dyntopo_intern.hh"
#include "pbvh_intern.hh"

#include <functional>
#include <stdio.h>

using blender::float2;
using blender::float3;
using blender::float4;
using blender::IndexRange;
using blender::Map;
using blender::Set;
using blender::Vector;

namespace blender::bke::dyntopo {

typedef struct TraceData {
  PBVH *pbvh;
  BMEdge *e;
  SculptSession *ss;
} TraceData;

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

void vert_ring_do_apply(BMVert *v,
                        std::function<void(BMElem *elem, void *userdata)> callback,
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

static void collapse_ring_callback_pre(BMElem *elem, void *userdata)
{
  TraceData *data = static_cast<TraceData *>(userdata);

  BM_idmap_check_assign(data->pbvh->bm_idmap, elem);
  BMesh *bm = data->pbvh->header.bm;

  switch (elem->head.htype) {
    case BM_VERT: {
      BMVert *v = reinterpret_cast<BMVert *>(elem);

      dyntopo_add_flag(data->pbvh, v, SCULPTFLAG_NEED_VALENCE);

      BM_log_vert_removed(bm, data->pbvh->bm_log, v);
      pbvh_bmesh_vert_remove(data->pbvh, v);
      BM_idmap_release(data->pbvh->bm_idmap, elem, false);
      break;
    }
    case BM_EDGE: {
      BMEdge *e = reinterpret_cast<BMEdge *>(elem);
      BM_log_edge_removed(bm, data->pbvh->bm_log, e);
      BM_idmap_release(data->pbvh->bm_idmap, elem, false);
      break;
    }
    case BM_FACE: {
      BMFace *f = reinterpret_cast<BMFace *>(elem);
      BM_log_face_removed(bm, data->pbvh->bm_log, f);
      pbvh_bmesh_face_remove(data->pbvh, f, false, false, false);
      BM_idmap_release(data->pbvh->bm_idmap, elem, false);
      break;
    }
  }
}

static void check_new_elem_id(BMElem *elem, TraceData *data)
{
  int id = BM_ELEM_CD_GET_INT(elem, data->pbvh->bm_idmap->cd_id_off[int(elem->head.htype)]);
  if (id != BM_ID_NONE) {
    BMElem *existing = id < data->pbvh->bm_idmap->map_size ?
                           BM_idmap_lookup(data->pbvh->bm_idmap, id) :
                           nullptr;

    if (existing) {
      BM_idmap_release(data->pbvh->bm_idmap, existing, true);
    }

    BM_idmap_assign(data->pbvh->bm_idmap, elem, id);

    if (existing) {
      BM_idmap_check_assign(data->pbvh->bm_idmap, existing);
    }
  }
  else {
    BM_idmap_check_assign(data->pbvh->bm_idmap, elem);
  }
}

static void collapse_ring_callback_post(BMElem *elem, void *userdata)
{
  TraceData *data = static_cast<TraceData *>(userdata);
  BMesh *bm = data->pbvh->header.bm;

  switch (elem->head.htype) {
    case BM_VERT: {
      BMVert *v = reinterpret_cast<BMVert *>(elem);

      dyntopo_add_flag(data->pbvh, v, SCULPTFLAG_NEED_VALENCE);

      check_new_elem_id(elem, data);
      BM_log_vert_added(bm, data->pbvh->bm_log, v);
      break;
    }
    case BM_EDGE: {
      BMEdge *e = reinterpret_cast<BMEdge *>(elem);
      check_new_elem_id(elem, data);

      BM_log_edge_added(bm, data->pbvh->bm_log, e);
      break;
    }
    case BM_FACE: {
      BMFace *f = reinterpret_cast<BMFace *>(elem);
      check_new_elem_id(elem, data);

      BM_log_face_added(bm, data->pbvh->bm_log, f);
      BKE_pbvh_bmesh_add_face(data->pbvh, f, false, false);
      break;
    }
  }
}

static void vert_ring_do(BMVert *v,
                         BMVert *v_extra,
                         void (*callback)(BMElem *elem, void *userdata),
                         void *userdata,
                         int /*tag*/,
                         int /*facetag*/,
                         int /*depth*/)
{
  blender::Set<BMFace *, 128> faces;

  std::function<void(BMVert * v, int depth)> recurse = [&](BMVert *v, int depth) {
    if (!v->e) {
      return;
    }

    const int max_depth = 1;
    BMEdge *e = v->e;
    do {
      BMVert *v2 = BM_edge_other_vert(e, v);

      if (!e->l) {
        if (depth < max_depth) {
          recurse(v2, depth + 1);
        }
        continue;
      }

      BMLoop *l = e->l;
      do {
        faces.add(l->f);
      } while ((l = l->radial_next) != e->l);

      if (depth < max_depth) {
        recurse(v2, depth + 1);
      }
    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
  };

  recurse(v, 0);
  if (v_extra) {
    recurse(v_extra, 0);
  }

  blender::Set<BMVert *, 64> verts;
  blender::Set<BMEdge *, 128> edges;

  for (BMFace *f : faces) {
    BMLoop *l = f->l_first;

    do {
      bool bad = false;

      BMLoop *l2 = l->radial_next;
      do {
        if (!faces.contains(l2->f)) {
          bad = true;
          break;
        }
      } while ((l2 = l2->radial_next) != l);

      if (!bad) {
        edges.add(l->e);
      }
    } while ((l = l->next) != f->l_first);
  }

  for (BMFace *f : faces) {
    BMLoop *l = f->l_first;
    do {
      bool bad = false;
      BMEdge *e = l->v->e;

      do {
        if (!edges.contains(e)) {
          bad = true;
          break;
        }
      } while ((e = BM_DISK_EDGE_NEXT(e, l->v)) != l->v->e);

      if (!bad) {
        verts.add(l->v);
      }
    } while ((l = l->next) != f->l_first);
  }

  for (BMFace *f : faces) {
    callback(reinterpret_cast<BMElem *>(f), userdata);
  }
  for (BMEdge *e : edges) {
    callback(reinterpret_cast<BMElem *>(e), userdata);
  }
  for (BMVert *v2 : verts) {
    callback(reinterpret_cast<BMElem *>(v2), userdata);
  }
}

bool pbvh_bmesh_collapse_edge_uvs(
    PBVH *pbvh, BMEdge *e, BMVert *v_conn, BMVert *v_del, EdgeQueueContext *eq_ctx)
{
  bm_logstack_push();

  int boundflag1 = BM_ELEM_CD_GET_INT(v_conn, pbvh->cd_boundary_flag);
  int boundflag2 = BM_ELEM_CD_GET_INT(v_del, pbvh->cd_boundary_flag);

  int uvidx = pbvh->header.bm->ldata.typemap[CD_PROP_FLOAT2];
  CustomDataLayer *uv_layer = nullptr;
  int totuv = 0;

  if (uvidx >= 0) {
    uv_layer = pbvh->header.bm->ldata.layers + uvidx;
    totuv = 0;

    while (uvidx < pbvh->header.bm->ldata.totlayer &&
           pbvh->header.bm->ldata.layers[uvidx].type == CD_PROP_FLOAT2)
    {
      uvidx++;
      totuv++;
    }
  }

  /*have to check edge flags directly, vertex flag test above isn't specific enough and
    can sometimes let bad edges through*/
  if ((boundflag1 & SCULPT_BOUNDARY_SHARP_MARK) && (e->head.hflag & BM_ELEM_SMOOTH)) {
    bm_logstack_pop();
    return false;
  }
  if ((boundflag1 & SCULPT_BOUNDARY_SEAM) && !(e->head.hflag & BM_ELEM_SEAM)) {
    bm_logstack_pop();
    return false;
  }

  bool snap = !(boundflag2 & SCULPTVERT_ALL_CORNER);

  /* Snap non-UV attributes. */
  if (snap) {
    /* Save a few attributes we don't want to snap. */
    int ni_conn = BM_ELEM_CD_GET_INT(v_conn, pbvh->cd_vert_node_offset);
    StrokeID stroke_id;
    if (eq_ctx->ss->attrs.stroke_id) {
      stroke_id = blender::bke::paint::vertex_attr_get<StrokeID>({(intptr_t)v_conn},
                                                                 eq_ctx->ss->attrs.stroke_id);
    }

    const float v_ws[2] = {0.5f, 0.5f};
    const void *v_blocks[2] = {v_del->head.data, v_conn->head.data};

    CustomData_bmesh_interp(
        &pbvh->header.bm->vdata, v_blocks, v_ws, nullptr, 2, v_conn->head.data);

    /* Restore node index. */
    BM_ELEM_CD_SET_INT(v_conn, pbvh->cd_vert_node_offset, ni_conn);

    /* Restore v_conn's stroke id.  This is needed to avoid a nasty
     * bug in the layer brush that leads to an exploding mesh.
     */
    if (eq_ctx->ss->attrs.stroke_id) {
      blender::bke::paint::vertex_attr_set<StrokeID>(
          {(intptr_t)v_conn}, eq_ctx->ss->attrs.stroke_id, stroke_id);
    }
  }

  /* Deal with UVs. */
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

    float(*uv)[2] = static_cast<float(*)[2]>(BLI_array_alloca(uv, 4 * totuv));

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

  bm_logstack_pop();
  return snap;
}

/*
 * This function is rather complicated.  It has to
 * snap UVs, log geometry and free ids.
 */
BMVert *pbvh_bmesh_collapse_edge(PBVH *pbvh,
                                 BMEdge *e,
                                 BMVert *v1,
                                 BMVert *v2,
                                 GHash *deleted_verts,
                                 BLI_Buffer * /*deleted_faces*/,
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

  tdata.ss = eq_ctx->ss;
  tdata.pbvh = pbvh;
  tdata.e = e;

  const int mupdateflag = SCULPTFLAG_NEED_VALENCE;
  // updateflag |= SCULPTFLAG_NEED_TRIANGULATE;  // to check for non-manifold flaps

  validate_edge(pbvh, pbvh->header.bm, e, true, true);

  check_vert_fan_are_tris(pbvh, e->v1);
  check_vert_fan_are_tris(pbvh, e->v2);

  int boundflag1 = BM_ELEM_CD_GET_INT(v1, pbvh->cd_boundary_flag);
  int boundflag2 = BM_ELEM_CD_GET_INT(v2, pbvh->cd_boundary_flag);

  /* one of the two vertices may be masked, select the correct one for deletion */
  if (!(boundflag1 & SCULPTVERT_ALL_CORNER) || DYNTOPO_MASK(eq_ctx->cd_vert_mask_offset, v1) <
                                                   DYNTOPO_MASK(eq_ctx->cd_vert_mask_offset, v2))
  {
    v_del = v1;
    v_conn = v2;
  }
  else {
    v_del = v2;
    v_conn = v1;

    SWAP(int, boundflag1, boundflag2);
  }

  /* Don't collapse across boundaries. */
  if ((boundflag1 & SCULPTVERT_ALL_CORNER) ||
      (boundflag1 & SCULPTVERT_ALL_BOUNDARY) != (boundflag2 & SCULPTVERT_ALL_BOUNDARY))
  {
    bm_logstack_pop();
    return nullptr;
  }

  /* Make sure original data is initialized before we snap it. */
  BKE_pbvh_bmesh_check_origdata(eq_ctx->ss, v_conn, pbvh->stroke_id);
  BKE_pbvh_bmesh_check_origdata(eq_ctx->ss, v_del, pbvh->stroke_id);

  bool uvs_snapped = pbvh_bmesh_collapse_edge_uvs(pbvh, e, v_conn, v_del, eq_ctx);

  validate_vert_faces(pbvh, pbvh->header.bm, v_conn, false, true);

  BMEdge *e2;

  const int tag = COLLAPSE_TAG;
  const int facetag = COLLAPSE_FACE_TAG;
  const int log_rings = 1;

  if (deleted_verts) {
    BLI_ghash_insert(deleted_verts, (void *)v_del, nullptr);
  }

  vert_ring_do(e->v1, e->v2, collapse_ring_callback_pre, &tdata, tag, facetag, log_rings - 1);

  if (!uvs_snapped) {
    float co[3];

    copy_v3_v3(co, v_conn->co);

    /* Full non-manifold collapse. */
    BM_edge_collapse(pbvh->header.bm, e, v_del, true, true, true, true);
    copy_v3_v3(v_conn->co, co);
  }
  else {
    float co[3];

    add_v3_v3v3(co, v_del->co, v_conn->co);
    mul_v3_fl(co, 0.5f);

    /* Full non-manifold collapse. */
    BM_edge_collapse(pbvh->header.bm, e, v_del, true, true, true, true);
    copy_v3_v3(v_conn->co, co);
  }

  vert_ring_do(v_conn,
               nullptr,
               collapse_ring_callback_post,
               static_cast<void *>(&tdata),
               tag,
               facetag,
               log_rings - 1);

  if (!v_conn->e) {
    printf("%s: pbvh error, v_conn->e was null\n", __func__);
    return v_conn;
  }

  validate_vert_faces(pbvh, pbvh->header.bm, v_conn, false, true);

  /* Flag boundaries for update. */
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
        dyntopo_add_flag(pbvh, l2->v, mupdateflag);
      } while ((l2 = l2->next) != l->f->l_first);
    } while ((l = l->radial_next) != e2->l);
  } while ((e2 = BM_DISK_EDGE_NEXT(e2, v_conn)) != v_conn->e);

  pbvh_bmesh_check_nodes(pbvh);

  if (!v_conn) {
    bm_logstack_pop();
    return nullptr;
  }

  if (v_conn->e && !v_conn->e->l) {
    BM_log_edge_removed(pbvh->header.bm, pbvh->bm_log, v_conn->e);
    if (BM_idmap_get_id(pbvh->bm_idmap, reinterpret_cast<BMElem *>(v_conn->e)) != BM_ID_NONE) {
      BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(v_conn->e), true);
    }
    BM_edge_kill(pbvh->header.bm, v_conn->e);
  }

  if (!v_conn->e) {
    /* Delete isolated vertex. */
    if (BM_ELEM_CD_GET_INT(v_conn, pbvh->cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
      blender::bke::dyntopo::pbvh_bmesh_vert_remove(pbvh, v_conn);
    }

    BM_log_vert_removed(pbvh->header.bm, pbvh->bm_log, v_conn);
    BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(v_conn), true);
    BM_vert_kill(pbvh->header.bm, v_conn);

    bm_logstack_pop();
    return nullptr;
  }

  pbvh_boundary_update_bmesh(pbvh, v_conn);
  dyntopo_add_flag(pbvh, v_conn, mupdateflag);

  if (BM_ELEM_CD_GET_INT(v_conn, pbvh->cd_vert_node_offset) == DYNTOPO_NODE_NONE) {
    printf("%s: error: failed to remove vert from pbvh?  v_conn->e: %p v_conn->e->l\n",
           __func__,
           v_conn->e,
           v_conn->e ? v_conn->e->l : nullptr);
  }

  if (v_conn) {
    check_for_fins(pbvh, v_conn);
  }

  validate_vert_faces(pbvh, pbvh->header.bm, v_conn, false, true);

  bm_logstack_pop();
  PBVH_CHECK_NAN(v_conn->co);

  return v_conn;
}
}  // namespace blender::bke::dyntopo
