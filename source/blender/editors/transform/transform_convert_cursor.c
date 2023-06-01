/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 *
 * Instead of transforming the selection, move the 2D/3D cursor.
 */

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "transform.h"
#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name Shared 2D Cursor Utilities
 * \{ */

static void createTransCursor_2D_impl(TransInfo *t, float cursor_location[2])
{
  TransData *td;
  TransData2D *td2d;
  {
    BLI_assert(t->data_container_len == 1);
    TransDataContainer *tc = t->data_container;
    tc->data_len = 1;
    td = tc->data = MEM_callocN(sizeof(TransData), "TransTexspace");
    td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransObData2D(Cursor)");
    td->ext = tc->data_ext = MEM_callocN(sizeof(TransDataExtension), "TransCursorExt");
  }

  td->flag = TD_SELECTED;

  td2d->loc2d = cursor_location;

  /* UV coords are scaled by aspects (see #UVsToTransData). This also applies for the Cursor in the
   * UV Editor which also means that for display and when the cursor coords are flushed
   * (recalcData_cursor_image), these are converted each time. */
  td2d->loc[0] = cursor_location[0] * t->aspect[0];
  td2d->loc[1] = cursor_location[1] * t->aspect[1];
  td2d->loc[2] = 0.0f;

  copy_v3_v3(td->center, td2d->loc);

  td->ob = NULL;

  unit_m3(td->mtx);
  unit_m3(td->axismtx);
  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td2d->loc);
}

static void recalcData_cursor_2D_impl(TransInfo *t)
{
  TransDataContainer *tc = t->data_container;
  TransData *td = tc->data;
  TransData2D *td2d = tc->data_2d;
  float aspect_inv[2];

  aspect_inv[0] = 1.0f / t->aspect[0];
  aspect_inv[1] = 1.0f / t->aspect[1];

  td2d->loc2d[0] = td->loc[0] * aspect_inv[0];
  td2d->loc2d[1] = td->loc[1] * aspect_inv[1];

  DEG_id_tag_update(&t->scene->id, ID_RECALC_COPY_ON_WRITE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Cursor
 * \{ */

static void createTransCursor_image(bContext *UNUSED(C), TransInfo *t)
{
  SpaceImage *sima = t->area->spacedata.first;
  createTransCursor_2D_impl(t, sima->cursor);
}

static void recalcData_cursor_image(TransInfo *t)
{
  recalcData_cursor_2D_impl(t);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequencer Cursor
 * \{ */

static void createTransCursor_sequencer(bContext *UNUSED(C), TransInfo *t)
{
  SpaceSeq *sseq = t->area->spacedata.first;
  if (sseq->mainb != SEQ_DRAW_IMG_IMBUF) {
    return;
  }
  createTransCursor_2D_impl(t, sseq->cursor);
}

static void recalcData_cursor_sequencer(TransInfo *t)
{
  recalcData_cursor_2D_impl(t);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View 3D Cursor
 * \{ */

static void createTransCursor_view3d(bContext *UNUSED(C), TransInfo *t)
{
  TransData *td;

  Scene *scene = t->scene;
  if (ID_IS_LINKED(scene)) {
    BKE_report(t->reports, RPT_ERROR, "Linked data can't text-space transform");
    return;
  }

  View3DCursor *cursor = &scene->cursor;
  {
    BLI_assert(t->data_container_len == 1);
    TransDataContainer *tc = t->data_container;
    tc->data_len = 1;
    td = tc->data = MEM_callocN(sizeof(TransData), "TransTexspace");
    td->ext = tc->data_ext = MEM_callocN(sizeof(TransDataExtension), "TransTexspace");
  }

  td->flag = TD_SELECTED;
  copy_v3_v3(td->center, cursor->location);
  td->ob = NULL;

  unit_m3(td->mtx);
  BKE_scene_cursor_rot_to_mat3(cursor, td->axismtx);
  normalize_m3(td->axismtx);
  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  td->loc = cursor->location;
  copy_v3_v3(td->iloc, cursor->location);

  if (cursor->rotation_mode > 0) {
    td->ext->rot = cursor->rotation_euler;
    td->ext->rotAxis = NULL;
    td->ext->rotAngle = NULL;
    td->ext->quat = NULL;

    copy_v3_v3(td->ext->irot, cursor->rotation_euler);
  }
  else if (cursor->rotation_mode == ROT_MODE_AXISANGLE) {
    td->ext->rot = NULL;
    td->ext->rotAxis = cursor->rotation_axis;
    td->ext->rotAngle = &cursor->rotation_angle;
    td->ext->quat = NULL;

    td->ext->irotAngle = cursor->rotation_angle;
    copy_v3_v3(td->ext->irotAxis, cursor->rotation_axis);
  }
  else {
    td->ext->rot = NULL;
    td->ext->rotAxis = NULL;
    td->ext->rotAngle = NULL;
    td->ext->quat = cursor->rotation_quaternion;

    copy_qt_qt(td->ext->iquat, cursor->rotation_quaternion);
  }
  td->ext->rotOrder = cursor->rotation_mode;
}

static void recalcData_cursor_view3d(TransInfo *t)
{
  DEG_id_tag_update(&t->scene->id, ID_RECALC_COPY_ON_WRITE);
}

/** \} */

TransConvertTypeInfo TransConvertType_CursorImage = {
    /*flags*/ T_2D_EDIT,
    /*createTransData*/ createTransCursor_image,
    /*recalcData*/ recalcData_cursor_image,
    /*special_aftertrans_update*/ NULL,
};

TransConvertTypeInfo TransConvertType_CursorSequencer = {
    /*flags*/ T_2D_EDIT,
    /*createTransData*/ createTransCursor_sequencer,
    /*recalcData*/ recalcData_cursor_sequencer,
    /*special_aftertrans_update*/ NULL,
};

TransConvertTypeInfo TransConvertType_Cursor3D = {
    /*flags*/ 0,
    /*createTransData*/ createTransCursor_view3d,
    /*recalcData*/ recalcData_cursor_view3d,
    /*special_aftertrans_update*/ NULL,
};
