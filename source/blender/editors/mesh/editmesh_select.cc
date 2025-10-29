/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include <algorithm>
#include <optional>

#include "MEM_guardedalloc.h"

#include "BLI_heap.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_utildefines_stack.h"
#include "BLI_vector.hh"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_transform.hh"
#include "ED_uvedit.hh"
#include "ED_view3d.hh"

#include "BLT_translation.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "bmesh_tools.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "DRW_select_buffer.hh"

#include "mesh_intern.hh" /* Own include. */

/** use #BMesh operator flags for a few operators. */
#define BMO_ELE_TAG 1

using blender::float3;
using blender::Span;
using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Generic Poll Functions
 * \{ */

static bool edbm_vert_or_edge_select_mode_poll(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_MESH) {
    const BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (em) {
      if (em->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) {
        return true;
      }
    }
  }

  CTX_wm_operator_poll_msg_set(C, "An edit-mesh with vertex or edge selection mode is required");

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common functions to count elements
 * \{ */

enum eElemCountType {
  ELEM_COUNT_LESS = 0,
  ELEM_COUNT_EQUAL,
  ELEM_COUNT_GREATER,
  ELEM_COUNT_NOT_EQUAL,
};

static const EnumPropertyItem elem_count_compare_items[] = {
    {ELEM_COUNT_LESS, "LESS", false, "Less Than", ""},
    {ELEM_COUNT_EQUAL, "EQUAL", false, "Equal To", ""},
    {ELEM_COUNT_GREATER, "GREATER", false, "Greater Than", ""},
    {ELEM_COUNT_NOT_EQUAL, "NOTEQUAL", false, "Not Equal To", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static inline bool is_count_a_match(const eElemCountType type,
                                    const int value_test,
                                    const int value_reference)
{
  switch (type) {
    case ELEM_COUNT_LESS:
      return (value_test < value_reference);
    case ELEM_COUNT_EQUAL:
      return (value_test == value_reference);
    case ELEM_COUNT_GREATER:
      return (value_test > value_reference);
    case ELEM_COUNT_NOT_EQUAL:
      return (value_test != value_reference);
    default:
      BLI_assert_unreachable(); /* Bad value of selection `type`. */
      return false;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Mirror
 * \{ */

void EDBM_select_mirrored(BMEditMesh *em,
                          const Mesh *mesh,
                          const int axis,
                          const bool extend,
                          int *r_totmirr,
                          int *r_totfail)
{
  BMesh *bm = em->bm;
  BMIter iter;
  int totmirr = 0;
  int totfail = 0;
  bool use_topology = mesh->editflag & ME_EDIT_MIRROR_TOPO;

  *r_totmirr = *r_totfail = 0;

  /* Flush (select -> tag). */
  if (bm->selectmode & SCE_SELECT_VERTEX) {
    BMVert *v;
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      BM_elem_flag_set(v, BM_ELEM_TAG, BM_elem_flag_test(v, BM_ELEM_SELECT));
    }
  }
  else if (em->selectmode & SCE_SELECT_EDGE) {
    BMEdge *e;
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      BM_elem_flag_set(e, BM_ELEM_TAG, BM_elem_flag_test(e, BM_ELEM_SELECT));
    }
  }
  else {
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      BM_elem_flag_set(f, BM_ELEM_TAG, BM_elem_flag_test(f, BM_ELEM_SELECT));
    }
  }

  EDBM_verts_mirror_cache_begin(em, axis, true, true, false, use_topology);

  if (!extend) {
    EDBM_flag_disable_all(em, BM_ELEM_SELECT);
  }

  if (bm->selectmode & SCE_SELECT_VERTEX) {
    BMVert *v;
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        continue;
      }

      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        BMVert *v_mirr = EDBM_verts_mirror_get(em, v);
        if (v_mirr && !BM_elem_flag_test(v_mirr, BM_ELEM_HIDDEN)) {
          BM_vert_select_set(bm, v_mirr, true);
          totmirr++;
        }
        else {
          totfail++;
        }
      }
    }
  }
  else if (em->selectmode & SCE_SELECT_EDGE) {
    BMEdge *e;
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
        continue;
      }

      if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
        BMEdge *e_mirr = EDBM_verts_mirror_get_edge(em, e);
        if (e_mirr && !BM_elem_flag_test(e_mirr, BM_ELEM_HIDDEN)) {
          BM_edge_select_set(bm, e_mirr, true);
          totmirr++;
        }
        else {
          totfail++;
        }
      }
    }
  }
  else {
    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }

      if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
        BMFace *f_mirr = EDBM_verts_mirror_get_face(em, f);
        if (f_mirr && !BM_elem_flag_test(f_mirr, BM_ELEM_HIDDEN)) {
          BM_face_select_set(bm, f_mirr, true);
          totmirr++;
        }
        else {
          totfail++;
        }
      }
    }
  }

  EDBM_verts_mirror_cache_end(em);

  *r_totmirr = totmirr;
  *r_totfail = totfail;
}

