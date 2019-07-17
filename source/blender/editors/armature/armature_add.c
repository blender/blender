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
 * Operators and API's for creating bones
 */

/** \file
 * \ingroup edarmature
 */

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_ghash.h"
#include "BLI_string_utils.h"

#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_deform.h"
#include "BKE_layer.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "armature_intern.h"

/* *************** Adding stuff in editmode *************** */

/* default bone add, returns it selected, but without tail set */
/* XXX should be used everywhere, now it mallocs bones still locally in functions */
EditBone *ED_armature_ebone_add(bArmature *arm, const char *name)
{
  EditBone *bone = MEM_callocN(sizeof(EditBone), "eBone");

  BLI_strncpy(bone->name, name, sizeof(bone->name));
  ED_armature_ebone_unique_name(arm->edbo, bone->name, NULL);

  BLI_addtail(arm->edbo, bone);

  bone->flag |= BONE_TIPSEL;
  bone->weight = 1.0f;
  bone->dist = 0.25f;
  bone->xwidth = 0.1f;
  bone->zwidth = 0.1f;
  bone->rad_head = 0.10f;
  bone->rad_tail = 0.05f;
  bone->segments = 1;
  bone->layer = arm->layer;

  /* Bendy-Bone parameters */
  bone->roll1 = 0.0f;
  bone->roll2 = 0.0f;
  bone->curve_in_x = 0.0f;
  bone->curve_in_y = 0.0f;
  bone->curve_out_x = 0.0f;
  bone->curve_out_y = 0.0f;
  bone->ease1 = 1.0f;
  bone->ease2 = 1.0f;
  bone->scale_in_x = 1.0f;
  bone->scale_in_y = 1.0f;
  bone->scale_out_x = 1.0f;
  bone->scale_out_y = 1.0f;

  return bone;
}

EditBone *ED_armature_ebone_add_primitive(Object *obedit_arm, float length, bool view_aligned)
{
  bArmature *arm = obedit_arm->data;
  EditBone *bone;

  ED_armature_edit_deselect_all(obedit_arm);

  /* Create a bone */
  bone = ED_armature_ebone_add(arm, "Bone");

  arm->act_edbone = bone;

  zero_v3(bone->head);
  zero_v3(bone->tail);

  bone->tail[view_aligned ? 1 : 2] = length;

  return bone;
}

/* previously addvert_armature */
/* the ctrl-click method */

/** Note this is already ported to multi-objects as it is.
 * Since only the active bone is extruded even for single objects,
 * it makes sense to stick to the active object here.
 *
 * If we want the support to be expanded we should something like the
 * offset we do for mesh click extrude.
 */
static int armature_click_extrude_exec(bContext *C, wmOperator *UNUSED(op))
{
  bArmature *arm;
  EditBone *ebone, *newbone, *flipbone;
  float mat[3][3], imat[3][3];
  int a, to_root = 0;
  Object *obedit;
  Scene *scene;

  scene = CTX_data_scene(C);
  obedit = CTX_data_edit_object(C);
  arm = obedit->data;

  /* find the active or selected bone */
  for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
    if (EBONE_VISIBLE(arm, ebone)) {
      if (ebone->flag & BONE_TIPSEL || arm->act_edbone == ebone) {
        break;
      }
    }
  }

  if (ebone == NULL) {
    for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      if (EBONE_VISIBLE(arm, ebone)) {
        if (ebone->flag & BONE_ROOTSEL || arm->act_edbone == ebone) {
          break;
        }
      }
    }
    if (ebone == NULL) {
      return OPERATOR_CANCELLED;
    }

    to_root = 1;
  }

  ED_armature_edit_deselect_all(obedit);

  /* we re-use code for mirror editing... */
  flipbone = NULL;
  if (arm->flag & ARM_MIRROR_EDIT) {
    flipbone = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
  }

  for (a = 0; a < 2; a++) {
    if (a == 1) {
      if (flipbone == NULL) {
        break;
      }
      else {
        SWAP(EditBone *, flipbone, ebone);
      }
    }

    newbone = ED_armature_ebone_add(arm, ebone->name);
    arm->act_edbone = newbone;

    if (to_root) {
      copy_v3_v3(newbone->head, ebone->head);
      newbone->rad_head = ebone->rad_tail;
      newbone->parent = ebone->parent;
    }
    else {
      copy_v3_v3(newbone->head, ebone->tail);
      newbone->rad_head = ebone->rad_tail;
      newbone->parent = ebone;
      newbone->flag |= BONE_CONNECTED;
    }

    const View3DCursor *curs = &scene->cursor;
    copy_v3_v3(newbone->tail, curs->location);
    sub_v3_v3v3(newbone->tail, newbone->tail, obedit->obmat[3]);

    if (a == 1) {
      newbone->tail[0] = -newbone->tail[0];
    }

    copy_m3_m4(mat, obedit->obmat);
    invert_m3_m3(imat, mat);
    mul_m3_v3(imat, newbone->tail);

    newbone->length = len_v3v3(newbone->head, newbone->tail);
    newbone->rad_tail = newbone->length * 0.05f;
    newbone->dist = newbone->length * 0.25f;
  }

  ED_armature_edit_sync_selection(arm->edbo);

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

  return OPERATOR_FINISHED;
}

