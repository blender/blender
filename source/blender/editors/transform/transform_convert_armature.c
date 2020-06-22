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

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BIK_api.h"

#include "ED_armature.h"
#include "ED_keyframing.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"

#include "transform.h"
#include "transform_snap.h"

/* Own include. */
#include "transform_convert.h"

typedef struct BoneInitData {
  struct EditBone *bone;
  float tail[3];
  float rad_head;
  float rad_tail;
  float roll;
  float head[3];
  float dist;
  float xwidth;
  float zwidth;
} BoneInitData;

/* Return if we need to update motion paths, only if they already exist,
 * and we will insert a keyframe at the end of transform. */
static bool motionpath_need_update_pose(Scene *scene, Object *ob)
{
  if (autokeyframe_cfra_can_key(scene, &ob->id)) {
    return (ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) != 0;
  }

  return false;
}

/**
 * Auto-keyframing feature - for poses/pose-channels
 *
 * \param tmode: A transform mode.
 *
 * targetless_ik: has targetless ik been done on any channels?
 *
 * \note Context may not always be available,
 * so must check before using it as it's a luxury for a few cases.
 */
static void autokeyframe_pose(
    bContext *C, Scene *scene, Object *ob, int tmode, short targetless_ik)
{
  Main *bmain = CTX_data_main(C);
  ID *id = &ob->id;
  AnimData *adt = ob->adt;
  bAction *act = (adt) ? adt->action : NULL;
  bPose *pose = ob->pose;
  bPoseChannel *pchan;
  FCurve *fcu;

  // TODO: this should probably be done per channel instead...
  if (!autokeyframe_cfra_can_key(scene, id)) {
    /* tag channels that should have unkeyed data */
    for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
      if (pchan->bone->flag & BONE_TRANSFORM) {
        /* tag this channel */
        pchan->bone->flag |= BONE_UNKEYED;
      }
    }
    return;
  }

  ReportList *reports = CTX_wm_reports(C);
  ToolSettings *ts = scene->toolsettings;
  KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
  ListBase nla_cache = {NULL, NULL};
  float cfra = (float)CFRA;
  eInsertKeyFlags flag = 0;

  /* flag is initialized from UserPref keyframing settings
   * - special exception for targetless IK - INSERTKEY_MATRIX keyframes should get
   *   visual keyframes even if flag not set, as it's not that useful otherwise
   *   (for quick animation recording)
   */
  flag = ANIM_get_keyframing_flags(scene, true);

  if (targetless_ik) {
    flag |= INSERTKEY_MATRIX;
  }

  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    if ((pchan->bone->flag & BONE_TRANSFORM) == 0 &&
        !((pose->flag & POSE_MIRROR_EDIT) && (pchan->bone->flag & BONE_TRANSFORM_MIRROR))) {
      continue;
    }

    ListBase dsources = {NULL, NULL};

    /* clear any 'unkeyed' flag it may have */
    pchan->bone->flag &= ~BONE_UNKEYED;

    /* add datasource override for the camera object */
    ANIM_relative_keyingset_add_source(&dsources, id, &RNA_PoseBone, pchan);

    /* only insert into active keyingset? */
    if (IS_AUTOKEY_FLAG(scene, ONLYKEYINGSET) && (active_ks)) {
      /* run the active Keying Set on the current datasource */
      ANIM_apply_keyingset(C, &dsources, NULL, active_ks, MODIFYKEY_MODE_INSERT, cfra);
    }
    /* only insert into available channels? */
    else if (IS_AUTOKEY_FLAG(scene, INSERTAVAIL)) {
      if (act) {
        for (fcu = act->curves.first; fcu; fcu = fcu->next) {
          /* only insert keyframes for this F-Curve if it affects the current bone */
          if (strstr(fcu->rna_path, "bones") == NULL) {
            continue;
          }
          char *pchanName = BLI_str_quoted_substrN(fcu->rna_path, "bones[");

          /* only if bone name matches too...
           * NOTE: this will do constraints too, but those are ok to do here too?
           */
          if (pchanName && STREQ(pchanName, pchan->name)) {
            insert_keyframe(bmain,
                            reports,
                            id,
                            act,
                            ((fcu->grp) ? (fcu->grp->name) : (NULL)),
                            fcu->rna_path,
                            fcu->array_index,
                            cfra,
                            ts->keyframe_type,
                            &nla_cache,
                            flag);
          }

          if (pchanName) {
            MEM_freeN(pchanName);
          }
        }
      }
    }
    /* only insert keyframe if needed? */
    else if (IS_AUTOKEY_FLAG(scene, INSERTNEEDED)) {
      bool do_loc = false, do_rot = false, do_scale = false;

      /* Filter the conditions when this happens
       * (assume that 'curarea->spacetype == SPACE_VIEW3D'). */
      if (tmode == TFM_TRANSLATION) {
        if (targetless_ik) {
          do_rot = true;
        }
        else {
          do_loc = true;
        }
      }
      else if (ELEM(tmode, TFM_ROTATION, TFM_TRACKBALL)) {
        if (ELEM(scene->toolsettings->transform_pivot_point,
                 V3D_AROUND_CURSOR,
                 V3D_AROUND_ACTIVE)) {
          do_loc = true;
        }

        if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
          do_rot = true;
        }
      }
      else if (tmode == TFM_RESIZE) {
        if (ELEM(scene->toolsettings->transform_pivot_point,
                 V3D_AROUND_CURSOR,
                 V3D_AROUND_ACTIVE)) {
          do_loc = true;
        }

        if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
          do_scale = true;
        }
      }

      if (do_loc) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOCATION_ID);
        ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
      }
      if (do_rot) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_ROTATION_ID);
        ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
      }
      if (do_scale) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_SCALING_ID);
        ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
      }
    }
    /* insert keyframe in all (transform) channels */
    else {
      KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOC_ROT_SCALE_ID);
      ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
    }

    /* free temp info */
    BLI_freelistN(&dsources);
  }

  BKE_animsys_free_nla_keyframing_context_cache(&nla_cache);
}

static bConstraint *add_temporary_ik_constraint(bPoseChannel *pchan,
                                                bKinematicConstraint *targetless_con)
{
  bConstraint *con = BKE_constraint_add_for_pose(
      NULL, pchan, "TempConstraint", CONSTRAINT_TYPE_KINEMATIC);

  /* for draw, but also for detecting while pose solving */
  pchan->constflag |= (PCHAN_HAS_IK | PCHAN_HAS_TARGET);

  bKinematicConstraint *temp_con_data = con->data;

  if (targetless_con) {
    /* if exists, use values from last targetless (but disabled) IK-constraint as base */
    *temp_con_data = *targetless_con;
  }
  else {
    temp_con_data->flag = CONSTRAINT_IK_TIP;
  }

  temp_con_data->flag |= CONSTRAINT_IK_TEMP | CONSTRAINT_IK_AUTO | CONSTRAINT_IK_POS;

  return con;
}

static void update_deg_with_temporary_ik(Main *bmain, Object *ob)
{
  BIK_clear_data(ob->pose);
  /* TODO(sergey): Consider doing partial update only. */
  DEG_relations_tag_update(bmain);
}

/* -------------------------------------------------------------------- */
/** \name Pose Auto-IK
 * \{ */