static bool UNUSED_FUNCTION(EDBM_select_mirrored_extend_all)(Object *obedit, BMEditMesh *em)
{
  BMesh *bm = em->bm;
  int selectmode = em->selectmode;
  bool changed = false;

  if (bm->totfacesel == 0) {
    selectmode &= ~SCE_SELECT_FACE;
  }
  if (bm->totedgesel == 0) {
    selectmode &= ~SCE_SELECT_EDGE;
  }
  if (bm->totvertsel == 0) {
    selectmode &= ~SCE_SELECT_VERTEX;
  }

  if (selectmode == 0) {
    return changed;
  }

  char symmetry_htype = 0;
  if (selectmode & SCE_SELECT_FACE) {
    symmetry_htype |= BM_FACE;
  }
  if (selectmode & SCE_SELECT_EDGE) {
    symmetry_htype |= BM_EDGE;
  }
  if (selectmode & SCE_SELECT_VERTEX) {
    symmetry_htype |= BM_VERT;
  }
  if (std::optional<EditMeshSymmetryHelper> symmetry_helper =
          EditMeshSymmetryHelper::create_if_needed(obedit, symmetry_htype))
  {
    const char hflag = BM_ELEM_SELECT;
    BMIter iter;

    if (selectmode & SCE_SELECT_FACE) {
      blender::Vector<BMFace *> source_faces;
      source_faces.reserve(bm->totfacesel);
      BMFace *f;
      BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(f, hflag)) {
          source_faces.append(f);
        }
      }
      const int totfacesel_prev = bm->totfacesel;
      for (BMFace *f_orig : source_faces) {
        symmetry_helper->set_hflag_on_mirror_faces(f_orig, hflag, true);
      }
      if (bm->totfacesel != totfacesel_prev) {
        changed = true;
      }
    }
    if (selectmode & SCE_SELECT_EDGE) {
      blender::Vector<BMEdge *> source_edges;
      source_edges.reserve(bm->totedgesel);
      BMEdge *e;
      BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(e, hflag)) {
          source_edges.append(e);
        }
      }
      const int totedgesel_prev = bm->totedgesel;
      for (BMEdge *e_orig : source_edges) {
        symmetry_helper->set_hflag_on_mirror_edges(e_orig, hflag, true);
      }
      if (bm->totedgesel != totedgesel_prev) {
        changed = true;
      }
    }
    if (selectmode & SCE_SELECT_VERTEX) {
      blender::Vector<BMVert *> source_verts;
      source_verts.reserve(bm->totvertsel);
      BMVert *v;
      BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(v, hflag)) {
          source_verts.append(v);
        }
      }
      const int totvertsel_prev = bm->totvertsel;
      for (BMVert *v_orig : source_verts) {
        symmetry_helper->set_hflag_on_mirror_verts(v_orig, hflag, true);
      }
      if (bm->totvertsel != totvertsel_prev) {
        changed = true;
      }
    }
    if (changed) {
      EDBM_selectmode_flush(em);
    }
  }

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Back-Buffer OpenGL Selection
 * \{ */

static BMElem *edbm_select_id_bm_elem_get(const Span<Base *> bases,
                                          const uint sel_id,
                                          uint &r_base_index)
{
  uint elem_id;
  char elem_type = 0;
  bool success = DRW_select_buffer_elem_get(sel_id, elem_id, r_base_index, elem_type);

  if (success) {
    Object *obedit = bases[r_base_index]->object;
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    switch (elem_type) {
      case SCE_SELECT_FACE:
        return (BMElem *)BM_face_at_index_find_or_table(em->bm, elem_id);
      case SCE_SELECT_EDGE:
        return (BMElem *)BM_edge_at_index_find_or_table(em->bm, elem_id);
      case SCE_SELECT_VERTEX:
        return (BMElem *)BM_vert_at_index_find_or_table(em->bm, elem_id);
      default:
        BLI_assert(0);
        return nullptr;
    }
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Nearest Vert/Edge/Face
 *
 * \note Screen-space manhattan distances are used here,
 * since its faster and good enough for the purpose of selection.
 *
 * \note \a dist_bias is used so we can bias against selected items.
 * when choosing between elements of a single type, but return the real distance
 * to avoid the bias interfering with distance comparisons when mixing types.
 * \{ */

#define FIND_NEAR_SELECT_BIAS 5
#define FIND_NEAR_CYCLE_THRESHOLD_MIN 3

struct NearestVertUserData_Hit {
  float dist;
  float dist_bias;
  int index;
  BMVert *vert;
};

struct NearestVertUserData {
  float mval_fl[2];
  bool use_select_bias;
  bool use_cycle;
  int cycle_index_prev;

  NearestVertUserData_Hit hit;
  NearestVertUserData_Hit hit_cycle;
};

static void findnearestvert__doClosest(void *user_data,
                                       BMVert *eve,
                                       const float screen_co[2],
                                       int index)
{
  NearestVertUserData *data = static_cast<NearestVertUserData *>(user_data);
  float dist_test, dist_test_bias;

  dist_test = dist_test_bias = len_manhattan_v2v2(data->mval_fl, screen_co);

  if (data->use_select_bias && BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
    dist_test_bias += FIND_NEAR_SELECT_BIAS;
  }

  if (dist_test_bias < data->hit.dist_bias) {
    data->hit.dist_bias = dist_test_bias;
    data->hit.dist = dist_test;
    data->hit.index = index;
    data->hit.vert = eve;
  }

  if (data->use_cycle) {
    if ((data->hit_cycle.vert == nullptr) && (index > data->cycle_index_prev) &&
        (dist_test_bias < FIND_NEAR_CYCLE_THRESHOLD_MIN))
    {
      data->hit_cycle.dist_bias = dist_test_bias;
      data->hit_cycle.dist = dist_test;
      data->hit_cycle.index = index;
      data->hit_cycle.vert = eve;
    }
  }
}

BMVert *EDBM_vert_find_nearest_ex(ViewContext *vc,
                                  float *dist_px_manhattan_p,
                                  const bool use_select_bias,
                                  bool use_cycle,
                                  const Span<Base *> bases,
                                  uint *r_base_index)
{
  uint base_index = 0;

  if (!XRAY_FLAG_ENABLED(vc->v3d)) {
    uint dist_px_manhattan_test = uint(
        ED_view3d_backbuf_sample_size_clamp(vc->region, *dist_px_manhattan_p));
    uint index;
    BMVert *eve;

    /* No after-queue (yet), so we check it now, otherwise the bm_xxxofs indices are bad. */
    {
      DRW_select_buffer_context_create(vc->depsgraph, bases, SCE_SELECT_VERTEX);

      index = DRW_select_buffer_find_nearest_to_point(
          vc->depsgraph, vc->region, vc->v3d, vc->mval, 1, UINT_MAX, &dist_px_manhattan_test);

      if (index) {
        eve = (BMVert *)edbm_select_id_bm_elem_get(bases, index, base_index);
      }
      else {
        eve = nullptr;
      }
    }

    if (eve) {
      if (dist_px_manhattan_test < *dist_px_manhattan_p) {
        if (r_base_index) {
          *r_base_index = base_index;
        }
        *dist_px_manhattan_p = dist_px_manhattan_test;
        return eve;
      }
    }
    return nullptr;
  }

  NearestVertUserData data = {{0}};
  const NearestVertUserData_Hit *hit = nullptr;
  const eV3DProjTest clip_flag = RV3D_CLIPPING_ENABLED(vc->v3d, vc->rv3d) ?
                                     V3D_PROJ_TEST_CLIP_DEFAULT :
                                     V3D_PROJ_TEST_CLIP_DEFAULT & ~V3D_PROJ_TEST_CLIP_BB;
  BMesh *prev_select_bm = nullptr;

  static struct {
    int index;
    const BMVert *elem;
    const BMesh *bm;
  } prev_select = {0};

  data.mval_fl[0] = vc->mval[0];
  data.mval_fl[1] = vc->mval[1];
  data.use_select_bias = use_select_bias;
  data.use_cycle = use_cycle;

  for (; base_index < bases.size(); base_index++) {
    Base *base_iter = bases[base_index];
    ED_view3d_viewcontext_init_object(vc, base_iter->object);
    if (use_cycle && prev_select.bm == vc->em->bm &&
        prev_select.elem == BM_vert_at_index_find_or_table(vc->em->bm, prev_select.index))
    {
      data.cycle_index_prev = prev_select.index;
      /* No need to compare in the rest of the loop. */
      use_cycle = false;
    }
    else {
      data.cycle_index_prev = 0;
    }

    data.hit.dist = data.hit_cycle.dist = data.hit.dist_bias = data.hit_cycle.dist_bias =
        *dist_px_manhattan_p;

    ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
    mesh_foreachScreenVert(vc, findnearestvert__doClosest, &data, clip_flag);

    hit = (data.use_cycle && data.hit_cycle.vert) ? &data.hit_cycle : &data.hit;

    if (hit->dist < *dist_px_manhattan_p) {
      if (r_base_index) {
        *r_base_index = base_index;
      }
      *dist_px_manhattan_p = hit->dist;
      prev_select_bm = vc->em->bm;
    }
  }

  if (hit == nullptr) {
    return nullptr;
  }

  prev_select.index = hit->index;
  prev_select.elem = hit->vert;
  prev_select.bm = prev_select_bm;

  return hit->vert;
}

BMVert *EDBM_vert_find_nearest(ViewContext *vc, float *dist_px_manhattan_p)
{
  BKE_view_layer_synced_ensure(vc->scene, vc->view_layer);
  Base *base = BKE_view_layer_base_find(vc->view_layer, vc->obact);
  return EDBM_vert_find_nearest_ex(vc, dist_px_manhattan_p, false, false, {base}, nullptr);
}

/** Find the distance to the edge we already have. */
struct NearestEdgeUserData_ZBuf {
  float mval_fl[2];
  float dist;
  const BMEdge *edge_test;
};

static void find_nearest_edge_center__doZBuf(void *user_data,
                                             BMEdge *eed,
                                             const float screen_co_a[2],
                                             const float screen_co_b[2],
                                             int /*index*/)
{
  NearestEdgeUserData_ZBuf *data = static_cast<NearestEdgeUserData_ZBuf *>(user_data);

  if (eed == data->edge_test) {
    float dist_test;
    float screen_co_mid[2];

    mid_v2_v2v2(screen_co_mid, screen_co_a, screen_co_b);
    dist_test = len_manhattan_v2v2(data->mval_fl, screen_co_mid);

    data->dist = std::min(dist_test, data->dist);
  }
}

struct NearestEdgeUserData_Hit {
  float dist;
  float dist_bias;
  int index;
  BMEdge *edge;

  /**
   * Edges only, un-biased manhattan distance to which ever edge we pick
   * (not used for choosing).
   */
  float dist_center_px_manhattan;
};

struct NearestEdgeUserData {
  ViewContext vc;
  float mval_fl[2];
  bool use_select_bias;
  bool use_cycle;
  int cycle_index_prev;

  NearestEdgeUserData_Hit hit;
  NearestEdgeUserData_Hit hit_cycle;
};

/* NOTE: uses v3d, so needs active 3d window. */
static void find_nearest_edge__doClosest(void *user_data,
                                         BMEdge *eed,
                                         const float screen_co_a[2],
                                         const float screen_co_b[2],
                                         int index)
{
  NearestEdgeUserData *data = static_cast<NearestEdgeUserData *>(user_data);
  float dist_test, dist_test_bias;

  float fac = line_point_factor_v2(data->mval_fl, screen_co_a, screen_co_b);
  float screen_co[2];

  if (fac <= 0.0f) {
    fac = 0.0f;
    copy_v2_v2(screen_co, screen_co_a);
  }
  else if (fac >= 1.0f) {
    fac = 1.0f;
    copy_v2_v2(screen_co, screen_co_b);
  }
  else {
    interp_v2_v2v2(screen_co, screen_co_a, screen_co_b, fac);
  }

  dist_test = dist_test_bias = len_manhattan_v2v2(data->mval_fl, screen_co);

  if (data->use_select_bias && BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
    dist_test_bias += FIND_NEAR_SELECT_BIAS;
  }

  if (data->vc.rv3d->rflag & RV3D_CLIPPING) {
    float vec[3];

    interp_v3_v3v3(vec, eed->v1->co, eed->v2->co, fac);
    if (ED_view3d_clipping_test(data->vc.rv3d, vec, true)) {
      return;
    }
  }

  if (dist_test_bias < data->hit.dist_bias) {
    float screen_co_mid[2];

    data->hit.dist_bias = dist_test_bias;
    data->hit.dist = dist_test;
    data->hit.index = index;
    data->hit.edge = eed;

    mid_v2_v2v2(screen_co_mid, screen_co_a, screen_co_b);
    data->hit.dist_center_px_manhattan = len_manhattan_v2v2(data->mval_fl, screen_co_mid);
  }

  if (data->use_cycle) {
    if ((data->hit_cycle.edge == nullptr) && (index > data->cycle_index_prev) &&
        (dist_test_bias < FIND_NEAR_CYCLE_THRESHOLD_MIN))
    {
      float screen_co_mid[2];

      data->hit_cycle.dist_bias = dist_test_bias;
      data->hit_cycle.dist = dist_test;
      data->hit_cycle.index = index;
      data->hit_cycle.edge = eed;

      mid_v2_v2v2(screen_co_mid, screen_co_a, screen_co_b);
      data->hit_cycle.dist_center_px_manhattan = len_manhattan_v2v2(data->mval_fl, screen_co_mid);
    }
  }
}

BMEdge *EDBM_edge_find_nearest_ex(ViewContext *vc,
                                  float *dist_px_manhattan_p,
                                  float *r_dist_center_px_manhattan,
                                  const bool use_select_bias,
                                  bool use_cycle,
                                  BMEdge **r_eed_zbuf,
                                  const Span<Base *> bases,
                                  uint *r_base_index)
{
  uint base_index = 0;

  if (!XRAY_FLAG_ENABLED(vc->v3d)) {
    uint dist_px_manhattan_test = uint(
        ED_view3d_backbuf_sample_size_clamp(vc->region, *dist_px_manhattan_p));
    uint index;
    BMEdge *eed;

    /* No after-queue (yet), so we check it now, otherwise the bm_xxxofs indices are bad. */
    {
      DRW_select_buffer_context_create(vc->depsgraph, bases, SCE_SELECT_EDGE);

      index = DRW_select_buffer_find_nearest_to_point(
          vc->depsgraph, vc->region, vc->v3d, vc->mval, 1, UINT_MAX, &dist_px_manhattan_test);

      if (index) {
        eed = (BMEdge *)edbm_select_id_bm_elem_get(bases, index, base_index);
      }
      else {
        eed = nullptr;
      }
    }

    if (r_eed_zbuf) {
      *r_eed_zbuf = eed;
    }

    /* Exception for faces (verts don't need this). */
    if (r_dist_center_px_manhattan && eed) {
      NearestEdgeUserData_ZBuf data;

      data.mval_fl[0] = vc->mval[0];
      data.mval_fl[1] = vc->mval[1];
      data.dist = FLT_MAX;
      data.edge_test = eed;

      ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

      mesh_foreachScreenEdge(vc,
                             find_nearest_edge_center__doZBuf,
                             &data,
                             V3D_PROJ_TEST_CLIP_DEFAULT | V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT);

      *r_dist_center_px_manhattan = data.dist;
    }
    /* End exception. */

    if (eed) {
      if (dist_px_manhattan_test < *dist_px_manhattan_p) {
        if (r_base_index) {
          *r_base_index = base_index;
        }
        *dist_px_manhattan_p = dist_px_manhattan_test;
        return eed;
      }
    }
    return nullptr;
  }

  NearestEdgeUserData data = {{nullptr}};
  const NearestEdgeUserData_Hit *hit = nullptr;
  /* Interpolate along the edge before doing a clipping plane test. */
  const eV3DProjTest clip_flag = V3D_PROJ_TEST_CLIP_DEFAULT & ~V3D_PROJ_TEST_CLIP_BB;
  BMesh *prev_select_bm = nullptr;

  static struct {
    int index;
    const BMEdge *elem;
    const BMesh *bm;
  } prev_select = {0};

  data.vc = *vc;
  data.mval_fl[0] = vc->mval[0];
  data.mval_fl[1] = vc->mval[1];
  data.use_select_bias = use_select_bias;
  data.use_cycle = use_cycle;

  for (; base_index < bases.size(); base_index++) {
    Base *base_iter = bases[base_index];
    ED_view3d_viewcontext_init_object(vc, base_iter->object);
    if (use_cycle && prev_select.bm == vc->em->bm &&
        prev_select.elem == BM_edge_at_index_find_or_table(vc->em->bm, prev_select.index))
    {
      data.cycle_index_prev = prev_select.index;
      /* No need to compare in the rest of the loop. */
      use_cycle = false;
    }
    else {
      data.cycle_index_prev = 0;
    }

    data.hit.dist = data.hit_cycle.dist = data.hit.dist_bias = data.hit_cycle.dist_bias =
        *dist_px_manhattan_p;

    ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
    mesh_foreachScreenEdge(
        vc, find_nearest_edge__doClosest, &data, clip_flag | V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT);

    hit = (data.use_cycle && data.hit_cycle.edge) ? &data.hit_cycle : &data.hit;

    if (hit->dist < *dist_px_manhattan_p) {
      if (r_base_index) {
        *r_base_index = base_index;
      }
      *dist_px_manhattan_p = hit->dist;
      prev_select_bm = vc->em->bm;
    }
  }

  if (hit == nullptr) {
    return nullptr;
  }

  if (r_dist_center_px_manhattan) {
    *r_dist_center_px_manhattan = hit->dist_center_px_manhattan;
  }

  prev_select.index = hit->index;
  prev_select.elem = hit->edge;
  prev_select.bm = prev_select_bm;

  return hit->edge;
}

BMEdge *EDBM_edge_find_nearest(ViewContext *vc, float *dist_px_manhattan_p)
{
  BKE_view_layer_synced_ensure(vc->scene, vc->view_layer);
  Base *base = BKE_view_layer_base_find(vc->view_layer, vc->obact);
  return EDBM_edge_find_nearest_ex(
      vc, dist_px_manhattan_p, nullptr, false, false, nullptr, {base}, nullptr);
}

/** Find the distance to the face we already have. */
struct NearestFaceUserData_ZBuf {
  float mval_fl[2];
  float dist_px_manhattan;
  const BMFace *face_test;
};

static void find_nearest_face_center__doZBuf(void *user_data,
                                             BMFace *efa,
                                             const float screen_co[2],
                                             int /*index*/)
{
  NearestFaceUserData_ZBuf *data = static_cast<NearestFaceUserData_ZBuf *>(user_data);

  if (efa == data->face_test) {
    const float dist_test = len_manhattan_v2v2(data->mval_fl, screen_co);

    data->dist_px_manhattan = std::min(dist_test, data->dist_px_manhattan);
  }
}

struct NearestFaceUserData_Hit {
  float dist;
  float dist_bias;
  int index;
  BMFace *face;
};

struct NearestFaceUserData {
  float mval_fl[2];
  bool use_select_bias;
  bool use_cycle;
  int cycle_index_prev;

  NearestFaceUserData_Hit hit;
  NearestFaceUserData_Hit hit_cycle;
};

static void findnearestface__doClosest(void *user_data,
                                       BMFace *efa,
                                       const float screen_co[2],
                                       int index)
{
  NearestFaceUserData *data = static_cast<NearestFaceUserData *>(user_data);
  float dist_test, dist_test_bias;

  dist_test = dist_test_bias = len_manhattan_v2v2(data->mval_fl, screen_co);

  if (data->use_select_bias && BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
    dist_test_bias += FIND_NEAR_SELECT_BIAS;
  }

  if (dist_test_bias < data->hit.dist_bias) {
    data->hit.dist_bias = dist_test_bias;
    data->hit.dist = dist_test;
    data->hit.index = index;
    data->hit.face = efa;
  }

  if (data->use_cycle) {
    if ((data->hit_cycle.face == nullptr) && (index > data->cycle_index_prev) &&
        (dist_test_bias < FIND_NEAR_CYCLE_THRESHOLD_MIN))
    {
      data->hit_cycle.dist_bias = dist_test_bias;
      data->hit_cycle.dist = dist_test;
      data->hit_cycle.index = index;
      data->hit_cycle.face = efa;
    }
  }
}

BMFace *EDBM_face_find_nearest_ex(ViewContext *vc,
                                  float *dist_px_manhattan_p,
                                  float *r_dist_center,
                                  const bool use_zbuf_single_px,
                                  const bool use_select_bias,
                                  bool use_cycle,
                                  BMFace **r_efa_zbuf,
                                  const Span<Base *> bases,
                                  uint *r_base_index)
{
  uint base_index = 0;

  if (!XRAY_FLAG_ENABLED(vc->v3d)) {
    float dist_test;
    uint index;
    BMFace *efa;

    {
      uint dist_px_manhattan_test = 0;
      if (*dist_px_manhattan_p != 0.0f && (use_zbuf_single_px == false)) {
        dist_px_manhattan_test = uint(
            ED_view3d_backbuf_sample_size_clamp(vc->region, *dist_px_manhattan_p));
      }

      DRW_select_buffer_context_create(vc->depsgraph, bases, SCE_SELECT_FACE);

      if (dist_px_manhattan_test == 0) {
        index = DRW_select_buffer_sample_point(vc->depsgraph, vc->region, vc->v3d, vc->mval);
        dist_test = 0.0f;
      }
      else {
        index = DRW_select_buffer_find_nearest_to_point(
            vc->depsgraph, vc->region, vc->v3d, vc->mval, 1, UINT_MAX, &dist_px_manhattan_test);
        dist_test = dist_px_manhattan_test;
      }

      if (index) {
        efa = (BMFace *)edbm_select_id_bm_elem_get(bases, index, base_index);
      }
      else {
        efa = nullptr;
      }
    }

    if (r_efa_zbuf) {
      *r_efa_zbuf = efa;
    }

    /* Exception for faces (verts don't need this). */
    if (r_dist_center && efa) {
      NearestFaceUserData_ZBuf data;

      data.mval_fl[0] = vc->mval[0];
      data.mval_fl[1] = vc->mval[1];
      data.dist_px_manhattan = FLT_MAX;
      data.face_test = efa;

      ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

      mesh_foreachScreenFace(
          vc, find_nearest_face_center__doZBuf, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

      *r_dist_center = data.dist_px_manhattan;
    }
    /* End exception. */

    if (efa) {
      if (dist_test < *dist_px_manhattan_p) {
        if (r_base_index) {
          *r_base_index = base_index;
        }
        *dist_px_manhattan_p = dist_test;
        return efa;
      }
    }
    return nullptr;
  }

  NearestFaceUserData data = {{0}};
  const NearestFaceUserData_Hit *hit = nullptr;
  const eV3DProjTest clip_flag = V3D_PROJ_TEST_CLIP_DEFAULT;
  BMesh *prev_select_bm = nullptr;

  static struct {
    int index;
    const BMFace *elem;
    const BMesh *bm;
  } prev_select = {0};

  data.mval_fl[0] = vc->mval[0];
  data.mval_fl[1] = vc->mval[1];
  data.use_select_bias = use_select_bias;
  data.use_cycle = use_cycle;

  for (; base_index < bases.size(); base_index++) {
    Base *base_iter = bases[base_index];
    ED_view3d_viewcontext_init_object(vc, base_iter->object);
    if (use_cycle && prev_select.bm == vc->em->bm &&
        prev_select.elem == BM_face_at_index_find_or_table(vc->em->bm, prev_select.index))
    {
      data.cycle_index_prev = prev_select.index;
      /* No need to compare in the rest of the loop. */
      use_cycle = false;
    }
    else {
      data.cycle_index_prev = 0;
    }

    data.hit.dist = data.hit_cycle.dist = data.hit.dist_bias = data.hit_cycle.dist_bias =
        *dist_px_manhattan_p;

    ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
    mesh_foreachScreenFace(vc, findnearestface__doClosest, &data, clip_flag);

    hit = (data.use_cycle && data.hit_cycle.face) ? &data.hit_cycle : &data.hit;

    if (hit->dist < *dist_px_manhattan_p) {
      if (r_base_index) {
        *r_base_index = base_index;
      }
      *dist_px_manhattan_p = hit->dist;
      prev_select_bm = vc->em->bm;
    }
  }

  if (hit == nullptr) {
    return nullptr;
  }

  if (r_dist_center) {
    *r_dist_center = hit->dist;
  }

  prev_select.index = hit->index;
  prev_select.elem = hit->face;
  prev_select.bm = prev_select_bm;

  return hit->face;
}

BMFace *EDBM_face_find_nearest(ViewContext *vc, float *dist_px_manhattan_p)
{
  BKE_view_layer_synced_ensure(vc->scene, vc->view_layer);
  Base *base = BKE_view_layer_base_find(vc->view_layer, vc->obact);
  return EDBM_face_find_nearest_ex(
      vc, dist_px_manhattan_p, nullptr, false, false, false, nullptr, {base}, nullptr);
}

#undef FIND_NEAR_SELECT_BIAS
#undef FIND_NEAR_CYCLE_THRESHOLD_MIN

/**
 * Find the nearest using the best distance based on screen coords.
 * Use `em->selectmode` to define how to use selected vertices and edges get disadvantage.
 *
 * \return true if found one.
 */
static bool unified_findnearest(ViewContext *vc,
                                const Span<Base *> bases,
                                int *r_base_index,
                                BMVert **r_eve,
                                BMEdge **r_eed,
                                BMFace **r_efa)
{
  BMEditMesh *em = vc->em;

  const bool use_cycle = !WM_cursor_test_motion_and_update(vc->mval);
  const float dist_init = ED_view3d_select_dist_px();
  /* Since edges select lines, we give dots advantage of ~20 pix. */
  const float dist_margin = (dist_init / 2);
  float dist = dist_init;

  struct {
    struct {
      BMVert *ele;
      int base_index;
    } v;
    struct {
      BMEdge *ele;
      int base_index;
    } e, e_zbuf;
    struct {
      BMFace *ele;
      int base_index;
    } f, f_zbuf;
  } hit = {{nullptr}};

  /* No after-queue (yet), so we check it now, otherwise the em_xxxofs indices are bad. */

  if ((dist > 0.0f) && (em->selectmode & SCE_SELECT_FACE)) {
    float dist_center = 0.0f;
    float *dist_center_p = (em->selectmode & (SCE_SELECT_EDGE | SCE_SELECT_VERTEX)) ?
                               &dist_center :
                               nullptr;

    uint base_index = 0;
    BMFace *efa_zbuf = nullptr;
    BMFace *efa_test = EDBM_face_find_nearest_ex(
        vc, &dist, dist_center_p, true, true, use_cycle, &efa_zbuf, bases, &base_index);

    if (efa_test && dist_center_p) {
      dist = min_ff(dist_margin, dist_center);
    }
    if (efa_test) {
      hit.f.base_index = base_index;
      hit.f.ele = efa_test;
    }
    if (efa_zbuf) {
      hit.f_zbuf.base_index = base_index;
      hit.f_zbuf.ele = efa_zbuf;
    }
  }

  if ((dist > 0.0f) && (em->selectmode & SCE_SELECT_EDGE)) {
    float dist_center = 0.0f;
    float *dist_center_p = (em->selectmode & SCE_SELECT_VERTEX) ? &dist_center : nullptr;

    uint base_index = 0;
    BMEdge *eed_zbuf = nullptr;
    BMEdge *eed_test = EDBM_edge_find_nearest_ex(
        vc, &dist, dist_center_p, true, use_cycle, &eed_zbuf, bases, &base_index);

    if (eed_test && dist_center_p) {
      dist = min_ff(dist_margin, dist_center);
    }
    if (eed_test) {
      hit.e.base_index = base_index;
      hit.e.ele = eed_test;
    }
    if (eed_zbuf) {
      hit.e_zbuf.base_index = base_index;
      hit.e_zbuf.ele = eed_zbuf;
    }
  }

  if ((dist > 0.0f) && (em->selectmode & SCE_SELECT_VERTEX)) {
    uint base_index = 0;
    BMVert *eve_test = EDBM_vert_find_nearest_ex(vc, &dist, true, use_cycle, bases, &base_index);

    if (eve_test) {
      hit.v.base_index = base_index;
      hit.v.ele = eve_test;
    }
  }

  /* Return only one of 3 pointers, for front-buffer redraws. */
  if (hit.v.ele) {
    hit.f.ele = nullptr;
    hit.e.ele = nullptr;
  }
  else if (hit.e.ele) {
    hit.f.ele = nullptr;
  }

  /* There may be a face under the cursor, who's center if too far away
   * use this if all else fails, it makes sense to select this. */
  if ((hit.v.ele || hit.e.ele || hit.f.ele) == 0) {
    if (hit.e_zbuf.ele) {
      hit.e.base_index = hit.e_zbuf.base_index;
      hit.e.ele = hit.e_zbuf.ele;
    }
    else if (hit.f_zbuf.ele) {
      hit.f.base_index = hit.f_zbuf.base_index;
      hit.f.ele = hit.f_zbuf.ele;
    }
  }

  /* Only one element type will be non-null. */
  BLI_assert(((hit.v.ele != nullptr) + (hit.e.ele != nullptr) + (hit.f.ele != nullptr)) <= 1);

  if (hit.v.ele) {
    *r_base_index = hit.v.base_index;
  }
  if (hit.e.ele) {
    *r_base_index = hit.e.base_index;
  }
  if (hit.f.ele) {
    *r_base_index = hit.f.base_index;
  }

  *r_eve = hit.v.ele;
  *r_eed = hit.e.ele;
  *r_efa = hit.f.ele;

  return (hit.v.ele || hit.e.ele || hit.f.ele);
}

#undef FAKE_SELECT_MODE_BEGIN
#undef FAKE_SELECT_MODE_END

bool EDBM_unified_findnearest(ViewContext *vc,
                              const Span<Base *> bases,
                              int *r_base_index,
                              BMVert **r_eve,
                              BMEdge **r_eed,
                              BMFace **r_efa)
{
  return unified_findnearest(vc, bases, r_base_index, r_eve, r_eed, r_efa);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Alternate Find Nearest Vert/Edge (optional boundary)
 *
 * \note This uses ray-cast method instead of back-buffer,
 * currently used for poly-build.
 * \{ */

bool EDBM_unified_findnearest_from_raycast(ViewContext *vc,
                                           const Span<Base *> bases,
                                           bool use_boundary_vertices,
                                           bool use_boundary_edges,
                                           int *r_base_index_vert,
                                           int *r_base_index_edge,
                                           int *r_base_index_face,
                                           BMVert **r_eve,
                                           BMEdge **r_eed,
                                           BMFace **r_efa)
{
  const float mval_fl[2] = {float(vc->mval[0]), float(vc->mval[1])};
  float ray_origin[3], ray_direction[3];

  struct {
    uint base_index;
    BMElem *ele;
  } best = {0, nullptr};
  /* Currently unused, keep since we may want to pick the best. */
  UNUSED_VARS(best);

  struct {
    uint base_index;
    BMElem *ele;
  } best_vert = {0, nullptr};

  struct {
    uint base_index;
    BMElem *ele;
  } best_edge = {0, nullptr};

  struct {
    uint base_index;
    BMElem *ele;
  } best_face = {0, nullptr};

  if (ED_view3d_win_to_ray_clipped(
          vc->depsgraph, vc->region, vc->v3d, mval_fl, ray_origin, ray_direction, true))
  {
    float dist_sq_best = FLT_MAX;
    float dist_sq_best_vert = FLT_MAX;
    float dist_sq_best_edge = FLT_MAX;
    float dist_sq_best_face = FLT_MAX;

    const bool use_vert = (r_eve != nullptr);
    const bool use_edge = (r_eed != nullptr);
    const bool use_face = (r_efa != nullptr);

    for (const int base_index : bases.index_range()) {
      Base *base_iter = bases[base_index];
      Object *obedit = base_iter->object;

      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      BMesh *bm = em->bm;
      float imat3[3][3];

      ED_view3d_viewcontext_init_object(vc, obedit);
      copy_m3_m4(imat3, obedit->object_to_world().ptr());
      invert_m3(imat3);

      Span<float3> vert_positions;
      {
        const Object *obedit_eval = DEG_get_evaluated(vc->depsgraph, obedit);
        const Mesh *mesh_eval = BKE_object_get_editmesh_eval_cage(obedit_eval);
        if (BKE_mesh_wrapper_vert_len(mesh_eval) == bm->totvert) {
          vert_positions = BKE_mesh_wrapper_vert_coords(mesh_eval);
        }
      }

      if (!vert_positions.is_empty()) {
        BM_mesh_elem_index_ensure(bm, BM_VERT);
      }

      if ((use_boundary_vertices || use_boundary_edges) && (use_vert || use_edge)) {
        BMEdge *e;
        BMIter eiter;
        BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
            continue;
          }

          if (BM_edge_is_boundary(e)) {
            if (use_vert && use_boundary_vertices) {
              for (uint j = 0; j < 2; j++) {
                BMVert *v = *((&e->v1) + j);
                float point[3];
                mul_v3_m4v3(point,
                            obedit->object_to_world().ptr(),
                            !vert_positions.is_empty() ? vert_positions[BM_elem_index_get(v)] :
                                                         v->co);
                const float dist_sq_test = dist_squared_to_ray_v3_normalized(
                    ray_origin, ray_direction, point);
                if (dist_sq_test < dist_sq_best_vert) {
                  dist_sq_best_vert = dist_sq_test;
                  best_vert.base_index = base_index;
                  best_vert.ele = (BMElem *)v;
                }
                if (dist_sq_test < dist_sq_best) {
                  dist_sq_best = dist_sq_test;
                  best.base_index = base_index;
                  best.ele = (BMElem *)v;
                }
              }
            }

            if (use_edge && use_boundary_edges) {
              float point[3];
#if 0
              const float dist_sq_test = dist_squared_ray_to_seg_v3(
                  ray_origin, ray_direction, e->v1->co, e->v2->co, point, &depth);
#else
              if (!vert_positions.is_empty()) {
                mid_v3_v3v3(point,
                            vert_positions[BM_elem_index_get(e->v1)],
                            vert_positions[BM_elem_index_get(e->v2)]);
              }
              else {
                mid_v3_v3v3(point, e->v1->co, e->v2->co);
              }
              mul_m4_v3(obedit->object_to_world().ptr(), point);
              const float dist_sq_test = dist_squared_to_ray_v3_normalized(
                  ray_origin, ray_direction, point);
              if (dist_sq_test < dist_sq_best_edge) {
                dist_sq_best_edge = dist_sq_test;
                best_edge.base_index = base_index;
                best_edge.ele = (BMElem *)e;
              }
              if (dist_sq_test < dist_sq_best) {
                dist_sq_best = dist_sq_test;
                best.base_index = base_index;
                best.ele = (BMElem *)e;
              }
#endif
            }
          }
        }
      }
      /* Non boundary case. */
      if (use_vert && !use_boundary_vertices) {
        BMVert *v;
        BMIter viter;
        BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
          if (BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
            continue;
          }

          float point[3];
          mul_v3_m4v3(point,
                      obedit->object_to_world().ptr(),
                      !vert_positions.is_empty() ? vert_positions[BM_elem_index_get(v)] : v->co);
          const float dist_sq_test = dist_squared_to_ray_v3_normalized(
              ray_origin, ray_direction, point);
          if (dist_sq_test < dist_sq_best_vert) {
            dist_sq_best_vert = dist_sq_test;
            best_vert.base_index = base_index;
            best_vert.ele = (BMElem *)v;
          }
          if (dist_sq_test < dist_sq_best) {
            dist_sq_best = dist_sq_test;
            best.base_index = base_index;
            best.ele = (BMElem *)v;
          }
        }
      }

      if (use_edge && !use_boundary_edges) {
        BMEdge *e;
        BMIter eiter;
        BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
            continue;
          }

          float point[3];
          if (!vert_positions.is_empty()) {
            mid_v3_v3v3(point,
                        vert_positions[BM_elem_index_get(e->v1)],
                        vert_positions[BM_elem_index_get(e->v2)]);
          }
          else {
            mid_v3_v3v3(point, e->v1->co, e->v2->co);
          }
          mul_m4_v3(obedit->object_to_world().ptr(), point);
          const float dist_sq_test = dist_squared_to_ray_v3_normalized(
              ray_origin, ray_direction, point);
          if (dist_sq_test < dist_sq_best_edge) {
            dist_sq_best_edge = dist_sq_test;
            best_edge.base_index = base_index;
            best_edge.ele = (BMElem *)e;
          }
          if (dist_sq_test < dist_sq_best) {
            dist_sq_best = dist_sq_test;
            best.base_index = base_index;
            best.ele = (BMElem *)e;
          }
        }
      }

      if (use_face) {
        BMFace *f;
        BMIter fiter;
        BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
          if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
            continue;
          }

          float point[3];
          if (!vert_positions.is_empty()) {
            BM_face_calc_center_median_vcos(bm, f, point, vert_positions);
          }
          else {
            BM_face_calc_center_median(f, point);
          }
          mul_m4_v3(obedit->object_to_world().ptr(), point);
          const float dist_sq_test = dist_squared_to_ray_v3_normalized(
              ray_origin, ray_direction, point);
          if (dist_sq_test < dist_sq_best_face) {
            dist_sq_best_face = dist_sq_test;
            best_face.base_index = base_index;
            best_face.ele = (BMElem *)f;
          }
          if (dist_sq_test < dist_sq_best) {
            dist_sq_best = dist_sq_test;
            best.base_index = base_index;
            best.ele = (BMElem *)f;
          }
        }
      }
    }
  }

  *r_base_index_vert = best_vert.base_index;
  *r_base_index_edge = best_edge.base_index;
  *r_base_index_face = best_face.base_index;

  if (r_eve) {
    *r_eve = nullptr;
  }
  if (r_eed) {
    *r_eed = nullptr;
  }
  if (r_efa) {
    *r_efa = nullptr;
  }

  if (best_vert.ele) {
    *r_eve = (BMVert *)best_vert.ele;
  }
  if (best_edge.ele) {
    *r_eed = (BMEdge *)best_edge.ele;
  }
  if (best_face.ele) {
    *r_efa = (BMFace *)best_face.ele;
  }

  return (best_vert.ele != nullptr || best_edge.ele != nullptr || best_face.ele != nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar Region Operator
 * \{ */

static wmOperatorStatus edbm_select_similar_region_exec(bContext *C, wmOperator *op)
{
  Object *obedit = CTX_data_edit_object(C);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  bool changed = false;

  /* Group variables. */
  int (*group_index)[2];
  int group_tot;
  int i;

  if (bm->totfacesel < 2) {
    BKE_report(op->reports, RPT_ERROR, "No face regions selected");
    return OPERATOR_CANCELLED;
  }

  int *groups_array = MEM_malloc_arrayN<int>(bm->totfacesel, __func__);
  group_tot = BM_mesh_calc_face_groups(
      bm, groups_array, &group_index, nullptr, nullptr, nullptr, BM_ELEM_SELECT, BM_VERT);

  BM_mesh_elem_table_ensure(bm, BM_FACE);

  for (i = 0; i < group_tot; i++) {
    ListBase faces_regions;
    int tot;

    const int fg_sta = group_index[i][0];
    const int fg_len = group_index[i][1];
    int j;
    BMFace **fg = MEM_malloc_arrayN<BMFace *>(fg_len, __func__);

    for (j = 0; j < fg_len; j++) {
      fg[j] = BM_face_at_index(bm, groups_array[fg_sta + j]);
    }

    tot = BM_mesh_region_match(bm, fg, fg_len, &faces_regions);

    MEM_freeN(fg);

    if (tot) {
      while (LinkData *link = static_cast<LinkData *>(BLI_pophead(&faces_regions))) {
        BMFace **faces = static_cast<BMFace **>(link->data);
        while (BMFace *f = *(faces++)) {
          BM_face_select_set(bm, f, true);
        }
        MEM_freeN(link->data);
        MEM_freeN(link);

        changed = true;
      }
    }
  }

  MEM_freeN(groups_array);
  MEM_freeN(group_index);

  if (changed) {
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }
  else {
    BKE_report(op->reports, RPT_WARNING, "No matching face regions found");
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_similar_region(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Similar Regions";
  ot->idname = "MESH_OT_select_similar_region";
  ot->description = "Select similar face regions to the current selection";

  /* API callbacks. */
  ot->exec = edbm_select_similar_region_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Mode Vert/Edge/Face Operator
 * \{ */

static wmOperatorStatus edbm_select_mode_exec(bContext *C, wmOperator *op)
{
  const int type = RNA_enum_get(op->ptr, "type");
  const int action = RNA_enum_get(op->ptr, "action");
  const bool use_extend = RNA_boolean_get(op->ptr, "use_extend");
  const bool use_expand = RNA_boolean_get(op->ptr, "use_expand");

  if (EDBM_selectmode_toggle_multi(C, type, action, use_extend, use_expand)) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus edbm_select_mode_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* Bypass when in UV non sync-select mode, fall through to keymap that edits. */
  if (CTX_wm_space_image(C)) {
    ToolSettings *ts = CTX_data_tool_settings(C);
    if ((ts->uv_flag & UV_FLAG_SELECT_SYNC) == 0) {
      return OPERATOR_PASS_THROUGH;
    }
    /* Bypass when no action is needed. */
    if (!RNA_struct_property_is_set(op->ptr, "type")) {
      return OPERATOR_CANCELLED;
    }
  }

  /* Detecting these options based on shift/control here is weak, but it's done
   * to make this work when clicking buttons or menus. */
  if (!RNA_struct_property_is_set(op->ptr, "use_extend")) {
    RNA_boolean_set(op->ptr, "use_extend", event->modifier & KM_SHIFT);
  }
  if (!RNA_struct_property_is_set(op->ptr, "use_expand")) {
    RNA_boolean_set(op->ptr, "use_expand", event->modifier & KM_CTRL);
  }

  return edbm_select_mode_exec(C, op);
}

static std::string edbm_select_mode_get_description(bContext * /*C*/,
                                                    wmOperatorType * /*ot*/,
                                                    PointerRNA *ptr)
{
  const int type = RNA_enum_get(ptr, "type");

  /* Because the special behavior for shift and ctrl click depend on user input, they may be
   * incorrect if the operator is used from a script or from a special button. So only return the
   * specialized descriptions if only the "type" is set, which conveys that the operator is meant
   * to be used with the logic in the `invoke` method. */
  if (RNA_struct_property_is_set(ptr, "type") && !RNA_struct_property_is_set(ptr, "use_extend") &&
      !RNA_struct_property_is_set(ptr, "use_expand") && !RNA_struct_property_is_set(ptr, "action"))
  {
    switch (type) {
      case SCE_SELECT_VERTEX:
        return TIP_(
            "Vertex select - Shift-Click for multiple modes, Ctrl-Click contracts selection");
      case SCE_SELECT_EDGE:
        return TIP_(
            "Edge select - Shift-Click for multiple modes, "
            "Ctrl-Click expands/contracts selection depending on the current mode");
      case SCE_SELECT_FACE:
        return TIP_("Face select - Shift-Click for multiple modes, Ctrl-Click expands selection");
    }
  }

  return "";
}

void MESH_OT_select_mode(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem actions_items[] = {
      {0, "DISABLE", false, "Disable", "Disable selected markers"},
      {1, "ENABLE", false, "Enable", "Enable selected markers"},
      {2, "TOGGLE", false, "Toggle", "Toggle disabled flag for selected markers"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Identifiers. */
  ot->name = "Select Mode";
  ot->idname = "MESH_OT_select_mode";
  ot->description = "Change selection mode";

  /* API callbacks. */
  ot->invoke = edbm_select_mode_invoke;
  ot->exec = edbm_select_mode_exec;
  ot->poll = ED_operator_editmesh;
  ot->get_description = edbm_select_mode_get_description;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  /* Hide all, not to show redo panel. */
  prop = RNA_def_boolean(ot->srna, "use_extend", false, "Extend", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "use_expand", false, "Expand", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  ot->prop = prop = RNA_def_enum(ot->srna, "type", rna_enum_mesh_select_mode_items, 0, "Type", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_enum(
      ot->srna, "action", actions_items, 2, "Action", "Selection action to execute");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Loop (Non Modal) Operator
 * \{ */

static void walker_select_count(BMEditMesh *em,
                                int walkercode,
                                void *start,
                                int r_count_by_select[2])
{
  BMesh *bm = em->bm;
  BMElem *ele;
  BMWalker walker;

  r_count_by_select[0] = r_count_by_select[1] = 0;

  BMW_init(&walker,
           bm,
           walkercode,
           BMW_MASK_NOP,
           BMW_MASK_NOP,
           BMW_MASK_NOP,
           BMW_FLAG_TEST_HIDDEN,
           BMW_NIL_LAY);

  for (ele = static_cast<BMElem *>(BMW_begin(&walker, start)); ele;
       ele = static_cast<BMElem *>(BMW_step(&walker)))
  {
    r_count_by_select[BM_elem_flag_test(ele, BM_ELEM_SELECT) ? 1 : 0] += 1;

    /* Early exit when mixed (could be optional if needed. */
    if (r_count_by_select[0] && r_count_by_select[1]) {
      r_count_by_select[0] = r_count_by_select[1] = -1;
      break;
    }
  }

  BMW_end(&walker);
}

static bool walker_select(BMEditMesh *em, int walkercode, void *start, const bool select)
{
  BMesh *bm = em->bm;
  BMElem *ele;
  BMWalker walker;
  bool changed = false;

  BMW_init(&walker,
           bm,
           walkercode,
           BMW_MASK_NOP,
           BMW_MASK_NOP,
           BMW_MASK_NOP,
           BMW_FLAG_TEST_HIDDEN,
           BMW_NIL_LAY);

  for (ele = static_cast<BMElem *>(BMW_begin(&walker, start)); ele;
       ele = static_cast<BMElem *>(BMW_step(&walker)))
  {
    if (!select) {
      BM_select_history_remove(bm, ele);
    }
    BM_elem_select_set(bm, ele, select);
    changed = true;
  }
  BMW_end(&walker);
  return changed;
}

static wmOperatorStatus edbm_loop_multiselect_exec(bContext *C, wmOperator *op)
{
  const bool is_ring = RNA_boolean_get(op->ptr, "ring");
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totedgesel == 0) {
      continue;
    }

    BMEdge *eed;
    int edindex;
    BMIter iter;
    int totedgesel = 0;

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
        totedgesel++;
      }
    }

    BMEdge **edarray = MEM_malloc_arrayN<BMEdge *>(totedgesel, "edge array");
    edindex = 0;

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
        edarray[edindex] = eed;
        edindex++;
      }
    }

    bool changed = false;
    if (is_ring) {
      for (edindex = 0; edindex < totedgesel; edindex += 1) {
        eed = edarray[edindex];
        changed |= walker_select(em, BMW_EDGERING, eed, true);
      }
      if (changed) {
        EDBM_selectmode_flush(em);
        EDBM_uvselect_clear(em);
      }
    }
    else {
      for (edindex = 0; edindex < totedgesel; edindex += 1) {
        eed = edarray[edindex];
        bool non_manifold = BM_edge_face_count_is_over(eed, 2);
        if (non_manifold) {
          changed |= walker_select(em, BMW_EDGELOOP_NONMANIFOLD, eed, true);
        }
        else {
          changed |= walker_select(em, BMW_EDGELOOP, eed, true);
        }
      }
      if (changed) {
        EDBM_selectmode_flush(em);
        EDBM_uvselect_clear(em);
      }
    }
    MEM_freeN(edarray);

    if (changed) {
      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_loop_multi_select(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Multi Select Loops";
  ot->idname = "MESH_OT_loop_multi_select";
  ot->description = "Select a loop of connected edges by connection type";

  /* API callbacks. */
  ot->exec = edbm_loop_multiselect_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_boolean(ot->srna, "ring", false, "Ring", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Loop (Cursor Pick) Operator
 * \{ */

static void mouse_mesh_loop_face(BMEditMesh *em, BMEdge *eed, bool select, bool select_clear)
{
  if (select_clear) {
    EDBM_flag_disable_all(em, BM_ELEM_SELECT);
  }

  walker_select(em, BMW_FACELOOP, eed, select);
}

static void mouse_mesh_loop_edge_ring(BMEditMesh *em, BMEdge *eed, bool select, bool select_clear)
{
  if (select_clear) {
    EDBM_flag_disable_all(em, BM_ELEM_SELECT);
  }

  walker_select(em, BMW_EDGERING, eed, select);
}

static void mouse_mesh_loop_edge(
    BMEditMesh *em, BMEdge *eed, bool select, bool select_clear, bool select_cycle)
{
  bool edge_boundary = false;
  bool non_manifold = BM_edge_face_count_is_over(eed, 2);

  /* Cycle between BMW_EDGELOOP / BMW_EDGEBOUNDARY. */
  if (select_cycle && BM_edge_is_boundary(eed)) {
    int count_by_select[2];

    /* If the loops selected toggle the boundaries. */
    walker_select_count(em, BMW_EDGELOOP, eed, count_by_select);
    if (count_by_select[!select] == 0) {
      edge_boundary = true;

      /* If the boundaries selected, toggle back to the loop. */
      walker_select_count(em, BMW_EDGEBOUNDARY, eed, count_by_select);
      if (count_by_select[!select] == 0) {
        edge_boundary = false;
      }
    }
  }

  if (select_clear) {
    EDBM_flag_disable_all(em, BM_ELEM_SELECT);
  }

  if (edge_boundary) {
    walker_select(em, BMW_EDGEBOUNDARY, eed, select);
  }
  else if (non_manifold) {
    walker_select(em, BMW_EDGELOOP_NONMANIFOLD, eed, select);
  }
  else {
    walker_select(em, BMW_EDGELOOP, eed, select);
  }
}

static bool mouse_mesh_loop(
    bContext *C, const int mval[2], bool extend, bool deselect, bool toggle, bool ring)
{
  Base *basact = nullptr;
  BMVert *eve = nullptr;
  BMEdge *eed = nullptr;
  BMFace *efa = nullptr;

  BMEditMesh *em;
  bool select = true;
  bool select_clear = false;
  bool select_cycle = true;
  float mvalf[2];

  ViewContext vc = em_setup_viewcontext(C);
  mvalf[0] = float(vc.mval[0] = mval[0]);
  mvalf[1] = float(vc.mval[1] = mval[1]);

  BMEditMesh *em_original = vc.em;
  const short selectmode = em_original->selectmode;
  em_original->selectmode = SCE_SELECT_EDGE;

  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode(
      vc.scene, vc.view_layer, vc.v3d);

  {
    int base_index = -1;
    if (EDBM_unified_findnearest(&vc, bases, &base_index, &eve, &eed, &efa)) {
      basact = bases[base_index];
      ED_view3d_viewcontext_init_object(&vc, basact->object);
      em = vc.em;
    }
    else {
      em = nullptr;
    }
  }

  em_original->selectmode = selectmode;

  if (em == nullptr || eed == nullptr) {
    return false;
  }

  if (extend == false && deselect == false && toggle == false) {
    select_clear = true;
  }

  if (extend) {
    select = true;
  }
  else if (deselect) {
    select = false;
  }
  else if (select_clear || (BM_elem_flag_test(eed, BM_ELEM_SELECT) == 0)) {
    select = true;
  }
  else if (toggle) {
    select = false;
    select_cycle = false;
  }

  if (select_clear) {
    for (Base *base_iter : bases) {
      Object *ob_iter = base_iter->object;
      BMEditMesh *em_iter = BKE_editmesh_from_object(ob_iter);

      if (em_iter->bm->totvertsel == 0) {
        continue;
      }

      if (em_iter == em) {
        continue;
      }

      EDBM_flag_disable_all(em_iter, BM_ELEM_SELECT);
      DEG_id_tag_update(static_cast<ID *>(ob_iter->data), ID_RECALC_SELECT);
    }
  }

  if (em->selectmode & SCE_SELECT_FACE) {
    mouse_mesh_loop_face(em, eed, select, select_clear);
  }
  else {
    if (ring) {
      mouse_mesh_loop_edge_ring(em, eed, select, select_clear);
    }
    else {
      mouse_mesh_loop_edge(em, eed, select, select_clear, select_cycle);
    }
  }

  EDBM_selectmode_flush(em);
  EDBM_uvselect_clear(em);

  /* Sets as active, useful for other tools. */
  if (select) {
    if (em->selectmode & SCE_SELECT_VERTEX) {
      /* Find nearest vert from mouse
       * (initialize to large values in case only one vertex can be projected). */
      float v1_co[2], v2_co[2];
      float length_1 = FLT_MAX;
      float length_2 = FLT_MAX;

      /* We can't be sure this has already been set... */
      ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

      if (ED_view3d_project_float_object(vc.region, eed->v1->co, v1_co, V3D_PROJ_TEST_CLIP_NEAR) ==
          V3D_PROJ_RET_OK)
      {
        length_1 = len_squared_v2v2(mvalf, v1_co);
      }

      if (ED_view3d_project_float_object(vc.region, eed->v2->co, v2_co, V3D_PROJ_TEST_CLIP_NEAR) ==
          V3D_PROJ_RET_OK)
      {
        length_2 = len_squared_v2v2(mvalf, v2_co);
      }
#if 0
      printf("mouse to v1: %f\nmouse to v2: %f\n",
             len_squared_v2v2(mvalf, v1_co),
             len_squared_v2v2(mvalf, v2_co));
#endif
      BM_select_history_store(em->bm, (length_1 < length_2) ? eed->v1 : eed->v2);
    }
    else if (em->selectmode & SCE_SELECT_EDGE) {
      BM_select_history_store(em->bm, eed);
    }
    else if (em->selectmode & SCE_SELECT_FACE) {
      /* Select the face of eed which is the nearest of mouse. */
      BMFace *f;
      BMIter iterf;
      float best_dist = FLT_MAX;
      efa = nullptr;

      /* We can't be sure this has already been set... */
      ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

      BM_ITER_ELEM (f, &iterf, eed, BM_FACES_OF_EDGE) {
        if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
          float cent[3];
          float co[2], tdist;

          BM_face_calc_center_median(f, cent);
          if (ED_view3d_project_float_object(vc.region, cent, co, V3D_PROJ_TEST_CLIP_NEAR) ==
              V3D_PROJ_RET_OK)
          {
            tdist = len_squared_v2v2(mvalf, co);
            if (tdist < best_dist) {
              // printf("Best face: %p (%f)\n", f, tdist);
              best_dist = tdist;
              efa = f;
            }
          }
        }
      }
      if (efa) {
        BM_mesh_active_face_set(em->bm, efa);
        BM_select_history_store(em->bm, efa);
      }
    }
  }

  DEG_id_tag_update(static_cast<ID *>(vc.obedit->data), ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);

  return true;
}

static wmOperatorStatus edbm_select_loop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{

  view3d_operator_needs_gpu(C);

  if (mouse_mesh_loop(C,
                      event->mval,
                      RNA_boolean_get(op->ptr, "extend"),
                      RNA_boolean_get(op->ptr, "deselect"),
                      RNA_boolean_get(op->ptr, "toggle"),
                      RNA_boolean_get(op->ptr, "ring")))
  {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void MESH_OT_loop_select(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Loop Select";
  ot->idname = "MESH_OT_loop_select";
  ot->description = "Select a loop of connected edges";

  /* API callbacks. */
  ot->invoke = edbm_select_loop_invoke;
  ot->poll = ED_operator_editmesh_region_view3d;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;

  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend Select", "Extend the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "Remove from the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "toggle", false, "Toggle Select", "Toggle the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "ring", false, "Select Ring", "Select ring");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void MESH_OT_edgering_select(wmOperatorType *ot)
{
  /* Description. */
  ot->name = "Edge Ring Select";
  ot->idname = "MESH_OT_edgering_select";
  ot->description = "Select an edge ring";

  /* Callbacks. */
  ot->invoke = edbm_select_loop_invoke;
  ot->poll = ED_operator_editmesh_region_view3d;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "Remove from the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "toggle", false, "Toggle Select", "Toggle the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "ring", true, "Select Ring", "Select ring");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name (De)Select All Operator
 * \{ */

static wmOperatorStatus edbm_select_all_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int action = RNA_enum_get(op->ptr, "action");

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    for (Object *obedit : objects) {
      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      if (em->bm->totvertsel || em->bm->totedgesel || em->bm->totfacesel) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    switch (action) {
      case SEL_SELECT:
        EDBM_flag_enable_all(em, BM_ELEM_SELECT);
        break;
      case SEL_DESELECT:
        EDBM_flag_disable_all(em, BM_ELEM_SELECT);
        break;
      case SEL_INVERT:
        if (em->bm->uv_select_sync_valid) {
          ED_uvedit_deselect_all(scene, obedit, SEL_INVERT);
        }
        else {
          EDBM_select_swap(em);
          EDBM_selectmode_flush(em);
        }
        break;
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "(De)select All";
  ot->idname = "MESH_OT_select_all";
  ot->description = "(De)select all vertices, edges or faces";

  /* API callbacks. */
  ot->exec = edbm_select_all_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Interior Faces Operator
 * \{ */

static wmOperatorStatus edbm_faces_select_interior_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (!EDBM_select_interior_faces(em)) {
      continue;
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_interior_faces(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Interior Faces";
  ot->idname = "MESH_OT_select_interior_faces";
  ot->description = "Select faces where all edges have more than 2 face users";

  /* API callbacks. */
  ot->exec = edbm_faces_select_interior_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Picking API
 *
 * Here actual select happens,
 * Gets called via generic mouse select operator.
 * \{ */

bool EDBM_select_pick(bContext *C, const int mval[2], const SelectPick_Params &params)
{
  int base_index_active = -1;
  BMVert *eve = nullptr;
  BMEdge *eed = nullptr;
  BMFace *efa = nullptr;

  /* Setup view context for argument to callbacks. */
  ViewContext vc = em_setup_viewcontext(C);
  vc.mval[0] = mval[0];
  vc.mval[1] = mval[1];

  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode(
      vc.scene, vc.view_layer, vc.v3d);

  bool changed = false;
  bool found = unified_findnearest(&vc, bases, &base_index_active, &eve, &eed, &efa);

  if (params.sel_op == SEL_OP_SET) {
    BMElem *ele = efa ? (BMElem *)efa : (eed ? (BMElem *)eed : (BMElem *)eve);
    if ((found && params.select_passthrough) && BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
      found = false;
    }
    else if (found || params.deselect_all) {
      /* Deselect everything. */
      for (Base *base_iter : bases) {
        Object *ob_iter = base_iter->object;
        EDBM_flag_disable_all(BKE_editmesh_from_object(ob_iter), BM_ELEM_SELECT);
        DEG_id_tag_update(static_cast<ID *>(ob_iter->data), ID_RECALC_SELECT);
        WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob_iter->data);
      }
      changed = true;
    }
  }

  if (found) {
    Base *basact = bases[base_index_active];
    ED_view3d_viewcontext_init_object(&vc, basact->object);
    Object *obedit = vc.obedit;
    BMEditMesh *em = vc.em;
    BMesh *bm = em->bm;

    const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_PROP_FLOAT2);
    const BMUVSelectPickParams uv_pick_params = {
        /*cd_loop_uv_offset*/ cd_loop_uv_offset,
        /*shared*/ vc.scene->toolsettings->uv_sticky == UV_STICKY_LOCATION,
    };

    if (efa) {
      switch (params.sel_op) {
        case SEL_OP_ADD: {
          BM_mesh_active_face_set(bm, efa);

          /* Work-around: deselect first, so we can guarantee it will
           * be active even if it was already selected. */
          BM_select_history_remove(bm, efa);
          BM_face_select_set(bm, efa, false);
          BM_select_history_store(bm, efa);
          BM_face_select_set(bm, efa, true);
          if (bm->uv_select_sync_valid) {
            BM_face_uvselect_set_pick(bm, efa, true, uv_pick_params);
          }
          break;
        }
        case SEL_OP_SUB: {
          BM_select_history_remove(bm, efa);
          BM_face_select_set(bm, efa, false);
          break;
        }
        case SEL_OP_XOR: {
          BM_mesh_active_face_set(bm, efa);
          if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
            BM_select_history_store(bm, efa);
            BM_face_select_set(bm, efa, true);
            if (bm->uv_select_sync_valid) {
              BM_face_uvselect_set_pick(bm, efa, true, uv_pick_params);
            }
          }
          else {
            BM_select_history_remove(bm, efa);
            BM_face_select_set(bm, efa, false);
            if (bm->uv_select_sync_valid) {
              BM_face_uvselect_set_pick(bm, efa, false, uv_pick_params);
            }
          }
          break;
        }
        case SEL_OP_SET: {
          BM_mesh_active_face_set(bm, efa);
          if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
            BM_select_history_store(bm, efa);
            BM_face_select_set(bm, efa, true);
          }
          /* UV select will have been cleared. */
          break;
        }
        case SEL_OP_AND: {
          BLI_assert_unreachable(); /* Doesn't make sense for picking. */
          break;
        }
      }
    }
    else if (eed) {

      switch (params.sel_op) {
        case SEL_OP_ADD: {
          /* Work-around: deselect first, so we can guarantee it will
           * be active even if it was already selected. */
          BM_select_history_remove(bm, eed);
          BM_edge_select_set(bm, eed, false);
          BM_select_history_store(bm, eed);
          BM_edge_select_set(bm, eed, true);
          if (bm->uv_select_sync_valid) {
            BM_edge_uvselect_set_pick(bm, eed, true, uv_pick_params);
          }
          break;
        }
        case SEL_OP_SUB: {
          BM_select_history_remove(bm, eed);
          BM_edge_select_set(bm, eed, false);
          if (bm->uv_select_sync_valid) {
            BM_edge_uvselect_set_pick(bm, eed, false, uv_pick_params);
          }
          break;
        }
        case SEL_OP_XOR: {
          if (!BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
            BM_select_history_store(bm, eed);
            BM_edge_select_set(bm, eed, true);
            if (bm->uv_select_sync_valid) {
              BM_edge_uvselect_set_pick(bm, eed, true, uv_pick_params);
            }
          }
          else {
            BM_select_history_remove(bm, eed);
            BM_edge_select_set(bm, eed, false);
            if (bm->uv_select_sync_valid) {
              BM_edge_uvselect_set_pick(bm, eed, false, uv_pick_params);
            }
          }
          break;
        }
        case SEL_OP_SET: {
          if (!BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
            BM_select_history_store(bm, eed);
            BM_edge_select_set(bm, eed, true);
          }
          break;
        }
        case SEL_OP_AND: {
          BLI_assert_unreachable(); /* Doesn't make sense for picking. */
          break;
        }
      }
    }
    else if (eve) {
      switch (params.sel_op) {
        case SEL_OP_ADD: {
          /* Work-around: deselect first, so we can guarantee it will
           * be active even if it was already selected. */
          BM_select_history_remove(bm, eve);
          BM_vert_select_set(bm, eve, false);
          BM_select_history_store(bm, eve);
          BM_vert_select_set(bm, eve, true);
          if (bm->uv_select_sync_valid) {
            BM_vert_uvselect_set_pick(bm, eve, true, uv_pick_params);
          }
          break;
        }
        case SEL_OP_SUB: {
          BM_select_history_remove(bm, eve);
          BM_vert_select_set(bm, eve, false);
          if (bm->uv_select_sync_valid) {
            BM_vert_uvselect_set_pick(bm, eve, false, uv_pick_params);
          }
          break;
        }
        case SEL_OP_XOR: {
          if (!BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
            BM_select_history_store(bm, eve);
            BM_vert_select_set(bm, eve, true);
            if (bm->uv_select_sync_valid) {
              BM_vert_uvselect_set_pick(bm, eve, true, uv_pick_params);
            }
          }
          else {
            BM_select_history_remove(bm, eve);
            BM_vert_select_set(bm, eve, false);
            if (bm->uv_select_sync_valid) {
              BM_vert_uvselect_set_pick(bm, eve, false, uv_pick_params);
            }
          }
          break;
        }
        case SEL_OP_SET: {
          if (!BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
            BM_select_history_store(bm, eve);
            BM_vert_select_set(bm, eve, true);
          }
          break;
        }
        case SEL_OP_AND: {
          BLI_assert_unreachable(); /* Doesn't make sense for picking. */
          break;
        }
      }
    }

    EDBM_selectmode_flush(em);

    if (efa) {
      blender::ed::object::material_active_index_set(obedit, efa->mat_nr);
      em->mat_nr = efa->mat_nr;
    }

    /* Changing active object is handy since it allows us to
     * switch UV layers, vgroups for eg. */
    BKE_view_layer_synced_ensure(vc.scene, vc.view_layer);
    if (BKE_view_layer_active_base_get(vc.view_layer) != basact) {
      blender::ed::object::base_activate(C, basact);
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

    changed = true;
  }

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Mode Utilities
 * \{ */

static void edbm_strip_selections(BMEditMesh *em)
{
  BMEditSelection *ese, *nextese;

  if (!(em->selectmode & SCE_SELECT_VERTEX)) {
    ese = static_cast<BMEditSelection *>(em->bm->selected.first);
    while (ese) {
      nextese = ese->next;
      if (ese->htype == BM_VERT) {
        BLI_freelinkN(&(em->bm->selected), ese);
      }
      ese = nextese;
    }
  }
  if (!(em->selectmode & SCE_SELECT_EDGE)) {
    ese = static_cast<BMEditSelection *>(em->bm->selected.first);
    while (ese) {
      nextese = ese->next;
      if (ese->htype == BM_EDGE) {
        BLI_freelinkN(&(em->bm->selected), ese);
      }
      ese = nextese;
    }
  }
  if (!(em->selectmode & SCE_SELECT_FACE)) {
    ese = static_cast<BMEditSelection *>(em->bm->selected.first);
    while (ese) {
      nextese = ese->next;
      if (ese->htype == BM_FACE) {
        BLI_freelinkN(&(em->bm->selected), ese);
      }
      ese = nextese;
    }
  }
}

void EDBM_selectmode_set(BMEditMesh *em, const short selectmode)
{
  BMVert *eve;
  BMEdge *eed;
  BMFace *efa;
  BMIter iter;

  const short selectmode_prev = em->selectmode;
  em->selectmode = selectmode;
  em->bm->selectmode = selectmode;

  /* Strip stored selection isn't relevant to the new mode. */
  edbm_strip_selections(em);

  if (em->bm->totvertsel == 0 && em->bm->totedgesel == 0 && em->bm->totfacesel == 0) {
    return;
  }

  if (em->selectmode & SCE_SELECT_VERTEX) {
    if (em->bm->totvertsel) {
      EDBM_select_flush_from_verts(em, true);
    }
  }
  else if (em->selectmode & SCE_SELECT_EDGE) {
    /* Deselect vertices, and select again based on edge select. */
    BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
      BM_vert_select_set(em->bm, eve, false);
    }

    if (em->bm->totedgesel) {
      BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          BM_edge_select_set(em->bm, eed, true);
        }
      }

      /* Selects faces based on edge status. */
      EDBM_selectmode_flush(em);
    }
  }
  else if (em->selectmode & SCE_SELECT_FACE) {
    /* Deselect edges, and select again based on face select. */
    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      BM_edge_select_set(em->bm, eed, false);
    }

    if (em->bm->totfacesel) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          BM_face_select_set(em->bm, efa, true);
        }
      }
    }
  }

  if (em->bm->uv_select_sync_valid) {
    /* NOTE(@ideasman42): this could/should use the "sticky" tool setting.
     * Although in practice it's OK to assume "connected" sticky in this case. */
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_mesh_uvselect_mode_flush_update(em->bm, selectmode_prev, selectmode, cd_loop_uv_offset);
  }
}

void EDBM_selectmode_convert(BMEditMesh *em,
                             const short selectmode_old,
                             const short selectmode_new)
{
  /* NOTE: it's important only the selection modes passed in a re used,
   * not the meshes current selection mode because this is called when the
   * selection mode is being manipulated (see: #EDBM_selectmode_toggle_multi). */

  BMesh *bm = em->bm;

  BMVert *eve;
  BMEdge *eed;
  BMFace *efa;
  BMIter iter;

  /* First tag-to-select, then select.
   * This avoids a feedback loop. */

  /* Have to find out what the selection-mode was previously. */
  if (selectmode_old == SCE_SELECT_VERTEX) {
    if (bm->totvertsel == 0) {
      /* Pass. */
    }
    else if (selectmode_new == SCE_SELECT_EDGE) {
      /* Flush up (vert -> edge). */

      /* Select all edges associated with every selected vert. */
      BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
        BM_elem_flag_set(eed, BM_ELEM_TAG, BM_edge_is_any_vert_flag_test(eed, BM_ELEM_SELECT));
      }

      BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(eed, BM_ELEM_TAG)) {
          BM_edge_select_set(bm, eed, true);
        }
      }
    }
    else if (selectmode_new == SCE_SELECT_FACE) {
      /* Flush up (vert -> face). */

      /* Select all faces associated with every selected vert. */
      BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
        BM_elem_flag_set(efa, BM_ELEM_TAG, BM_face_is_any_vert_flag_test(efa, BM_ELEM_SELECT));
      }

      BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
          BM_face_select_set(bm, efa, true);
        }
      }
    }
  }
  else if (selectmode_old == SCE_SELECT_EDGE) {
    if (bm->totedgesel == 0) {
      /* Pass. */
    }
    else if (selectmode_new == SCE_SELECT_FACE) {
      /* Flush up (edge -> face). */

      /* Select all faces associated with every selected edge. */
      BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
        BM_elem_flag_set(efa, BM_ELEM_TAG, BM_face_is_any_edge_flag_test(efa, BM_ELEM_SELECT));
      }

      BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
          BM_face_select_set(bm, efa, true);
        }
      }
    }
    else if (selectmode_new == SCE_SELECT_VERTEX) {
      /* Flush down (edge -> vert). */

      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (!BM_vert_is_all_edge_flag_test(eve, BM_ELEM_SELECT, true)) {
          BM_vert_select_set(bm, eve, false);
        }
      }
      /* Deselect edges without both verts selected. */
      BM_mesh_select_flush_from_verts(bm, false);
    }
  }
  else if (selectmode_old == SCE_SELECT_FACE) {
    if (bm->totfacesel == 0) {
      /* Pass. */
    }
    else if (selectmode_new == SCE_SELECT_EDGE) {
      /* Flush down (face -> edge). */

      BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
        if (!BM_edge_is_all_face_flag_test(eed, BM_ELEM_SELECT, true)) {
          BM_edge_select_set(bm, eed, false);
        }
      }
      /* Deselect faces without edges selected. */
      BM_mesh_select_flush_from_verts(bm, false);
    }
    else if (selectmode_new == SCE_SELECT_VERTEX) {
      /* Flush down (face -> vert). */

      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (!BM_vert_is_all_face_flag_test(eve, BM_ELEM_SELECT, true)) {
          BM_vert_select_set(bm, eve, false);
        }
      }
      /* Deselect faces without verts selected. */
      BM_mesh_select_flush_from_verts(bm, false);
    }
  }
}

bool EDBM_selectmode_toggle_multi(bContext *C,
                                  const short selectmode_toggle,
                                  const int action,
                                  const bool use_extend,
                                  const bool use_expand)
{
  BLI_assert(ELEM(selectmode_toggle, SCE_SELECT_VERTEX, SCE_SELECT_EDGE, SCE_SELECT_FACE));
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  bool ret = false;

  short selectmode_new;
  /* Avoid mixing up the active/iterable edit-mesh by limiting its scope. */
  {
    Object *obedit = CTX_data_edit_object(C);
    BMEditMesh *em = nullptr;

    if (obedit && obedit->type == OB_MESH) {
      em = BKE_editmesh_from_object(obedit);
    }

    if (em == nullptr) {
      return ret;
    }

    selectmode_new = em->selectmode;
  }
  /* Assign before the new value is modified. */
  const short selectmode_old = selectmode_new;

  bool only_update = false;
  switch (action) {
    case -1:
      /* Already set. */
      break;
    case 0: /* Disable. */
      /* Check we have something to do. */
      if ((selectmode_old & selectmode_toggle) == 0) {
        only_update = true;
        break;
      }
      selectmode_new &= ~selectmode_toggle;
      break;
    case 1: /* Enable. */
      /* Check we have something to do. */
      if ((selectmode_old & selectmode_toggle) != 0) {
        only_update = true;
        break;
      }
      selectmode_new |= selectmode_toggle;
      break;
    case 2: /* Toggle. */
      /* Can't disable this flag if its the only one set. */
      if (selectmode_old == selectmode_toggle) {
        only_update = true;
        break;
      }
      selectmode_new ^= selectmode_toggle;
      break;
    default:
      BLI_assert(0);
      break;
  }

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  if (only_update) {
    for (Object *ob_iter : objects) {
      BMEditMesh *em_iter = BKE_editmesh_from_object(ob_iter);
      em_iter->selectmode = selectmode_new;
    }

    return false;
  }

  /* WARNING: unfortunately failing to ensure this causes problems in *some* cases.
   * Adding UV data has negative performance impacts, but failing to do this means
   * switching to the UV editor *might* should strange selection.
   * Since we can't know if users will proceed to do UV editing after switching modes,
   * ensure the UV data.
   *
   * Even though the data is added, it's only added if it's needed,
   * so selecting all/none or when there are no UV's.
   *
   * Failing to do this means switching from face to vertex selection modes
   * will leave vertices on adjacent islands selected - which seems like a bug. */
  bool use_uv_select_ensure = false;

  /* Only do this when sync-select is enabled so users can have better
   * performance when editing high poly meshes. */
  if (ts->uv_flag & UV_FLAG_SELECT_SYNC) {
    /* Only when flushing down. */
    if ((bitscan_forward_i(selectmode_new) < bitscan_forward_i(selectmode_old))) {
      use_uv_select_ensure = true;
    }
  }

  if (use_extend == false || selectmode_new == 0) {
    if (use_expand) {
      const short selectmode_max = highest_order_bit_s(selectmode_old);
      for (Object *ob_iter : objects) {
        BMEditMesh *em_iter = BKE_editmesh_from_object(ob_iter);
        EDBM_selectmode_convert(em_iter, selectmode_max, selectmode_toggle);
        /* NOTE: This could be supported, but converting UV's too is reasonably complicated.
         * This can be considered a low priority TODO. */
        EDBM_uvselect_clear(em_iter);
      }
      use_uv_select_ensure = false;
    }
  }

  switch (selectmode_toggle) {
    case SCE_SELECT_VERTEX:
      if (use_extend == false || selectmode_new == 0) {
        selectmode_new = SCE_SELECT_VERTEX;
      }
      ret = true;
      break;
    case SCE_SELECT_EDGE:
      if (use_extend == false || selectmode_new == 0) {
        selectmode_new = SCE_SELECT_EDGE;
      }
      ret = true;
      break;
    case SCE_SELECT_FACE:
      if (use_extend == false || selectmode_new == 0) {
        selectmode_new = SCE_SELECT_FACE;
      }
      ret = true;
      break;
    default:
      BLI_assert(0);
      break;
  }

  if (ret == true) {
    BLI_assert(selectmode_new != 0);
    for (Object *ob_iter : objects) {
      BMEditMesh *em_iter = BKE_editmesh_from_object(ob_iter);

      if (use_uv_select_ensure) {
        if (BM_mesh_select_is_mixed(em_iter->bm)) {
          ED_uvedit_sync_uvselect_ensure_if_needed(ts, em_iter->bm);
        }
        else {
          EDBM_uvselect_clear(em_iter);
        }
      }

      EDBM_selectmode_set(em_iter, selectmode_new);
      DEG_id_tag_update(static_cast<ID *>(ob_iter->data),
                        ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob_iter->data);
    }

    ts->selectmode = selectmode_new;
    WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);
    DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  }

  return ret;
}