static int armature_click_extrude_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* TODO most of this code is copied from set3dcursor_invoke,
   * it would be better to reuse code in set3dcursor_invoke */

  /* temporarily change 3d cursor position */
  Scene *scene;
  ARegion *ar;
  View3D *v3d;
  float tvec[3], oldcurs[3], mval_f[2];
  int retv;

  scene = CTX_data_scene(C);
  ar = CTX_wm_region(C);
  v3d = CTX_wm_view3d(C);

  View3DCursor *cursor = &scene->cursor;

  copy_v3_v3(oldcurs, cursor->location);

  copy_v2fl_v2i(mval_f, event->mval);
  ED_view3d_win_to_3d(v3d, ar, cursor->location, mval_f, tvec);
  copy_v3_v3(cursor->location, tvec);

  /* extrude to the where new cursor is and store the operation result */
  retv = armature_click_extrude_exec(C, op);

  /* restore previous 3d cursor position */
  copy_v3_v3(cursor->location, oldcurs);

  return retv;
}

void ARMATURE_OT_click_extrude(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Click-Extrude";
  ot->idname = "ARMATURE_OT_click_extrude";
  ot->description = "Create a new bone going from the last selected joint to the mouse position";

  /* api callbacks */
  ot->invoke = armature_click_extrude_invoke;
  ot->exec = armature_click_extrude_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
}

/* adds an EditBone between the nominated locations (should be in the right space) */
EditBone *add_points_bone(Object *obedit, float head[3], float tail[3])
{
  EditBone *ebo;

  ebo = ED_armature_ebone_add(obedit->data, "Bone");

  copy_v3_v3(ebo->head, head);
  copy_v3_v3(ebo->tail, tail);

  return ebo;
}

static EditBone *get_named_editbone(ListBase *edbo, const char *name)
{
  EditBone *eBone;

  if (name) {
    for (eBone = edbo->first; eBone; eBone = eBone->next) {
      if (STREQ(name, eBone->name)) {
        return eBone;
      }
    }
  }

  return NULL;
}

/* Call this before doing any duplications
 * */
void preEditBoneDuplicate(ListBase *editbones)
{
  /* clear temp */
  ED_armature_ebone_listbase_temp_clear(editbones);
}

/**
 * Helper function for #postEditBoneDuplicate,
 * return the destination pchan from the original.
 */
static bPoseChannel *pchan_duplicate_map(const bPose *pose,
                                         GHash *name_map,
                                         bPoseChannel *pchan_src)
{
  bPoseChannel *pchan_dst = NULL;
  const char *name_src = pchan_src->name;
  const char *name_dst = BLI_ghash_lookup(name_map, name_src);
  if (name_dst) {
    pchan_dst = BKE_pose_channel_find_name(pose, name_dst);
  }

  if (pchan_dst == NULL) {
    pchan_dst = pchan_src;
  }

  return pchan_dst;
}

void postEditBoneDuplicate(struct ListBase *editbones, Object *ob)
{
  if (ob->pose == NULL) {
    return;
  }

  BKE_pose_channels_hash_free(ob->pose);
  BKE_pose_channels_hash_make(ob->pose);

  GHash *name_map = BLI_ghash_str_new(__func__);

  for (EditBone *ebone_src = editbones->first; ebone_src; ebone_src = ebone_src->next) {
    EditBone *ebone_dst = ebone_src->temp.ebone;
    if (!ebone_dst) {
      ebone_dst = ED_armature_ebone_get_mirrored(editbones, ebone_src);
    }
    if (ebone_dst) {
      BLI_ghash_insert(name_map, ebone_src->name, ebone_dst->name);
    }
  }

  for (EditBone *ebone_src = editbones->first; ebone_src; ebone_src = ebone_src->next) {
    EditBone *ebone_dst = ebone_src->temp.ebone;
    if (ebone_dst) {
      bPoseChannel *pchan_src = BKE_pose_channel_find_name(ob->pose, ebone_src->name);
      if (pchan_src) {
        bPoseChannel *pchan_dst = BKE_pose_channel_find_name(ob->pose, ebone_dst->name);
        if (pchan_dst) {
          if (pchan_src->custom_tx) {
            pchan_dst->custom_tx = pchan_duplicate_map(ob->pose, name_map, pchan_src->custom_tx);
          }
          if (pchan_src->bbone_prev) {
            pchan_dst->bbone_prev = pchan_duplicate_map(ob->pose, name_map, pchan_src->bbone_prev);
          }
          if (pchan_src->bbone_next) {
            pchan_dst->bbone_next = pchan_duplicate_map(ob->pose, name_map, pchan_src->bbone_next);
          }
        }
      }
    }
  }

  BLI_ghash_free(name_map, NULL, NULL);
}

