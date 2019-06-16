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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edscr
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BLI_utildefines.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_paint.h"
#include "BKE_gpencil.h"
#include "BKE_layer.h"
#include "BKE_sequencer.h"
#include "BKE_workspace.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"

#include "ED_armature.h"
#include "ED_gpencil.h"
#include "ED_anim_api.h"
#include "ED_uvedit.h"

#include "WM_api.h"
#include "UI_interface.h"

#include "screen_intern.h"

const char *screen_context_dir[] = {
    "scene",
    "view_layer",
    "visible_objects",
    "selectable_objects",
    "selected_objects",
    "editable_objects",
    "selected_editable_objects",
    "objects_in_mode",
    "objects_in_mode_unique_data",
    "visible_bones",
    "editable_bones",
    "selected_bones",
    "selected_editable_bones",
    "visible_pose_bones",
    "selected_pose_bones",
    "selected_pose_bones_from_active_object",
    "active_bone",
    "active_pose_bone",
    "active_object",
    "object",
    "edit_object",
    "sculpt_object",
    "vertex_paint_object",
    "weight_paint_object",
    "image_paint_object",
    "particle_edit_object",
    "pose_object",
    "sequences",
    "selected_sequences",
    "selected_editable_sequences", /* sequencer */
    "gpencil_data",
    "gpencil_data_owner", /* grease pencil data */
    "visible_gpencil_layers",
    "editable_gpencil_layers",
    "editable_gpencil_strokes",
    "active_gpencil_layer",
    "active_gpencil_frame",
    "active_operator",
    "visible_fcurves",
    "editable_fcurves",
    "selected_visible_fcurves",
    "selected_editable_fcurves",
    "active_editable_fcurve",
    NULL,
};

