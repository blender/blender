/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 */

#include <cstdlib>
#include <cstring>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_utildefines.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_object.hh"
#include "BKE_tracking.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_anim_api.hh"
#include "ED_armature.hh"
#include "ED_clip.hh"
#include "ED_gpencil_legacy.hh"

#include "SEQ_channels.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_transform.hh"

#include "UI_interface.hh"
#include "WM_api.hh"

#include "ANIM_action.hh"
#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"

#include "screen_intern.hh"

using blender::Vector;

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
    "active_nla_track",
    "active_nla_strip",
    "selected_nla_strips", /* nla editor */
    "selected_movieclip_tracks",
    /* Legacy Grease Pencil */
    "annotation_data",
    "annotation_data_owner",
    "active_annotation_layer",
    /* Grease Pencil */
    "grease_pencil",
    "active_operator",
    "active_action",
    "selected_visible_actions",
    "selected_editable_actions",
    "visible_fcurves",
    "editable_fcurves",
    "selected_visible_fcurves",
    "selected_editable_fcurves",
    "active_editable_fcurve",
    "selected_editable_keyframes",
    "ui_list",
    "property",
    "asset_library_reference",
    "active_strip",
    "strips",
    "selected_strips",
    "selected_editable_strips",
    "sequencer_scene",
    nullptr,
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
  View3D *v3d = CTX_wm_view3d(C); /* This may be nullptr in a lot of cases. */
  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);

  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (BASE_VISIBLE(v3d, base)) {
      CTX_data_id_list_add(result, &base->object->id);
    }
  }
  CTX_data_type_set(result, ContextDataType::Collection);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_selectable_objects(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be nullptr in a lot of cases. */
  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);

  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (BASE_SELECTABLE(v3d, base)) {
      CTX_data_id_list_add(result, &base->object->id);
    }
  }
  CTX_data_type_set(result, ContextDataType::Collection);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_selected_objects(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be nullptr in a lot of cases. */
  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);

  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (BASE_SELECTED(v3d, base)) {
      CTX_data_id_list_add(result, &base->object->id);
    }
  }
  CTX_data_type_set(result, ContextDataType::Collection);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_selected_editable_objects(const bContext *C,
                                                           bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be nullptr in a lot of cases. */
  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);

  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (BASE_SELECTED_EDITABLE(v3d, base)) {
      CTX_data_id_list_add(result, &base->object->id);
    }
  }
  CTX_data_type_set(result, ContextDataType::Collection);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_editable_objects(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be nullptr in a lot of cases. */
  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);

  /* Visible + Editable, but not necessarily selected */
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (BASE_EDITABLE(v3d, base)) {
      CTX_data_id_list_add(result, &base->object->id);
    }
  }
  CTX_data_type_set(result, ContextDataType::Collection);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_objects_in_mode(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be nullptr in a lot of cases. */
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);

  if (obact && (obact->mode != OB_MODE_OBJECT)) {
    FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, obact->type, obact->mode, ob_iter) {
      CTX_data_id_list_add(result, &ob_iter->id);
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  CTX_data_type_set(result, ContextDataType::Collection);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_objects_in_mode_unique_data(const bContext *C,
                                                             bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be nullptr in a lot of cases. */
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);

  if (obact && (obact->mode != OB_MODE_OBJECT)) {
    FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, obact->type, obact->mode, ob_iter) {
      ob_iter->id.tag |= ID_TAG_DOIT;
    }
    FOREACH_OBJECT_IN_MODE_END;
    FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, obact->type, obact->mode, ob_iter) {
      if (ob_iter->id.tag & ID_TAG_DOIT) {
        ob_iter->id.tag &= ~ID_TAG_DOIT;
        CTX_data_id_list_add(result, &ob_iter->id);
      }
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  CTX_data_type_set(result, ContextDataType::Collection);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_visible_or_editable_bones_(const bContext *C,
                                                            bContextDataResult *result,
                                                            const bool editable_bones)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);

  bArmature *arm = static_cast<bArmature *>(
      (obedit && obedit->type == OB_ARMATURE) ? obedit->data : nullptr);
  EditBone *flipbone = nullptr;

  if (arm && arm->edbo) {
    Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        scene, view_layer, CTX_wm_view3d(C));
    for (Object *ob : objects) {
      arm = static_cast<bArmature *>(ob->data);

      /* Attention: X-Axis Mirroring is also handled here... */
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        /* first and foremost, bone must be visible and selected */
        if (blender::animrig::bone_is_visible(arm, ebone)) {
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

            if ((flipbone) && blender::animrig::bone_is_visible(arm, flipbone) == 0) {
              CTX_data_list_add(result, &arm->id, &RNA_EditBone, flipbone);
            }
          }
        }
      }
    }

    CTX_data_type_set(result, ContextDataType::Collection);
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
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  bArmature *arm = static_cast<bArmature *>(
      (obedit && obedit->type == OB_ARMATURE) ? obedit->data : nullptr);
  EditBone *flipbone = nullptr;

  if (arm && arm->edbo) {
    Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        scene, view_layer, CTX_wm_view3d(C));
    for (Object *ob : objects) {
      arm = static_cast<bArmature *>(ob->data);

      /* Attention: X-Axis Mirroring is also handled here... */
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        /* first and foremost, bone must be visible and selected */
        if (blender::animrig::bone_is_visible(arm, ebone) && (ebone->flag & BONE_SELECTED)) {
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

    CTX_data_type_set(result, ContextDataType::Collection);
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
  View3D *v3d = CTX_wm_view3d(C); /* This may be nullptr in a lot of cases. */
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  Object *obpose = BKE_object_pose_armature_get(obact);
  if (obpose && obpose->pose && obpose->data) {
    if (obpose != obact) {
      FOREACH_PCHAN_VISIBLE_IN_OBJECT_BEGIN (obpose, pchan) {
        CTX_data_list_add(result, &obpose->id, &RNA_PoseBone, pchan);
      }
      FOREACH_PCHAN_SELECTED_IN_OBJECT_END;
    }
    else if (obact->mode & OB_MODE_POSE) {
      FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob_iter) {
        FOREACH_PCHAN_VISIBLE_IN_OBJECT_BEGIN (ob_iter, pchan) {
          CTX_data_list_add(result, &ob_iter->id, &RNA_PoseBone, pchan);
        }
        FOREACH_PCHAN_VISIBLE_IN_OBJECT_END;
      }
      FOREACH_OBJECT_IN_MODE_END;
    }
    CTX_data_type_set(result, ContextDataType::Collection);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_pose_bones(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C); /* This may be nullptr in a lot of cases. */
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  Object *obpose = BKE_object_pose_armature_get(obact);
  if (obpose && obpose->pose && obpose->data) {
    if (obpose != obact) {
      FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (obpose, pchan) {
        CTX_data_list_add(result, &obpose->id, &RNA_PoseBone, pchan);
      }
      FOREACH_PCHAN_SELECTED_IN_OBJECT_END;
    }
    else if (obact->mode & OB_MODE_POSE) {
      FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob_iter) {
        FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (ob_iter, pchan) {
          CTX_data_list_add(result, &ob_iter->id, &RNA_PoseBone, pchan);
        }
        FOREACH_PCHAN_SELECTED_IN_OBJECT_END;
      }
      FOREACH_OBJECT_IN_MODE_END;
    }
    CTX_data_type_set(result, ContextDataType::Collection);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_pose_bones_from_active_object(const bContext *C,
                                                                        bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
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
    CTX_data_type_set(result, ContextDataType::Collection);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_bone(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (obact && obact->type == OB_ARMATURE) {
    bArmature *arm = static_cast<bArmature *>(obact->data);
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
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  Object *obpose = BKE_object_pose_armature_get(obact);

  bPoseChannel *pchan = BKE_pose_channel_active_if_bonecoll_visible(obpose);
  if (pchan) {
    CTX_data_pointer_set(result, &obpose->id, &RNA_PoseBone, pchan);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);

  if (obact) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}

static eContextResult screen_ctx_property(const bContext *C, bContextDataResult *result)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);
  if (ptr.data && prop) {
    /* UI_context_active_but_prop_get returns an index of 0 if the property is not
     * an array, but other functions expect -1 for non-arrays. */
    if (!RNA_property_array_check(prop)) {
      index = -1;
    }

    CTX_data_type_set(result, ContextDataType::Property);
    CTX_data_pointer_set_ptr(result, &ptr);
    CTX_data_prop_set(result, prop, index);
  }

  return CTX_RESULT_OK;
}

static eContextResult screen_ctx_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);

  if (obact) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_edit_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  /* convenience for now, 1 object per scene in editmode */
  if (obedit) {
    CTX_data_id_pointer_set(result, &obedit->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_sculpt_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);

  if (obact && (obact->mode & OB_MODE_SCULPT)) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_vertex_paint_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (obact && (obact->mode & OB_MODE_VERTEX_PAINT)) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_weight_paint_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (obact && (obact->mode & OB_MODE_ALL_WEIGHT_PAINT)) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_image_paint_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (obact && (obact->mode & OB_MODE_TEXTURE_PAINT)) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_particle_edit_object(const bContext *C,
                                                      bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (obact && (obact->mode & OB_MODE_PARTICLE_EDIT)) {
    CTX_data_id_pointer_set(result, &obact->id);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_pose_object(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  Object *obpose = BKE_object_pose_armature_get(obact);
  if (obpose) {
    CTX_data_id_pointer_set(result, &obpose->id);
  }
  return CTX_RESULT_OK;
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
    ListBase anim_data = {nullptr, nullptr};

    ANIM_animdata_filter(
        &ac, &anim_data, ANIMFILTER_DATA_VISIBLE, ac.data, eAnimCont_Types(ac.datatype));
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

    CTX_data_type_set(result, ContextDataType::Collection);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_movieclip_tracks(const bContext *C,
                                                           bContextDataResult *result)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  if (space_clip == nullptr) {
    return CTX_RESULT_NO_DATA;
  }
  MovieClip *clip = ED_space_clip_get_clip(space_clip);
  if (clip == nullptr) {
    return CTX_RESULT_NO_DATA;
  }

  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (!TRACK_SELECTED(track)) {
      continue;
    }
    CTX_data_list_add(result, &clip->id, &RNA_MovieTrackingTrack, track);
  }

  CTX_data_type_set(result, ContextDataType::Collection);
  return CTX_RESULT_OK;
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
      CTX_data_pointer_set(result, &gpd->id, &RNA_AnnotationLayer, gpl);
      return CTX_RESULT_OK;
    }
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_grease_pencil_data(const bContext *C, bContextDataResult *result)
{
  wmWindow *win = CTX_wm_window(C);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (obact && obact->type == OB_GREASE_PENCIL) {
    GreasePencil *grease_pencil = static_cast<GreasePencil *>(obact->data);
    CTX_data_id_pointer_set(result, &grease_pencil->id);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_active_operator(const bContext *C, bContextDataResult *result)
{
  wmOperator *op = nullptr;

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
    CTX_data_pointer_set(result, nullptr, &RNA_Operator, op);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_sel_actions_impl(const bContext *C,
                                                  bContextDataResult *result,
                                                  bool active_only,
                                                  bool editable)
{
  bAnimContext ac;
  if (!ANIM_animdata_get_context(C, &ac) || !ELEM(ac.spacetype, SPACE_ACTION, SPACE_GRAPH)) {
    return CTX_RESULT_NO_DATA;
  }

  /* In the Action and Shape Key editor always use the action field at the top. */
  if (ac.spacetype == SPACE_ACTION) {
    SpaceAction *saction = (SpaceAction *)ac.sl;

    if (ELEM(saction->mode, SACTCONT_ACTION, SACTCONT_SHAPEKEY)) {
      ID *active_action_id = ac.active_action ? &ac.active_action->id : nullptr;

      if (active_only) {
        CTX_data_id_pointer_set(result, active_action_id);
      }
      else {
        if (active_action_id && !(editable && !ID_IS_EDITABLE(active_action_id))) {
          CTX_data_id_list_add(result, active_action_id);
        }

        CTX_data_type_set(result, ContextDataType::Collection);
      }

      return CTX_RESULT_OK;
    }
  }

  /* Search for selected animation data items. */
  ListBase anim_data = {nullptr, nullptr};

  int filter = ANIMFILTER_DATA_VISIBLE;
  bool check_selected = false;

  switch (ac.spacetype) {
    case SPACE_GRAPH:
      filter |= ANIMFILTER_FCURVESONLY | ANIMFILTER_CURVE_VISIBLE |
                (active_only ? ANIMFILTER_ACTIVE : ANIMFILTER_SEL);
      break;

    case SPACE_ACTION:
      filter |= ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS;
      check_selected = true;
      break;
    default:
      BLI_assert_unreachable();
  }

  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  blender::Set<bAction *> seen_set;

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    /* In dope-sheet check selection status of individual items, skipping
     * if not selected or has no selection flag. This is needed so that
     * selecting action or group rows without any channels works. */
    if (check_selected && ANIM_channel_setting_get(&ac, ale, ACHANNEL_SETTING_SELECT) <= 0) {
      continue;
    }

    bAction *action = ANIM_channel_action_get(ale);
    if (!action) {
      continue;
    }

    if (active_only) {
      CTX_data_id_pointer_set(result, (ID *)action);
      break;
    }
    if (editable && !ID_IS_EDITABLE(action)) {
      continue;
    }

    /* Add the action to the output list if not already added. */
    if (seen_set.add(action)) {
      CTX_data_id_list_add(result, &action->id);
    }
  }

  ANIM_animdata_freelist(&anim_data);

  if (!active_only) {
    CTX_data_type_set(result, ContextDataType::Collection);
  }

  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_active_action(const bContext *C, bContextDataResult *result)
{
  return screen_ctx_sel_actions_impl(C, result, true, false);
}
static eContextResult screen_ctx_selected_visible_actions(const bContext *C,
                                                          bContextDataResult *result)
{
  return screen_ctx_sel_actions_impl(C, result, false, false);
}
static eContextResult screen_ctx_selected_editable_actions(const bContext *C,
                                                           bContextDataResult *result)
{
  return screen_ctx_sel_actions_impl(C, result, false, true);
}
static eContextResult screen_ctx_sel_edit_fcurves_(const bContext *C,
                                                   bContextDataResult *result,
                                                   const int extra_filter)
{
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) && ELEM(ac.spacetype, SPACE_ACTION, SPACE_GRAPH)) {
    ListBase anim_data = {nullptr, nullptr};

    int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS) |
                 (ac.spacetype == SPACE_GRAPH ?
                      (ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY) :
                      ANIMFILTER_LIST_VISIBLE) |
                 extra_filter;

    ANIM_animdata_filter(
        &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        CTX_data_list_add(result, ale->fcurve_owner_id, &RNA_FCurve, ale->data);
      }
    }

    ANIM_animdata_freelist(&anim_data);

    CTX_data_type_set(result, ContextDataType::Collection);
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
    ListBase anim_data = {nullptr, nullptr};

    int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ACTIVE | ANIMFILTER_FOREDIT |
                  ANIMFILTER_FCURVESONLY | ANIMFILTER_CURVE_VISIBLE);

    ANIM_animdata_filter(
        &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

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
    ListBase anim_data = {nullptr, nullptr};

    /* Use keyframes from editable selected FCurves. */
    int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS | ANIMFILTER_FOREDIT |
                  ANIMFILTER_SEL) |
                 (ac.spacetype == SPACE_GRAPH ?
                      (ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY) :
                      ANIMFILTER_LIST_VISIBLE);

    ANIM_animdata_filter(
        &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

    int i;
    FCurve *fcurve;
    BezTriple *bezt;
    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      if (!ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        continue;
      }

      fcurve = (FCurve *)ale->data;
      if (fcurve->bezt == nullptr) {
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

    CTX_data_type_set(result, ContextDataType::Collection);
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
      CTX_data_pointer_set(result, nullptr, &RNA_UIList, list);
      return CTX_RESULT_OK;
    }
  }
  return CTX_RESULT_NO_DATA;
}

static eContextResult screen_ctx_active_strip(const bContext *C, bContextDataResult *result)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return CTX_RESULT_NO_DATA;
  }
  Strip *strip = blender::seq::select_active_get(scene);
  if (strip) {
    CTX_data_pointer_set(result, &scene->id, &RNA_Strip, strip);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_strips(const bContext *C, bContextDataResult *result)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return CTX_RESULT_NO_DATA;
  }
  Editing *ed = blender::seq::editing_get(scene);
  if (ed) {
    LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
      CTX_data_list_add(result, &scene->id, &RNA_Strip, strip);
    }
    CTX_data_type_set(result, ContextDataType::Collection);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_strips(const bContext *C, bContextDataResult *result)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return CTX_RESULT_NO_DATA;
  }
  Editing *ed = blender::seq::editing_get(scene);
  if (ed) {
    LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
      if (strip->flag & SELECT) {
        CTX_data_list_add(result, &scene->id, &RNA_Strip, strip);
      }
    }
    CTX_data_type_set(result, ContextDataType::Collection);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}