/*
 * Note: When duplicating cross objects, editbones here is the list of bones
 * from the SOURCE object but ob is the DESTINATION object
 * */
void updateDuplicateSubtargetObjects(EditBone *dupBone,
                                     ListBase *editbones,
                                     Object *src_ob,
                                     Object *dst_ob)
{
  /* If an edit bone has been duplicated, lets
   * update it's constraints if the subtarget
   * they point to has also been duplicated
   */
  EditBone *oldtarget, *newtarget;
  bPoseChannel *pchan;
  bConstraint *curcon;
  ListBase *conlist;

  if ((pchan = BKE_pose_channel_verify(dst_ob->pose, dupBone->name))) {
    if ((conlist = &pchan->constraints)) {
      for (curcon = conlist->first; curcon; curcon = curcon->next) {
        /* does this constraint have a subtarget in
         * this armature?
         */
        const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(curcon);
        ListBase targets = {NULL, NULL};
        bConstraintTarget *ct;

        if (cti && cti->get_constraint_targets) {
          cti->get_constraint_targets(curcon, &targets);

          for (ct = targets.first; ct; ct = ct->next) {
            if ((ct->tar == src_ob) && (ct->subtarget[0])) {
              ct->tar = dst_ob; /* update target */
              oldtarget = get_named_editbone(editbones, ct->subtarget);
              if (oldtarget) {
                /* was the subtarget bone duplicated too? If
                 * so, update the constraint to point at the
                 * duplicate of the old subtarget.
                 */
                if (oldtarget->temp.ebone) {
                  newtarget = oldtarget->temp.ebone;
                  BLI_strncpy(ct->subtarget, newtarget->name, sizeof(ct->subtarget));
                }
              }
            }
          }

          if (cti->flush_constraint_targets) {
            cti->flush_constraint_targets(curcon, &targets, 0);
          }
        }
      }
    }
  }
}

void updateDuplicateSubtarget(EditBone *dupBone, ListBase *editbones, Object *ob)
{
  updateDuplicateSubtargetObjects(dupBone, editbones, ob, ob);
}

EditBone *duplicateEditBoneObjects(
    EditBone *curBone, const char *name, ListBase *editbones, Object *src_ob, Object *dst_ob)
{
  EditBone *eBone = MEM_mallocN(sizeof(EditBone), "addup_editbone");

  /* Copy data from old bone to new bone */
  memcpy(eBone, curBone, sizeof(EditBone));

  curBone->temp.ebone = eBone;
  eBone->temp.ebone = curBone;

  if (name != NULL) {
    BLI_strncpy(eBone->name, name, sizeof(eBone->name));
  }

  ED_armature_ebone_unique_name(editbones, eBone->name, NULL);
  BLI_addtail(editbones, eBone);

  /* copy the ID property */
  if (curBone->prop) {
    eBone->prop = IDP_CopyProperty(curBone->prop);
  }

  /* Lets duplicate the list of constraints that the
   * current bone has.
   */
  if (src_ob->pose) {
    bPoseChannel *chanold, *channew;

    chanold = BKE_pose_channel_verify(src_ob->pose, curBone->name);
    if (chanold) {
      /* WARNING: this creates a new posechannel, but there will not be an attached bone
       * yet as the new bones created here are still 'EditBones' not 'Bones'.
       */
      channew = BKE_pose_channel_verify(dst_ob->pose, eBone->name);

      if (channew) {
        BKE_pose_channel_copy_data(channew, chanold);
      }
    }
  }

  return eBone;
}

EditBone *duplicateEditBone(EditBone *curBone, const char *name, ListBase *editbones, Object *ob)
{
  return duplicateEditBoneObjects(curBone, name, editbones, ob, ob);
}