int ed_screen_context(const bContext *C, const char *member, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be NULL in a lot of cases. */
  bScreen *sc = CTX_wm_screen(C);
  ScrArea *sa = CTX_wm_area(C);
  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = (view_layer && view_layer->basact) ? view_layer->basact->object : NULL;
  Object *obedit = view_layer ? OBEDIT_FROM_VIEW_LAYER(view_layer) : NULL;

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, screen_context_dir);
    return 1;
  }
  else if (CTX_data_equals(member, "scene")) {
    CTX_data_id_pointer_set(result, &scene->id);
    return 1;
  }
  else if (CTX_data_equals(member, "visible_objects")) {
    for (Base *base = view_layer->object_bases.first; base; base = base->next) {
      if (BASE_VISIBLE(v3d, base)) {
        CTX_data_id_list_add(result, &base->object->id);
      }
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return 1;
  }
  else if (CTX_data_equals(member, "selectable_objects")) {
    for (Base *base = view_layer->object_bases.first; base; base = base->next) {
      if (BASE_SELECTABLE(v3d, base)) {
        CTX_data_id_list_add(result, &base->object->id);
      }
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return 1;
  }
  else if (CTX_data_equals(member, "selected_objects")) {
    for (Base *base = view_layer->object_bases.first; base; base = base->next) {
      if (BASE_SELECTED(v3d, base)) {
        CTX_data_id_list_add(result, &base->object->id);
      }
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return 1;
  }
  else if (CTX_data_equals(member, "selected_editable_objects")) {
    for (Base *base = view_layer->object_bases.first; base; base = base->next) {
      if (BASE_SELECTED_EDITABLE(v3d, base)) {
        CTX_data_id_list_add(result, &base->object->id);
      }
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return 1;
  }
  else if (CTX_data_equals(member, "editable_objects")) {
    /* Visible + Editable, but not necessarily selected */
    for (Base *base = view_layer->object_bases.first; base; base = base->next) {
      if (BASE_EDITABLE(v3d, base)) {
        CTX_data_id_list_add(result, &base->object->id);
      }
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return 1;
  }
  else if (CTX_data_equals(member, "objects_in_mode")) {
    if (obact && (obact->mode != OB_MODE_OBJECT)) {
      FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, v3d, obact->type, obact->mode, ob_iter) {
        CTX_data_id_list_add(result, &ob_iter->id);
      }
      FOREACH_OBJECT_IN_MODE_END;
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return 1;
  }
  else if (CTX_data_equals(member, "objects_in_mode_unique_data")) {
    if (obact && (obact->mode != OB_MODE_OBJECT)) {
      FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, v3d, obact->type, obact->mode, ob_iter) {
        ob_iter->id.tag |= LIB_TAG_DOIT;
      }
      FOREACH_OBJECT_IN_MODE_END;
      FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, v3d, obact->type, obact->mode, ob_iter) {
        if (ob_iter->id.tag & LIB_TAG_DOIT) {
          ob_iter->id.tag &= ~LIB_TAG_DOIT;
          CTX_data_id_list_add(result, &ob_iter->id);
        }
      }
      FOREACH_OBJECT_IN_MODE_END;
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return 1;
  }
  else if (CTX_data_equals(member, "visible_bones") || CTX_data_equals(member, "editable_bones")) {
    bArmature *arm = (obedit && obedit->type == OB_ARMATURE) ? obedit->data : NULL;
    EditBone *ebone, *flipbone = NULL;
    const bool editable_bones = CTX_data_equals(member, "editable_bones");

    if (arm && arm->edbo) {
      uint objects_len;
      Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
          view_layer, CTX_wm_view3d(C), &objects_len);
      for (uint i = 0; i < objects_len; i++) {
        Object *ob = objects[i];
        arm = ob->data;

        /* Attention: X-Axis Mirroring is also handled here... */
        for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
          /* first and foremost, bone must be visible and selected */
          if (EBONE_VISIBLE(arm, ebone)) {
            /* Get 'x-axis mirror equivalent' bone if the X-Axis Mirroring option is enabled
             * so that most users of this data don't need to explicitly check for it themselves.
             *
             * We need to make sure that these mirrored copies are not selected, otherwise some
             * bones will be operated on twice.
             */
            if (arm->flag & ARM_MIRROR_EDIT) {
              flipbone = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
            }

            /* if we're filtering for editable too, use the check for that instead,
             * as it has selection check too */
            if (editable_bones) {
              /* only selected + editable */
              if (EBONE_EDITABLE(ebone)) {
                CTX_data_list_add(result, &arm->id, &RNA_EditBone, ebone);

                if ((flipbone) && !(flipbone->flag & BONE_SELECTED)) {
                  CTX_data_list_add(result, &arm->id, &RNA_EditBone, flipbone);
                }
              }
            }
            else {
              /* only include bones if visible */
              CTX_data_list_add(result, &arm->id, &RNA_EditBone, ebone);

              if ((flipbone) && EBONE_VISIBLE(arm, flipbone) == 0) {
                CTX_data_list_add(result, &arm->id, &RNA_EditBone, flipbone);
              }
            }
          }
        }
      }
      MEM_freeN(objects);

      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "selected_bones") ||
           CTX_data_equals(member, "selected_editable_bones")) {
    bArmature *arm = (obedit && obedit->type == OB_ARMATURE) ? obedit->data : NULL;
    EditBone *ebone, *flipbone = NULL;
    const bool selected_editable_bones = CTX_data_equals(member, "selected_editable_bones");

    if (arm && arm->edbo) {
      uint objects_len;
      Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
          view_layer, CTX_wm_view3d(C), &objects_len);
      for (uint i = 0; i < objects_len; i++) {
        Object *ob = objects[i];
        arm = ob->data;

        /* Attention: X-Axis Mirroring is also handled here... */
        for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
          /* first and foremost, bone must be visible and selected */
          if (EBONE_VISIBLE(arm, ebone) && (ebone->flag & BONE_SELECTED)) {
            /* Get 'x-axis mirror equivalent' bone if the X-Axis Mirroring option is enabled
             * so that most users of this data don't need to explicitly check for it themselves.
             *
             * We need to make sure that these mirrored copies are not selected, otherwise some
             * bones will be operated on twice.
             */
            if (arm->flag & ARM_MIRROR_EDIT) {
              flipbone = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
            }

            /* if we're filtering for editable too, use the check for that instead,
             * as it has selection check too */
            if (selected_editable_bones) {
              /* only selected + editable */
              if (EBONE_EDITABLE(ebone)) {
                CTX_data_list_add(result, &arm->id, &RNA_EditBone, ebone);

                if ((flipbone) && !(flipbone->flag & BONE_SELECTED)) {
                  CTX_data_list_add(result, &arm->id, &RNA_EditBone, flipbone);
                }
              }
            }
            else {
              /* only include bones if selected */
              CTX_data_list_add(result, &arm->id, &RNA_EditBone, ebone);

              if ((flipbone) && !(flipbone->flag & BONE_SELECTED)) {
                CTX_data_list_add(result, &arm->id, &RNA_EditBone, flipbone);
              }
            }
          }
        }
      }
      MEM_freeN(objects);

      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "visible_pose_bones")) {
    Object *obpose = BKE_object_pose_armature_get(obact);
    if (obpose && obpose->pose && obpose->data) {
      if (obpose != obact) {
        FOREACH_PCHAN_VISIBLE_IN_OBJECT_BEGIN (obpose, pchan) {
          CTX_data_list_add(result, &obpose->id, &RNA_PoseBone, pchan);
        }
        FOREACH_PCHAN_SELECTED_IN_OBJECT_END;
      }
      else if (obact->mode & OB_MODE_POSE) {
        FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob_iter) {
          FOREACH_PCHAN_VISIBLE_IN_OBJECT_BEGIN (ob_iter, pchan) {
            CTX_data_list_add(result, &ob_iter->id, &RNA_PoseBone, pchan);
          }
          FOREACH_PCHAN_VISIBLE_IN_OBJECT_END;
        }
        FOREACH_OBJECT_IN_MODE_END;
      }
      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "selected_pose_bones")) {
    Object *obpose = BKE_object_pose_armature_get(obact);
    if (obpose && obpose->pose && obpose->data) {
      if (obpose != obact) {
        FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (obpose, pchan) {
          CTX_data_list_add(result, &obpose->id, &RNA_PoseBone, pchan);
        }
        FOREACH_PCHAN_SELECTED_IN_OBJECT_END;
      }
      else if (obact->mode & OB_MODE_POSE) {
        FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob_iter) {
          FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (ob_iter, pchan) {
            CTX_data_list_add(result, &ob_iter->id, &RNA_PoseBone, pchan);
          }
          FOREACH_PCHAN_SELECTED_IN_OBJECT_END;
        }
        FOREACH_OBJECT_IN_MODE_END;
      }
      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "selected_pose_bones_from_active_object")) {
    Object *obpose = BKE_object_pose_armature_get(obact);
    if (obpose && obpose->pose && obpose->data) {
      if (obpose != obact) {
        FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (obpose, pchan) {
          CTX_data_list_add(result, &obpose->id, &RNA_PoseBone, pchan);
        }
        FOREACH_PCHAN_SELECTED_IN_OBJECT_END;
      }
      else if (obact->mode & OB_MODE_POSE) {
        FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (obact, pchan) {
          CTX_data_list_add(result, &obact->id, &RNA_PoseBone, pchan);
        }
        FOREACH_PCHAN_SELECTED_IN_OBJECT_END;
      }
      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "active_bone")) {
    if (obact && obact->type == OB_ARMATURE) {
      bArmature *arm = obact->data;
      if (arm->edbo) {
        if (arm->act_edbone) {
          CTX_data_pointer_set(result, &arm->id, &RNA_EditBone, arm->act_edbone);
          return 1;
        }
      }
      else {
        if (arm->act_bone) {
          CTX_data_pointer_set(result, &arm->id, &RNA_Bone, arm->act_bone);
          return 1;
        }
      }
    }
  }
  else if (CTX_data_equals(member, "active_pose_bone")) {
    bPoseChannel *pchan;
    Object *obpose = BKE_object_pose_armature_get(obact);

    pchan = BKE_pose_channel_active(obpose);
    if (pchan) {
      CTX_data_pointer_set(result, &obpose->id, &RNA_PoseBone, pchan);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "active_object")) {
    if (obact) {
      CTX_data_id_pointer_set(result, &obact->id);
    }

    return 1;
  }
  else if (CTX_data_equals(member, "object")) {
    if (obact) {
      CTX_data_id_pointer_set(result, &obact->id);
    }

    return 1;
  }
  else if (CTX_data_equals(member, "edit_object")) {
    /* convenience for now, 1 object per scene in editmode */
    if (obedit) {
      CTX_data_id_pointer_set(result, &obedit->id);
    }

    return 1;
  }
  else if (CTX_data_equals(member, "sculpt_object")) {
    if (obact && (obact->mode & OB_MODE_SCULPT)) {
      CTX_data_id_pointer_set(result, &obact->id);
    }

    return 1;
  }
  else if (CTX_data_equals(member, "vertex_paint_object")) {
    if (obact && (obact->mode & OB_MODE_VERTEX_PAINT)) {
      CTX_data_id_pointer_set(result, &obact->id);
    }

    return 1;
  }
  else if (CTX_data_equals(member, "weight_paint_object")) {
    if (obact && (obact->mode & OB_MODE_WEIGHT_PAINT)) {
      CTX_data_id_pointer_set(result, &obact->id);
    }

    return 1;
  }
  else if (CTX_data_equals(member, "image_paint_object")) {
    if (obact && (obact->mode & OB_MODE_TEXTURE_PAINT)) {
      CTX_data_id_pointer_set(result, &obact->id);
    }

    return 1;
  }
  else if (CTX_data_equals(member, "particle_edit_object")) {
    if (obact && (obact->mode & OB_MODE_PARTICLE_EDIT)) {
      CTX_data_id_pointer_set(result, &obact->id);
    }

    return 1;
  }
  else if (CTX_data_equals(member, "pose_object")) {
    Object *obpose = BKE_object_pose_armature_get(obact);
    if (obpose) {
      CTX_data_id_pointer_set(result, &obpose->id);
    }
    return 1;
  }
  else if (CTX_data_equals(member, "sequences")) {
    Editing *ed = BKE_sequencer_editing_get(scene, false);
    if (ed) {
      Sequence *seq;
      for (seq = ed->seqbasep->first; seq; seq = seq->next) {
        CTX_data_list_add(result, &scene->id, &RNA_Sequence, seq);
      }
      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "selected_sequences")) {
    Editing *ed = BKE_sequencer_editing_get(scene, false);
    if (ed) {
      Sequence *seq;
      for (seq = ed->seqbasep->first; seq; seq = seq->next) {
        if (seq->flag & SELECT) {
          CTX_data_list_add(result, &scene->id, &RNA_Sequence, seq);
        }
      }
      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "selected_editable_sequences")) {
    Editing *ed = BKE_sequencer_editing_get(scene, false);
    if (ed) {
      Sequence *seq;
      for (seq = ed->seqbasep->first; seq; seq = seq->next) {
        if (seq->flag & SELECT && !(seq->flag & SEQ_LOCK)) {
          CTX_data_list_add(result, &scene->id, &RNA_Sequence, seq);
        }
      }
      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "gpencil_data")) {
    /* FIXME: for some reason, CTX_data_active_object(C) returns NULL when called from these
     * situations (as outlined above - see Campbell's #ifdefs).
     * That causes the get_active function to fail when called from context.
     * For that reason, we end up using an alternative where we pass everything in!
     */
    bGPdata *gpd = ED_gpencil_data_get_active_direct((ID *)sc, sa, scene, obact);

    if (gpd) {
      CTX_data_id_pointer_set(result, &gpd->id);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "gpencil_data_owner")) {
    /* Pointer to which data/datablock owns the reference to the Grease Pencil data being used
     * (as gpencil_data).
     * XXX: see comment for gpencil_data case.
     */
    bGPdata **gpd_ptr = NULL;
    PointerRNA ptr;

    /* get pointer to Grease Pencil Data */
    gpd_ptr = ED_gpencil_data_get_pointers_direct((ID *)sc, sa, scene, obact, &ptr);

    if (gpd_ptr) {
      CTX_data_pointer_set(result, ptr.id.data, ptr.type, ptr.data);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "active_gpencil_layer")) {
    /* XXX: see comment for gpencil_data case... */
    bGPdata *gpd = ED_gpencil_data_get_active_direct((ID *)sc, sa, scene, obact);

    if (gpd) {
      bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);

      if (gpl) {
        CTX_data_pointer_set(result, &gpd->id, &RNA_GPencilLayer, gpl);
        return 1;
      }
    }
  }
  else if (CTX_data_equals(member, "active_gpencil_frame")) {
    /* XXX: see comment for gpencil_data case... */
    bGPdata *gpd = ED_gpencil_data_get_active_direct((ID *)sc, sa, scene, obact);

    if (gpd) {
      bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);

      if (gpl) {
        CTX_data_pointer_set(result, &gpd->id, &RNA_GPencilLayer, gpl->actframe);
        return 1;
      }
    }
  }
  else if (CTX_data_equals(member, "visible_gpencil_layers")) {
    /* XXX: see comment for gpencil_data case... */
    bGPdata *gpd = ED_gpencil_data_get_active_direct((ID *)sc, sa, scene, obact);

    if (gpd) {
      bGPDlayer *gpl;

      for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
        if ((gpl->flag & GP_LAYER_HIDE) == 0) {
          CTX_data_list_add(result, &gpd->id, &RNA_GPencilLayer, gpl);
        }
      }
      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "editable_gpencil_layers")) {
    /* XXX: see comment for gpencil_data case... */
    bGPdata *gpd = ED_gpencil_data_get_active_direct((ID *)sc, sa, scene, obact);

    if (gpd) {
      bGPDlayer *gpl;

      for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
        if (gpencil_layer_is_editable(gpl)) {
          CTX_data_list_add(result, &gpd->id, &RNA_GPencilLayer, gpl);
        }
      }
      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "editable_gpencil_strokes")) {
    /* XXX: see comment for gpencil_data case... */
    bGPdata *gpd = ED_gpencil_data_get_active_direct((ID *)sc, sa, scene, obact);
    const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);

    if (gpd) {
      bGPDlayer *gpl;

      for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
        if (gpencil_layer_is_editable(gpl) && (gpl->actframe)) {
          bGPDframe *gpf;
          bGPDstroke *gps;
          bGPDframe *init_gpf = gpl->actframe;
          if (is_multiedit) {
            init_gpf = gpl->frames.first;
          }

          for (gpf = init_gpf; gpf; gpf = gpf->next) {
            if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
              for (gps = gpf->strokes.first; gps; gps = gps->next) {
                if (ED_gpencil_stroke_can_use_direct(sa, gps)) {
                  /* check if the color is editable */
                  if (ED_gpencil_stroke_color_use(obact, gpl, gps) == false) {
                    continue;
                  }

                  CTX_data_list_add(result, &gpd->id, &RNA_GPencilStroke, gps);
                }
              }
            }
            /* if not multiedit out of loop */
            if (!is_multiedit) {
              break;
            }
          }
        }
      }
      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "active_operator")) {
    wmOperator *op = NULL;

    SpaceFile *sfile = CTX_wm_space_file(C);
    if (sfile) {
      op = sfile->op;
    }
    else if ((op = UI_context_active_operator_get(C))) {
      /* do nothing */
    }
    else {
      /* note, this checks poll, could be a problem, but this also
       * happens for the toolbar */
      op = WM_operator_last_redo(C);
    }
    /* TODO, get the operator from popup's */

    if (op && op->ptr) {
      CTX_data_pointer_set(result, NULL, &RNA_Operator, op);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "editable_fcurves") ||
           CTX_data_equals(member, "visible_fcurves") ||
           CTX_data_equals(member, "selected_editable_fcurves") ||
           CTX_data_equals(member, "selected_visible_fcurves")) {
    bAnimContext ac;

    if (ANIM_animdata_get_context(C, &ac) && ELEM(ac.spacetype, SPACE_ACTION, SPACE_GRAPH)) {
      ListBase anim_data = {NULL, NULL};

      int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS) |
                   (ac.spacetype == SPACE_GRAPH ? ANIMFILTER_CURVE_VISIBLE :
                                                  ANIMFILTER_LIST_VISIBLE);

      if (strstr(member, "editable_")) {
        filter |= ANIMFILTER_FOREDIT;
      }
      if (STRPREFIX(member, "selected_")) {
        filter |= ANIMFILTER_SEL;
      }

      ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

      for (bAnimListElem *ale = anim_data.first; ale; ale = ale->next) {
        if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
          CTX_data_list_add(result, ale->fcurve_owner_id, &RNA_FCurve, ale->data);
        }
      }

      ANIM_animdata_freelist(&anim_data);

      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      return 1;
    }
  }
  else if (CTX_data_equals(member, "active_editable_fcurve")) {
    bAnimContext ac;

    if (ANIM_animdata_get_context(C, &ac) && ELEM(ac.spacetype, SPACE_GRAPH)) {
      ListBase anim_data = {NULL, NULL};

      int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ACTIVE | ANIMFILTER_FOREDIT |
                    ANIMFILTER_CURVE_VISIBLE);

      ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

      for (bAnimListElem *ale = anim_data.first; ale; ale = ale->next) {
        if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
          CTX_data_pointer_set(result, ale->fcurve_owner_id, &RNA_FCurve, ale->data);
          break;
        }
      }

      ANIM_animdata_freelist(&anim_data);
      return 1;
    }
  }
  else {
    return 0; /* not found */
  }

  return -1; /* found but not available */
}
