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
#include "BKE_sculpt.h"

#include "../../bmesh/intern/bmesh_collapse.hh"
#include "bmesh.h"
#include "bmesh_log.h"

#include "dyntopo_intern.hh"
#include "pbvh_intern.hh"

#include <array>
#include <cstdio>
#include <functional>

using blender::float2;
using blender::float3;
using blender::float4;
using blender::IndexRange;
using blender::Map;
using blender::MutableSpan;
using blender::Set;
using blender::Span;
using blender::Vector;

namespace blender::bke::dyntopo {

struct TraceData {
  PBVH *pbvh;
  BMEdge *e;
  SculptSession *ss;
};

template<typename T = BMVert> static void check_new_elem_id(T *elem, PBVH *pbvh)
{
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

bool pbvh_bmesh_collapse_edge_uvs(
    PBVH *pbvh, BMEdge *e, BMVert *v_conn, BMVert *v_del, EdgeQueueContext *eq_ctx)
{
  pbvh_check_vert_boundary_bmesh(pbvh, v_conn);
  pbvh_check_vert_boundary_bmesh(pbvh, v_del);

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
    return false;
  }
  if ((boundflag1 & SCULPT_BOUNDARY_SEAM) && !(e->head.hflag & BM_ELEM_SEAM)) {
    return false;
  }

  bool snap = !(boundflag1 & SCULPTVERT_ALL_CORNER);

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
    eCustomDataMask typemask = CD_MASK_PROP_ALL;

    CustomData_bmesh_interp_ex(
        &pbvh->header.bm->vdata, v_blocks, v_ws, nullptr, 2, v_conn->head.data, typemask);

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

#if 0
  if (!e->l) {
    return snap;
  }

  /* Deal with UVs. */
  BMLoop *l = e->l;
  float ws[2];
  BMLoop *ls[2];
  const void *blocks[2];

  do {
    if (l->v == v_conn) {
      ls[0] = l;
      ls[1] = l->next;
    }
    else {
      ls[0] = l->next;
      ls[1] = l;
    }

    if (snap) {
      ws[0] = ws[1] = 0.5f;
    }
    else {
      ws[0] = l->v == v_conn ? 1.0f : 0.0f;
      ws[1] = l->v == v_conn ? 0.0f : 0.0f;
    }

    blocks[0] = ls[0]->head.data;
    blocks[1] = ls[1]->head.data;

#  if 0
    CustomData_bmesh_interp(&bm->ldata, blocks, ws, nullptr, 2, l->head.data);
    CustomData_bmesh_copy_data(
        &bm->ldata, &pbvh->header.bm->ldata, l->head.data, &l->next->head.data);
#  else

    PBVHVertRef vertex = {reinterpret_cast<intptr_t>(v_conn)};
    blender::bke::sculpt::interp_face_corners(
        pbvh, vertex, Span<BMLoop *>(ls, 2), Span<float>(ws, 2), 1.0f, pbvh->cd_boundary_flag);
#  endif
  } while ((l = l->radial_next) != e->l);

  return snap;
#endif

  const float limit = 0.005;
  BMLoop *l = e->l;

  for (BMVert *v : std::array<BMVert *, 2>({e->v1, e->v2})) {
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

  l = e->l;
  if (!l) {
    return snap;
  }

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

            if (delta < limit) {
              l3->head.index |= flag;
              copy_v2_v2(luv2, luv1);
            }
          }
        } while ((l2 = l2->radial_next) != e2->l);
      } while ((e2 = BM_DISK_EDGE_NEXT(e2, v)) != v->e);
    }
  } while ((l = l->radial_next) != e->l);

  return snap;
}

class DyntopoCollapseCallbacks {
  PBVH *pbvh;
  BMesh *bm;

 public:
  DyntopoCollapseCallbacks(PBVH *pbvh_) : pbvh(pbvh_), bm(pbvh_->header.bm) {}