static int armature_duplicate_selected_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool do_flip_names = RNA_boolean_get(op->ptr, "do_flip_names");

  /* cancel if nothing selected */
  if (CTX_DATA_COUNT(C, selected_bones) == 0) {
    return OPERATOR_CANCELLED;
  }

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    EditBone *ebone_iter;
    /* The beginning of the duplicated bones in the edbo list */
    EditBone *ebone_first_dupe = NULL;

    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;

    ED_armature_edit_sync_selection(arm->edbo);  // XXX why is this needed?

    preEditBoneDuplicate(arm->edbo);

    /* Select mirrored bones */
    if (arm->flag & ARM_MIRROR_EDIT) {
      for (ebone_iter = arm->edbo->first; ebone_iter; ebone_iter = ebone_iter->next) {
        if (EBONE_VISIBLE(arm, ebone_iter) && (ebone_iter->flag & BONE_SELECTED)) {
          EditBone *ebone;

          ebone = ED_armature_ebone_get_mirrored(arm->edbo, ebone_iter);
          if (ebone) {
            ebone->flag |= BONE_SELECTED;
          }
        }
      }
    }

    /* Find the selected bones and duplicate them as needed */
    for (ebone_iter = arm->edbo->first; ebone_iter && ebone_iter != ebone_first_dupe;
         ebone_iter = ebone_iter->next) {
      if (EBONE_VISIBLE(arm, ebone_iter) && (ebone_iter->flag & BONE_SELECTED)) {
        EditBone *ebone;
        char new_bone_name_buff[MAXBONENAME];
        char *new_bone_name = ebone_iter->name;

        if (do_flip_names) {
          BLI_string_flip_side_name(
              new_bone_name_buff, ebone_iter->name, false, sizeof(new_bone_name_buff));

          /* Only use flipped name if not yet in use. Otherwise we'd get again inconsistent namings
           * (different numbers), better keep default behavior in this case. */
          if (ED_armature_ebone_find_name(arm->edbo, new_bone_name_buff) == NULL) {
            new_bone_name = new_bone_name_buff;
          }
        }

        ebone = duplicateEditBone(ebone_iter, new_bone_name, arm->edbo, ob);

        if (!ebone_first_dupe) {
          ebone_first_dupe = ebone;
        }
      }
    }

    /* Run though the list and fix the pointers */
    for (ebone_iter = arm->edbo->first; ebone_iter && ebone_iter != ebone_first_dupe;
         ebone_iter = ebone_iter->next) {
      if (EBONE_VISIBLE(arm, ebone_iter) && (ebone_iter->flag & BONE_SELECTED)) {
        EditBone *ebone = ebone_iter->temp.ebone;

        if (!ebone_iter->parent) {
          /* If this bone has no parent,
           * Set the duplicate->parent to NULL
           */
          ebone->parent = NULL;
        }
        else if (ebone_iter->parent->temp.ebone) {
          /* If this bone has a parent that was duplicated,
           * Set the duplicate->parent to the curBone->parent->temp
           */
          ebone->parent = ebone_iter->parent->temp.ebone;
        }
        else {
          /* If this bone has a parent that IS not selected,
           * Set the duplicate->parent to the curBone->parent
           */
          ebone->parent = (EditBone *)ebone_iter->parent;
          ebone->flag &= ~BONE_CONNECTED;
        }

        /* Update custom handle links. */
        if (ebone_iter->bbone_prev && ebone_iter->bbone_prev->temp.ebone) {
          ebone->bbone_prev = ebone_iter->bbone_prev->temp.ebone;
        }
        if (ebone_iter->bbone_next && ebone_iter->bbone_next->temp.ebone) {
          ebone->bbone_next = ebone_iter->bbone_next->temp.ebone;
        }

        /* Lets try to fix any constraint subtargets that might
         * have been duplicated
         */
        updateDuplicateSubtarget(ebone, arm->edbo, ob);
      }
    }

    /* correct the active bone */
    if (arm->act_edbone && arm->act_edbone->temp.ebone) {
      arm->act_edbone = arm->act_edbone->temp.ebone;
    }

    /* Deselect the old bones and select the new ones */
    for (ebone_iter = arm->edbo->first; ebone_iter && ebone_iter != ebone_first_dupe;
         ebone_iter = ebone_iter->next) {
      if (EBONE_VISIBLE(arm, ebone_iter)) {
        ebone_iter->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
      }
    }

    postEditBoneDuplicate(arm->edbo, ob);

    ED_armature_edit_validate_active(arm);

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Selected Bone(s)";
  ot->idname = "ARMATURE_OT_duplicate";
  ot->description = "Make copies of the selected bones within the same armature";

  /* api callbacks */
  ot->exec = armature_duplicate_selected_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna,
      "do_flip_names",
      false,
      "Flip Names",
      "Try to flip names of the bones, if possible, instead of adding a number extension");
}

