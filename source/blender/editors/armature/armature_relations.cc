/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 * Operators for relations between bones and for transferring bones between armature objects.
 */

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_fcurve_driver.h"
#include "BKE_idprop.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"

#include "armature_intern.hh"

using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Edit Armature Join
 *
 * \note No operator define here as this is exported to the Object-level operator.
 * \{ */

static void joined_armature_fix_links_constraints(Main *bmain,
                                                  Object *ob,
                                                  Object *tarArm,
                                                  Object *srcArm,
                                                  bPoseChannel *pchan,
                                                  EditBone *curbone,
                                                  ListBase *lb)
{
  bool changed = false;

  LISTBASE_FOREACH (bConstraint *, con, lb) {
    ListBase targets = {nullptr, nullptr};

    /* constraint targets */
    if (BKE_constraint_targets_get(con, &targets)) {
      LISTBASE_FOREACH (bConstraintTarget *, ct, &targets) {
        if (ct->tar == srcArm) {
          if (ct->subtarget[0] == '\0') {
            ct->tar = tarArm;
            changed = true;
          }
          else if (STREQ(ct->subtarget, pchan->name)) {
            ct->tar = tarArm;
            STRNCPY_UTF8(ct->subtarget, curbone->name);
            changed = true;
          }
        }
      }

      BKE_constraint_targets_flush(con, &targets, false);
    }

    /* action constraint? (pose constraints only) */
    if (con->type == CONSTRAINT_TYPE_ACTION) {
      bActionConstraint *data = static_cast<bActionConstraint *>(con->data);

      if (data->act) {
        BKE_action_fix_paths_rename(&tarArm->id,
                                    data->act,
                                    data->action_slot_handle,
                                    "pose.bones[",
                                    pchan->name,
                                    curbone->name,
                                    0,
                                    0,
                                    false);

        DEG_id_tag_update_ex(bmain, &data->act->id, ID_RECALC_SYNC_TO_EVAL);
      }
    }
  }

  if (changed) {
    DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_SYNC_TO_EVAL);
  }
}

/* Callback to pass to BKE_animdata_main_cb() for fixing driver ID's to point to the new ID. */
/* FIXME: For now, we only care about drivers here.
 *        When editing rigs, it's very rare to have animation on the rigs being edited already,
 *        so it should be safe to skip these.
 */
static void joined_armature_fix_animdata_cb(
    Main *bmain, ID *id, FCurve *fcu, Object *srcArm, Object *tarArm, GHash *names_map)
{
  ID *src_id = &srcArm->id;
  ID *dst_id = &tarArm->id;

  GHashIterator gh_iter;
  bool changed = false;

  /* Fix paths - If this is the target object, it will have some "dirty" paths */
  if ((id == src_id) && strstr(fcu->rna_path, "pose.bones[")) {
    GHASH_ITER (gh_iter, names_map) {
      const char *old_name = static_cast<const char *>(BLI_ghashIterator_getKey(&gh_iter));
      const char *new_name = static_cast<const char *>(BLI_ghashIterator_getValue(&gh_iter));

      /* only remap if changed; this still means there will be some
       * waste if there aren't many drivers/keys */
      if (!STREQ(old_name, new_name) && strstr(fcu->rna_path, old_name)) {
        fcu->rna_path = BKE_animsys_fix_rna_path_rename(
            id, fcu->rna_path, "pose.bones", old_name, new_name, 0, 0, false);

        changed = true;

        /* we don't want to apply a second remapping on this driver now,
         * so stop trying names, but keep fixing drivers
         */
        break;
      }
    }
  }

  /* Driver targets */
  if (fcu->driver) {
    ChannelDriver *driver = fcu->driver;

    /* Ensure that invalid drivers gets re-evaluated in case they become valid once the join
     * operation is finished. */
    fcu->flag &= ~FCURVE_DISABLED;
    driver->flag &= ~DRIVER_FLAG_INVALID;

    /* Fix driver references to invalid ID's */
    LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
      /* only change the used targets, since the others will need fixing manually anyway */
      DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
        /* change the ID's used... */
        if (dtar->id == src_id) {
          dtar->id = dst_id;

          changed = true;

          /* also check on the subtarget...
           * XXX: We duplicate the logic from drivers_path_rename_fix() here, with our own
           *      little twists so that we know that it isn't going to clobber the wrong data
           */
          if ((dtar->rna_path && strstr(dtar->rna_path, "pose.bones[")) || (dtar->pchan_name[0])) {
            GHASH_ITER (gh_iter, names_map) {
              const char *old_name = static_cast<const char *>(BLI_ghashIterator_getKey(&gh_iter));
              const char *new_name = static_cast<const char *>(
                  BLI_ghashIterator_getValue(&gh_iter));

              /* only remap if changed */
              if (!STREQ(old_name, new_name)) {
                if ((dtar->rna_path) && strstr(dtar->rna_path, old_name)) {
                  /* Fix up path */
                  dtar->rna_path = BKE_animsys_fix_rna_path_rename(
                      id, dtar->rna_path, "pose.bones", old_name, new_name, 0, 0, false);
                  break; /* no need to try any more names for bone path */
                }
                if (STREQ(dtar->pchan_name, old_name)) {
                  /* Change target bone name */
                  STRNCPY_UTF8(dtar->pchan_name, new_name);
                  break; /* no need to try any more names for bone subtarget */
                }
              }
            }
          }
        }
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }

  if (changed) {
    DEG_id_tag_update_ex(bmain, id, ID_RECALC_SYNC_TO_EVAL);
  }
}