  inline void on_vert_kill(BMVert *v)
  {
    BM_log_vert_removed(bm, pbvh->bm_log, v);
    pbvh_bmesh_vert_remove(pbvh, v);
    BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(v), false);
  }
  inline void on_edge_kill(BMEdge *e)
  {
    dyntopo_add_flag(pbvh, e->v1, SCULPTFLAG_NEED_VALENCE);
    dyntopo_add_flag(pbvh, e->v2, SCULPTFLAG_NEED_VALENCE);

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
    check_new_elem_id(v, pbvh);
    pbvh_boundary_update_bmesh(pbvh, v);
    dyntopo_add_flag(pbvh, v, SCULPTFLAG_NEED_VALENCE);
    BM_log_vert_added(bm, pbvh->bm_log, v);
  }

  inline void on_vert_combine(BMVert *dest, BMVert *source)
  {
    /* Combine boundary flags. */
    int boundflag = BM_ELEM_CD_GET_INT(source, pbvh->cd_boundary_flag);
    BM_ELEM_CD_SET_INT(dest, pbvh->cd_boundary_flag, boundflag);

    dyntopo_add_flag(pbvh, dest, SCULPTFLAG_NEED_VALENCE);
  }

  inline void on_edge_combine(BMEdge *dest, BMEdge *source)
  {
    dyntopo_add_flag(pbvh, dest->v1, SCULPTFLAG_NEED_VALENCE);
    dyntopo_add_flag(pbvh, dest->v2, SCULPTFLAG_NEED_VALENCE);

    /* Combine boundary flags. */
    int boundflag = BM_ELEM_CD_GET_INT(source, pbvh->cd_edge_boundary);
    BM_ELEM_CD_SET_INT(dest, pbvh->cd_edge_boundary, boundflag);

    pbvh_boundary_update_bmesh(pbvh, dest->v1);
    pbvh_boundary_update_bmesh(pbvh, dest->v2);
  }

  inline void on_edge_create(BMEdge *e)
  {
    dyntopo_add_flag(pbvh, e->v1, SCULPTFLAG_NEED_VALENCE);
    dyntopo_add_flag(pbvh, e->v2, SCULPTFLAG_NEED_VALENCE);

    check_new_elem_id(e, pbvh);
    pbvh_boundary_update_bmesh(pbvh, e);
    BM_log_edge_added(bm, pbvh->bm_log, e);
  }
  inline void on_face_create(BMFace *f)
  {
    check_new_elem_id(f, pbvh);
    BM_log_face_added(bm, pbvh->bm_log, f);
    BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);
    BKE_pbvh_bmesh_add_face(pbvh, f, false, false);

    BMLoop *l = f->l_first;
    do {
      dyntopo_add_flag(pbvh, l->v, SCULPTFLAG_NEED_VALENCE);
      pbvh_boundary_update_bmesh(pbvh, l->v);
      pbvh_boundary_update_bmesh(pbvh, l->e);
    } while ((l = l->next) != f->l_first);
  }
};

/*
 * This function is rather complicated.  It has to
 * snap UVs, log geometry and free ids.
 */