static bKinematicConstraint *has_targetless_ik(bPoseChannel *pchan)
{
  bConstraint *con = pchan->constraints.first;

  for (; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC && (con->flag & CONSTRAINT_OFF) == 0 &&
        (con->enforce != 0.0f)) {
      bKinematicConstraint *data = con->data;

      if (data->tar == NULL) {
        return data;
      }
      if (data->tar->type == OB_ARMATURE && data->subtarget[0] == 0) {
        return data;
      }
    }
  }
  return NULL;
}

/* adds the IK to pchan - returns if added */
static short pose_grab_with_ik_add(bPoseChannel *pchan)
{
  bKinematicConstraint *targetless = NULL;
  bKinematicConstraint *data;
  bConstraint *con;

  /* Sanity check */
  if (pchan == NULL) {
    return 0;
  }

  /* Rule: not if there's already an IK on this channel */
  for (con = pchan->constraints.first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC && (con->flag & CONSTRAINT_OFF) == 0) {
      data = con->data;

      if (data->tar == NULL || (data->tar->type == OB_ARMATURE && data->subtarget[0] == '\0')) {
        /* make reference to constraint to base things off later
         * (if it's the last targetless constraint encountered) */
        targetless = (bKinematicConstraint *)con->data;

        /* but, if this is a targetless IK, we make it auto anyway (for the children loop) */
        if (con->enforce != 0.0f) {
          data->flag |= CONSTRAINT_IK_AUTO;

          /* if no chain length has been specified,
           * just make things obey standard rotation locks too */
          if (data->rootbone == 0) {
            for (bPoseChannel *pchan_iter = pchan; pchan_iter; pchan_iter = pchan_iter->parent) {
              /* here, we set ik-settings for bone from pchan->protectflag */
              // XXX: careful with quats/axis-angle rotations where we're locking 4d components
              if (pchan_iter->protectflag & OB_LOCK_ROTX) {
                pchan_iter->ikflag |= BONE_IK_NO_XDOF_TEMP;
              }
              if (pchan_iter->protectflag & OB_LOCK_ROTY) {
                pchan_iter->ikflag |= BONE_IK_NO_YDOF_TEMP;
              }
              if (pchan_iter->protectflag & OB_LOCK_ROTZ) {
                pchan_iter->ikflag |= BONE_IK_NO_ZDOF_TEMP;
              }
            }
          }
        }
      }

      if ((con->flag & CONSTRAINT_DISABLE) == 0 && (con->enforce != 0.0f)) {
        return 0;
      }
    }
  }

  data = add_temporary_ik_constraint(pchan, targetless)->data;

  copy_v3_v3(data->grabtarget, pchan->pose_tail);

  /* watch-it! has to be 0 here, since we're still on the
   * same bone for the first time through the loop T25885. */
  data->rootbone = 0;

  /* we only include bones that are part of a continual connected chain */
  do {
    /* here, we set ik-settings for bone from pchan->protectflag */
    // XXX: careful with quats/axis-angle rotations where we're locking 4d components
    if (pchan->protectflag & OB_LOCK_ROTX) {
      pchan->ikflag |= BONE_IK_NO_XDOF_TEMP;
    }
    if (pchan->protectflag & OB_LOCK_ROTY) {
      pchan->ikflag |= BONE_IK_NO_YDOF_TEMP;
    }
    if (pchan->protectflag & OB_LOCK_ROTZ) {
      pchan->ikflag |= BONE_IK_NO_ZDOF_TEMP;
    }

    /* now we count this pchan as being included */
    data->rootbone++;

    /* continue to parent, but only if we're connected to it */
    if (pchan->bone->flag & BONE_CONNECTED) {
      pchan = pchan->parent;
    }
    else {
      pchan = NULL;
    }
  } while (pchan);

  /* make a copy of maximum chain-length */
  data->max_rootbone = data->rootbone;

  return 1;
}

/* bone is a candidate to get IK, but we don't do it if it has children connected */
static short pose_grab_with_ik_children(bPose *pose, Bone *bone)
{
  Bone *bonec;
  short wentdeeper = 0, added = 0;

  /* go deeper if children & children are connected */
  for (bonec = bone->childbase.first; bonec; bonec = bonec->next) {
    if (bonec->flag & BONE_CONNECTED) {
      wentdeeper = 1;
      added += pose_grab_with_ik_children(pose, bonec);
    }
  }
  if (wentdeeper == 0) {
    bPoseChannel *pchan = BKE_pose_channel_find_name(pose, bone->name);
    if (pchan) {
      added += pose_grab_with_ik_add(pchan);
    }
  }

  return added;
}

