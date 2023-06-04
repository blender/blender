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
#include "BLI_timeit.hh"
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

#include "../../bmesh/intern/bmesh_collapse.hh"
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

const int COLLAPSE_TAG = 32;
const int COLLAPSE_FACE_TAG = 64;

template<typename T = BMVert> static void check_new_elem_id(T *elem, PBVH *pbvh)
{
#if 1
  int id = BM_ELEM_CD_GET_INT(elem, pbvh->bm_idmap->cd_id_off[int(elem->head.htype)]);
  if (id != BM_ID_NONE) {
    BMElem *existing = id < pbvh->bm_idmap->map_size ? BM_idmap_lookup(pbvh->bm_idmap, id) :
                                                       nullptr;

    if (existing) {
      BM_idmap_check_assign(pbvh->bm_idmap, reinterpret_cast<BMElem *>(elem));
      BM_idmap_release(pbvh->bm_idmap, existing, true);
    }

    BM_idmap_assign(pbvh->bm_idmap, reinterpret_cast<BMElem *>(elem), id);

    if (existing) {
      BM_idmap_check_assign(pbvh->bm_idmap, existing);
    }
  }
  else {
    BM_idmap_check_assign(pbvh->bm_idmap, reinterpret_cast<BMElem *>(elem));
  }
#else
  BM_idmap_check_assign(pbvh->bm_idmap, reinterpret_cast<BMElem *>(elem));
#endif
}

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
      pbvh_bmesh_face_remove(data->pbvh, f, false, true, true);
      BM_idmap_release(data->pbvh->bm_idmap, elem, false);
      break;
    }
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

      check_new_elem_id(v, data->pbvh);
      BM_log_vert_added(bm, data->pbvh->bm_log, v);
      break;
    }
    case BM_EDGE: {
      BMEdge *e = reinterpret_cast<BMEdge *>(elem);
      check_new_elem_id(e, data->pbvh);

      BM_log_edge_added(bm, data->pbvh->bm_log, e);
      break;
    }
    case BM_FACE: {
      BMFace *f = reinterpret_cast<BMFace *>(elem);
      check_new_elem_id(f, data->pbvh);

      BM_log_face_added(bm, data->pbvh->bm_log, f);
      BKE_pbvh_bmesh_add_face(data->pbvh, f, false, false);
      break;
    }
  }
}