/* Get the duplicated or existing mirrored copy of the bone. */
static EditBone *get_symmetrized_bone(bArmature *arm, EditBone *bone)
{
  if (bone == NULL) {
    return NULL;
  }
  else if (bone->temp.ebone != NULL) {
    return bone->temp.ebone;
  }
  else {
    EditBone *mirror = ED_armature_ebone_get_mirrored(arm->edbo, bone);
    return (mirror != NULL) ? mirror : bone;
  }
}

/**
 * near duplicate of #armature_duplicate_selected_exec,
 * except for parenting part (keep in sync)
 */
static int armature_symmetrize_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const int direction = RNA_enum_get(op->ptr, "direction");
  const int axis = 0;

  /* cancel if nothing selected */
  if (CTX_DATA_COUNT(C, selected_bones) == 0) {
    return OPERATOR_CANCELLED;
  }

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    bArmature *arm = obedit->data;

    EditBone *ebone_iter;
    /* The beginning of the duplicated mirrored bones in the edbo list */
    EditBone *ebone_first_dupe = NULL;

    ED_armature_edit_sync_selection(arm->edbo);  // XXX why is this needed?

    preEditBoneDuplicate(arm->edbo);

    /* Select mirrored bones */
    for (ebone_iter = arm->edbo->first; ebone_iter; ebone_iter = ebone_iter->next) {
      if (EBONE_VISIBLE(arm, ebone_iter) && (ebone_iter->flag & BONE_SELECTED)) {
        char name_flip[MAXBONENAME];

        BLI_string_flip_side_name(name_flip, ebone_iter->name, false, sizeof(name_flip));

        if (STREQ(name_flip, ebone_iter->name)) {
          /* if the name matches, we don't have the potential to be mirrored, just skip */
          ebone_iter->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
        }
        else {
          EditBone *ebone = ED_armature_ebone_find_name(arm->edbo, name_flip);

          if (ebone) {
            if ((ebone->flag & BONE_SELECTED) == 0) {
              /* simple case, we're selected, the other bone isn't! */
              ebone_iter->temp.ebone = ebone;
            }
            else {
              /* complicated - choose which direction to copy */
              float axis_delta;

              axis_delta = ebone->head[axis] - ebone_iter->head[axis];
              if (axis_delta == 0.0f) {
                axis_delta = ebone->tail[axis] - ebone_iter->tail[axis];
              }

              if (axis_delta == 0.0f) {
                /* both mirrored bones exist and point to eachother and overlap exactly.
                 *
                 * in this case there's no well defined solution, so de-select both and skip.
                 */
                ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
                ebone_iter->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
              }
              else {
                EditBone *ebone_src, *ebone_dst;
                if (((axis_delta < 0.0f) ? -1 : 1) == direction) {
                  ebone_src = ebone;
                  ebone_dst = ebone_iter;
                }
                else {
                  ebone_src = ebone_iter;
                  ebone_dst = ebone;
                }

                ebone_src->temp.ebone = ebone_dst;
                ebone_dst->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
              }
            }
          }
        }
      }
    }

    /*  Find the selected bones and duplicate them as needed, with mirrored name */
    for (ebone_iter = arm->edbo->first; ebone_iter && ebone_iter != ebone_first_dupe;
         ebone_iter = ebone_iter->next) {
      if (EBONE_VISIBLE(arm, ebone_iter) && (ebone_iter->flag & BONE_SELECTED) &&
          /* will be set if the mirror bone already exists (no need to make a new one) */
          (ebone_iter->temp.ebone == NULL)) {
        char name_flip[MAXBONENAME];

        BLI_string_flip_side_name(name_flip, ebone_iter->name, false, sizeof(name_flip));

        /* bones must have a side-suffix */
        if (!STREQ(name_flip, ebone_iter->name)) {
          EditBone *ebone;

          ebone = duplicateEditBone(ebone_iter, name_flip, arm->edbo, obedit);

          if (!ebone_first_dupe) {
            ebone_first_dupe = ebone;
          }
        }
      }
    }

    /*  Run through the list and fix the pointers */
    for (ebone_iter = arm->edbo->first; ebone_iter && ebone_iter != ebone_first_dupe;
         ebone_iter = ebone_iter->next) {
      if (ebone_iter->temp.ebone) {
        /* copy all flags except for ... */
        const int flag_copy = ((int)~0) & ~(BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL);

        EditBone *ebone = ebone_iter->temp.ebone;

        /* copy flags incase bone is pre-existing data */
        ebone->flag = (ebone->flag & ~flag_copy) | (ebone_iter->flag & flag_copy);

        if (ebone_iter->parent == NULL) {
          /* If this bone has no parent,
           * Set the duplicate->parent to NULL
           */
          ebone->parent = NULL;
          ebone->flag &= ~BONE_CONNECTED;
        }
        else {
          /* the parent may have been duplicated, if not lookup the mirror parent */
          EditBone *ebone_parent = get_symmetrized_bone(arm, ebone_iter->parent);

          if (ebone_parent == ebone_iter->parent) {
            /* If the mirror lookup failed, (but the current bone has a parent)
             * then we can assume the parent has no L/R but is a center bone.
             * So just use the same parent for both.
             */
            ebone->flag &= ~BONE_CONNECTED;
          }

          ebone->parent = ebone_parent;
        }

        /* Update custom handle links. */
        ebone->bbone_prev = get_symmetrized_bone(arm, ebone_iter->bbone_prev);
        ebone->bbone_next = get_symmetrized_bone(arm, ebone_iter->bbone_next);

        /* Lets try to fix any constraint subtargets that might
         * have been duplicated
         */
        updateDuplicateSubtarget(ebone, arm->edbo, obedit);
      }
    }

    ED_armature_edit_transform_mirror_update(obedit);

    /* Selected bones now have their 'temp' pointer set,
     * so we don't need this anymore */

    /* Deselect the old bones and select the new ones */
    for (ebone_iter = arm->edbo->first; ebone_iter && ebone_iter != ebone_first_dupe;
         ebone_iter = ebone_iter->next) {
      if (EBONE_VISIBLE(arm, ebone_iter)) {
        ebone_iter->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
      }
    }

    /* New bones will be selected, but some of the bones may already exist */
    for (ebone_iter = arm->edbo->first; ebone_iter && ebone_iter != ebone_first_dupe;
         ebone_iter = ebone_iter->next) {
      EditBone *ebone = ebone_iter->temp.ebone;
      if (ebone && EBONE_SELECTABLE(arm, ebone)) {
        ED_armature_ebone_select_set(ebone, true);
      }
    }

    /* correct the active bone */
    if (arm->act_edbone && arm->act_edbone->temp.ebone) {
      arm->act_edbone = arm->act_edbone->temp.ebone;
    }

    postEditBoneDuplicate(arm->edbo, obedit);

    ED_armature_edit_validate_active(arm);

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