/* Helper function for armature joining - link fixing */
static void joined_armature_fix_links(
    Main *bmain, Object *tarArm, Object *srcArm, bPoseChannel *pchan, EditBone *curbone)
{
  Object *ob;
  bPose *pose;

  /* let's go through all objects in database */
  for (ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
    /* do some object-type specific things */
    if (ob->type == OB_ARMATURE) {
      pose = ob->pose;
      LISTBASE_FOREACH (bPoseChannel *, pchant, &pose->chanbase) {
        joined_armature_fix_links_constraints(
            bmain, ob, tarArm, srcArm, pchan, curbone, &pchant->constraints);
      }
    }

    /* fix object-level constraints */
    if (ob != srcArm) {
      joined_armature_fix_links_constraints(
          bmain, ob, tarArm, srcArm, pchan, curbone, &ob->constraints);
    }

    /* See if an object is parented to this armature */
    if (ob->parent && (ob->parent == srcArm)) {
      /* Is object parented to a bone of this src armature? */
      if (ob->partype == PARBONE) {
        /* bone name in object */
        if (STREQ(ob->parsubstr, pchan->name)) {
          STRNCPY_UTF8(ob->parsubstr, curbone->name);
        }
      }

      /* make tar armature be new parent */
      ob->parent = tarArm;

      DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
}

static BoneCollection *join_armature_remap_collection(
    const bArmature *src_arm,
    const int src_index,
    bArmature *dest_arm,
    blender::Map<std::string, BoneCollection *> &bone_collection_by_name)
{
  using namespace blender::animrig;
  const BoneCollection *bcoll = src_arm->collection_array[src_index];

  /* Check if already remapped. */
  BoneCollection *mapped = bone_collection_by_name.lookup_default(bcoll->name, nullptr);

  if (mapped) {
    return mapped;
  }

  /* Remap the parent collection if necessary. */
  const int src_parent_index = armature_bonecoll_find_parent_index(src_arm, src_index);
  int parent_index = -1;

  if (src_parent_index >= 0) {
    BoneCollection *mapped_parent = join_armature_remap_collection(
        src_arm, src_parent_index, dest_arm, bone_collection_by_name);

    if (mapped_parent) {
      parent_index = armature_bonecoll_find_index(dest_arm, mapped_parent);
    }
  }

  /* Create the new collection instance. */
  BoneCollection *new_bcoll = ANIM_armature_bonecoll_new(dest_arm, bcoll->name, parent_index);

  /* Copy collection visibility. */
  new_bcoll->flags = bcoll->flags;

  /* Copy custom properties. */
  if (bcoll->prop) {
    new_bcoll->prop = IDP_CopyProperty_ex(bcoll->prop, 0);
  }
  if (bcoll->system_properties) {
    new_bcoll->system_properties = IDP_CopyProperty_ex(bcoll->system_properties, 0);
  }

  bone_collection_by_name.add(bcoll->name, new_bcoll);
  return new_bcoll;
}

wmOperatorStatus ED_armature_join_objects_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob_active = CTX_data_active_object(C);
  bArmature *arm = static_cast<bArmature *>((ob_active) ? ob_active->data : nullptr);
  bPose *pose, *opose;
  bPoseChannel *pchan, *pchann;
  EditBone *curbone;
  float mat[4][4], oimat[4][4];
  bool ok = false;

  /* Ensure we're not in edit-mode and that the active object is an armature. */
  if (!ob_active || ob_active->type != OB_ARMATURE) {
    return OPERATOR_CANCELLED;
  }
  if (!arm || arm->edbo) {
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == ob_active) {
      ok = true;
      break;
    }
  }
  CTX_DATA_END;

  /* that way the active object is always selected */
  if (ok == false) {
    BKE_report(op->reports, RPT_WARNING, "Active object is not a selected armature");
    return OPERATOR_CANCELLED;
  }

  /* Check that there are no shared Armatures, as the code below assumes that
   * each to-be-joined Armature is unique. */
  {
    blender::Set<const bArmature *> seen_armatures;
    CTX_DATA_BEGIN (C, const Object *, ob_iter, selected_editable_objects) {
      if (ob_iter->type != OB_ARMATURE) {
        continue;
      }

      const bArmature *armature = static_cast<bArmature *>(ob_iter->data);
      if (seen_armatures.add(armature)) {
        /* Armature pointer was added to the set, which means it wasn't seen before. */
        continue;
      }

      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Cannot join objects that share armature data: %s",
                  armature->id.name + 2);
      return OPERATOR_CANCELLED;
    }
    CTX_DATA_END;
  }

  /* Inverse transform for all selected armatures in this object,
   * See #object_join_exec for detailed comment on why the safe version is used. */
  invert_m4_m4_safe_ortho(oimat, ob_active->object_to_world().ptr());

  /* Index bone collections by name.  This is also used later to keep track
   * of collections added from other armatures. */
  blender::Map<std::string, BoneCollection *> bone_collection_by_name;
  for (BoneCollection *bcoll : arm->collections_span()) {
    bone_collection_by_name.add(bcoll->name, bcoll);
  }

  /* Used to track how bone collections should be remapped after merging
   * other armatures. */
  blender::Map<BoneCollection *, BoneCollection *> bone_collection_remap;

  /* Get edit-bones of active armature to add edit-bones to */
  ED_armature_to_edit(arm);

  /* Get pose of active object and move it out of pose-mode */
  pose = ob_active->pose;
  ob_active->mode &= ~OB_MODE_POSE;

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if ((ob_iter->type == OB_ARMATURE) && (ob_iter != ob_active)) {
      bArmature *curarm = static_cast<bArmature *>(ob_iter->data);

      /* we assume that each armature datablock is only used in a single place */
      BLI_assert(ob_active->data != ob_iter->data);

      /* init callback data for fixing up AnimData links later */
      GHash *names_map = BLI_ghash_str_new("join_armature_adt_fix");

      /* Make a list of edit-bones in current armature */
      ED_armature_to_edit(curarm);

      /* Copy new bone collections, and store their remapping info. */
      for (int i = 0; i < curarm->collection_array_num; i++) {
        BoneCollection *mapped = join_armature_remap_collection(
            curarm, i, arm, bone_collection_by_name);

        bone_collection_remap.add(curarm->collection_array[i], mapped);
      }

      /* Get Pose of current armature */
      opose = ob_iter->pose;
      ob_iter->mode &= ~OB_MODE_POSE;
      // BASACT->flag &= ~OB_MODE_POSE;

      /* Find the difference matrix */
      mul_m4_m4m4(mat, oimat, ob_iter->object_to_world().ptr());

      /* Copy bones and pose-channels from the object to the edit armature. */
      for (pchan = static_cast<bPoseChannel *>(opose->chanbase.first); pchan; pchan = pchann) {
        pchann = pchan->next;
        curbone = ED_armature_ebone_find_name(curarm->edbo, pchan->name);

        /* Get new name */
        ED_armature_ebone_unique_name(arm->edbo, curbone->name, nullptr);
        BLI_ghash_insert(names_map, BLI_strdup(pchan->name), curbone->name);

        /* Transform the bone */
        {
          float premat[4][4];
          float postmat[4][4];
          float difmat[4][4];
          float imat[4][4];
          float temp[3][3];

          /* Get the premat */
          ED_armature_ebone_to_mat3(curbone, temp);

          unit_m4(premat); /* mul_m4_m3m4 only sets 3x3 part */
          mul_m4_m3m4(premat, temp, mat);

          mul_m4_v3(mat, curbone->head);
          mul_m4_v3(mat, curbone->tail);

          /* Get the postmat */
          ED_armature_ebone_to_mat3(curbone, temp);
          copy_m4_m3(postmat, temp);

          /* Find the roll */
          invert_m4_m4(imat, premat);
          mul_m4_m4m4(difmat, imat, postmat);

          curbone->roll -= atan2f(difmat[2][0], difmat[2][2]);
        }

        /* Fix Constraints and Other Links to this Bone and Armature */
        joined_armature_fix_links(bmain, ob_active, ob_iter, pchan, curbone);

        /* Rename pchan */
        STRNCPY_UTF8(pchan->name, curbone->name);

        /* Jump Ship! */
        BLI_remlink(curarm->edbo, curbone);
        BLI_addtail(arm->edbo, curbone);

        /* Pose channel is moved from one storage to another, its UUID is still unique. */
        BLI_remlink(&opose->chanbase, pchan);
        BLI_addtail(&pose->chanbase, pchan);
        BKE_pose_channels_hash_free(opose);
        BKE_pose_channels_hash_free(pose);

        /* Remap collections. */
        LISTBASE_FOREACH (BoneCollectionReference *, bcoll_ref, &curbone->bone_collections) {
          bcoll_ref->bcoll = bone_collection_remap.lookup(bcoll_ref->bcoll);
        }
      }

      /* Armature ID itself is not freed below, however it has been modified (and is now completely
       * empty). This needs to be told to the depsgraph, it will also ensure that the global
       * memfile undo system properly detects the change.
       *
       * FIXME: Modifying an existing obdata because we are joining an object using it into another
       * object is a very questionable behavior, which also does not match with other object types
       * joining. */
      DEG_id_tag_update_ex(bmain, &curarm->id, ID_RECALC_GEOMETRY);

      /* Fix all the drivers (and animation data) */
      BKE_fcurves_main_cb(bmain, [&](ID *id, FCurve *fcu) {
        joined_armature_fix_animdata_cb(bmain, id, fcu, ob_iter, ob_active, names_map);
      });
      BLI_ghash_free(names_map, MEM_freeN, nullptr);

      /* Only copy over animdata now, after all the remapping has been done,
       * so that we don't have to worry about ambiguities re which armature
       * a bone came from!
       */
      if (ob_iter->adt) {
        if (ob_active->adt == nullptr) {
          /* no animdata, so just use a copy of the whole thing */
          ob_active->adt = BKE_animdata_copy(bmain, ob_iter->adt, 0);
        }
        else {
          /* merge in data - we'll fix the drivers manually */
          BKE_animdata_merge_copy(
              bmain, &ob_active->id, &ob_iter->id, ADT_MERGECOPY_KEEP_DST, false);
        }
      }

      if (curarm->adt) {
        if (arm->adt == nullptr) {
          /* no animdata, so just use a copy of the whole thing */
          arm->adt = BKE_animdata_copy(bmain, curarm->adt, 0);
        }
        else {
          /* merge in data - we'll fix the drivers manually */
          BKE_animdata_merge_copy(bmain, &arm->id, &curarm->id, ADT_MERGECOPY_KEEP_DST, false);
        }
      }

      /* Free the old object data */
      blender::ed::object::base_free_and_unlink(bmain, scene, ob_iter);
    }
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain); /* because we removed object(s) */

  ED_armature_from_edit(bmain, arm);
  ED_armature_edit_free(arm);

  /* Make sure to recompute bone collection visibility. */
  ANIM_armature_runtime_refresh(arm);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Armature Separate
 * \{ */

/* Helper function for armature separating - link fixing */
static void separated_armature_fix_links(Main *bmain, Object *origArm, Object *newArm)
{
  Object *ob;
  ListBase *opchans, *npchans;

  /* Get reference to list of bones in original and new armatures. */
  opchans = &origArm->pose->chanbase;
  npchans = &newArm->pose->chanbase;

  /* let's go through all objects in database */
  for (ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
    /* do some object-type specific things */
    if (ob->type == OB_ARMATURE) {
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
          ListBase targets = {nullptr, nullptr};

          /* constraint targets */
          if (BKE_constraint_targets_get(con, &targets)) {
            LISTBASE_FOREACH (bConstraintTarget *, ct, &targets) {
              /* Any targets which point to original armature
               * are redirected to the new one only if:
               * - The target isn't origArm/newArm itself.
               * - The target is one that can be found in newArm/origArm.
               */
              if (ct->subtarget[0] != 0) {
                if (ct->tar == origArm) {
                  if (BLI_findstring(npchans, ct->subtarget, offsetof(bPoseChannel, name))) {
                    ct->tar = newArm;
                  }
                }
                else if (ct->tar == newArm) {
                  if (BLI_findstring(opchans, ct->subtarget, offsetof(bPoseChannel, name))) {
                    ct->tar = origArm;
                  }
                }
              }
            }

            BKE_constraint_targets_flush(con, &targets, false);
          }
        }
      }
    }

    /* fix object-level constraints */
    if (ob != origArm) {
      LISTBASE_FOREACH (bConstraint *, con, &ob->constraints) {
        ListBase targets = {nullptr, nullptr};

        /* constraint targets */
        if (BKE_constraint_targets_get(con, &targets)) {
          LISTBASE_FOREACH (bConstraintTarget *, ct, &targets) {
            /* any targets which point to original armature are redirected to the new one only if:
             * - the target isn't origArm/newArm itself
             * - the target is one that can be found in newArm/origArm
             */
            if (ct->subtarget[0] != '\0') {
              if (ct->tar == origArm) {
                if (BLI_findstring(npchans, ct->subtarget, offsetof(bPoseChannel, name))) {
                  ct->tar = newArm;
                }
              }
              else if (ct->tar == newArm) {
                if (BLI_findstring(opchans, ct->subtarget, offsetof(bPoseChannel, name))) {
                  ct->tar = origArm;
                }
              }
            }
          }

          BKE_constraint_targets_flush(con, &targets, false);
        }
      }
    }

    /* See if an object is parented to this armature */
    if (ob->parent && (ob->parent == origArm)) {
      /* Is object parented to a bone of this src armature? */
      if ((ob->partype == PARBONE) && (ob->parsubstr[0] != '\0')) {
        if (BLI_findstring(npchans, ob->parsubstr, offsetof(bPoseChannel, name))) {
          ob->parent = newArm;
        }
      }
    }
  }
}

