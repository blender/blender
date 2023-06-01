/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DNA_mesh_types.h"

#include "transform.h"
#include "transform_snap.h"

/* Own include. */
#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name Texture Space Transform Creation
 *
 * Instead of transforming the selection, move the 2D/3D cursor.
 *
 * \{ */

static void createTransTexspace(bContext *UNUSED(C), TransInfo *t)
{
  ViewLayer *view_layer = t->view_layer;
  TransData *td;
  Object *ob;
  ID *id;
  char *texspace_flag;

  BKE_view_layer_synced_ensure(t->scene, t->view_layer);
  ob = BKE_view_layer_active_object_get(view_layer);

  if (ob == NULL) { /* Shouldn't logically happen, but still. */
    return;
  }

  id = ob->data;
  if (id == NULL || !ELEM(GS(id->name), ID_ME, ID_CU_LEGACY, ID_MB)) {
    BKE_report(t->reports, RPT_ERROR, "Unsupported object type for text-space transform");
    return;
  }

  if (BKE_object_obdata_is_libdata(ob)) {
    BKE_report(t->reports, RPT_ERROR, "Linked data can't text-space transform");
    return;
  }

  {
    BLI_assert(t->data_container_len == 1);
    TransDataContainer *tc = t->data_container;
    tc->data_len = 1;
    td = tc->data = MEM_callocN(sizeof(TransData), "TransTexspace");
    td->ext = tc->data_ext = MEM_callocN(sizeof(TransDataExtension), "TransTexspace");
  }

  td->flag = TD_SELECTED;
  td->ob = ob;

  copy_m3_m4(td->mtx, ob->object_to_world);
  copy_m3_m4(td->axismtx, ob->object_to_world);
  normalize_m3(td->axismtx);
  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  if (BKE_object_obdata_texspace_get(ob, &texspace_flag, &td->loc, &td->ext->size)) {
    ob->dtx |= OB_TEXSPACE;
    *texspace_flag &= ~ME_TEXSPACE_FLAG_AUTO;
  }

  copy_v3_v3(td->iloc, td->loc);
  copy_v3_v3(td->center, td->loc);
  copy_v3_v3(td->ext->isize, td->ext->size);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Data object
 * \{ */

static void recalcData_texspace(TransInfo *t)
{

  if (t->state != TRANS_CANCEL) {
    transform_snap_project_individual_apply(t);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;

    for (int i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }
      DEG_id_tag_update(&td->ob->id, ID_RECALC_GEOMETRY);
    }
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_ObjectTexSpace = {
    /*flags*/ 0,
    /*createTransData*/ createTransTexspace,
    /*recalcData*/ recalcData_texspace,
    /*special_aftertrans_update*/ NULL,
};
