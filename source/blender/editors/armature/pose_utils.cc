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
#include "BLI_vector.hh"

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

#include "ED_anim_api.hh"
#include "ED_armature.hh"
#include "ED_keyframing.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_keyingsets.hh"

#include "armature_intern.hh"

namespace blender {

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

static eAction_TransformFlags get_item_transform_flags_and_fcurves(Object &ob,
                                                                   bPoseChannel &pchan,
                                                                   Vector<FCurve *> &r_curves)
{
  if (!ob.adt || !ob.adt->action) {
    return eAction_TransformFlags(0);
  }
  animrig::Action &action = ob.adt->action->wrap();

  short flags = 0;

  /* Build PointerRNA from provided data to obtain the paths to use. */
  PointerRNA ptr = RNA_pointer_create_discrete(reinterpret_cast<ID *>(&ob), RNA_PoseBone, &pchan);

  /* Get the basic path to the properties of interest. */
  const std::optional<std::string> basePath = RNA_path_from_ID_to_struct(&ptr);
  if (!basePath) {
    return eAction_TransformFlags(0);
  }

  /* Search F-Curves for the given properties
   * - we cannot use the groups, since they may not be grouped in that way...
   */
  animrig::foreach_fcurve_in_action_slot(action, ob.adt->slot_handle, [&](FCurve &fcurve) {
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
      r_curves.append(&fcurve);
      return;
    }

    pPtr = strstr(bPtr, "scale");
    if (pPtr) {
      flags |= ACT_TRANS_SCALE;
      r_curves.append(&fcurve);
      return;
    }

    pPtr = strstr(bPtr, "rotation");
    if (pPtr) {
      flags |= ACT_TRANS_ROT;
      r_curves.append(&fcurve);
      return;
    }

    pPtr = strstr(bPtr, "bbone_");
    if (pPtr) {
      flags |= ACT_TRANS_BBONE;
      r_curves.append(&fcurve);
      return;
    }

    /* Custom properties only. */
    pPtr = strstr(bPtr, "[\"");
    if (pPtr) {
      flags |= ACT_TRANS_PROP;
      r_curves.append(&fcurve);
      return;
    }
  });

  /* return flags found */
  return eAction_TransformFlags(flags);
}