/**
 * Helper function for armature separating - remove certain bones from the given armature.
 *
 * \param ob: Armature object (must not be is not in edit-mode).
 * \param is_select: remove selected bones from the armature,
 * otherwise the unselected bones are removed.
 */
static void separate_armature_bones(Main *bmain, Object *ob, const bool is_select)
{
  bArmature *arm = static_cast<bArmature *>(ob->data);
  bPoseChannel *pchan, *pchann;
  EditBone *curbone;

  /* make local set of edit-bones to manipulate here */
  ED_armature_to_edit(arm);

  /* go through pose-channels, checking if a bone should be removed */
  for (pchan = static_cast<bPoseChannel *>(ob->pose->chanbase.first); pchan; pchan = pchann) {
    pchann = pchan->next;
    curbone = ED_armature_ebone_find_name(arm->edbo, pchan->name);

    /* check if bone needs to be removed */
    if (is_select == blender::animrig::bone_is_selected(arm, curbone)) {

      /* Clear the bone->parent var of any bone that had this as its parent. */
      LISTBASE_FOREACH (EditBone *, ebo, arm->edbo) {
        if (ebo->parent == curbone) {
          ebo->parent = nullptr;
          /* this is needed to prevent random crashes with in ED_armature_from_edit */
          ebo->temp.p = nullptr;
          ebo->flag &= ~BONE_CONNECTED;
        }
      }

      /* clear the pchan->parent var of any pchan that had this as its parent */
      LISTBASE_FOREACH (bPoseChannel *, pchn, &ob->pose->chanbase) {
        if (pchn->parent == pchan) {
          pchn->parent = nullptr;
        }
        if (pchn->bbone_next == pchan) {
          pchn->bbone_next = nullptr;
        }
        if (pchn->bbone_prev == pchan) {
          pchn->bbone_prev = nullptr;
        }
      }

      /* Free any of the extra-data this pchan might have. */
      BKE_pose_channel_free(pchan);
      BKE_pose_channels_hash_free(ob->pose);

      /* get rid of unneeded bone */
      bone_free(arm, curbone);
      BLI_freelinkN(&ob->pose->chanbase, pchan);
    }
  }

  /* Exit edit-mode (recalculates pose-channels too). */
  ED_armature_edit_deselect_all(ob);
  ED_armature_from_edit(bmain, static_cast<bArmature *>(ob->data));
  ED_armature_edit_free(static_cast<bArmature *>(ob->data));
}