/* following conventions from #MESH_OT_symmetrize */
void ARMATURE_OT_symmetrize(wmOperatorType *ot)
{
  /* subset of 'rna_enum_symmetrize_direction_items' */
  static const EnumPropertyItem arm_symmetrize_direction_items[] = {
      {-1, "NEGATIVE_X", 0, "-X to +X", ""},
      {+1, "POSITIVE_X", 0, "+X to -X", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Symmetrize";
  ot->idname = "ARMATURE_OT_symmetrize";
  ot->description = "Enforce symmetry, make copies of the selection or use existing";

  /* api callbacks */
  ot->exec = armature_symmetrize_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna,
                          "direction",
                          arm_symmetrize_direction_items,
                          -1,
                          "Direction",
                          "Which sides to copy from and to (when both are selected)");
}

/* ------------------------------------------ */

/* previously extrude_armature */
/* context; editmode armature */
/* if forked && mirror-edit: makes two bones with flipped names */
static int armature_extrude_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool forked = RNA_boolean_get(op->ptr, "forked");
  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    bArmature *arm = ob->data;
    bool forked_iter = forked;

    EditBone *newbone = NULL, *ebone, *flipbone, *first = NULL;
    int a, totbone = 0, do_extrude;

    /* since we allow root extrude too, we have to make sure selection is OK */
    for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
      if (EBONE_VISIBLE(arm, ebone)) {
        if (ebone->flag & BONE_ROOTSEL) {
          if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
            if (ebone->parent->flag & BONE_TIPSEL) {
              ebone->flag &= ~BONE_ROOTSEL;
            }
          }
        }
      }
    }

    /* Duplicate the necessary bones */
    for (ebone = arm->edbo->first; ((ebone) && (ebone != first)); ebone = ebone->next) {
      if (EBONE_VISIBLE(arm, ebone)) {
        /* we extrude per definition the tip */
        do_extrude = false;
        if (ebone->flag & (BONE_TIPSEL | BONE_SELECTED)) {
          do_extrude = true;
        }
        else if (ebone->flag & BONE_ROOTSEL) {
          /* but, a bone with parent deselected we do the root... */
          if (ebone->parent && (ebone->parent->flag & BONE_TIPSEL)) {
            /* pass */
          }
          else {
            do_extrude = 2;
          }
        }

        if (do_extrude) {
          /* we re-use code for mirror editing... */
          flipbone = NULL;
          if (arm->flag & ARM_MIRROR_EDIT) {
            flipbone = ED_armature_ebone_get_mirrored(arm->edbo, ebone);
            if (flipbone) {
              forked_iter = 0;  // we extrude 2 different bones
              if (flipbone->flag & (BONE_TIPSEL | BONE_ROOTSEL | BONE_SELECTED)) {
                /* don't want this bone to be selected... */
                flipbone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
              }
            }
            if ((flipbone == NULL) && (forked_iter)) {
              flipbone = ebone;
            }
          }

          for (a = 0; a < 2; a++) {
            if (a == 1) {
              if (flipbone == NULL) {
                break;
              }
              else {
                SWAP(EditBone *, flipbone, ebone);
              }
            }

            totbone++;
            newbone = MEM_callocN(sizeof(EditBone), "extrudebone");

            if (do_extrude == true) {
              copy_v3_v3(newbone->head, ebone->tail);
              copy_v3_v3(newbone->tail, newbone->head);
              newbone->parent = ebone;

              /* copies it, in case mirrored bone */
              newbone->flag = ebone->flag & (BONE_TIPSEL | BONE_RELATIVE_PARENTING);

              if (newbone->parent) {
                newbone->flag |= BONE_CONNECTED;
              }
            }
            else {
              copy_v3_v3(newbone->head, ebone->head);
              copy_v3_v3(newbone->tail, ebone->head);
              newbone->parent = ebone->parent;

              newbone->flag = BONE_TIPSEL;

              if (newbone->parent && (ebone->flag & BONE_CONNECTED)) {
                newbone->flag |= BONE_CONNECTED;
              }
            }

            newbone->weight = ebone->weight;
            newbone->dist = ebone->dist;
            newbone->xwidth = ebone->xwidth;
            newbone->zwidth = ebone->zwidth;
            newbone->rad_head = ebone->rad_tail;  // don't copy entire bone...
            newbone->rad_tail = ebone->rad_tail;
            newbone->segments = 1;
            newbone->layer = ebone->layer;

            /* Bendy-Bone parameters */
            newbone->roll1 = ebone->roll1;
            newbone->roll2 = ebone->roll2;
            newbone->curve_in_x = ebone->curve_in_x;
            newbone->curve_in_y = ebone->curve_in_y;
            newbone->curve_out_x = ebone->curve_out_x;
            newbone->curve_out_y = ebone->curve_out_y;
            newbone->ease1 = ebone->ease1;
            newbone->ease2 = ebone->ease2;
            newbone->scale_in_x = ebone->scale_in_x;
            newbone->scale_in_y = ebone->scale_in_y;
            newbone->scale_out_x = ebone->scale_out_x;
            newbone->scale_out_y = ebone->scale_out_y;

            BLI_strncpy(newbone->name, ebone->name, sizeof(newbone->name));

            if (flipbone && forked_iter) {  // only set if mirror edit
              if (strlen(newbone->name) < (MAXBONENAME - 2)) {
                if (a == 0) {
                  strcat(newbone->name, "_L");
                }
                else {
                  strcat(newbone->name, "_R");
                }
              }
            }
            ED_armature_ebone_unique_name(arm->edbo, newbone->name, NULL);

            /* Add the new bone to the list */
            BLI_addtail(arm->edbo, newbone);
            if (!first) {
              first = newbone;
            }

            /* restore ebone if we were flipping */
            if (a == 1 && flipbone) {
              SWAP(EditBone *, flipbone, ebone);
            }
          }
        }

        /* Deselect the old bone */
        ebone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
      }
    }
    /* if only one bone, make this one active */
    if (totbone == 1 && first) {
      arm->act_edbone = first;
    }
    else {
      arm->act_edbone = newbone;
    }

    if (totbone == 0) {
      continue;
    }

    changed_multi = true;

    /* Transform the endpoints */
    ED_armature_edit_sync_selection(arm->edbo);

    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
  }
  MEM_freeN(objects);

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ARMATURE_OT_extrude(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude";
  ot->idname = "ARMATURE_OT_extrude";
  ot->description = "Create new bones from the selected joints";

  /* api callbacks */
  ot->exec = armature_extrude_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(ot->srna, "forked", 0, "Forked", "");
}

