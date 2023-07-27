/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_curve_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_curve.h"

#include "transform.hh"
#include "transform_snap.hh"

/* Own include. */
#include "transform_convert.hh"
#include "transform_orientations.hh"

/* -------------------------------------------------------------------- */
/** \name Curve/Surfaces Transform Creation
 * \{ */

/**
 * For the purpose of transform code we need to behave as if handles are selected,
 * even when they aren't (see special case below).
 */
static int bezt_select_to_transform_triple_flag(const BezTriple *bezt, const bool hide_handles)
{
  int flag = 0;

  if (hide_handles) {
    if (bezt->f2 & SELECT) {
      flag = (1 << 0) | (1 << 1) | (1 << 2);
    }
  }
  else {
    flag = (((bezt->f1 & SELECT) ? (1 << 0) : 0) | ((bezt->f2 & SELECT) ? (1 << 1) : 0) |
            ((bezt->f3 & SELECT) ? (1 << 2) : 0));
  }

  /* Special case for auto & aligned handles:
   * When a center point is being moved without the handles,
   * leaving the handles stationary makes no sense and only causes strange behavior,
   * where one handle is arbitrarily anchored, the other one is aligned and lengthened
   * based on where the center point is moved. Also a bug when canceling, see: #52007.
   *
   * A more 'correct' solution could be to store handle locations in 'TransDataCurveHandleFlags'.
   * However that doesn't resolve odd behavior, so best transform the handles in this case.
   */
  if ((flag != ((1 << 0) | (1 << 1) | (1 << 2))) && (flag & (1 << 1))) {
    if (ELEM(bezt->h1, HD_AUTO, HD_ALIGN) && ELEM(bezt->h2, HD_AUTO, HD_ALIGN)) {
      flag = (1 << 0) | (1 << 1) | (1 << 2);
    }
  }

  return flag;
}

