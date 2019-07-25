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
 * \ingroup wm
 *
 * \name Preselection Gizmo
 *
 * Use for tools to hover over data before activation.
 *
 * \note This is a slight misuse of gizmo's, since clicking performs no action.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_mesh_types.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_editmesh.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "bmesh.h"

#include "ED_screen.h"
#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_gizmo_library.h"

/* -------------------------------------------------------------------- */
/** \name Mesh Element (Vert/Edge/Face) Pre-Select Gizmo API
 *
 * \{ */

typedef struct MeshElemGizmo3D {
  wmGizmo gizmo;
  Base **bases;
  uint bases_len;
  int base_index;
  int vert_index;
  int edge_index;
  int face_index;
  struct EditMesh_PreSelElem *psel;
} MeshElemGizmo3D;

static void gizmo_preselect_elem_draw(const bContext *UNUSED(C), wmGizmo *gz)
{
  MeshElemGizmo3D *gz_ele = (MeshElemGizmo3D *)gz;
  if (gz_ele->base_index != -1) {
    Object *ob = gz_ele->bases[gz_ele->base_index]->object;
    EDBM_preselect_elem_draw(gz_ele->psel, ob->obmat);
  }
}

static int gizmo_preselect_elem_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  MeshElemGizmo3D *gz_ele = (MeshElemGizmo3D *)gz;
  struct {
    Object *ob;
    BMElem *ele;
    float dist;
    int base_index;
  } best = {
      .dist = ED_view3d_select_dist_px(),
  };

  struct {
    int base_index;
    int vert_index;
    int edge_index;
    int face_index;
  } prev = {
      .base_index = gz_ele->base_index,
      .vert_index = gz_ele->vert_index,
      .edge_index = gz_ele->edge_index,
      .face_index = gz_ele->face_index,
  };

  {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    View3D *v3d = CTX_wm_view3d(C);
    if (((gz_ele->bases)) == NULL || (gz_ele->bases[0] != view_layer->basact)) {
      MEM_SAFE_FREE(gz_ele->bases);
      gz_ele->bases = BKE_view_layer_array_from_bases_in_edit_mode(
          view_layer, v3d, &gz_ele->bases_len);
    }
  }

  ViewContext vc;
  em_setup_viewcontext(C, &vc);
  copy_v2_v2_int(vc.mval, mval);

  {
    /* TODO: support faces. */
    int base_index = -1;
    BMVert *eve_test;
    BMEdge *eed_test;

    if (EDBM_unified_findnearest_from_raycast(&vc,
                                              gz_ele->bases,
                                              gz_ele->bases_len,
                                              true,
                                              &base_index,
                                              &eve_test,
                                              &eed_test,
                                              NULL)) {
      Base *base = gz_ele->bases[base_index];
      best.ob = base->object;
      if (eve_test) {
        best.ele = (BMElem *)eve_test;
      }
      else if (eed_test) {
        best.ele = (BMElem *)eed_test;
      }
      else {
        BLI_assert(0);
      }
      best.base_index = base_index;
      /* Check above should never fail, if it does it's an internal error. */
      BLI_assert(best.base_index != -1);
    }
  }

  BMesh *bm = NULL;

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

  if ((prev.base_index == gz_ele->base_index) && (prev.vert_index == gz_ele->vert_index) &&
      (prev.edge_index == gz_ele->edge_index) && (prev.face_index == gz_ele->face_index)) {
    /* pass (only recalculate on change) */
  }
  else {
    if (best.ele) {
      const float(*coords)[3] = NULL;
      {
        Object *ob = gz_ele->bases[gz_ele->base_index]->object;
        Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
        Mesh *me_eval = (Mesh *)DEG_get_evaluated_id(depsgraph, ob->data);
        if (me_eval->runtime.edit_data) {
          coords = me_eval->runtime.edit_data->vertexCos;
        }
      }
      EDBM_preselect_elem_update_from_single(gz_ele->psel, bm, best.ele, coords);
    }
    else {
      EDBM_preselect_elem_clear(gz_ele->psel);
    }

    RNA_int_set(gz->ptr, "object_index", gz_ele->base_index);
    RNA_int_set(gz->ptr, "vert_index", gz_ele->vert_index);
    RNA_int_set(gz->ptr, "edge_index", gz_ele->edge_index);
    RNA_int_set(gz->ptr, "face_index", gz_ele->face_index);

    ARegion *ar = CTX_wm_region(C);
    ED_region_tag_redraw(ar);
  }

  // return best.eed ? 0 : -1;
  return -1;
}

static void gizmo_preselect_elem_setup(wmGizmo *gz)
{
  MeshElemGizmo3D *gz_ele = (MeshElemGizmo3D *)gz;
  if (gz_ele->psel == NULL) {
    gz_ele->psel = EDBM_preselect_elem_create();
  }
  gz_ele->base_index = -1;
}

static void gizmo_preselect_elem_free(wmGizmo *gz)
{
  MeshElemGizmo3D *gz_ele = (MeshElemGizmo3D *)gz;
  EDBM_preselect_elem_destroy(gz_ele->psel);
  gz_ele->psel = NULL;
  MEM_SAFE_FREE(gz_ele->bases);
}

