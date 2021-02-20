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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
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

void createTransTexspace(TransInfo *t)
{
  ViewLayer *view_layer = t->view_layer;
  TransData *td;
  Object *ob;
  ID *id;
  short *texflag;

  ob = OBACT(view_layer);

  if (ob == NULL) { /* Shouldn't logically happen, but still. */
    return;
  }

  id = ob->data;
  if (id == NULL || !ELEM(GS(id->name), ID_ME, ID_CU, ID_MB)) {
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
  copy_v3_v3(td->center, ob->obmat[3]);
  td->ob = ob;

  copy_m3_m4(td->mtx, ob->obmat);
  copy_m3_m4(td->axismtx, ob->obmat);
  normalize_m3(td->axismtx);
  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  if (BKE_object_obdata_texspace_get(ob, &texflag, &td->loc, &td->ext->size)) {
    ob->dtx |= OB_TEXSPACE;
    *texflag &= ~ME_AUTOSPACE;
  }

  copy_v3_v3(td->iloc, td->loc);
  copy_v3_v3(td->ext->isize, td->ext->size);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Data object
 * \{ */

/* helper for recalcData() - for object transforms, typically in the 3D view */
void recalcData_texspace(TransInfo *t)
{

  if (t->state != TRANS_CANCEL) {
    applyProject(t);
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
