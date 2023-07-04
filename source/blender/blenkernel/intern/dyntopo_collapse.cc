#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_alloca.h"
#include "BLI_array.hh"
#include "BLI_asan.h"
#include "BLI_buffer.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_math.h"
#include "BLI_math_vector.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "PIL_time.h"
#include "atomic_ops.h"

#include "BKE_customdata.h"
#include "BKE_dyntopo.hh"
#include "BKE_paint.h"
#include "BKE_pbvh_api.hh"
#include "BKE_sculpt.h"

#include "../../bmesh/intern/bmesh_collapse.hh"
#include "bmesh.h"
#include "bmesh_log.h"

#include "dyntopo_intern.hh"
#include "pbvh_intern.hh"

#include <array>
#include <cstdio>
#include <functional>
#include <type_traits>

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

template<typename T, typename SumT = T>
static void snap_corner_data(
    BMesh *bm, BMEdge *e, BMVert *v_del, Span<BMLoop *> ls, int cd_offset, bool snap_midpoint)
{
  using namespace blender;

  Vector<Vector<void *, 24>, 4> blocks;
  Vector<Vector<float, 24>, 4> weights;
  Vector<Vector<BMLoop *, 24>, 4> loops;
  Vector<BMLoop *, 24> final_loops;
  int cur_set = 0;

  for (int i : ls.index_range()) {
    ls[i]->head.index = -1;
    ls[i]->next->head.index = -1; /* So we can test l->next without checking if it's inside ls.*/
  }

  // XXX todo: preserve UV pins

  /* Build snapping sets (of UV vertices). */
  for (BMLoop *l1 : ls) {
    if (l1->head.index != -1) {
      continue;
    }

    l1->head.index = cur_set;

    blocks.resize(cur_set + 1);
    weights.resize(cur_set + 1);
    loops.resize(cur_set + 1);

    final_loops.append(l1);

    blocks[cur_set].append(l1->head.data);
    weights[cur_set].append(l1->v == v_del && !snap_midpoint ? 0.0f : 1.0f);
    loops[cur_set].append(l1);

    T uv1 = *BM_ELEM_CD_PTR<T *>(l1, cd_offset);

    T uvnext = *BM_ELEM_CD_PTR<T *>(l1->next, cd_offset);
    float limit = max_ff(math::distance_squared(uv1, uvnext) * 0.1f, 0.00001f);

    for (BMLoop *l2 : ls) {
      T uv2 = *BM_ELEM_CD_PTR<T *>(l2, cd_offset);

      if (l2 == l1 || l2->head.index != -1) {
        continue;
      }

      if (math::distance_squared(uv1, uv2) < limit) {
        l2->head.index = cur_set;
        blocks[cur_set].append(l2->head.data);
        weights[cur_set].append(l2->v == v_del && !snap_midpoint ? 0.0f : 1.0f);
        loops[cur_set].append(l2);
      }
    }

    cur_set++;
  }

  /* Merge sets */
  for (int set1 : final_loops.index_range()) {
    if (!final_loops[set1]) {
      continue;
    }

    BMLoop *final_l = final_loops[set1];
    BMLoop *next_l = final_l->next;

    if (final_l->e != e || next_l->head.index == -1 || next_l->head.index == set1) {
      continue;
    }

    int set2 = next_l->head.index;

    for (void *block : blocks[set2]) {
      blocks[set1].append(block);
    }
    for (float w : weights[set2]) {
      weights[set1].append(w);
    }

    for (BMLoop *l : loops[set2]) {
      l->head.index = set1;
    }

    /* Flag set as deleted. */
    final_loops[set2] = nullptr;
  }

  /* Perform final UV snapping. */
  for (int set : final_loops.index_range()) {
    BMLoop *final_l = final_loops[set];

    if (!final_l) {
      continue;
    }

    float totw = 0.0f;
    for (float w : weights[set]) {
      totw += w;
    }

    if (totw == 0.0f) {
      BLI_assert_unreachable();

      /* You never know with topology; this could happen. . .in that case
       * just use uniform weights.
       */

      for (int i : weights[set].index_range()) {
        weights[set][i] = 1.0f;
      }

      totw = float(weights[set].size());
    }

    totw = 1.0f / totw;
    for (int i : weights[set].index_range()) {
      weights[set][i] *= totw;
    }

    SumT sum = {};

    /* TODO: port this code to a BMesh library function and use it in the collapse_uvs BMesh
     * operator. */

    /* Use minmax centroid.  Seems like the edge weights should have summed so their centroid
     * is the same as a minmax centroid,but apparently not, and this is how editmode does it.
     */
#if 1
    SumT min = {}, max = {};

    for (int i = 0; i < SumT::type_length; i++) {
      min[i] = FLT_MAX;
      max[i] = FLT_MIN;
    }

    for (int i : blocks[set].index_range()) {
      T value1 = *static_cast<T *>(POINTER_OFFSET(blocks[set][i], cd_offset));
      SumT value;

      if (weights[set][i] == 0.0f) {
        continue;
      }

      if constexpr (std::is_same_v<T, uchar4>) {
        value = SumT(value1[0], value1[1], value1[2], value1[3]);
      }
      else {
        value = value1;
      }

      min = math::min(value, min);
      max = math::max(value, max);
    }

    sum = (min + max) * 0.5f;
#else /* Original centroid version. */
    for (int i : blocks[set].index_range()) {
      T value = *static_cast<T *>(POINTER_OFFSET(blocks[set][i], cd_offset));

      if (std::is_same_v<T, uchar4>) {
        sum[0] += float(value[0]) * weights[set][i];
        sum[1] += float(value[1]) * weights[set][i];
        sum[2] += float(value[2]) * weights[set][i];
        sum[3] += float(value[3]) * weights[set][i];
      }
      else {
        sum += value * weights[set][i];
      }
    }
#endif

    if constexpr (std::is_same_v<T, SumT>) {
      *BM_ELEM_CD_PTR<T *>(final_l, cd_offset) = sum;
    }
    else if constexpr (std::is_same_v<T, uchar4>) {
      *BM_ELEM_CD_PTR<T *>(final_l, cd_offset) = {
          uchar(sum[0]), uchar(sum[1]), uchar(sum[2]), uchar(sum[3])};
    }
  }

  /* Copy snapped UVs to all loops. */
  for (BMLoop *l : ls) {
    if (l->head.index == -1) {
      printf("%s: Error: invalid uv set for loop\n", __func__);
      continue;
    }

    int set = l->head.index;

    if (final_loops[set] && l != final_loops[set]) {
      *BM_ELEM_CD_PTR<T *>(l, cd_offset) = *BM_ELEM_CD_PTR<T *>(final_loops[set], cd_offset);
    }
  }
}