BMVert *EdgeQueueContext::collapse_edge(PBVH *pbvh, BMEdge *e, BMVert *v1, BMVert *v2)
{
  BMVert *v_del, *v_conn;

  if (pbvh->dyntopo_stop) {
    return nullptr;
  }

  PBVH_CHECK_NAN(v1->co);
  PBVH_CHECK_NAN(v2->co);

  TraceData tdata;

  tdata.ss = ss;
  tdata.pbvh = pbvh;
  tdata.e = e;

  pbvh_bmesh_check_nodes(pbvh);

  const int updateflag = SCULPTFLAG_NEED_VALENCE;

  validate_edge(pbvh, e);

  check_vert_fan_are_tris(pbvh, e->v1);
  check_vert_fan_are_tris(pbvh, e->v2);

  pbvh_bmesh_check_nodes(pbvh);

  pbvh_check_vert_boundary_bmesh(pbvh, v1);
  pbvh_check_vert_boundary_bmesh(pbvh, v2);

  int boundflag1 = BM_ELEM_CD_GET_INT(v1, pbvh->cd_boundary_flag);
  int boundflag2 = BM_ELEM_CD_GET_INT(v2, pbvh->cd_boundary_flag);

  /* Don't collapse across boundaries. */
  if ((boundflag1 & SCULPTVERT_ALL_BOUNDARY) != (boundflag2 & SCULPTVERT_ALL_BOUNDARY)) {
    return nullptr;
  }

  float w1 = mask_cb ? 1.0f - mask_cb({reinterpret_cast<intptr_t>(v1)}, mask_cb_data) : 0.0f;
  float w2 = mask_cb ? 1.0f - mask_cb({reinterpret_cast<intptr_t>(v2)}, mask_cb_data) : 0.0f;

  bool corner1 = (boundflag1 & SCULPTVERT_ALL_CORNER) || w1 >= 0.85;
  bool corner2 = (boundflag2 & SCULPTVERT_ALL_CORNER) || w2 >= 0.85;

  /* We allow two corners of the same type[s] to collapse */
  if ((boundflag1 & SCULPTVERT_ALL_CORNER) != (boundflag2 & SCULPTVERT_ALL_CORNER))
  {
    return nullptr;
  }

  if (w1 >= 0.85 && w2 >= 0.85) {
    return nullptr;
  }

  /* One of the two vertices may be masked or a corner,
   * select the correct one for deletion.
   */
  if (corner2 && !corner1) {
    v_del = v1;
    v_conn = v2;
  }
  else {
    v_del = v2;
    v_conn = v1;
  }

  bool non_manifold_v1 = vert_is_nonmanifold(e->v1);
  bool non_manifold_v2 = vert_is_nonmanifold(e->v2);

  /* Do not collapse non-manifold verts into manifold ones. */
  if (non_manifold_v1 != non_manifold_v2) {
    return nullptr;
  }

  DyntopoCollapseCallbacks callbacks(pbvh);

  /* Make sure original data is initialized before we snap it. */
  BKE_pbvh_bmesh_check_origdata(ss, v_conn, pbvh->stroke_id);
  BKE_pbvh_bmesh_check_origdata(ss, v_del, pbvh->stroke_id);

  pbvh_bmesh_check_nodes(pbvh);

  /* Snap UVS. */
  bool uvs_snapped = pbvh_bmesh_collapse_edge_uvs(pbvh, e, v_conn, v_del, this);
  validate_vert(pbvh, v_conn, CHECK_VERT_FACES | CHECK_VERT_NODE_ASSIGNED);

  if (uvs_snapped) {
    interp_v3_v3v3(v_conn->co, v_del->co, v_conn->co, 0.5f);
  }

  /* Full non-manifold collapse. */
  blender::bmesh::join_vert_kill_edge(pbvh->header.bm, e, v_del, true, true, callbacks);

  if (BM_elem_is_free((BMElem *)v_conn, BM_VERT)) {
    printf("v_conn was freed\n");
    return nullptr;
  }

  validate_vert(pbvh, v_conn, CHECK_VERT_FACES | CHECK_VERT_NODE_ASSIGNED);

  pbvh_boundary_update_bmesh(pbvh, v_conn);
  dyntopo_add_flag(pbvh, v_conn, updateflag);

  if (!v_conn->e) {
    printf("%s: pbvh error, v_conn->e was null\n", __func__);
    return v_conn;
  }

  if (v_conn->e) {
    BMEdge *e2 = v_conn->e;
    do {
      validate_edge(pbvh, e2);
    } while ((e2 = BM_DISK_EDGE_NEXT(e2, v_conn)) != v_conn->e);

    /* Flag boundaries for update. */
    e2 = v_conn->e;
    do {
      BMLoop *l = e2->l;

      pbvh_boundary_update_bmesh(pbvh, e2);

      if (!l) {
        BMVert *v2 = BM_edge_other_vert(e2, v_conn);
        pbvh_boundary_update_bmesh(pbvh, v2);
        dyntopo_add_flag(pbvh, v2, updateflag);
        continue;
      }

      do {
        BMLoop *l2 = l->f->l_first;
        do {
          pbvh_boundary_update_bmesh(pbvh, l2->v);
          pbvh_boundary_update_bmesh(pbvh, l2->e);
          dyntopo_add_flag(pbvh, l2->v, updateflag);
        } while ((l2 = l2->next) != l->f->l_first);
      } while ((l = l->radial_next) != e2->l);
    } while ((e2 = BM_DISK_EDGE_NEXT(e2, v_conn)) != v_conn->e);
  }

  pbvh_bmesh_check_nodes(pbvh);

  /* Destroy wire edges */
  if (v_conn->e) {
    Vector<BMEdge *, 16> es;
    BMEdge *e2 = v_conn->e;

    do {
      if (!e2->l) {
        es.append(e2);
      }
    } while ((e2 = BM_DISK_EDGE_NEXT(e2, v_conn)) != v_conn->e);

    for (BMEdge *e2 : es) {
      BMVert *v2 = BM_edge_other_vert(e2, v_conn);

      BM_log_edge_removed(pbvh->header.bm, pbvh->bm_log, e2);
      BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(e2), true);
      BM_edge_kill(pbvh->header.bm, e2);

      dyntopo_add_flag(pbvh, v2, SCULPTFLAG_NEED_VALENCE | SCULPTFLAG_NEED_TRIANGULATE);
      BKE_sculpt_boundary_flag_update<PBVHVertRef>(ss, {reinterpret_cast<intptr_t>(v2)});
    }
  }

  if (!v_conn->e) {
    /* Delete isolated vertex. */
    if (BM_ELEM_CD_GET_INT(v_conn, pbvh->cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
      blender::bke::dyntopo::pbvh_bmesh_vert_remove(pbvh, v_conn);
    }

    BM_log_vert_removed(pbvh->header.bm, pbvh->bm_log, v_conn);
    BM_idmap_release(pbvh->bm_idmap, reinterpret_cast<BMElem *>(v_conn), true);
    BM_vert_kill(pbvh->header.bm, v_conn);

    return nullptr;
  }

  if (BM_ELEM_CD_GET_INT(v_conn, pbvh->cd_vert_node_offset) == DYNTOPO_NODE_NONE) {
    printf("%s: error: failed to remove vert from pbvh?  v_conn->e: %p v_conn->e->l: %p\n",
           __func__,
           v_conn->e,
           v_conn->e ? v_conn->e->l : nullptr);
  }

  validate_vert(pbvh, v_conn, CHECK_VERT_FACES | CHECK_VERT_NODE_ASSIGNED);

  if (v_conn) {
    check_for_fins(pbvh, v_conn);

    if (BM_elem_is_free((BMElem *)v_conn, BM_VERT)) {
      v_conn = nullptr;
    }
  }

  validate_vert(pbvh, v_conn, CHECK_VERT_FACES | CHECK_VERT_NODE_ASSIGNED);

  if (v_conn) {
    PBVH_CHECK_NAN(v_conn->co);
  }

  return v_conn;
}
}  // namespace blender::bke::dyntopo