/* separate selected bones into their armature */
static wmOperatorStatus separate_armature_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  bool ok = false;

  /* set wait cursor in case this takes a while */
  WM_cursor_wait(true);

  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Base *base_old : bases) {
    Object *ob_old = base_old->object;

    {
      bArmature *arm_old = static_cast<bArmature *>(ob_old->data);
      bool has_selected_bone = false;
      bool has_selected_any = false;
      LISTBASE_FOREACH (EditBone *, ebone, arm_old->edbo) {
        if (blender::animrig::bone_is_visible(arm_old, ebone)) {
          if (ebone->flag & BONE_SELECTED) {
            has_selected_bone = true;
            break;
          }
          if (ebone->flag & (BONE_TIPSEL | BONE_ROOTSEL)) {
            has_selected_any = true;
          }
        }
      }
      if (has_selected_bone == false) {
        if (has_selected_any) {
          /* Without this, we may leave head/tail selected
           * which isn't expected after separating. */
          ED_armature_edit_deselect_all(ob_old);
        }
        continue;
      }
    }

    /* We are going to do this as follows (unlike every other instance of separate):
     * 1. Exit edit-mode & pose-mode for active armature/base. Take note of what this is.
     * 2. Duplicate base - BASACT is the new one now
     * 3. For each of the two armatures,
     *    enter edit-mode -> remove appropriate bones -> exit edit-mode + recalculate.
     * 4. Fix constraint links
     * 5. Make original armature active and enter edit-mode
     */

    /* 1) store starting settings and exit edit-mode */
    ob_old->mode &= ~OB_MODE_POSE;

    ED_armature_from_edit(bmain, static_cast<bArmature *>(ob_old->data));
    ED_armature_edit_free(static_cast<bArmature *>(ob_old->data));

    /* 2) duplicate base */

    /* Only duplicate linked armature but take into account
     * user preferences for duplicating actions. */
    short dupflag = USER_DUP_ARM | (U.dupflag & USER_DUP_ACT);
    Base *base_new = blender::ed::object::add_duplicate(
        bmain, scene, view_layer, base_old, eDupli_ID_Flags(dupflag));
    Object *ob_new = base_new->object;

    DEG_relations_tag_update(bmain);

    /* 3) remove bones that shouldn't still be around on both armatures */
    separate_armature_bones(bmain, ob_old, true);
    separate_armature_bones(bmain, ob_new, false);

    /* 4) fix links before depsgraph flushes, err... or after? */
    separated_armature_fix_links(bmain, ob_old, ob_new);

    DEG_id_tag_update(&ob_old->id, ID_RECALC_GEOMETRY); /* this is the original one */
    DEG_id_tag_update(&ob_new->id, ID_RECALC_GEOMETRY); /* this is the separated one */

    /* 5) restore original conditions */
    ED_armature_to_edit(static_cast<bArmature *>(ob_old->data));

    /* parents tips remain selected when connected children are removed. */
    ED_armature_edit_deselect_all(ob_old);

    ok = true;

    /* NOTE: notifier might evolve. */
    WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob_old);
  }

  /* Recalculate/redraw + cleanup */
  WM_cursor_wait(false);

  if (ok) {
    BKE_report(op->reports, RPT_INFO, "Separated bones");
    ED_outliner_select_sync_from_object_tag(C);
  }

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_separate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Separate Bones";
  ot->idname = "ARMATURE_OT_separate";
  ot->description = "Isolate selected bones into a separate armature";

  /* callbacks */
  ot->exec = separate_armature_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Armature Parenting
 * \{ */

/* armature parenting options */
#define ARM_PAR_CONNECT 1
#define ARM_PAR_OFFSET 2

/* armature un-parenting options */
#define ARM_PAR_CLEAR 1
#define ARM_PAR_CLEAR_DISCONNECT 2

/* check for null, before calling! */
static void bone_connect_to_existing_parent(EditBone *bone)
{
  bone->flag |= BONE_CONNECTED;
  copy_v3_v3(bone->head, bone->parent->tail);
  bone->rad_head = bone->parent->rad_tail;
}

static void bone_connect_to_new_parent(ListBase *edbo,
                                       EditBone *selbone,
                                       EditBone *actbone,
                                       short mode)
{
  EditBone *ebone;
  float offset[3];

  if ((selbone->parent) && (selbone->flag & BONE_CONNECTED)) {
    selbone->parent->flag &= ~BONE_TIPSEL;
  }

  /* make actbone the parent of selbone */
  selbone->parent = actbone;

  /* in actbone tree we cannot have a loop */
  for (ebone = actbone->parent; ebone; ebone = ebone->parent) {
    if (ebone->parent == selbone) {
      ebone->parent = nullptr;
      ebone->flag &= ~BONE_CONNECTED;
    }
  }

  if (mode == ARM_PAR_CONNECT) {
    /* Connected: Child bones will be moved to the parent tip */
    selbone->flag |= BONE_CONNECTED;
    sub_v3_v3v3(offset, actbone->tail, selbone->head);

    copy_v3_v3(selbone->head, actbone->tail);
    selbone->rad_head = actbone->rad_tail;

    add_v3_v3(selbone->tail, offset);

    /* offset for all its children */
    LISTBASE_FOREACH (EditBone *, ebone, edbo) {
      EditBone *par;

      for (par = ebone->parent; par; par = par->parent) {
        if (par == selbone) {
          add_v3_v3(ebone->head, offset);
          add_v3_v3(ebone->tail, offset);
          break;
        }
      }
    }
  }
  else {
    /* Offset: Child bones will retain their distance from the parent tip */
    selbone->flag &= ~BONE_CONNECTED;
  }
}

static const EnumPropertyItem prop_editarm_make_parent_types[] = {
    {ARM_PAR_CONNECT, "CONNECTED", 0, "Connected", ""},
    {ARM_PAR_OFFSET, "OFFSET", 0, "Keep Offset", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus armature_parent_set_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_edit_object(C);
  bArmature *arm = static_cast<bArmature *>(ob->data);
  EditBone *actbone = CTX_data_active_bone(C);
  EditBone *actmirb = nullptr;
  short val = RNA_enum_get(op->ptr, "type");

  /* there must be an active bone */
  if (actbone == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Operation requires an active bone");
    return OPERATOR_CANCELLED;
  }
  if (arm->flag & ARM_MIRROR_EDIT) {
    /* For X-Axis Mirror Editing option, we may need a mirror copy of actbone:
     * - If there's a mirrored copy of selbone, try to find a mirrored copy of actbone
     *   (i.e.  selbone="child.L" and actbone="parent.L", find "child.R" and "parent.R").
     *   This is useful for arm-chains, for example parenting lower arm to upper arm.
     * - If there's no mirrored copy of actbone (i.e. actbone = "parent.C" or "parent")
     *   then just use actbone. Useful when doing upper arm to spine.
     */
    actmirb = ED_armature_ebone_get_mirrored(arm->edbo, actbone);
    if (actmirb == nullptr) {
      actmirb = actbone;
    }
  }

  /* If there is only 1 selected bone, we assume that it is the active bone,
   * since a user will need to have clicked on a bone (thus selecting it) to make it active. */
  bool is_active_only_selected = false;
  if (actbone->flag & BONE_SELECTED) {
    is_active_only_selected = true;
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_EDITABLE(ebone)) {
        if (ebone != actbone) {
          is_active_only_selected = false;
          break;
        }
      }
    }
  }

  if (is_active_only_selected) {
    /* When only the active bone is selected, and it has a parent,
     * connect it to the parent, as that is the only possible outcome.
     */
    if (actbone->parent) {
      bone_connect_to_existing_parent(actbone);

      if ((arm->flag & ARM_MIRROR_EDIT) && (actmirb->parent)) {
        bone_connect_to_existing_parent(actmirb);
      }
    }
  }
  else {
    /* Parent 'selected' bones to the active one:
     * - The context iterator contains both selected bones and their mirrored copies,
     *   so we assume that unselected bones are mirrored copies of some selected bone.
     * - Since the active one (and/or its mirror) will also be selected, we also need
     *   to check that we are not trying to operate on them, since such an operation
     *   would cause errors.
     */

    /* Parent selected bones to the active one. */
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_EDITABLE(ebone)) {
        if (ebone != actbone) {
          bone_connect_to_new_parent(arm->edbo, ebone, actbone, val);
        }

        if (arm->flag & ARM_MIRROR_EDIT) {
          EditBone *ebone_mirror = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
          if (ebone_mirror && (ebone_mirror->flag & BONE_SELECTED) == 0) {
            if (ebone_mirror != actmirb) {
              bone_connect_to_new_parent(arm->edbo, ebone_mirror, actmirb, val);
            }
          }
        }
      }
    }
  }

  /* NOTE: notifier might evolve. */
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_SELECT);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus armature_parent_set_invoke(bContext *C,
                                                   wmOperator * /*op*/,
                                                   const wmEvent * /*event*/)
{
  /* False when all selected bones are parented to the active bone. */
  bool enable_offset = false;
  /* False when all selected bones are connected to the active bone. */
  bool enable_connect = false;
  {
    Object *ob = CTX_data_edit_object(C);
    bArmature *arm = static_cast<bArmature *>(ob->data);
    EditBone *actbone = arm->act_edbone;
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (!EBONE_EDITABLE(ebone) || !(ebone->flag & BONE_SELECTED)) {
        continue;
      }
      if (ebone == actbone) {
        continue;
      }

      if (ebone->parent != actbone) {
        enable_offset = true;
        enable_connect = true;
        break;
      }
      if (!(ebone->flag & BONE_CONNECTED)) {
        enable_connect = true;
      }
    }
  }

  uiPopupMenu *pup = UI_popup_menu_begin(
      C, CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Make Parent"), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  uiLayout *row_offset = &layout->row(false);
  row_offset->enabled_set(enable_offset);
  PointerRNA op_ptr = row_offset->op("ARMATURE_OT_parent_set", IFACE_("Keep Offset"), ICON_NONE);
  RNA_enum_set(&op_ptr, "type", ARM_PAR_OFFSET);

  uiLayout *row_connect = &layout->row(false);
  row_connect->enabled_set(enable_connect);
  op_ptr = row_connect->op("ARMATURE_OT_parent_set", IFACE_("Connected"), ICON_NONE);
  RNA_enum_set(&op_ptr, "type", ARM_PAR_CONNECT);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void ARMATURE_OT_parent_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Parent";
  ot->idname = "ARMATURE_OT_parent_set";
  ot->description = "Set the active bone as the parent of the selected bones";

  /* API callbacks. */
  ot->invoke = armature_parent_set_invoke;
  ot->exec = armature_parent_set_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "type", prop_editarm_make_parent_types, 0, "Parent Type", "Type of parenting");
}

