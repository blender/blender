/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 *
 * based on mouse cursor position, split of vertices along the closest edge.
 */

#include "DNA_object_types.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

#include "WM_types.hh"

#include "ED_mesh.hh"
#include "ED_transform.hh"
#include "ED_view3d.hh"

#include "bmesh.hh"

#include "mesh_intern.hh" /* own include */

using blender::float2;
using blender::Vector;

/* uses total number of selected edges around a vertex to choose how to extend */
#define USE_TRICKY_EXTEND

static wmOperatorStatus edbm_rip_edge_invoke(bContext *C,
                                             wmOperator * /*op*/,
                                             const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;

    BMIter viter;
    BMVert *v;
    const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
    float cent_sco[2];
    int cent_tot;
    bool changed = false;

    /* mouse direction to view center */
    float mval_dir[2];

    if (bm->totvertsel == 0) {
      continue;
    }

    const blender::float4x4 projectMat = ED_view3d_ob_project_mat_get(rv3d, obedit);

    zero_v2(cent_sco);
    cent_tot = 0;

    /* clear tags and calc screen center */
    BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
      BM_elem_flag_disable(v, BM_ELEM_TAG);

      if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
        const float2 v_sco = ED_view3d_project_float_v2_m4(region, v->co, projectMat);

        add_v2_v2(cent_sco, v_sco);
        cent_tot += 1;
      }
    }
    mul_v2_fl(cent_sco, 1.0f / float(cent_tot));

    /* not essential, but gives more expected results with edge selection */
    if (bm->totedgesel) {
      /* angle against center can give odd result,
       * try re-position the center to the closest edge */
      BMIter eiter;
      BMEdge *e;
      float dist_sq_best = len_squared_v2v2(cent_sco, mval_fl);

      BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
          float cent_sco_test[2];
          float dist_sq_test;

          const float2 e_sco_0 = ED_view3d_project_float_v2_m4(region, e->v1->co, projectMat);
          const float2 e_sco_1 = ED_view3d_project_float_v2_m4(region, e->v2->co, projectMat);

          closest_to_line_segment_v2(cent_sco_test, mval_fl, e_sco_0, e_sco_1);
          dist_sq_test = len_squared_v2v2(cent_sco_test, mval_fl);
          if (dist_sq_test < dist_sq_best) {
            dist_sq_best = dist_sq_test;

            /* we have a new screen center */
            copy_v2_v2(cent_sco, cent_sco_test);
          }
        }
      }
    }

    sub_v2_v2v2(mval_dir, mval_fl, cent_sco);
    normalize_v2(mval_dir);

    /* operate on selected verts */
    BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
      BMIter eiter;
      BMEdge *e;
      float2 v_sco;

      if (BM_elem_flag_test(v, BM_ELEM_SELECT) && BM_elem_flag_test(v, BM_ELEM_TAG) == false) {
        /* Rules for */
        float angle_best = FLT_MAX;
        BMEdge *e_best = nullptr;

#ifdef USE_TRICKY_EXTEND
        /* first check if we can select the edge to split based on selection-only */
        int tot_sel = 0;

        BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
          if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
            if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
              e_best = e;
              tot_sel += 1;
            }
          }
        }
        if (tot_sel != 1) {
          e_best = nullptr;
        }

        /* only one edge selected, operate on that */
        if (e_best) {
          goto found_edge;
        }
        /* none selected, fall through and find one */
        else if (tot_sel == 0) {
          /* pass */
        }
        /* selection not 0 or 1, do nothing */
        else {
          goto found_edge;
        }
#endif
        v_sco = ED_view3d_project_float_v2_m4(region, v->co, projectMat);

        BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
          if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
            BMVert *v_other = BM_edge_other_vert(e, v);
            float angle_test;

            float2 v_other_sco = ED_view3d_project_float_v2_m4(region, v_other->co, projectMat);

            /* avoid comparing with view-axis aligned edges (less than a pixel) */
            if (len_squared_v2v2(v_sco, v_other_sco) > 1.0f) {
              float v_dir[2];

              sub_v2_v2v2(v_dir, v_other_sco, v_sco);
              normalize_v2(v_dir);

              angle_test = angle_normalized_v2v2(mval_dir, v_dir);

              if (angle_test < angle_best) {
                angle_best = angle_test;
                e_best = e;
              }
            }
          }
        }

#ifdef USE_TRICKY_EXTEND
      found_edge:
#endif
        if (e_best) {
          const bool e_select = BM_elem_flag_test_bool(e_best, BM_ELEM_SELECT);
          BMVert *v_new;
          BMEdge *e_new;

          v_new = BM_edge_split(bm, e_best, v, &e_new, 0.0f);

          BM_vert_select_set(bm, v, false);
          BM_edge_select_set(bm, e_new, false);

          BM_vert_select_set(bm, v_new, true);
          if (e_select) {
            BM_edge_select_set(bm, e_best, true);
          }
          BM_elem_flag_enable(v_new, BM_ELEM_TAG); /* prevent further splitting */

          /* When UV sync select is enabled, the wrong UV's will be selected
           * because the existing loops will have the selection and the new ones won't.
           * transfer the selection state to the new loops. */
          if (bm->uv_select_sync_valid) {
            if (e_best->l) {
              BMLoop *l_iter, *l_first;
              l_iter = l_first = e_best->l;
              do {
                bool was_select = false;
                if (l_iter->next->e == e_new) {
                  if (BM_elem_flag_test(l_iter, BM_ELEM_SELECT_UV)) {
                    BM_loop_edge_uvselect_set(bm, l_iter->next, false);
                    was_select = true;
                  }
                }
                else {
                  BLI_assert(l_iter->prev->e == e_new);
                  if (BM_elem_flag_test(l_iter->prev, BM_ELEM_SELECT_UV)) {
                    BM_loop_edge_uvselect_set(bm, l_iter->prev, false);
                    was_select = true;
                  }
                }
                if (was_select) {
                  BM_loop_edge_uvselect_set(bm, l_iter, true);
                }

              } while ((l_iter = l_iter->radial_next) != l_first);
            }
          }

          changed = true;
        }
      }
    }

    if (changed) {
      BM_select_history_clear(bm);

      BM_mesh_select_mode_flush(bm);

      EDBMUpdate_Params params{};
      params.calc_looptris = true;
      params.calc_normals = false;
      params.is_destructive = true;
      EDBM_update(static_cast<Mesh *>(obedit->data), &params);
    }
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_rip_edge(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extend Vertices";
  ot->idname = "MESH_OT_rip_edge";
  ot->description = "Extend vertices along the edge closest to the cursor";

  /* API callbacks. */
  ot->invoke = edbm_rip_edge_invoke;
  ot->poll = EDBM_view3d_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* to give to transform */
  blender::ed::transform::properties_register(ot, P_PROPORTIONAL | P_MIRROR_DUMMY);
}