static eContextResult screen_ctx_selected_editable_strips(const bContext *C,
                                                          bContextDataResult *result)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return CTX_RESULT_NO_DATA;
  }
  Editing *ed = blender::seq::editing_get(scene);
  if (ed == nullptr) {
    return CTX_RESULT_NO_DATA;
  }

  ListBase *channels = blender::seq::channels_displayed_get(ed);
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT && !blender::seq::transform_is_locked(channels, strip)) {
      CTX_data_list_add(result, &scene->id, &RNA_Strip, strip);
    }
  }
  CTX_data_type_set(result, ContextDataType::Collection);
  return CTX_RESULT_OK;
}
static eContextResult screen_ctx_sequencer_scene(const bContext *C, bContextDataResult *result)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (scene) {
    CTX_data_id_pointer_set(result, &scene->id);
    return CTX_RESULT_OK;
  }
  return CTX_RESULT_NO_DATA;
}

/* Registry of context callback functions. */

using context_callback = eContextResult (*)(const bContext *C, bContextDataResult *result);

static const blender::Map<blender::StringRef, context_callback> &
ensure_ed_screen_context_functions()
{
  static blender::Map<blender::StringRef, context_callback> screen_context_functions = []() {
    blender::Map<blender::StringRef, context_callback> map;
    map.add("scene", screen_ctx_scene);
    map.add("visible_objects", screen_ctx_visible_objects);
    map.add("selectable_objects", screen_ctx_selectable_objects);
    map.add("selected_objects", screen_ctx_selected_objects);
    map.add("selected_editable_objects", screen_ctx_selected_editable_objects);
    map.add("editable_objects", screen_ctx_editable_objects);
    map.add("objects_in_mode", screen_ctx_objects_in_mode);
    map.add("objects_in_mode_unique_data", screen_ctx_objects_in_mode_unique_data);
    map.add("visible_bones", screen_ctx_visible_bones);
    map.add("editable_bones", screen_ctx_editable_bones);
    map.add("selected_bones", screen_ctx_selected_bones);
    map.add("selected_editable_bones", screen_ctx_selected_editable_bones);
    map.add("visible_pose_bones", screen_ctx_visible_pose_bones);
    map.add("selected_pose_bones", screen_ctx_selected_pose_bones);
    map.add("selected_pose_bones_from_active_object",
            screen_ctx_selected_pose_bones_from_active_object);
    map.add("active_bone", screen_ctx_active_bone);
    map.add("active_pose_bone", screen_ctx_active_pose_bone);
    map.add("active_object", screen_ctx_active_object);
    map.add("object", screen_ctx_object);
    map.add("edit_object", screen_ctx_edit_object);
    map.add("sculpt_object", screen_ctx_sculpt_object);
    map.add("vertex_paint_object", screen_ctx_vertex_paint_object);
    map.add("weight_paint_object", screen_ctx_weight_paint_object);
    map.add("image_paint_object", screen_ctx_image_paint_object);
    map.add("particle_edit_object", screen_ctx_particle_edit_object);
    map.add("pose_object", screen_ctx_pose_object);
    map.add("active_nla_track", screen_ctx_active_nla_track);
    map.add("active_nla_strip", screen_ctx_active_nla_strip);
    map.add("selected_nla_strips", screen_ctx_selected_nla_strips);
    map.add("selected_movieclip_tracks", screen_ctx_selected_movieclip_tracks);
    map.add("annotation_data", screen_ctx_annotation_data);
    map.add("annotation_data_owner", screen_ctx_annotation_data_owner);
    map.add("active_annotation_layer", screen_ctx_active_annotation_layer);
    map.add("grease_pencil", screen_ctx_grease_pencil_data);
    map.add("active_operator", screen_ctx_active_operator);
    map.add("active_action", screen_ctx_active_action);
    map.add("selected_visible_actions", screen_ctx_selected_visible_actions);
    map.add("selected_editable_actions", screen_ctx_selected_editable_actions);
    map.add("editable_fcurves", screen_ctx_editable_fcurves);
    map.add("visible_fcurves", screen_ctx_visible_fcurves);
    map.add("selected_editable_fcurves", screen_ctx_selected_editable_fcurves);
    map.add("selected_visible_fcurves", screen_ctx_selected_visible_fcurves);
    map.add("active_editable_fcurve", screen_ctx_active_editable_fcurve);
    map.add("selected_editable_keyframes", screen_ctx_selected_editable_keyframes);
    map.add("asset_library_reference", screen_ctx_asset_library);
    map.add("ui_list", screen_ctx_ui_list);
    map.add("property", screen_ctx_property);
    map.add("active_strip", screen_ctx_active_strip);
    map.add("strips", screen_ctx_strips);
    map.add("selected_strips", screen_ctx_selected_strips);
    map.add("selected_editable_strips", screen_ctx_selected_editable_strips);
    map.add("sequencer_scene", screen_ctx_sequencer_scene);
    return map;
  }();
  return screen_context_functions;
}

int ed_screen_context(const bContext *C, const char *member, bContextDataResult *result)
{
  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, screen_context_dir);
    return CTX_RESULT_OK;
  }

  const blender::Map<blender::StringRef, context_callback> &functions =
      ensure_ed_screen_context_functions();
  context_callback callback = functions.lookup_default(member, nullptr);
  if (callback == nullptr) {
    return CTX_RESULT_MEMBER_NOT_FOUND;
  }

  return callback(C, result);
}
