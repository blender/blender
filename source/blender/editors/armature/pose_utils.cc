/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_idprop.hh"
#include "BKE_layer.hh"
#include "BKE_object.hh"

#include "BKE_context.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_keyframing.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_keyingsets.hh"

#include "armature_intern.hh"

/* *********************************************** */
/* Contents of this File:
 *
 * This file contains methods shared between Pose Slide and Pose Lib;
 * primarily the functions in question concern Animato <-> Pose
 * convenience functions, such as applying/getting pose values
 * and/or inserting keyframes for these.
 */
/* *********************************************** */
/* FCurves <-> PoseChannels Links */

/**
 * Types of transforms applied to the given item:
 * - these are the return flags for get_item_transform_flags()
 */
enum eAction_TransformFlags {
  ACT_TRANS_LOC = (1 << 0),
  ACT_TRANS_ROT = (1 << 1),
  ACT_TRANS_SCALE = (1 << 2),

  /* BBone shape - for all the parameters, provided one is set. */
  ACT_TRANS_BBONE = (1 << 3),
  ACT_TRANS_PROP = (1 << 4),

  ACT_TRANS_ONLY = (ACT_TRANS_LOC | ACT_TRANS_ROT | ACT_TRANS_SCALE),
  ACT_TRANS_ALL = (ACT_TRANS_ONLY | ACT_TRANS_PROP),
};

static eAction_TransformFlags get_item_transform_flags_and_fcurves(Object &ob,
                                                                   bPoseChannel &pchan,
                                                                   ListBase &r_curves)
{
  if (!ob.adt || !ob.adt->action) {
    return eAction_TransformFlags(0);
  }
  blender::animrig::Action &action = ob.adt->action->wrap();

  short flags = 0;

  /* Build PointerRNA from provided data to obtain the paths to use. */
  PointerRNA ptr = RNA_pointer_create_discrete(reinterpret_cast<ID *>(&ob), &RNA_PoseBone, &pchan);

  /* Get the basic path to the properties of interest. */
  const std::optional<std::string> basePath = RNA_path_from_ID_to_struct(&ptr);
  if (!basePath) {
    return eAction_TransformFlags(0);
  }

  /* Search F-Curves for the given properties
   * - we cannot use the groups, since they may not be grouped in that way...
   */
  blender::animrig::foreach_fcurve_in_action_slot(
      action, ob.adt->slot_handle, [&](FCurve &fcurve) {
        const char *bPtr = nullptr, *pPtr = nullptr;

        if (fcurve.rna_path == nullptr) {
          return;
        }

        /* Step 1: check for matching base path */
        bPtr = strstr(fcurve.rna_path, basePath->c_str());

        if (!bPtr) {
          return;
        }

        /* We must add `len(basePath)` bytes to the match so that we are at the end of the
         * base path so that we don't get false positives with these strings in the names
         */
        bPtr += strlen(basePath->c_str());

        /* Step 2: check for some property with transforms
         * - once a match has been found, the curve cannot possibly be any other one
         */
        pPtr = strstr(bPtr, "location");
        if (pPtr) {
          flags |= ACT_TRANS_LOC;

          BLI_addtail(&r_curves, BLI_genericNodeN(&fcurve));
          return;
        }

        pPtr = strstr(bPtr, "scale");
        if (pPtr) {
          flags |= ACT_TRANS_SCALE;

          BLI_addtail(&r_curves, BLI_genericNodeN(&fcurve));
          return;
        }

        pPtr = strstr(bPtr, "rotation");
        if (pPtr) {
          flags |= ACT_TRANS_ROT;

          BLI_addtail(&r_curves, BLI_genericNodeN(&fcurve));
          return;
        }

        pPtr = strstr(bPtr, "bbone_");
        if (pPtr) {
          flags |= ACT_TRANS_BBONE;

          BLI_addtail(&r_curves, BLI_genericNodeN(&fcurve));
          return;
        }

        /* Custom properties only. */
        pPtr = strstr(bPtr, "[\"");
        if (pPtr) {
          flags |= ACT_TRANS_PROP;

          BLI_addtail(&r_curves, BLI_genericNodeN(&fcurve));
          return;
        }
      });

  /* return flags found */
  return eAction_TransformFlags(flags);
}

