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
 * \ingroup edobj
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_collection_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_editmesh.h"
#include "BKE_lattice.h"

#include "WM_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_object.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Active Element Center
 * \{ */

bool ED_object_calc_active_center_for_editmode(Object *obedit,
                                               const bool select_only,
                                               float r_center[3])
{
  switch (obedit->type) {
    case OB_MESH: {
      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      BMEditSelection ese;

      if (BM_select_history_active_get(em->bm, &ese)) {
        BM_editselection_center(&ese, r_center);
        return true;
      }
      break;
    }
    case OB_ARMATURE: {
      bArmature *arm = obedit->data;
      EditBone *ebo = arm->act_edbone;

      if (ebo && (!select_only || (ebo->flag & (BONE_SELECTED | BONE_ROOTSEL)))) {
        copy_v3_v3(r_center, ebo->head);
        return true;
      }

      break;
    }
    case OB_CURVE:
    case OB_SURF: {
      Curve *cu = obedit->data;

      if (ED_curve_active_center(cu, r_center)) {
        return true;
      }
      break;
    }
    case OB_MBALL: {
      MetaBall *mb = obedit->data;
      MetaElem *ml_act = mb->lastelem;

      if (ml_act && (!select_only || (ml_act->flag & SELECT))) {
        copy_v3_v3(r_center, &ml_act->x);
        return true;
      }
      break;
    }
    case OB_LATTICE: {
      BPoint *actbp = BKE_lattice_active_point_get(obedit->data);

      if (actbp) {
        copy_v3_v3(r_center, actbp->vec);
        return true;
      }
      break;
    }
  }

  return false;
}

bool ED_object_calc_active_center_for_posemode(Object *ob,
                                               const bool select_only,
                                               float r_center[3])
{
  bPoseChannel *pchan = BKE_pose_channel_active(ob);
  if (pchan && (!select_only || (pchan->bone->flag & BONE_SELECTED))) {
    copy_v3_v3(r_center, pchan->pose_head);
    return true;
  }
  return false;
}

bool ED_object_calc_active_center(Object *ob, const bool select_only, float r_center[3])
{
  if (ob->mode & OB_MODE_EDIT) {
    if (ED_object_calc_active_center_for_editmode(ob, select_only, r_center)) {
      mul_m4_v3(ob->obmat, r_center);
      return true;
    }
    return false;
  }
  else if (ob->mode & OB_MODE_POSE) {
    if (ED_object_calc_active_center_for_posemode(ob, select_only, r_center)) {
      mul_m4_v3(ob->obmat, r_center);
      return true;
    }
    return false;
  }
  else {
    if (!select_only || (ob->base_flag & BASE_SELECTED)) {
      copy_v3_v3(r_center, ob->obmat[3]);
      return true;
    }
    return false;
  }
}

/** \} */
