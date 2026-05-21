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
#include "ED_anim_transformable.hh"
#include "ED_armature.hh"
#include "ED_keyframing.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_keyingsets.hh"
#include "ANIM_rna.hh"

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

/**
 * Fills the `r_curves` vector with curves used by the given `ptr`.
 * The returned flags indicate which properties are animated.
 */
static eAction_TransformFlags get_item_transform_flags_and_fcurves(ID &id,
                                                                   PointerRNA &ptr,
                                                                   Vector<FCurve *> &r_curves)
{
  AnimData *adt = BKE_animdata_from_id(&id);
  if (!adt || !adt->action) {
    return eAction_TransformFlags(0);
  }
  animrig::Action &action = adt->action->wrap();

  short flags = 0;

  /* Get the basic path to the properties of interest. */
  const std::optional<std::string> path_to_struct = RNA_path_from_ID_to_struct(&ptr);
  StringRef base_path;
  if (RNA_struct_is_ID(ptr.type)) {
    base_path = "";
  }
  else {
    if (!path_to_struct.has_value()) {
      BLI_assert_unreachable();
      return eAction_TransformFlags(0);
    }
    base_path = path_to_struct.value();
  }

  animrig::foreach_fcurve_in_action_slot(action, adt->slot_handle, [&](FCurve &fcurve) {
    if (fcurve.rna_path == nullptr) {
      return;
    }
    StringRefNull fcurve_path(fcurve.rna_path);

    if (!base_path.is_empty() && !fcurve_path.startswith(base_path)) {
      return;
    }

    StringRef property_name;
    if (base_path.is_empty()) {
      property_name = fcurve_path;
    }
    else {
      /* Normal properties are separated by a dot, custom properties don't have that. */
      if (fcurve_path[base_path.size()] == '.') {
        property_name = fcurve_path.substr(base_path.size() + 1);
      }
      else {
        property_name = fcurve_path.substr(base_path.size());
      }
    }

    if (property_name == "location") {
      flags |= ACT_TRANS_LOC;
      r_curves.append(&fcurve);
      return;
    }

    if (property_name == "scale") {
      flags |= ACT_TRANS_SCALE;
      r_curves.append(&fcurve);
      return;
    }

    if (property_name == "rotation_euler" || property_name == "rotation_quaternion" ||
        property_name == "rotation_axis_angle")
    {
      flags |= ACT_TRANS_ROT;
      r_curves.append(&fcurve);
      return;
    }

    if (property_name.startswith("bbone_")) {
      flags |= ACT_TRANS_BBONE;
      r_curves.append(&fcurve);
      return;
    }

    /* Custom properties only. */
    if (property_name.startswith("[\"")) {
      flags |= ACT_TRANS_PROP;
      r_curves.append(&fcurve);
      return;
    }
  });

  /* return flags found */
  return eAction_TransformFlags(flags);
}
/**
 * Stores a `PropertySnapshot` of the property with the given `property_name` in the given Vector.
 * If the property does not exist in the `ptr` the function doesn't do anything. Also the property
 * has to be supported by `ed::rna_property_get_as_float`.
 */
static void store_property_snapshot(PointerRNA &ptr,
                                    const StringRef property_name,
                                    Vector<PropertySnapshot> &snapshots)
{

  PropertyRNA *prop = RNA_struct_find_property(&ptr, property_name.data());
  if (!prop) {
    return;
  }
  Array<float> property_values = animrig::rna_property_get_as_float(ptr, *prop);
  if (property_values.size() == 0) {
    /* Unsupported property type. */
    return;
  }
  snapshots.append({prop, std::move(property_values)});
}

