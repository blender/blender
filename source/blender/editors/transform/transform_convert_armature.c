/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edtransform
 */

#include "DNA_action_types.h"
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
#include "RNA_prototypes.h"

#include "transform.h"
#include "transform_orientations.h"
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

  if (!autokeyframe_cfra_can_key(scene, id)) {
    return;
  }

  ReportList *reports = CTX_wm_reports(C);
  ToolSettings *ts = scene->toolsettings;
  KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
  ListBase nla_cache = {NULL, NULL};
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, (float)scene->r.cfra);
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

  const bool is_auto_ik = (pose->flag & POSE_AUTO_IK) != 0;
  const bool mirror = !is_auto_ik && ((pose->flag & POSE_MIRROR_EDIT) != 0);
  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    if ((pchan->bone->flag & BONE_TRANSFORM) == 0 &&
        !(mirror && (pchan->bone->flag & BONE_TRANSFORM_MIRROR)))
    {
      continue;
    }

    ListBase dsources = {NULL, NULL};

    /* Add data-source override for the camera object. */
    ANIM_relative_keyingset_add_source(&dsources, id, &RNA_PoseBone, pchan);

    /* only insert into active keyingset? */
    if (IS_AUTOKEY_FLAG(scene, ONLYKEYINGSET) && (active_ks)) {
      /* Run the active Keying Set on the current data-source. */
      ANIM_apply_keyingset(
          C, &dsources, NULL, active_ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
    }
    /* only insert into available channels? */
    else if (IS_AUTOKEY_FLAG(scene, INSERTAVAIL)) {
      if (act) {
        for (fcu = act->curves.first; fcu; fcu = fcu->next) {
          /* only insert keyframes for this F-Curve if it affects the current bone */
          char pchan_name[sizeof(pchan->name)];
          if (!BLI_str_quoted_substr(fcu->rna_path, "bones[", pchan_name, sizeof(pchan_name))) {
            continue;
          }

          /* only if bone name matches too...
           * NOTE: this will do constraints too, but those are ok to do here too?
           */
          if (STREQ(pchan_name, pchan->name)) {
            insert_keyframe(bmain,
                            reports,
                            id,
                            act,
                            ((fcu->grp) ? (fcu->grp->name) : (NULL)),
                            fcu->rna_path,
                            fcu->array_index,
                            &anim_eval_context,
                            ts->keyframe_type,
                            &nla_cache,
                            flag);
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
        do_loc = true;
      }
      else if (ELEM(tmode, TFM_ROTATION, TFM_TRACKBALL)) {
        if (ELEM(scene->toolsettings->transform_pivot_point, V3D_AROUND_CURSOR, V3D_AROUND_ACTIVE))
        {
          do_loc = true;
        }

        if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
          do_rot = true;
        }
      }
      else if (tmode == TFM_RESIZE) {
        if (ELEM(scene->toolsettings->transform_pivot_point, V3D_AROUND_CURSOR, V3D_AROUND_ACTIVE))
        {
          do_loc = true;
        }

        if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
          do_scale = true;
        }
      }

      if (do_loc) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOCATION_ID);
        ANIM_apply_keyingset(
            C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
      }
      if (do_rot) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_ROTATION_ID);
        ANIM_apply_keyingset(
            C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
      }
      if (do_scale) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_SCALING_ID);
        ANIM_apply_keyingset(
            C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
      }
    }
    /* insert keyframe in all (transform) channels */
    else {
      KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOC_ROT_SCALE_ID);
      ANIM_apply_keyingset(
          C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
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
  bConstraint *con = pchan->constraints.last;

  for (; con; con = con->prev) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC && (con->flag & CONSTRAINT_OFF) == 0 &&
        (con->enforce != 0.0f))
    {
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

static bConstraint *get_last_ik(bPoseChannel *pchan)
{
  bConstraint *con = pchan->constraints.last;

  for (; con; con = con->prev) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC && (con->flag & CONSTRAINT_OFF) == 0 &&
        (con->enforce != 0.0f))
    {
      return con;
    }
  }
  return NULL;
}