/* main call which adds temporal IK chains */
static short pose_grab_with_ik(Main *bmain, Object *ob)
{
  bArmature *arm;
  bPoseChannel *pchan, *parent;
  Bone *bonec;
  short tot_ik = 0;

  if ((ob == NULL) || (ob->pose == NULL) || (ob->mode & OB_MODE_POSE) == 0) {
    return 0;
  }

  arm = ob->data;

  /* Rule: allow multiple Bones
   * (but they must be selected, and only one ik-solver per chain should get added) */
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    if (pchan->bone->layer & arm->layer) {
      if (pchan->bone->flag & (BONE_SELECTED | BONE_TRANSFORM_MIRROR)) {
        /* Rule: no IK for solitatry (unconnected) bones */
        for (bonec = pchan->bone->childbase.first; bonec; bonec = bonec->next) {
          if (bonec->flag & BONE_CONNECTED) {
            break;
          }
        }
        if ((pchan->bone->flag & BONE_CONNECTED) == 0 && (bonec == NULL)) {
          continue;
        }

        /* rule: if selected Bone is not a root bone, it gets a temporal IK */
        if (pchan->parent) {
          /* only adds if there's no IK yet (and no parent bone was selected) */
          for (parent = pchan->parent; parent; parent = parent->parent) {
            if (parent->bone->flag & (BONE_SELECTED | BONE_TRANSFORM_MIRROR)) {
              break;
            }
          }
          if (parent == NULL) {
            tot_ik += pose_grab_with_ik_add(pchan);
          }
        }
        else {
          /* rule: go over the children and add IK to the tips */
          tot_ik += pose_grab_with_ik_children(ob->pose, pchan->bone);
        }
      }
    }
  }

  /* iTaSC needs clear for new IK constraints */
  if (tot_ik) {
    update_deg_with_temporary_ik(bmain, ob);
  }

  return (tot_ik) ? 1 : 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Mirror
 * \{ */

typedef struct PoseInitData_Mirror {
  /** Points to the bone which this info is initialized & restored to.
   * A NULL value is used to terminate the array. */
  struct bPoseChannel *pchan;
  struct {
    float loc[3];
    float size[3];
    union {
      float eul[3];
      float quat[4];
      float axis_angle[4];
    };
    float curve_in_x;
    float curve_out_x;
    float roll1;
    float roll2;
  } orig;
  /**
   * An extra offset to apply after mirroring.
   * Use with #POSE_MIRROR_RELATIVE.
   */
  float offset_mtx[4][4];
} PoseInitData_Mirror;

static void pose_mirror_info_init(PoseInitData_Mirror *pid,
                                  bPoseChannel *pchan,
                                  bPoseChannel *pchan_orig,
                                  bool is_mirror_relative)
{
  pid->pchan = pchan;
  copy_v3_v3(pid->orig.loc, pchan->loc);
  copy_v3_v3(pid->orig.size, pchan->size);
  pid->orig.curve_in_x = pchan->curve_in_x;
  pid->orig.curve_out_x = pchan->curve_out_x;
  pid->orig.roll1 = pchan->roll1;
  pid->orig.roll2 = pchan->roll2;

  if (pchan->rotmode > 0) {
    copy_v3_v3(pid->orig.eul, pchan->eul);
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    copy_v3_v3(pid->orig.axis_angle, pchan->rotAxis);
    pid->orig.axis_angle[3] = pchan->rotAngle;
  }
  else {
    copy_qt_qt(pid->orig.quat, pchan->quat);
  }

  if (is_mirror_relative) {
    float pchan_mtx[4][4];
    float pchan_mtx_mirror[4][4];

    float flip_mtx[4][4];
    unit_m4(flip_mtx);
    flip_mtx[0][0] = -1;

    BKE_pchan_to_mat4(pchan_orig, pchan_mtx_mirror);
    BKE_pchan_to_mat4(pchan, pchan_mtx);

    mul_m4_m4m4(pchan_mtx_mirror, pchan_mtx_mirror, flip_mtx);
    mul_m4_m4m4(pchan_mtx_mirror, flip_mtx, pchan_mtx_mirror);

    invert_m4(pchan_mtx_mirror);
    mul_m4_m4m4(pid->offset_mtx, pchan_mtx, pchan_mtx_mirror);
  }
  else {
    unit_m4(pid->offset_mtx);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convert Armature
 * \{ */

static void add_pose_transdata(
    TransInfo *t, bPoseChannel *pchan, Object *ob, TransDataContainer *tc, TransData *td)
{
  Bone *bone = pchan->bone;
  float pmat[3][3], omat[3][3];
  float cmat[3][3], tmat[3][3];
  float vec[3];

  copy_v3_v3(vec, pchan->pose_mat[3]);
  copy_v3_v3(td->center, vec);

  td->ob = ob;
  td->flag = TD_SELECTED;
  if (bone->flag & BONE_HINGE_CHILD_TRANSFORM) {
    td->flag |= TD_NOCENTER;
  }

  if (bone->flag & BONE_TRANSFORM_CHILD) {
    td->flag |= TD_NOCENTER;
    td->flag |= TD_NO_LOC;
  }

  td->extra = pchan;
  td->protectflag = pchan->protectflag;

  td->loc = pchan->loc;
  copy_v3_v3(td->iloc, pchan->loc);

  td->ext->size = pchan->size;
  copy_v3_v3(td->ext->isize, pchan->size);

  if (pchan->rotmode > 0) {
    td->ext->rot = pchan->eul;
    td->ext->rotAxis = NULL;
    td->ext->rotAngle = NULL;
    td->ext->quat = NULL;

    copy_v3_v3(td->ext->irot, pchan->eul);
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    td->ext->rot = NULL;
    td->ext->rotAxis = pchan->rotAxis;
    td->ext->rotAngle = &pchan->rotAngle;
    td->ext->quat = NULL;

    td->ext->irotAngle = pchan->rotAngle;
    copy_v3_v3(td->ext->irotAxis, pchan->rotAxis);
  }
  else {
    td->ext->rot = NULL;
    td->ext->rotAxis = NULL;
    td->ext->rotAngle = NULL;
    td->ext->quat = pchan->quat;

    copy_qt_qt(td->ext->iquat, pchan->quat);
  }
  td->ext->rotOrder = pchan->rotmode;

  /* proper way to get parent transform + own transform + constraints transform */
  copy_m3_m4(omat, ob->obmat);

  /* New code, using "generic" BKE_bone_parent_transform_calc_from_pchan(). */
  {
    BoneParentTransform bpt;
    float rpmat[3][3];

    BKE_bone_parent_transform_calc_from_pchan(pchan, &bpt);
    if (t->mode == TFM_TRANSLATION) {
      copy_m3_m4(pmat, bpt.loc_mat);
    }
    else {
      copy_m3_m4(pmat, bpt.rotscale_mat);
    }

    /* Grrr! Exceptional case: When translating pose bones that are either Hinge or NoLocal,
     * and want align snapping, we just need both loc_mat and rotscale_mat.
     * So simply always store rotscale mat in td->ext, and always use it to apply rotations...
     * Ugly to need such hacks! :/ */
    copy_m3_m4(rpmat, bpt.rotscale_mat);

    if (constraints_list_needinv(t, &pchan->constraints)) {
      copy_m3_m4(tmat, pchan->constinv);
      invert_m3_m3(cmat, tmat);
      mul_m3_series(td->mtx, cmat, omat, pmat);
      mul_m3_series(td->ext->r_mtx, cmat, omat, rpmat);
    }
    else {
      mul_m3_series(td->mtx, omat, pmat);
      mul_m3_series(td->ext->r_mtx, omat, rpmat);
    }
    invert_m3_m3(td->ext->r_smtx, td->ext->r_mtx);
  }

  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  /* exceptional case: rotate the pose bone which also applies transformation
   * when a parentless bone has BONE_NO_LOCAL_LOCATION [] */
  if (!ELEM(t->mode, TFM_TRANSLATION, TFM_RESIZE) &&
      (pchan->bone->flag & BONE_NO_LOCAL_LOCATION)) {
    if (pchan->parent) {
      /* same as td->smtx but without pchan->bone->bone_mat */
      td->flag |= TD_PBONE_LOCAL_MTX_C;
      mul_m3_m3m3(td->ext->l_smtx, pchan->bone->bone_mat, td->smtx);
    }
    else {
      td->flag |= TD_PBONE_LOCAL_MTX_P;
    }
  }

  /* for axismat we use bone's own transform */
  copy_m3_m4(pmat, pchan->pose_mat);
  mul_m3_m3m3(td->axismtx, omat, pmat);
  normalize_m3(td->axismtx);

  if (t->mode == TFM_BONE_ENVELOPE_DIST) {
    td->loc = NULL;
    td->val = &bone->dist;
    td->ival = bone->dist;
  }
  else if (t->mode == TFM_BONESIZE) {
    // abusive storage of scale in the loc pointer :)
    td->loc = &bone->xwidth;
    copy_v3_v3(td->iloc, td->loc);
    td->val = NULL;
  }

  /* in this case we can do target-less IK grabbing */
  if (t->mode == TFM_TRANSLATION) {
    bKinematicConstraint *data = has_targetless_ik(pchan);
    if (data) {
      if (data->flag & CONSTRAINT_IK_TIP) {
        copy_v3_v3(data->grabtarget, pchan->pose_tail);
      }
      else {
        copy_v3_v3(data->grabtarget, pchan->pose_head);
      }
      td->loc = data->grabtarget;
      copy_v3_v3(td->iloc, td->loc);

      data->flag |= CONSTRAINT_IK_AUTO;

      /* Add a temporary auto IK constraint here, as we will only temporarily active this
       * targetless bone during transform. (Targetless IK constraints are treated as if they are
       * disabled unless they are transformed). */
      add_temporary_ik_constraint(pchan, data);
      Main *bmain = CTX_data_main(t->context);
      update_deg_with_temporary_ik(bmain, ob);

      /* only object matrix correction */
      copy_m3_m3(td->mtx, omat);
      pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);
    }
  }

  /* store reference to first constraint */
  td->con = pchan->constraints.first;
}

/**
 * When objects array is NULL, use 't->data_container' as is.
 */
void createTransPose(TransInfo *t)
{
  Main *bmain = CTX_data_main(t->context);

  t->data_len_all = 0;

  bool has_translate_rotate_buf[2] = {false, false};
  bool *has_translate_rotate = (t->mode == TFM_TRANSLATION) ? has_translate_rotate_buf : NULL;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Object *ob = tc->poseobj;
    bPose *pose = ob->pose;

    bArmature *arm;

    /* check validity of state */
    arm = BKE_armature_from_object(tc->poseobj);
    if ((arm == NULL) || (pose == NULL)) {
      continue;
    }

    const bool mirror = ((pose->flag & POSE_MIRROR_EDIT) != 0);

    /* set flags and count total */
    tc->data_len = transform_convert_pose_transflags_update(
        ob, t->mode, t->around, has_translate_rotate);

    if (tc->data_len == 0) {
      continue;
    }

    if (arm->flag & ARM_RESTPOS) {
      if (ELEM(t->mode, TFM_DUMMY, TFM_BONESIZE) == 0) {
        BKE_report(t->reports, RPT_ERROR, "Cannot change Pose when 'Rest Position' is enabled");
        tc->data_len = 0;
        continue;
      }
    }

    if (mirror) {
      int total_mirrored = 0;
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        /* Clear the MIRROR flag from previous runs. */
        pchan->bone->flag &= ~BONE_TRANSFORM_MIRROR;

        if ((pchan->bone->flag & BONE_TRANSFORM) &&
            BKE_pose_channel_get_mirrored(ob->pose, pchan->name)) {
          total_mirrored++;
        }
      }

      PoseInitData_Mirror *pid = MEM_mallocN((total_mirrored + 1) * sizeof(PoseInitData_Mirror),
                                             "PoseInitData_Mirror");

      /* Trick to terminate iteration. */
      pid[total_mirrored].pchan = NULL;

      tc->custom.type.data = pid;
      tc->custom.type.use_free = true;
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len == 0) {
      continue;
    }
    Object *ob = tc->poseobj;
    TransData *td;
    TransDataExtension *tdx;
    int i;

    PoseInitData_Mirror *pid = tc->custom.type.data;
    int pid_index = 0;
    bPose *pose = ob->pose;

    if (pose == NULL) {
      continue;
    }

    const bool mirror = ((pose->flag & POSE_MIRROR_EDIT) != 0);
    const bool is_mirror_relative = ((pose->flag & POSE_MIRROR_RELATIVE) != 0);

    tc->poseobj = ob; /* we also allow non-active objects to be transformed, in weightpaint */

    /* init trans data */
    td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransPoseBone");
    tdx = tc->data_ext = MEM_callocN(tc->data_len * sizeof(TransDataExtension),
                                     "TransPoseBoneExt");
    for (i = 0; i < tc->data_len; i++, td++, tdx++) {
      td->ext = tdx;
      td->val = NULL;
    }

    if (mirror) {
      LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
        if (pchan->bone->flag & BONE_TRANSFORM) {
          bPoseChannel *pchan_mirror = BKE_pose_channel_get_mirrored(ob->pose, pchan->name);
          if (pchan_mirror) {
            pchan_mirror->bone->flag |= BONE_TRANSFORM_MIRROR;
            pose_mirror_info_init(&pid[pid_index], pchan_mirror, pchan, is_mirror_relative);
            pid_index++;
          }
        }
      }
    }

    /* do we need to add temporal IK chains? */
    if ((pose->flag & POSE_AUTO_IK) && t->mode == TFM_TRANSLATION) {
      if (pose_grab_with_ik(bmain, ob)) {
        t->flag |= T_AUTOIK;
        has_translate_rotate[0] = true;
      }
    }

    /* use pose channels to fill trans data */
    td = tc->data;
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      if (pchan->bone->flag & BONE_TRANSFORM) {
        add_pose_transdata(t, pchan, ob, tc, td);
        td++;
      }
    }

    if (td != (tc->data + tc->data_len)) {
      BKE_report(t->reports, RPT_DEBUG, "Bone selection count error");
    }
  }

  /* initialize initial auto=ik chainlen's? */
  if (t->flag & T_AUTOIK) {
    transform_autoik_update(t, 0);
  }

  /* if there are no translatable bones, do rotation */
  if ((t->mode == TFM_TRANSLATION) && !has_translate_rotate[0]) {
    if (has_translate_rotate[1]) {
      t->mode = TFM_ROTATION;
    }
    else {
      t->mode = TFM_RESIZE;
    }
  }

  t->flag |= T_POSE;
  /* disable PET, its not usable in pose mode yet [#32444] */
  t->flag &= ~T_PROP_EDIT_ALL;
}