/* ********************** Bone Add *************************************/

/*op makes a new bone and returns it with its tip selected */

static int armature_bone_primitive_add_exec(bContext *C, wmOperator *op)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Object *obedit = CTX_data_edit_object(C);
  EditBone *bone;
  float obmat[3][3], curs[3], viewmat[3][3], totmat[3][3], imat[3][3];
  char name[MAXBONENAME];

  RNA_string_get(op->ptr, "name", name);

  copy_v3_v3(curs, CTX_data_scene(C)->cursor.location);

  /* Get inverse point for head and orientation for tail */
  invert_m4_m4(obedit->imat, obedit->obmat);
  mul_m4_v3(obedit->imat, curs);

  if (rv3d && (U.flag & USER_ADD_VIEWALIGNED)) {
    copy_m3_m4(obmat, rv3d->viewmat);
  }
  else {
    unit_m3(obmat);
  }

  copy_m3_m4(viewmat, obedit->obmat);
  mul_m3_m3m3(totmat, obmat, viewmat);
  invert_m3_m3(imat, totmat);

  ED_armature_edit_deselect_all(obedit);

  /*  Create a bone */
  bone = ED_armature_ebone_add(obedit->data, name);

  copy_v3_v3(bone->head, curs);

  if (rv3d && (U.flag & USER_ADD_VIEWALIGNED)) {
    add_v3_v3v3(bone->tail, bone->head, imat[1]);  // bone with unit length 1
  }
  else {
    add_v3_v3v3(bone->tail, bone->head, imat[2]);  // bone with unit length 1, pointing up Z
  }

  ED_armature_edit_refresh_layer_used(obedit->data);

  /* note, notifier might evolve */
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_bone_primitive_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Bone";
  ot->idname = "ARMATURE_OT_bone_primitive_add";
  ot->description = "Add a new bone located at the 3D-Cursor";

  /* api callbacks */
  ot->exec = armature_bone_primitive_add_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_string(ot->srna, "name", "Bone", MAXBONENAME, "Name", "Name of the newly created bone");
}

