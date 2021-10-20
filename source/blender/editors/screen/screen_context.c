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

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_tracking.h"

#include "RNA_access.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_clip.h"
#include "ED_gpencil.h"

#include "SEQ_select.h"
#include "SEQ_sequencer.h"

#include "UI_interface.h"
#include "WM_api.h"

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
    "active_sequence_strip",
    "sequences",
    "selected_sequences",
    "selected_editable_sequences", /* sequencer */
    "active_nla_track",
    "active_nla_strip",
    "selected_nla_strips", /* nla editor */
    "selected_movieclip_tracks",
    "gpencil_data",
    "gpencil_data_owner", /* grease pencil data */
    "annotation_data",
    "annotation_data_owner",
    "visible_gpencil_layers",
    "editable_gpencil_layers",
    "editable_gpencil_strokes",
    "active_gpencil_layer",
    "active_gpencil_frame",
    "active_annotation_layer",
    "active_operator",
    "visible_fcurves",
    "editable_fcurves",
    "selected_visible_fcurves",
    "selected_editable_fcurves",
    "active_editable_fcurve",
    "selected_editable_keyframes",
    "ui_list",
    "asset_library_ref",
    NULL,
};

/* Each function `screen_ctx_XXX()` will be called when the screen context "XXX" is requested.
 * ensure_ed_screen_context_functions() is responsible for creating the hash map from context
 * member name to function. */