void createTransArmatureVerts(TransInfo *t)
{
  t->data_len_all = 0;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    EditBone *ebo, *eboflip;
    bArmature *arm = tc->obedit->data;
    ListBase *edbo = arm->edbo;
    bool mirror = ((arm->flag & ARM_MIRROR_EDIT) != 0);
    int total_mirrored = 0;

    tc->data_len = 0;
    for (ebo = edbo->first; ebo; ebo = ebo->next) {
      const int data_len_prev = tc->data_len;

      if (EBONE_VISIBLE(arm, ebo) && !(ebo->flag & BONE_EDITMODE_LOCKED)) {
        if (ELEM(t->mode, TFM_BONESIZE, TFM_BONE_ENVELOPE_DIST)) {
          if (ebo->flag & BONE_SELECTED) {
            tc->data_len++;
          }
        }
        else if (t->mode == TFM_BONE_ROLL) {
          if (ebo->flag & BONE_SELECTED) {
            tc->data_len++;
          }
        }
        else {
          if (ebo->flag & BONE_TIPSEL) {
            tc->data_len++;
          }
          if (ebo->flag & BONE_ROOTSEL) {
            tc->data_len++;
          }
        }
      }

      if (mirror && (data_len_prev < tc->data_len)) {
        eboflip = ED_armature_ebone_get_mirrored(arm->edbo, ebo);
        if (eboflip) {
          total_mirrored++;
        }
      }
    }
    if (!tc->data_len) {
      continue;
    }

    if (mirror) {
      BoneInitData *bid = MEM_mallocN((total_mirrored + 1) * sizeof(BoneInitData), "BoneInitData");

      /* trick to terminate iteration */
      bid[total_mirrored].bone = NULL;

      tc->custom.type.data = bid;
      tc->custom.type.use_free = true;
    }
    t->data_len_all += tc->data_len;
  }

  transform_around_single_fallback(t);
  t->data_len_all = -1;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (!tc->data_len) {
      continue;
    }

    EditBone *ebo, *eboflip;
    bArmature *arm = tc->obedit->data;
    ListBase *edbo = arm->edbo;
    TransData *td, *td_old;
    float mtx[3][3], smtx[3][3], bonemat[3][3];
    bool mirror = ((arm->flag & ARM_MIRROR_EDIT) != 0);
    BoneInitData *bid = tc->custom.type.data;

    copy_m3_m4(mtx, tc->obedit->obmat);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransEditBone");
    int i = 0;

    for (ebo = edbo->first; ebo; ebo = ebo->next) {
      td_old = td;
      ebo->oldlength =
          ebo->length;  // length==0.0 on extrude, used for scaling radius of bone points

      if (EBONE_VISIBLE(arm, ebo) && !(ebo->flag & BONE_EDITMODE_LOCKED)) {
        if (t->mode == TFM_BONE_ENVELOPE) {
          if (ebo->flag & BONE_ROOTSEL) {
            td->val = &ebo->rad_head;
            td->ival = *td->val;

            copy_v3_v3(td->center, ebo->head);
            td->flag = TD_SELECTED;

            copy_m3_m3(td->smtx, smtx);
            copy_m3_m3(td->mtx, mtx);

            td->loc = NULL;
            td->ext = NULL;
            td->ob = tc->obedit;

            td++;
          }
          if (ebo->flag & BONE_TIPSEL) {
            td->val = &ebo->rad_tail;
            td->ival = *td->val;
            copy_v3_v3(td->center, ebo->tail);
            td->flag = TD_SELECTED;

            copy_m3_m3(td->smtx, smtx);
            copy_m3_m3(td->mtx, mtx);

            td->loc = NULL;
            td->ext = NULL;
            td->ob = tc->obedit;

            td++;
          }
        }
        else if (ELEM(t->mode, TFM_BONESIZE, TFM_BONE_ENVELOPE_DIST)) {
          if (ebo->flag & BONE_SELECTED) {
            if (t->mode == TFM_BONE_ENVELOPE_DIST) {
              td->loc = NULL;
              td->val = &ebo->dist;
              td->ival = ebo->dist;
            }
            else {
              // abusive storage of scale in the loc pointer :)
              td->loc = &ebo->xwidth;
              copy_v3_v3(td->iloc, td->loc);
              td->val = NULL;
            }
            copy_v3_v3(td->center, ebo->head);
            td->flag = TD_SELECTED;

            /* use local bone matrix */
            ED_armature_ebone_to_mat3(ebo, bonemat);
            mul_m3_m3m3(td->mtx, mtx, bonemat);
            invert_m3_m3(td->smtx, td->mtx);

            copy_m3_m3(td->axismtx, td->mtx);
            normalize_m3(td->axismtx);

            td->ext = NULL;
            td->ob = tc->obedit;

            td++;
          }
        }
        else if (t->mode == TFM_BONE_ROLL) {
          if (ebo->flag & BONE_SELECTED) {
            td->loc = NULL;
            td->val = &(ebo->roll);
            td->ival = ebo->roll;

            copy_v3_v3(td->center, ebo->head);
            td->flag = TD_SELECTED;

            td->ext = NULL;
            td->ob = tc->obedit;

            td++;
          }
        }
        else {
          if (ebo->flag & BONE_TIPSEL) {
            copy_v3_v3(td->iloc, ebo->tail);

            /* Don't allow single selected tips to have a modified center,
             * causes problem with snapping (see T45974).
             * However, in rotation mode, we want to keep that 'rotate bone around root with
             * only its tip selected' behavior (see T46325). */
            if ((t->around == V3D_AROUND_LOCAL_ORIGINS) &&
                ((t->mode == TFM_ROTATION) || (ebo->flag & BONE_ROOTSEL))) {
              copy_v3_v3(td->center, ebo->head);
            }
            else {
              copy_v3_v3(td->center, td->iloc);
            }

            td->loc = ebo->tail;
            td->flag = TD_SELECTED;
            if (ebo->flag & BONE_EDITMODE_LOCKED) {
              td->protectflag = OB_LOCK_LOC | OB_LOCK_ROT | OB_LOCK_SCALE;
            }

            copy_m3_m3(td->smtx, smtx);
            copy_m3_m3(td->mtx, mtx);

            ED_armature_ebone_to_mat3(ebo, td->axismtx);

            if ((ebo->flag & BONE_ROOTSEL) == 0) {
              td->extra = ebo;
              td->ival = ebo->roll;
            }

            td->ext = NULL;
            td->val = NULL;
            td->ob = tc->obedit;

            td++;
          }
          if (ebo->flag & BONE_ROOTSEL) {
            copy_v3_v3(td->iloc, ebo->head);
            copy_v3_v3(td->center, td->iloc);
            td->loc = ebo->head;
            td->flag = TD_SELECTED;
            if (ebo->flag & BONE_EDITMODE_LOCKED) {
              td->protectflag = OB_LOCK_LOC | OB_LOCK_ROT | OB_LOCK_SCALE;
            }

            copy_m3_m3(td->smtx, smtx);
            copy_m3_m3(td->mtx, mtx);

            ED_armature_ebone_to_mat3(ebo, td->axismtx);

            td->extra = ebo; /* to fix roll */
            td->ival = ebo->roll;

            td->ext = NULL;
            td->val = NULL;
            td->ob = tc->obedit;

            td++;
          }
        }
      }

      if (mirror && (td_old != td)) {
        eboflip = ED_armature_ebone_get_mirrored(arm->edbo, ebo);
        if (eboflip) {
          bid[i].bone = eboflip;
          bid[i].dist = eboflip->dist;
          bid[i].rad_head = eboflip->rad_head;
          bid[i].rad_tail = eboflip->rad_tail;
          bid[i].roll = eboflip->roll;
          bid[i].xwidth = eboflip->xwidth;
          bid[i].zwidth = eboflip->zwidth;
          copy_v3_v3(bid[i].head, eboflip->head);
          copy_v3_v3(bid[i].tail, eboflip->tail);
          i++;
        }
      }
    }

    if (mirror) {
      /* trick to terminate iteration */
      BLI_assert(i + 1 == (MEM_allocN_len(bid) / sizeof(*bid)));
      bid[i].bone = NULL;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Data Edit Armature
 * \{ */

static void restoreBones(TransDataContainer *tc)
{
  bArmature *arm;
  BoneInitData *bid = tc->custom.type.data;
  EditBone *ebo;

  if (tc->obedit) {
    arm = tc->obedit->data;
  }
  else {
    BLI_assert(tc->poseobj != NULL);
    arm = tc->poseobj->data;
  }

  while (bid->bone) {
    ebo = bid->bone;

    ebo->dist = bid->dist;
    ebo->rad_head = bid->rad_head;
    ebo->rad_tail = bid->rad_tail;
    ebo->roll = bid->roll;
    ebo->xwidth = bid->xwidth;
    ebo->zwidth = bid->zwidth;
    copy_v3_v3(ebo->head, bid->head);
    copy_v3_v3(ebo->tail, bid->tail);

    if (arm->flag & ARM_MIRROR_EDIT) {
      EditBone *ebo_child;

      /* Also move connected ebo_child, in case ebo_child's name aren't mirrored properly */
      for (ebo_child = arm->edbo->first; ebo_child; ebo_child = ebo_child->next) {
        if ((ebo_child->flag & BONE_CONNECTED) && (ebo_child->parent == ebo)) {
          copy_v3_v3(ebo_child->head, ebo->tail);
          ebo_child->rad_head = ebo->rad_tail;
        }
      }

      /* Also move connected parent, in case parent's name isn't mirrored properly */
      if ((ebo->flag & BONE_CONNECTED) && ebo->parent) {
        EditBone *parent = ebo->parent;
        copy_v3_v3(parent->tail, ebo->head);
        parent->rad_tail = ebo->rad_head;
      }
    }

    bid++;
  }
}

void recalcData_edit_armature(TransInfo *t)
{
  if (t->state != TRANS_CANCEL) {
    applyProject(t);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    bArmature *arm = tc->obedit->data;
    ListBase *edbo = arm->edbo;
    EditBone *ebo, *ebo_parent;
    TransData *td = tc->data;
    int i;

    /* Ensure all bones are correctly adjusted */
    for (ebo = edbo->first; ebo; ebo = ebo->next) {
      ebo_parent = (ebo->flag & BONE_CONNECTED) ? ebo->parent : NULL;

      if (ebo_parent) {
        /* If this bone has a parent tip that has been moved */
        if (ebo_parent->flag & BONE_TIPSEL) {
          copy_v3_v3(ebo->head, ebo_parent->tail);
          if (t->mode == TFM_BONE_ENVELOPE) {
            ebo->rad_head = ebo_parent->rad_tail;
          }
        }
        /* If this bone has a parent tip that has NOT been moved */
        else {
          copy_v3_v3(ebo_parent->tail, ebo->head);
          if (t->mode == TFM_BONE_ENVELOPE) {
            ebo_parent->rad_tail = ebo->rad_head;
          }
        }
      }

      /* on extrude bones, oldlength==0.0f, so we scale radius of points */
      ebo->length = len_v3v3(ebo->head, ebo->tail);
      if (ebo->oldlength == 0.0f) {
        ebo->rad_head = 0.25f * ebo->length;
        ebo->rad_tail = 0.10f * ebo->length;
        ebo->dist = 0.25f * ebo->length;
        if (ebo->parent) {
          if (ebo->rad_head > ebo->parent->rad_tail) {
            ebo->rad_head = ebo->parent->rad_tail;
          }
        }
      }
      else if (t->mode != TFM_BONE_ENVELOPE) {
        /* if bones change length, lets do that for the deform distance as well */
        ebo->dist *= ebo->length / ebo->oldlength;
        ebo->rad_head *= ebo->length / ebo->oldlength;
        ebo->rad_tail *= ebo->length / ebo->oldlength;
        ebo->oldlength = ebo->length;

        if (ebo_parent) {
          ebo_parent->rad_tail = ebo->rad_head;
        }
      }
    }

    if (!ELEM(t->mode, TFM_BONE_ROLL, TFM_BONE_ENVELOPE, TFM_BONE_ENVELOPE_DIST, TFM_BONESIZE)) {
      /* fix roll */
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->extra) {
          float vec[3], up_axis[3];
          float qrot[4];
          float roll;

          ebo = td->extra;

          if (t->state == TRANS_CANCEL) {
            /* restore roll */
            ebo->roll = td->ival;
          }
          else {
            copy_v3_v3(up_axis, td->axismtx[2]);

            sub_v3_v3v3(vec, ebo->tail, ebo->head);
            normalize_v3(vec);
            rotation_between_vecs_to_quat(qrot, td->axismtx[1], vec);
            mul_qt_v3(qrot, up_axis);

            /* roll has a tendency to flip in certain orientations - [#34283], [#33974] */
            roll = ED_armature_ebone_roll_to_vector(ebo, up_axis, false);
            ebo->roll = angle_compat_rad(roll, td->ival);
          }
        }
      }
    }

    if (arm->flag & ARM_MIRROR_EDIT) {
      if (t->state != TRANS_CANCEL) {
        ED_armature_edit_transform_mirror_update(tc->obedit);
      }
      else {
        restoreBones(tc);
      }
    }

    /* Tag for redraw/invalidate overlay cache. */
    DEG_id_tag_update(&arm->id, ID_RECALC_SELECT);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Data Pose
 * \{ */

/**
 * if pose bone (partial) selected, copy data.
 * context; posemode armature, with mirror editing enabled.
 */
static void pose_transform_mirror_update(TransInfo *t, TransDataContainer *tc, Object *ob)
{
  float flip_mtx[4][4];
  unit_m4(flip_mtx);
  flip_mtx[0][0] = -1;

  LISTBASE_FOREACH (bPoseChannel *, pchan_orig, &ob->pose->chanbase) {
    /* Clear the MIRROR flag from previous runs. */
    pchan_orig->bone->flag &= ~BONE_TRANSFORM_MIRROR;
  }

  bPose *pose = ob->pose;
  PoseInitData_Mirror *pid = NULL;
  if ((t->mode != TFM_BONESIZE) && (pose->flag & POSE_MIRROR_RELATIVE)) {
    pid = tc->custom.type.data;
  }

  TransData *td = tc->data;
  for (int i = tc->data_len; i--; td++) {
    bPoseChannel *pchan_orig = td->extra;
    BLI_assert(pchan_orig->bone->flag & BONE_TRANSFORM);
    /* No layer check, correct mirror is more important. */
    bPoseChannel *pchan = BKE_pose_channel_get_mirrored(pose, pchan_orig->name);
    if (pchan == NULL) {
      continue;
    }

    /* Also do bbone scaling. */
    pchan->bone->xwidth = pchan_orig->bone->xwidth;
    pchan->bone->zwidth = pchan_orig->bone->zwidth;

    /* We assume X-axis flipping for now. */
    pchan->curve_in_x = pchan_orig->curve_in_x * -1;
    pchan->curve_out_x = pchan_orig->curve_out_x * -1;
    pchan->roll1 = pchan_orig->roll1 * -1;  // XXX?
    pchan->roll2 = pchan_orig->roll2 * -1;  // XXX?

    float pchan_mtx_final[4][4];
    BKE_pchan_to_mat4(pchan_orig, pchan_mtx_final);
    mul_m4_m4m4(pchan_mtx_final, pchan_mtx_final, flip_mtx);
    mul_m4_m4m4(pchan_mtx_final, flip_mtx, pchan_mtx_final);
    if (pid) {
      mul_m4_m4m4(pchan_mtx_final, pid->offset_mtx, pchan_mtx_final);
    }
    BKE_pchan_apply_mat4(pchan, pchan_mtx_final, false);

    /* Set flag to let autokeyframe know to keyframe the mirrred bone. */
    pchan->bone->flag |= BONE_TRANSFORM_MIRROR;

    /* In this case we can do target-less IK grabbing. */
    if (t->mode == TFM_TRANSLATION) {
      bKinematicConstraint *data = has_targetless_ik(pchan);
      if (data == NULL) {
        continue;
      }
      mul_v3_m4v3(data->grabtarget, flip_mtx, td->loc);
      if (pid) {
        /* TODO(germano): Realitve Mirror support */
      }
      data->flag |= CONSTRAINT_IK_AUTO;
      /* Add a temporary auto IK constraint here, as we will only temporarily active this
       * target-less bone during transform. (Target-less IK constraints are treated as if they are
       * disabled unless they are transformed) */
      add_temporary_ik_constraint(pchan, data);
      Main *bmain = CTX_data_main(t->context);
      update_deg_with_temporary_ik(bmain, ob);
    }

    if (pid) {
      pid++;
    }
  }
}

static void pose_mirror_info_restore(const PoseInitData_Mirror *pid)
{
  bPoseChannel *pchan = pid->pchan;
  copy_v3_v3(pchan->loc, pid->orig.loc);
  copy_v3_v3(pchan->size, pid->orig.size);
  pchan->curve_in_x = pid->orig.curve_in_x;
  pchan->curve_out_x = pid->orig.curve_out_x;
  pchan->roll1 = pid->orig.roll1;
  pchan->roll2 = pid->orig.roll2;

  if (pchan->rotmode > 0) {
    copy_v3_v3(pchan->eul, pid->orig.eul);
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    copy_v3_v3(pchan->rotAxis, pid->orig.axis_angle);
    pchan->rotAngle = pid->orig.axis_angle[3];
  }
  else {
    copy_qt_qt(pchan->quat, pid->orig.quat);
  }
}

static void restoreMirrorPoseBones(TransDataContainer *tc)
{
  bPose *pose = tc->poseobj->pose;

  if (!(pose->flag & POSE_MIRROR_EDIT)) {
    return;
  }

  for (PoseInitData_Mirror *pid = tc->custom.type.data; pid->pchan; pid++) {
    pose_mirror_info_restore(pid);
  }
}

void recalcData_pose(TransInfo *t)
{
  if (t->mode == TFM_BONESIZE) {
    /* Handle the exception where for TFM_BONESIZE in edit mode we pretend to be
     * in pose mode (to use bone orientation matrix),
     * in that case we have to do mirroring as well. */
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      Object *ob = tc->poseobj;
      bArmature *arm = ob->data;
      if (ob->mode == OB_MODE_EDIT) {
        if (arm->flag & ARM_MIRROR_EDIT) {
          if (t->state != TRANS_CANCEL) {
            ED_armature_edit_transform_mirror_update(ob);
          }
          else {
            restoreBones(tc);
          }
        }
      }
      else if (ob->mode == OB_MODE_POSE) {
        /* actually support TFM_BONESIZE in posemode as well */
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        bPose *pose = ob->pose;
        if (arm->flag & ARM_MIRROR_EDIT || pose->flag & POSE_MIRROR_EDIT) {
          pose_transform_mirror_update(t, tc, ob);
        }
      }
    }
  }
  else {
    GSet *motionpath_updates = BLI_gset_ptr_new("motionpath updates");

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      Object *ob = tc->poseobj;
      bPose *pose = ob->pose;

      if (pose->flag & POSE_MIRROR_EDIT) {
        if (t->state != TRANS_CANCEL) {
          pose_transform_mirror_update(t, tc, ob);
        }
        else {
          restoreMirrorPoseBones(tc);
        }
      }

      /* if animtimer is running, and the object already has animation data,
       * check if the auto-record feature means that we should record 'samples'
       * (i.e. un-editable animation values)
       *
       * context is needed for keying set poll() functions.
       */

      /* TODO: autokeyframe calls need some setting to specify to add samples
       * (FPoints) instead of keyframes? */
      if ((t->animtimer) && (t->context) && IS_AUTOKEY_ON(t->scene)) {
        int targetless_ik =
            (t->flag & T_AUTOIK);  // XXX this currently doesn't work, since flags aren't set yet!

        animrecord_check_state(t, ob);
        autokeyframe_pose(t->context, t->scene, ob, t->mode, targetless_ik);
      }

      if (motionpath_need_update_pose(t->scene, ob)) {
        BLI_gset_insert(motionpath_updates, ob);
      }

      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }

    /* Update motion paths once for all transformed bones in an object. */
    GSetIterator gs_iter;
    GSET_ITER (gs_iter, motionpath_updates) {
      Object *ob = BLI_gsetIterator_getKey(&gs_iter);
      ED_pose_recalculate_paths(t->context, t->scene, ob, POSE_PATH_CALC_RANGE_CURRENT_FRAME);
    }
    BLI_gset_free(motionpath_updates, NULL);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Pose
 * \{ */

static void bone_children_clear_transflag(int mode, short around, ListBase *lb)
{
  Bone *bone = lb->first;

  for (; bone; bone = bone->next) {
    if ((bone->flag & BONE_HINGE) && (bone->flag & BONE_CONNECTED)) {
      bone->flag |= BONE_HINGE_CHILD_TRANSFORM;
    }
    else if ((bone->flag & BONE_TRANSFORM) && (mode == TFM_ROTATION || mode == TFM_TRACKBALL) &&
             (around == V3D_AROUND_LOCAL_ORIGINS)) {
      bone->flag |= BONE_TRANSFORM_CHILD;
    }
    else {
      bone->flag &= ~BONE_TRANSFORM;
    }

    bone_children_clear_transflag(mode, around, &bone->childbase);
  }
}

/* Sets transform flags in the bones.
 * Returns total number of bones with `BONE_TRANSFORM`. */
int transform_convert_pose_transflags_update(Object *ob,
                                             const int mode,
                                             const short around,
                                             bool has_translate_rotate[2])
{
  bArmature *arm = ob->data;
  bPoseChannel *pchan;
  Bone *bone;
  int total = 0;

  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    bone = pchan->bone;
    if (PBONE_VISIBLE(arm, bone)) {
      if ((bone->flag & BONE_SELECTED)) {
        bone->flag |= BONE_TRANSFORM;
      }
      else {
        bone->flag &= ~BONE_TRANSFORM;
      }

      bone->flag &= ~BONE_HINGE_CHILD_TRANSFORM;
      bone->flag &= ~BONE_TRANSFORM_CHILD;
    }
    else {
      bone->flag &= ~BONE_TRANSFORM;
    }
  }

  /* make sure no bone can be transformed when a parent is transformed */
  /* since pchans are depsgraph sorted, the parents are in beginning of list */
  if (!ELEM(mode, TFM_BONESIZE, TFM_BONE_ENVELOPE_DIST)) {
    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      bone = pchan->bone;
      if (bone->flag & BONE_TRANSFORM) {
        bone_children_clear_transflag(mode, around, &bone->childbase);
      }
    }
  }
  /* now count, and check if we have autoIK or have to switch from translate to rotate */
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    bone = pchan->bone;
    if (bone->flag & BONE_TRANSFORM) {
      total++;

      if (has_translate_rotate != NULL) {
        if (has_targetless_ik(pchan) == NULL) {
          if (pchan->parent && (pchan->bone->flag & BONE_CONNECTED)) {
            if (pchan->bone->flag & BONE_HINGE_CHILD_TRANSFORM) {
              has_translate_rotate[0] = true;
            }
          }
          else {
            if ((pchan->protectflag & OB_LOCK_LOC) != OB_LOCK_LOC) {
              has_translate_rotate[0] = true;
            }
          }
          if ((pchan->protectflag & OB_LOCK_ROT) != OB_LOCK_ROT) {
            has_translate_rotate[1] = true;
          }
        }
        else {
          has_translate_rotate[0] = true;
        }
      }
    }
  }

  return total;
}

static short apply_targetless_ik(Object *ob)
{
  bPoseChannel *pchan, *parchan, *chanlist[256];
  bKinematicConstraint *data;
  int segcount, apply = 0;

  /* now we got a difficult situation... we have to find the
   * target-less IK pchans, and apply transformation to the all
   * pchans that were in the chain */

  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    data = has_targetless_ik(pchan);
    if (data && (data->flag & CONSTRAINT_IK_AUTO)) {

      /* fill the array with the bones of the chain (armature.c does same, keep it synced) */
      segcount = 0;

      /* exclude tip from chain? */
      if (!(data->flag & CONSTRAINT_IK_TIP)) {
        parchan = pchan->parent;
      }
      else {
        parchan = pchan;
      }

      /* Find the chain's root & count the segments needed */
      for (; parchan; parchan = parchan->parent) {
        chanlist[segcount] = parchan;
        segcount++;

        if (segcount == data->rootbone || segcount > 255) {
          break;  // 255 is weak
        }
      }
      for (; segcount; segcount--) {
        Bone *bone;
        float mat[4][4];

        /* pose_mat(b) = pose_mat(b-1) * offs_bone * channel * constraint * IK  */
        /* we put in channel the entire result of mat = (channel * constraint * IK) */
        /* pose_mat(b) = pose_mat(b-1) * offs_bone * mat  */
        /* mat = pose_mat(b) * inv(pose_mat(b-1) * offs_bone ) */

        parchan = chanlist[segcount - 1];
        bone = parchan->bone;
        bone->flag |= BONE_TRANSFORM; /* ensures it gets an auto key inserted */

        BKE_armature_mat_pose_to_bone(parchan, parchan->pose_mat, mat);
        /* apply and decompose, doesn't work for constraints or non-uniform scale well */
        {
          float rmat3[3][3], qrmat[3][3], imat3[3][3], smat[3][3];

          copy_m3_m4(rmat3, mat);
          /* Make sure that our rotation matrix only contains rotation and not scale. */
          normalize_m3(rmat3);

          /* rotation */
          /* [#22409] is partially caused by this, as slight numeric error introduced during
           * the solving process leads to locked-axis values changing. However, we cannot modify
           * the values here, or else there are huge discrepancies between IK-solver (interactive)
           * and applied poses. */
          BKE_pchan_mat3_to_rot(parchan, rmat3, false);

          /* for size, remove rotation */
          /* causes problems with some constraints (so apply only if needed) */
          if (data->flag & CONSTRAINT_IK_STRETCH) {
            BKE_pchan_rot_to_mat3(parchan, qrmat);
            invert_m3_m3(imat3, qrmat);
            mul_m3_m3m3(smat, rmat3, imat3);
            mat3_to_size(parchan->size, smat);
          }

          /* causes problems with some constraints (e.g. childof), so disable this */
          /* as it is IK shouldn't affect location directly */
          /* copy_v3_v3(parchan->loc, mat[3]); */
        }
      }

      apply = 1;
      data->flag &= ~CONSTRAINT_IK_AUTO;
    }
  }

  return apply;
}