static void tag_vert_ring(BMVert *v, const int tag, const int facetag)
{
  v->head.api_flag |= tag;

  BMEdge *e = v->e;
  do {
    BMVert *v_other = v == e->v1 ? e->v2 : e->v1;

    e->head.api_flag |= tag;
    v_other->head.api_flag |= tag;

    BMLoop *l = e->l;
    if (!l) {
      continue;
    }
    do {
      l->f->head.api_flag |= tag | facetag;
      BMLoop *l2 = l;
      do {
        l2->v->head.api_flag |= tag;
        l2->e->head.api_flag |= tag;
      } while ((l2 = l2->next) != l);
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
}

template<typename T = BMElem, typename Vec = Vector<T *>>
static void add_elem(T *elem, Vec &vec, int tag)
{
  if (elem->head.api_flag & tag) {
    elem->head.api_flag &= ~tag;
    vec.append(elem);
  }
}

template<typename VV = Vector<BMVert *>,
         typename EV = Vector<BMVert *>,
         typename FV = Vector<BMVert *>>

static void add_vert_to_ring(
    BMVert *v, const int tag, const int facetag, VV &verts, EV &edges, FV &faces)
{
  if (v->head.api_flag & tag) {
    return;
  }

  v->head.api_flag &= ~tag;

  if (!v->e) {
    return;
  }

  BMEdge *e = v->e;
  do {
    BMLoop *l = e->l;
    if (!l) {
      continue;
    }

    do {
      if (!(l->f->head.api_flag & facetag)) {
        return;
      }
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  verts.append(v);
}

template<typename VV = Vector<BMVert *>,
         typename EV = Vector<BMVert *>,
         typename FV = Vector<BMVert *>>

static void build_vert_ring(
    BMVert *v, const int tag, const int facetag, VV &verts, EV &edges, FV &faces)
{
  add_elem(v, verts, tag);

  BMEdge *e = v->e;
  do {
    BMVert *v_other = v == e->v1 ? e->v2 : e->v1;
    add_vert_to_ring(v_other, tag, facetag, verts, edges, faces);

    BMLoop *l = e->l;
    bool bad = false;

    if (!l) {
      continue;
    }
    do {
      if (!(l->f->head.api_flag & facetag)) {
        bad = true;
        break;
      }

      add_elem(l->f, faces, tag);
      BMLoop *l2 = l;
      do {
        add_vert_to_ring(l2->v, tag, facetag, verts, edges, faces);
      } while ((l2 = l2->next) != l);
    } while ((l = l->radial_next) != e->l);

    if (!bad) {
      add_elem(e, edges, tag);
    }
    else {
      e->head.api_flag &= ~tag;
    }
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  e = v->e;
  do {
    BMLoop *l = e->l;
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
}

static void vert_ring_do_(BMesh *bm,
                          BMVert *v,
                          BMVert *v_extra,
                          void (*callback)(BMElem *elem, void *userdata),
                          void *userdata,
                          const int tag,
                          const int facetag,
                          const int depth = 0)
{
  tag_vert_ring(v, tag, facetag);
  if (v_extra) {
    tag_vert_ring(v_extra, tag, facetag);
  }

  Vector<BMVert *, 32> verts;
  Vector<BMEdge *, 64> edges;
  Vector<BMFace *, 80> faces;

  build_vert_ring(v, tag, facetag, verts, edges, faces);
  if (v_extra) {
    build_vert_ring(v_extra, tag, facetag, verts, edges, faces);
  }

  for (BMFace *f : faces) {
    f->head.api_flag &= ~facetag;
  }

  // printf("%d %d %d\n", verts.size(), edges.size(), faces.size());

#if 0
  BMIter iter;
  BMVert *vi;
  BMEdge *ei;
  BMFace *fi;
  BM_ITER_MESH (vi, &iter, bm, BM_VERTS_OF_MESH) {
    vi->head.hflag &= ~BM_ELEM_SELECT;
  }
  BM_ITER_MESH (ei, &iter, bm, BM_EDGES_OF_MESH) {
    ei->head.hflag &= ~BM_ELEM_SELECT;
  }
  BM_ITER_MESH (fi, &iter, bm, BM_FACES_OF_MESH) {
    fi->head.hflag &= ~BM_ELEM_SELECT;
  }
#endif

  for (BMFace *f : faces) {
    f->head.hflag |= BM_ELEM_SELECT;
    callback(reinterpret_cast<BMElem *>(f), userdata);
  }

  BMEdge *exist_e = v_extra ? BM_edge_exists(v, v_extra) : nullptr;
  for (BMEdge *e : edges) {
    if (exist_e == e) {
      e->head.hflag &= ~BM_ELEM_SELECT;
    }
    else {
      e->head.hflag |= BM_ELEM_SELECT;
    }
    callback(reinterpret_cast<BMElem *>(e), userdata);
  }
  for (BMVert *v2 : verts) {
    v2->head.hflag |= BM_ELEM_SELECT;
    callback(reinterpret_cast<BMElem *>(v2), userdata);
  }
}

template<typename FaceSet = blender::Set<BMFace *>>
static void vert_ring_recurse(BMVert *v, FaceSet &faces, int depth)
{
  if (!v->e) {
    return;
  }

  BMEdge *e = v->e;
  do {
    BMVert *v2 = BM_edge_other_vert(e, v);

    if (!e->l) {
      if (depth > 0) {
        vert_ring_recurse(v2, faces, depth - 1);
      }
      continue;
    }

    BMLoop *l = e->l;
    do {
      faces.add(l->f);
    } while ((l = l->radial_next) != e->l);

    if (depth > 0) {
      vert_ring_recurse(v2, faces, depth - 1);
    }
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
}

static bool vert_is_nonmanifold(BMVert *v)
{
  if (!v->e) {
    return false;
  }

  BMEdge *e = v->e;
  do {
    if (e->l && e->l->radial_next != e->l && e->l->radial_next->radial_next != e->l) {
      return true;
    }
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  return false;
}

static void vert_ring_do(BMesh *bm,
                         BMVert *v,
                         BMVert *v_extra,
                         void (*callback)(BMElem *elem, void *userdata),
                         void *userdata,
                         int /*tag*/,
                         int /*facetag*/,
                         int max_depth = 0)
{
  blender::Set<BMFace *, 128> faces;

  vert_ring_recurse(v, faces, max_depth);
  if (v_extra) {
    vert_ring_recurse(v_extra, faces, max_depth);
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

  // printf("%d %d %d\n", verts.size(), edges.size(), faces.size());

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

class DyntopoCollapseCallbacks {
  PBVH *pbvh;
  BMesh *bm;

 public:
  DyntopoCollapseCallbacks(PBVH *pbvh_) : pbvh(pbvh_), bm(pbvh_->header.bm) {}

  inline void on_vert_kill(BMVert *v)
  {
    dyntopo_add_flag(pbvh, v, SCULPTFLAG_NEED_VALENCE);

    BM_log_vert_removed(bm, pbvh->bm_log, v);
    pbvh_bmesh_vert_remove(pbvh, v);
    BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(v), false);
  }
  inline void on_edge_kill(BMEdge *e)
  {
    BM_log_edge_removed(bm, pbvh->bm_log, e);
    BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(e), false);
  }
  inline void on_face_kill(BMFace *f)
  {
    BM_log_face_removed(bm, pbvh->bm_log, f);
    pbvh_bmesh_face_remove(pbvh, f, false, true, true);
    BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(f), false);
  }

  inline void on_vert_create(BMVert *v)
  {
    dyntopo_add_flag(pbvh, v, SCULPTFLAG_NEED_VALENCE);

    check_new_elem_id(v, pbvh);
    BM_log_vert_added(bm, pbvh->bm_log, v);
  }
  inline void on_edge_create(BMEdge *e)
  {
    check_new_elem_id(e, pbvh);
    BM_log_edge_added(bm, pbvh->bm_log, e);
  }
  inline void on_face_create(BMFace *f)
  {
    check_new_elem_id(f, pbvh);
    BM_log_face_added(bm, pbvh->bm_log, f);
    BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);
    BKE_pbvh_bmesh_add_face(pbvh, f, false, false);
  }
};

/*
 * This function is rather complicated.  It has to
 * snap UVs, log geometry and free ids.
 */
BMVert *pbvh_bmesh_collapse_edge(
    PBVH *pbvh, BMEdge *e, BMVert *v1, BMVert *v2, EdgeQueueContext *eq_ctx)
{
  bm_logstack_push();

  BMVert *v_del, *v_conn;

  if (pbvh->dyntopo_stop) {
    bm_logstack_pop();
    return nullptr;
  }

  TraceData tdata;

  tdata.ss = eq_ctx->ss;
  tdata.pbvh = pbvh;
  tdata.e = e;

  pbvh_bmesh_check_nodes(pbvh);

  const int mupdateflag = SCULPTFLAG_NEED_VALENCE;
  // updateflag |= SCULPTFLAG_NEED_TRIANGULATE;  // to check for non-manifold flaps

  validate_edge(pbvh, e);

  check_vert_fan_are_tris(pbvh, e->v1);
  check_vert_fan_are_tris(pbvh, e->v2);

  pbvh_bmesh_check_nodes(pbvh);

  pbvh_check_vert_boundary(pbvh, v1);
  pbvh_check_vert_boundary(pbvh, v2);

  int boundflag1 = BM_ELEM_CD_GET_INT(v1, pbvh->cd_boundary_flag);
  int boundflag2 = BM_ELEM_CD_GET_INT(v2, pbvh->cd_boundary_flag);

  if ((boundflag1 & SCULPT_BOUNDARY_UV) != (boundflag2 & SCULPT_BOUNDARY_UV)) {
    return nullptr;
  }

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

  /* Needed for vert_ring_do. */
  const int tag = COLLAPSE_TAG;
  const int facetag = COLLAPSE_FACE_TAG;
  int vert_ring_maxdepth = 0;

  bool non_manifold_v1 = vert_is_nonmanifold(e->v1);
  bool non_manifold_v2 = vert_is_nonmanifold(e->v2);

  if (non_manifold_v1 && non_manifold_v2) {
    vert_ring_maxdepth++;
  }
  /* Do not collapse non-manifold verts into manifold ones. */
  else if (non_manifold_v1 != non_manifold_v2) {
    return nullptr;
  }

#define USE_COLLAPSE_CALLBACKS

#ifdef USE_COLLAPSE_CALLBACKS
  DyntopoCollapseCallbacks callbacks(pbvh);
#endif

  /* Make sure original data is initialized before we snap it. */
  BKE_pbvh_bmesh_check_origdata(eq_ctx->ss, v_conn, pbvh->stroke_id);
  BKE_pbvh_bmesh_check_origdata(eq_ctx->ss, v_del, pbvh->stroke_id);

#ifndef USE_COLLAPSE_CALLBACKS
  /* Remove topology from PBVH and insert into bmlog. */
  vert_ring_do(pbvh->header.bm,
               e->v1,
               e->v2,
               collapse_ring_callback_pre,
               &tdata,
               tag,
               facetag,
               vert_ring_maxdepth);
#endif
  pbvh_bmesh_check_nodes(pbvh);

  /* Snap UVS. */
  bool uvs_snapped = pbvh_bmesh_collapse_edge_uvs(pbvh, e, v_conn, v_del, eq_ctx);
  validate_vert(pbvh, v_conn, CHECK_VERT_FACES | CHECK_VERT_NODE_ASSIGNED);

  BMEdge *e2;
  if (!uvs_snapped) {
    float co[3];

    copy_v3_v3(co, v_conn->co);
    copy_v3_v3(v_conn->co, co);
  }
  else {
    float co[3];

    add_v3_v3v3(co, v_del->co, v_conn->co);
    mul_v3_fl(co, 0.5f);

    copy_v3_v3(v_conn->co, co);
  }

  /* Full non-manifold collapse. */
#ifdef USE_COLLAPSE_CALLBACKS
  blender::bmesh::join_vert_kill_edge(pbvh->header.bm, e, v_del, true, true, callbacks);
#else
  BM_edge_collapse(pbvh->header.bm, e, v_del, true, true, true, true);
#endif

#ifndef USE_COLLAPSE_CALLBACKS
  vert_ring_do(pbvh->header.bm,
               v_conn,
               nullptr,
               collapse_ring_callback_post,
               static_cast<void *>(&tdata),
               tag,
               facetag,
               vert_ring_maxdepth);
#endif

  if (!v_conn->e) {
    printf("%s: pbvh error, v_conn->e was null\n", __func__);
    return v_conn;
  }

  if (v_conn->e) {
    BMEdge *e2 = v_conn->e;
    do {
      validate_edge(pbvh, e2);
    } while ((e2 = BM_DISK_EDGE_NEXT(e2, v_conn)) != v_conn->e);
  }
  validate_vert(pbvh, v_conn, CHECK_VERT_FACES | CHECK_VERT_NODE_ASSIGNED);

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

  if (0) {  // XXX v_conn->e && !v_conn->e->l) {
    BM_log_edge_removed(pbvh->header.bm, pbvh->bm_log, v_conn->e);
    if (BM_idmap_get_id(pbvh->bm_idmap, reinterpret_cast<BMElem *>(v_conn->e)) != BM_ID_NONE) {
      BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(v_conn->e), true);
    }
    BM_edge_kill(pbvh->header.bm, v_conn->e);
  }

  if (0) {  // XXX !v_conn->e) {
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

  validate_vert(pbvh, v_conn, CHECK_VERT_FACES | CHECK_VERT_NODE_ASSIGNED);

  bm_logstack_pop();
  PBVH_CHECK_NAN(v_conn->co);

  return v_conn;
}
}  // namespace blender::bke::dyntopo