/* helper for slide_subjects_get() -> get the relevant F-Curves per PoseChannel */
static void pchan_to_slide_subject(ListBaseT<SlideSubject> &slide_subjects,
                                   Object &ob,
                                   bPoseChannel &pchan)
{
  PointerRNA bone_ptr = RNA_pointer_create_discrete(&ob.id, RNA_PoseBone, &pchan);
  Vector<FCurve *> curves;
  const eAction_TransformFlags transFlags = get_item_transform_flags_and_fcurves(
      ob.id, bone_ptr, curves);

  if (!transFlags) {
    return;
  }

  SlideSubject *slide_subject = MEM_new<SlideSubject>("SlideSubject");
  BLI_addtail(&slide_subjects, slide_subject);
  slide_subject->fcurves = curves;

  ed::AnimTransformable *transformable = MEM_new<ed::AnimTransformable>(
      "transformable_pose_bone", ob, pchan);
  slide_subject->transformable = transformable;

  /* Set pchan's transform flags. */
  slide_subject->transform_flag = transFlags;

  slide_subject->old_loc = transformable->get_property(
      ed::AnimTransformable::PropertyType::LOCATION);
  slide_subject->old_rot = transformable->get_rotation();
  slide_subject->old_scale = transformable->get_property(
      ed::AnimTransformable::PropertyType::SCALE);

  slide_subject->ptr = bone_ptr;

  if (transFlags & ACT_TRANS_BBONE) {
    store_property_snapshot(bone_ptr, "bbone_rollin", slide_subject->additional_properties);
    store_property_snapshot(bone_ptr, "bbone_rollout", slide_subject->additional_properties);
    store_property_snapshot(bone_ptr, "bbone_curveinx", slide_subject->additional_properties);
    store_property_snapshot(bone_ptr, "bbone_curveoutx", slide_subject->additional_properties);
    store_property_snapshot(bone_ptr, "bbone_curveinz", slide_subject->additional_properties);
    store_property_snapshot(bone_ptr, "bbone_curveoutz", slide_subject->additional_properties);
    store_property_snapshot(bone_ptr, "bbone_easein", slide_subject->additional_properties);
    store_property_snapshot(bone_ptr, "bbone_easeout", slide_subject->additional_properties);
    store_property_snapshot(bone_ptr, "bbone_scalein", slide_subject->additional_properties);
    store_property_snapshot(bone_ptr, "bbone_scaleout", slide_subject->additional_properties);
  }

  /* Make copy of custom properties. */
  if (transFlags & ACT_TRANS_PROP) {
    if (pchan.prop) {
      for (const IDProperty &id_prop : pchan.prop->data.group) {
        if (ELEM(id_prop.type, IDP_STRING, IDP_ID, IDP_IDPARRAY)) {
          continue;
        }
        char name_escaped[MAX_IDPROP_NAME * 2];
        BLI_str_escape(name_escaped, id_prop.name, sizeof(name_escaped));
        std::string property_name_with_brackets = fmt::format("[\"{}\"]", name_escaped);
        store_property_snapshot(bone_ptr, property_name_with_brackets, slide_subject->properties);
      }
    }
    if (pchan.system_properties) {
      for (const IDProperty &id_prop : pchan.system_properties->data.group) {
        if (ELEM(id_prop.type, IDP_STRING, IDP_ID, IDP_IDPARRAY)) {
          continue;
        }
        store_property_snapshot(bone_ptr, id_prop.name, slide_subject->system_properties);
      }
    }
  }
}

static Object *animated_armature_ob_get(Object *ob_)
{
  Object *ob = BKE_object_pose_armature_get(ob_);
  if (!ELEM(nullptr, ob, ob->data, ob->adt, ob->adt->action)) {
    return ob;
  }
  return nullptr;
}

static void get_pose_bones_for_slide(bContext *C, ListBaseT<SlideSubject> &slide_subjects)
{
  /* For each Pose-Channel which gets affected, get the F-Curves for that channel
   * and set the relevant transform flags... */
  Object *prev_ob, *ob_pose_armature;

  prev_ob = nullptr;
  ob_pose_armature = nullptr;
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, selected_pose_bones, Object *, ob) {
    BLI_assert(pchan != nullptr);
    if (ob != prev_ob) {
      prev_ob = ob;
      ob_pose_armature = animated_armature_ob_get(ob);
    }

    if (ob_pose_armature == nullptr) {
      continue;
    }
    if (!ob_pose_armature->adt || !ob_pose_armature->adt->action) {
      /* No action means no FCurves. */
      continue;
    }

    pchan_to_slide_subject(slide_subjects, *ob_pose_armature, *pchan);
  }
  CTX_DATA_END;

  /* If no PoseChannels were found, try a second pass, doing visible ones instead.
   * i.e. if nothing selected, do whole pose.
   */
  if (slide_subjects.is_empty()) {
    prev_ob = nullptr;
    ob_pose_armature = nullptr;
    CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, visible_pose_bones, Object *, ob) {
      BLI_assert(pchan != nullptr);
      if (ob != prev_ob) {
        prev_ob = ob;
        ob_pose_armature = animated_armature_ob_get(ob);
      }

      if (ob_pose_armature == nullptr) {
        continue;
      }
      if (!ob_pose_armature->adt || !ob_pose_armature->adt->action) {
        /* No action means no FCurves. */
        continue;
      }

      pchan_to_slide_subject(slide_subjects, *ob_pose_armature, *pchan);
    }
    CTX_DATA_END;
  }
}