/* helper for poseAnim_mapping_get() -> get the relevant F-Curves per PoseChannel */
static void fcurves_to_pchan_links_get(ListBase &pfLinks, Object &ob, bPoseChannel &pchan)
{
  ListBase curves = {nullptr, nullptr};
  const eAction_TransformFlags transFlags = get_item_transform_flags_and_fcurves(
      ob, pchan, curves);

  pchan.flag &= ~(POSE_LOC | POSE_ROT | POSE_SCALE | POSE_BBONE_SHAPE);

  if (!transFlags) {
    return;
  }

  tPChanFCurveLink *pfl = MEM_callocN<tPChanFCurveLink>("tPChanFCurveLink");

  pfl->ob = &ob;
  pfl->fcurves = curves;
  pfl->pchan = &pchan;

  /* Get the RNA path to this pchan - this needs to be freed! */
  PointerRNA ptr = RNA_pointer_create_discrete(reinterpret_cast<ID *>(&ob), &RNA_PoseBone, &pchan);
  pfl->pchan_path = BLI_strdup(RNA_path_from_ID_to_struct(&ptr).value_or("").c_str());

  BLI_addtail(&pfLinks, pfl);

  /* Set pchan's transform flags. */
  if (transFlags & ACT_TRANS_LOC) {
    pchan.flag |= POSE_LOC;
  }
  if (transFlags & ACT_TRANS_ROT) {
    pchan.flag |= POSE_ROT;
  }
  if (transFlags & ACT_TRANS_SCALE) {
    pchan.flag |= POSE_SCALE;
  }
  if (transFlags & ACT_TRANS_BBONE) {
    pchan.flag |= POSE_BBONE_SHAPE;
  }

  copy_v3_v3(pfl->oldloc, pchan.loc);
  copy_v3_v3(pfl->oldrot, pchan.eul);
  copy_v3_v3(pfl->oldscale, pchan.scale);
  copy_qt_qt(pfl->oldquat, pchan.quat);
  copy_v3_v3(pfl->oldaxis, pchan.rotAxis);
  pfl->oldangle = pchan.rotAngle;

  /* Store current bbone values. */
  pfl->roll1 = pchan.roll1;
  pfl->roll2 = pchan.roll2;
  pfl->curve_in_x = pchan.curve_in_x;
  pfl->curve_in_z = pchan.curve_in_z;
  pfl->curve_out_x = pchan.curve_out_x;
  pfl->curve_out_z = pchan.curve_out_z;
  pfl->ease1 = pchan.ease1;
  pfl->ease2 = pchan.ease2;

  copy_v3_v3(pfl->scale_in, pchan.scale_in);
  copy_v3_v3(pfl->scale_out, pchan.scale_out);

  /* Make copy of custom properties. */
  if (transFlags & ACT_TRANS_PROP) {
    if (pchan.prop) {
      pfl->oldprops = IDP_CopyProperty(pchan.prop);
    }
    if (pchan.system_properties) {
      pfl->old_system_properties = IDP_CopyProperty(pchan.system_properties);
    }
  }
}

Object *poseAnim_object_get(Object *ob_)
{
  Object *ob = BKE_object_pose_armature_get(ob_);
  if (!ELEM(nullptr, ob, ob->data, ob->adt, ob->adt->action)) {
    return ob;
  }
  return nullptr;
}

void poseAnim_mapping_get(bContext *C, ListBase *pfLinks)
{
  BLI_assert(pfLinks != nullptr);
  /* For each Pose-Channel which gets affected, get the F-Curves for that channel
   * and set the relevant transform flags...
   */
  Object *prev_ob, *ob_pose_armature;

  prev_ob = nullptr;
  ob_pose_armature = nullptr;
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, selected_pose_bones, Object *, ob) {
    BLI_assert(pchan != nullptr);
    if (ob != prev_ob) {
      prev_ob = ob;
      ob_pose_armature = poseAnim_object_get(ob);
    }

    if (ob_pose_armature == nullptr) {
      continue;
    }
    if (!ob_pose_armature->adt || !ob_pose_armature->adt->action) {
      /* No action means no FCurves. */
      continue;
    }

    fcurves_to_pchan_links_get(*pfLinks, *ob_pose_armature, *pchan);
  }
  CTX_DATA_END;

  /* If no PoseChannels were found, try a second pass, doing visible ones instead.
   * i.e. if nothing selected, do whole pose.
   */
  if (BLI_listbase_is_empty(pfLinks)) {
    prev_ob = nullptr;
    ob_pose_armature = nullptr;
    CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object *, ob) {
      BLI_assert(pchan != nullptr);
      if (ob != prev_ob) {
        prev_ob = ob;
        ob_pose_armature = poseAnim_object_get(ob);
      }

      if (ob_pose_armature == nullptr) {
        continue;
      }
      if (!ob_pose_armature->adt || !ob_pose_armature->adt->action) {
        /* No action means no FCurves. */
        continue;
      }

      fcurves_to_pchan_links_get(*pfLinks, *ob_pose_armature, *pchan);
    }
    CTX_DATA_END;
  }
}

void poseAnim_mapping_free(ListBase *pfLinks)
{
  tPChanFCurveLink *pfl, *pfln = nullptr;

  /* free the temp pchan links and their data */
  for (pfl = static_cast<tPChanFCurveLink *>(pfLinks->first); pfl; pfl = pfln) {
    pfln = pfl->next;

    /* free custom properties */
    if (pfl->oldprops) {
      IDP_FreeProperty(pfl->oldprops);
    }

    /* free list of F-Curve reference links */
    BLI_freelistN(&pfl->fcurves);

    /* free pchan RNA Path */
    MEM_freeN(pfl->pchan_path);

    /* free link itself */
    BLI_freelinkN(pfLinks, pfl);
  }
}