static void createTransCurveVerts(bContext * /*C*/, TransInfo *t)
{

#define SEL_F1 (1 << 0)
#define SEL_F2 (1 << 1)
#define SEL_F3 (1 << 2)

  t->data_len_all = 0;

  /* Count control points (one per #BezTriple) if any number of handles are selected.
   * Needed for #transform_around_single_fallback_ex. */
  int data_len_all_pt = 0;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Curve *cu = static_cast<Curve *>(tc->obedit->data);
    BLI_assert(cu->editnurb != nullptr);
    BezTriple *bezt;
    BPoint *bp;
    int a;
    int count = 0, countsel = 0;
    int count_pt = 0, countsel_pt = 0;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
    const bool is_prop_connected = (t->flag & T_PROP_CONNECTED) != 0;
    View3D *v3d = static_cast<View3D *>(t->view);
    short hide_handles = (v3d != nullptr) ? (v3d->overlay.handle_display == CURVE_HANDLE_NONE) :
                                            false;

    /* count total of vertices, check identical as in 2nd loop for making transdata! */
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);
    LISTBASE_FOREACH (Nurb *, nu, nurbs) {
      if (nu->type == CU_BEZIER) {
        for (a = 0, bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
          if (bezt->hide == 0) {
            const int bezt_tx = bezt_select_to_transform_triple_flag(bezt, hide_handles);
            if (bezt_tx & (SEL_F1 | SEL_F2 | SEL_F3)) {
              if (bezt_tx & SEL_F1) {
                countsel++;
              }
              if (bezt_tx & SEL_F2) {
                countsel++;
              }
              if (bezt_tx & SEL_F3) {
                countsel++;
              }
              countsel_pt++;
            }
            if (is_prop_edit) {
              count += 3;
              count_pt++;
            }
          }
        }
      }
      else {
        for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a > 0; a--, bp++) {
          if (bp->hide == 0) {
            if (bp->f1 & SELECT) {
              countsel++;
              countsel_pt++;
            }
            if (is_prop_edit) {
              count++;
              count_pt++;
            }
          }
        }
      }
    }

    /* Support other objects using proportional editing to adjust these, unless connected is
     * enabled. */
    if (((is_prop_edit && !is_prop_connected) ? count : countsel) == 0) {
      tc->data_len = 0;
      continue;
    }

    int data_len_pt = 0;

    if (is_prop_edit) {
      tc->data_len = count;
      data_len_pt = count_pt;
    }
    else {
      tc->data_len = countsel;
      data_len_pt = countsel_pt;
    }
    tc->data = static_cast<TransData *>(
        MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Curve EditMode)"));

    t->data_len_all += tc->data_len;
    data_len_all_pt += data_len_pt;
  }

  transform_around_single_fallback_ex(t, data_len_all_pt);
  t->data_len_all = -1;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len == 0) {
      continue;
    }

    Curve *cu = static_cast<Curve *>(tc->obedit->data);
    BezTriple *bezt;
    BPoint *bp;
    int a;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
    View3D *v3d = static_cast<View3D *>(t->view);
    short hide_handles = (v3d != nullptr) ? (v3d->overlay.handle_display == CURVE_HANDLE_NONE) :
                                            false;

    bool use_around_origins_for_handles_test = ((t->around == V3D_AROUND_LOCAL_ORIGINS) &&
                                                transform_mode_use_local_origins(t));
    float mtx[3][3], smtx[3][3];

    copy_m3_m4(mtx, tc->obedit->object_to_world);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    TransData *td = tc->data;
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);
    LISTBASE_FOREACH (Nurb *, nu, nurbs) {
      TransData *head, *tail;
      head = tail = td;
      bool has_any_selected = false;
      if (nu->type == CU_BEZIER) {
        for (a = 0, bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
          if (bezt->hide == 0) {
            TransDataCurveHandleFlags *hdata = nullptr;
            float axismtx[3][3];

            if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
              float normal[3], plane[3];

              BKE_nurb_bezt_calc_normal(nu, bezt, normal);
              BKE_nurb_bezt_calc_plane(nu, bezt, plane);

              if (createSpaceNormalTangent(axismtx, normal, plane)) {
                /* pass */
              }
              else {
                normalize_v3(normal);
                axis_dominant_v3_to_m3(axismtx, normal);
                invert_m3(axismtx);
              }
            }

            /* Elements that will be transform (not always a match to selection). */
            const int bezt_tx = bezt_select_to_transform_triple_flag(bezt, hide_handles);
            has_any_selected |= bezt_tx != 0;

            if (is_prop_edit || bezt_tx & SEL_F1) {
              copy_v3_v3(td->iloc, bezt->vec[0]);
              td->loc = bezt->vec[0];
              copy_v3_v3(td->center,
                         bezt->vec[(hide_handles || (t->around == V3D_AROUND_LOCAL_ORIGINS) ||
                                    (bezt->f2 & SELECT)) ?
                                       1 :
                                       0]);
              if (hide_handles) {
                if (bezt->f2 & SELECT) {
                  td->flag = TD_SELECTED;
                }
                else {
                  td->flag = 0;
                }
              }
              else {
                if (bezt->f1 & SELECT) {
                  td->flag = TD_SELECTED;
                }
                else {
                  td->flag = 0;
                }
              }
              td->ext = nullptr;
              td->val = nullptr;

              hdata = initTransDataCurveHandles(td, bezt);

              copy_m3_m3(td->smtx, smtx);
              copy_m3_m3(td->mtx, mtx);
              if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
                copy_m3_m3(td->axismtx, axismtx);
              }

              td++;
              tail++;
            }

            /* This is the Curve Point, the other two are handles */
            if (is_prop_edit || bezt_tx & SEL_F2) {
              copy_v3_v3(td->iloc, bezt->vec[1]);
              td->loc = bezt->vec[1];
              copy_v3_v3(td->center, td->loc);
              if (bezt->f2 & SELECT) {
                td->flag = TD_SELECTED;
              }
              else {
                td->flag = 0;
              }
              td->ext = nullptr;

              /* TODO: make points scale. */
              if (t->mode == TFM_CURVE_SHRINKFATTEN /* `|| t->mode == TFM_RESIZE` */) {
                td->val = &(bezt->radius);
                td->ival = bezt->radius;
              }
              else if (t->mode == TFM_TILT) {
                td->val = &(bezt->tilt);
                td->ival = bezt->tilt;
              }
              else {
                td->val = nullptr;
              }

              copy_m3_m3(td->smtx, smtx);
              copy_m3_m3(td->mtx, mtx);
              if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
                copy_m3_m3(td->axismtx, axismtx);
              }

              if ((bezt_tx & SEL_F1) == 0 && (bezt_tx & SEL_F3) == 0) {
                /* If the middle is selected but the sides aren't, this is needed. */
                if (hdata == nullptr) {
                  /* if the handle was not saved by the previous handle */
                  hdata = initTransDataCurveHandles(td, bezt);
                }
              }

              td++;
              tail++;
            }
            if (is_prop_edit || bezt_tx & SEL_F3) {
              copy_v3_v3(td->iloc, bezt->vec[2]);
              td->loc = bezt->vec[2];
              copy_v3_v3(td->center,
                         bezt->vec[(hide_handles || (t->around == V3D_AROUND_LOCAL_ORIGINS) ||
                                    (bezt->f2 & SELECT)) ?
                                       1 :
                                       2]);
              if (hide_handles) {
                if (bezt->f2 & SELECT) {
                  td->flag = TD_SELECTED;
                }
                else {
                  td->flag = 0;
                }
              }
              else {
                if (bezt->f3 & SELECT) {
                  td->flag = TD_SELECTED;
                }
                else {
                  td->flag = 0;
                }
              }
              td->ext = nullptr;
              td->val = nullptr;

              if (hdata == nullptr) {
                /* if the handle was not saved by the previous handle */
                hdata = initTransDataCurveHandles(td, bezt);
              }

              copy_m3_m3(td->smtx, smtx);
              copy_m3_m3(td->mtx, mtx);
              if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
                copy_m3_m3(td->axismtx, axismtx);
              }

              td++;
              tail++;
            }

            (void)hdata; /* quiet warning */
          }
        }
      }
      else {
        for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a > 0; a--, bp++) {
          if (bp->hide == 0) {
            if (is_prop_edit || (bp->f1 & SELECT)) {
              copy_v3_v3(td->iloc, bp->vec);
              td->loc = bp->vec;
              copy_v3_v3(td->center, td->loc);
              if (bp->f1 & SELECT) {
                td->flag = TD_SELECTED;
                has_any_selected |= true;
              }
              else {
                td->flag = 0;
              }
              td->ext = nullptr;

              if (ELEM(t->mode, TFM_CURVE_SHRINKFATTEN, TFM_RESIZE)) {
                td->val = &(bp->radius);
                td->ival = bp->radius;
              }
              else {
                td->val = &(bp->tilt);
                td->ival = bp->tilt;
              }

              copy_m3_m3(td->smtx, smtx);
              copy_m3_m3(td->mtx, mtx);

              if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
                if (nu->pntsv == 1) {
                  float normal[3], plane[3];

                  BKE_nurb_bpoint_calc_normal(nu, bp, normal);
                  BKE_nurb_bpoint_calc_plane(nu, bp, plane);

                  if (createSpaceNormalTangent(td->axismtx, normal, plane)) {
                    /* pass */
                  }
                  else {
                    normalize_v3(normal);
                    axis_dominant_v3_to_m3(td->axismtx, normal);
                    invert_m3(td->axismtx);
                  }
                }
              }

              td++;
              tail++;
            }
          }
        }
      }
      if (is_prop_edit && head != tail) {
        tail -= 1;
        if (!has_any_selected) {
          for (td = head; td <= tail; td++) {
            td->flag |= TD_NOTCONNECTED;
          }
        }
        bool cyclic = (nu->flagu & CU_NURB_CYCLIC) != 0;
        calc_distanceCurveVerts(head, tail, cyclic);
      }

      /* TODO: in the case of tilt and radius we can also avoid allocating the
       * initTransDataCurveHandles but for now just don't change handle types */
      if ((nu->type == CU_BEZIER) &&
          ELEM(t->mode, TFM_CURVE_SHRINKFATTEN, TFM_TILT, TFM_DUMMY) == 0) {
        /* sets the handles based on their selection,
         * do this after the data is copied to the TransData */
        BKE_nurb_handles_test(nu, !hide_handles, use_around_origins_for_handles_test);
      }
    }
  }
#undef SEL_F1
#undef SEL_F2
#undef SEL_F3
}

static void recalcData_curve(TransInfo *t)
{
  if (t->state != TRANS_CANCEL) {
    transform_snap_project_individual_apply(t);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Curve *cu = static_cast<Curve *>(tc->obedit->data);
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);
    Nurb *nu = static_cast<Nurb *>(nurbs->first);

    DEG_id_tag_update(static_cast<ID *>(tc->obedit->data), ID_RECALC_GEOMETRY);

    if (t->state == TRANS_CANCEL) {
      while (nu) {
        /* Can't do testhandlesNurb here, it messes up the h1 and h2 flags */
        BKE_nurb_handles_calc(nu);
        nu = nu->next;
      }
    }
    else {
      /* Apply clipping after so we never project past the clip plane #25423. */
      transform_convert_clip_mirror_modifier_apply(tc);

      /* Normal updating. */
      BKE_curve_dimension_update(cu);
    }
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_Curve = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*create_trans_data*/ createTransCurveVerts,
    /*recalc_data*/ recalcData_curve,
    /*special_aftertrans_update*/ nullptr,
};