bool EDBM_selectmode_set_multi_ex(Scene *scene, Span<Object *> objects, const short selectmode)
{
  ToolSettings *ts = scene->toolsettings;
  bool changed = false;
  bool changed_toolsettings = false;

  if (ts->selectmode != selectmode) {
    ts->selectmode = selectmode;
    changed_toolsettings = true;
  }

  for (Object *ob_iter : objects) {
    BMEditMesh *em_iter = BKE_editmesh_from_object(ob_iter);
    if (em_iter->selectmode == selectmode) {
      continue;
    }
    EDBM_selectmode_set(em_iter, selectmode);
    DEG_id_tag_update(static_cast<ID *>(ob_iter->data), ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT);
    WM_main_add_notifier(NC_GEOM | ND_SELECT, ob_iter->data);
    changed = true;
  }

  if (changed_toolsettings) {
    WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);
    DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  }

  return changed || changed_toolsettings;
}

bool EDBM_selectmode_set_multi(bContext *C, const short selectmode)
{
  BLI_assert(selectmode != 0);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (!(obact && (obact->type == OB_MESH) && (obact->mode & OB_MODE_EDIT) &&
        (BKE_editmesh_from_object(obact) != nullptr)))
  {
    return false;
  }

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  return EDBM_selectmode_set_multi_ex(scene, objects, selectmode);
}

