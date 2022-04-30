/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 *
 * Intended for use by `paint_vertex.c` & `paint_vertex_color_ops.c`.
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math_base.h"
#include "BLI_math_color.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "BKE_context.h"
#include "BKE_mesh.h"

#include "DEG_depsgraph.h"

#include "ED_mesh.h"

#include "paint_intern.h" /* own include */

#define EPS_SATURATION 0.0005f

bool ED_vpaint_color_transform(struct Object *ob,
                               VPaintTransform_Callback vpaint_tx_fn,
                               const void *user_data)
{
  Mesh *me;
  const MPoly *mp;
  int i, j;

  if (((me = BKE_mesh_from_object(ob)) == NULL) || (ED_mesh_color_ensure(me, NULL) == false)) {
    return false;
  }

  const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  mp = me->mpoly;
  for (i = 0; i < me->totpoly; i++, mp++) {
    MLoopCol *lcol = me->mloopcol + mp->loopstart;

    if (use_face_sel && !(mp->flag & ME_FACE_SEL)) {
      continue;
    }

    j = 0;
    do {
      uint vidx = me->mloop[mp->loopstart + j].v;
      if (!(use_vert_sel && !(me->mvert[vidx].flag & SELECT))) {
        float col_mix[3];
        rgb_uchar_to_float(col_mix, &lcol->r);

        vpaint_tx_fn(col_mix, user_data, col_mix);

        rgb_float_to_uchar(&lcol->r, col_mix);
      }
      lcol++;
      j++;
    } while (j < mp->totloop);
  }

  /* remove stale me->mcol, will be added later */
  BKE_mesh_tessface_clear(me);

  DEG_id_tag_update(&me->id, 0);

  return true;
}