/* frees temporal IKs */
static void pose_grab_with_ik_clear(Main *bmain, Object *ob)
{
  bKinematicConstraint *data;
  bPoseChannel *pchan;
  bConstraint *con, *next;
  bool relations_changed = false;

  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    /* clear all temporary lock flags */
    pchan->ikflag &= ~(BONE_IK_NO_XDOF_TEMP | BONE_IK_NO_YDOF_TEMP | BONE_IK_NO_ZDOF_TEMP);

    pchan->constflag &= ~(PCHAN_HAS_IK | PCHAN_HAS_TARGET);

    /* remove all temporary IK-constraints added */
    for (con = pchan->constraints.first; con; con = next) {
      next = con->next;
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        data = con->data;
        if (data->flag & CONSTRAINT_IK_TEMP) {
          relations_changed = true;

          /* iTaSC needs clear for removed constraints */
          BIK_clear_data(ob->pose);

          BLI_remlink(&pchan->constraints, con);
          MEM_freeN(con->data);
          MEM_freeN(con);
          continue;
        }
        pchan->constflag |= PCHAN_HAS_IK;
        if (data->tar == NULL || (data->tar->type == OB_ARMATURE && data->subtarget[0] == 0)) {
          pchan->constflag |= PCHAN_HAS_TARGET;
        }
      }
    }
  }

  if (relations_changed) {
    /* TODO(sergey): Consider doing partial update only. */
    DEG_relations_tag_update(bmain);
  }
}