void slide_subjects_get(bContext *C, ListBaseT<SlideSubject> *r_transformable_list)
{
  BLI_assert(r_transformable_list != nullptr);
  const eContextObjectMode mode = CTX_data_mode_enum(C);
  switch (mode) {
    case CTX_MODE_POSE:
      get_pose_bones_for_slide(C, *r_transformable_list);
      break;

    default:
      /* Not implemented. */
      BLI_assert_unreachable();
      break;
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

    MEM_delete(slide_subject->transformable);

    /* We cannot use BLI_freelinkN because that casts the SlideSubject to a C-style
     * struct causing MEM_delete to do a C-style delete and not deallocate the Vector. */
    BLI_remlink(slide_subjects, slide_subject);
    MEM_delete(slide_subject);
  }
}

/* ------------------------- */

void slide_subjects_refresh(bContext *C, ID *id)
{
  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  switch (GS(id->name)) {
    case ID_OB:
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, id_cast<Object *>(id));
      break;
    default:
      /* Not implemented. */
      BLI_assert_unreachable();
      break;
  }

  AnimData *adt = BKE_animdata_from_id(id);
  if (adt && adt->action) {
    DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
  }
}

void slide_subjects_reset(ListBaseT<SlideSubject> *slide_subjects)
{
  /* Iterate over each transformable affected, restoring all channels to their original values. */
  for (SlideSubject &slide_subject : *slide_subjects) {
    ed::AnimTransformable *transformable = slide_subject.transformable;

    /* just copy all the values over regardless of whether they changed or not */
    transformable->set_property(ed::AnimTransformable::PropertyType::LOCATION,
                                slide_subject.old_loc,
                                ed::AXIS_MUTABLE_ALL);
    transformable->set_rotation(slide_subject.old_rot);
    transformable->set_property(
        ed::AnimTransformable::PropertyType::SCALE, slide_subject.old_scale, ed::AXIS_MUTABLE_ALL);

    for (PropertySnapshot &extra_prop : slide_subject.additional_properties) {
      animrig::rna_property_set_as_float(
          slide_subject.ptr, *extra_prop.property, extra_prop.values);
    }

    for (PropertySnapshot &custom_prop : slide_subject.properties) {
      animrig::rna_property_set_as_float(
          slide_subject.ptr, *custom_prop.property, custom_prop.values);
    }
    for (PropertySnapshot &custom_prop : slide_subject.system_properties) {
      animrig::rna_property_set_as_float(
          slide_subject.ptr, *custom_prop.property, custom_prop.values);
    }
  }
}

void slide_subjects_autokey(bContext *C,
                            Scene *scene,
                            const ListBaseT<SlideSubject> *slide_subjects,
                            const float cframe)
{
  /* Insert keyframes as necessary if auto-key-framing.
   * TODO: don't use a keyingset here. Just use the keyframing code directly. */
  KeyingSet *ks = animrig::get_keyingset_for_autokeying(scene, ANIM_KS_WHOLE_CHARACTER_ID);
  Vector<PointerRNA> sources;

  for (SlideSubject &slide_subject : *slide_subjects) {
    PointerRNA &ptr = slide_subject.ptr;
    if (!animrig::autokeyframe_cfra_can_key(scene, slide_subject.ptr.owner_id)) {
      continue;
    }
    animrig::relative_keyingset_add_source(sources, ptr.owner_id, ptr.type, ptr.data);
  }

  /* insert keyframes for all relevant bones in one go */
  animrig::apply_keyingset(C, &sources, ks, animrig::ModifyKeyMode::INSERT, cframe);

  for (SlideSubject &slide_subject : *slide_subjects) {
    ID *owner_id = slide_subject.transformable->owner_id();
    if (GS(owner_id->name) != ID_OB) {
      continue;
    }
    Object *ob = id_cast<Object *>(owner_id);
    if (!ob->pose) {
      continue;
    }
    if (ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) {
      /* TODO(sergey): Should ensure we can use more narrow update range here. */
      ED_pose_recalculate_paths(C, scene, ob, ANIMVIZ_CALC_RANGE_FULL);
    }
  }
}

/* *********************************************** */

}  // namespace blender