static bool is_targeted_ik(bKinematicConstraint *data)
{
  if (data->tar == NULL) {
    return false;
  }
  if (data->tar->type == OB_ARMATURE && data->subtarget[0] == 0) {
    return false;
  }
  return true;
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
              /* Here, we set IK-settings for bone from `pchan->protectflag`. */
              /* XXX: careful with quaternion/axis-angle rotations
               * where we're locking 4d components. */
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

          /* Return early (as in: don't actually create a temporary constraint here), since adding
           * will take place later in add_pose_transdata() for targetless constraints. */
          return 0;
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
   * same bone for the first time through the loop #25885. */
  data->rootbone = 0;

  /* we only include bones that are part of a continual connected chain */
  do {
    /* Here, we set IK-settings for bone from `pchan->protectflag`. */
    /* XXX: careful with quaternion/axis-angle rotations where we're locking 4D components. */
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

typedef enum eIKGrabDataFlag {
  IKGRAB_FLAG_REDIRECT_TD_LOC = (1 << 0),
} eIKGrabDataFlag;

typedef enum eIKGrabDataSyncMode {
  IKGRAB_MODE_SYNC_AT_HEAD = 0,
  IKGRAB_MODE_SYNC_AT_TAIL = 1,
} eIKGrabDataSyncMode;

typedef struct IKGrabData {
  bPoseChannel *pchan;
  /** TODO: GG: rename these fields to be more descriptive of their meaning instead of what they
   * eventually write to... Looking back at this 6 months later, these vars mean nothing at a
   * glance... or maybe the understanding is obvious from when they're written to*/
  /** Set to pose_head or pose_tail, depending on whether grabbed_pchan is the implicit target
   * (pose_head) or grabbed_pchan ...(TODO: GG: improve description..) */
  /** GG: Q: Is this an offset to the .. pivot??
   * Grabbed pose-space location of pchan.
   */
  float td_center[3];
  // float _pad;
  /** GG: Q: Pointer to pchan's original location? What is this? */
  float *td_loc;
  char flag;
  float pchan_length;

  bKinematicConstraint *synced_ik_data;
  char sync_mode;
} IKGrabData;

typedef struct IKGrabDatas {
  struct IKGrabDatas *prev, *next;

  struct IKGrabData *buffer;
  int total;
} IKGrabDatas;

typedef struct IKChain2Way {
  struct IKChain2Way *prev, *next;

  ListBase owner_chain;
  ListBase target_chain;
} IKChain2Way;

static short pose_grab_with_ik_simpler(Main *bmain,
                                       Object *ob,
                                       IKGrabDatas *r_grab_datas,
                                       ListBase *ensured_keyed_pchans)
{
  /**
   * DESIGN: AutoIK must respect user ik locks and preserve pinned nonselected bones through the ik
   * system. Preserving pinned nonselected bones through any means and without restriction is
   * insufficent and not the goal.
   *
   * DESIGN: AutoIK pinning should not be animatable. Otherwise, we're adding an additional
   * constraint system ontop of the existing one, further complicating Blender's animation and
   * rigging system. If the user wants such behavior, then they should add the support themselves.
   *
   * We must respect the user's ik locks. Imagine a staff being held by 2 twoway IK hands. The
   * attachment points on the staff can only slide along the staff's axis. If we were to pin the
   * attachment point, then forcefully fix the tail in recalcData_pose(), then the attachment
   * point's off-staff axis position may be nonzero which isn't what the animator wants- they want
   * the hands to remain attached to the attachment points and keep the attachment points on the
   * staff. Animators expect AutoIK pinned bones to act the same as manually pinned bones. They
   * must create an IK constraint and target to keep the pinned bones in place, which is exactly
   * what we do, though they have more control over exact how and the resulting behavior.
   *
   * TODO: GG: For selected bones with animspace locked properties, add support for  AutoIK option
   * for whether to constrain xform to the locks or not. (restspace locks require we respect the
   * locks). For animspace, there is more freedom of xform.
   *
   * BUG: GG: For 2way IK, grabbing the owner tip doesn't cause target tip to follow
   *    -need to create a oneway for target to owner in this case.
   *    -owner chain also needs to become a oneway instead to ensure its pose is kept (not neccsary
   * actually)
   *
   * BUG: GG: For 2way IK, grabbing tip adn target doesn't actually grab target.
   *
   */

  bool any_temp_ik_created = false;
  if ((ob == NULL) || (ob->pose == NULL) || (ob->mode & OB_MODE_POSE) == 0) {
    return any_temp_ik_created;
  }

  /* Pin all selected pose bones. */
  bool any_bone_transformed = false; 
  GSet *pinned_pbones = BLI_gset_ptr_new(__func__);
  for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    const bool is_selected = (pchan->bone->flag & BONE_SELECTED) != 0;
    const bool is_visible = BKE_pose_is_layer_visible(ob->data, pchan);
    const bool is_transformed = (is_selected && is_visible);
    any_bone_transformed |= is_transformed;

    if ((pchan->ikflag_general & BONE_AUTOIK_DO_PIN) != 0 &&
        (pchan->ikflag_general & BONE_AUTOIK_DO_PIN_ANY) != 0)
    {
      BLI_gset_insert(pinned_pbones, pchan);
      continue;
    }

    if (!is_transformed) {
      continue;
    }
    BLI_gset_insert(pinned_pbones, pchan);
  }
  
  if(!any_bone_transformed){
    BLI_gset_free(pinned_pbones, NULL);
    pinned_pbones = NULL;
    any_temp_ik_created = false;
    return any_temp_ik_created;
  }

  GHash *pchans_from_posetree_pchan = NULL;
  {
    GHash *solver_from_chain_root = BKE_determine_posetree_roots(&ob->pose->chanbase);
    GHash *explicit_pchans_from_posetree_pchan;
    GHash *implicit_pchans_from_posetree_pchan;
    BKE_determine_posetree_pchan_implicity(&ob->pose->chanbase,
                                           solver_from_chain_root,
                                           &explicit_pchans_from_posetree_pchan,
                                           &implicit_pchans_from_posetree_pchan);
    pchans_from_posetree_pchan = BKE_union_pchans_from_posetree(
        explicit_pchans_from_posetree_pchan, implicit_pchans_from_posetree_pchan);

    BLI_ghash_free(solver_from_chain_root, NULL, NULL);
    BLI_ghash_free(explicit_pchans_from_posetree_pchan, NULL, BLI_gset_freefp_no_keyfree);
    BLI_ghash_free(implicit_pchans_from_posetree_pchan, NULL, BLI_gset_freefp_no_keyfree);
    solver_from_chain_root = NULL;
    explicit_pchans_from_posetree_pchan = NULL;
    implicit_pchans_from_posetree_pchan = NULL;
  }

  const bool do_op_chain_length_override_pose = (ob->pose->flag &
                                                 POSE_AUTO_IK_USE_OPERATOR_CHAIN_LENGTH) != 0;
  const int max_expected_grab_datas = BLI_gset_len(pinned_pbones);
  IKGrabData *all_grab_datas = MEM_callocN(max_expected_grab_datas * sizeof(IKGrabData), __func__);
  int index_grab_data = 0;
  GSET_FOREACH_BEGIN (bPoseChannel *, pchan_pin, pinned_pbones) {
    const bool is_dynamic_grab_location = pchan_pin->bone->flag & BONE_SELECTED;

    const bool any_posetree_evaluates_pchan = BKE_posetree_any_has_pchan(
        pchans_from_posetree_pchan, pchan_pin);
    /* If pchan_pin is not part of chain, then xform w/ non autoik behavior. */
    /** XXX:X pchan may not be free when a pinned bone may create a posetree that itll be part
     * of!*/
    // if (!any_posetree_evaluates_pchan && is_dynamic_grab_location && (pchan_pin->parent ==
    // NULL)) {
    //   IKGrabData *grab_data = all_grab_datas + index_grab_data;
    //   index_grab_data++;

    //   grab_data->pchan = pchan_pin;
    //   /* Redundant flag set, but explicit for clarity. */
    //   grab_data->flag &= ~IKGRAB_FLAG_REDIRECT_TD_LOC;
    //   grab_data->synced_ik_data = NULL;
    //   continue;
    // }

    const bool is_pchan_selected = (pchan_pin->bone->flag & BONE_SELECTED) != 0;
    const bool do_pin_masking = is_pchan_selected;
    // GG: CLEANUP: move outside loop. 
    const bool mask_pin_head = ob->pose->flag & POSE_AUTO_IK_SELECTION_PIN_HEAD;
    const bool mask_pin_tail = ob->pose->flag & POSE_AUTO_IK_SELECTION_PIN_TAIL;
    const bool mask_pin_rotation = ob->pose->flag & POSE_AUTO_IK_SELECTION_PIN_ROTATION;

    const bool do_pin_head = (do_pin_masking && mask_pin_head) ||
                             (!do_pin_masking &&
                              (pchan_pin->ikflag_general & BONE_AUTOIK_DO_PIN_HEAD));
    const bool do_pin_tail = (do_pin_masking && mask_pin_tail) ||
                             (!do_pin_masking &&
                              (pchan_pin->ikflag_general & BONE_AUTOIK_DO_PIN_TAIL));
    const bool do_pin_rotation = (do_pin_masking && mask_pin_rotation) ||
                                 (!do_pin_masking &&
                                  (pchan_pin->ikflag_general & BONE_AUTOIK_DO_PIN_ROTATION));

    const bool do_pin_location = do_pin_head || do_pin_tail;

    bConstraint *base_ik_con = get_last_ik(pchan_pin);
    if (base_ik_con != NULL) {
      const bool initially_enabled = (base_ik_con->flag & CONSTRAINT_DISABLE) == 0;
      if (initially_enabled) {
        base_ik_con->flag |= CONSTRAINT_TEMP_DISABLED_DURING_TRANSFORM;
      }
      base_ik_con->flag |= CONSTRAINT_DISABLE;
    }

    bKinematicConstraint *base_ik = NULL;
    if (base_ik_con) {
      base_ik = base_ik_con->data;
    }
    bConstraint *auto_ik = add_temporary_ik_constraint(pchan_pin, base_ik);
    any_temp_ik_created = true;

    /** GG: NOTE: I intentionally did not mark chain pchan's tmp IK lock based on
     * pchan->protectflag because... well normal Ik eval doesn't respect them so doing so is a bit
     * inconsistent. (verify: Out of date?)
     */
    bKinematicConstraint *ikdata = auto_ik->data;
    ikdata->autoik_flag |= CONSTRAINT_AUTOIK_ENABLED;

    const bool do_op_override_chain_length_bone = do_op_chain_length_override_pose &&
                                                  is_pchan_selected;
    const bool do_inherit_chain_length = !do_op_override_chain_length_bone &&
                                         ((pchan_pin->ikflag_general &
                                           BONE_AUTOIK_INHERIT_CHAIN_LENGTH) != 0);
    const bool is_targeted_base_ik = base_ik && base_ik->tar &&
                                     (base_ik->tar->type != OB_ARMATURE ||
                                      (base_ik->tar->type == OB_ARMATURE &&
                                       base_ik->subtarget[0] != 0));
    if (!is_targeted_base_ik && do_inherit_chain_length && any_posetree_evaluates_pchan) {
      /* NOTE: This flag is off when !any_posetree_evaluates_pchan since we need the constraint to
       * generate an iksolver and posetree.*/
      ikdata->flag |= CONSTRAINT_IK_DO_NOT_CREATE_POSETREE;
    }
    
    const bool use_manual_length = (pchan_pin->ikflag_general &
                                    BONE_AUTOIK_DERIVE_CHAIN_LENGTH_FROM_CONNECT) == 0;
    // const bool derive_length_from_connect = !use_manual_length;.
    
    /* Set a useful chain length when pchan_pin is not part of any posetree so it must use its
      * ik constraint as a posetree solver. */
    if(base_ik){
      ikdata->max_rootbone = base_ik->rootbone;
    }
    else if(use_manual_length){
      ikdata->rootbone = pchan_pin->autoik_chain_length;
      ikdata->max_rootbone = pchan_pin->autoik_chain_length;
    }
    else {
      int chain_length = 1;
      for (bPoseChannel *chain_pbone = pchan_pin->parent, *prev_pbone = pchan_pin; chain_pbone;
           chain_pbone = chain_pbone->parent, chain_length++)
      {
            
        if ((prev_pbone->bone->flag & BONE_CONNECTED) == 0) {
          break;
        }
        prev_pbone = chain_pbone;
      }
      ikdata->rootbone = chain_length;
      ikdata->max_rootbone = 0;
    }

    if (do_pin_head) {
      ikdata->autoik_flag |= CONSTRAINT_AUTOIK_USE_HEAD;
      ikdata->autoik_weight_head = 1; /** GG: TODO: Use a user weight. */
    }
    if (do_pin_tail) {
      ikdata->autoik_flag |= CONSTRAINT_AUTOIK_USE_TAIL;
      ikdata->autoik_weight_tail = 1; /** GG: TODO: Use a user weight. */
    }
    if (do_pin_rotation) {
      ikdata->autoik_flag |= CONSTRAINT_AUTOIK_USE_ROTATION;
      ikdata->autoik_weight_rotation = 1; /** GG: TODO: Use a user weight. */
    }

    // if (base_ik == NULL) {
    //   ikdata->iterations = ikdata->iterations / 10;
    // }
    // else {
    //   ikdata->iterations = base_ik->iterations / 10;
    // }
   
    /** GG: XXX: I don't know why but using pose_head here works just fine. I assume the xform
     * system applies deltas in globalspace, thus we can sort of store whatever value we want here.
     * By initializing to pose_head, then recalcData_pose() doesn't have to do any additional
     * matrix math to find the updated grabtarget position. Doing it the proper(?) way and using
     * td's matrices leads to bugs because.. well I don't understand it well enough to do it the
     * proper way.
     */
    copy_v3_v3(ikdata->grabtarget, pchan_pin->pose_head);
    // copy_v3_v3(ikdata->grabtarget, pchan_pin->loc);
    /** (is this comment still valid?) TODO: GG: Currently we lose the ability to pin both head and
     * tail at same time (with !pin_rotation). Such a setup would have allowed the bone to still
     * twist as its hierarchy is iksolved. */
    copy_v3_v3(ikdata->autoik_target_tail, pchan_pin->pose_tail);
    copy_m3_m4(ikdata->rotation_target, pchan_pin->pose_mat);

    if (!is_dynamic_grab_location) {
      /* No need to create an grab data since pinned bone is not selected. */
      continue;
    }

    /** (out of date comment?) TODO: generate grab data and syncing grab targets and stuff */
    IKGrabData *grab_data = all_grab_datas + index_grab_data;
    index_grab_data++;
    grab_data->pchan = pchan_pin;
    grab_data->td_loc = ikdata->grabtarget;
    if (!do_pin_head && do_pin_tail) {
      /* Condition synced within recalcData_pose(). */
      grab_data->td_loc = ikdata->autoik_target_tail;
    }

    grab_data->flag |= IKGRAB_FLAG_REDIRECT_TD_LOC;
    grab_data->synced_ik_data = ikdata;
    grab_data->pchan_length = len_v3v3(pchan_pin->pose_head, pchan_pin->pose_tail);

    /* Set td_center to the posespace of pchan_pin's at-rest head location relative to its animated
     * parent. */
    // BoneParentTransform bpt;
    // BKE_bone_parent_transform_calc_from_pchan(pchan_pin, &bpt);
    // copy_v3_v3(grab_data->td_center, bpt.loc_mat[3]);
    copy_v3_v3(grab_data->td_center, ikdata->grabtarget);
  }
  GSET_FOREACH_END();

  // BLI_assert_msg(false, "TODO: maek buffer a list or vie versa");
  r_grab_datas->buffer = all_grab_datas;
  r_grab_datas->total = index_grab_data;
  /** TODO: GG: XXX: ... definitely shuold've just made grab datas a listbase.. */
  BLI_assert_msg(index_grab_data <= max_expected_grab_datas, "Buffer overflow!");

  BLI_gset_free(pinned_pbones, NULL);
  pinned_pbones = NULL;

  BLI_ghash_free(pchans_from_posetree_pchan, NULL, BLI_gset_freefp_no_keyfree);
  pchans_from_posetree_pchan = NULL;
  /*
   * Foreach pinned bone, add a temporary IK:
   *    if bone already had an IK, then disable it and reuse its chain length. Ensure it still gens
   * a posetree. Create grab data for pinned parts.
   *
   *    if pin head, then sync ik's grabtarget head and flag USE_TIP_HEAD_AS_EE
   *    if pin tail, then sync ik's grabtarget head and disable USE_TIP_HEAD_AS_EE
   *    if pin rot, then sync ik's grabrotation
   *
   *    -if bone not part of any existing ik chain:
   *        -then chain length extends as long as each bone is connected. If chainlength=1, then
   *        ignore the disconnection and continue extending (a single length chain cannot preserve
   *        itself if any of its properties are locked since iksolver not allowed to modify locked
   *        properties and neither are we)
   *   -if bone is part of an existing ik chain:
   *        -then just tag the constraint as CONSTRAINT_IK_DONT_CREATE_POSETREE so that depsgraph
   *        and iksolver don't generate new ik posetrees due to this constraint. Any posetree that
   *        contains the bone will generate targets (to preserve loc/rot). This is simpler than
   *        explicitly searchign for the last evaluated posetree then ensure the chain length is
   *        such that the constraint evaluates as part of it... When AutoIK applies VisualXform on
   *        grab init, hopefully the differences won't matter (really, if a smaller later
   *        evaluating posetree can't preserve the selected bone, then it's probably not what the
   *        user wanted anyways)
   */
  /* iTaSC needs clear for new IK constraints */
  if (any_temp_ik_created) {
    update_deg_with_temporary_ik(bmain, ob);
  }

  return any_temp_ik_created;
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

typedef struct PoseData_AutoIK {
  struct bPoseChannel *pchan;
  bKinematicConstraint *synced_ik_data;
  char sync_mode;
  float initial_length; 

  // GG: inconsistency: td->loc points to ik_constraint_data->grabtarget but the rotations are
  // explicitly placed here.
  float eul[3];
  float quat[4];
  float rotAxis[3], rotAngle;

  /* Scale not included we never overwrite it anyways. */
  float initial_loc[3];
  float initial_eul[3];
  float initial_quat[4];
  float initial_rotAxis[3];
  float initial_rotAngle;
  float initial_scale[3]; 
} PoseData_AutoIK;

typedef struct PoseData {
  struct PoseInitData_Mirror *mirror;
  /* Element per TransData. Length matchs TransDataContainer. */
  struct PoseData_AutoIK *autoik;
  /** GG: CLEANUP: UNUSED */
  struct ListBase ensured_keyed_pchans;
} PoseData;

static void free_transcustomdata_posedata(TransInfo *t,
                                          TransDataContainer *UNUSED(tc),
                                          TransCustomData *custom_data)
{
  PoseData *pd = custom_data->data;

  MEM_SAFE_FREE(pd->mirror);
  MEM_SAFE_FREE(pd->autoik);
  BLI_freelistN(&pd->ensured_keyed_pchans);
  MEM_freeN(custom_data->data);

  custom_data->data = NULL;
}

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

static void pchan_apply_posemat_from_ik(bPoseChannel *pchan, float pose_mat[4][4], bool do_scale)
{
  float chan_mat[4][4];
  BKE_armature_mat_pose_to_bone(pchan, pose_mat, chan_mat);

  /* apply and decompose, doesn't work for constraints or non-uniform scale well */
  if ((pchan->bone->flag & BONE_CONNECTED) == 0) {
    copy_v3_v3(pchan->loc, chan_mat[3]);
  }

  float rmat3[3][3];
  copy_m3_m4(rmat3, chan_mat);
  /* Make sure that our rotation matrix only contains rotation and not scale. */
  normalize_m3(rmat3);

  /* rotation */
  /* #22409 is partially caused by this, as slight numeric error introduced during
   * the solving process leads to locked-axis values changing. However, we cannot modify
   * the values here, or else there are huge discrepancies between IK-solver (interactive)
   * and applied poses. */
  if (pchan->rotmode == ROT_MODE_QUAT) {
    float tmp_quat[4];
    mat3_normalized_to_quat(tmp_quat, rmat3);

    float quat_orig[4];
    copy_v4_v4(quat_orig, pchan->quat);
    quat_to_compatible_quat(pchan->quat, tmp_quat, quat_orig);
  }
  else {
    BKE_pchan_mat3_to_rot(pchan, rmat3, true);
  }

  /* for size, remove rotation */
  /* causes problems with some constraints (so apply only if needed) */
  // GG: TODO: only write scale if any ik in posetree is using scale and
  // only for the bones that are stretched

  /** GG: TODO: I wonder if BKE_determine_posetree_roots() should provide more data about
   * posetrees? In this case, we need to know if every assoc. ik chain is stretched. If so
   * then so is the posetree. */
  // const bool is_posetree_stretch = data->flag & CONSTRAINT_IK_STRETCH;
  if (!do_scale) {
    return;
  }

  float qrmat[3][3], imat3[3][3], smat[3][3];
  BKE_pchan_rot_to_mat3(pchan, qrmat);
  invert_m3_m3(imat3, qrmat);
  // GG: Is this a bug in vanilla blender that just doesn't occur? (original forgets to copy and
  // thus scale is always 1) GG: inconvenient: should autoIK treat scale diff from loc/rot? wihle
  // rotating a bone, it gets ik scaled.
  //  then autoIK applies that scale to the bone. Further auotiK rots will never return the bone to
  //  its orignal scale. So maybe, it's generally more useful if scale is never applied?...but then
  //  the pose isn't preserved..
  // perhaps limit expected use case of autoik for when bones aren't actively ikstretched? - we can
  // still apply scale to ensure pose is preserved.. or temporarily dsiabl all ikstretch during
  // autoik+apply_scale to preserve existing scale? that way the pose is preserved w/o having to
  // apply or be affected by an active ikstretch? then when ikstretch re-enabled, since the pose
  // was preserved, then it won't grow/shrink anyways?.. that still has compounded scale issue.
  //..still unsure of useful behavior ... neither seems good.. the tmp de-activate ikstretch seems
  // good? animator
  // will just have to manually reset scale as needed?..no.. becuse what if animator uses stretch
  // as location extension? we can't just disable that.. (though we can as default for
  // user-exposed-option) .. or maybe this is as simple as the ik-extension limit being the pose
  // length but it should be the base bone lenght instead? (really extension/stretch needs min/max
  // and space exposed too..)... and aproblem w/ using base bone length is that if user manually
  // scaled teh bone, then they amy not expect it to ever get smaller... yet it would in this case.
  copy_m3_m4(rmat3, chan_mat);
  // GG: Q: wrong order of mul?
  mul_m3_m3m3(smat, rmat3, imat3);
  mat3_to_size(pchan->size, smat);
  // float new_scale[3] = {stretch, stretch, stretch};
  // GG: Q: why did they invert rotation instead of just checking the y-axis scale
  // directly?
  // normalize_v3_length(pchan->size, stretch);
}

static short autoik_pinned_bones_apply_visual_pose(Object *ob, const bool do_snap_targets)
{
  /**
   * GG: BUG: autoik doesn't work on targeted IK that has UseTip=False since that
   *  excludes the constraint owner tip chan from the posetree chain, which means the
   *  pinned owner tipchan isn't within the posetree it creates and so its pose is never
   *  applied. I beleive pchan loc/rot syncing in recalc() also doens't account for UseTip=False..
   *
   * GG: TODO: I need to apply all bone visual pose (atleats those affected by xform) to avoid
   * snap on grab. Can call this func after all tmp IK's created, really can do so at end of grab()
   * func.
   *
   * GG: TODO: Need to cache bones which are affectd by all tmp IKs (can just use the resulting
   * posetrees of all tmp IK's and use their implciit/explicit pchan sets and merge together).
   * Then, per any-includingNontmp IK's, we check their posetree implicit/explicit set and if any
   * overlap with the tmp set, then we also apply their pose. Only have to do this once per
   * posetree, walking chain is only to find associated posetree, another reason to store posetree
   * info in the tip instead of chain root.. */
  int apply = 0;
  typedef struct IKData {
    struct IKData *prev, *next;

    bPoseChannel *owner_tip_chan;
    bPoseChannel *owner_root_chan;
    bKinematicConstraint *con_data;
  } IKData;

  /* now we got a difficult situation... we have to find the
   * target-less IK pchans, and apply transformation to the all
   * pchans that were in the chain */

  /* The proper way to insert keys is to apply visual xform to all IK chains that are directly
   * affected by the selected or pinned bones.
   */

  GSet *pinned_pbones = BLI_gset_ptr_new(__func__);
  for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    if ((pchan->ikflag_general & BONE_AUTOIK_DO_PIN) != 0 &&
        (pchan->ikflag_general & BONE_AUTOIK_DO_PIN_ANY) != 0)
    {
      BLI_gset_insert(pinned_pbones, pchan);
      continue;
    }

    if (!BKE_pose_is_layer_visible(ob->data, pchan)) {
      continue;
    }
    if ((pchan->bone->flag & BONE_SELECTED) == 0) {
      continue;
    }
    BLI_gset_insert(pinned_pbones, pchan);
  }

  GHash *pchans_from_posetree_pchan = NULL;
  GHash *solver_from_chain_root = BKE_determine_posetree_roots(&ob->pose->chanbase);
  {
    GHash *explicit_pchans_from_posetree_pchan;
    GHash *implicit_pchans_from_posetree_pchan;
    BKE_determine_posetree_pchan_implicity(&ob->pose->chanbase,
                                           solver_from_chain_root,
                                           &explicit_pchans_from_posetree_pchan,
                                           &implicit_pchans_from_posetree_pchan);

    pchans_from_posetree_pchan = BKE_union_pchans_from_posetree(
        explicit_pchans_from_posetree_pchan, implicit_pchans_from_posetree_pchan);

    BLI_ghash_free(explicit_pchans_from_posetree_pchan, NULL, BLI_gset_freefp_no_keyfree);
    BLI_ghash_free(implicit_pchans_from_posetree_pchan, NULL, BLI_gset_freefp_no_keyfree);
    explicit_pchans_from_posetree_pchan = NULL;
    implicit_pchans_from_posetree_pchan = NULL;
  }

  /* Apply pose matrix to bone local transforms. */
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, pchans_from_posetree_pchan) {
    GSet *all_pchans = BLI_ghashIterator_getValue(&gh_iter);

    bool any_pinned_bone_affects_posetree = false;
    {
      GSET_FOREACH_BEGIN (bPoseChannel *, pchan, pinned_pbones) {
        if (!BLI_gset_haskey(all_pchans, pchan)) {
          continue;
        }
        any_pinned_bone_affects_posetree = true;
        break;
      }
      GSET_FOREACH_END();
    }
    if (!any_pinned_bone_affects_posetree) {
      continue;
    }

    /* Apply pose matrix to bone local transforms. */
    // GG: TODO: rename parchan to pchan.. parent has nothing to do with anything..
    GSET_FOREACH_BEGIN (bPoseChannel *, parchan, all_pchans) {
      Bone *bone;

      /* `pose_mat(b) = pose_mat(b-1) * offs_bone * channel * constraint * IK` */
      /* We put in channel the entire result of: `mat = (channel * constraint * IK)` */
      /* `pose_mat(b) = pose_mat(b-1) * offs_bone * mat` */
      /* `mat = pose_mat(b) * inv(pose_mat(b-1) * offs_bone)` */

      bone = parchan->bone;
      bone->flag |= BONE_TRANSFORM; /* ensures it gets an auto key inserted */

      /* stretch causes problems with some constraints (so apply only if needed) */
      // GG: TODO: only write scale if any ik in posetree is using scale and
      // only for the bones that are stretched
      const bool is_posetree_stretch = true;
      pchan_apply_posemat_from_ik(parchan, parchan->pose_mat, is_posetree_stretch);
        }
    GSET_FOREACH_END();

    /** TODO: GG: need to tag */
    apply = 1;
    /** TODO: GG: Is this flag relevant? we cleanup our constraints anyways?*/
    // data->flag &= ~CONSTRAINT_IK_AUTO;

    if (!do_snap_targets) {
      continue;
    }

    ListBase ik_datas = {NULL, NULL};

    bPoseChannel *solver_chan = BLI_ghashIterator_getKey(&gh_iter);
    GSET_FOREACH_BEGIN (bPoseChannel *, pchan, all_pchans) {
      LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
        if ((con->flag & CONSTRAINT_DISABLE) != 0) {
          if ((con->flag & CONSTRAINT_TEMP_DISABLED_DURING_TRANSFORM) == 0) {
            continue;
        }
        }

        if (con->type != CONSTRAINT_TYPE_KINEMATIC) {
          continue;
        }

        bKinematicConstraint *data = (bKinematicConstraint *)con->data;
        if (data->flag & (CONSTRAINT_IK_AUTO | CONSTRAINT_IK_TEMP)) {
          continue;
      }
        // This flag set only for temp autoIK cons, which are skipped.
        /*if (data->flag & CONSTRAINT_IK_DO_NOT_CREATE_POSETREE) {
          continue;
        }*/
        if ((data->flag & CONSTRAINT_IK_POS) == 0 && (data->flag & CONSTRAINT_IK_ROT) == 0) {
          continue;
    }

        bPoseChannel *owner_rootchan = BKE_armature_ik_solver_find_root(pchan, data);
        // This removes the need to check for a non-NULL end effector.
        if (owner_rootchan == NULL) {
          /* Invalid data. */
          continue;
        }

        bPoseChannel *roots_solver_pchan = BLI_ghash_lookup(solver_from_chain_root,
                                                            owner_rootchan);
        if (solver_chan != roots_solver_pchan) {
          continue;
        }

        IKData *snapped_ik_data = MEM_mallocN(sizeof(IKData), "Snapped IK Data");
        snapped_ik_data->owner_root_chan = owner_rootchan;
        snapped_ik_data->owner_tip_chan = pchan;
        snapped_ik_data->con_data = data;
        BLI_addhead(&ik_datas, snapped_ik_data);
      }
    }
    GSET_FOREACH_END();

    LISTBASE_FOREACH (IKData *, ik_data, &ik_datas) {
      Object *owner_ob = ob;
      bKinematicConstraint *con_data = (bKinematicConstraint *)ik_data->con_data;

      // GG: TODO: add support for non-pchan target.
      if (con_data->tar->type != OB_ARMATURE) {
        continue;
  }

      float tobj_pose_from_world[4][4];
      copy_m4_m4(tobj_pose_from_world, con_data->tar->world_to_object);

      bPoseChannel *end_effector_chan = ik_data->owner_tip_chan;
      if ((con_data->flag & CONSTRAINT_IK_TIP) == 0) {
        end_effector_chan = end_effector_chan->parent;
      }
      BLI_assert(end_effector_chan);

      float end_effector_tarpose_matrix[4][4];
      copy_m4_m4(end_effector_tarpose_matrix, end_effector_chan->pose_mat);
      if ((con_data->flag & CONSTRAINT_IK_TIP_HEAD_AS_EE_POS) == 0) {
        float ee_pose_length = end_effector_chan->bone->length *
                               len_v3(end_effector_chan->pose_mat[1]);
        float length_vec[3] = {0, ee_pose_length, 0};
        mul_v3_m4v3(end_effector_tarpose_matrix[3], end_effector_tarpose_matrix, length_vec);
      }
      mul_m4_m4_pre(end_effector_tarpose_matrix, owner_ob->object_to_world);
      mul_m4_m4m4(end_effector_tarpose_matrix, tobj_pose_from_world, end_effector_tarpose_matrix);

      /* For twoway IK, we assume the target's position already properly placed due to iksolver. */
      const bool is_target_already_correct = (con_data->flag & CONSTRAINT_IK_IS_TWOWAY) != 0;
      if (!is_target_already_correct) {
        // TODO: properly support when target/pole is not a pchan;
        bPoseChannel *target_chan = BKE_pose_channel_find_name(con_data->tar->pose,
                                                               con_data->subtarget);
        BLI_assert(target_chan);

        float target_pose_mat[4][4];
        copy_m4_m4(target_pose_mat, target_chan->pose_mat);
        if (con_data->flag & CONSTRAINT_IK_POS) {
          copy_v3_v3(target_pose_mat[3], end_effector_tarpose_matrix[3]);
        }

        if (con_data->flag & CONSTRAINT_IK_ROT) {
          float target_loc[3], target_rot[3][3], target_scale[3];
          mat4_to_loc_rot_size(target_loc, target_rot, target_scale, target_pose_mat);

          float ee_rot[3][3];
          copy_m3_m4(ee_rot, end_effector_tarpose_matrix);
          loc_rot_size_to_mat4(target_pose_mat, target_loc, ee_rot, target_scale);
        }

        const bool do_scale = false;
        copy_m4_m4(target_chan->pose_mat, target_pose_mat);
        pchan_apply_posemat_from_ik(target_chan, target_pose_mat, do_scale);
      }

      if ((con_data->flag & CONSTRAINT_IK_POS) == 0) {
        continue;
      }
      if (con_data->poletar == NULL) {
        continue;
      }
      if (con_data->poletar->type != OB_ARMATURE) {
        continue;
      }
      if (con_data->polesubtarget[0] == 0) {
        continue;
      }

      bPoseChannel *pole_chan = BKE_pose_channel_find_name(con_data->poletar->pose,
                                                           con_data->polesubtarget);
      BLI_assert(pole_chan);

      if ((pole_chan->bone->flag & BONE_CONNECTED) != 0) {
        continue;
      }
      /* Project pole's pose location onto chain's pole plane, stored in pole_tarpose_mat */
      float pole_tarpose_mat[4][4];
      copy_m4_m4(pole_tarpose_mat, pole_chan->pose_mat);
      normalize_m4(pole_tarpose_mat);
      {
        /** Assumes that end effector tarpose location equal to target tarpose location.
         *
         * pole_matrix[0] direction in root matrice's XZ plane, origin at root, that points to pole
         *          location. AKA dir_from_pole_angle.
         *
         * pole_matrix[1] direction from root to end effector location. AKA
         * dir_root_to_end_effector.
         *
         * pole_matrix[2] cross product of other axes. We project the pole
         *          location along this axis onto the root's XZ plane which
         *          ensures the pose unaffected by the pole, preserving the pose.
         */

        /* Use the chain's effective root that has the pole constraint applied to it. */
        bPoseChannel *effective_root_chan = ik_data->owner_root_chan;
        while (effective_root_chan->parent != NULL &&
               BLI_gset_haskey(all_pchans, effective_root_chan->parent))
        {
          effective_root_chan = effective_root_chan->parent;
        }

        float root_tarpose_matrix[4][4];
        mul_m4_series(root_tarpose_matrix,
                      tobj_pose_from_world,
                      owner_ob->object_to_world,
                      effective_root_chan->pose_mat);

        float dir_root_to_end_effector[3];
        sub_v3_v3v3(
            dir_root_to_end_effector, end_effector_tarpose_matrix[3], root_tarpose_matrix[3]);

        float dir_from_pole_angle[3];
        zero_v3(dir_from_pole_angle);
        const float pole_angle = con_data->poleangle;
        madd_v3_v3fl(dir_from_pole_angle, root_tarpose_matrix[0], cos(pole_angle));
        madd_v3_v3fl(dir_from_pole_angle, root_tarpose_matrix[2], sin(pole_angle));

        float pole_plane_normal[3];
        cross_v3_v3v3(pole_plane_normal, dir_root_to_end_effector, dir_from_pole_angle);
        normalize_v3(pole_plane_normal);

        /* Orient pole to pole plane so its convenient for animator to move the
         * pole along it's XZ axis to distance pole from the chain without
         * affecting the pose. */
        copy_v3_v3(pole_tarpose_mat[0], dir_from_pole_angle);
        copy_v3_v3(pole_tarpose_mat[1], pole_plane_normal);
        copy_v3_v3(pole_tarpose_mat[2], dir_root_to_end_effector);

        cross_v3_v3v3(pole_tarpose_mat[0], pole_tarpose_mat[1], pole_tarpose_mat[2]);
        normalize_m4(pole_tarpose_mat);

        // Sign matters for pole location along dir_from_pole_angle axis.
        project_v3_plane(pole_tarpose_mat[3], pole_plane_normal, root_tarpose_matrix[3]);

        float pole_loc_offset_root[3];
        sub_v3_v3v3(pole_loc_offset_root, pole_tarpose_mat[3], root_tarpose_matrix[3]);
        float distance_along_pole_axis = dot_v3v3(pole_loc_offset_root, pole_tarpose_mat[0]);
        float abs_distance = fabsf(distance_along_pole_axis);
        madd_v3_v3fl(pole_loc_offset_root, pole_tarpose_mat[0], -distance_along_pole_axis);
        madd_v3_v3fl(pole_loc_offset_root, pole_tarpose_mat[0], abs_distance);
        add_v3_v3v3(pole_tarpose_mat[3], pole_loc_offset_root, root_tarpose_matrix[3]);
      }

      const bool do_scale_pole = false;
      pchan_apply_posemat_from_ik(pole_chan, pole_tarpose_mat, do_scale_pole);
    }

    BLI_freelistN(&ik_datas);
  }

  BLI_ghash_free(solver_from_chain_root, NULL, NULL);
  solver_from_chain_root = NULL;

  BLI_ghash_free(pchans_from_posetree_pchan, NULL, BLI_gset_freefp_no_keyfree);
  pchans_from_posetree_pchan = NULL;

  BLI_gset_free(pinned_pbones, NULL);
  pinned_pbones = NULL;

  return apply;
}

