/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_bounds.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mball.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_tracking.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_anim_api.hh"
#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_keyframing.hh"
#include "ED_object.hh"
#include "ED_pointcloud.hh"
#include "ED_screen.hh"
#include "ED_transverts.hh"

#include "ANIM_action.hh"
#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_keyingsets.hh"

#include "view3d_intern.hh"

using blender::Vector;

static bool snap_curs_to_sel_ex(bContext *C, const int pivot_point, float r_cursor[3]);
static bool snap_calc_active_center(bContext *C, const bool select_only, float r_center[3]);

/* -------------------------------------------------------------------- */
/** \name Snap Selection to Grid Operator
 * \{ */

/** Snaps every individual object center to its nearest point on the grid. */
static wmOperatorStatus snap_sel_to_grid_exec(bContext *C, wmOperator *op)
{
  using namespace blender::ed;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  Object *obact = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  TransVertStore tvs = {nullptr};
  TransVert *tv;
  float gridf, imat[3][3], bmat[3][3], vec[3];
  int a;

  gridf = ED_view3d_grid_view_scale(scene, v3d, region, nullptr);

  if (OBEDIT_FROM_OBACT(obact)) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        scene, view_layer, CTX_wm_view3d(C));
    for (Object *obedit : objects) {
      if (obedit->type == OB_MESH) {
        BMEditMesh *em = BKE_editmesh_from_object(obedit);

        if (em->bm->totvertsel == 0) {
          continue;
        }
      }

      if (blender::ed::object::shape_key_report_if_locked(obedit, op->reports)) {
        continue;
      }

      if (ED_transverts_check_obedit(obedit)) {
        ED_transverts_create_from_obedit(&tvs, obedit, 0);
      }

      if (tvs.transverts_tot != 0) {
        copy_m3_m4(bmat, obedit->object_to_world().ptr());
        invert_m3_m3(imat, bmat);

        tv = tvs.transverts;
        for (a = 0; a < tvs.transverts_tot; a++, tv++) {
          copy_v3_v3(vec, tv->loc);
          mul_m3_v3(bmat, vec);
          add_v3_v3(vec, obedit->object_to_world().location());
          vec[0] = gridf * floorf(0.5f + vec[0] / gridf);
          vec[1] = gridf * floorf(0.5f + vec[1] / gridf);
          vec[2] = gridf * floorf(0.5f + vec[2] / gridf);
          sub_v3_v3(vec, obedit->object_to_world().location());

          mul_m3_v3(imat, vec);
          copy_v3_v3(tv->loc, vec);
        }
        ED_transverts_update_obedit(&tvs, obedit);
      }
      ED_transverts_free(&tvs);
    }
  }
  else if (OBPOSE_FROM_OBACT(obact)) {
    KeyingSet *ks = blender::animrig::get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);
    Vector<Object *> objects_eval = BKE_object_pose_array_get(scene, view_layer_eval, v3d);
    for (Object *ob_eval : objects_eval) {
      Object *ob = DEG_get_original(ob_eval);
      bArmature *arm_eval = static_cast<bArmature *>(ob_eval->data);

      invert_m4_m4(ob_eval->runtime->world_to_object.ptr(), ob_eval->object_to_world().ptr());

      LISTBASE_FOREACH (bPoseChannel *, pchan_eval, &ob_eval->pose->chanbase) {
        if (pchan_eval->flag & POSE_SELECTED) {
          if (ANIM_bonecoll_is_visible_pchan(arm_eval, pchan_eval)) {
            if ((pchan_eval->bone->flag & BONE_CONNECTED) == 0) {
              float nLoc[3];

              /* get nearest grid point to snap to */
              copy_v3_v3(nLoc, pchan_eval->pose_mat[3]);
              /* We must operate in world space! */
              mul_m4_v3(ob_eval->object_to_world().ptr(), nLoc);
              vec[0] = gridf * floorf(0.5f + nLoc[0] / gridf);
              vec[1] = gridf * floorf(0.5f + nLoc[1] / gridf);
              vec[2] = gridf * floorf(0.5f + nLoc[2] / gridf);
              /* Back in object space... */
              mul_m4_v3(ob_eval->world_to_object().ptr(), vec);

              /* Get location of grid point in pose space. */
              BKE_armature_loc_pose_to_bone(pchan_eval, vec, vec);

              /* Adjust location on the original pchan. */
              bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, pchan_eval->name);
              BKE_pchan_protected_location_set(pchan, vec);

              /* auto-keyframing */
              blender::animrig::autokeyframe_pchan(C, scene, ob, pchan, ks);
            }
            /* if the bone has a parent and is connected to the parent,
             * don't do anything - will break chain unless we do auto-ik.
             */
          }
        }
      }
      ob->pose->flag |= (POSE_LOCKED | POSE_DO_UNLOCK);

      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }
  else {
    /* Object mode. */
    Main *bmain = CTX_data_main(C);

    KeyingSet *ks = blender::animrig::get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);

    const bool use_transform_skip_children = (scene->toolsettings->transform_flag &
                                              SCE_XFORM_SKIP_CHILDREN);
    const bool use_transform_data_origin = (scene->toolsettings->transform_flag &
                                            SCE_XFORM_DATA_ORIGIN);
    object::XFormObjectSkipChild_Container *xcs = nullptr;
    object::XFormObjectData_Container *xds = nullptr;

    /* Build object array. */
    Vector<Object *> objects_eval;
    Vector<Object *> objects_orig;
    {
      FOREACH_SELECTED_EDITABLE_OBJECT_BEGIN (view_layer_eval, v3d, ob_eval) {
        objects_eval.append(ob_eval);
        objects_orig.append(DEG_get_original(ob_eval));
      }
      FOREACH_SELECTED_EDITABLE_OBJECT_END;
    }

    if (use_transform_skip_children) {
      ViewLayer *view_layer = CTX_data_view_layer(C);

      Vector<Object *> objects(objects_eval.size());
      for (Object *ob_eval : objects_eval) {
        objects.append_unchecked(DEG_get_original(ob_eval));
      }
      BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
      xcs = object::xform_skip_child_container_create();
      object::xform_skip_child_container_item_ensure_from_array(
          xcs, scene, view_layer, objects.data(), objects.size());
    }
    if (use_transform_data_origin) {
      BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
      xds = object::data_xform_container_create();
    }

    if (blender::animrig::is_autokey_on(scene)) {
      ANIM_deselect_keys_in_animation_editors(C);
    }

    for (Object *ob_eval : objects_eval) {
      Object *ob = DEG_get_original(ob_eval);
      vec[0] = -ob_eval->object_to_world().location()[0] +
               gridf * floorf(0.5f + ob_eval->object_to_world().location()[0] / gridf);
      vec[1] = -ob_eval->object_to_world().location()[1] +
               gridf * floorf(0.5f + ob_eval->object_to_world().location()[1] / gridf);
      vec[2] = -ob_eval->object_to_world().location()[2] +
               gridf * floorf(0.5f + ob_eval->object_to_world().location()[2] / gridf);

      if (ob->parent) {
        float originmat[3][3];
        BKE_object_where_is_calc_ex(depsgraph, scene, nullptr, ob, originmat);

        invert_m3_m3(imat, originmat);
        mul_m3_v3(imat, vec);
      }

      const blender::float3 loc_final = blender::float3(ob_eval->loc) + blender::float3(vec);
      BKE_object_protected_location_set(ob, loc_final);

      /* auto-keyframing */
      blender::animrig::autokeyframe_object(C, scene, ob, ks);

      if (use_transform_data_origin) {
        object::data_xform_container_item_ensure(xds, ob);
      }

      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
    }

    if (use_transform_skip_children) {
      object::object_xform_skip_child_container_update_all(xcs, bmain, depsgraph);
      object::object_xform_skip_child_container_destroy(xcs);
    }
    if (use_transform_data_origin) {
      object::data_xform_container_update_all(xds, bmain, depsgraph);
      object::data_xform_container_destroy(xds);
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_selected_to_grid(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Grid";
  ot->description = "Snap selected item(s) to their nearest grid division";
  ot->idname = "VIEW3D_OT_snap_selected_to_grid";

  /* API callbacks. */
  ot->exec = snap_sel_to_grid_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Selection to Location (Utility)
 * \{ */

/* Return true if the bone or any of its parents has the given runtime flag set. */
static bool pose_bone_runtime_flag_test_recursive(const bPoseChannel *pose_bone, int flag)
{
  if (pose_bone->runtime.flag & flag) {
    return true;
  }
  if (pose_bone->parent) {
    return pose_bone_runtime_flag_test_recursive(pose_bone->parent, flag);
  }
  return false;
}

/**
 * Snaps the selection as a whole (use_offset=true) or each selected object to the given location.
 *
 * \param target_loc_global: a location in global space to snap to
 * (eg. 3D cursor or active object).
 * \param target_orientation_global: a 3d cursor which will provides rotation.
 * When non-null objects are rotated to match the rotation of the 3d cursor,
 * otherwise, keep their original rotation.
 * Note that a more generic orientation parameter could be supported in future, but for now,
 * only the 3d cursor is used.
 * \param use_offset: if the selected objects should maintain their relative offsets
 * and be snapped by the selection pivot point (median, active),
 * or if every object origin should be snapped to the given location.
 */
static bool snap_selected_to_location_rotation(bContext *C,
                                               wmOperator *op,
                                               const blender::float3 &target_loc_global,
                                               const View3DCursor *target_orientation_global,
                                               const bool use_offset,
                                               const int pivot_point,
                                               const bool use_toolsettings)
{
  using namespace blender::ed;
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);
  Object *obact = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  TransVertStore tvs = {nullptr};
  TransVert *tv;
  float imat[3][3], bmat[3][3];
  float center_global[3];
  float offset_global[3];
  int a;

  /* Some use of this needs to transform into local-space. */
  const bool use_rotation = target_orientation_global != nullptr;
  const blender::float3x3 target_rot_global =
      target_orientation_global ? target_orientation_global->matrix<blender::float3x3>() :
                                  blender::float3x3::zero();

  if (use_offset) {
    if ((pivot_point == V3D_AROUND_ACTIVE) && snap_calc_active_center(C, true, center_global)) {
      /* pass */
    }
    else {
      snap_curs_to_sel_ex(C, pivot_point, center_global);
    }
    sub_v3_v3v3(offset_global, target_loc_global, center_global);
  }

  if (obedit) {
    blender::float3 target_loc_local;
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        scene, view_layer, v3d);
    for (const int ob_index : objects.index_range()) {
      obedit = objects[ob_index];

      if (obedit->type == OB_MESH) {
        BMEditMesh *em = BKE_editmesh_from_object(obedit);

        if (em->bm->totvertsel == 0) {
          continue;
        }
      }

      if (blender::ed::object::shape_key_report_if_locked(obedit, op->reports)) {
        continue;
      }

      if (ED_transverts_check_obedit(obedit)) {
        ED_transverts_create_from_obedit(&tvs, obedit, 0);
      }

      if (tvs.transverts_tot != 0) {
        copy_m3_m4(bmat, obedit->object_to_world().ptr());
        invert_m3_m3(imat, bmat);

        /* Get the `target_loc_global` in object space. */
        sub_v3_v3v3(target_loc_local, target_loc_global, obedit->object_to_world().location());
        mul_m3_v3(imat, target_loc_local);

        if (use_offset) {
          blender::float3 offset_local;

          mul_v3_m3v3(offset_local, imat, offset_global);

          tv = tvs.transverts;
          for (a = 0; a < tvs.transverts_tot; a++, tv++) {
            add_v3_v3(tv->loc, offset_local);
          }
        }
        else {
          tv = tvs.transverts;
          for (a = 0; a < tvs.transverts_tot; a++, tv++) {
            copy_v3_v3(tv->loc, target_loc_local);
          }
        }
        ED_transverts_update_obedit(&tvs, obedit);
      }
      ED_transverts_free(&tvs);
    }
  }
  else if (OBPOSE_FROM_OBACT(obact)) {
    KeyingSet *ks = blender::animrig::get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Vector<Object *> objects = BKE_object_pose_array_get(scene, view_layer, v3d);
    Main *bmain = CTX_data_main(C);
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

    for (Object *ob : objects) {
      bArmature *arm = static_cast<bArmature *>(ob->data);
      blender::float3 target_loc_local;

      invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
      mul_v3_m4v3(target_loc_local, ob->world_to_object().ptr(), target_loc_global);

      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        if ((pchan->flag & POSE_SELECTED) && blender::animrig::bone_is_visible(arm, pchan) &&
            /* if the bone has a parent and is connected to the parent,
             * don't do anything - will break chain unless we do auto-ik.
             */
            (pchan->bone->flag & BONE_CONNECTED) == 0)
        {
          pchan->runtime.flag |= POSE_RUNTIME_TRANSFORM;
        }
        else {
          pchan->runtime.flag &= ~POSE_RUNTIME_TRANSFORM;
        }
      }

      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        if ((pchan->runtime.flag & POSE_RUNTIME_TRANSFORM) &&
            /* check that our parents not transformed (if we have one) */
            ((pchan->bone->parent &&
              pose_bone_runtime_flag_test_recursive(pchan->parent, POSE_RUNTIME_TRANSFORM)) == 0))
        {
          /* Get position in pchan (pose) space. */
          blender::float3 target_loc_pose;

          if (use_offset) {
            mul_v3_m4v3(target_loc_pose, ob->object_to_world().ptr(), pchan->pose_mat[3]);
            add_v3_v3(target_loc_pose, offset_global);

            if (use_rotation) {
              sub_v3_v3(target_loc_pose, target_loc_global);
              mul_m3_v3(target_rot_global.ptr(), target_loc_pose);
              add_v3_v3(target_loc_pose, target_loc_global);
            }

            mul_m4_v3(ob->world_to_object().ptr(), target_loc_pose);
            BKE_armature_loc_pose_to_bone(pchan, target_loc_pose, target_loc_pose);
          }
          else {
            BKE_armature_loc_pose_to_bone(pchan, target_loc_local, target_loc_pose);
          }

          if (use_rotation) {
            BKE_pchan_mat3_to_rot(pchan, target_rot_global.ptr(), false);

            if (pchan->rotmode == ROT_MODE_QUAT) {
              float quat[4];
              mat3_normalized_to_quat(quat, target_rot_global.ptr());

              if (use_toolsettings) {
                BKE_pchan_protected_rotation_quaternion_set(pchan, quat);
              }
              else {
                copy_v4_v4(pchan->quat, quat);
              }
            }
            else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
              float rot_axis[3];
              float rot_angle;
              mat3_to_axis_angle(rot_axis, &rot_angle, target_rot_global.ptr());

              if (use_toolsettings) {
                BKE_pchan_protected_rotation_axisangle_set(pchan, rot_axis, rot_angle);
              }
              else {
                copy_v3_v3(pchan->rotAxis, rot_axis);
                pchan->rotAngle = rot_angle;
              }
            }
            else {
              float rot_euler[3];
              mat3_to_eulO(rot_euler, pchan->rotmode, target_rot_global.ptr());

              if (use_toolsettings) {
                BKE_pchan_protected_rotation_euler_set(pchan, rot_euler);
              }
              else {
                copy_v3_v3(pchan->eul, rot_euler);
              }
            }
          }

          /* copy new position */
          if (use_toolsettings) {
            BKE_pchan_protected_location_set(pchan, target_loc_pose);

            /* auto-keyframing */
            blender::animrig::autokeyframe_pchan(C, scene, ob, pchan, ks);
          }
          else {
            copy_v3_v3(pchan->loc, target_loc_pose);
          }
        }
      }

      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        pchan->runtime.flag &= ~POSE_RUNTIME_TRANSFORM;
      }

      ob->pose->flag |= (POSE_LOCKED | POSE_DO_UNLOCK);

      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }
  else {
    KeyingSet *ks = blender::animrig::get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);
    Main *bmain = CTX_data_main(C);
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

    /* Reset flags. */
    for (Object *ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      ob->flag &= ~OB_DONE;
    }

    /* Build object array, tag objects we're transforming. */
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Vector<Object *> objects;
    {
      FOREACH_SELECTED_EDITABLE_OBJECT_BEGIN (view_layer, v3d, ob) {
        objects.append(ob);
        ob->flag |= OB_DONE;
      }
      FOREACH_SELECTED_EDITABLE_OBJECT_END;
    }

    const bool use_transform_skip_children = use_toolsettings &&
                                             (scene->toolsettings->transform_flag &
                                              SCE_XFORM_SKIP_CHILDREN);
    const bool use_transform_data_origin = use_toolsettings &&
                                           (scene->toolsettings->transform_flag &
                                            SCE_XFORM_DATA_ORIGIN);
    object::XFormObjectSkipChild_Container *xcs = nullptr;
    object::XFormObjectData_Container *xds = nullptr;

    if (use_transform_skip_children) {
      xcs = object::xform_skip_child_container_create();
      object::xform_skip_child_container_item_ensure_from_array(
          xcs, scene, view_layer, objects.data(), objects.size());
    }
    if (use_transform_data_origin) {
      xds = object::data_xform_container_create();

      /* Initialize the transform data in a separate loop because the depsgraph
       * may be evaluated while setting the locations. */
      for (Object *ob : objects) {
        object::data_xform_container_item_ensure(xds, ob);
      }
    }

    if (blender::animrig::is_autokey_on(scene)) {
      ANIM_deselect_keys_in_animation_editors(C);
    }

    for (Object *ob : objects) {
      /* With offset enabled, skip child objects whose parents are also transformed
       * to avoid double transform. */
      if (use_offset) {
        if (ob->parent && BKE_object_flag_test_recursive(ob->parent, OB_DONE)) {
          continue;
        }
      }

      blender::float3 target_loc_local; /* parent-relative */

      if (use_offset) {
        add_v3_v3v3(target_loc_local, ob->object_to_world().location(), offset_global);

        if (use_rotation) {
          sub_v3_v3(target_loc_local, target_loc_global);
          mul_m3_v3(target_rot_global.ptr(), target_loc_local);
          add_v3_v3(target_loc_local, target_loc_global);
        }
      }
      else {
        copy_v3_v3(target_loc_local, target_loc_global);
      }
      sub_v3_v3(target_loc_local, ob->object_to_world().location());

      /* Calculate a parent relative copy. */
      blender::float3x3 target_rot = target_rot_global;
      if (ob->parent) {
        float originmat[3][3], parentmat[4][4];
        /* Use the evaluated object here because sometimes
         * `ob->parent->runtime->curve_cache` is required. */
        BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
        Object *ob_eval = DEG_get_evaluated(depsgraph, ob);

        BKE_object_get_parent_matrix(ob_eval, ob_eval->parent, parentmat);
        mul_m3_m4m4(originmat, parentmat, ob->parentinv);
        invert_m3_m3(imat, originmat);
        mul_m3_v3(imat, target_loc_local);
        mul_m3_m3m3(target_rot.ptr(), imat, target_rot.ptr());
      }
      if (use_toolsettings) {
        const blender::float3 loc_final = blender::float3(ob->loc) + target_loc_local;
        BKE_object_protected_location_set(ob, loc_final);

        /* auto-keyframing */
        blender::animrig::autokeyframe_object(C, scene, ob, ks);
      }
      else {
        add_v3_v3(ob->loc, target_loc_local);
      }

      if (use_rotation) {
        const bool assign_rotation_directly = (ob->rotmode ==
                                                   target_orientation_global->rotation_mode &&
                                               ob->parent == nullptr);

        if (ob->rotmode == ROT_MODE_QUAT) {
          float quat[4];
          if (assign_rotation_directly) {
            copy_v4_v4(quat, target_orientation_global->rotation_quaternion);
          }
          else {
            mat3_normalized_to_quat(quat, target_rot.ptr());
          }
          if (use_toolsettings) {
            BKE_object_protected_rotation_quaternion_set(ob, quat);
          }
          else {
            copy_v4_v4(ob->quat, quat);
          }
        }
        else if (ob->rotmode == ROT_MODE_AXISANGLE) {
          float rot_axis[3];
          float rot_angle;
          if (assign_rotation_directly) {
            copy_v3_v3(rot_axis, target_orientation_global->rotation_axis);
            rot_angle = target_orientation_global->rotation_angle;
          }
          else {
            mat3_to_axis_angle(rot_axis, &rot_angle, target_rot.ptr());
          }
          if (use_toolsettings) {
            BKE_object_protected_rotation_axisangle_set(ob, rot_axis, rot_angle);
          }
          else {
            copy_v3_v3(ob->rotAxis, rot_axis);
            ob->rotAngle = rot_angle;
          }
        }
        else {
          float rot_euler[3];
          if (assign_rotation_directly) {
            copy_v3_v3(rot_euler, target_orientation_global->rotation_euler);
          }
          else {
            mat3_to_eulO(rot_euler, ob->rotmode, target_rot.ptr());
          }
          if (use_toolsettings) {
            BKE_object_protected_rotation_euler_set(ob, rot_euler);
          }
          else {
            copy_v3_v3(ob->rot, rot_euler);
          }
        }

        /* auto-keyframing */
        blender::animrig::autokeyframe_object(C, scene, ob, ks);
      }

      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
    }

    if (use_transform_skip_children) {
      object::object_xform_skip_child_container_update_all(xcs, bmain, depsgraph);
      object::object_xform_skip_child_container_destroy(xcs);
    }
    if (use_transform_data_origin) {
      object::data_xform_container_update_all(xds, bmain, depsgraph);
      object::data_xform_container_destroy(xds);
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

  return true;
}

