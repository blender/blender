/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Pre-selection Gizmo
 *
 * Use for tools to hover over data before activation.
 *
 * \note This is a slight misuse of gizmo's, since clicking performs no action.
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "bmesh.h"

#include "ED_gizmo_library.hh"
#include "ED_mesh.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

/* -------------------------------------------------------------------- */
/** \name Shared Internal API
 * \{ */

/**
 * Check if drawing should be performed, clear the pre-selection in the case it's disabled.
 * Without this, the gizmo would be visible while transforming. See #92954.
 *
 * NOTE(@ideasman42): This is a workaround for the gizmo system, since typically poll
 * would be used for this purpose. The problem with using poll is once the gizmo is visible again
 * is there is a visible flicker showing the previous location before cursor motion causes the
 * pre selection to be updated. While this is only a glitch, it's distracting.
 * The gizmo system itself could support this use case by tracking which gizmos draw and ensure
 * gizmos always run #wmGizmoType.test_select before drawing, however pre-selection is already
 * outside the scope of what gizmos are meant to be used for, so keep this workaround localized
 * to this gizmo type unless this seems worth supporting for more typical use-cases.
 *
 * Longer term it may be better to use #wmPaintCursor instead of gizmos (as snapping preview does).
 */
static bool gizmo_preselect_poll_for_draw(const bContext *C, wmGizmo *gz)
{
  if (G.moving == false) {
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (!(rv3d && (rv3d->rflag & RV3D_NAVIGATING))) {
      return true;
    }
  }
  ED_view3d_gizmo_mesh_preselect_clear(gz);
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Element (Vert/Edge/Face) Pre-Select Gizmo API
 * \{ */

struct MeshElemGizmo3D {
  wmGizmo gizmo;
  Base **bases;
  uint bases_len;
  int base_index;
  int vert_index;
  int edge_index;
  int face_index;
  EditMesh_PreSelElem *psel;
};

static void gizmo_preselect_elem_draw(const bContext *C, wmGizmo *gz)
{
  if (!gizmo_preselect_poll_for_draw(C, gz)) {
    return;
  }

  MeshElemGizmo3D *gz_ele = (MeshElemGizmo3D *)gz;
  if (gz_ele->base_index != -1) {
    Object *ob = gz_ele->bases[gz_ele->base_index]->object;
    EDBM_preselect_elem_draw(gz_ele->psel, ob->object_to_world);
  }
}

static int gizmo_preselect_elem_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  wmEvent *event = CTX_wm_window(C)->eventstate;
  MeshElemGizmo3D *gz_ele = (MeshElemGizmo3D *)gz;

  /* Hack: Switch action mode based on key input */
  const bool is_ctrl_pressed = (event->modifier & KM_CTRL) != 0;
  const bool is_shift_pressed = (event->modifier & KM_SHIFT) != 0;
  EDBM_preselect_action_set(gz_ele->psel, PRESELECT_ACTION_TRANSFORM);
  if (is_ctrl_pressed && !is_shift_pressed) {
    EDBM_preselect_action_set(gz_ele->psel, PRESELECT_ACTION_CREATE);
  }
  if (!is_ctrl_pressed && is_shift_pressed) {
    EDBM_preselect_action_set(gz_ele->psel, PRESELECT_ACTION_DELETE);
  }

  struct Best {
    Object *ob;
    BMElem *ele;
    float dist;
    int base_index;
  } best{};
  best.dist = ED_view3d_select_dist_px();

  {
    const Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    View3D *v3d = CTX_wm_view3d(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    if ((gz_ele->bases) == nullptr ||
        (gz_ele->bases[0] != BKE_view_layer_active_base_get(view_layer))) {
      MEM_SAFE_FREE(gz_ele->bases);
      gz_ele->bases = BKE_view_layer_array_from_bases_in_edit_mode(
          scene, view_layer, v3d, &gz_ele->bases_len);
    }
  }

  ViewContext vc;
  em_setup_viewcontext(C, &vc);
  copy_v2_v2_int(vc.mval, mval);

  {
    /* TODO: support faces. */
    int base_index_vert = -1;
    int base_index_edge = -1;
    int base_index_face = -1;
    BMVert *eve_test;
    BMEdge *eed_test;
    BMFace *efa_test;

    if (EDBM_unified_findnearest_from_raycast(&vc,
                                              gz_ele->bases,
                                              gz_ele->bases_len,
                                              false,
                                              true,
                                              &base_index_vert,
                                              &base_index_edge,
                                              &base_index_face,
                                              &eve_test,
                                              &eed_test,
                                              &efa_test))
    {
      if (EDBM_preselect_action_get(gz_ele->psel) == PRESELECT_ACTION_DELETE) {
        /* Delete action */
        if (efa_test) {
          best.ele = (BMElem *)efa_test;
          best.base_index = base_index_face;
        }
      }

      else {
        /* Transform and create action */
        if (eed_test) {
          best.ele = (BMElem *)eed_test;
          best.base_index = base_index_edge;
        }
      }

      /* All actions use same vertex pre-selection. */
      /* Re-topology should always prioritize edge pre-selection.
       * Only pre-select a vertex when the cursor is really close to it. */
      if (eve_test) {
        BMVert *vert = (BMVert *)eve_test;
        float vert_p_co[2], vert_co[3];
        const float mval_f[2] = {float(vc.mval[0]), float(vc.mval[1])};
        mul_v3_m4v3(vert_co, gz_ele->bases[base_index_vert]->object->object_to_world, vert->co);
        ED_view3d_project_v2(vc.region, vert_co, vert_p_co);
        float len = len_v2v2(vert_p_co, mval_f);
        if (len < 35) {
          best.ele = (BMElem *)eve_test;
          best.base_index = base_index_vert;
        }
        if (!BM_vert_is_boundary(vert) &&
            EDBM_preselect_action_get(gz_ele->psel) != PRESELECT_ACTION_DELETE)
        {
          best.ele = (BMElem *)eve_test;
          best.base_index = base_index_vert;
        }
      }

      /* Check above should never fail, if it does it's an internal error. */
      BLI_assert(best.base_index != -1);

      Base *base = gz_ele->bases[best.base_index];
      best.ob = base->object;
    }
  }

  BMesh *bm = nullptr;

  gz_ele->base_index = -1;
  gz_ele->vert_index = -1;
  gz_ele->edge_index = -1;
  gz_ele->face_index = -1;

  if (best.ele) {
    gz_ele->base_index = best.base_index;
    bm = BKE_editmesh_from_object(gz_ele->bases[gz_ele->base_index]->object)->bm;
    BM_mesh_elem_index_ensure(bm, best.ele->head.htype);

    if (best.ele->head.htype == BM_VERT) {
      gz_ele->vert_index = BM_elem_index_get(best.ele);
    }
    else if (best.ele->head.htype == BM_EDGE) {
      gz_ele->edge_index = BM_elem_index_get(best.ele);
    }
    else if (best.ele->head.htype == BM_FACE) {
      gz_ele->face_index = BM_elem_index_get(best.ele);
    }
  }

  if (best.ele) {
    const float(*coords)[3] = nullptr;
    {
      Object *ob = gz_ele->bases[gz_ele->base_index]->object;
      Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      Mesh *me_eval = (Mesh *)DEG_get_evaluated_id(depsgraph, static_cast<ID *>(ob->data));
      if (BKE_mesh_wrapper_vert_len(me_eval) == bm->totvert) {
        coords = BKE_mesh_wrapper_vert_coords(me_eval);
      }
    }
    EDBM_preselect_elem_update_from_single(gz_ele->psel, bm, best.ele, coords);
    EDBM_preselect_elem_update_preview(gz_ele->psel, &vc, bm, best.ele, mval);
  }
  else {
    EDBM_preselect_elem_clear(gz_ele->psel);
    EDBM_preselect_preview_clear(gz_ele->psel);
  }

  RNA_int_set(gz->ptr, "object_index", gz_ele->base_index);
  RNA_int_set(gz->ptr, "vert_index", gz_ele->vert_index);
  RNA_int_set(gz->ptr, "edge_index", gz_ele->edge_index);
  RNA_int_set(gz->ptr, "face_index", gz_ele->face_index);

  if (best.ele) {
    ARegion *region = CTX_wm_region(C);
    ED_region_tag_redraw_editor_overlays(region);
  }

  // return best.eed ? 0 : -1;
  return -1;
}

static void gizmo_preselect_elem_setup(wmGizmo *gz)
{
  MeshElemGizmo3D *gz_ele = (MeshElemGizmo3D *)gz;
  if (gz_ele->psel == nullptr) {
    gz_ele->psel = EDBM_preselect_elem_create();
  }
  gz_ele->base_index = -1;
}

static void gizmo_preselect_elem_free(wmGizmo *gz)
{
  MeshElemGizmo3D *gz_ele = (MeshElemGizmo3D *)gz;
  EDBM_preselect_elem_destroy(gz_ele->psel);
  gz_ele->psel = nullptr;
  MEM_SAFE_FREE(gz_ele->bases);
}

static int gizmo_preselect_elem_invoke(bContext * /*C*/,
                                       wmGizmo * /*gz*/,
                                       const wmEvent * /*event*/)
{
  return OPERATOR_PASS_THROUGH;
}

static void GIZMO_GT_mesh_preselect_elem_3d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_mesh_preselect_elem_3d";

  /* api callbacks */
  gzt->invoke = gizmo_preselect_elem_invoke;
  gzt->draw = gizmo_preselect_elem_draw;
  gzt->test_select = gizmo_preselect_elem_test_select;
  gzt->setup = gizmo_preselect_elem_setup;
  gzt->free = gizmo_preselect_elem_free;

  gzt->struct_size = sizeof(MeshElemGizmo3D);

  RNA_def_int(gzt->srna, "object_index", -1, -1, INT_MAX, "Object Index", "", -1, INT_MAX);
  RNA_def_int(gzt->srna, "vert_index", -1, -1, INT_MAX, "Vert Index", "", -1, INT_MAX);
  RNA_def_int(gzt->srna, "edge_index", -1, -1, INT_MAX, "Edge Index", "", -1, INT_MAX);
  RNA_def_int(gzt->srna, "face_index", -1, -1, INT_MAX, "Face Index", "", -1, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Edge-Ring Pre-Select Gizmo API
 * \{ */

struct MeshEdgeRingGizmo3D {
  wmGizmo gizmo;
  Base **bases;
  uint bases_len;
  int base_index;
  int edge_index;
  EditMesh_PreSelEdgeRing *psel;
};

static void gizmo_preselect_edgering_draw(const bContext *C, wmGizmo *gz)
{
  if (!gizmo_preselect_poll_for_draw(C, gz)) {
    return;
  }

  MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
  if (gz_ring->base_index != -1) {
    Object *ob = gz_ring->bases[gz_ring->base_index]->object;
    EDBM_preselect_edgering_draw(gz_ring->psel, ob->object_to_world);
  }
}

static int gizmo_preselect_edgering_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
  struct Best {
    Object *ob;
    BMEdge *eed;
    float dist;
    int base_index;
  } best{};
  best.dist = ED_view3d_select_dist_px();

  struct Prev {
    int base_index;
    int edge_index;
  } prev{};
  prev.base_index = gz_ring->base_index;
  prev.edge_index = gz_ring->edge_index;

  {
    const Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    View3D *v3d = CTX_wm_view3d(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    if ((gz_ring->bases) == nullptr ||
        (gz_ring->bases[0] != BKE_view_layer_active_base_get(view_layer)))
    {
      MEM_SAFE_FREE(gz_ring->bases);
      gz_ring->bases = BKE_view_layer_array_from_bases_in_edit_mode(
          scene, view_layer, v3d, &gz_ring->bases_len);
    }
  }

  ViewContext vc;
  em_setup_viewcontext(C, &vc);
  copy_v2_v2_int(vc.mval, mval);

  uint base_index;
  BMEdge *eed_test = EDBM_edge_find_nearest_ex(&vc,
                                               &best.dist,
                                               nullptr,
                                               false,
                                               false,
                                               nullptr,
                                               gz_ring->bases,
                                               gz_ring->bases_len,
                                               &base_index);

  if (eed_test) {
    best.ob = gz_ring->bases[base_index]->object;
    best.eed = eed_test;
    best.base_index = base_index;
  }

  BMesh *bm = nullptr;
  if (best.eed) {
    gz_ring->base_index = best.base_index;
    bm = BKE_editmesh_from_object(gz_ring->bases[gz_ring->base_index]->object)->bm;
    BM_mesh_elem_index_ensure(bm, BM_EDGE);
    gz_ring->edge_index = BM_elem_index_get(best.eed);
  }
  else {
    gz_ring->base_index = -1;
    gz_ring->edge_index = -1;
  }

  if ((prev.base_index == gz_ring->base_index) && (prev.edge_index == gz_ring->edge_index)) {
    /* pass (only recalculate on change) */
  }
  else {
    if (best.eed) {
      Object *ob = gz_ring->bases[gz_ring->base_index]->object;
      Scene *scene_eval = (Scene *)DEG_get_evaluated_id(vc.depsgraph, &vc.scene->id);
      Object *ob_eval = DEG_get_evaluated_object(vc.depsgraph, ob);
      BMEditMesh *em_eval = BKE_editmesh_from_object(ob_eval);
      /* Re-allocate coords each update isn't ideal, however we can't be sure
       * the mesh hasn't been edited since last update. */
      bool is_alloc = false;
      const float(*coords)[3] = BKE_editmesh_vert_coords_when_deformed(
          vc.depsgraph, em_eval, scene_eval, ob_eval, nullptr, &is_alloc);
      EDBM_preselect_edgering_update_from_edge(gz_ring->psel, bm, best.eed, 1, coords);
      if (is_alloc) {
        MEM_freeN((void *)coords);
      }
    }
    else {
      EDBM_preselect_edgering_clear(gz_ring->psel);
    }

    RNA_int_set(gz->ptr, "object_index", gz_ring->base_index);
    RNA_int_set(gz->ptr, "edge_index", gz_ring->edge_index);

    ARegion *region = CTX_wm_region(C);
    ED_region_tag_redraw_editor_overlays(region);
  }

  // return best.eed ? 0 : -1;
  return -1;
}

static void gizmo_preselect_edgering_setup(wmGizmo *gz)
{
  MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
  if (gz_ring->psel == nullptr) {
    gz_ring->psel = EDBM_preselect_edgering_create();
  }
  gz_ring->base_index = -1;
}

static void gizmo_preselect_edgering_free(wmGizmo *gz)
{
  MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
  EDBM_preselect_edgering_destroy(gz_ring->psel);
  gz_ring->psel = nullptr;
  MEM_SAFE_FREE(gz_ring->bases);
}

static int gizmo_preselect_edgering_invoke(bContext * /*C*/,
                                           wmGizmo * /*gz*/,
                                           const wmEvent * /*event*/)
{
  return OPERATOR_PASS_THROUGH;
}

static void GIZMO_GT_mesh_preselect_edgering_3d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_mesh_preselect_edgering_3d";

  /* api callbacks */
  gzt->invoke = gizmo_preselect_edgering_invoke;
  gzt->draw = gizmo_preselect_edgering_draw;
  gzt->test_select = gizmo_preselect_edgering_test_select;
  gzt->setup = gizmo_preselect_edgering_setup;
  gzt->free = gizmo_preselect_edgering_free;

  gzt->struct_size = sizeof(MeshEdgeRingGizmo3D);

  RNA_def_int(gzt->srna, "object_index", -1, -1, INT_MAX, "Object Index", "", -1, INT_MAX);
  RNA_def_int(gzt->srna, "edge_index", -1, -1, INT_MAX, "Edge Index", "", -1, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo API
 * \{ */

void ED_gizmotypes_preselect_3d()
{
  WM_gizmotype_append(GIZMO_GT_mesh_preselect_elem_3d);
  WM_gizmotype_append(GIZMO_GT_mesh_preselect_edgering_3d);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo Accessors
 *
 * This avoids each user of the gizmo needing to write their own lookups to access
 * the information from this gizmo.
 * \{ */

void ED_view3d_gizmo_mesh_preselect_get_active(bContext *C,
                                               wmGizmo *gz,
                                               Base **r_base,
                                               BMElem **r_ele)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  const int object_index = RNA_int_get(gz->ptr, "object_index");

  /* weak, allocate an array just to access the index. */
  Base *base = nullptr;
  Object *obedit = nullptr;
  {
    uint bases_len;
    Base **bases = BKE_view_layer_array_from_bases_in_edit_mode(
        scene, view_layer, CTX_wm_view3d(C), &bases_len);
    if (object_index < bases_len) {
      base = bases[object_index];
      obedit = base->object;
    }
    MEM_freeN(bases);
  }

  *r_base = base;
  *r_ele = nullptr;

  if (obedit) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    PropertyRNA *prop;

    /* Ring select only defines edge, check properties exist first. */
    prop = RNA_struct_find_property(gz->ptr, "vert_index");
    const int vert_index = prop ? RNA_property_int_get(gz->ptr, prop) : -1;
    prop = RNA_struct_find_property(gz->ptr, "edge_index");
    const int edge_index = prop ? RNA_property_int_get(gz->ptr, prop) : -1;
    prop = RNA_struct_find_property(gz->ptr, "face_index");
    const int face_index = prop ? RNA_property_int_get(gz->ptr, prop) : -1;

    if (vert_index != -1) {
      *r_ele = (BMElem *)BM_vert_at_index_find(bm, vert_index);
    }
    else if (edge_index != -1) {
      *r_ele = (BMElem *)BM_edge_at_index_find(bm, edge_index);
    }
    else if (face_index != -1) {
      *r_ele = (BMElem *)BM_face_at_index_find(bm, face_index);
    }
  }
}

void ED_view3d_gizmo_mesh_preselect_clear(wmGizmo *gz)
{
  if (STREQ(gz->type->idname, "GIZMO_GT_mesh_preselect_elem_3d")) {
    MeshElemGizmo3D *gz_ele = (MeshElemGizmo3D *)gz;
    gz_ele->base_index = -1;
    gz_ele->vert_index = -1;
    gz_ele->edge_index = -1;
    gz_ele->face_index = -1;
  }
  else if (STREQ(gz->type->idname, "GIZMO_GT_mesh_preselect_edgering_3d")) {
    MeshEdgeRingGizmo3D *gz_ele = (MeshEdgeRingGizmo3D *)gz;
    gz_ele->base_index = -1;
    gz_ele->edge_index = -1;
  }
  else {
    BLI_assert_unreachable();
  }

  const char *prop_ids[] = {"object_index", "vert_index", "edge_index", "face_index"};
  for (int i = 0; i < ARRAY_SIZE(prop_ids); i++) {
    PropertyRNA *prop = RNA_struct_find_property(gz->ptr, prop_ids[i]);
    if (prop == nullptr) {
      continue;
    }
    RNA_property_int_set(gz->ptr, prop, -1);
  }
}

/** \} */