static void add_pose_transdata(TransInfo *t, bPoseChannel *pchan, Object *ob, TransData *td)
{
  Bone *bone = pchan->bone;
  float pmat[3][3], omat[3][3];
  float cmat[3][3], tmat[3][3];
  float vec[3];

  bArmature *arm = (bArmature*)ob->data;
  const bool do_custom_transform = (pchan->custom) && !(arm->flag & ARM_NO_CUSTOM) &&
                                   (pchan->custom_tx);
  if (do_custom_transform) {
    copy_v3_v3(td->center, pchan->custom_tx->pose_mat[3]);
  }
  else {
  copy_v3_v3(vec, pchan->pose_mat[3]);
  copy_v3_v3(td->center, vec);
  }

  td->ob = ob;
  td->flag = TD_SELECTED;
  if (bone->flag & BONE_HINGE_CHILD_TRANSFORM) {
    td->flag |= TD_NOCENTER;
  }

  /** GG: .. I don't know why this exists. It leads to inconsistent/unexpected transforms.
   *  I think it leads to "affeect Locations Only" to not work properly.
   * I assume this block was added so that non-connected bones don't locally translate
   * as you rotate w/ individual origins?.. no that shoiuldnt occur anyways since
   * we're using indiv. origins.. which makes this block redundant?
   */
  if (bone->flag & BONE_TRANSFORM_CHILD) {
    td->flag |= TD_NOCENTER;
    td->flag |= TD_NO_LOC;
  }

  td->extra = pchan;
  td->protectflag = pchan->protectflag;

  /** GG: NOTE: So td->loc is relative to bone, equivalent to fcurve data space. Which explains why
   * td->mtx stores world_from_bone (and stores its inverse), so that xform system can convert
   * td->loc back and forth between global space and the space of the data that's being trasnformed
   * (td->loc => pchan->loc) for proper writing to the data based on worlds-apce dat
   */
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
  copy_m3_m4(omat, ob->object_to_world);

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

  //print_m3("mtx: ", td->mtx);

  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  /* exceptional case: rotate the pose bone which also applies transformation
   * when a parentless bone has BONE_NO_LOCAL_LOCATION [] */
  if (!ELEM(t->mode, TFM_TRANSLATION, TFM_RESIZE) && (pchan->bone->flag & BONE_NO_LOCAL_LOCATION))
  {
    if (pchan->parent) {
      /* same as td->smtx but without pchan->bone->bone_mat */
      td->flag |= TD_PBONE_LOCAL_MTX_C;
      /* bone_mat: parent_from_bonerest
       *  mtx: world_from_parent (really parent's tail)
       * smtx: parent_from_world
       *
       * bone_mat * smtx = parent_from_bonerest * parent_from_world
       *
       * .. this doesn't make sense so maybe some other code is hardcoded to be aware of
       * parent_from_bonerest?
       */
      mul_m3_m3m3(td->ext->l_smtx, pchan->bone->bone_mat, td->smtx);
    }
    else {
      td->flag |= TD_PBONE_LOCAL_MTX_P;
    }
  }

  /* For `axismtx` we use bone's own transform. */
  copy_m3_m4(pmat, pchan->pose_mat);
  mul_m3_m3m3(td->axismtx, omat, pmat);
  normalize_m3(td->axismtx);

  if (t->orient_type_mask & (1 << V3D_ORIENT_GIMBAL)) {
    if (!gimbal_axis_pose(ob, pchan, td->ext->axismtx_gimbal)) {
      copy_m3_m3(td->ext->axismtx_gimbal, td->axismtx);
    }
  }

  if (t->mode == TFM_BONE_ENVELOPE_DIST) {
    td->loc = NULL;
    td->val = &bone->dist;
    td->ival = bone->dist;
  }
  else if (t->mode == TFM_BONESIZE) {
    /* Abusive storage of scale in the loc pointer :) */
    td->loc = &bone->xwidth;
    copy_v3_v3(td->iloc, td->loc);
    td->val = NULL;
  }

  // /* in this case we can do target-less IK grabbing */
  // bKinematicConstraint *data = has_targetless_ik(pchan);
  // if (data) {
  //   if (data->flag & CONSTRAINT_IK_TIP) {
  //     copy_v3_v3(data->grabtarget, pchan->pose_tail);
  //   }
  //   else {
  /** GG: NOTE: So autoIK w/ IK_TIP off used selected bone's head as the end effector position,
   * but non-temporary IK cons make the tip's parent's tail as teh end effector position.
   * (inconsistency)
   */
  //  copy_v3_v3(data->grabtarget, pchan->pose_head);
  //   }
  //   td->loc = data->grabtarget;
  //   copy_v3_v3(td->iloc, td->loc);

  //   // data->flag |= CONSTRAINT_IK_AUTO;

  //   /* Add a temporary auto IK constraint here, as we will only temporarily active this
  //    * targetless bone during transform. (Targetless IK constraints are treated as if they are
  //    * disabled unless they are transformed).
  //    * Only do this for targetless IK though, AutoIK already added a constraint in
  //    * pose_grab_with_ik_add() beforehand. */
  //   /** NOTE: GG: what was the point of delaying creation of this temp IK??
  //    * GG: commented the block out since I already create (WIP) all the Iks beforehand.
  //    */

  //   // if ((data->flag & CONSTRAINT_IK_TEMP) == 0) {
  //   //   add_temporary_ik_constraint(pchan, data);
  //   //   Main *bmain = CTX_data_main(t->context);
  //   //   update_deg_with_temporary_ik(bmain, ob);
  //   // }

  //   /* only object matrix correction */
  //   copy_m3_m3(td->mtx, omat);
  //   pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);
  // }

  /* store reference to first constraint */
  td->con = pchan->constraints.first;
}