void special_aftertrans_update__pose(bContext *C, TransInfo *t)
{
  Object *ob;

  if (t->mode == TFM_BONESIZE) {
    /* Handle the exception where for TFM_BONESIZE in edit mode we pretend to be
     * in pose mode (to use bone orientation matrix),
     * in that case we don't do operations like auto-keyframing. */
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      ob = tc->poseobj;
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }
  else {
    const bool canceled = (t->state == TRANS_CANCEL);
    GSet *motionpath_updates = BLI_gset_ptr_new("motionpath updates");

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {

      bPoseChannel *pchan;
      short targetless_ik = 0;

      ob = tc->poseobj;

      if ((t->flag & T_AUTOIK) && (t->options & CTX_AUTOCONFIRM)) {
        /* when running transform non-interactively (operator exec),
         * we need to update the pose otherwise no updates get called during
         * transform and the auto-ik is not applied. see [#26164] */
        struct Object *pose_ob = tc->poseobj;
        BKE_pose_where_is(t->depsgraph, t->scene, pose_ob);
      }

      /* set BONE_TRANSFORM flags for autokey, gizmo draw might have changed them */
      if (!canceled && (t->mode != TFM_DUMMY)) {
        transform_convert_pose_transflags_update(ob, t->mode, t->around, NULL);
      }

      /* if target-less IK grabbing, we calculate the pchan transforms and clear flag */
      if (!canceled && t->mode == TFM_TRANSLATION) {
        targetless_ik = apply_targetless_ik(ob);
      }
      else {
        /* not forget to clear the auto flag */
        for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
          bKinematicConstraint *data = has_targetless_ik(pchan);
          if (data) {
            data->flag &= ~CONSTRAINT_IK_AUTO;
          }
        }
      }

      if (t->mode == TFM_TRANSLATION) {
        struct Main *bmain = CTX_data_main(t->context);
        pose_grab_with_ik_clear(bmain, ob);
      }

      /* automatic inserting of keys and unkeyed tagging -
       * only if transform wasn't canceled (or TFM_DUMMY) */
      if (!canceled && (t->mode != TFM_DUMMY)) {
        autokeyframe_pose(C, t->scene, ob, t->mode, targetless_ik);
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
      else {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }

      if (t->mode != TFM_DUMMY && motionpath_need_update_pose(t->scene, ob)) {
        BLI_gset_insert(motionpath_updates, ob);
      }
    }

    /* Update motion paths once for all transformed bones in an object. */
    GSetIterator gs_iter;
    GSET_ITER (gs_iter, motionpath_updates) {
      const ePosePathCalcRange range = canceled ? POSE_PATH_CALC_RANGE_CURRENT_FRAME :
                                                  POSE_PATH_CALC_RANGE_CHANGED;
      ob = BLI_gsetIterator_getKey(&gs_iter);
      ED_pose_recalculate_paths(C, t->scene, ob, range);
    }
    BLI_gset_free(motionpath_updates, NULL);
  }
}

/** \} */