static int gizmo_preselect_elem_invoke(bContext *UNUSED(C),
                                       wmGizmo *UNUSED(gz),
                                       const wmEvent *UNUSED(event))
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
 *
 * \{ */

typedef struct MeshEdgeRingGizmo3D {
  wmGizmo gizmo;
  Base **bases;
  uint bases_len;
  int base_index;
  int edge_index;
  struct EditMesh_PreSelEdgeRing *psel;
} MeshEdgeRingGizmo3D;

static void gizmo_preselect_edgering_draw(const bContext *UNUSED(C), wmGizmo *gz)
{
  MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
  if (gz_ring->base_index != -1) {
    Object *ob = gz_ring->bases[gz_ring->base_index]->object;
    EDBM_preselect_edgering_draw(gz_ring->psel, ob->obmat);
  }
}

static int gizmo_preselect_edgering_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
  struct {
    Object *ob;
    BMEdge *eed;
    float dist;
    int base_index;
  } best = {
      .dist = ED_view3d_select_dist_px(),
  };

  struct {
    int base_index;
    int edge_index;
  } prev = {
      .base_index = gz_ring->base_index,
      .edge_index = gz_ring->edge_index,
  };

  {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    View3D *v3d = CTX_wm_view3d(C);
    if (((gz_ring->bases)) == NULL || (gz_ring->bases[0] != view_layer->basact)) {
      MEM_SAFE_FREE(gz_ring->bases);
      gz_ring->bases = BKE_view_layer_array_from_bases_in_edit_mode(
          view_layer, v3d, &gz_ring->bases_len);
    }
  }

  ViewContext vc;
  em_setup_viewcontext(C, &vc);
  copy_v2_v2_int(vc.mval, mval);

  uint base_index;
  BMEdge *eed_test = EDBM_edge_find_nearest_ex(
      &vc, &best.dist, NULL, false, false, NULL, gz_ring->bases, gz_ring->bases_len, &base_index);

  if (eed_test) {
    best.ob = gz_ring->bases[base_index]->object;
    best.eed = eed_test;
    best.base_index = base_index;
  }

  BMesh *bm = NULL;
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
      const float(*coords)[3] = NULL;
      {
        Object *ob = gz_ring->bases[gz_ring->base_index]->object;
        Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
        Mesh *me_eval = (Mesh *)DEG_get_evaluated_id(depsgraph, ob->data);
        if (me_eval->runtime.edit_data) {
          coords = me_eval->runtime.edit_data->vertexCos;
        }
      }
      EDBM_preselect_edgering_update_from_edge(gz_ring->psel, bm, best.eed, 1, coords);
    }
    else {
      EDBM_preselect_edgering_clear(gz_ring->psel);
    }

    RNA_int_set(gz->ptr, "object_index", gz_ring->base_index);
    RNA_int_set(gz->ptr, "edge_index", gz_ring->edge_index);

    ARegion *ar = CTX_wm_region(C);
    ED_region_tag_redraw(ar);
  }

  // return best.eed ? 0 : -1;
  return -1;
}

static void gizmo_preselect_edgering_setup(wmGizmo *gz)
{
  MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
  if (gz_ring->psel == NULL) {
    gz_ring->psel = EDBM_preselect_edgering_create();
  }
  gz_ring->base_index = -1;
}

static void gizmo_preselect_edgering_free(wmGizmo *gz)
{
  MeshEdgeRingGizmo3D *gz_ring = (MeshEdgeRingGizmo3D *)gz;
  EDBM_preselect_edgering_destroy(gz_ring->psel);
  gz_ring->psel = NULL;
  MEM_SAFE_FREE(gz_ring->bases);
}

static int gizmo_preselect_edgering_invoke(bContext *UNUSED(C),
                                           wmGizmo *UNUSED(gz),
                                           const wmEvent *UNUSED(event))
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
 *
 * \{ */

void ED_gizmotypes_preselect_3d(void)
{
  WM_gizmotype_append(GIZMO_GT_mesh_preselect_elem_3d);
  WM_gizmotype_append(GIZMO_GT_mesh_preselect_edgering_3d);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo Accessors
 *
 * This avoids each user of the gizmo needing to write their own look-ups to access
 * the information from this gizmo.
 * \{ */

void ED_view3d_gizmo_mesh_preselect_get_active(bContext *C,
                                               wmGizmo *gz,
                                               Base **r_base,
                                               BMElem **r_ele)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);

  const int object_index = RNA_int_get(gz->ptr, "object_index");

  /* weak, allocate an array just to access the index. */
  Base *base = NULL;
  Object *obedit = NULL;
  {
    uint bases_len;
    Base **bases = BKE_view_layer_array_from_bases_in_edit_mode(
        view_layer, CTX_wm_view3d(C), &bases_len);
    if (object_index < bases_len) {
      base = bases[object_index];
      obedit = base->object;
    }
    MEM_freeN(bases);
  }

  *r_base = base;
  *r_ele = NULL;

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

/** \} */