static void createTransPose(bContext *UNUSED(C), TransInfo *t)
{
  Main *bmain = CTX_data_main(t->context);

  t->data_len_all = 0;

  bool has_translate_rotate_buf[2] = {false, false};
  bool *has_translate_rotate = (t->mode == TFM_TRANSLATION) ? has_translate_rotate_buf : NULL;

  /* Element: LinkData of IKGrabDatas */
  ListBase grabbed_data_per_tc = {NULL, NULL};
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Object *ob = tc->poseobj;
    bPose *pose = ob->pose;

    bArmature *arm;

    /* check validity of state */
    arm = BKE_armature_from_object(tc->poseobj);
    if ((arm == NULL) || (pose == NULL)) {
      continue;
    }
    // GG: TODO: Add support for mirroring during autoIK.
    const bool is_auto_ik = (pose->flag & POSE_AUTO_IK) != 0;
    const bool mirror = !is_auto_ik && ((pose->flag & POSE_MIRROR_EDIT) != 0);

    /* Set flags. */
    transform_convert_pose_transflags_update(ob, t->mode, t->around);

    /* Now count, and check if we have autoIK or have to switch from translate to rotate. */
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      Bone *bone = pchan->bone;
      if (!(bone->flag & BONE_TRANSFORM)) {
        continue;
      }

      tc->data_len++;

      if (has_translate_rotate != NULL) {
        if (has_translate_rotate[0] && has_translate_rotate[1]) {
          continue;
        }

        if (has_targetless_ik(pchan) == NULL) {
          if (pchan->parent && (bone->flag & BONE_CONNECTED)) {
            if (bone->flag & BONE_HINGE_CHILD_TRANSFORM) {
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

    IKGrabDatas *grabbed_datas = MEM_callocN(sizeof(IKGrabDatas), __func__);
    ListBase ensured_keyed_pchans = {NULL, NULL};
    if ((pose->flag & POSE_AUTO_IK)) {
      int total_grabbed_datas = 0;
      /** TODO: GG: always true? when doing POSE_AUTO_IK and a bone is selected*/
      if (pose_grab_with_ik_simpler(bmain, ob, grabbed_datas, &ensured_keyed_pchans)) {
        if (has_translate_rotate) {
          has_translate_rotate[0] = true;
        }

        t->flag |= T_AUTOIK;
        tc->data_len = grabbed_datas->total;

        if (tc->data_len > 0) {
          const bool do_snap_targets = (ob->pose->flag & POSE_AUTO_IK_SNAP_TARGET_ON_CONFIRM) != 0;
          autoik_pinned_bones_apply_visual_pose(ob, do_snap_targets);
        }
      }
    }
    BLI_addtail(&grabbed_data_per_tc, grabbed_datas);

    if (tc->data_len == 0) {
      BLI_freelistN(&ensured_keyed_pchans);
      continue;
    }

    /** TODO: GG: account for restpose and mirroring. I've been ignoring them during
     * implementaiton*/
    if (arm->flag & ARM_RESTPOS) {
      if (ELEM(t->mode, TFM_DUMMY, TFM_BONESIZE) == 0) {
        BKE_report(t->reports, RPT_ERROR, "Cannot change Pose when 'Rest Position' is enabled");
        tc->data_len = 0;

        BLI_freelistN(&ensured_keyed_pchans);
        continue;
      }
    }
    PoseData *pd;
    tc->custom.type.data = pd = MEM_callocN(sizeof(PoseData), "PoseData");
    tc->custom.type.use_free = false;
    tc->custom.type.free_cb = free_transcustomdata_posedata;

    pd->autoik = MEM_callocN(sizeof(PoseData_AutoIK) * tc->data_len, "PoseData_AutoIK");
    BLI_movelisttolist(&pd->ensured_keyed_pchans, &ensured_keyed_pchans);

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
      pd->mirror = pid;
    }
  }

  int tc_index = 0;
  FOREACH_TRANS_DATA_CONTAINER_INDEX(t, tc, tc_index)
  {
    /** TODO: GG: unnecessray linaer walk.. can fix later. */
    IKGrabDatas *grabbed_datas = BLI_findlink(&grabbed_data_per_tc, tc_index);

    if (tc->data_len == 0) {
      continue;
    }
    Object *ob = tc->poseobj;
    TransData *td;
    TransDataExtension *tdx;
    int i;

    PoseData *pd = tc->custom.type.data;
    PoseInitData_Mirror *pid = pd->mirror;
    int pid_index = 0;
    bPose *pose = ob->pose;

    if (pose == NULL) {
      continue;
    }

    // GG: TODO: Add support for mirroring during autoIK.
    const bool is_auto_ik = (pose->flag & POSE_AUTO_IK) != 0;
    const bool mirror = !is_auto_ik && ((pose->flag & POSE_MIRROR_EDIT) != 0);
    const bool is_mirror_relative = ((pose->flag & POSE_MIRROR_RELATIVE) != 0);

    tc->poseobj = ob; /* we also allow non-active objects to be transformed, in weightpaint */
    ob->pose->flag1 |= POSE1_IS_TRANSFORMING_PCHAN;

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

    /* use pose channels to fill trans data */
    td = tc->data;

    if (!is_auto_ik) {
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        if (pchan->bone->flag & BONE_TRANSFORM) {
          add_pose_transdata(t, pchan, ob, td);
          td++;
        }
      }
    }
    else {

      /* do we need to add temporal IK chains? */
      /* Always add temporary IK chains, even for non-translation transformations. We want full
       * control over transforming selected bones while constraining containing chains to be
       * autoIk-ed. */

      for (int grab_data_index = 0; grab_data_index < grabbed_datas->total; grab_data_index++) {
        IKGrabData *grab_data = grabbed_datas->buffer + grab_data_index;
        PoseData_AutoIK *ik_sync_data = pd->autoik + grab_data_index;

        if (grab_data->synced_ik_data != NULL) {
          ik_sync_data->pchan = grab_data->pchan;
          ik_sync_data->synced_ik_data = grab_data->synced_ik_data;
          ik_sync_data->sync_mode = grab_data->sync_mode;
          copy_v3_v3(ik_sync_data->initial_loc, ik_sync_data->pchan->loc);
          copy_v3_v3(ik_sync_data->initial_eul, ik_sync_data->pchan->eul);
          copy_v4_v4(ik_sync_data->initial_quat, ik_sync_data->pchan->quat);
          copy_v3_v3(ik_sync_data->initial_rotAxis, ik_sync_data->pchan->rotAxis);
          copy_v3_v3(ik_sync_data->initial_scale, ik_sync_data->pchan->size);
          ik_sync_data->initial_rotAngle = ik_sync_data->pchan->rotAngle;
          // Don't use pose length. We use base length so that scale can return to unit after
          // rotating while using ik stretch... though.. i suppsoe if a bone is plain scaled and
          // ikstretch is off, then you would want to use the pose length... no that cant be it..
          // because posing is fine without ever ..ah because there is the autoik added goals. So
          // it does seem like we might have to apply scale in order to preserve the pose?
          //ik_sync_data->initial_length = ik_sync_data->pchan->bone->length;
          ik_sync_data->initial_length = len_v3v3(ik_sync_data->pchan->pose_head,
                                                  ik_sync_data->pchan->pose_tail);
        }

        /** TODO: GG: XXX: tc->data_len logic breaks when an existing nonauto IK chain exists since
         * we'd split that and create ea temp IK, thus its not just 2 td per selected bone.
         */
        // if ((pchan->bone->flag & BONE_TRANSFORM) == 0) {
        //   continue;
        // }
        bPoseChannel *pchan = grab_data->pchan;
        add_pose_transdata(t, pchan, ob, td);
        if ((grab_data->flag & IKGRAB_FLAG_REDIRECT_TD_LOC) == 0) {
          td++;
          continue;
        }

        // const float length_from_head = len_v3v3(grab_data->td_center, pchan->pose_head);
        // const bool grabbing_head = IS_EQF(length_from_head, 0.0f);

        // td->flag &= ~TD_NO_LOC;
        // td->flag &= ~TD_NOCENTER;

        // if (!grabbing_head) {
        //   // td++;
        //   // td->flag |= TD_NOCENTER;
        //   // continue;
        //   // td->flag |= TD_NO_LOC;
        //   td->flag |= TD_NOCENTER;
        //   copy_v3_v3(td->center, pchan->pose_head);
        // }
        // else {
        // }
        // td->flag &= ~TD_SELECTED;

        // Allow transdata to freely translate and rotate w/o restriction. 
        // IKSolver will satisfy the pchan's locks.

        const bool do_defer_loc_xform_to_ik = (ik_sync_data->synced_ik_data->autoik_flag &
                                               (CONSTRAINT_AUTOIK_USE_HEAD |
                                                CONSTRAINT_AUTOIK_USE_TAIL)) != 0;
        if (do_defer_loc_xform_to_ik) {
          // IKsolver will attempt to satisfy the location, so td must be allowed to translate.
          // By not clearing the flag when not deferred, the animator can still translate the 
          // bone directly.
          td->protectflag &= ~OB_LOCK_LOC;
          
          td->loc = grab_data->td_loc;
          copy_v3_v3(td->iloc, td->loc);

          /* only object matrix correction */
          copy_m3_m4(td->mtx, ob->object_to_world);
          pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);
        }

        copy_v3_v3(td->center, grab_data->td_center);
        const bool do_defer_rot_xform_to_ik = (ik_sync_data->synced_ik_data->autoik_flag &
                                               CONSTRAINT_AUTOIK_USE_ROTATION) != 0;
        if (do_defer_rot_xform_to_ik){
          // IKsolver will attempt to satisfy the rotation, so td must be allowed to rotate.
          // By not clearing the flag when not deferred, the animator can still rotate the 
          // bone directly.
          td->protectflag &= ~(OB_LOCK_ROT | OB_LOCK_ROTW | OB_LOCK_ROT4D);

          if (pchan->rotmode > 0) {
            td->ext->rot = ik_sync_data->eul;
            td->ext->rotAxis = NULL;
            td->ext->rotAngle = NULL;
            td->ext->quat = NULL;

            copy_v3_v3(ik_sync_data->eul, pchan->eul);
            copy_v3_v3(td->ext->irot, ik_sync_data->eul);
          }
          else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
            td->ext->rot = NULL;
            td->ext->rotAxis = ik_sync_data->rotAxis;
            td->ext->rotAngle = &ik_sync_data->rotAngle;
            td->ext->quat = NULL;

            copy_v3_v3(ik_sync_data->rotAxis, pchan->rotAxis);
            ik_sync_data->rotAngle = pchan->rotAngle;

            td->ext->irotAngle = ik_sync_data->rotAngle;
            copy_v3_v3(td->ext->irotAxis, ik_sync_data->rotAxis);
          }
          else {
            td->ext->rot = NULL;
            td->ext->rotAxis = NULL;
            td->ext->rotAngle = NULL;
            td->ext->quat = ik_sync_data->quat;

            copy_qt_qt(ik_sync_data->quat, pchan->quat);
            copy_qt_qt(td->ext->iquat, ik_sync_data->quat);
          }
        }
        // /* For nonselected bones, don't let them affect the transform op's center. */
        // if ((pchan->bone->flag & BONE_TRANSFORM) == 0) {
        //   // copy_v3_v3(td->center, pchan->pose_tail);
        //   td->flag |= TD_NOCENTER;
        //   //  td->flag |= TD_NO_LOC;
        // }
        // else {
        //   // copy_v3_v3(td->center, pchan->pose_head);
        //   // td->flag &= ~TD_SELECTED;
        //   // td->flag |= TD_NOCENTER;
        //   // td->flag |= TD_NO_LOC;
        // }
        td++;
        /** TODO: GG: BUG: when parent chain grabbed and child chjain grabbed, the child doesn't
         * move 1:1 iwth mouse (doublt xform?).. its probably why the support wasn't theere in the
         * firset place...*/
      }
    }

    if (td != (tc->data + tc->data_len)) {
      BKE_report(t->reports, RPT_DEBUG, "Bone selection count error");
    }
  }

  LISTBASE_FOREACH (IKGrabDatas *, grab_datas, &grabbed_data_per_tc) {
    if (grab_datas->buffer) {
      MEM_freeN(grab_datas->buffer);
    }
  }
  BLI_freelistN(&grabbed_data_per_tc);

  /* Initialize initial auto=IK chain-length's? */
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
}