static eContextResult screen_ctx_scene(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = WM_window_get_active_scene(win);
  CTX_data_id_pointer_set(result, &scene->id);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_visible_objects(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be NULL in a lot of cases. */
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);

  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    if (BASE_VISIBLE(v3d, base)) {
      CTX_data_id_list_add(result, &base->object->id);
    }
  }
  CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_selectable_objects(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be NULL in a lot of cases. */
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);

  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    if (BASE_SELECTABLE(v3d, base)) {
      CTX_data_id_list_add(result, &base->object->id);
    }
  }
  CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_selected_objects(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be NULL in a lot of cases. */
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);

  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    if (BASE_SELECTED(v3d, base)) {
      CTX_data_id_list_add(result, &base->object->id);
    }
  }
  CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_selected_editable_objects(const bContext *C,
                                                           bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be NULL in a lot of cases. */
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);

  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    if (BASE_SELECTED_EDITABLE(v3d, base)) {
      CTX_data_id_list_add(result, &base->object->id);
    }
  }
  CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_editable_objects(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be NULL in a lot of cases. */
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);

  /* Visible + Editable, but not necessarily selected */
  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    if (BASE_EDITABLE(v3d, base)) {
      CTX_data_id_list_add(result, &base->object->id);
    }
  }
  CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_objects_in_mode(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be NULL in a lot of cases. */
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;

  if (obact && (obact->mode != OB_MODE_OBJECT)) {
    FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, v3d, obact->type, obact->mode, ob_iter) {
      CTX_data_id_list_add(result, &ob_iter->id);
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_objects_in_mode_unique_data(const bContext *C,
                                                             bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be NULL in a lot of cases. */
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;

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
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_visible_or_editable_bones_(const bContext *C,
                                                            bContextDataResult *result,
                                                            const bool editable_bones)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);

  bArmature *arm = (obedit && obedit->type == OB_ARMATURE) ? obedit->data : NULL;
  EditBone *flipbone = NULL;

  if (arm && arm->edbo) {
    uint objects_len;
    Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        view_layer, CTX_wm_view3d(C), &objects_len);
    for (uint i = 0; i < objects_len; i++) {
      Object *ob = objects[i];
      arm = ob->data;

      /* Attention: X-Axis Mirroring is also handled here... */
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
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
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_visible_bones(const bContext *C, bContextDataResult *result)
{
  return screen_ctx_visible_or_editable_bones_(C, result, false);
}
static eContextResult screen_ctx_editable_bones(const bContext *C, bContextDataResult *result)
{
  return screen_ctx_visible_or_editable_bones_(C, result, true);
}
static eContextResult screen_ctx_selected_bones_(const bContext *C,
                                                 bContextDataResult *result,
                                                 const bool selected_editable_bones)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);
  bArmature *arm = (obedit && obedit->type == OB_ARMATURE) ? obedit->data : NULL;
  EditBone *flipbone = NULL;

  if (arm && arm->edbo) {
    uint objects_len;
    Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        view_layer, CTX_wm_view3d(C), &objects_len);
    for (uint i = 0; i < objects_len; i++) {
      Object *ob = objects[i];
      arm = ob->data;

      /* Attention: X-Axis Mirroring is also handled here... */
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
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
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_bones(const bContext *C, bContextDataResult *result)
{
  return screen_ctx_selected_bones_(C, result, false);
}
static eContextResult screen_ctx_selected_editable_bones(const bContext *C,
                                                         bContextDataResult *result)
{
  return screen_ctx_selected_bones_(C, result, true);
}
static eContextResult screen_ctx_visible_pose_bones(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be NULL in a lot of cases. */
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
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
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_pose_bones(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be NULL in a lot of cases. */
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
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
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_pose_bones_from_active_object(const bContext *C,
                                                                        bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
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
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_bone(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  if (obact && obact->type == OB_ARMATURE) {
    bArmature *arm = obact->data;
    if (arm->edbo) {
      if (arm->act_edbone) {
        CTX_data_pointer_set(result, &arm->id, &RNA_EditBone, arm->act_edbone);
        return CTX_RESULT_OK;
      }
    }
    else {
      if (arm->act_bone) {
        CTX_data_pointer_set(result, &arm->id, &RNA_Bone, arm->act_bone);
        return CTX_RESULT_OK;
      }
    }
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_pose_bone(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  Object *obpose = BKE_object_pose_armature_get(obact);

  bPoseChannel *pchan = BKE_pose_channel_active(obpose);
  if (pchan) {
    CTX_data_pointer_set(result, &obpose->id, &RNA_PoseBone, pchan);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;

  if (obact) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;

  if (obact) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_edit_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);
  /* convenience for now, 1 object per scene in editmode */
  if (obedit) {
    CTX_data_id_pointer_set(result, &obedit->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_sculpt_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;

  if (obact && (obact->mode & OB_MODE_SCULPT)) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_vertex_paint_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  if (obact && (obact->mode & OB_MODE_VERTEX_PAINT)) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_weight_paint_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  if (obact && (obact->mode & OB_MODE_ALL_WEIGHT_PAINT)) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_image_paint_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  if (obact && (obact->mode & OB_MODE_TEXTURE_PAINT)) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_particle_edit_object(const bContext *C,
                                                      bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  if (obact && (obact->mode & OB_MODE_PARTICLE_EDIT)) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_pose_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  Object *obpose = BKE_object_pose_armature_get(obact);
  if (obpose) {
    CTX_data_id_pointer_set(result, &obpose->id);
  }
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_active_sequence_strip(const bContext *C,
                                                       bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = WM_window_get_active_scene(win);
  Sequence *seq = SEQ_select_active_get(scene);
  if (seq) {
    CTX_data_pointer_set(result, &scene->id, &RNA_Sequence, seq);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_sequences(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = WM_window_get_active_scene(win);
  Editing *ed = SEQ_editing_get(scene);
  if (ed) {
    LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
      CTX_data_list_add(result, &scene->id, &RNA_Sequence, seq);
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_sequences(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = WM_window_get_active_scene(win);
  Editing *ed = SEQ_editing_get(scene);
  if (ed) {
    LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
      if (seq->flag & SELECT) {
        CTX_data_list_add(result, &scene->id, &RNA_Sequence, seq);
      }
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_editable_sequences(const bContext *C,
                                                             bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = WM_window_get_active_scene(win);
  Editing *ed = SEQ_editing_get(scene);
  if (ed) {
    LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
      if (seq->flag & SELECT && !(seq->flag & SEQ_LOCK)) {
        CTX_data_list_add(result, &scene->id, &RNA_Sequence, seq);
      }
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_nla_track(const bContext *C, bContextDataResult *result)
{
  PointerRNA ptr;
  if (ANIM_nla_context_track_ptr(C, &ptr)) {
    CTX_data_pointer_set_ptr(result, &ptr);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_nla_strip(const bContext *C, bContextDataResult *result)
{
  PointerRNA ptr;
  if (ANIM_nla_context_strip_ptr(C, &ptr)) {
    CTX_data_pointer_set_ptr(result, &ptr);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_nla_strips(const bContext *C, bContextDataResult *result)
{
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) != 0) {
    ListBase anim_data = {NULL, NULL};

    ANIM_animdata_filter(&ac, &anim_data, ANIMFILTER_DATA_VISIBLE, ac.data, ac.datatype);
    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      if (ale->datatype != ALE_NLASTRIP) {
        continue;
      }
      NlaTrack *nlt = (NlaTrack *)ale->data;
      LISTBASE_FOREACH (NlaStrip *, strip, &nlt->strips) {
        if (strip->flag & NLASTRIP_FLAG_SELECT) {
          CTX_data_list_add(result, ale->id, &RNA_NlaStrip, strip);
        }
      }
    }
    ANIM_animdata_freelist(&anim_data);

    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_movieclip_tracks(const bContext *C,
                                                           bContextDataResult *result)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  if (space_clip == NULL) {
    return CTX_RESULT_NO_DATA;
  }
  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  if (clip == NULL) {
    return CTX_RESULT_NO_DATA;
  }
  MovieTracking *tracking = &clip->tracking;
  if (tracking == NULL) {
    return CTX_RESULT_NO_DATA;
  }

  ListBase *tracks_list = BKE_tracking_get_active_tracks(tracking);
  LISTBASE_FOREACH (MovieTrackingTrack *, track, tracks_list) {
    if (!TRACK_SELECTED(track)) {
      continue;
    }
    CTX_data_list_add(result, &clip->id, &RNA_MovieTrackingTrack, track);
  }

  CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_gpencil_data(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  /* FIXME: for some reason, CTX_data_active_object(C) returns NULL when called from these
   * situations (as outlined above - see Campbell's #ifdefs).
   * That causes the get_active function to fail when called from context.
   * For that reason, we end up using an alternative where we pass everything in!
   */
  bGPdata *gpd = ED_gpencil_data_get_active_direct(area, obact);

  if (gpd) {
    CTX_data_id_pointer_set(result, &gpd->id);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_gpencil_data_owner(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;

  /* Pointer to which data/datablock owns the reference to the Grease Pencil data being used
   * (as gpencil_data). */
  PointerRNA ptr;
  bGPdata **gpd_ptr = ED_gpencil_data_get_pointers_direct(area, obact, &ptr);

  if (gpd_ptr) {
    CTX_data_pointer_set_ptr(result, &ptr);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_annotation_data(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = WM_window_get_active_scene(win);
  bGPdata *gpd = ED_annotation_data_get_active_direct((ID *)screen, area, scene);

  if (gpd) {
    CTX_data_id_pointer_set(result, &gpd->id);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_annotation_data_owner(const bContext *C,
                                                       bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = WM_window_get_active_scene(win);

  /* Pointer to which data/datablock owns the reference to the Grease Pencil data being used. */
  PointerRNA ptr;
  bGPdata **gpd_ptr = ED_annotation_data_get_pointers_direct((ID *)screen, area, scene, &ptr);

  if (gpd_ptr) {
    CTX_data_pointer_set_ptr(result, &ptr);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_gpencil_layer(const bContext *C,
                                                      bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  bGPdata *gpd = ED_gpencil_data_get_active_direct(area, obact);

  if (gpd) {
    bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

    if (gpl) {
      CTX_data_pointer_set(result, &gpd->id, &RNA_GPencilLayer, gpl);
      return CTX_RESULT_OK;
    }
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_annotation_layer(const bContext *C,
                                                         bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = WM_window_get_active_scene(win);
  bGPdata *gpd = ED_annotation_data_get_active_direct((ID *)screen, area, scene);

  if (gpd) {
    bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

    if (gpl) {
      CTX_data_pointer_set(result, &gpd->id, &RNA_GPencilLayer, gpl);
      return CTX_RESULT_OK;
    }
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_gpencil_frame(const bContext *C,
                                                      bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  bGPdata *gpd = ED_gpencil_data_get_active_direct(area, obact);

  if (gpd) {
    bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

    if (gpl) {
      CTX_data_pointer_set(result, &gpd->id, &RNA_GPencilLayer, gpl->actframe);
      return CTX_RESULT_OK;
    }
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_visible_gpencil_layers(const bContext *C,
                                                        bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  bGPdata *gpd = ED_gpencil_data_get_active_direct(area, obact);

  if (gpd) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if ((gpl->flag & GP_LAYER_HIDE) == 0) {
        CTX_data_list_add(result, &gpd->id, &RNA_GPencilLayer, gpl);
      }
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_editable_gpencil_layers(const bContext *C,
                                                         bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;
  bGPdata *gpd = ED_gpencil_data_get_active_direct(area, obact);

  if (gpd) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if (BKE_gpencil_layer_is_editable(gpl)) {
        CTX_data_list_add(result, &gpd->id, &RNA_GPencilLayer, gpl);
      }
    }
    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_editable_gpencil_strokes(const bContext *C,
                                                          bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = view_layer->basact ? view_layer->basact->object : NULL;

  bGPdata *gpd = ED_gpencil_data_get_active_direct(area, obact);
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);

  if (gpd == NULL) {
    return CTX_RESULT_NO_DATA;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe)) {
      bGPDframe *gpf;
      bGPDframe *init_gpf = gpl->actframe;
      if (is_multiedit) {
        init_gpf = gpl->frames.first;
      }

      for (gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
          LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
            if (ED_gpencil_stroke_can_use_direct(area, gps)) {
              /* check if the color is editable */
              if (ED_gpencil_stroke_material_editable(obact, gpl, gps) == false) {
                continue;
              }

              CTX_data_list_add(result, &gpd->id, &RNA_GPencilStroke, gps);
            }
          }
        }
        /* If not multi-edit out of loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_active_operator(const bContext *C, bContextDataResult *result)
{
  wmOperator *op = NULL;

  SpaceFile *sfile = CTX_wm_space_file(C);
  if (sfile) {
    op = sfile->op;
  }
  else if ((op = UI_context_active_operator_get(C))) {
    /* do nothing */
  }
  else {
    /* NOTE: this checks poll, could be a problem, but this also
     * happens for the toolbar */
    op = WM_operator_last_redo(C);
  }
  /* TODO: get the operator from popup's. */

  if (op && op->ptr) {
    CTX_data_pointer_set(result, NULL, &RNA_Operator, op);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_sel_edit_fcurves_(const bContext *C,
                                                   bContextDataResult *result,
                                                   const int extra_filter)
{
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) && ELEM(ac.spacetype, SPACE_ACTION, SPACE_GRAPH)) {
    ListBase anim_data = {NULL, NULL};

    int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS) |
                 (ac.spacetype == SPACE_GRAPH ? ANIMFILTER_CURVE_VISIBLE :
                                                ANIMFILTER_LIST_VISIBLE) |
                 extra_filter;

    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        CTX_data_list_add(result, ale->fcurve_owner_id, &RNA_FCurve, ale->data);
      }
    }

    ANIM_animdata_freelist(&anim_data);

    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_editable_fcurves(const bContext *C, bContextDataResult *result)
{
  return screen_ctx_sel_edit_fcurves_(C, result, ANIMFILTER_FOREDIT);
}
static eContextResult screen_ctx_visible_fcurves(const bContext *C, bContextDataResult *result)
{
  return screen_ctx_sel_edit_fcurves_(C, result, 0);
}
static eContextResult screen_ctx_selected_editable_fcurves(const bContext *C,
                                                           bContextDataResult *result)
{
  return screen_ctx_sel_edit_fcurves_(C, result, ANIMFILTER_SEL | ANIMFILTER_FOREDIT);
}
static eContextResult screen_ctx_selected_visible_fcurves(const bContext *C,
                                                          bContextDataResult *result)
{
  return screen_ctx_sel_edit_fcurves_(C, result, ANIMFILTER_SEL);
}
static eContextResult screen_ctx_active_editable_fcurve(const bContext *C,
                                                        bContextDataResult *result)
{
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) && ELEM(ac.spacetype, SPACE_GRAPH)) {
    ListBase anim_data = {NULL, NULL};

    int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ACTIVE | ANIMFILTER_FOREDIT |
                  ANIMFILTER_CURVE_VISIBLE);

    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        CTX_data_pointer_set(result, ale->fcurve_owner_id, &RNA_FCurve, ale->data);
        break;
      }
    }

    ANIM_animdata_freelist(&anim_data);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_editable_keyframes(const bContext *C,
                                                             bContextDataResult *result)
{
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) && ELEM(ac.spacetype, SPACE_ACTION, SPACE_GRAPH)) {
    ListBase anim_data = {NULL, NULL};

    /* Use keyframes from editable selected FCurves. */
    int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS | ANIMFILTER_FOREDIT |
                  ANIMFILTER_SEL) |
                 (ac.spacetype == SPACE_GRAPH ? ANIMFILTER_CURVE_VISIBLE :
                                                ANIMFILTER_LIST_VISIBLE);

    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

    int i;
    FCurve *fcurve;
    BezTriple *bezt;
    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      if (!ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        continue;
      }

      fcurve = (FCurve *)ale->data;
      if (fcurve->bezt == NULL) {
        /* Skip baked FCurves. */
        continue;
      }

      for (i = 0, bezt = fcurve->bezt; i < fcurve->totvert; i++, bezt++) {
        if ((bezt->f2 & SELECT) == 0) {
          continue;
        }

        CTX_data_list_add(result, ale->fcurve_owner_id, &RNA_Keyframe, bezt);
      }
    }

    ANIM_animdata_freelist(&anim_data);

    CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}

static eContextResult screen_ctx_asset_library(const bContext *C, bContextDataResult *result)
{
  WorkSpace *workspace = CTX_wm_workspace(C);
  CTX_data_pointer_set(
      result, &workspace->id, &RNA_AssetLibraryReference, &workspace->asset_library_ref);
  return CTX_RESULT_OK;
}

static eContextResult screen_ctx_ui_list(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  ARegion *region = CTX_wm_region(C);
  if (region) {
    uiList *list = UI_list_find_mouse_over(region, win->eventstate);
    if (list) {
      CTX_data_pointer_set(result, NULL, &RNA_UIList, list);
      return CTX_RESULT_OK;
    }
  }
  return CTX_RESULT_NO_DATA;
}

/* Registry of context callback functions. */

typedef eContextResult (*context_callback)(const bContext *C, bContextDataResult *result);
static GHash *ed_screen_context_functions = NULL;

static void free_context_function_ghash(void *UNUSED(user_data))
{
  BLI_ghash_free(ed_screen_context_functions, NULL, NULL);
}
static inline void register_context_function(const char *member, context_callback function)
{
  BLI_ghash_insert(ed_screen_context_functions, (void *)member, function);
}

static void ensure_ed_screen_context_functions(void)
{
  if (ed_screen_context_functions != NULL) {
    return;
  }

  /* Murmur hash is faster for smaller strings (according to BLI_hash_mm2). */
  ed_screen_context_functions = BLI_ghash_new(
      BLI_ghashutil_strhash_p_murmur, BLI_ghashutil_strcmp, __func__);

  BKE_blender_atexit_register(free_context_function_ghash, NULL);

  register_context_function("scene", screen_ctx_scene);
  register_context_function("visible_objects", screen_ctx_visible_objects);
  register_context_function("selectable_objects", screen_ctx_selectable_objects);
  register_context_function("selected_objects", screen_ctx_selected_objects);
  register_context_function("selected_editable_objects", screen_ctx_selected_editable_objects);
  register_context_function("editable_objects", screen_ctx_editable_objects);
  register_context_function("objects_in_mode", screen_ctx_objects_in_mode);
  register_context_function("objects_in_mode_unique_data", screen_ctx_objects_in_mode_unique_data);
  register_context_function("visible_bones", screen_ctx_visible_bones);
  register_context_function("editable_bones", screen_ctx_editable_bones);
  register_context_function("selected_bones", screen_ctx_selected_bones);
  register_context_function("selected_editable_bones", screen_ctx_selected_editable_bones);
  register_context_function("visible_pose_bones", screen_ctx_visible_pose_bones);
  register_context_function("selected_pose_bones", screen_ctx_selected_pose_bones);
  register_context_function("selected_pose_bones_from_active_object",
                            screen_ctx_selected_pose_bones_from_active_object);
  register_context_function("active_bone", screen_ctx_active_bone);
  register_context_function("active_pose_bone", screen_ctx_active_pose_bone);
  register_context_function("active_object", screen_ctx_active_object);
  register_context_function("object", screen_ctx_object);
  register_context_function("edit_object", screen_ctx_edit_object);
  register_context_function("sculpt_object", screen_ctx_sculpt_object);
  register_context_function("vertex_paint_object", screen_ctx_vertex_paint_object);
  register_context_function("weight_paint_object", screen_ctx_weight_paint_object);
  register_context_function("image_paint_object", screen_ctx_image_paint_object);
  register_context_function("particle_edit_object", screen_ctx_particle_edit_object);
  register_context_function("pose_object", screen_ctx_pose_object);
  register_context_function("active_sequence_strip", screen_ctx_active_sequence_strip);
  register_context_function("sequences", screen_ctx_sequences);
  register_context_function("selected_sequences", screen_ctx_selected_sequences);
  register_context_function("selected_editable_sequences", screen_ctx_selected_editable_sequences);
  register_context_function("active_nla_track", screen_ctx_active_nla_track);
  register_context_function("active_nla_strip", screen_ctx_active_nla_strip);
  register_context_function("selected_nla_strips", screen_ctx_selected_nla_strips);
  register_context_function("selected_movieclip_tracks", screen_ctx_selected_movieclip_tracks);
  register_context_function("gpencil_data", screen_ctx_gpencil_data);
  register_context_function("gpencil_data_owner", screen_ctx_gpencil_data_owner);
  register_context_function("annotation_data", screen_ctx_annotation_data);
  register_context_function("annotation_data_owner", screen_ctx_annotation_data_owner);
  register_context_function("active_gpencil_layer", screen_ctx_active_gpencil_layer);
  register_context_function("active_annotation_layer", screen_ctx_active_annotation_layer);
  register_context_function("active_gpencil_frame", screen_ctx_active_gpencil_frame);
  register_context_function("visible_gpencil_layers", screen_ctx_visible_gpencil_layers);
  register_context_function("editable_gpencil_layers", screen_ctx_editable_gpencil_layers);
  register_context_function("editable_gpencil_strokes", screen_ctx_editable_gpencil_strokes);
  register_context_function("active_operator", screen_ctx_active_operator);
  register_context_function("editable_fcurves", screen_ctx_editable_fcurves);
  register_context_function("visible_fcurves", screen_ctx_visible_fcurves);
  register_context_function("selected_editable_fcurves", screen_ctx_selected_editable_fcurves);
  register_context_function("selected_visible_fcurves", screen_ctx_selected_visible_fcurves);
  register_context_function("active_editable_fcurve", screen_ctx_active_editable_fcurve);
  register_context_function("selected_editable_keyframes", screen_ctx_selected_editable_keyframes);
  register_context_function("asset_library_ref", screen_ctx_asset_library);
  register_context_function("ui_list", screen_ctx_ui_list);
}

/* Entry point for the screen context. */
int ed_screen_context(const bContext *C, const char *member, bContextDataResult *result)
{
  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, screen_context_dir);
    return CTX_RESULT_OK;
  }

  ensure_ed_screen_context_functions();
  context_callback callback = BLI_ghash_lookup(ed_screen_context_functions, member);
  if (callback == NULL) {
    return CTX_RESULT_MEMBER_NOT_FOUND;
  }

  return callback(C, result);
}