bool pbvh_bmesh_collapse_edge_uvs(
    PBVH *pbvh, BMEdge *e, BMVert *v_conn, BMVert *v_del, EdgeQueueContext *eq_ctx)
{
  BMesh *bm = pbvh->header.bm;

  pbvh_check_vert_boundary_bmesh(pbvh, v_conn);
  pbvh_check_vert_boundary_bmesh(pbvh, v_del);

  int boundflag1 = BM_ELEM_CD_GET_INT(v_conn, pbvh->cd_boundary_flag);
  // int boundflag2 = BM_ELEM_CD_GET_INT(v_del, pbvh->cd_boundary_flag);

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
        &bm->vdata, v_blocks, v_ws, nullptr, 2, v_conn->head.data, typemask);

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

  if (!e->l) {
    return snap;
  }

  Vector<BMLoop *, 32> ls;

  /* Append loops around edge first. */
  BMLoop *l = e->l;
  do {
    ls.append(l);
  } while ((l = l->radial_next) != e->l);

  /* Now find loops around e->v1 and e->v2. */
  for (BMVert *v : std::array<BMVert *, 2>({e->v1, e->v2})) {
    BMEdge *e = v->e;
    do {
      BMLoop *l = e->l;

      if (!l) {
        continue;
      }

      do {
        BMLoop *uv_l = l->v == v ? l : l->next;

        ls.append_non_duplicates(uv_l);
      } while ((l = l->radial_next) != e->l);
    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
  }

  for (int i : IndexRange(bm->ldata.totlayer)) {
    CustomDataLayer *layer = &bm->ldata.layers[i];

    if (layer->flag & CD_FLAG_ELEM_NOINTERP) {
      continue;
    }

    switch (layer->type) {
      case CD_PROP_FLOAT:
        snap_corner_data<VecBase<float, 1>>(bm, e, v_del, ls, layer->offset, snap);
        break;
      case CD_PROP_FLOAT2:
        snap_corner_data<float2>(bm, e, v_del, ls, layer->offset, snap);
        break;
      case CD_PROP_FLOAT3:
        snap_corner_data<float3>(bm, e, v_del, ls, layer->offset, snap);
        break;
      case CD_PROP_COLOR:
        snap_corner_data<float4>(bm, e, v_del, ls, layer->offset, snap);
        break;
      case CD_PROP_BYTE_COLOR:
        snap_corner_data<uchar4>(bm, e, v_del, ls, layer->offset, snap);
        break;
      default:
        break;
    }
  }

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
  int e_boundflag = BM_ELEM_CD_GET_INT(e, pbvh->cd_edge_boundary);

  /* Don't collapse across boundaries. */
  if ((boundflag1 & SCULPTVERT_ALL_BOUNDARY) != (boundflag2 & SCULPTVERT_ALL_BOUNDARY)) {
    return nullptr;
  }

  if ((boundflag1 & SCULPTVERT_ALL_BOUNDARY) != (e_boundflag & SCULPTVERT_ALL_BOUNDARY)) {
    return nullptr;
  }

  float w1 = mask_cb ? 1.0f - mask_cb({reinterpret_cast<intptr_t>(v1)}, mask_cb_data) : 0.0f;
  float w2 = mask_cb ? 1.0f - mask_cb({reinterpret_cast<intptr_t>(v2)}, mask_cb_data) : 0.0f;

  bool corner1 = (boundflag1 & SCULPTVERT_ALL_CORNER) || w1 >= 0.85;
  bool corner2 = (boundflag2 & SCULPTVERT_ALL_CORNER) || w2 >= 0.85;

  /* We allow two corners of the same type[s] to collapse */
  if ((boundflag1 & SCULPTVERT_ALL_CORNER) != (boundflag2 & SCULPTVERT_ALL_CORNER)) {
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