/* helper for slide_subjects_get() -> get the relevant F-Curves per PoseChannel */
static void fcurves_to_pchan_links_get(ListBaseT<SlideSubject> &slide_subjects,
                                       Object &ob,
                                       bPoseChannel &pchan)
{
  Vector<FCurve *> curves;
  const eAction_TransformFlags transFlags = get_item_transform_flags_and_fcurves(
      ob, pchan, curves);

  if (!transFlags) {
    return;
  }

  SlideSubject *slide_subject = MEM_new<SlideSubject>("SlideSubject");

  slide_subject->ob = &ob;
  slide_subject->fcurves = curves;
  slide_subject->pchan = &pchan;

  /* Get the RNA path to this pchan - this needs to be freed! */
  PointerRNA ptr = RNA_pointer_create_discrete(reinterpret_cast<ID *>(&ob), RNA_PoseBone, &pchan);
  slide_subject->pchan_path = BLI_strdup(RNA_path_from_ID_to_struct(&ptr).value_or("").c_str());

  BLI_addtail(&slide_subjects, slide_subject);

  /* Set pchan's transform flags. */
  slide_subject->transform_flag = transFlags;

  copy_v3_v3(slide_subject->oldloc, pchan.loc);
  copy_v3_v3(slide_subject->oldrot, pchan.eul);
  copy_v3_v3(slide_subject->oldscale, pchan.scale);
  copy_qt_qt(slide_subject->oldquat, pchan.quat);
  copy_v3_v3(slide_subject->oldaxis, pchan.rotAxis);
  slide_subject->oldangle = pchan.rotAngle;

  /* Store current bbone values. */
  slide_subject->roll1 = pchan.roll1;
  slide_subject->roll2 = pchan.roll2;
  slide_subject->curve_in_x = pchan.curve_in_x;
  slide_subject->curve_in_z = pchan.curve_in_z;
  slide_subject->curve_out_x = pchan.curve_out_x;
  slide_subject->curve_out_z = pchan.curve_out_z;
  slide_subject->ease1 = pchan.ease1;
  slide_subject->ease2 = pchan.ease2;

  copy_v3_v3(slide_subject->scale_in, pchan.scale_in);
  copy_v3_v3(slide_subject->scale_out, pchan.scale_out);

  /* Make copy of custom properties. */
  if (transFlags & ACT_TRANS_PROP) {
    if (pchan.prop) {
      slide_subject->oldprops = IDP_CopyProperty(pchan.prop);
    }
    if (pchan.system_properties) {
      slide_subject->old_system_properties = IDP_CopyProperty(pchan.system_properties);
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

void slide_subjects_get(bContext *C, ListBaseT<SlideSubject> *slide_subjects)
{
  BLI_assert(slide_subjects != nullptr);
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

    fcurves_to_pchan_links_get(*slide_subjects, *ob_pose_armature, *pchan);
  }
  CTX_DATA_END;

  /* If no PoseChannels were found, try a second pass, doing visible ones instead.
   * i.e. if nothing selected, do whole pose.
   */
  if (BLI_listbase_is_empty(slide_subjects)) {
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

      fcurves_to_pchan_links_get(*slide_subjects, *ob_pose_armature, *pchan);
    }
    CTX_DATA_END;
  }
}

void slide_subjects_free(ListBaseT<SlideSubject> *slide_subjects)
{
  SlideSubject *slide_subject, *pfln = nullptr;

  /* free the temp pchan links and their data */
  for (slide_subject = static_cast<SlideSubject *>(slide_subjects->first); slide_subject;
       slide_subject = pfln)
  {
    pfln = slide_subject->next;

    /* free custom properties */
    if (slide_subject->oldprops) {
      IDP_FreeProperty(slide_subject->oldprops);
    }

    /* free pchan RNA Path */
    MEM_delete(slide_subject->pchan_path);

    /* We cannot use BLI_freelinkN because that casts the TransformableFCurveLink to a C-style
     * struct causing MEM_delete to do a C-style delete and not deallocate the Vector. */
    BLI_remlink(slide_subjects, slide_subject);
    MEM_delete(slide_subject);
  }
}

/* ------------------------- */

void slide_subjects_refresh(bContext *C, Scene * /*scene*/, Object *ob)
{
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

  AnimData *adt = BKE_animdata_from_id(&ob->id);
  if (adt && adt->action) {
    DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
  }
}

void slide_subjects_reset(ListBaseT<SlideSubject> *slide_subjects)
{
  /* iterate over each pose-channel affected, restoring all channels to their original values */
  for (SlideSubject &slide_subject : *slide_subjects) {
    bPoseChannel *pchan = slide_subject.pchan;

    /* just copy all the values over regardless of whether they changed or not */
    copy_v3_v3(pchan->loc, slide_subject.oldloc);
    copy_v3_v3(pchan->eul, slide_subject.oldrot);
    copy_v3_v3(pchan->scale, slide_subject.oldscale);
    copy_qt_qt(pchan->quat, slide_subject.oldquat);
    copy_v3_v3(pchan->rotAxis, slide_subject.oldaxis);
    pchan->rotAngle = slide_subject.oldangle;

    /* store current bbone values */
    pchan->roll1 = slide_subject.roll1;
    pchan->roll2 = slide_subject.roll2;
    pchan->curve_in_x = slide_subject.curve_in_x;
    pchan->curve_in_z = slide_subject.curve_in_z;
    pchan->curve_out_x = slide_subject.curve_out_x;
    pchan->curve_out_z = slide_subject.curve_out_z;
    pchan->ease1 = slide_subject.ease1;
    pchan->ease2 = slide_subject.ease2;

    copy_v3_v3(pchan->scale_in, slide_subject.scale_in);
    copy_v3_v3(pchan->scale_out, slide_subject.scale_out);

    /* just overwrite values of properties from the stored copies (there should be some) */
    if (slide_subject.oldprops) {
      IDP_SyncGroupValues(slide_subject.pchan->prop, slide_subject.oldprops);
    }
    if (slide_subject.old_system_properties) {
      IDP_SyncGroupValues(slide_subject.pchan->system_properties,
                          slide_subject.old_system_properties);
    }
  }
}

void slide_subjects_autokey(bContext *C,
                            Scene *scene,
                            ListBaseT<SlideSubject> *slide_subjects,
                            float cframe)
{
  const Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  bool skip = true;

  FOREACH_OBJECT_IN_MODE_BEGIN (bmain, scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob) {
    ob->id.tag &= ~ID_TAG_DOIT;
    ob = poseAnim_object_get(ob);

    /* Ensure validity of the settings from the context. */
    if (ob == nullptr) {
      continue;
    }

    if (animrig::autokeyframe_cfra_can_key(scene, &ob->id)) {
      ob->id.tag |= ID_TAG_DOIT;
      skip = false;
    }
  }
  FOREACH_OBJECT_IN_MODE_END;

  if (skip) {
    return;
  }

  /* Insert keyframes as necessary if auto-key-framing. */
  KeyingSet *ks = animrig::get_keyingset_for_autokeying(scene, ANIM_KS_WHOLE_CHARACTER_ID);
  Vector<PointerRNA> sources;

  /* iterate over each pose-channel affected, tagging bones to be keyed */
  /* XXX: here we already have the information about what transforms exist, though
   * it might be easier to just overwrite all using normal mechanisms
   */
  for (SlideSubject &slide_subject : *slide_subjects) {
    bPoseChannel *pchan = slide_subject.pchan;

    if ((slide_subject.ob->id.tag & ID_TAG_DOIT) == 0) {
      continue;
    }

    /* Add data-source override for the PoseChannel, to be used later. */
    animrig::relative_keyingset_add_source(sources, &slide_subject.ob->id, RNA_PoseBone, pchan);
  }

  /* insert keyframes for all relevant bones in one go */
  animrig::apply_keyingset(C, &sources, ks, animrig::ModifyKeyMode::INSERT, cframe);

  /* do the bone paths
   * - only do this if keyframes should have been added
   * - do not calculate unless there are paths already to update...
   */
  FOREACH_OBJECT_IN_MODE_BEGIN (bmain, scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob) {
    if (ob->id.tag & ID_TAG_DOIT) {
      if (ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) {
        // ED_pose_clear_paths(C, ob); /* XXX for now, don't need to clear. */
        /* TODO(sergey): Should ensure we can use more narrow update range here. */
        ED_pose_recalculate_paths(C, scene, ob, ANIMVIZ_CALC_RANGE_FULL);
      }
    }
  }
  FOREACH_OBJECT_IN_MODE_END;
}

/* *********************************************** */

}  // namespace blender