/* ------------------------- */

void poseAnim_mapping_refresh(bContext *C, Scene * /*scene*/, Object *ob)
{
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

  AnimData *adt = BKE_animdata_from_id(&ob->id);
  if (adt && adt->action) {
    DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
  }
}

void poseAnim_mapping_reset(ListBase *pfLinks)
{
  /* iterate over each pose-channel affected, restoring all channels to their original values */
  LISTBASE_FOREACH (tPChanFCurveLink *, pfl, pfLinks) {
    bPoseChannel *pchan = pfl->pchan;

    /* just copy all the values over regardless of whether they changed or not */
    copy_v3_v3(pchan->loc, pfl->oldloc);
    copy_v3_v3(pchan->eul, pfl->oldrot);
    copy_v3_v3(pchan->scale, pfl->oldscale);
    copy_qt_qt(pchan->quat, pfl->oldquat);
    copy_v3_v3(pchan->rotAxis, pfl->oldaxis);
    pchan->rotAngle = pfl->oldangle;

    /* store current bbone values */
    pchan->roll1 = pfl->roll1;
    pchan->roll2 = pfl->roll2;
    pchan->curve_in_x = pfl->curve_in_x;
    pchan->curve_in_z = pfl->curve_in_z;
    pchan->curve_out_x = pfl->curve_out_x;
    pchan->curve_out_z = pfl->curve_out_z;
    pchan->ease1 = pfl->ease1;
    pchan->ease2 = pfl->ease2;

    copy_v3_v3(pchan->scale_in, pfl->scale_in);
    copy_v3_v3(pchan->scale_out, pfl->scale_out);

    /* just overwrite values of properties from the stored copies (there should be some) */
    if (pfl->oldprops) {
      IDP_SyncGroupValues(pfl->pchan->prop, pfl->oldprops);
    }
    if (pfl->old_system_properties) {
      IDP_SyncGroupValues(pfl->pchan->system_properties, pfl->old_system_properties);
    }
  }
}

void poseAnim_mapping_autoKeyframe(bContext *C, Scene *scene, ListBase *pfLinks, float cframe)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  bool skip = true;

  FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob) {
    ob->id.tag &= ~ID_TAG_DOIT;
    ob = poseAnim_object_get(ob);

    /* Ensure validity of the settings from the context. */
    if (ob == nullptr) {
      continue;
    }

    if (blender::animrig::autokeyframe_cfra_can_key(scene, &ob->id)) {
      ob->id.tag |= ID_TAG_DOIT;
      skip = false;
    }
  }
  FOREACH_OBJECT_IN_MODE_END;

  if (skip) {
    return;
  }

  /* Insert keyframes as necessary if auto-key-framing. */
  KeyingSet *ks = blender::animrig::get_keyingset_for_autokeying(scene,
                                                                 ANIM_KS_WHOLE_CHARACTER_ID);
  blender::Vector<PointerRNA> sources;

  /* iterate over each pose-channel affected, tagging bones to be keyed */
  /* XXX: here we already have the information about what transforms exist, though
   * it might be easier to just overwrite all using normal mechanisms
   */
  LISTBASE_FOREACH (tPChanFCurveLink *, pfl, pfLinks) {
    bPoseChannel *pchan = pfl->pchan;

    if ((pfl->ob->id.tag & ID_TAG_DOIT) == 0) {
      continue;
    }

    /* Add data-source override for the PoseChannel, to be used later. */
    blender::animrig::relative_keyingset_add_source(sources, &pfl->ob->id, &RNA_PoseBone, pchan);
  }

  /* insert keyframes for all relevant bones in one go */
  blender::animrig::apply_keyingset(
      C, &sources, ks, blender::animrig::ModifyKeyMode::INSERT, cframe);

  /* do the bone paths
   * - only do this if keyframes should have been added
   * - do not calculate unless there are paths already to update...
   */
  FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob) {
    if (ob->id.tag & ID_TAG_DOIT) {
      if (ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) {
        // ED_pose_clear_paths(C, ob); /* XXX for now, don't need to clear. */
        /* TODO(sergey): Should ensure we can use more narrow update range here. */
        ED_pose_recalculate_paths(C, scene, ob, POSE_PATH_CALC_RANGE_FULL);
      }
    }
  }
  FOREACH_OBJECT_IN_MODE_END;
}

/* ------------------------- */

LinkData *poseAnim_mapping_getNextFCurve(ListBase *fcuLinks, LinkData *prev, const char *path)
{
  LinkData *first = static_cast<LinkData *>((prev)     ? prev->next :
                                            (fcuLinks) ? fcuLinks->first :
                                                         nullptr);
  LinkData *ld;

  /* check each link to see if the linked F-Curve has a matching path */
  for (ld = first; ld; ld = ld->next) {
    const FCurve *fcu = static_cast<const FCurve *>(ld->data);

    /* check if paths match */
    if (STREQ(path, fcu->rna_path)) {
      return ld;
    }
  }

  /* none found */
  return nullptr;
}

/* *********************************************** */