/**
 * Ensure all edit-meshes have the same select-mode.
 *
 * While this is almost always the case as the UI syncs the values when set,
 * it's not guaranteed because objects can be shared across scenes and each
 * scene has its own select-mode which is applied to the object when entering edit-mode.
 *
 * This function should only be used when an operation would cause errors
 * when applied in the wrong selection mode.
 *
 * \return True when a change was made.
 */
static bool edbm_selectmode_sync_multi_ex(Span<Object *> objects)
{
  if (objects.size() <= 1) {
    return false;
  }

  bool changed = false;
  BMEditMesh *em_active = BKE_editmesh_from_object(objects[0]);
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (em_active->selectmode == em->selectmode) {
      continue;
    }
    EDBM_selectmode_set(em, em_active->selectmode);
    changed = true;

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT);
    WM_main_add_notifier(NC_GEOM | ND_SELECT, obedit->data);
  }

  return changed;
}

bool EDBM_selectmode_disable(Scene *scene,
                             BMEditMesh *em,
                             const short selectmode_disable,
                             const short selectmode_fallback)
{
  /* Not essential, but switch out of vertex mode since the
   * selected regions won't be nicely isolated after flushing. */
  if (em->selectmode & selectmode_disable) {
    const short selectmode = (em->selectmode == selectmode_disable) ?
                                 selectmode_fallback :
                                 (em->selectmode & ~selectmode_disable);
    scene->toolsettings->selectmode = selectmode;
    EDBM_selectmode_set(em, selectmode);

    WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, scene);

    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Toggle
 * \{ */

bool EDBM_deselect_by_material(BMEditMesh *em, const short index, const bool select)
{
  BMIter iter;
  BMFace *efa;
  bool changed = false;

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
      continue;
    }
    if (efa->mat_nr == index) {
      changed = true;
      BM_face_select_set(em->bm, efa, select);
    }
  }
  return changed;
}