static const EnumPropertyItem prop_editarm_clear_parent_types[] = {
    {ARM_PAR_CLEAR, "CLEAR", 0, "Clear Parent", ""},
    {ARM_PAR_CLEAR_DISCONNECT, "DISCONNECT", 0, "Disconnect Bone", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void editbone_clear_parent(EditBone *ebone, int mode)
{
  if (ebone->parent) {
    /* for nice selection */
    ebone->parent->flag &= ~BONE_TIPSEL;
  }

  if (mode == 1) {
    ebone->parent = nullptr;
  }
  ebone->flag &= ~BONE_CONNECTED;
}

static wmOperatorStatus armature_parent_clear_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const int val = RNA_enum_get(op->ptr, "type");

  CTX_DATA_BEGIN (C, EditBone *, ebone, selected_editable_bones) {
    editbone_clear_parent(ebone, val);
  }
  CTX_DATA_END;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *ob : objects) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bool changed = false;

    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (EBONE_EDITABLE(ebone)) {
        changed = true;
        break;
      }
    }

    if (!changed) {
      continue;
    }

    ED_armature_edit_sync_selection(arm->edbo);

    /* NOTE: notifier might evolve. */
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
  }
  return OPERATOR_FINISHED;
}

static wmOperatorStatus armature_parent_clear_invoke(bContext *C,
                                                     wmOperator * /*op*/,
                                                     const wmEvent * /*event*/)
{
  /* False when no selected bones are connected to the active bone. */
  bool enable_disconnect = false;
  /* False when no selected bones are parented to the active bone. */
  bool enable_clear = false;
  {
    Object *ob = CTX_data_edit_object(C);
    bArmature *arm = static_cast<bArmature *>(ob->data);
    LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
      if (!EBONE_EDITABLE(ebone) || !(ebone->flag & BONE_SELECTED)) {
        continue;
      }
      if (ebone->parent == nullptr) {
        continue;
      }
      enable_clear = true;

      if (ebone->flag & BONE_CONNECTED) {
        enable_disconnect = true;
        break;
      }
    }
  }

  uiPopupMenu *pup = UI_popup_menu_begin(
      C, CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Parent"), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  uiLayout *row_clear = &layout->row(false);
  row_clear->enabled_set(enable_clear);
  PointerRNA op_ptr = row_clear->op("ARMATURE_OT_parent_clear", IFACE_("Clear Parent"), ICON_NONE);
  RNA_enum_set(&op_ptr, "type", ARM_PAR_CLEAR);

  uiLayout *row_disconnect = &layout->row(false);
  row_disconnect->enabled_set(enable_disconnect);
  op_ptr = row_disconnect->op("ARMATURE_OT_parent_clear", IFACE_("Disconnect Bone"), ICON_NONE);
  RNA_enum_set(&op_ptr, "type", ARM_PAR_CLEAR_DISCONNECT);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void ARMATURE_OT_parent_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Parent";
  ot->idname = "ARMATURE_OT_parent_clear";
  ot->description =
      "Remove the parent-child relationship between selected bones and their parents";

  /* API callbacks. */
  ot->invoke = armature_parent_clear_invoke;
  ot->exec = armature_parent_clear_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          prop_editarm_clear_parent_types,
                          0,
                          "Clear Type",
                          "What way to clear parenting");
}

/** \} */
