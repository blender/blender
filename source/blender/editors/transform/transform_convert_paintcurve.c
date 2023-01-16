/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edtransform
 */

#include "DNA_brush_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_paint.h"

#include "transform.h"
#include "transform_convert.h"

typedef struct TransDataPaintCurve {
  struct PaintCurvePoint *pcp; /* initial curve point */
  char id;
} TransDataPaintCurve;

/* -------------------------------------------------------------------- */
/** \name Paint Curve Transform Creation
 * \{ */

#define PC_IS_ANY_SEL(pc) (((pc)->bez.f1 | (pc)->bez.f2 | (pc)->bez.f3) & SELECT)

static void PaintCurveConvertHandle(
    PaintCurvePoint *pcp, int id, TransData2D *td2d, TransDataPaintCurve *tdpc, TransData *td)
{
  BezTriple *bezt = &pcp->bez;
  copy_v2_v2(td2d->loc, bezt->vec[id]);
  td2d->loc[2] = 0.0f;
  td2d->loc2d = bezt->vec[id];

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->center, bezt->vec[1]);
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;
  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  tdpc->id = id;
  tdpc->pcp = pcp;
}

static void PaintCurvePointToTransData(PaintCurvePoint *pcp,
                                       TransData *td,
                                       TransData2D *td2d,
                                       TransDataPaintCurve *tdpc)
{
  BezTriple *bezt = &pcp->bez;

  if (pcp->bez.f2 == SELECT) {
    int i;
    for (i = 0; i < 3; i++) {
      copy_v2_v2(td2d->loc, bezt->vec[i]);
      td2d->loc[2] = 0.0f;
      td2d->loc2d = bezt->vec[i];

      td->flag = 0;
      td->loc = td2d->loc;
      copy_v3_v3(td->center, bezt->vec[1]);
      copy_v3_v3(td->iloc, td->loc);

      memset(td->axismtx, 0, sizeof(td->axismtx));
      td->axismtx[2][2] = 1.0f;

      td->ext = NULL;
      td->val = NULL;
      td->flag |= TD_SELECTED;
      td->dist = 0.0;

      unit_m3(td->mtx);
      unit_m3(td->smtx);

      tdpc->id = i;
      tdpc->pcp = pcp;

      td++;
      td2d++;
      tdpc++;
    }
  }
  else {
    if (bezt->f3 & SELECT) {
      PaintCurveConvertHandle(pcp, 2, td2d, tdpc, td);
      td2d++;
      tdpc++;
      td++;
    }

    if (bezt->f1 & SELECT) {
      PaintCurveConvertHandle(pcp, 0, td2d, tdpc, td);
    }
  }
}

static void createTransPaintCurveVerts(bContext *C, TransInfo *t)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  PaintCurve *pc;
  PaintCurvePoint *pcp;
  Brush *br;
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  TransDataPaintCurve *tdpc = NULL;
  int i;
  int total = 0;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!paint || !paint->brush || !paint->brush->paint_curve) {
    return;
  }

  br = paint->brush;
  pc = br->paint_curve;

  for (pcp = pc->points, i = 0; i < pc->tot_points; i++, pcp++) {
    if (PC_IS_ANY_SEL(pcp)) {
      if (pcp->bez.f2 & SELECT) {
        total += 3;
        continue;
      }
      if (pcp->bez.f1 & SELECT) {
        total++;
      }
      if (pcp->bez.f3 & SELECT) {
        total++;
      }
    }
  }

  if (!total) {
    return;
  }

  tc->data_len = total;
  td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransData2D");
  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData");
  tc->custom.type.data = tdpc = MEM_callocN(tc->data_len * sizeof(TransDataPaintCurve),
                                            "TransDataPaintCurve");
  tc->custom.type.use_free = true;

  for (pcp = pc->points, i = 0; i < pc->tot_points; i++, pcp++) {
    if (PC_IS_ANY_SEL(pcp)) {
      PaintCurvePointToTransData(pcp, td, td2d, tdpc);

      if (pcp->bez.f2 & SELECT) {
        td += 3;
        td2d += 3;
        tdpc += 3;
      }
      else {
        if (pcp->bez.f1 & SELECT) {
          td++;
          td2d++;
          tdpc++;
        }
        if (pcp->bez.f3 & SELECT) {
          td++;
          td2d++;
          tdpc++;
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Curve Transform Flush
 * \{ */

static void flushTransPaintCurve(TransInfo *t)
{
  int i;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  TransData2D *td2d = tc->data_2d;
  TransDataPaintCurve *tdpc = tc->custom.type.data;

  for (i = 0; i < tc->data_len; i++, tdpc++, td2d++) {
    PaintCurvePoint *pcp = tdpc->pcp;
    copy_v2_v2(pcp->bez.vec[tdpc->id], td2d->loc);
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_PaintCurve = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*createTransData*/ createTransPaintCurveVerts,
    /*recalcData*/ flushTransPaintCurve,
    /*special_aftertrans_update*/ NULL,
};