void EDBM_select_toggle_all(BMEditMesh *em) /* Exported for UV. */
{
  if (em->bm->totvertsel || em->bm->totedgesel || em->bm->totfacesel) {
    EDBM_flag_disable_all(em, BM_ELEM_SELECT);
  }
  else {
    EDBM_flag_enable_all(em, BM_ELEM_SELECT);
  }
}

void EDBM_select_swap(BMEditMesh *em) /* Exported for UV. */
{
  BMIter iter;
  BMVert *eve;
  BMEdge *eed;
  BMFace *efa;

  if (em->bm->selectmode & SCE_SELECT_VERTEX) {
    BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
        continue;
      }
      BM_vert_select_set(em->bm, eve, !BM_elem_flag_test(eve, BM_ELEM_SELECT));
    }
  }
  else if (em->selectmode & SCE_SELECT_EDGE) {
    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
        continue;
      }
      BM_edge_select_set(em->bm, eed, !BM_elem_flag_test(eed, BM_ELEM_SELECT));
    }
  }
  else {
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
        continue;
      }
      BM_face_select_set(em->bm, efa, !BM_elem_flag_test(efa, BM_ELEM_SELECT));
    }
  }
}

bool EDBM_mesh_deselect_all_multi_ex(const Span<Base *> bases)
{
  bool changed_multi = false;
  for (Base *base_iter : bases) {
    Object *ob_iter = base_iter->object;
    BMEditMesh *em_iter = BKE_editmesh_from_object(ob_iter);

    if (em_iter->bm->totvertsel == 0) {
      continue;
    }

    EDBM_flag_disable_all(em_iter, BM_ELEM_SELECT);
    DEG_id_tag_update(static_cast<ID *>(ob_iter->data), ID_RECALC_SELECT);
    changed_multi = true;
  }
  return changed_multi;
}

bool EDBM_mesh_deselect_all_multi(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.scene, vc.view_layer, vc.v3d);
  return EDBM_mesh_deselect_all_multi_ex(bases);
}

bool EDBM_selectmode_disable_multi_ex(Scene *scene,
                                      const Span<Base *> bases,
                                      const short selectmode_disable,
                                      const short selectmode_fallback)
{
  bool changed_multi = false;
  for (Base *base_iter : bases) {
    Object *ob_iter = base_iter->object;
    BMEditMesh *em_iter = BKE_editmesh_from_object(ob_iter);

    if (EDBM_selectmode_disable(scene, em_iter, selectmode_disable, selectmode_fallback)) {
      changed_multi = true;
    }
  }
  return changed_multi;
}

bool EDBM_selectmode_disable_multi(bContext *C,
                                   const short selectmode_disable,
                                   const short selectmode_fallback)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.scene, vc.view_layer, nullptr);
  return EDBM_selectmode_disable_multi_ex(scene, bases, selectmode_disable, selectmode_fallback);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Interior Faces
 *
 * Overview of the algorithm:
 * - Groups faces surrounded by edges with 3+ faces using them.
 * - Calculates a cost of each face group comparing its angle with the faces
 *   connected to its non-manifold edges.
 * - Mark the face group as interior, and mark connected face groups for recalculation.
 * - Continue to remove the face groups with the highest 'cost'.
 *
 * \{ */

struct BMFaceLink {
  BMFaceLink *next, *prev;
  BMFace *face;
  float area;
};

static bool bm_interior_loop_filter_fn(const BMLoop *l, void * /*user_data*/)
{
  if (BM_elem_flag_test(l->e, BM_ELEM_TAG)) {
    return false;
  }
  return true;
}
static bool bm_interior_edge_is_manifold_except_face_index(BMEdge *e,
                                                           int face_index,
                                                           BMLoop *r_l_pair[2])
{

  BMLoop *l_iter = e->l;
  int loop_index = 0;
  do {
    BMFace *f = l_iter->f;
    int i = BM_elem_index_get(f);
    if (!ELEM(i, -1, face_index)) {
      if (loop_index == 2) {
        return false;
      }
      r_l_pair[loop_index++] = l_iter;
    }
  } while ((l_iter = l_iter->radial_next) != e->l);
  return (loop_index == 2);
}

/**
 * Calculate the cost of the face group.
 * A higher value means it's more likely to remove first.
 */
static float bm_interior_face_group_calc_cost(ListBase *ls, const float *edge_lengths)
{
  /* Dividing by the area is important so larger face groups (which will become the outer shell)
   * aren't detected as having a high cost. */
  float area = 0.0f;
  float cost = 0.0f;
  bool found = false;
  LISTBASE_FOREACH (BMFaceLink *, f_link, ls) {
    BMFace *f = f_link->face;
    area += f_link->area;
    int i = BM_elem_index_get(f);
    BLI_assert(i != -1);
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (BM_elem_flag_test(l_iter->e, BM_ELEM_TAG)) {
        float cost_test = 0.0f;
        int cost_count = 0;
        /* All other faces. */
        BMLoop *l_radial_iter = l_iter;
        do {
          int i_other = BM_elem_index_get(l_radial_iter->f);
          if (!ELEM(i_other, -1, i)) {
            float angle = angle_normalized_v3v3(f->no, l_radial_iter->f->no);
            /* Ignore face direction since in the case on non-manifold faces connecting edges,
             * the face flipping may not be meaningful. */
            if (angle > DEG2RADF(90)) {
              angle = DEG2RADF(180) - angle;
            }
            /* Avoid calculating it inline, pass in pre-calculated edge lengths. */
#if 0
            cost_test += BM_edge_calc_length(l_iter->e) * angle;
#else
            BLI_assert(edge_lengths[BM_elem_index_get(l_iter->e)] != -1.0f);
            cost_test += edge_lengths[BM_elem_index_get(l_iter->e)] * angle;
#endif
            cost_count += 1;
          }
        } while ((l_radial_iter = l_radial_iter->radial_next) != l_iter);

        if (cost_count >= 2) {
          cost += cost_test;
          found = true;
        }
      }
    } while ((l_iter = l_iter->next) != l_first);
  }
  return found ? cost / area : FLT_MAX;
}

bool EDBM_select_interior_faces(BMEditMesh *em)
{
  BMesh *bm = em->bm;
  BMIter iter;
  bool changed = false;

  float *edge_lengths = MEM_malloc_arrayN<float>(bm->totedge, __func__);

  {
    bool has_nonmanifold = false;
    BMEdge *e;
    int i;
    BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
      const bool is_over = BM_edge_face_count_is_over(e, 2);
      if (is_over) {
        BM_elem_flag_enable(e, BM_ELEM_TAG);
        has_nonmanifold = true;
        edge_lengths[i] = BM_edge_calc_length(e);
      }
      else {
        BM_elem_flag_disable(e, BM_ELEM_TAG);
        edge_lengths[i] = -1.0;
      }

      BM_elem_index_set(e, i); /* set_inline */
    }
    bm->elem_index_dirty &= ~BM_EDGE;

    if (has_nonmanifold == false) {
      MEM_freeN(edge_lengths);
      return false;
    }
  }

  /* Group variables. */
  int (*fgroup_index)[2];
  int fgroup_len;

  int *fgroup_array = MEM_malloc_arrayN<int>(bm->totface, __func__);
  fgroup_len = BM_mesh_calc_face_groups(
      bm, fgroup_array, &fgroup_index, bm_interior_loop_filter_fn, nullptr, nullptr, 0, BM_EDGE);

  int *fgroup_recalc_stack = MEM_malloc_arrayN<int>(fgroup_len, __func__);
  STACK_DECLARE(fgroup_recalc_stack);
  STACK_INIT(fgroup_recalc_stack, fgroup_len);

  BM_mesh_elem_table_ensure(bm, BM_FACE);

  {
    BMFace *f;
    BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_elem_index_set(f, -1); /* set_dirty! */
    }
  }
  bm->elem_index_dirty |= BM_FACE;

  ListBase *fgroup_listbase = MEM_calloc_arrayN<ListBase>(fgroup_len, __func__);
  BMFaceLink *f_link_array = MEM_calloc_arrayN<BMFaceLink>(bm->totface, __func__);

  for (int i = 0; i < fgroup_len; i++) {
    const int fg_sta = fgroup_index[i][0];
    const int fg_len = fgroup_index[i][1];
    for (int j = 0; j < fg_len; j++) {
      const int face_index = fgroup_array[fg_sta + j];
      BMFace *f = BM_face_at_index(bm, face_index);
      BM_elem_index_set(f, i);

      BMFaceLink *f_link = &f_link_array[face_index];
      f_link->face = f;
      f_link->area = BM_face_calc_area(f);
      BLI_addtail(&fgroup_listbase[i], f_link);
    }
  }

  MEM_freeN(fgroup_array);
  MEM_freeN(fgroup_index);

  Heap *fgroup_heap = BLI_heap_new_ex(fgroup_len);
  HeapNode **fgroup_table = MEM_malloc_arrayN<HeapNode *>(fgroup_len, __func__);
  bool *fgroup_dirty = MEM_calloc_arrayN<bool>(fgroup_len, __func__);

  for (int i = 0; i < fgroup_len; i++) {
    const float cost = bm_interior_face_group_calc_cost(&fgroup_listbase[i], edge_lengths);
    if (cost != FLT_MAX) {
      fgroup_table[i] = BLI_heap_insert(fgroup_heap, -cost, POINTER_FROM_INT(i));
    }
    else {
      fgroup_table[i] = nullptr;
    }
  }

  /* Avoid re-running cost calculations for large face-groups which will end up forming the
   * outer shell and not be considered interior.
   * As these face groups become increasingly bigger - their chance of being considered
   * interior reduces as does the time to calculate their cost.
   *
   * This delays recalculating them until they are considered can dates to remove
   * which becomes less and less likely as they increase in area. */

#define USE_DELAY_FACE_GROUP_COST_CALC

  while (true) {

#if defined(USE_DELAY_FACE_GROUP_COST_CALC)
    while (!BLI_heap_is_empty(fgroup_heap)) {
      HeapNode *node_min = BLI_heap_top(fgroup_heap);
      const int i = POINTER_AS_INT(BLI_heap_node_ptr(node_min));
      if (fgroup_dirty[i]) {
        const float cost = bm_interior_face_group_calc_cost(&fgroup_listbase[i], edge_lengths);
        if (cost != FLT_MAX) {
          /* The cost may have improves (we may be able to skip this),
           * however the cost should _never_ make this a choice. */
          BLI_assert(-BLI_heap_node_value(node_min) >= cost);
          BLI_heap_node_value_update(fgroup_heap, fgroup_table[i], -cost);
        }
        else {
          BLI_heap_remove(fgroup_heap, fgroup_table[i]);
          fgroup_table[i] = nullptr;
        }
        fgroup_dirty[i] = false;
      }
      else {
        break;
      }
    }
#endif

    if (BLI_heap_is_empty(fgroup_heap)) {
      break;
    }

    const int i_min = POINTER_AS_INT(BLI_heap_pop_min(fgroup_heap));
    BLI_assert(fgroup_table[i_min] != nullptr);
    BLI_assert(fgroup_dirty[i_min] == false);
    fgroup_table[i_min] = nullptr;
    changed = true;

    while (BMFaceLink *f_link = static_cast<BMFaceLink *>(BLI_pophead(&fgroup_listbase[i_min]))) {
      BMFace *f = f_link->face;
      BM_face_select_set(bm, f, true);
      BM_elem_index_set(f, -1); /* set_dirty */

      BMLoop *l_iter, *l_first;

      /* Loop over edges face edges, merging groups which are no longer separated
       * by non-manifold edges (when manifold check ignores faces from this group). */
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        BMLoop *l_pair[2];
        if (bm_interior_edge_is_manifold_except_face_index(l_iter->e, i_min, l_pair)) {
          BM_elem_flag_disable(l_iter->e, BM_ELEM_TAG);

          int i_a = BM_elem_index_get(l_pair[0]->f);
          int i_b = BM_elem_index_get(l_pair[1]->f);
          if (i_a != i_b) {
            /* Only for predictable results that don't depend on the order of radial loops,
             * not essential. */
            if (i_a > i_b) {
              std::swap(i_a, i_b);
            }

            /* Merge the groups. */
            LISTBASE_FOREACH (LinkData *, n, &fgroup_listbase[i_b]) {
              BMFace *f_iter = static_cast<BMFace *>(n->data);
              BM_elem_index_set(f_iter, i_a);
            }
            BLI_movelisttolist(&fgroup_listbase[i_a], &fgroup_listbase[i_b]);

            /* This may have been added to 'fgroup_recalc_stack', instead of removing it,
             * just check the heap node isn't nullptr before recalculating. */
            BLI_heap_remove(fgroup_heap, fgroup_table[i_b]);
            fgroup_table[i_b] = nullptr;
            /* Keep the dirty flag as-is for 'i_b', because it may be in the 'fgroup_recalc_stack'
             * and we don't want to add it again.
             * Instead rely on the 'fgroup_table[i_b]' being nullptr as a secondary check. */

            if (fgroup_dirty[i_a] == false) {
              BLI_assert(fgroup_table[i_a] != nullptr);
              STACK_PUSH(fgroup_recalc_stack, i_a);
              fgroup_dirty[i_a] = true;
            }
          }
        }

        /* Mark all connected groups for re-calculation. */
        BMLoop *l_radial_iter = l_iter->radial_next;
        if (l_radial_iter != l_iter) {
          do {
            int i_other = BM_elem_index_get(l_radial_iter->f);
            if (!ELEM(i_other, -1, i_min)) {
              if ((fgroup_table[i_other] != nullptr) && (fgroup_dirty[i_other] == false)) {
#if !defined(USE_DELAY_FACE_GROUP_COST_CALC)
                STACK_PUSH(fgroup_recalc_stack, i_other);
#endif
                fgroup_dirty[i_other] = true;
              }
            }
          } while ((l_radial_iter = l_radial_iter->radial_next) != l_iter);
        }

      } while ((l_iter = l_iter->next) != l_first);
    }

    for (int index = 0; index < STACK_SIZE(fgroup_recalc_stack); index++) {
      const int i = fgroup_recalc_stack[index];
      if (fgroup_table[i] != nullptr && fgroup_dirty[i] == true) {
        /* First update edge tags. */
        const float cost = bm_interior_face_group_calc_cost(&fgroup_listbase[i], edge_lengths);
        if (cost != FLT_MAX) {
          BLI_heap_node_value_update(fgroup_heap, fgroup_table[i], -cost);
        }
        else {
          BLI_heap_remove(fgroup_heap, fgroup_table[i]);
          fgroup_table[i] = nullptr;
        }
      }
      fgroup_dirty[i] = false;
    }
    STACK_CLEAR(fgroup_recalc_stack);
  }

  MEM_freeN(edge_lengths);
  MEM_freeN(f_link_array);
  MEM_freeN(fgroup_listbase);
  MEM_freeN(fgroup_recalc_stack);
  MEM_freeN(fgroup_table);
  MEM_freeN(fgroup_dirty);

  BLI_heap_free(fgroup_heap, nullptr);

  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 *
 * Support delimiting on different edge properties.
 * \{ */

/** So we can have last-used default depend on selection mode (rare exception!). */
#define USE_LINKED_SELECT_DEFAULT_HACK

struct DelimitData {
  eCustomDataType cd_loop_type;
  int cd_loop_offset;
};

static bool select_linked_delimit_test(BMEdge *e, int delimit, const DelimitData *delimit_data)
{
  BLI_assert(delimit);

  if (delimit & BMO_DELIM_SEAM) {
    if (BM_elem_flag_test(e, BM_ELEM_SEAM)) {
      return true;
    }
  }

  if (delimit & BMO_DELIM_SHARP) {
    if (BM_elem_flag_test(e, BM_ELEM_SMOOTH) == 0) {
      return true;
    }
  }

  if (delimit & BMO_DELIM_NORMAL) {
    if (!BM_edge_is_contiguous(e)) {
      return true;
    }
  }

  if (delimit & BMO_DELIM_MATERIAL) {
    if (e->l && e->l->radial_next != e->l) {
      const short mat_nr = e->l->f->mat_nr;
      BMLoop *l_iter = e->l->radial_next;
      do {
        if (l_iter->f->mat_nr != mat_nr) {
          return true;
        }
      } while ((l_iter = l_iter->radial_next) != e->l);
    }
  }

  if (delimit & BMO_DELIM_UV) {
    if (BM_edge_is_contiguous_loop_cd(
            e, delimit_data->cd_loop_type, delimit_data->cd_loop_offset) == 0)
    {
      return true;
    }
  }

  return false;
}

#ifdef USE_LINKED_SELECT_DEFAULT_HACK
/**
 * Gets the default from the operator fallback to own last-used value
 * (selected based on mode)
 */
static int select_linked_delimit_default_from_op(wmOperator *op, const int select_mode)
{
  static char delimit_last_store[2] = {0, BMO_DELIM_SEAM};
  int delimit_last_index = (select_mode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) == 0;
  char *delimit_last = &delimit_last_store[delimit_last_index];
  PropertyRNA *prop_delimit = RNA_struct_find_property(op->ptr, "delimit");
  int delimit;

  if (RNA_property_is_set(op->ptr, prop_delimit)) {
    delimit = RNA_property_enum_get(op->ptr, prop_delimit);
    *delimit_last = delimit;
  }
  else {
    delimit = *delimit_last;
    RNA_property_enum_set(op->ptr, prop_delimit, delimit);
  }
  return delimit;
}
#endif

static void select_linked_delimit_validate(BMesh *bm, int *delimit)
{
  if ((*delimit) & BMO_DELIM_UV) {
    if (!CustomData_has_layer(&bm->ldata, CD_PROP_FLOAT2)) {
      (*delimit) &= ~BMO_DELIM_UV;
    }
  }
}

static void select_linked_delimit_begin(BMesh *bm, int delimit)
{
  DelimitData delimit_data{};

  if (delimit & BMO_DELIM_UV) {
    delimit_data.cd_loop_type = CD_PROP_FLOAT2;
    delimit_data.cd_loop_offset = CustomData_get_offset(&bm->ldata, delimit_data.cd_loop_type);
    if (delimit_data.cd_loop_offset == -1) {
      delimit &= ~BMO_DELIM_UV;
    }
  }

  /* Shouldn't need to allocated BMO flags here (sigh). */
  BM_mesh_elem_toolflags_ensure(bm);

  {
    BMIter iter;
    BMEdge *e;

    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      const bool is_walk_ok = (select_linked_delimit_test(e, delimit, &delimit_data) == false);

      BMO_edge_flag_set(bm, e, BMO_ELE_TAG, is_walk_ok);
    }
  }
}

static void select_linked_delimit_end(BMEditMesh *em)
{
  BMesh *bm = em->bm;

  BM_mesh_elem_toolflags_clear(bm);
}

static wmOperatorStatus edbm_select_linked_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

#ifdef USE_LINKED_SELECT_DEFAULT_HACK
  const int delimit_init = select_linked_delimit_default_from_op(op,
                                                                 scene->toolsettings->selectmode);
#else
  const int delimit_init = RNA_enum_get(op->ptr, "delimit");