static void createTransArmatureVerts(bContext *UNUSED(C), TransInfo *t)
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

    copy_m3_m4(mtx, tc->obedit->object_to_world);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransEditBone");
    int i = 0;

    for (ebo = edbo->first; ebo; ebo = ebo->next) {
      td_old = td;

      /* (length == 0.0) on extrude, used for scaling radius of bone points. */
      ebo->oldlength = ebo->length;

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
              /* Abusive storage of scale in the loc pointer :). */
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
             * causes problem with snapping (see #45974).
             * However, in rotation mode, we want to keep that 'rotate bone around root with
             * only its tip selected' behavior (see #46325). */
            if ((t->around == V3D_AROUND_LOCAL_ORIGINS) &&
                ((t->mode == TFM_ROTATION) || (ebo->flag & BONE_ROOTSEL)))
            {
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

static void recalcData_edit_armature(TransInfo *t)
{
  if (t->state != TRANS_CANCEL) {
    transform_snap_project_individual_apply(t);
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

            /* roll has a tendency to flip in certain orientations - #34283, #33974. */
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

  PoseData *pd = tc->custom.type.data;
  PoseInitData_Mirror *pid = NULL;
  if ((t->mode != TFM_BONESIZE) && (pose->flag & POSE_MIRROR_RELATIVE)) {
    pid = pd->mirror;
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
    pchan->roll1 = pchan_orig->roll1 * -1; /* XXX? */
    pchan->roll2 = pchan_orig->roll2 * -1; /* XXX? */

    float pchan_mtx_final[4][4];
    BKE_pchan_to_mat4(pchan_orig, pchan_mtx_final);
    mul_m4_m4m4(pchan_mtx_final, pchan_mtx_final, flip_mtx);
    mul_m4_m4m4(pchan_mtx_final, flip_mtx, pchan_mtx_final);
    if (pid) {
      mul_m4_m4m4(pchan_mtx_final, pid->offset_mtx, pchan_mtx_final);
    }
    BKE_pchan_apply_mat4(pchan, pchan_mtx_final, false);

    /* Set flag to let auto key-frame know to key-frame the mirrored bone. */
    pchan->bone->flag |= BONE_TRANSFORM_MIRROR;

    /* In this case we can do target-less IK grabbing. */
    bKinematicConstraint *data = has_targetless_ik(pchan);
    if (data == NULL) {
      continue;
    }
    mul_v3_m4v3(data->grabtarget, flip_mtx, td->loc);
    if (pid) {
      /* TODO(@germano): Relative Mirror support. */
    }
    data->flag |= CONSTRAINT_IK_AUTO;
    /* Add a temporary auto IK constraint here, as we will only temporarily active this
     * target-less bone during transform. (Target-less IK constraints are treated as if they are
     * disabled unless they are transformed).
     * Only do this for targetless IK though, AutoIK already added a constraint in
     * pose_grab_with_ik_add() beforehand. */
    if ((data->flag & CONSTRAINT_IK_TEMP) == 0) {
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

  const bool is_auto_ik = (pose->flag & POSE_AUTO_IK) != 0;
  const bool mirror = !is_auto_ik && ((pose->flag & POSE_MIRROR_EDIT) != 0);
  if (!mirror) {
    return;
  }

  PoseData *pd = tc->custom.type.data;
  for (PoseInitData_Mirror *pid = pd->mirror; pid->pchan; pid++) {
    pose_mirror_info_restore(pid);
  }
}

static void recalcData_pose(TransInfo *t)
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
        const bool is_auto_ik = (pose->flag & POSE_AUTO_IK) != 0;
        const bool mirror_pose = !is_auto_ik && ((pose->flag & POSE_MIRROR_EDIT) != 0);
        if (arm->flag & ARM_MIRROR_EDIT || mirror_pose) {
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

      const bool is_auto_ik = (pose->flag & POSE_AUTO_IK) != 0;
      const bool mirror = !is_auto_ik && ((pose->flag & POSE_MIRROR_EDIT) != 0);
      if (mirror) {
        if (t->state != TRANS_CANCEL) {
          pose_transform_mirror_update(t, tc, ob);
        }
        else {
          restoreMirrorPoseBones(tc);
        }
      }

      if (t->state != TRANS_CANCEL) {
        PoseData *pd = tc->custom.type.data;
        TransData *td = tc->data;
        float td_matrix[4][4];
        float pchan_mat4[4][4];
        float pchan_rot[3][3];
        float pose_from_world[4][4];
        float pose_from_world_rotscale[3][3];
        float pchan_size[3];
        copy_v3_fl(pchan_size, 1.0f);

        copy_m4_m4(pose_from_world, ob->object_to_world);
        invert_m4(pose_from_world);
        copy_m3_m4(pose_from_world_rotscale, pose_from_world);

        for (int i = 0; i < tc->data_len; td++, i++) {
          // continue;
          PoseData_AutoIK *pd_ik_data = pd->autoik + i;

          if (pd_ik_data->synced_ik_data == NULL) {
            continue;
          }
          bKinematicConstraint *ik_data = pd_ik_data->synced_ik_data;

          bPoseChannel *pchan = pd_ik_data->pchan;

          // BKE_pchan_rot_to_mat3(pchan, pchan_rot);  // GG: XXX: unused copy?
          // loc_rot_size_to_mat4(td_matrix, td->loc, pchan_rot, pchan->size);
          // unit_m3(pchan_rot);

          // quat_to_mat3(pchan_rot, td->ext->quat);
          /* rotations may either be quats, eulers (with various rotation orders), or axis-angle */
          if (pchan->rotmode > 0) {
            /* Euler rotations (will cause gimbal lock,
            * but this can be alleviated a bit with rotation orders) */
            eulO_to_mat3(pchan_rot, td->ext->rot, pchan->rotmode);
          }
          else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
            /* axis-angle - not really that great for 3D-changing orientations */
            axis_angle_to_mat3(pchan_rot, td->ext->rotAxis, *td->ext->rotAngle);
          }
          else {
            /* quats are normalized before use to eliminate scaling issues */
            float quat[4];

            /* NOTE: we now don't normalize the stored values anymore,
            * since this was kind of evil in some cases but if this proves to be too problematic,
            * switch back to the old system of operating directly on the stored copy. */
            normalize_qt_qt(quat, td->ext->quat);
            quat_to_mat3(pchan_rot, quat);
          }
          
          // copy_m3_m4(pchan_rot, pchan_mat4);
          float td_matrix_rotscale[3][3];
          mul_m3_series(td_matrix_rotscale, pose_from_world_rotscale, td->ext->r_mtx, pchan_rot);
          copy_m4_m3(td_matrix, td_matrix_rotscale);

          //// Commented out since it doesn't work when bone is disconnected from parent? There's a
          //// discontinuity/teleport when initially autoIK grabbing the bone..
          ////  td->loc == pchan->loc
          ////  td->center == posespace of pchan's atrest head location relative to animated parent
          // float loc_posespace[3];
          // {
          //   /* Although td->loc has globalspace deltas applied to it, so we could have
          //   initialized
          //    * td->loc to pchan->pose_head then loc_posespace = td->loc and avoided the math.
          //    I'm
          //    * assuming the transform system applies deltas in global space, which makes sense
          //    * since the local space really doesn't matter. Even axis constraints (i.e. local
          //    * space) is applied in global space as a delta.
          //    *
          //    * I've chosen to do the proper(?) math in case my assumptions are wrong or have an
          //    * edge case. */
          //   copy_v3_v3(loc_posespace, td->loc);
          //   mul_m3_v3(td->mtx, loc_posespace);
          //   /* td->center is initialized to the resting posepace position of pchan. td->mtx is a
          //    * 3x3 that represents the starting pose_from_animated_pchan so it doesn't include
          //    the
          //    * starting posespace position of pchan. */
          //   add_v3_v3(loc_posespace, td->center);
          //   mul_m4_v3(pose_from_world, loc_posespace);
          // }
          // if (pd_ik_data->sync_mode & IKGRAB_MODE_SYNC_ROTATION) {
          if (ik_data->autoik_flag & CONSTRAINT_AUTOIK_USE_ROTATION) {
            copy_m3_m3(ik_data->rotation_target, td_matrix_rotscale);
          }

          /* Sync ik_data->grabtarget with either pchan's transformed head or tail position. */
          if (ik_data->autoik_flag & CONSTRAINT_AUTOIK_USE_HEAD) {
            copy_v3_v3(ik_data->grabtarget, td->loc);
          }
          if (ik_data->autoik_flag & CONSTRAINT_AUTOIK_USE_TAIL) {
            /* Condition synced within pose_grab_with_ik_simpler(). */
            if ((ik_data->autoik_flag & CONSTRAINT_AUTOIK_USE_HEAD) == 0) {
              copy_v3_v3(ik_data->autoik_target_tail, td->loc);
            }
            else {
              copy_v3_v3(ik_data->autoik_target_tail, td->loc);
              float tail_location[3] = {0, pd_ik_data->initial_length, 0};
              mul_m4_v3(td_matrix, tail_location);
              add_v3_v3(tail_location, td->loc);
              copy_v3_v3(ik_data->autoik_target_tail, tail_location);
            }
          }
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

        /* XXX: this currently doesn't work, since flags aren't set yet! */
        int targetless_ik = (t->flag & T_AUTOIK);

        animrecord_check_state(t, &ob->id);
        autokeyframe_pose(t->context, t->scene, ob, t->mode, targetless_ik);
      }

      if (motionpath_need_update_pose(t->scene, ob)) {
        BLI_gset_insert(motionpath_updates, ob);
      }

      /** NOTE: GG: So this flushed updates to the rest of teh hierarachy and even visually shows
       * changes to selected bones.
       */
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
    else if ((bone->flag & BONE_TRANSFORM) && ELEM(mode, TFM_ROTATION, TFM_TRACKBALL) &&
             (around == V3D_AROUND_LOCAL_ORIGINS))
    {
      bone->flag |= BONE_TRANSFORM_CHILD;
    }
    else {
      bone->flag &= ~BONE_TRANSFORM;
    }

    bone_children_clear_transflag(mode, around, &bone->childbase);
  }
}

void transform_convert_pose_transflags_update(Object *ob, const int mode, const short around)
{
  bArmature *arm = ob->data;
  bPoseChannel *pchan;
  Bone *bone;

  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    bone = pchan->bone;
    if (PBONE_VISIBLE(arm, bone)) {
      if (bone->flag & BONE_SELECTED) {
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
    pchan->ikflag_location &= ~(BONE_IK_NO_XDOF_TEMP | BONE_IK_NO_YDOF_TEMP |
                                BONE_IK_NO_ZDOF_TEMP);
    pchan->ikflag_stretch &= ~(BONE_IK_NO_XDOF_TEMP | BONE_IK_NO_YDOF_TEMP | BONE_IK_NO_ZDOF_TEMP);
    pchan->constflag &= ~(PCHAN_HAS_IK | PCHAN_HAS_TARGET);

    /* remove all temporary IK-constraints added */
    for (con = pchan->constraints.first; con; con = next) {
      next = con->next;
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        data = con->data;

        if (con->flag & CONSTRAINT_TEMP_DISABLED_DURING_TRANSFORM) {
          con->flag &= ~(CONSTRAINT_DISABLE | CONSTRAINT_TEMP_DISABLED_DURING_TRANSFORM);
        }

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

static void special_aftertrans_update__pose(bContext *C, TransInfo *t)
{
  /** NOTE: GG: If multiple bones selected, then the child selected bone is likely to twist
   * undesirable during rotation xform. This is due to the child inheriting rotation effects. It is
   * fixed by disabling inehrit rotation but that's not practical from user standpoint. We can
   * temporarily disable inherit rotation... but that means locked rotation properties are not
   * respected.
   *
   *
    GG: TODO: still have to make this work for mirroring (or I could just leave as TODO.. but
   should still ensure it doesn't break when attempted..)
  */
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
      ob->pose->flag1 &= ~POSE1_IS_TRANSFORMING_PCHAN;

      if ((t->flag & T_AUTOIK) && (t->options & CTX_AUTOCONFIRM)) {
        /* when running transform non-interactively (operator exec),
         * we need to update the pose otherwise no updates get called during
         * transform and the auto-ik is not applied. see #26164. */
        struct Object *pose_ob = tc->poseobj;
        BKE_pose_where_is(t->depsgraph, t->scene, pose_ob);
      }

      /* Set BONE_TRANSFORM flags for auto-key, gizmo draw might have changed them. */
      if (!canceled && (t->mode != TFM_DUMMY)) {
        transform_convert_pose_transflags_update(ob, t->mode, t->around);
      }

      PoseData *pd = tc->custom.type.data;
      {
        LISTBASE_FOREACH (LinkData *, ld_pchan, &pd->ensured_keyed_pchans) {
          bPoseChannel *pchan = ld_pchan->data;
          pchan->bone->flag |= BONE_TRANSFORM;
        }
      }

      /* if target-less IK grabbing, we calculate the pchan transforms and clear flag */
      if (!canceled && (t->flag & T_AUTOIK) != 0) {
        const bool do_snap_targets = (ob->pose->flag & POSE_AUTO_IK_SNAP_TARGET_ON_CONFIRM) != 0;
        targetless_ik = autoik_pinned_bones_apply_visual_pose(ob, do_snap_targets);
      }
      else if (canceled) {
        /* not forget to clear the auto flag */
        for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
          bKinematicConstraint *data = has_targetless_ik(pchan);
          if (data) {
            data->flag &= ~CONSTRAINT_IK_AUTO;
          }
        }
        for (int i = 0; i < tc->data_len; i++) {
          PoseData_AutoIK *ik_sync_data = pd->autoik + i;
          bPoseChannel *pchan = ik_sync_data->pchan;
          if (pchan == NULL) {
            continue;
          }

          copy_v3_v3(pchan->loc, ik_sync_data->initial_loc);
          copy_v3_v3(pchan->eul, ik_sync_data->initial_eul);
          copy_v4_v4(pchan->quat, ik_sync_data->initial_quat);
          copy_v3_v3(pchan->rotAxis, ik_sync_data->initial_rotAxis);
          pchan->rotAngle = ik_sync_data->initial_rotAngle;
          copy_v3_v3(pchan->size, ik_sync_data->initial_scale);
        }
      }

      struct Main *bmain = CTX_data_main(t->context);
      pose_grab_with_ik_clear(bmain, ob);

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

TransConvertTypeInfo TransConvertType_EditArmature = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*createTransData*/ createTransArmatureVerts,
    /*recalcData*/ recalcData_edit_armature,
    /*special_aftertrans_update*/ NULL,
};

TransConvertTypeInfo TransConvertType_Pose = {
    /*flags*/ 0,
    /*createTransData*/ createTransPose,
    /*recalcData*/ recalcData_pose,
    /*special_aftertrans_update*/ special_aftertrans_update__pose,
};