/* ********************** Subdivide *******************************/

/* Subdivide Operators:
 * This group of operators all use the same 'exec' callback, but they are called
 * through several different operators - a combined menu (which just calls the exec in the
 * appropriate ways), and two separate ones.
 */

static int armature_subdivide_exec(bContext *C, wmOperator *op)
{
  Object *obedit = CTX_data_edit_object(C);
  EditBone *newbone, *tbone;
  int cuts, i;

  /* there may not be a number_cuts property defined (for 'simple' subdivide) */
  cuts = RNA_int_get(op->ptr, "number_cuts");

  /* loop over all editable bones */
  // XXX the old code did this in reverse order though!
  CTX_DATA_BEGIN_WITH_ID (C, EditBone *, ebone, selected_editable_bones, bArmature *, arm) {
    for (i = cuts + 1; i > 1; i--) {
      /* compute cut ratio first */
      float cutratio = 1.0f / (float)i;
      float cutratioI = 1.0f - cutratio;

      float val1[3];
      float val2[3];
      float val3[3];

      newbone = MEM_mallocN(sizeof(EditBone), "ebone subdiv");
      *newbone = *ebone;
      BLI_addtail(arm->edbo, newbone);

      /* calculate location of newbone->head */
      copy_v3_v3(val1, ebone->head);
      copy_v3_v3(val2, ebone->tail);
      copy_v3_v3(val3, newbone->head);

      val3[0] = val1[0] * cutratio + val2[0] * cutratioI;
      val3[1] = val1[1] * cutratio + val2[1] * cutratioI;
      val3[2] = val1[2] * cutratio + val2[2] * cutratioI;

      copy_v3_v3(newbone->head, val3);
      copy_v3_v3(newbone->tail, ebone->tail);
      copy_v3_v3(ebone->tail, newbone->head);

      newbone->rad_head = ((ebone->rad_head * cutratio) + (ebone->rad_tail * cutratioI));
      ebone->rad_tail = newbone->rad_head;

      newbone->flag |= BONE_CONNECTED;

      newbone->prop = NULL;

      ED_armature_ebone_unique_name(arm->edbo, newbone->name, NULL);

      /* correct parent bones */
      for (tbone = arm->edbo->first; tbone; tbone = tbone->next) {
        if (tbone->parent == ebone) {
          tbone->parent = newbone;
        }
      }
      newbone->parent = ebone;
    }
  }
  CTX_DATA_END;

  /* note, notifier might evolve */
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

  return OPERATOR_FINISHED;
}

void ARMATURE_OT_subdivide(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Subdivide Multi";
  ot->idname = "ARMATURE_OT_subdivide";
  ot->description = "Break selected bones into chains of smaller bones";

  /* api callbacks */
  ot->exec = armature_subdivide_exec;
  ot->poll = ED_operator_editarmature;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties */
  prop = RNA_def_int(ot->srna, "number_cuts", 1, 1, 1000, "Number of Cuts", "", 1, 10);
  /* Avoid re-using last var because it can cause
   * _very_ high poly meshes and annoy users (or worse crash) */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}