#endif

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {

    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    BMIter iter;
    BMWalker walker;

    int delimit = delimit_init;

    select_linked_delimit_validate(bm, &delimit);

    if (delimit) {
      select_linked_delimit_begin(em->bm, delimit);
    }

    if (em->selectmode & SCE_SELECT_VERTEX) {
      BMVert *v;

      BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
        BM_elem_flag_set(v, BM_ELEM_TAG, BM_elem_flag_test(v, BM_ELEM_SELECT));
      }

      /* Exclude all delimited verts. */
      if (delimit) {
        BMEdge *e;
        BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
          if (!BMO_edge_flag_test(bm, e, BMO_ELE_TAG)) {
            /* Check the edge for selected faces,
             * this supports stepping off isolated vertices which would otherwise be ignored. */
            if (BM_edge_is_any_face_flag_test(e, BM_ELEM_SELECT)) {
              BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
              BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
            }
          }
        }
      }

      BMW_init(&walker,
               em->bm,
               delimit ? BMW_LOOP_SHELL_WIRE : BMW_VERT_SHELL,
               BMW_MASK_NOP,
               delimit ? BMO_ELE_TAG : BMW_MASK_NOP,
               BMW_MASK_NOP,
               BMW_FLAG_TEST_HIDDEN,
               BMW_NIL_LAY);

      if (delimit) {
        BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
          if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
            BMElem *ele_walk;
            BMW_ITER (ele_walk, &walker, v) {
              if (ele_walk->head.htype == BM_LOOP) {
                BMVert *v_step = ((BMLoop *)ele_walk)->v;
                BM_vert_select_set(em->bm, v_step, true);
                BM_elem_flag_disable(v_step, BM_ELEM_TAG);
              }
              else {
                BMEdge *e_step = (BMEdge *)ele_walk;
                BLI_assert(ele_walk->head.htype == BM_EDGE);
                BM_edge_select_set(em->bm, e_step, true);
                BM_elem_flag_disable(e_step->v1, BM_ELEM_TAG);
                BM_elem_flag_disable(e_step->v2, BM_ELEM_TAG);
              }
            }
          }
        }
      }
      else {
        BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
          if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
            BMEdge *e_walk;
            BMW_ITER (e_walk, &walker, v) {
              BM_edge_select_set(em->bm, e_walk, true);
              BM_elem_flag_disable(e_walk, BM_ELEM_TAG);
            }
          }
        }
      }

      BMW_end(&walker);

      EDBM_selectmode_flush(em);
    }
    else if (em->selectmode & SCE_SELECT_EDGE) {
      BMEdge *e;

      if (delimit) {
        BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
          /* Check the edge for selected faces,
           * this supports stepping off isolated edges which would otherwise be ignored. */
          BM_elem_flag_set(e,
                           BM_ELEM_TAG,
                           (BM_elem_flag_test(e, BM_ELEM_SELECT) &&
                            (BMO_edge_flag_test(bm, e, BMO_ELE_TAG) ||
                             !BM_edge_is_any_face_flag_test(e, BM_ELEM_SELECT))));
        }
      }
      else {
        BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
          BM_elem_flag_set(e, BM_ELEM_TAG, BM_elem_flag_test(e, BM_ELEM_SELECT));
        }
      }

      BMW_init(&walker,
               em->bm,
               delimit ? BMW_LOOP_SHELL_WIRE : BMW_VERT_SHELL,
               BMW_MASK_NOP,
               delimit ? BMO_ELE_TAG : BMW_MASK_NOP,
               BMW_MASK_NOP,
               BMW_FLAG_TEST_HIDDEN,
               BMW_NIL_LAY);

      if (delimit) {
        BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
            BMElem *ele_walk;
            BMW_ITER (ele_walk, &walker, e) {
              if (ele_walk->head.htype == BM_LOOP) {
                BMLoop *l_step = (BMLoop *)ele_walk;
                BM_edge_select_set(em->bm, l_step->e, true);
                BM_edge_select_set(em->bm, l_step->prev->e, true);
                BM_elem_flag_disable(l_step->e, BM_ELEM_TAG);
              }
              else {
                BMEdge *e_step = (BMEdge *)ele_walk;
                BLI_assert(ele_walk->head.htype == BM_EDGE);
                BM_edge_select_set(em->bm, e_step, true);
                BM_elem_flag_disable(e_step, BM_ELEM_TAG);
              }
            }
          }
        }
      }
      else {
        BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
            BMEdge *e_walk;
            BMW_ITER (e_walk, &walker, e) {
              BM_edge_select_set(em->bm, e_walk, true);
              BM_elem_flag_disable(e_walk, BM_ELEM_TAG);
            }
          }
        }
      }

      BMW_end(&walker);

      EDBM_selectmode_flush(em);
    }
    else {
      BMFace *f;

      BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_set(f, BM_ELEM_TAG, BM_elem_flag_test(f, BM_ELEM_SELECT));
      }

      BMW_init(&walker,
               bm,
               BMW_ISLAND,
               BMW_MASK_NOP,
               delimit ? BMO_ELE_TAG : BMW_MASK_NOP,
               BMW_MASK_NOP,
               BMW_FLAG_TEST_HIDDEN,
               BMW_NIL_LAY);

      BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
          BMFace *f_walk;
          BMW_ITER (f_walk, &walker, f) {
            BM_face_select_set(bm, f_walk, true);
            BM_elem_flag_disable(f_walk, BM_ELEM_TAG);
          }
        }
      }

      BMW_end(&walker);
    }

    if (delimit) {
      select_linked_delimit_end(em);
    }

    EDBM_uvselect_clear(em);

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_linked(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Select Linked All";
  ot->idname = "MESH_OT_select_linked";
  ot->description = "Select all vertices connected to the current selection";

  /* API callbacks. */
  ot->exec = edbm_select_linked_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_enum_flag(ot->srna,
                           "delimit",
                           rna_enum_mesh_delimit_mode_items,
                           BMO_DELIM_SEAM,
                           "Delimit",
                           "Delimit selected region");
#ifdef USE_LINKED_SELECT_DEFAULT_HACK
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
#else
  UNUSED_VARS(prop);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked (Cursor Pick) Operator
 * \{ */

static wmOperatorStatus edbm_select_linked_pick_exec(bContext *C, wmOperator *op);

static void edbm_select_linked_pick_ex(BMEditMesh *em, BMElem *ele, bool sel, int delimit)
{
  BMesh *bm = em->bm;
  BMWalker walker;

  select_linked_delimit_validate(bm, &delimit);

  if (delimit) {
    select_linked_delimit_begin(bm, delimit);
  }

  /* NOTE: logic closely matches #edbm_select_linked_exec, keep in sync. */

  if (ele->head.htype == BM_VERT) {
    BMVert *eve = (BMVert *)ele;

    BMW_init(&walker,
             bm,
             delimit ? BMW_LOOP_SHELL_WIRE : BMW_VERT_SHELL,
             BMW_MASK_NOP,
             delimit ? BMO_ELE_TAG : BMW_MASK_NOP,
             BMW_MASK_NOP,
             BMW_FLAG_TEST_HIDDEN,
             BMW_NIL_LAY);

    if (delimit) {
      BMElem *ele_walk;
      BMW_ITER (ele_walk, &walker, eve) {
        if (ele_walk->head.htype == BM_LOOP) {
          BMVert *v_step = ((BMLoop *)ele_walk)->v;
          BM_vert_select_set(bm, v_step, sel);
        }
        else {
          BMEdge *e_step = (BMEdge *)ele_walk;
          BLI_assert(ele_walk->head.htype == BM_EDGE);
          BM_edge_select_set(bm, e_step, sel);
        }
      }
    }
    else {
      BMEdge *e_walk;
      BMW_ITER (e_walk, &walker, eve) {
        BM_edge_select_set(bm, e_walk, sel);
      }
    }

    BMW_end(&walker);

    EDBM_selectmode_flush(em);
  }
  else if (ele->head.htype == BM_EDGE) {
    BMEdge *eed = (BMEdge *)ele;

    BMW_init(&walker,
             bm,
             delimit ? BMW_LOOP_SHELL_WIRE : BMW_VERT_SHELL,
             BMW_MASK_NOP,
             delimit ? BMO_ELE_TAG : BMW_MASK_NOP,
             BMW_MASK_NOP,
             BMW_FLAG_TEST_HIDDEN,
             BMW_NIL_LAY);

    if (delimit) {
      BMElem *ele_walk;
      BMW_ITER (ele_walk, &walker, eed) {
        if (ele_walk->head.htype == BM_LOOP) {
          BMEdge *e_step = ((BMLoop *)ele_walk)->e;
          BM_edge_select_set(bm, e_step, sel);
        }
        else {
          BMEdge *e_step = (BMEdge *)ele_walk;
          BLI_assert(ele_walk->head.htype == BM_EDGE);
          BM_edge_select_set(bm, e_step, sel);
        }
      }
    }
    else {
      BMEdge *e_walk;
      BMW_ITER (e_walk, &walker, eed) {
        BM_edge_select_set(bm, e_walk, sel);
      }
    }

    BMW_end(&walker);

    EDBM_selectmode_flush(em);
  }
  else if (ele->head.htype == BM_FACE) {
    BMFace *efa = (BMFace *)ele;

    BMW_init(&walker,
             bm,
             BMW_ISLAND,
             BMW_MASK_NOP,
             delimit ? BMO_ELE_TAG : BMW_MASK_NOP,
             BMW_MASK_NOP,
             BMW_FLAG_TEST_HIDDEN,
             BMW_NIL_LAY);

    {
      BMFace *f_walk;
      BMW_ITER (f_walk, &walker, efa) {
        BM_face_select_set(bm, f_walk, sel);
        BM_elem_flag_disable(f_walk, BM_ELEM_TAG);
      }
    }

    BMW_end(&walker);
  }

  EDBM_uvselect_clear(em);

  if (delimit) {
    select_linked_delimit_end(em);
  }
}

static wmOperatorStatus edbm_select_linked_pick_invoke(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *event)
{
  Base *basact = nullptr;
  BMVert *eve;
  BMEdge *eed;
  BMFace *efa;
  const bool sel = !RNA_boolean_get(op->ptr, "deselect");
  int index;

  if (RNA_struct_property_is_set(op->ptr, "index")) {
    return edbm_select_linked_pick_exec(C, op);
  }

  /* #unified_findnearest needs OpenGL. */
  view3d_operator_needs_gpu(C);

  /* Setup view context for argument to callbacks. */
  ViewContext vc = em_setup_viewcontext(C);

  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode(
      vc.scene, vc.view_layer, vc.v3d);

  {
    bool has_edges = false;
    for (Base *base : bases) {
      Object *ob_iter = base->object;
      ED_view3d_viewcontext_init_object(&vc, ob_iter);
      if (vc.em->bm->totedge) {
        has_edges = true;
      }
    }
    if (has_edges == false) {
      return OPERATOR_CANCELLED;
    }
  }

  vc.mval[0] = event->mval[0];
  vc.mval[1] = event->mval[1];

  /* Return warning. */
  {
    int base_index = -1;
    const bool ok = unified_findnearest(&vc, bases, &base_index, &eve, &eed, &efa);
    if (!ok) {
      return OPERATOR_CANCELLED;
    }
    basact = bases[base_index];
  }

  ED_view3d_viewcontext_init_object(&vc, basact->object);
  BMEditMesh *em = vc.em;
  BMesh *bm = em->bm;

#ifdef USE_LINKED_SELECT_DEFAULT_HACK
  int delimit = select_linked_delimit_default_from_op(op, vc.scene->toolsettings->selectmode);
#else
  int delimit = RNA_enum_get(op->ptr, "delimit");
#endif

  BMElem *ele = EDBM_elem_from_selectmode(em, eve, eed, efa);

  edbm_select_linked_pick_ex(em, ele, sel, delimit);

  /* To support redo. */
  {
    /* Note that the `base_index` can't be used as the index depends on the 3D Viewport
     * which might not be available on redo. */
    BM_mesh_elem_index_ensure(bm, ele->head.htype);
    int object_index;
    index = EDBM_elem_to_index_any_multi(vc.scene, vc.view_layer, em, ele, &object_index);
    BLI_assert(object_index >= 0);
    RNA_int_set(op->ptr, "object_index", object_index);
    RNA_int_set(op->ptr, "index", index);
  }

  DEG_id_tag_update(static_cast<ID *>(basact->object->data), ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, basact->object->data);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus edbm_select_linked_pick_exec(bContext *C, wmOperator *op)
{
  Object *obedit = nullptr;
  BMElem *ele;

  {
    const Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    /* Intentionally wrap negative values so the lookup fails. */
    const uint object_index = uint(RNA_int_get(op->ptr, "object_index"));
    const uint index = uint(RNA_int_get(op->ptr, "index"));
    ele = EDBM_elem_from_index_any_multi(scene, view_layer, object_index, index, &obedit);
  }

  if (ele == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const bool sel = !RNA_boolean_get(op->ptr, "deselect");

#ifdef USE_LINKED_SELECT_DEFAULT_HACK
  int delimit = select_linked_delimit_default_from_op(op, em->selectmode);
#else
  int delimit = RNA_enum_get(op->ptr, "delimit");
#endif

  edbm_select_linked_pick_ex(em, ele, sel, delimit);

  DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

  return OPERATOR_FINISHED;
}

void MESH_OT_select_linked_pick(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Select Linked";
  ot->idname = "MESH_OT_select_linked_pick";
  ot->description = "(De)select all vertices linked to the edge under the mouse cursor";

  /* API callbacks. */
  ot->invoke = edbm_select_linked_pick_invoke;
  ot->exec = edbm_select_linked_pick_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "");
  prop = RNA_def_enum_flag(ot->srna,
                           "delimit",
                           rna_enum_mesh_delimit_mode_items,
                           BMO_DELIM_SEAM,
                           "Delimit",
                           "Delimit selected region");
#ifdef USE_LINKED_SELECT_DEFAULT_HACK
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
#endif

  /* Use for redo. */
  prop = RNA_def_int(ot->srna, "object_index", -1, -1, INT_MAX, "", "", 0, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "index", -1, -1, INT_MAX, "", "", 0, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select by Pole Count Operator
 * \{ */

static wmOperatorStatus edbm_select_by_pole_count_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool exclude_nonmanifold = RNA_boolean_get(op->ptr, "exclude_nonmanifold");
  const int pole_count = RNA_int_get(op->ptr, "pole_count");
  const eElemCountType type = eElemCountType(RNA_enum_get(op->ptr, "type"));
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    bool changed = false;

    BMIter iter;
    BMVert *v;

    if (!extend) {
      EDBM_flag_disable_all(em, BM_ELEM_SELECT);
      changed = true;
    }

    BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        continue;
      }

      const int v_edge_count = BM_vert_edge_count_at_most(v, pole_count + 1);
      if (!is_count_a_match(type, v_edge_count, pole_count)) {
        continue;
      }

      if (exclude_nonmanifold) {
        /* Exclude non-manifold vertices (no edges). */
        if (BM_vert_is_manifold(v) == false) {
          continue;
        }

        /* Exclude vertices connected to non-manifold edges. */
        BMIter eiter;
        BMEdge *e;
        bool all_edges_manifold = true;
        BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
          if (BM_edge_is_manifold(e) == false) {
            all_edges_manifold = false;
            break;
          }
        }

        if (all_edges_manifold == false) {
          continue;
        }
      }

      /* All tests passed, perform the selection. */

      /* Multiple selection modes may be active.
       * Select elements per the finest-grained choice. */
      changed = true;

      if (em->selectmode & SCE_SELECT_VERTEX) {
        BM_vert_select_set(em->bm, v, true);
      }
      else if (em->selectmode & SCE_SELECT_EDGE) {
        BMIter eiter;
        BMEdge *e;
        BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
          BM_edge_select_set(em->bm, e, true);
        }
      }
      else if (em->selectmode & SCE_SELECT_FACE) {
        BMIter fiter;
        BMFace *f;
        BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
          BM_face_select_set(em->bm, f, true);
        }
      }
      else {
        BLI_assert_unreachable();
      }
    }

    if (changed) {
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_by_pole_count(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select By Pole Count";
  ot->description =
      "Select vertices at poles by the number of connected edges. "
      "In edge and face mode the geometry connected to the vertices is selected";
  ot->idname = "MESH_OT_select_by_pole_count";

  /* API callbacks. */
  ot->exec = edbm_select_by_pole_count_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_int(ot->srna, "pole_count", 4, 0, INT_MAX, "Pole Count", "", 0, INT_MAX);
  RNA_def_enum(ot->srna,
               "type",
               elem_count_compare_items,
               ELEM_COUNT_NOT_EQUAL,
               "Type",
               "Type of comparison to make");
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
  RNA_def_boolean(
      ot->srna, "exclude_nonmanifold", true, "Exclude Non Manifold", "Exclude non-manifold poles");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Face by Sides Operator
 * \{ */

static wmOperatorStatus edbm_select_face_by_sides_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const int numverts = RNA_int_get(op->ptr, "number");
  const eElemCountType type = eElemCountType(RNA_enum_get(op->ptr, "type"));
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    bool changed = false;

    BMFace *efa;
    BMIter iter;

    if (!extend) {
      EDBM_flag_disable_all(em, BM_ELEM_SELECT);
      changed = true;
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
        continue;
      }

      if (is_count_a_match(type, efa->len, numverts)) {
        changed = true;
        BM_face_select_set(em->bm, efa, true);
      }
    }

    if (changed) {
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_face_by_sides(wmOperatorType *ot)
{

  /* Identifiers. */
  ot->name = "Select Faces by Sides";
  ot->description = "Select vertices or faces by the number of face sides";
  ot->idname = "MESH_OT_select_face_by_sides";

  /* API callbacks. */
  ot->exec = edbm_select_face_by_sides_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_int(ot->srna, "number", 4, 3, INT_MAX, "Number of Vertices", "", 3, INT_MAX);
  RNA_def_enum(ot->srna,
               "type",
               elem_count_compare_items,
               ELEM_COUNT_EQUAL,
               "Type",
               "Type of comparison to make");
  RNA_def_boolean(ot->srna, "extend", true, "Extend", "Extend the selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Loose Operator
 * \{ */

static wmOperatorStatus edbm_select_loose_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    BMIter iter;

    bool changed = false;

    if (!extend) {
      EDBM_flag_disable_all(em, BM_ELEM_SELECT);
      changed = true;
    }

    if (em->selectmode & SCE_SELECT_VERTEX) {
      BMVert *eve;
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
          continue;
        }
        if (!eve->e) {
          BM_vert_select_set(bm, eve, true);
          changed = true;
        }
      }
    }

    if (em->selectmode & SCE_SELECT_EDGE) {
      BMEdge *eed;
      BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
          continue;
        }
        if (BM_edge_is_wire(eed)) {
          BM_edge_select_set(bm, eed, true);
          changed = true;
        }
      }
    }

    if (em->selectmode & SCE_SELECT_FACE) {
      BMFace *efa;
      BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
          continue;
        }
        BMIter liter;
        BMLoop *l;
        bool is_loose = true;
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (!BM_edge_is_boundary(l->e)) {
            is_loose = false;
            break;
          }
        }
        if (is_loose) {
          BM_face_select_set(bm, efa, true);
          changed = true;
        }
      }
    }

    if (changed) {
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_loose(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Loose Geometry";
  ot->description = "Select loose geometry based on the selection mode";
  ot->idname = "MESH_OT_select_loose";

  /* API callbacks. */
  ot->exec = edbm_select_loose_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Props. */
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Mirror Operator
 * \{ */

static wmOperatorStatus edbm_select_mirror_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const int axis_flag = RNA_enum_get(op->ptr, "axis");
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  Object *obedit_active = CTX_data_edit_object(C);
  BMEditMesh *em_active = BKE_editmesh_from_object(obedit_active);
  const int select_mode = em_active->bm->selectmode;
  int tot_mirr = 0, tot_fail = 0;

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totvertsel == 0) {
      continue;
    }

    int tot_mirr_iter = 0, tot_fail_iter = 0;

    for (int axis = 0; axis < 3; axis++) {
      if ((1 << axis) & axis_flag) {
        EDBM_select_mirrored(em,
                             static_cast<const Mesh *>(obedit->data),
                             axis,
                             extend,
                             &tot_mirr_iter,
                             &tot_fail_iter);
      }
    }

    if (tot_mirr_iter) {
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }

    tot_fail += tot_fail_iter;
    tot_mirr += tot_mirr_iter;
  }

  if (tot_mirr || tot_fail) {
    ED_mesh_report_mirror_ex(*op->reports, tot_mirr, tot_fail, select_mode);
  }
  return OPERATOR_FINISHED;
}

