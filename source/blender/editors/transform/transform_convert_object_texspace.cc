/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_layer.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "DNA_mesh_types.h"

#include "transform.hh"
#include "transform_snap.hh"

/* Own include. */
#include "transform_convert.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Texture Space Transform Creation
 *
 * Instead of transforming the selection, move the 2D/3D cursor.
 *
 * \{ */

static void createTransTexspace(bContext * /*C*/, TransInfo *t)
{
  ViewLayer *view_layer = t->view_layer;
  TransData *td;
  TransDataExtension *td_ext;
  Object *ob;
  ID *id;
  char *texspace_flag;

  BKE_view_layer_synced_ensure(t->scene, t->view_layer);
  ob = BKE_view_layer_active_object_get(view_layer);

  if (ob == nullptr) { /* Shouldn't logically happen, but still. */
    return;
  }

  id = static_cast<ID *>(ob->data);
  if (id == nullptr || !ELEM(GS(id->name), ID_ME, ID_CU_LEGACY, ID_MB)) {
    BKE_report(t->reports, RPT_ERROR, "Unsupported object type for texture space transform");
    return;
  }

  if (BKE_object_obdata_is_libdata(ob)) {
    BKE_report(t->reports, RPT_ERROR, "Cannot create transform on linked data");
    return;
  }

  {
    BLI_assert(t->data_container_len == 1);
    TransDataContainer *tc = t->data_container;
    tc->data_len = 1;
    td = tc->data = MEM_callocN<TransData>("TransTexspace");
    td_ext = tc->data_ext = MEM_callocN<TransDataExtension>("TransTexspace");
  }

  td->flag = TD_SELECTED;
  td->extra = ob;

  copy_m3_m4(td->mtx, ob->object_to_world().ptr());
  copy_m3_m4(td->axismtx, ob->object_to_world().ptr());
  normalize_m3(td->axismtx);
  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  if (BKE_object_obdata_texspace_get(ob, &texspace_flag, &td->loc, &td_ext->scale)) {
    ob->dtx |= OB_TEXSPACE;
    *texspace_flag &= ~ME_TEXSPACE_FLAG_AUTO;
  }

  copy_v3_v3(td->iloc, td->loc);
  copy_v3_v3(td->center, td->loc);
  copy_v3_v3(td_ext->iscale, td_ext->scale);
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
      Object *ob = static_cast<Object *>(td->extra);
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_ObjectTexSpace = {
    /*flags*/ 0,
    /*create_trans_data*/ createTransTexspace,
    /*recalc_data*/ recalcData_texspace,
    /*special_aftertrans_update*/ nullptr,
};

}  // namespace blender::ed::transform