bool ED_view3d_snap_selected_to_location(bContext *C,
                                         wmOperator *op,
                                         const float target_loc_global[3],
                                         const int pivot_point)
{
  /* These could be passed as arguments if needed. */
  /* Always use pivot point. */
  const bool use_offset = true;
  /* Disable object protected flags & auto-keyframing,
   * so this can be used as a low level function. */
  const bool use_toolsettings = false;
  return snap_selected_to_location_rotation(
      C, op, target_loc_global, nullptr, use_offset, pivot_point, use_toolsettings);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Selection to Cursor Operator
 * \{ */

static wmOperatorStatus snap_selected_to_cursor_exec(bContext *C, wmOperator *op)
{
  const bool use_offset = RNA_boolean_get(op->ptr, "use_offset");
  const bool use_rotation = RNA_boolean_get(op->ptr, "use_rotation");

  Scene *scene = CTX_data_scene(C);

  const float *target_loc_global = scene->cursor.location;
  const View3DCursor *snap_orientation = use_rotation ? &scene->cursor : nullptr;
  const int pivot_point = scene->toolsettings->transform_pivot_point;

  if (snap_selected_to_location_rotation(
          C, op, target_loc_global, snap_orientation, use_offset, pivot_point, true))
  {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_snap_selected_to_cursor(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Cursor";
  ot->description = "Snap selected item(s) to the 3D cursor";
  ot->idname = "VIEW3D_OT_snap_selected_to_cursor";

  /* API callbacks. */
  ot->exec = snap_selected_to_cursor_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  RNA_def_boolean(ot->srna,
                  "use_offset",
                  true,
                  "Offset",
                  "If the selection should be snapped as a whole or by each object center");
  RNA_def_boolean(ot->srna,
                  "use_rotation",
                  false,
                  "Rotation",
                  "If the selection should be rotated to match the cursor");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Selection to Active Operator
 * \{ */

/** Snaps each selected object to the location of the active selected object. */
static wmOperatorStatus snap_selected_to_active_exec(bContext *C, wmOperator *op)
{
  float target_loc_global[3];

  if (snap_calc_active_center(C, false, target_loc_global) == false) {
    BKE_report(op->reports, RPT_ERROR, "No active element found!");
    return OPERATOR_CANCELLED;
  }

  if (!snap_selected_to_location_rotation(C, op, target_loc_global, nullptr, false, -1, true)) {
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_selected_to_active(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Active";
  ot->description = "Snap selected item(s) to the active item";
  ot->idname = "VIEW3D_OT_snap_selected_to_active";

  /* API callbacks. */
  ot->exec = snap_selected_to_active_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Cursor to Grid Operator
 * \{ */

/** Snaps the 3D cursor location to its nearest point on the grid. */
static wmOperatorStatus snap_curs_to_grid_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  float gridf, *curs;

  gridf = ED_view3d_grid_view_scale(scene, v3d, region, nullptr);
  curs = scene->cursor.location;

  curs[0] = gridf * floorf(0.5f + curs[0] / gridf);
  curs[1] = gridf * floorf(0.5f + curs[1] / gridf);
  curs[2] = gridf * floorf(0.5f + curs[2] / gridf);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr); /* hrm */
  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_grid(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to Grid";
  ot->description = "Snap 3D cursor to the nearest grid division";
  ot->idname = "VIEW3D_OT_snap_cursor_to_grid";

  /* API callbacks. */
  ot->exec = snap_curs_to_grid_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Cursor to Selection Operator
 * \{ */

/**
 * Returns the center position of a tracking marker visible on the viewport
 * (useful to snap to).
 */
static void bundle_midpoint(Scene *scene, Object *ob, float r_vec[3])
{
  MovieClip *clip = BKE_object_movieclip_get(scene, ob, false);
  bool ok = false;
  float min[3], max[3], mat[4][4], pos[3], cammat[4][4];

  if (!clip) {
    return;
  }

  MovieTracking *tracking = &clip->tracking;

  copy_m4_m4(cammat, ob->object_to_world().ptr());

  BKE_tracking_get_camera_object_matrix(ob, mat);

  INIT_MINMAX(min, max);

  LISTBASE_FOREACH (MovieTrackingObject *, tracking_object, &tracking->objects) {
    float obmat[4][4];

    if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
      copy_m4_m4(obmat, mat);
    }
    else {
      float imat[4][4];

      BKE_tracking_camera_get_reconstructed_interpolate(
          tracking, tracking_object, scene->r.cfra, imat);
      invert_m4(imat);

      mul_m4_m4m4(obmat, cammat, imat);
    }

    LISTBASE_FOREACH (const MovieTrackingTrack *, track, &tracking_object->tracks) {
      if ((track->flag & TRACK_HAS_BUNDLE) && TRACK_SELECTED(track)) {
        ok = true;
        mul_v3_m4v3(pos, obmat, track->bundle_pos);
        minmax_v3v3_v3(min, max, pos);
      }
    }
  }

  if (ok) {
    mid_v3_v3v3(r_vec, min, max);
  }
}

/** Snaps the 3D cursor location to the median point of the selection. */
static bool snap_curs_to_sel_ex(bContext *C, const int pivot_point, float r_cursor[3])
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  Object *obedit = CTX_data_edit_object(C);
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  TransVertStore tvs = {nullptr};
  TransVert *tv;
  float bmat[3][3], vec[3], min[3], max[3], centroid[3];
  int count = 0;

  INIT_MINMAX(min, max);
  zero_v3(centroid);

  if (obedit) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        scene, view_layer, CTX_wm_view3d(C));
    for (const int ob_index : objects.index_range()) {
      obedit = objects[ob_index];

      /* We can do that quick check for meshes only... */
      if (obedit->type == OB_MESH) {
        BMEditMesh *em = BKE_editmesh_from_object(obedit);

        if (em->bm->totvertsel == 0) {
          continue;
        }
      }

      if (ED_transverts_check_obedit(obedit)) {
        const Object *obedit_eval = DEG_get_evaluated(depsgraph, obedit);
        ED_transverts_create_from_obedit(&tvs, obedit_eval, TM_ALL_JOINTS | TM_SKIP_HANDLES);
      }

      count += tvs.transverts_tot;
      if (tvs.transverts_tot != 0) {
        Object *obedit_eval = DEG_get_evaluated(depsgraph, obedit);
        copy_m3_m4(bmat, obedit_eval->object_to_world().ptr());

        tv = tvs.transverts;
        for (int i = 0; i < tvs.transverts_tot; i++, tv++) {
          copy_v3_v3(vec, tv->loc);
          mul_m3_v3(bmat, vec);
          add_v3_v3(vec, obedit_eval->object_to_world().location());
          add_v3_v3(centroid, vec);
          minmax_v3v3_v3(min, max, vec);
        }
      }
      ED_transverts_free(&tvs);
    }
  }
  else {
    Object *obact = CTX_data_active_object(C);

    if (obact && (obact->mode & OB_MODE_POSE)) {
      Object *obact_eval = DEG_get_evaluated(depsgraph, obact);
      bArmature *arm = static_cast<bArmature *>(obact_eval->data);
      LISTBASE_FOREACH (bPoseChannel *, pchan, &obact_eval->pose->chanbase) {
        if (ANIM_bonecoll_is_visible_pchan(arm, pchan)) {
          if (pchan->flag & POSE_SELECTED) {
            copy_v3_v3(vec, pchan->pose_head);
            mul_m4_v3(obact_eval->object_to_world().ptr(), vec);
            add_v3_v3(centroid, vec);
            minmax_v3v3_v3(min, max, vec);
            count++;
          }
        }
      }
    }
    else {
      FOREACH_SELECTED_OBJECT_BEGIN (view_layer_eval, v3d, ob_eval) {
        copy_v3_v3(vec, ob_eval->object_to_world().location());

        /* special case for camera -- snap to bundles */
        if (ob_eval->type == OB_CAMERA) {
          /* snap to bundles should happen only when bundles are visible */
          if (v3d->flag2 & V3D_SHOW_RECONSTRUCTION) {
            bundle_midpoint(scene, DEG_get_original(ob_eval), vec);
          }
        }

        add_v3_v3(centroid, vec);
        minmax_v3v3_v3(min, max, vec);
        count++;
      }
      FOREACH_SELECTED_OBJECT_END;
    }
  }

  if (count == 0) {
    return false;
  }

  if (pivot_point == V3D_AROUND_CENTER_BOUNDS) {
    mid_v3_v3v3(r_cursor, min, max);
  }
  else {
    mul_v3_fl(centroid, 1.0f / float(count));
    copy_v3_v3(r_cursor, centroid);
  }
  return true;
}

static wmOperatorStatus snap_curs_to_sel_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  const int pivot_point = scene->toolsettings->transform_pivot_point;
  if (snap_curs_to_sel_ex(C, pivot_point, scene->cursor.location)) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
    DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_snap_cursor_to_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to Selected";
  ot->description = "Snap 3D cursor to the middle of the selected item(s)";
  ot->idname = "VIEW3D_OT_snap_cursor_to_selected";

  /* API callbacks. */
  ot->exec = snap_curs_to_sel_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Cursor to Active Operator
 * \{ */

/**
 * Calculates the center position of the active object in global space.
 *
 * NOTE: this could be exported to be a generic function.
 * see: #calculateCenterActive
 */
static bool snap_calc_active_center(bContext *C, const bool select_only, float r_center[3])
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  return blender::ed::object::calc_active_center(ob, select_only, r_center);
}

static wmOperatorStatus snap_curs_to_active_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  if (snap_calc_active_center(C, false, scene->cursor.location)) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
    DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_snap_cursor_to_active(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to Active";
  ot->description = "Snap 3D cursor to the active item";
  ot->idname = "VIEW3D_OT_snap_cursor_to_active";

  /* API callbacks. */
  ot->exec = snap_curs_to_active_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Cursor to Center Operator
 * \{ */

/** Snaps the 3D cursor location to the origin and clears cursor rotation. */
static wmOperatorStatus snap_curs_to_center_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  scene->cursor.set_matrix(blender::float4x4::identity(), false);

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
  return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_center(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to World Origin";
  ot->description = "Snap 3D cursor to the world origin";
  ot->idname = "VIEW3D_OT_snap_cursor_to_center";

  /* API callbacks. */
  ot->exec = snap_curs_to_center_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Min/Max Object Vertices Utility
 * \{ */

static std::optional<blender::Bounds<blender::float3>> bounds_min_max_with_transform(
    const blender::float4x4 &transform,
    const blender::Span<blender::float3> positions,
    const blender::IndexMask &mask)
{
  using namespace blender;
  if (mask.is_empty()) {
    return std::nullopt;
  }
  return threading::parallel_reduce(
      mask.index_range(),
      1024,
      Bounds<float3>(math::transform_point(transform, positions[mask.first()])),
      [&](const IndexRange range, Bounds<float3> init) {
        mask.slice(range).foreach_index([&](const int i) {
          math::min_max(math::transform_point(transform, positions[i]), init.min, init.max);
        });
        return init;
      },
      [](const Bounds<float3> &a, const Bounds<float3> &b) { return bounds::merge(a, b); });
}

bool ED_view3d_minmax_verts(const Scene *scene, Object *obedit, float r_min[3], float r_max[3])
{
  using namespace blender;
  using namespace blender::ed;
  TransVertStore tvs = {nullptr};
  TransVert *tv;
  float centroid[3], vec[3], bmat[3][3];

  /* Meta-balls are an exception. */
  if (obedit->type == OB_MBALL) {
    float ob_min[3], ob_max[3];
    bool changed;

    changed = BKE_mball_minmax_ex(static_cast<const MetaBall *>(obedit->data),
                                  ob_min,
                                  ob_max,
                                  obedit->object_to_world().ptr(),
                                  SELECT);
    if (changed) {
      minmax_v3v3_v3(r_min, r_max, ob_min);
      minmax_v3v3_v3(r_min, r_max, ob_max);
    }
    return changed;
  }
  if (obedit->type == OB_POINTCLOUD) {
    const Object &ob_orig = *DEG_get_original(obedit);
    const PointCloud &pointcloud = *static_cast<const PointCloud *>(ob_orig.data);

    IndexMaskMemory memory;
    const IndexMask mask = pointcloud::retrieve_selected_points(pointcloud, memory);

    const std::optional<Bounds<float3>> bounds = bounds_min_max_with_transform(
        obedit->object_to_world(), pointcloud.positions(), mask);

    if (bounds) {
      minmax_v3v3_v3(r_min, r_max, bounds->min);
      minmax_v3v3_v3(r_min, r_max, bounds->max);
      return true;
    }
    return false;
  }
  if (obedit->type == OB_CURVES) {
    const Object &ob_orig = *DEG_get_original(obedit);
    const Curves &curves_id = *static_cast<const Curves *>(ob_orig.data);
    const bke::CurvesGeometry &curves = curves_id.geometry.wrap();

    IndexMaskMemory memory;
    const IndexMask mask = curves::retrieve_selected_points(curves, memory);

    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(obedit, ob_orig);

    const std::optional<Bounds<float3>> bounds = bounds_min_max_with_transform(
        obedit->object_to_world(), deformation.positions, mask);

    if (bounds) {
      minmax_v3v3_v3(r_min, r_max, bounds->min);
      minmax_v3v3_v3(r_min, r_max, bounds->max);
      return true;
    }
    return false;
  }
  if (obedit->type == OB_GREASE_PENCIL) {
    Object &ob_orig = *DEG_get_original(obedit);
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob_orig.data);

    std::optional<Bounds<float3>> bounds = std::nullopt;

    const Vector<greasepencil::MutableDrawingInfo> drawings =
        greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
    for (const greasepencil::MutableDrawingInfo info : drawings) {
      const bke::CurvesGeometry &curves = info.drawing.strokes();
      if (curves.is_empty()) {
        continue;
      }

      IndexMaskMemory memory;
      const IndexMask points = greasepencil::retrieve_editable_and_selected_points(
          ob_orig, info.drawing, info.layer_index, memory);
      if (points.is_empty()) {
        continue;
      }

      const bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
              obedit, ob_orig, info.drawing);

      const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
      const float4x4 layer_to_world = layer.to_world_space(*obedit);

      bounds = bounds::merge(
          bounds, bounds_min_max_with_transform(layer_to_world, deformation.positions, points));
    }

    if (bounds) {
      minmax_v3v3_v3(r_min, r_max, bounds->min);
      minmax_v3v3_v3(r_min, r_max, bounds->max);
      return true;
    }
    return false;
  }

  if (ED_transverts_check_obedit(obedit)) {
    ED_transverts_create_from_obedit(&tvs, obedit, TM_ALL_JOINTS | TM_CALC_MAPLOC);
  }

  if (tvs.transverts_tot == 0) {
    return false;
  }

  copy_m3_m4(bmat, obedit->object_to_world().ptr());

  tv = tvs.transverts;
  for (int a = 0; a < tvs.transverts_tot; a++, tv++) {
    copy_v3_v3(vec, (tv->flag & TX_VERT_USE_MAPLOC) ? tv->maploc : tv->loc);
    mul_m3_v3(bmat, vec);
    add_v3_v3(vec, obedit->object_to_world().location());
    add_v3_v3(centroid, vec);
    minmax_v3v3_v3(r_min, r_max, vec);
  }

  ED_transverts_free(&tvs);

  return true;
}

/** \} */