void MESH_OT_select_mirror(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Mirror";
  ot->description = "Select mesh items at mirrored locations";
  ot->idname = "MESH_OT_select_mirror";

  /* API callbacks. */
  ot->exec = edbm_select_mirror_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Props. */
  RNA_def_enum_flag(ot->srna, "axis", rna_enum_axis_flag_xyz_items, (1 << 0), "Axis", "");

  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the existing selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More Operator
 * \{ */

static wmOperatorStatus edbm_select_more_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool use_face_step = RNA_boolean_get(op->ptr, "use_face_step");

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;

    if ((bm->totvertsel == 0) && (bm->totedgesel == 0) && (bm->totfacesel == 0)) {
      continue;
    }

    EDBM_select_more(em, use_face_step);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_more(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select More";
  ot->idname = "MESH_OT_select_more";
  ot->description = "Select more vertices, edges or faces connected to initial selection";

  /* API callbacks */
  ot->exec = edbm_select_more_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "use_face_step", true, "Face Step", "Connected faces (instead of edges)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More Operator
 * \{ */

static wmOperatorStatus edbm_select_less_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool use_face_step = RNA_boolean_get(op->ptr, "use_face_step");

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;

    if ((bm->totvertsel == 0) && (bm->totedgesel == 0) && (bm->totfacesel == 0)) {
      continue;
    }

    EDBM_select_less(em, use_face_step);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_less(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Less";
  ot->idname = "MESH_OT_select_less";
  ot->description = "Deselect vertices, edges or faces at the boundary of each selection region";

  /* API callbacks */
  ot->exec = edbm_select_less_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "use_face_step", true, "Face Step", "Connected faces (instead of edges)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select N'th Operator
 * \{ */

/**
 * Check if we're connected to another selected edge.
 */
static bool bm_edge_is_select_isolated(BMEdge *e)
{
  BMIter viter;
  BMVert *v;

  BM_ITER_ELEM (v, &viter, e, BM_VERTS_OF_EDGE) {
    BMIter eiter;
    BMEdge *e_other;

    BM_ITER_ELEM (e_other, &eiter, v, BM_EDGES_OF_VERT) {
      if ((e_other != e) && BM_elem_flag_test(e_other, BM_ELEM_SELECT)) {
        return false;
      }
    }
  }
  return true;
}

static BMEdge *bm_step_over_vert_to_next_selected_edge_in_chain(const BMEdge *e_curr, BMVert *v)
{
  BMIter eiter;
  BMEdge *e_other, *e_next = nullptr;
  int count = 0;
  const int count_expected = 1;

  BM_ITER_ELEM (e_other, &eiter, v, BM_EDGES_OF_VERT) {
    if (e_other == e_curr || !BM_elem_flag_test(e_other, BM_ELEM_SELECT)) {
      continue;
    }
    if (++count > count_expected) {
      return nullptr;
    }
    e_next = e_other;
  }
  return (count == count_expected) ? e_next : nullptr;
}

static BMVert *bm_step_to_next_selected_vert_in_chain(BMVert *v_curr, BMVert *v_prev)
{
  BMIter eiter;
  BMEdge *e;
  BMVert *v_next = nullptr;
  int count = 0;
  const int count_expected = v_prev ? 1 : 2;

  BM_ITER_ELEM (e, &eiter, v_curr, BM_EDGES_OF_VERT) {
    BMVert *v_other = BM_edge_other_vert(e, v_curr);
    if (v_other == v_prev || !BM_elem_flag_test(v_other, BM_ELEM_SELECT)) {
      continue;
    }
    if (++count > count_expected) {
      return nullptr;
    }
    v_next = v_other;
  }
  return (count == count_expected) ? v_next : nullptr;
}

static BMFace *bm_step_over_shared_edge_to_next_selected_face_in_chain(BMFace *f_curr,
                                                                       BMFace *f_prev)
{
  BMIter liter;
  BMLoop *l;
  BMFace *f_next = nullptr;
  int count = 0;
  const int count_expected = f_prev ? 1 : 2;

  BM_ITER_ELEM (l, &liter, f_curr, BM_LOOPS_OF_FACE) {
    BMIter fiter;
    BMFace *f_other;
    BM_ITER_ELEM (f_other, &fiter, l->e, BM_FACES_OF_EDGE) {
      if (ELEM(f_other, f_curr, f_prev) || !BM_elem_flag_test(f_other, BM_ELEM_SELECT)) {
        continue;
      }
      if (++count > count_expected) {
        return nullptr;
      }
      f_next = f_other;
    }
  }
  return (count == count_expected) ? f_next : nullptr;
}

/**
 * Check if the selected vertices form a loop cyclic chain.
 */
static bool bm_verts_form_cyclic_chain(BMVert *v_start)
{
  BMVert *v_prev = nullptr, *v_curr = v_start;

  do {
    int selected_neighbor_count = 0;
    BMIter eiter;
    BMEdge *e;
    BM_ITER_ELEM (e, &eiter, v_curr, BM_EDGES_OF_VERT) {
      BMVert *v_other = BM_edge_other_vert(e, v_curr);
      if (BM_elem_flag_test(v_other, BM_ELEM_SELECT)) {
        if (++selected_neighbor_count > 2) {
          return false;
        }
      }
    }
    if (selected_neighbor_count != 2) {
      return false;
    }

    BMVert *v_next = bm_step_to_next_selected_vert_in_chain(v_curr, v_prev);
    if (v_next == nullptr) {
      return false;
    }
    v_prev = v_curr;
    v_curr = v_next;
  } while (v_curr != v_start);

  return true;
}

/**
 * Check if the selected edges form a loop cyclic chain.
 */
static bool bm_edges_form_cyclic_chain(BMEdge *e_start)
{
  BMEdge *e_curr = e_start;
  BMVert *v_through = e_start->v1;

  do {
    BMEdge *e_next = bm_step_over_vert_to_next_selected_edge_in_chain(e_curr, v_through);
    if (e_next == nullptr) {
      return false;
    }
    v_through = BM_edge_other_vert(e_next, v_through);
    e_curr = e_next;

  } while (e_curr != e_start);

  return true;
}

/**
 * Check if the selected faces form a loop cyclic chain.
 */
static bool bm_faces_form_cyclic_chain(BMFace *f_start)
{
  BMFace *f_prev = nullptr;
  BMFace *f_curr = f_start;

  do {
    int selected_neighbor_count = 0;
    BMIter liter;
    BMLoop *l;

    BM_ITER_ELEM (l, &liter, f_curr, BM_LOOPS_OF_FACE) {
      BMIter fiter;
      BMFace *f_other;
      BM_ITER_ELEM (f_other, &fiter, l->e, BM_FACES_OF_EDGE) {
        if (f_other != f_curr && BM_elem_flag_test(f_other, BM_ELEM_SELECT)) {
          selected_neighbor_count++;
        }
      }
    }
    if (selected_neighbor_count != 2) {
      return false;
    }

    BMFace *f_next = bm_step_over_shared_edge_to_next_selected_face_in_chain(f_curr, f_prev);
    if (f_next == nullptr) {
      return false;
    }

    f_prev = f_curr;
    f_curr = f_next;
  } while (f_curr != f_start);

  return true;
}

static void walker_deselect_nth_vertex_chain(BMEditMesh *em,
                                             const CheckerIntervalParams *op_params,
                                             BMVert *v_start)
{
  BMesh *bm = em->bm;
  BMVert *v_prev = nullptr;
  BMVert *v_curr = v_start;
  int index = 0;

  /* Mark all vertices as unvisited. */
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);

  while (v_curr && !BM_elem_flag_test(v_curr, BM_ELEM_TAG)) {
    /* Mark as visited. */
    BM_elem_flag_enable(v_curr, BM_ELEM_TAG);

    /* Apply checker pattern based on position in loop. */
    if (!WM_operator_properties_checker_interval_test(op_params, index)) {
      BM_elem_select_set(bm, (BMElem *)v_curr, false);
    }

    /* Find next vertex in the loop. */
    BMVert *v_next = bm_step_to_next_selected_vert_in_chain(v_curr, v_prev);
    if (ELEM(v_next, nullptr, v_start)) {
      break;
    }

    v_prev = v_curr;
    v_curr = v_next;
    index++;
  }
}

static void walker_deselect_nth_edge_chain(BMEditMesh *em,
                                           const CheckerIntervalParams *op_params,
                                           BMEdge *e_start)
{
  BMesh *bm = em->bm;
  BMEdge *e_curr = e_start;
  BMVert *v_through = e_start->v1;
  int index = 0;

  /* Mark all edges as unvisited. */
  BM_mesh_elem_hflag_disable_all(bm, BM_EDGE, BM_ELEM_TAG, false);

  while (e_curr && !BM_elem_flag_test(e_curr, BM_ELEM_TAG)) {
    /* Mark as visited. */
    BM_elem_flag_enable(e_curr, BM_ELEM_TAG);

    /* Apply checker pattern based on position in loop. */
    if (!WM_operator_properties_checker_interval_test(op_params, index)) {
      BM_elem_select_set(bm, (BMElem *)e_curr, false);
    }

    /* Find next edge in the loop. */
    BMEdge *e_next = bm_step_over_vert_to_next_selected_edge_in_chain(e_curr, v_through);
    if (ELEM(e_next, nullptr, e_start)) {
      break;
    }

    v_through = BM_edge_other_vert(e_next, v_through);
    e_curr = e_next;
    index++;
  }
}

static void walker_deselect_nth_face_chain(BMEditMesh *em,
                                           const CheckerIntervalParams *op_params,
                                           BMFace *f_start)
{
  BMesh *bm = em->bm;
  BMFace *f_prev = nullptr;
  BMFace *f_curr = f_start;
  int index = 0;

  /* Mark all faces as unvisited. */
  BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);

  while (f_curr && !BM_elem_flag_test(f_curr, BM_ELEM_TAG)) {
    BM_elem_flag_enable(f_curr, BM_ELEM_TAG);

    BMFace *f_next = bm_step_over_shared_edge_to_next_selected_face_in_chain(f_curr, f_prev);

    /* Apply checker pattern to current face. */
    if (!WM_operator_properties_checker_interval_test(op_params, index)) {
      BM_elem_select_set(bm, (BMElem *)f_curr, false);
    }

    if (ELEM(f_next, nullptr, f_start)) {
      break;
    }

    f_prev = f_curr;
    f_curr = f_next;
    index++;
  }
}

/* Walk all reachable elements of the same type as h_act in breadth-first
 * order, starting from h_act. Deselects elements if the depth when they
 * are reached is not a multiple of "nth". */
static void walker_deselect_nth(BMEditMesh *em,
                                const CheckerIntervalParams *op_params,
                                BMHeader *h_act)
{
  BMElem *ele;
  BMesh *bm = em->bm;
  BMWalker walker;
  BMIter iter;
  int walktype = 0, itertype = 0, flushtype = 0;
  short mask_vert = 0, mask_edge = 0, mask_face = 0;

  /* No active element from which to start - nothing to do. */
  if (h_act == nullptr) {
    return;
  }

  /* Note on cyclic-chain handling here:
   *
   * The use of a breadth first search to determine element order
   * causes problems with cyclic topology.
   *
   * The walker ordered vertices by their graph depth from the active element.
   * This approach was failing on loops like a circle because the breadth first
   * search expanded in two directions simultaneously, creating a symmetrical
   * but non sequential depth map, see: #126909. */
  if (h_act->htype == BM_VERT) {
    BMVert *v_start = (BMVert *)h_act;
    if (bm_verts_form_cyclic_chain(v_start)) {
      walker_deselect_nth_vertex_chain(em, op_params, v_start);
      EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);
      return;
    }
  }
  else if (h_act->htype == BM_EDGE) {
    BMEdge *e_start = (BMEdge *)h_act;
    if (bm_edges_form_cyclic_chain(e_start)) {
      walker_deselect_nth_edge_chain(em, op_params, e_start);
      EDBM_selectmode_flush_ex(em, SCE_SELECT_EDGE);
      return;
    }
  }
  else if (h_act->htype == BM_FACE) {
    BMFace *f_start = (BMFace *)h_act;
    if (bm_faces_form_cyclic_chain(f_start)) {
      walker_deselect_nth_face_chain(em, op_params, f_start);
      EDBM_selectmode_flush_ex(em, SCE_SELECT_FACE);
      return;
    }
  }

  /* Determine which type of iterator, walker, and select flush to use
   * based on type of the elements being deselected. */
  switch (h_act->htype) {
    case BM_VERT:
      itertype = BM_VERTS_OF_MESH;
      walktype = BMW_CONNECTED_VERTEX;
      flushtype = SCE_SELECT_VERTEX;
      mask_vert = BMO_ELE_TAG;
      break;
    case BM_EDGE:
      /* When an edge has no connected-selected edges,
       * use face-stepping (supports edge-rings). */
      itertype = BM_EDGES_OF_MESH;
      walktype = bm_edge_is_select_isolated((BMEdge *)h_act) ? BMW_FACE_SHELL : BMW_VERT_SHELL;
      flushtype = SCE_SELECT_EDGE;
      mask_edge = BMO_ELE_TAG;
      break;
    case BM_FACE:
      itertype = BM_FACES_OF_MESH;
      walktype = BMW_ISLAND;
      flushtype = SCE_SELECT_FACE;
      mask_face = BMO_ELE_TAG;
      break;
  }

  /* Shouldn't need to allocate BMO flags here (sigh). */
  BM_mesh_elem_toolflags_ensure(bm);

  /* Walker restrictions uses BMO flags, not header flags,
   * so transfer BM_ELEM_SELECT from HFlags onto a BMO flag layer. */
  BMO_push(bm, nullptr);
  BM_ITER_MESH (ele, &iter, bm, itertype) {
    if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
      BMO_elem_flag_enable(bm, (BMElemF *)ele, BMO_ELE_TAG);
    }
  }

  /* Walk over selected elements starting at active. */
  BMW_init(&walker,
           bm,
           walktype,
           mask_vert,
           mask_edge,
           mask_face,
           BMW_FLAG_NOP, /* Don't use #BMW_FLAG_TEST_HIDDEN here since we want to deselect all. */
           BMW_NIL_LAY);

  /* Use tag to avoid touching the same elems twice. */
  BM_ITER_MESH (ele, &iter, bm, itertype) {
    BM_elem_flag_disable(ele, BM_ELEM_TAG);
  }

  BLI_assert(walker.order == BMW_BREADTH_FIRST);
  for (ele = static_cast<BMElem *>(BMW_begin(&walker, h_act)); ele != nullptr;
       ele = static_cast<BMElem *>(BMW_step(&walker)))
  {
    if (!BM_elem_flag_test(ele, BM_ELEM_TAG)) {
      /* Deselect elements that aren't at "nth" depth from active. */
      const int depth = BMW_current_depth(&walker) - 1;
      if (!WM_operator_properties_checker_interval_test(op_params, depth)) {
        BM_elem_select_set(bm, ele, false);
      }
      BM_elem_flag_enable(ele, BM_ELEM_TAG);
    }
  }
  BMW_end(&walker);

  BMO_pop(bm);

  /* Flush selection up. */
  EDBM_selectmode_flush_ex(em, flushtype);
}

static void deselect_nth_active(BMEditMesh *em, BMVert **r_eve, BMEdge **r_eed, BMFace **r_efa)
{
  BMIter iter;
  BMElem *ele;

  *r_eve = nullptr;
  *r_eed = nullptr;
  *r_efa = nullptr;

  EDBM_selectmode_flush(em);
  ele = BM_mesh_active_elem_get(em->bm);

  if (ele && BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
    switch (ele->head.htype) {
      case BM_VERT:
        *r_eve = (BMVert *)ele;
        return;
      case BM_EDGE:
        *r_eed = (BMEdge *)ele;
        return;
      case BM_FACE:
        *r_efa = (BMFace *)ele;
        return;
    }
  }

  if (em->selectmode & SCE_SELECT_VERTEX) {
    BMVert *v;
    BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
        *r_eve = v;
        return;
      }
    }
  }
  else if (em->selectmode & SCE_SELECT_EDGE) {
    BMEdge *e;
    BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
        *r_eed = e;
        return;
      }
    }
  }
  else if (em->selectmode & SCE_SELECT_FACE) {
    BMFace *f = BM_mesh_active_face_get(em->bm, true, false);
    if (f && BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      *r_efa = f;
      return;
    }
  }
}

static bool edbm_deselect_nth(BMEditMesh *em, const CheckerIntervalParams *op_params)
{
  BMVert *v;
  BMEdge *e;
  BMFace *f;

  deselect_nth_active(em, &v, &e, &f);

  if (v) {
    walker_deselect_nth(em, op_params, &v->head);
    return true;
  }
  if (e) {
    walker_deselect_nth(em, op_params, &e->head);
    return true;
  }
  if (f) {
    walker_deselect_nth(em, op_params, &f->head);
    return true;
  }

  return false;
}

static wmOperatorStatus edbm_select_nth_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  CheckerIntervalParams op_params;
  WM_operator_properties_checker_interval_from_op(op, &op_params);
  bool found_active_elt = false;

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if ((em->bm->totvertsel == 0) && (em->bm->totedgesel == 0) && (em->bm->totfacesel == 0)) {
      continue;
    }

    if (edbm_deselect_nth(em, &op_params) == true) {
      EDBM_uvselect_clear(em);

      found_active_elt = true;
      EDBMUpdate_Params params{};
      params.calc_looptris = false;
      params.calc_normals = false;
      params.is_destructive = false;
      EDBM_update(static_cast<Mesh *>(obedit->data), &params);
    }
  }

  if (!found_active_elt) {
    BKE_report(op->reports, RPT_ERROR, "Mesh object(s) have no active vertex/edge/face");
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_nth(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Checker Deselect";
  ot->idname = "MESH_OT_select_nth";
  ot->description = "Deselect every Nth element starting from the active vertex, edge or face";

  /* API callbacks. */
  ot->exec = edbm_select_nth_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_checker_interval(ot, false);
}

ViewContext em_setup_viewcontext(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  if (vc.obedit) {
    vc.em = BKE_editmesh_from_object(vc.obedit);
  }
  return vc;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Sharp Edges Operator
 * \{ */

static wmOperatorStatus edbm_select_sharp_edges_exec(bContext *C, wmOperator *op)
{
  /* Find edges that have exactly two neighboring faces,
   * check the angle between those faces, and if angle is
   * small enough, select the edge. */
  const float angle_limit_cos = cosf(RNA_float_get(op->ptr, "sharpness"));

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMIter iter;
    BMEdge *e;

    BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
        continue;
      }

      BMLoop *l_a, *l_b;
      if (BM_edge_loop_pair(e, &l_a, &l_b)) {
        /* Edge has exactly two neighboring faces, check angle. */
        const float angle_cos = dot_v3v3(l_a->f->no, l_b->f->no);

        if (angle_cos < angle_limit_cos) {
          BM_edge_select_set(em->bm, e, true);
        }
      }
    }

    if ((em->bm->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) == 0) {
      /* Since we can't select individual edges, select faces connected to them. */
      EDBM_selectmode_convert(em, SCE_SELECT_EDGE, SCE_SELECT_FACE);
    }
    else {
      EDBM_selectmode_flush(em);
    }
    EDBM_uvselect_clear(em);

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_edges_select_sharp(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Select Sharp Edges";
  ot->description = "Select all sharp enough edges";
  ot->idname = "MESH_OT_edges_select_sharp";

  /* API callbacks. */
  ot->exec = edbm_select_sharp_edges_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Props. */
  prop = RNA_def_float_rotation(ot->srna,
                                "sharpness",
                                0,
                                nullptr,
                                DEG2RADF(0.01f),
                                DEG2RADF(180.0f),
                                "Sharpness",
                                "",
                                DEG2RADF(1.0f),
                                DEG2RADF(180.0f));
  RNA_def_property_float_default(prop, DEG2RADF(30.0f));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Flat Faces Operator
 * \{ */

static wmOperatorStatus edbm_select_linked_flat_faces_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  const float angle_limit_cos = cosf(RNA_float_get(op->ptr, "sharpness"));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;

    if (bm->totfacesel == 0) {
      continue;
    }

    blender::Vector<BMFace *> stack;

    BMIter iter, liter, liter2;
    BMFace *f;
    BMLoop *l, *l2;

    BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);

    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if ((BM_elem_flag_test(f, BM_ELEM_HIDDEN) != 0) ||
          (BM_elem_flag_test(f, BM_ELEM_TAG) != 0) || (BM_elem_flag_test(f, BM_ELEM_SELECT) == 0))
      {
        continue;
      }

      BLI_assert(stack.is_empty());

      do {
        BM_face_select_set(bm, f, true);

        BM_elem_flag_enable(f, BM_ELEM_TAG);

        BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
          BM_ITER_ELEM (l2, &liter2, l, BM_LOOPS_OF_LOOP) {
            float angle_cos;

            if (BM_elem_flag_test(l2->f, BM_ELEM_TAG) || BM_elem_flag_test(l2->f, BM_ELEM_HIDDEN))
            {
              continue;
            }

            angle_cos = dot_v3v3(f->no, l2->f->no);

            if (angle_cos > angle_limit_cos) {
              stack.append(l2->f);
            }
          }
        }
      } while (!stack.is_empty() && (f = stack.pop_last()));
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_faces_select_linked_flat(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Select Linked Flat Faces";
  ot->description = "Select linked faces by angle";
  ot->idname = "MESH_OT_faces_select_linked_flat";

  /* API callbacks. */
  ot->exec = edbm_select_linked_flat_faces_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Props. */
  prop = RNA_def_float_rotation(ot->srna,
                                "sharpness",
                                0,
                                nullptr,
                                DEG2RADF(0.01f),
                                DEG2RADF(180.0f),
                                "Sharpness",
                                "",
                                DEG2RADF(1.0f),
                                DEG2RADF(180.0f));
  RNA_def_property_float_default(prop, DEG2RADF(1.0f));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Non-Manifold Operator
 * \{ */

static wmOperatorStatus edbm_select_non_manifold_exec(bContext *C, wmOperator *op)
{
  const bool use_extend = RNA_boolean_get(op->ptr, "extend");
  const bool use_wire = RNA_boolean_get(op->ptr, "use_wire");
  const bool use_boundary = RNA_boolean_get(op->ptr, "use_boundary");
  const bool use_multi_face = RNA_boolean_get(op->ptr, "use_multi_face");
  const bool use_non_contiguous = RNA_boolean_get(op->ptr, "use_non_contiguous");
  const bool use_verts = RNA_boolean_get(op->ptr, "use_verts");

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  edbm_selectmode_sync_multi_ex(objects);

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMVert *v;
    BMEdge *e;
    BMIter iter;

    bool changed = false;

    if (!use_extend) {
      EDBM_flag_disable_all(em, BM_ELEM_SELECT);
      changed = true;
    }

    /* Selects isolated verts, and edges that do not have 2 neighboring faces. */
    if (use_verts) {
      BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
          continue;
        }

        if (!BM_vert_is_manifold(v)) {
          BM_vert_select_set(em->bm, v, true);
          changed = true;
        }
      }
    }

    if (use_wire || use_boundary || use_multi_face || use_non_contiguous) {
      BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
          continue;
        }
        if ((use_wire && BM_edge_is_wire(e)) || (use_boundary && BM_edge_is_boundary(e)) ||
            (use_non_contiguous && (BM_edge_is_manifold(e) && !BM_edge_is_contiguous(e))) ||
            (use_multi_face && BM_edge_face_count_is_over(e, 2)))
        {
          /* Check we never select perfect edge (in test above). */
          BLI_assert(!(BM_edge_is_manifold(e) && BM_edge_is_contiguous(e)));

          BM_edge_select_set(em->bm, e, true);
          changed = true;
        }
      }
    }

    if (changed) {
      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);
    }
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_non_manifold(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Non-Manifold";
  ot->description = "Select all non-manifold vertices or edges";
  ot->idname = "MESH_OT_select_non_manifold";

  /* API callbacks */
  ot->exec = edbm_select_non_manifold_exec;
  ot->poll = edbm_vert_or_edge_select_mode_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Props. */
  RNA_def_boolean(ot->srna, "extend", true, "Extend", "Extend the selection");
  /* Edges. */
  RNA_def_boolean(ot->srna, "use_wire", true, "Wire", "Wire edges");
  RNA_def_boolean(ot->srna, "use_boundary", true, "Boundaries", "Boundary edges");
  RNA_def_boolean(
      ot->srna, "use_multi_face", true, "Multiple Faces", "Edges shared by more than two faces");
  RNA_def_boolean(ot->srna,
                  "use_non_contiguous",
                  true,
                  "Non Contiguous",
                  "Edges between faces pointing in alternate directions");
  /* Verts. */
  RNA_def_boolean(
      ot->srna, "use_verts", true, "Vertices", "Vertices connecting multiple face regions");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Random Operator
 * \{ */

static wmOperatorStatus edbm_select_random_exec(bContext *C, wmOperator *op)
{
  const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);
  const float randfac = RNA_float_get(op->ptr, "ratio");
  const int seed = WM_operator_properties_select_random_seed_increment_get(op);

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (const int ob_index : objects.index_range()) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMIter iter;
    int seed_iter = seed;

    /* This gives a consistent result regardless of object order. */
    if (ob_index) {
      seed_iter += BLI_ghashutil_strhash_p(obedit->id.name);
    }

    if (em->selectmode & SCE_SELECT_VERTEX) {
      int elem_map_len = 0;
      BMVert **elem_map = static_cast<BMVert **>(
          MEM_mallocN(sizeof(*elem_map) * em->bm->totvert, __func__));
      BMVert *eve;
      BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
        if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
          elem_map[elem_map_len++] = eve;
        }
      }

      BLI_array_randomize(elem_map, sizeof(*elem_map), elem_map_len, seed_iter);
      const int count_select = elem_map_len * randfac;
      for (int i = 0; i < count_select; i++) {
        BM_vert_select_set(em->bm, elem_map[i], select);
      }
      MEM_freeN(elem_map);
    }
    else if (em->selectmode & SCE_SELECT_EDGE) {
      int elem_map_len = 0;
      BMEdge **elem_map = static_cast<BMEdge **>(
          MEM_mallocN(sizeof(*elem_map) * em->bm->totedge, __func__));
      BMEdge *eed;
      BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
        if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
          elem_map[elem_map_len++] = eed;
        }
      }
      BLI_array_randomize(elem_map, sizeof(*elem_map), elem_map_len, seed_iter);
      const int count_select = elem_map_len * randfac;
      for (int i = 0; i < count_select; i++) {
        BM_edge_select_set(em->bm, elem_map[i], select);
      }
      MEM_freeN(elem_map);
    }
    else {
      int elem_map_len = 0;
      BMFace **elem_map = static_cast<BMFace **>(
          MEM_mallocN(sizeof(*elem_map) * em->bm->totface, __func__));
      BMFace *efa;
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
          elem_map[elem_map_len++] = efa;
        }
      }
      BLI_array_randomize(elem_map, sizeof(*elem_map), elem_map_len, seed_iter);
      const int count_select = elem_map_len * randfac;
      for (int i = 0; i < count_select; i++) {
        BM_face_select_set(em->bm, elem_map[i], select);
      }
      MEM_freeN(elem_map);
    }

    if (select) {
      /* Was #EDBM_select_flush_from_verts, but it over selects in edge/face mode. */
      EDBM_selectmode_flush(em);
    }
    else {
      EDBM_select_flush_from_verts(em, false);
    }
    EDBM_uvselect_clear(em);

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_random(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Random";
  ot->description = "Randomly select vertices";
  ot->idname = "MESH_OT_select_random";

  /* API callbacks */
  ot->exec = edbm_select_random_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Props. */
  WM_operator_properties_select_random(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Ungrouped Operator
 * \{ */

static bool edbm_select_ungrouped_poll(bContext *C)
{
  if (ED_operator_editmesh(C)) {
    Object *obedit = CTX_data_edit_object(C);
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

    const ListBase *defbase = BKE_object_defgroup_list(obedit);
    if ((em->selectmode & SCE_SELECT_VERTEX) == 0) {
      CTX_wm_operator_poll_msg_set(C, "Must be in vertex selection mode");
    }
    else if (BLI_listbase_is_empty(defbase) || cd_dvert_offset == -1) {
      CTX_wm_operator_poll_msg_set(C, "No weights/vertex groups on object");
    }
    else {
      return true;
    }
  }
  return false;
}

static wmOperatorStatus edbm_select_ungrouped_exec(bContext *C, wmOperator *op)
{
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

    if (cd_dvert_offset == -1) {
      continue;
    }

    BMVert *eve;
    BMIter iter;

    bool changed = false;

    if (!extend) {
      if (em->bm->totvertsel) {
        EDBM_flag_disable_all(em, BM_ELEM_SELECT);
        changed = true;
      }
    }

    BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
      if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
        MDeformVert *dv = static_cast<MDeformVert *>(BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));
        /* Skip `dv` or `dv` set with zero weight. */
        if (ELEM(nullptr, dv, dv->dw)) {
          BM_vert_select_set(em->bm, eve, true);
          changed = true;
        }
      }
    }

    if (changed) {
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }
  return OPERATOR_FINISHED;
}

void MESH_OT_select_ungrouped(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Ungrouped";
  ot->idname = "MESH_OT_select_ungrouped";
  ot->description = "Select vertices without a group";

  /* API callbacks. */
  ot->exec = edbm_select_ungrouped_exec;
  ot->poll = edbm_select_ungrouped_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Axis Operator
 * \{ */

enum {
  SELECT_AXIS_POS = 0,
  SELECT_AXIS_NEG = 1,
  SELECT_AXIS_ALIGN = 2,
};

static wmOperatorStatus edbm_select_axis_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obedit = CTX_data_edit_object(C);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMVert *v_act = BM_mesh_active_vert_get(em->bm);
  const int orientation = RNA_enum_get(op->ptr, "orientation");
  const int axis = RNA_enum_get(op->ptr, "axis");
  const int sign = RNA_enum_get(op->ptr, "sign");

  if (v_act == nullptr) {
    BKE_report(
        op->reports, RPT_WARNING, "This operator requires an active vertex (last selected)");
    return OPERATOR_CANCELLED;
  }

  const float limit = RNA_float_get(op->ptr, "threshold");

  float value;
  float axis_mat[3][3];

  /* 3D view variables may be nullptr, (no need to check in poll function). */
  blender::ed::transform::calc_orientation_from_type_ex(scene,
                                                        view_layer,
                                                        CTX_wm_view3d(C),
                                                        CTX_wm_region_view3d(C),
                                                        obedit,
                                                        obedit,
                                                        orientation,
                                                        V3D_AROUND_ACTIVE,
                                                        axis_mat);

  const float *axis_vector = axis_mat[axis];

  {
    float vertex_world[3];
    mul_v3_m4v3(vertex_world, obedit->object_to_world().ptr(), v_act->co);
    value = dot_v3v3(axis_vector, vertex_world);
  }

  if (sign == SELECT_AXIS_NEG) {
    value += limit;
  }
  else if (sign == SELECT_AXIS_POS) {
    value -= limit;
  }

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit_iter : objects) {
    BMEditMesh *em_iter = BKE_editmesh_from_object(obedit_iter);
    BMesh *bm = em_iter->bm;

    if (bm->totvert == bm->totvertsel) {
      continue;
    }

    BMIter iter;
    BMVert *v;
    bool changed = false;

    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN | BM_ELEM_SELECT)) {
        float v_iter_world[3];
        mul_v3_m4v3(v_iter_world, obedit_iter->object_to_world().ptr(), v->co);
        const float value_iter = dot_v3v3(axis_vector, v_iter_world);
        switch (sign) {
          case SELECT_AXIS_ALIGN:
            if (fabsf(value_iter - value) < limit) {
              BM_vert_select_set(bm, v, true);
              changed = true;
            }
            break;
          case SELECT_AXIS_NEG:
            if (value_iter < value) {
              BM_vert_select_set(bm, v, true);
              changed = true;
            }
            break;
          case SELECT_AXIS_POS:
            if (value_iter > value) {
              BM_vert_select_set(bm, v, true);
              changed = true;
            }
            break;
        }
      }
    }
    if (changed) {
      EDBM_selectmode_flush(em_iter);
      EDBM_uvselect_clear(em);

      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit_iter->data);
      DEG_id_tag_update(static_cast<ID *>(obedit_iter->data), ID_RECALC_SELECT);
    }
  }
  return OPERATOR_FINISHED;
}

void MESH_OT_select_axis(wmOperatorType *ot)
{
  static const EnumPropertyItem axis_sign_items[] = {
      {SELECT_AXIS_POS, "POS", false, "Positive Axis", ""},
      {SELECT_AXIS_NEG, "NEG", false, "Negative Axis", ""},
      {SELECT_AXIS_ALIGN, "ALIGN", false, "Aligned Axis", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Identifiers. */
  ot->name = "Select Axis";
  ot->description = "Select all data in the mesh on a single axis";
  ot->idname = "MESH_OT_select_axis";

  /* API callbacks. */
  ot->exec = edbm_select_axis_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_enum(ot->srna,
               "orientation",
               rna_enum_transform_orientation_items,
               V3D_ORIENT_LOCAL,
               "Axis Mode",
               "Axis orientation");
  RNA_def_enum(ot->srna, "sign", axis_sign_items, SELECT_AXIS_POS, "Axis Sign", "Side to select");
  RNA_def_enum(ot->srna,
               "axis",
               rna_enum_axis_xyz_items,
               0,
               "Axis",
               "Select the axis to compare each vertex on");
  RNA_def_float(
      ot->srna, "threshold", 0.0001f, 0.000001f, 50.0f, "Threshold", "", 0.00001f, 10.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Region to Loop Operator
 * \{ */

static wmOperatorStatus edbm_region_to_loop_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  bool changed = false;
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totfacesel == 0) {
      continue;
    }
    BMFace *f;
    BMEdge *e;
    BMIter iter;

    BM_mesh_elem_hflag_disable_all(em->bm, BM_EDGE, BM_ELEM_TAG, false);

    BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l1, *l2;
      BMIter liter1, liter2;

      BM_ITER_ELEM (l1, &liter1, f, BM_LOOPS_OF_FACE) {
        int tot = 0, totsel = 0;

        BM_ITER_ELEM (l2, &liter2, l1->e, BM_LOOPS_OF_EDGE) {
          tot++;
          totsel += BM_elem_flag_test(l2->f, BM_ELEM_SELECT) != 0;
        }

        if ((tot != totsel && totsel > 0) || (totsel == 1 && tot == 1)) {
          BM_elem_flag_enable(l1->e, BM_ELEM_TAG);
        }
      }
    }

    EDBM_flag_disable_all(em, BM_ELEM_SELECT);

    BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
        BM_edge_select_set(em->bm, e, true);
        changed = true;
      }
    }

    DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  if (changed) {
    /* If in face-only select mode, switch to edge select mode so that
     * an edge-only selection is not inconsistent state. Do this for all meshes in multi-object
     * editmode so their selectmode is in sync for following operators. */
    EDBM_selectmode_disable_multi(C, SCE_SELECT_FACE, SCE_SELECT_EDGE);
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_region_to_loop(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Boundary Loop";
  ot->idname = "MESH_OT_region_to_loop";
  ot->description = "Select boundary edges around the selected faces";

  /* API callbacks. */
  ot->exec = edbm_region_to_loop_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Loop to Region Operator
 * \{ */

static int loop_find_region(BMLoop *l, int flag, GSet *visit_face_set, BMFace ***region_out)
{
  blender::Vector<BMFace *> stack;
  blender::Vector<BMFace *> region;

  stack.append(l->f);
  BLI_gset_insert(visit_face_set, l->f);

  while (!stack.is_empty()) {
    BMIter liter1, liter2;
    BMLoop *l1, *l2;

    BMFace *f = stack.pop_last();
    region.append(f);

    BM_ITER_ELEM (l1, &liter1, f, BM_LOOPS_OF_FACE) {
      if (BM_elem_flag_test(l1->e, flag)) {
        continue;
      }

      BM_ITER_ELEM (l2, &liter2, l1->e, BM_LOOPS_OF_EDGE) {
        /* Avoids finding same region twice
         * (otherwise) the logic works fine without. */
        if (BM_elem_flag_test(l2->f, BM_ELEM_TAG)) {
          continue;
        }

        if (BLI_gset_add(visit_face_set, l2->f)) {
          stack.append(l2->f);
        }
      }
    }
  }

  BMFace **region_alloc = MEM_malloc_arrayN<BMFace *>(region.size(), __func__);
  memcpy(region_alloc, region.data(), region.as_span().size_in_bytes());
  *region_out = region_alloc;
  return region.size();
}

static int verg_radial(const void *va, const void *vb)
{
  const BMEdge *e_a = *((const BMEdge **)va);
  const BMEdge *e_b = *((const BMEdge **)vb);

  const int a = BM_edge_face_count(e_a);
  const int b = BM_edge_face_count(e_b);

  if (a > b) {
    return -1;
  }
  if (a < b) {
    return 1;
  }
  return 0;
}

/**
 * This function leaves faces tagged which are a part of the new region.
 *
 * \note faces already tagged are ignored, to avoid finding the same regions twice:
 * important when we have regions with equal face counts, see: #40309
 */
static int loop_find_regions(BMEditMesh *em, const bool selbigger)
{
  GSet *visit_face_set;
  BMIter iter;
  const int edges_len = em->bm->totedgesel;
  BMEdge *e;
  int count = 0, i;

  visit_face_set = BLI_gset_ptr_new_ex(__func__, edges_len);
  BMEdge **edges = MEM_malloc_arrayN<BMEdge *>(edges_len, __func__);

  i = 0;
  BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
      edges[i++] = e;
      BM_elem_flag_enable(e, BM_ELEM_TAG);
    }
    else {
      BM_elem_flag_disable(e, BM_ELEM_TAG);
    }
  }

  /* Sort edges by radial cycle length. */
  qsort(edges, edges_len, sizeof(*edges), verg_radial);

  for (i = 0; i < edges_len; i++) {
    BMIter liter;
    BMLoop *l;
    BMFace **region = nullptr, **region_out;
    int c, tot = 0;

    e = edges[i];

    if (!BM_elem_flag_test(e, BM_ELEM_TAG)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
      if (BLI_gset_haskey(visit_face_set, l->f)) {
        continue;
      }

      c = loop_find_region(l, BM_ELEM_SELECT, visit_face_set, &region_out);

      if (!region || (selbigger ? c >= tot : c < tot)) {
        /* This region is the best seen so far. */
        tot = c;
        if (region) {
          /* Free the previous best. */
          MEM_freeN(region);
        }
        /* Track the current region as the new best. */
        region = region_out;
      }
      else {
        /* This region is not as good as best so far, just free it. */
        MEM_freeN(region_out);
      }
    }

    if (region) {
      int j;

      for (j = 0; j < tot; j++) {
        BM_elem_flag_enable(region[j], BM_ELEM_TAG);
        BM_ITER_ELEM (l, &liter, region[j], BM_LOOPS_OF_FACE) {
          BM_elem_flag_disable(l->e, BM_ELEM_TAG);
        }
      }

      count += tot;

      MEM_freeN(region);
    }
  }

  MEM_freeN(edges);
  BLI_gset_free(visit_face_set, nullptr);

  return count;
}

static wmOperatorStatus edbm_loop_to_region_exec(bContext *C, wmOperator *op)
{
  const bool select_bigger = RNA_boolean_get(op->ptr, "select_bigger");

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totedgesel == 0) {
      continue;
    }

    BMIter iter;
    BMFace *f;

    /* Find the set of regions with smallest number of total faces. */
    BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);
    const int a = loop_find_regions(em, select_bigger);
    const int b = loop_find_regions(em, !select_bigger);

    BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);
    loop_find_regions(em, ((a <= b) != select_bigger) ? select_bigger : !select_bigger);

    /* Unlike most operators, always de-select all. */
    bool changed = true;
    EDBM_flag_disable_all(em, BM_ELEM_SELECT);

    BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
        BM_face_select_set(em->bm, f, true);
      }
    }

    if (changed) {
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_loop_to_region(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Loop Inner-Region";
  ot->idname = "MESH_OT_loop_to_region";
  ot->description = "Select region of faces inside of a selected loop of edges";

  /* API callbacks. */
  ot->exec = edbm_loop_to_region_exec;
  ot->poll = ED_operator_editmesh;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "select_bigger",
                  false,
                  "Select Bigger",
                  "Select bigger regions instead of smaller ones");
}

static bool edbm_select_by_attribute_poll(bContext *C)
{
  using namespace blender;
  if (!ED_operator_editmesh(C)) {
    return false;
  }
  Object *obedit = CTX_data_edit_object(C);
  const Mesh *mesh = static_cast<const Mesh *>(obedit->data);
  AttributeOwner owner = AttributeOwner::from_id(&const_cast<ID &>(mesh->id));
  const std::optional<StringRef> name = BKE_attributes_active_name_get(owner);
  if (!name) {
    CTX_wm_operator_poll_msg_set(C, "There must be an active attribute");
    return false;
  }
  const BMDataLayerLookup attr = BM_data_layer_lookup(*mesh->runtime->edit_mesh->bm, *name);
  if (attr.type != bke::AttrType::Bool) {
    CTX_wm_operator_poll_msg_set(C, "The active attribute must have a boolean type");
    return false;
  }
  if (attr.domain == bke::AttrDomain::Corner) {
    CTX_wm_operator_poll_msg_set(
        C, "The active attribute must be on the vertex, edge, or face domain");
    return false;
  }
  return true;
}

static std::optional<BMIterType> domain_to_iter_type(const blender::bke::AttrDomain domain)
{
  using namespace blender;
  switch (domain) {
    case bke::AttrDomain::Point:
      return BM_VERTS_OF_MESH;
    case bke::AttrDomain::Edge:
      return BM_EDGES_OF_MESH;
    case bke::AttrDomain::Face:
      return BM_FACES_OF_MESH;
    default:
      return std::nullopt;
  }
}

static wmOperatorStatus edbm_select_by_attribute_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Mesh *mesh = static_cast<Mesh *>(obedit->data);
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
    const std::optional<StringRef> name = BKE_attributes_active_name_get(owner);
    if (!name) {
      continue;
    }
    const BMDataLayerLookup attr = BM_data_layer_lookup(*bm, *name);
    if (!attr) {
      continue;
    }
    if (attr.type != bke::AttrType::Bool) {
      continue;
    }
    if (attr.domain == bke::AttrDomain::Corner) {
      continue;
    }
    const std::optional<BMIterType> iter_type = domain_to_iter_type(attr.domain);
    if (!iter_type) {
      continue;
    }

    bool changed = false;
    BMElem *elem;
    BMIter iter;
    BM_ITER_MESH (elem, &iter, bm, *iter_type) {
      if (BM_elem_flag_test(elem, BM_ELEM_HIDDEN | BM_ELEM_SELECT)) {
        continue;
      }
      if (BM_ELEM_CD_GET_BOOL(elem, attr.offset)) {
        BM_elem_select_set(bm, elem, true);
        changed = true;
      }
    }

    if (changed) {
      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_select_by_attribute(wmOperatorType *ot)
{
  ot->name = "Select by Attribute";
  ot->idname = "MESH_OT_select_by_attribute";
  ot->description = "Select elements based on the active boolean attribute";

  ot->exec = edbm_select_by_attribute_exec;
  ot->poll = edbm_select_by_attribute_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
