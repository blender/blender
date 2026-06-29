/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 *
 * Pose 'Sliding' Tools
 * ====================
 *
 * - Push & Relax, Breakdowner
 *
 *   These tools provide the animator with various capabilities
 *   for interactively controlling the spacing of poses, but also
 *   for 'pushing' and/or 'relaxing' extremes as they see fit.
 *
 * - Propagate
 *
 *   This tool copies elements of the selected pose to successive
 *   keyframes, allowing the animator to go back and modify the poses
 *   for some "static" pose controls, without having to repeatedly
 *   doing a "next paste" dance.
 *
 * - Pose Sculpting (TODO)
 *
 *   This is yet to be implemented, but the idea here is to use
 *   sculpting techniques to make it easier to pose rigs by allowing
 *   rigs to be manipulated using a familiar paint-based interface.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_listbase.hh"
#include "BLI_math_rotation_c.hh"
#include "BLI_string.hh"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.hh"
#include "BKE_nla.hh"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_unit.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"

#include "ED_anim_transformable.hh"
#include "ED_keyframes_edit.hh"
#include "ED_keyframes_keylist.hh"
#include "ED_markers.hh"
#include "ED_numinput.hh"
#include "ED_screen.hh"
#include "ED_util.hh"

#include "ANIM_fcurve.hh"
#include "ANIM_rna.hh"

#include "armature_intern.hh"

namespace blender {

static bool pose_slide_poll(bContext *C)
{
  Object *obact = CTX_data_active_object(C);
  if (!obact) {
    return false;
  }
  const eContextObjectMode mode = CTX_data_mode_enum(C);
  if (mode == CTX_MODE_OBJECT) {
    return true;
  }

  if (!(obact->mode & OB_MODE_EDIT)) {
    Object *obpose = BKE_object_pose_armature_get(obact);
    if (obpose != nullptr) {
      if ((obact == obpose) || (obact->mode & OB_MODE_ALL_WEIGHT_PAINT)) {
        return true;
      }
    }
  }

  return false;
}

/* **************************************************** */
/* A) Push & Relax, Breakdowner */

/** Pose Sliding Modes. */
enum ePoseSlide_Modes {
  /** Exaggerate the pose. */
  POSESLIDE_PUSH = 0,
  /** soften the pose. */
  POSESLIDE_RELAX,
  /** Slide between the endpoint poses, finding a 'soft' spot. */
  POSESLIDE_BREAKDOWN,
  POSESLIDE_BLEND_REST,
  POSESLIDE_BLEND,
};

/** Transforms/Channels to Affect. */
enum ePoseSlide_Channels {
  PS_TFM_ALL = 0, /* All transforms and properties */

  PS_TFM_LOC, /* Loc/Rot/Scale */
  PS_TFM_ROT,
  PS_TFM_SCALE,

  PS_TFM_BBONE_SHAPE, /* Bendy Bones */

  PS_TFM_PROPS, /* Custom Properties */
};

/**
 * Stores the frame range per Object. Since objects can have an NLA, the frame for looking up keys
 * needs to be adjusted per object.
 */
struct ObjectFrameRange {
  /** The Object for which these frame values are valid. */
  Object *object;
  /** `prev_frame`, but in local action time (for F-Curve look-ups to work). */
  float prev_frame;
  /** `next_frame`, but in local action time (for F-Curve look-ups to work). */
  float next_frame;
};

/** Temporary data shared between these operators. */
struct tPoseSlideOp {
  /** current scene */
  Scene *scene;
  /** area that we're operating in (needed for modal()) */
  ScrArea *area;
  /** Region we're operating in (needed for modal()). */
  ARegion *region;
  /** len of the PoseSlideObject array. */

  /** The data to be modified by the slider operator. */
  ListBaseT<SlideSubject> slide_subjects;
  /** binary tree for quicker searching for keyframes (when applicable) */
  AnimKeylist *keylist;

  /** current frame number - global time */
  int current_frame;

  /** frame before current frame (blend-from) - global time */
  int prev_frame;
  /** frame after current frame (blend-to)    - global time */
  int next_frame;

  /** Sliding Mode. */
  ePoseSlide_Modes mode;

  /* Store overlay settings when invoking the operator. Bones will be temporarily hidden. */
  eView3DOverlay_Flag overlay_flag;

  /** Which transforms/channels are affected. */
  ePoseSlide_Channels channels;
  /** Axis-limits for transforms. If any flag is set, the transforms are only applied for that
   * axis. If none are set, all axes are modified. */
  ed::AxisMutable axis_mutability;

  tSlider *slider;

  /** Numeric input. */
  NumInput num;

  Array<ObjectFrameRange> ob_data_array;
};

/** Property enum for #ePoseSlide_Channels. */
static const EnumPropertyItem prop_channels_types[] = {
    {PS_TFM_ALL,
     "ALL",
     0,
     "All Properties",
     "All properties, including transforms, bendy bone shape, and custom properties"},
    {PS_TFM_LOC, "LOC", 0, "Location", "Location only"},
    {PS_TFM_ROT, "ROT", 0, "Rotation", "Rotation only"},
    /* NOTE: `SIZE` identifier is only used for compatibility, should be `SCALE`. */
    {PS_TFM_SCALE, "SIZE", 0, "Scale", "Scale only"},
    {PS_TFM_BBONE_SHAPE, "BBONE", 0, "Bendy Bone", "Bendy Bone shape properties"},
    {PS_TFM_PROPS, "CUSTOM", 0, "Custom Properties", "Custom properties"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Property enum for AxisMutable. */
static const EnumPropertyItem prop_axis_lock_types[] = {
    {ed::AXIS_MUTABLE_ALL, "FREE", 0, "Free", "All axes are affected"},
    {ed::AXIS_MUTABLE_X, "X", 0, "X", "Only X-axis transforms are affected"},
    {ed::AXIS_MUTABLE_Y, "Y", 0, "Y", "Only Y-axis transforms are affected"},
    {ed::AXIS_MUTABLE_Z, "Z", 0, "Z", "Only Z-axis transforms are affected"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* ------------------------------------ */

/**
 * Returns a subset of the given curves where the rna_path matches the given path.
 */
static Vector<FCurve *> fcurves_filtered_by_path(const Span<FCurve *> input_fcurves,
                                                 const StringRef path)
{
  Vector<FCurve *> fcurves;
  for (FCurve *fcurve : input_fcurves) {
    if (StringRefNull(fcurve->rna_path) != path) {
      continue;
    }
    fcurves.append(fcurve);
  }
  return fcurves;
}

/** Operator custom-data initialization. */
static int pose_slide_init(bContext *C, wmOperator *op, ePoseSlide_Modes mode)
{
  tPoseSlideOp *pso = MEM_new<tPoseSlideOp>(__func__);
  op->customdata = pso;

  /* Get info from context. */
  pso->scene = CTX_data_scene(C);
  pso->area = CTX_wm_area(C);     /* Only really needed when doing modal(). */
  pso->region = CTX_wm_region(C); /* Only really needed when doing modal(). */

  pso->current_frame = pso->scene->r.cfra;
  pso->mode = mode;

  /* Set range info from property values - these may get overridden for the invoke(). */
  pso->prev_frame = RNA_int_get(op->ptr, "prev_frame");
  pso->next_frame = RNA_int_get(op->ptr, "next_frame");

  /* Get the set of properties/axes that can be operated on. */
  pso->channels = ePoseSlide_Channels(RNA_enum_get(op->ptr, "channels"));
  pso->axis_mutability = ed::AxisMutable(RNA_enum_get(op->ptr, "axis_lock"));

  pso->slider = ED_slider_create(C);
  ED_slider_factor_set(pso->slider, RNA_float_get(op->ptr, "factor"));

  /* For each Pose-Channel which gets affected, get the F-Curves for that channel
   * and set the relevant transform flags. */
  slide_subjects_get(C, &pso->slide_subjects);
  Set<ID *> unique_ids;
  for (const SlideSubject &tflink : pso->slide_subjects) {
    unique_ids.add(tflink.ptr.owner_id);
  }
  pso->ob_data_array.reinitialize(unique_ids.size());
  int i = 0;
  for (ID *id : unique_ids) {
    ObjectFrameRange *range_data = &pso->ob_data_array[i];
    i++;
    BLI_assert(GS(id->name) == ID_OB);
    range_data->object = id_cast<Object *>(id);
    AnimData *adt = BKE_animdata_from_id(id);
    /* Apply NLA mapping corrections so the frame look-ups work. */
    range_data->prev_frame = BKE_nla_tweakedit_remap(adt, pso->prev_frame, NLATIME_CONVERT_UNMAP);
    range_data->next_frame = BKE_nla_tweakedit_remap(adt, pso->next_frame, NLATIME_CONVERT_UNMAP);
  }

  /* Do basic initialize of RB-BST used for finding keyframes, but leave the filling of it up
   * to the caller of this (usually only invoke() will do it, to make things more efficient). */
  pso->keylist = ED_keylist_create();

  /* Initialize numeric input. */
  initNumInput(&pso->num);
  pso->num.idx_max = 0;                /* One axis. */
  pso->num.unit_type[0] = B_UNIT_NONE; /* Percentages don't have any units. */

  if (pso->area && (pso->area->spacetype == SPACE_VIEW3D)) {
    /* Save current bone visibility. */
    View3D *v3d = static_cast<View3D *>(pso->area->spacedata.first);
    pso->overlay_flag = v3d->overlay.flag;
  }

  /* Return status is whether we've got all the data we were requested to get. */
  return 1;
}

/**
 * Exiting the operator (free data).
 */
static void pose_slide_exit(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso = static_cast<tPoseSlideOp *>(op->customdata);

  ED_slider_destroy(C, pso->slider);

  /* Hide Bone Overlay. */
  if (pso->area && (pso->area->spacetype == SPACE_VIEW3D)) {
    View3D *v3d = static_cast<View3D *>(pso->area->spacedata.first);
    v3d->overlay.flag = pso->overlay_flag;
  }

  /* Free the temp pchan links and their data. */
  slide_subjects_free(&pso->slide_subjects);

  /* Free RB-BST for keyframes (if it contained data). */
  ED_keylist_free(pso->keylist);

  /* Free data itself. */
  MEM_delete(pso);

  /* Cleanup. */
  op->customdata = nullptr;
}

/* ------------------------------------ */

/**
 * Helper for apply() / reset() - refresh the data.
 */
static void pose_slide_refresh(bContext *C, tPoseSlideOp *pso)
{
  /* Wrapper around the generic version, allowing us to add some custom stuff later still. */
  for (SlideSubject &slide_subject : pso->slide_subjects) {
    slide_subjects_refresh(C, slide_subject);
  }
}

/**
 * Get the frame range for the given ID, which is NLA mapped.
 */
static bool pose_frame_range_from_id_get(const tPoseSlideOp *pso,
                                         const ID *id,
                                         float *prev_frame,
                                         float *next_frame)
{
  for (const ObjectFrameRange &object_range : pso->ob_data_array) {
    if (&object_range.object->id == id) {
      *prev_frame = object_range.prev_frame;
      *next_frame = object_range.next_frame;
      return true;
    }
  }
  *prev_frame = *next_frame = 0.0f;
  return false;
}

/* Apply linear blending to the values of the given `prop_type`. */
static void pose_slide_apply_linear(tPoseSlideOp &pso,
                                    SlideSubject &slide_subject,
                                    const ed::AnimTransformable::PropertyType prop_type)
{

  const float factor = ED_slider_factor_get(pso.slider);
  ed::AnimTransformable *transformable = slide_subject.transformable;
  Array<float> prev_values = transformable->get_property(prop_type);
  Array<float> next_values = prev_values;

  {
    const std::string path = transformable->rna_path_to_property(prop_type);
    const Vector<FCurve *> fcurves = fcurves_filtered_by_path(slide_subject.fcurves, path);
    float prev_frame, next_frame;
    pose_frame_range_from_id_get(&pso, transformable->owner_id(), &prev_frame, &next_frame);
    for (const FCurve *fcurve : fcurves) {
      prev_values[fcurve->array_index] = evaluate_fcurve(fcurve, prev_frame);
      next_values[fcurve->array_index] = evaluate_fcurve(fcurve, next_frame);
    }
  }

  /* Encodes a percentage value of where the current frame is between prev_- and next_frame. At 0
   * it is at prev_frame. */
  const float current_frame_factor = (pso.current_frame - pso.prev_frame) /
                                     float(pso.next_frame - pso.prev_frame);
  /* Note christoph: After looking at POSESLIDE_PUSH and _RELAX for a long time I finally realized
   * what they do. They take the linear interpolation of the values based on the current frame and
   * blend the current pose towards or away from it. The usefulness of this is likely limited and
   * the naming could be better. Also this could be combined into a single slider. */
  Array<float> current_frame_breakdown = ed::property_interpolated(
      prev_values, next_values, current_frame_factor);

  const ed::AxisMutable axis_flag = ed::AxisMutable(pso.axis_mutability);

  switch (pso.mode) {

    case POSESLIDE_PUSH: {
      /* Slide the pose away from the breakdown pose in the timeline */
      transformable->blend_property_to(prop_type, current_frame_breakdown, -factor, axis_flag);
      break;
    }
    case POSESLIDE_RELAX: {
      /* Slide the pose towards the breakdown pose in the timeline */
      transformable->blend_property_to(prop_type, current_frame_breakdown, factor, axis_flag);
      break;
    }
    case POSESLIDE_BREAKDOWN: /* Make the current pose slide around between the endpoints. */
    {
      /* Perform simple linear interpolation. */
      Array<float> breakdown = ed::property_interpolated(prev_values, next_values, factor);
      transformable->set_property(prop_type, breakdown, axis_flag);
      break;
    }
    case POSESLIDE_BLEND: /* Blend the current pose with the previous (<50%) or next key (>50%). */
    {
      /* Convert factor to absolute 0-1 range which is needed for `blend_property_to`. */
      const float blend_factor = fabs((factor - 0.5f) * 2);

      if (factor < 0.5) {
        /* Blend to previous key. */
        transformable->blend_property_to(prop_type, prev_values, blend_factor, axis_flag);
      }
      else {
        /* Blend to next key. */
        transformable->blend_property_to(prop_type, next_values, blend_factor, axis_flag);
      }

      break;
    }
    /* Those are handled in pose_slide_rest_pose_apply. */
    case POSESLIDE_BLEND_REST: {
      break;
    }
  }
}

static void pose_slide_apply_property_snapshots(tPoseSlideOp &pso,
                                                SlideSubject &slide_subject,
                                                const Span<PropertySnapshot> snapshots)
{
  for (const PropertySnapshot &snapshot : snapshots) {
    std::optional<std::string> path = RNA_path_from_ID_to_property(&slide_subject.ptr,
                                                                   snapshot.property);
    if (!path) {
      BLI_assert_unreachable();
      continue;
    }
    const float factor = ED_slider_factor_get(pso.slider);
    Array<float> base_values = snapshot.values;
    Array<float> next_frame_values = base_values;
    Array<float> prev_frame_values = base_values;
    {
      float prev_frame, next_frame;
      const bool success = pose_frame_range_from_id_get(
          &pso, slide_subject.ptr.owner_id, &prev_frame, &next_frame);
      /* All `SlideSubject`s should have a frame range. */
      BLI_assert(success);
      UNUSED_VARS_NDEBUG(success);
      const Vector<FCurve *> fcurves = fcurves_filtered_by_path(slide_subject.fcurves,
                                                                path.value());
      if (fcurves.size() == 0) {
        /* Property is not animated. */
        continue;
      }
      for (const FCurve *fcurve : fcurves) {
        prev_frame_values[fcurve->array_index] = evaluate_fcurve(fcurve, prev_frame);
        next_frame_values[fcurve->array_index] = evaluate_fcurve(fcurve, next_frame);
      }
    }

    Array<float> values;
    switch (pso.mode) {
      case POSESLIDE_PUSH:
      case POSESLIDE_RELAX: {
        /* See comment in `pose_slide_apply_linear` for the meaning of those values
         * and push/relax. */
        const float current_frame_factor = (pso.current_frame - pso.prev_frame) /
                                           float(pso.next_frame - pso.prev_frame);
        const Array<float> current_frame_breakdown = ed::property_interpolated(
            prev_frame_values, next_frame_values, current_frame_factor);
        const float factor_sign = pso.mode == POSESLIDE_RELAX ? 1 : -1;
        values = ed::property_interpolated(
            base_values, current_frame_breakdown, factor * factor_sign);
        break;
      }

      case POSESLIDE_BREAKDOWN:
        values = ed::property_interpolated(prev_frame_values, next_frame_values, factor);
        break;

      case POSESLIDE_BLEND: {
        const float blend_factor = fabsf((factor - 0.5f) * 2);
        if (factor < 0.5) {
          values = ed::property_interpolated(base_values, prev_frame_values, blend_factor);
        }
        else {
          values = ed::property_interpolated(base_values, next_frame_values, blend_factor);
        }
        break;
      }
      case POSESLIDE_BLEND_REST:
        /* Those are handled in pose_slide_rest_pose_apply. */
        BLI_assert_unreachable();
        values = base_values;
        break;
    }
    animrig::rna_property_set_as_float(slide_subject.ptr, *snapshot.property, values);
  }
}

/**
 * Helper for apply() - perform sliding for quaternion rotations (using quat blending).
 */
static void pose_slide_apply_quat(tPoseSlideOp *pso, SlideSubject *slide_subject)
{
  ed::AnimTransformable *transformable = slide_subject->transformable;
  float prev_frame, next_frame;

  if (!pose_frame_range_from_id_get(pso, transformable->owner_id(), &prev_frame, &next_frame)) {
    BLI_assert_msg(0, "Invalid slide_subject data");
    return;
  }

  const std::string path = transformable->rna_path_to_property(
      ed::AnimTransformable::PropertyType::ROTATION);

  const float current_frame = float(pso->current_frame);
  const float factor = ED_slider_factor_get(pso->slider);

  /* By using `get_rotation()` we use the current values as default in case they are not animated.
   * Due to using spherical interpolation, the not-animated values may be modified which may not be
   * expected by the user. Ideally this throws a warning.  */
  ed::Rotation rot_prev_frame = transformable->get_rotation();
  ed::Rotation rot_next_frame = rot_prev_frame;
  Vector<FCurve *> quaternion_fcurves = fcurves_filtered_by_path(slide_subject->fcurves, path);
  for (const FCurve *fcurve : quaternion_fcurves) {
    rot_prev_frame.values[fcurve->array_index] = evaluate_fcurve(fcurve, prev_frame);
    rot_next_frame.values[fcurve->array_index] = evaluate_fcurve(fcurve, next_frame);
  }
  normalize_qt(rot_prev_frame.values.data());
  normalize_qt(rot_next_frame.values.data());

  switch (pso->mode) {
    case POSESLIDE_PUSH:
    case POSESLIDE_RELAX: {
      /* Compute breakdown based on actual frame range. */
      const float interp_factor = (current_frame - pso->prev_frame) /
                                  float(pso->next_frame - pso->prev_frame);
      ed::Rotation current = transformable->get_rotation();
      ed::Rotation breakdown = ed::rotation_interpolated(
          rot_prev_frame, rot_next_frame, interp_factor);

      if (pso->mode == POSESLIDE_PUSH) {
        transformable->set_rotation(breakdown);
        transformable->blend_rotation_to(current, factor, ed::AXIS_MUTABLE_ALL);
      }
      else {
        BLI_assert(pso->mode == POSESLIDE_RELAX);
        transformable->set_rotation(current);
        transformable->blend_rotation_to(breakdown, factor, ed::AXIS_MUTABLE_ALL);
      }
      break;
    }

    case POSESLIDE_BREAKDOWN:
      transformable->set_rotation(rot_prev_frame);
      transformable->blend_rotation_to(rot_next_frame, factor, ed::AXIS_MUTABLE_ALL);
      break;

    case POSESLIDE_BLEND: {
      const float blend_factor = fabs((factor - 0.5f) * 2);
      if (factor < 0.5) {
        transformable->blend_rotation_to(rot_prev_frame, blend_factor, ed::AXIS_MUTABLE_ALL);
      }
      else {
        transformable->blend_rotation_to(rot_next_frame, blend_factor, ed::AXIS_MUTABLE_ALL);
      }
      break;
    }

    case POSESLIDE_BLEND_REST:
      BLI_assert_unreachable();
      break;
  }
}

/**
 * apply() - perform the pose sliding between the current pose and the rest pose.
 */
static void pose_slide_rest_pose_apply(bContext *C, tPoseSlideOp *pso)
{
  const ed::AxisMutable axis_flag = ed::AxisMutable(pso->axis_mutability);
  const float slider_factor = ED_slider_factor_get(pso->slider);
  /* For each link, handle each set of transforms. */
  for (SlideSubject &slide_subject : pso->slide_subjects) {
    /* Valid transforms for each #bPoseChannel should have been noted already.
     * - Sliding the pose should be a straightforward exercise for location+rotation,
     *   but rotations get more complicated since we may want to use quaternion blending
     *   for quaternions instead.
     */
    ed::AnimTransformable *transformable = slide_subject.transformable;

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_LOC) &&
        (slide_subject.transform_flag & ACT_TRANS_LOC))
    {
      transformable->blend_property_to(
          ed::AnimTransformable::PropertyType::LOCATION, 0.0f, slider_factor, axis_flag);
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_SCALE) &&
        (slide_subject.transform_flag & ACT_TRANS_SCALE))
    {
      transformable->blend_property_to(
          ed::AnimTransformable::PropertyType::SCALE, 1.0f, slider_factor, axis_flag);
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_ROT) &&
        (slide_subject.transform_flag & ACT_TRANS_ROT))
    {
      transformable->blend_rotation_to(
          ed::identity_rotation(transformable->get_rotation_mode()), slider_factor, axis_flag);
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_BBONE_SHAPE) &&
        (slide_subject.transform_flag & ACT_TRANS_BBONE))
    {
      /* TODO: Not implemented. */
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_PROPS)) {
      /* TODO: Not implemented. */
    }
  }

  /* Depsgraph updates + redraws. */
  pose_slide_refresh(C, pso);
}

/**
 * apply() - perform the pose sliding based on weighting various poses.
 */
static void pose_slide_apply(bContext *C, tPoseSlideOp *pso)
{
  /* Sanitize the frame ranges. */
  if (pso->prev_frame == pso->next_frame) {
    /* Move out one step either side. */
    pso->prev_frame--;
    pso->next_frame++;

    for (ObjectFrameRange &object_range : pso->ob_data_array) {
      AnimData *adt = object_range.object->adt;
      /* Apply NLA mapping corrections so the frame look-ups work. */
      object_range.prev_frame = BKE_nla_tweakedit_remap(
          adt, pso->prev_frame, NLATIME_CONVERT_UNMAP);
      object_range.next_frame = BKE_nla_tweakedit_remap(
          adt, pso->next_frame, NLATIME_CONVERT_UNMAP);
    }
  }

  /* For each link, handle each set of transforms. */
  for (SlideSubject &slide_subject : pso->slide_subjects) {
    ed::AnimTransformable *transformable = slide_subject.transformable;

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_LOC) &&
        (slide_subject.transform_flag & ACT_TRANS_LOC))
    {
      /* Calculate these for the 'location' vector, and use location curves. */
      pose_slide_apply_linear(*pso, slide_subject, ed::AnimTransformable::PropertyType::LOCATION);
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_SCALE) &&
        (slide_subject.transform_flag & ACT_TRANS_SCALE))
    {
      /* Calculate these for the 'scale' vector, and use scale curves. */
      pose_slide_apply_linear(*pso, slide_subject, ed::AnimTransformable::PropertyType::SCALE);
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_ROT) &&
        (slide_subject.transform_flag & ACT_TRANS_ROT))
    {
      /* Everything depends on the rotation mode. */
      const eRotationModes rot_mode = transformable->get_rotation_mode();
      if (rot_mode > 0) {
        pose_slide_apply_linear(
            *pso, slide_subject, ed::AnimTransformable::PropertyType::ROTATION);
      }
      else if (rot_mode == ROT_MODE_AXISANGLE) {
        /* TODO: need to figure out how to do this! */
      }
      else {
        /* Quaternions - use quaternion blending. */
        pose_slide_apply_quat(pso, &slide_subject);
      }
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_BBONE_SHAPE) &&
        (slide_subject.transform_flag & ACT_TRANS_BBONE))
    {
      pose_slide_apply_property_snapshots(
          *pso, slide_subject, slide_subject.additional_properties);
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_PROPS)) {
      pose_slide_apply_property_snapshots(*pso, slide_subject, slide_subject.properties);
      pose_slide_apply_property_snapshots(*pso, slide_subject, slide_subject.system_properties);
    }
  }

  /* Depsgraph updates + redraws. */
  pose_slide_refresh(C, pso);
}

/**
 * Perform auto-key-framing after changes were made + confirmed.
 */
static void pose_slide_autoKeyframe(bContext *C, tPoseSlideOp *pso)
{
  /* Wrapper around the generic call. */
  slide_subjects_autokey(C, pso->scene, &pso->slide_subjects);
}

/**
 * Reset changes made to current pose.
 */
static void pose_slide_reset(tPoseSlideOp *pso)
{
  /* Wrapper around the generic call, so that custom stuff can be added later. */
  slide_subjects_reset(&pso->slide_subjects);
}

/* ------------------------------------ */

/**
 * Draw percentage indicator in status-bar.
 *
 * TODO: Include hints about locks here.
 */
static void pose_slide_draw_status(bContext *C, tPoseSlideOp *pso)
{
  const char *mode_st;
  switch (pso->mode) {
    case POSESLIDE_PUSH:
      mode_st = IFACE_("Push Pose");
      break;
    case POSESLIDE_RELAX:
      mode_st = IFACE_("Relax Pose");
      break;
    case POSESLIDE_BREAKDOWN:
      mode_st = IFACE_("Breakdown");
      break;
    case POSESLIDE_BLEND:
      mode_st = IFACE_("Blend to Neighbor");
      break;
    default:
      /* Unknown. */
      mode_st = IFACE_("Sliding-Tool");
      break;
  }

  ED_slider_property_label_set(pso->slider, mode_st);

  WorkspaceStatus status(C);

  status.item(IFACE_("Confirm"), ICON_MOUSE_LMB);
  status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
  status.item(IFACE_("Adjust"), ICON_MOUSE_MOVE);

  status.item_bool("", pso->channels == PS_TFM_LOC, ICON_EVENT_G);
  status.item_bool("", pso->channels == PS_TFM_ROT, ICON_EVENT_R);
  status.item_bool("", pso->channels == PS_TFM_SCALE, ICON_EVENT_S);
  status.item_bool("", pso->channels == PS_TFM_BBONE_SHAPE, ICON_EVENT_B);
  status.item_bool("", pso->channels == PS_TFM_PROPS, ICON_EVENT_C);

  switch (pso->channels) {
    case PS_TFM_LOC:
      status.item("Location Only", ICON_NONE);
      break;
    case PS_TFM_ROT:
      status.item("Rotation Only", ICON_NONE);
      break;
    case PS_TFM_SCALE:
      status.item("Scale Only", ICON_NONE);
      break;
    case PS_TFM_BBONE_SHAPE:
      status.item("Bendy Bones Only", ICON_NONE);
      break;
    case PS_TFM_PROPS:
      status.item("Custom Properties Only", ICON_NONE);
      break;
    default:
      status.item("Transform limits", ICON_NONE);
      break;
  }

  if (ELEM(pso->channels, PS_TFM_LOC, PS_TFM_ROT, PS_TFM_SCALE)) {
    status.item_bool("", pso->axis_mutability & ed::AXIS_MUTABLE_X, ICON_EVENT_X);
    status.item_bool("", pso->axis_mutability & ed::AXIS_MUTABLE_Y, ICON_EVENT_Y);
    status.item_bool("", pso->axis_mutability & ed::AXIS_MUTABLE_Z, ICON_EVENT_Z);
    status.item(pso->axis_mutability == ed::AXIS_MUTABLE_ALL ? IFACE_("All Axes") :
                                                               IFACE_("Single Axis"),
                ICON_NONE);
  }

  if (hasNumInput(&pso->num)) {
    Scene *scene = pso->scene;
    char str_offs[NUM_STR_REP_LEN];

    outputNumInput(&pso->num, str_offs, scene->unit);

    status.item(str_offs, ICON_NONE);
  }
  else if (pso->area && (pso->area->spacetype == SPACE_VIEW3D)) {
    ED_slider_status_get(pso->slider, status);
    View3D *v3d = static_cast<View3D *>(pso->area->spacedata.first);
    status.item_bool(
        IFACE_("Bone Visibility"), !(v3d->overlay.flag & V3D_OVERLAY_HIDE_BONES), ICON_EVENT_H);
  }
}

/**
 * Common code for invoke() methods.
 */
static wmOperatorStatus pose_slide_invoke_common(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);

  tPoseSlideOp *pso = static_cast<tPoseSlideOp *>(op->customdata);

  ED_slider_init(pso->slider, event);

  /* For each link, add all its keyframes to the search tree. */
  for (SlideSubject &slide_subject : pso->slide_subjects) {
    /* Do this for each F-Curve. */
    for (FCurve *fcu : slide_subject.fcurves) {
      AnimData *adt = BKE_animdata_from_id(slide_subject.transformable->owner_id());
      fcurve_to_keylist(adt, fcu, pso->keylist, 0, {-FLT_MAX, FLT_MAX}, adt != nullptr);
    }
  }

  /* Cancel if no keyframes found. */
  ED_keylist_prepare_for_direct_access(pso->keylist);
  if (ED_keylist_is_empty(pso->keylist)) {
    BKE_report(op->reports, RPT_ERROR, "No keyframes to slide between");
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  float current_frame = float(pso->current_frame);

  /* Firstly, check if the current frame is a keyframe. */
  const ActKeyColumn *ak = ED_keylist_find_exact(pso->keylist, current_frame);

  if (ak == nullptr) {
    /* Current frame is not a keyframe, so search. */
    const ActKeyColumn *pk = ED_keylist_find_prev(pso->keylist, current_frame);
    const ActKeyColumn *nk = ED_keylist_find_next(pso->keylist, current_frame);

    /* New set the frames. */
    /* Previous frame. */
    pso->prev_frame = (pk) ? (pk->cfra) : (pso->current_frame - 1);
    RNA_int_set(op->ptr, "prev_frame", pso->prev_frame);
    /* Next frame. */
    pso->next_frame = (nk) ? (nk->cfra) : (pso->current_frame + 1);
    RNA_int_set(op->ptr, "next_frame", pso->next_frame);
  }
  else {
    /* Current frame itself is a keyframe, so just take keyframes on either side. */
    /* Previous frame. */
    pso->prev_frame = (ak->prev) ? (ak->prev->cfra) : (pso->current_frame - 1);
    RNA_int_set(op->ptr, "prev_frame", pso->prev_frame);
    /* Next frame. */
    pso->next_frame = (ak->next) ? (ak->next->cfra) : (pso->current_frame + 1);
    RNA_int_set(op->ptr, "next_frame", pso->next_frame);
  }

  /* Apply NLA mapping corrections so the frame look-ups work. */
  for (ObjectFrameRange &object_range : pso->ob_data_array) {
    AnimData *adt = object_range.object->adt;
    object_range.prev_frame = BKE_nla_tweakedit_remap(adt, pso->prev_frame, NLATIME_CONVERT_UNMAP);
    object_range.next_frame = BKE_nla_tweakedit_remap(adt, pso->next_frame, NLATIME_CONVERT_UNMAP);
  }

  /* Initial apply for operator. */
  /* TODO: need to calculate factor for initial round too. */
  if (!ELEM(pso->mode, POSESLIDE_BLEND_REST)) {
    pose_slide_apply(C, pso);
  }
  else {
    pose_slide_rest_pose_apply(C, pso);
  }

  /* Depsgraph updates + redraws. */
  pose_slide_refresh(C, pso);

  /* Set cursor to indicate modal. */
  WM_cursor_modal_set(win, WM_CURSOR_EW_SCROLL);

  /* Header print. */
  pose_slide_draw_status(C, pso);

  /* Add a modal handler for this operator. */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/**
 * Handle an event to toggle channels mode.
 */
static void pose_slide_toggle_channels_mode(wmOperator *op,
                                            tPoseSlideOp *pso,
                                            ePoseSlide_Channels channel)
{
  /* Turn channel on or off? */
  if (pso->channels == channel) {
    /* Already limiting to transform only, so pressing this again turns it off */
    pso->channels = PS_TFM_ALL;
  }
  else {
    /* Only this set of channels */
    pso->channels = channel;
  }
  RNA_enum_set(op->ptr, "channels", pso->channels);

  /* Reset axis limits too for good measure */
  pso->axis_mutability = ed::AXIS_MUTABLE_ALL;
  RNA_enum_set(op->ptr, "axis_lock", pso->axis_mutability);
}

/**
 * Handle an event to toggle axis mutability - returns whether any change in state is needed.
 */
static bool pose_slide_toggle_axis_mutability(wmOperator *op,
                                              tPoseSlideOp *pso,
                                              const ed::AxisMutable axis)
{
  /* Axis can only be set when a transform is set - it doesn't make sense otherwise */
  if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_BBONE_SHAPE, PS_TFM_PROPS)) {
    pso->axis_mutability = ed::AXIS_MUTABLE_ALL;
    RNA_enum_set(op->ptr, "axis_lock", pso->axis_mutability);
    return false;
  }

  /* Turn on or off? */
  if (pso->axis_mutability == axis) {
    /* Already limiting on this axis, so turn off */
    pso->axis_mutability = ed::AXIS_MUTABLE_ALL;
  }
  else {
    /* Only this axis */
    pso->axis_mutability = axis;
  }
  RNA_enum_set(op->ptr, "axis_lock", pso->axis_mutability);

  /* Setting changed, so pose update is needed */
  return true;
}

/**
 * Operator `modal()` callback.
 */
static wmOperatorStatus pose_slide_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tPoseSlideOp *pso = static_cast<tPoseSlideOp *>(op->customdata);
  wmWindow *win = CTX_wm_window(C);
  bool do_pose_update = false;

  const bool has_numinput = hasNumInput(&pso->num);

  do_pose_update = ED_slider_modal(pso->slider, event);

  switch (event->type) {
    case LEFTMOUSE: /* Confirm. */
    case EVT_RETKEY:
    case EVT_PADENTER: {
      if (event->val == KM_PRESS) {
        /* Return to normal cursor and header status. */
        ED_workspace_status_text(C, nullptr);
        WM_cursor_modal_restore(win);

        /* Depsgraph updates + redraws. Redraw needed to remove UI. */
        pose_slide_refresh(C, pso);

        /* Insert keyframes as required. */
        pose_slide_autoKeyframe(C, pso);
        pose_slide_exit(C, op);

        /* Done! */
        return OPERATOR_FINISHED;
      }
      break;
    }

    case EVT_ESCKEY: /* Cancel. */
    case RIGHTMOUSE: {
      if (event->val == KM_PRESS) {
        /* Return to normal cursor and header status. */
        ED_workspace_status_text(C, nullptr);
        WM_cursor_modal_restore(win);

        /* Reset transforms back to original state. */
        pose_slide_reset(pso);

        /* Depsgraph updates + redraws. */
        pose_slide_refresh(C, pso);

        /* Clean up temp data. */
        pose_slide_exit(C, op);

        /* Canceled! */
        return OPERATOR_CANCELLED;
      }
      break;
    }

    /* Factor Change... */
    case MOUSEMOVE: /* Calculate new position. */
    {
      /* Only handle mouse-move if not doing numinput. */
      if (has_numinput == false) {
        /* Update pose to reflect the new values (see below). */
        do_pose_update = true;
      }
      break;
    }
    default: {
      if ((event->val == KM_PRESS) && handleNumInput(C, &pso->num, event)) {
        float value;

        /* Grab percentage from numeric input, and store this new value for redo
         * NOTE: users see ints, while internally we use a 0-1 float
         */
        value = ED_slider_factor_get(pso->slider) * 100.0f;
        applyNumInput(&pso->num, &value);

        float factor = value / 100;
        ED_slider_factor_set(pso->slider, factor);
        RNA_float_set(op->ptr, "factor", ED_slider_factor_get(pso->slider));

        /* Update pose to reflect the new values (see below) */
        do_pose_update = true;
        break;
      }
      if (event->val == KM_PRESS) {
        switch (event->type) {
          /* Transform Channel Limits. */
          /* XXX: Replace these hard-coded hotkeys with a modal-map that can be customized. */
          case EVT_GKEY: /* Location */
          {
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_LOC);
            do_pose_update = true;
            break;
          }
          case EVT_RKEY: /* Rotation */
          {
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_ROT);
            do_pose_update = true;
            break;
          }
          case EVT_SKEY: /* Scale */
          {
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_SCALE);
            do_pose_update = true;
            break;
          }
          case EVT_BKEY: /* Bendy Bones */
          {
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_BBONE_SHAPE);
            do_pose_update = true;
            break;
          }
          case EVT_CKEY: /* Custom Properties */
          {
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_PROPS);
            do_pose_update = true;
            break;
          }

          /* Axis Locks */
          /* XXX: Hardcoded... */
          case EVT_XKEY: {
            if (pose_slide_toggle_axis_mutability(op, pso, ed::AXIS_MUTABLE_X)) {
              do_pose_update = true;
            }
            break;
          }
          case EVT_YKEY: {
            if (pose_slide_toggle_axis_mutability(op, pso, ed::AXIS_MUTABLE_Y)) {
              do_pose_update = true;
            }
            break;
          }
          case EVT_ZKEY: {
            if (pose_slide_toggle_axis_mutability(op, pso, ed::AXIS_MUTABLE_Z)) {
              do_pose_update = true;
            }
            break;
          }

          /* Toggle Bone visibility. */
          case EVT_HKEY: {
            if (pso->area && (pso->area->spacetype == SPACE_VIEW3D)) {
              View3D *v3d = static_cast<View3D *>(pso->area->spacedata.first);
              v3d->overlay.flag ^= V3D_OVERLAY_HIDE_BONES;
              ED_region_tag_redraw(pso->region);
            }
            break;
          }

          default: /* Some other unhandled key... */
            break;
        }
      }
      else {
        /* Unhandled event - maybe it was some view manipulation? */
        /* Allow to pass through. */
        return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
      }
    }
  }

  /* Perform pose updates - in response to some user action
   * (e.g. pressing a key or moving the mouse). */
  if (do_pose_update) {
    RNA_float_set(op->ptr, "factor", ED_slider_factor_get(pso->slider));

    /* Update percentage indicator in header. */
    pose_slide_draw_status(C, pso);

    /* Reset transforms (to avoid accumulation errors). */
    pose_slide_reset(pso);

    /* Apply. */
    if (!ELEM(pso->mode, POSESLIDE_BLEND_REST)) {
      pose_slide_apply(C, pso);
    }
    else {
      pose_slide_rest_pose_apply(C, pso);
    }
  }

  /* Still running. */
  return OPERATOR_RUNNING_MODAL;
}

/**
 * Common code for cancel()
 */
static void pose_slide_cancel(bContext *C, wmOperator *op)
{
  /* Cleanup and done. */
  pose_slide_exit(C, op);
}

/**
 * Common code for exec() methods.
 */
static wmOperatorStatus pose_slide_exec_common(bContext *C, wmOperator *op, tPoseSlideOp *pso)
{
  /* Settings should have been set up ok for applying, so just apply! */
  if (!ELEM(pso->mode, POSESLIDE_BLEND_REST)) {
    pose_slide_apply(C, pso);
  }
  else {
    pose_slide_rest_pose_apply(C, pso);
  }

  /* Insert keyframes if needed. */
  pose_slide_autoKeyframe(C, pso);

  /* Cleanup and done. */
  pose_slide_exit(C, op);

  return OPERATOR_FINISHED;
}

/**
 * Common code for defining RNA properties.
 */
static void pose_slide_opdef_properties(wmOperatorType *ot)
{
  PropertyRNA *prop;

  prop = RNA_def_float_factor(ot->srna,
                              "factor",
                              0.5f,
                              0.0f,
                              1.0f,
                              "Factor",
                              "Weighting factor for which keyframe is favored more",
                              0.0,
                              1.0);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_int(ot->srna,
                     "prev_frame",
                     0,
                     MINAFRAME,
                     MAXFRAME,
                     "Previous Keyframe",
                     "Frame number of keyframe immediately before the current frame",
                     0,
                     50);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_int(ot->srna,
                     "next_frame",
                     0,
                     MINAFRAME,
                     MAXFRAME,
                     "Next Keyframe",
                     "Frame number of keyframe immediately after the current frame",
                     0,
                     50);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "channels",
                      prop_channels_types,
                      PS_TFM_ALL,
                      "Channels",
                      "Set of properties that are affected");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_enum(ot->srna,
                      "axis_lock",
                      prop_axis_lock_types,
                      ed::AXIS_MUTABLE_ALL,
                      "Axis Lock",
                      "Transform axis to restrict effects to");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ------------------------------------ */

/**
 * Operator `invoke()` callback for 'push from breakdown' mode.
 */
static wmOperatorStatus pose_slide_push_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* Initialize data. */
  if (pose_slide_init(C, op, POSESLIDE_PUSH) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  /* Do common setup work. */
  return pose_slide_invoke_common(C, op, event);
}

/**
 * Operator `exec()` callback - for push.
 */
static wmOperatorStatus pose_slide_push_exec(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso;

  /* Initialize data (from RNA-props). */
  if (pose_slide_init(C, op, POSESLIDE_PUSH) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  pso = static_cast<tPoseSlideOp *>(op->customdata);

  /* Do common exec work. */
  return pose_slide_exec_common(C, op, pso);
}

void POSE_OT_push(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Push Pose from Breakdown";
  ot->idname = "POSE_OT_push";
  ot->description = "Exaggerate the current pose in regards to the breakdown pose";

  /* callbacks */
  ot->exec = pose_slide_push_exec;
  ot->invoke = pose_slide_push_invoke;
  ot->modal = pose_slide_modal;
  ot->cancel = pose_slide_cancel;
  ot->poll = pose_slide_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */

/**
 * Invoke callback - for 'relax to breakdown' mode.
 */
static wmOperatorStatus pose_slide_relax_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* Initialize data. */
  if (pose_slide_init(C, op, POSESLIDE_RELAX) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  /* Do common setup work. */
  return pose_slide_invoke_common(C, op, event);
}

/**
 * Operator exec() - for relax.
 */
static wmOperatorStatus pose_slide_relax_exec(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso;

  /* Initialize data (from RNA-props). */
  if (pose_slide_init(C, op, POSESLIDE_RELAX) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  pso = static_cast<tPoseSlideOp *>(op->customdata);

  /* Do common exec work. */
  return pose_slide_exec_common(C, op, pso);
}

void POSE_OT_relax(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Relax Pose to Breakdown";
  ot->idname = "POSE_OT_relax";
  ot->description = "Make the current pose more similar to its breakdown pose";

  /* callbacks */
  ot->exec = pose_slide_relax_exec;
  ot->invoke = pose_slide_relax_invoke;
  ot->modal = pose_slide_modal;
  ot->cancel = pose_slide_cancel;
  ot->poll = pose_slide_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */
/**
 * Operator `invoke()` - for 'blend with rest pose' mode.
 */
static wmOperatorStatus pose_slide_blend_rest_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  /* Initialize data. */
  if (pose_slide_init(C, op, POSESLIDE_BLEND_REST) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  tPoseSlideOp *pso = static_cast<tPoseSlideOp *>(op->customdata);
  ED_slider_factor_set(pso->slider, 0);
  ED_slider_factor_bounds_set(pso->slider, -1, 1);

  /* do common setup work */
  return pose_slide_invoke_common(C, op, event);
}

/**
 * Operator `exec()` - for push.
 */
static wmOperatorStatus pose_slide_blend_rest_exec(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso;

  /* Initialize data (from RNA-props). */
  if (pose_slide_init(C, op, POSESLIDE_BLEND_REST) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  pso = static_cast<tPoseSlideOp *>(op->customdata);

  /* Do common exec work. */
  return pose_slide_exec_common(C, op, pso);
}

void POSE_OT_blend_with_rest(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Blend Pose with Rest Pose";
  ot->idname = "POSE_OT_blend_with_rest";
  ot->description = "Make the current pose more similar to, or further away from, the rest pose";

  /* callbacks */
  ot->exec = pose_slide_blend_rest_exec;
  ot->invoke = pose_slide_blend_rest_invoke;
  ot->modal = pose_slide_modal;
  ot->cancel = pose_slide_cancel;
  ot->poll = pose_slide_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */

/**
 * Operator `invoke()` - for 'breakdown' mode.
 */
static wmOperatorStatus pose_slide_breakdown_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  /* Initialize data. */
  if (pose_slide_init(C, op, POSESLIDE_BREAKDOWN) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  /* Do common setup work. */
  return pose_slide_invoke_common(C, op, event);
}

/**
 * Operator exec() - for breakdown.
 */
static wmOperatorStatus pose_slide_breakdown_exec(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso;

  /* Initialize data (from RNA-props). */
  if (pose_slide_init(C, op, POSESLIDE_BREAKDOWN) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  pso = static_cast<tPoseSlideOp *>(op->customdata);

  /* Do common exec work. */
  return pose_slide_exec_common(C, op, pso);
}

void POSE_OT_breakdown(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pose Breakdowner";
  ot->idname = "POSE_OT_breakdown";
  ot->description = "Create a suitable breakdown pose on the current frame";

  /* callbacks */
  ot->exec = pose_slide_breakdown_exec;
  ot->invoke = pose_slide_breakdown_invoke;
  ot->modal = pose_slide_modal;
  ot->cancel = pose_slide_cancel;
  ot->poll = pose_slide_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */
static wmOperatorStatus pose_slide_blend_to_neighbors_invoke(bContext *C,
                                                             wmOperator *op,
                                                             const wmEvent *event)
{
  /* Initialize data. */
  if (pose_slide_init(C, op, POSESLIDE_BLEND) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  /* Do common setup work. */
  return pose_slide_invoke_common(C, op, event);
}

static wmOperatorStatus pose_slide_blend_to_neighbors_exec(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso;

  /* Initialize data (from RNA-props). */
  if (pose_slide_init(C, op, POSESLIDE_BLEND) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  pso = static_cast<tPoseSlideOp *>(op->customdata);

  /* Do common exec work. */
  return pose_slide_exec_common(C, op, pso);
}

void POSE_OT_blend_to_neighbors(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Blend to Neighbor";
  ot->idname = "POSE_OT_blend_to_neighbor";
  ot->description = "Blend from current position to previous or next keyframe";

  /* Callbacks. */
  ot->exec = pose_slide_blend_to_neighbors_exec;
  ot->invoke = pose_slide_blend_to_neighbors_invoke;
  ot->modal = pose_slide_modal;
  ot->cancel = pose_slide_cancel;
  ot->poll = pose_slide_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* Properties. */
  pose_slide_opdef_properties(ot);
}

/* **************************************************** */
/* B) Pose Propagate */

/* "termination conditions" - i.e. when we stop */
enum ePosePropagate_Termination {
  /** Only do on the last keyframe. */
  POSE_PROPAGATE_LAST_KEY = 0,
  /** Stop after the next keyframe. */
  POSE_PROPAGATE_NEXT_KEY,
  /** Stop after the specified frame. */
  POSE_PROPAGATE_BEFORE_FRAME,
  /** Stop when we run out of keyframes. */
  POSE_PROPAGATE_BEFORE_END,

  /** Only do on keyframes that are selected. */
  POSE_PROPAGATE_SELECTED_KEYS,
  /** Only do on the frames where markers are selected. */
  POSE_PROPAGATE_SELECTED_MARKERS,
};

/* --------------------------------- */

struct FrameLink {
  FrameLink *next, *prev;
  float frame;
};

static void propagate_curve_values(ListBaseT<SlideSubject> *slide_subjects,
                                   const float source_frame,
                                   ListBaseT<FrameLink> *target_frames)
{
  using namespace blender::animrig;
  const KeyframeSettings settings = get_keyframe_settings(true);
  for (SlideSubject &slide_subject : *slide_subjects) {
    for (FCurve *fcu : slide_subject.fcurves) {
      if (!fcu->bezt) {
        continue;
      }
      const float current_fcu_value = evaluate_fcurve(fcu, source_frame);
      for (FrameLink &target_frame : *target_frames) {
        insert_vert_fcurve(
            fcu, {target_frame.frame, current_fcu_value}, settings, INSERTKEY_NOFLAGS);
      }
    }
  }
}

static float find_next_key(const ListBaseT<SlideSubject> *slide_subjects, const float start_frame)
{
  float target_frame = FLT_MAX;
  for (const SlideSubject &slide_subject : *slide_subjects) {
    for (const FCurve *fcu : slide_subject.fcurves) {
      if (!fcu->bezt) {
        continue;
      }
      bool replace;
      int current_frame_index = BKE_fcurve_bezt_binarysearch_index(
          fcu->bezt, start_frame, fcu->totvert, &replace);
      if (replace) {
        current_frame_index += 1;
      }
      const int bezt_index = min_ii(current_frame_index, fcu->totvert - 1);
      target_frame = min_ff(target_frame, fcu->bezt[bezt_index].vec[1][0]);
    }
  }

  return target_frame;
}

static float find_last_key(const ListBaseT<SlideSubject> *slide_subjects)
{
  float target_frame = FLT_MIN;
  for (const SlideSubject &slide_subject : *slide_subjects) {
    for (const FCurve *fcu : slide_subject.fcurves) {
      if (!fcu->bezt) {
        continue;
      }
      target_frame = max_ff(target_frame, fcu->bezt[fcu->totvert - 1].vec[1][0]);
    }
  }

  return target_frame;
}

static void get_selected_marker_positions(Scene *scene, ListBaseT<FrameLink> *target_frames)
{
  ListBaseT<CfraElem> selected_markers = {nullptr, nullptr};
  ED_markers_make_cfra_list(&scene->markers, &selected_markers, true);
  for (const CfraElem &marker : selected_markers) {
    FrameLink *link = MEM_new_zeroed<FrameLink>("Marker Key Link");
    link->frame = marker.cfra;
    BLI_addtail(target_frames, link);
  }
  selected_markers.free_no_destruct();
}

static void get_keyed_frames_in_range(const ListBaseT<SlideSubject> *slide_subjects,
                                      const float start_frame,
                                      const float end_frame,
                                      ListBaseT<FrameLink> *target_frames)
{
  AnimKeylist *keylist = ED_keylist_create();
  for (const SlideSubject &slide_subject : *slide_subjects) {
    for (FCurve *fcu : slide_subject.fcurves) {
      fcurve_to_keylist(nullptr, fcu, keylist, 0, {start_frame, end_frame}, false);
    }
  }
  for (const ActKeyColumn &column : *ED_keylist_listbase(keylist)) {
    if (column.cfra <= start_frame) {
      continue;
    }
    if (column.cfra > end_frame) {
      break;
    }
    FrameLink *link = MEM_new_zeroed<FrameLink>("Marker Key Link");
    link->frame = column.cfra;
    BLI_addtail(target_frames, link);
  }
  ED_keylist_free(keylist);
}

static void get_selected_frames(const ListBaseT<SlideSubject> *slide_subjects,
                                ListBaseT<FrameLink> *target_frames)
{
  AnimKeylist *keylist = ED_keylist_create();
  for (const SlideSubject &slide_subject : *slide_subjects) {
    for (FCurve *fcu : slide_subject.fcurves) {
      fcurve_to_keylist(nullptr, fcu, keylist, 0, {-FLT_MAX, FLT_MAX}, false);
    }
  }
  for (ActKeyColumn &column : *ED_keylist_listbase(keylist)) {
    if (!column.sel) {
      continue;
    }
    FrameLink *link = MEM_new_zeroed<FrameLink>("Marker Key Link");
    link->frame = column.cfra;
    BLI_addtail(target_frames, link);
  }
  ED_keylist_free(keylist);
}

/* --------------------------------- */

static wmOperatorStatus pose_propagate_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ListBaseT<SlideSubject> slide_subjects = {nullptr, nullptr};

  const int mode = RNA_enum_get(op->ptr, "mode");

  /* Isolate F-Curves related to the selected bones. */
  slide_subjects_get(C, &slide_subjects);

  if (slide_subjects.is_empty()) {
    /* There is a change the reason the list is empty is
     * that there is no valid object to propagate poses for.
     * This is very unlikely though, so we focus on the most likely issue. */
    BKE_report(op->reports, RPT_ERROR, "No keyframed poses to propagate to");
    return OPERATOR_CANCELLED;
  }

  const float end_frame = RNA_float_get(op->ptr, "end_frame");
  const float current_frame = BKE_scene_frame_get(scene);

  ListBaseT<FrameLink> target_frames = {nullptr, nullptr};

  switch (mode) {
    case POSE_PROPAGATE_NEXT_KEY: {
      float target_frame = find_next_key(&slide_subjects, current_frame);
      FrameLink *link = MEM_new_zeroed<FrameLink>("Next Key Link");
      link->frame = target_frame;
      BLI_addtail(&target_frames, link);
      propagate_curve_values(&slide_subjects, current_frame, &target_frames);
      break;
    }

    case POSE_PROPAGATE_LAST_KEY: {
      float target_frame = find_last_key(&slide_subjects);
      FrameLink *link = MEM_new_zeroed<FrameLink>("Last Key Link");
      link->frame = target_frame;
      BLI_addtail(&target_frames, link);
      propagate_curve_values(&slide_subjects, current_frame, &target_frames);
      break;
    }

    case POSE_PROPAGATE_SELECTED_MARKERS: {
      get_selected_marker_positions(scene, &target_frames);
      propagate_curve_values(&slide_subjects, current_frame, &target_frames);
      break;
    }

    case POSE_PROPAGATE_BEFORE_END: {
      get_keyed_frames_in_range(&slide_subjects, current_frame, FLT_MAX, &target_frames);
      propagate_curve_values(&slide_subjects, current_frame, &target_frames);
      break;
    }
    case POSE_PROPAGATE_BEFORE_FRAME: {
      get_keyed_frames_in_range(&slide_subjects, current_frame, end_frame, &target_frames);
      propagate_curve_values(&slide_subjects, current_frame, &target_frames);
      break;
    }
    case POSE_PROPAGATE_SELECTED_KEYS: {
      get_selected_frames(&slide_subjects, &target_frames);
      propagate_curve_values(&slide_subjects, current_frame, &target_frames);
      break;
    }
  }

  target_frames.free_no_destruct();

  for (SlideSubject &slide_subject : slide_subjects) {
    slide_subjects_refresh(C, slide_subject);
  }

  /* Free temp data. */
  slide_subjects_free(&slide_subjects);

  return OPERATOR_FINISHED;
}

/* --------------------------------- */

void POSE_OT_propagate(wmOperatorType *ot)
{
  static const EnumPropertyItem terminate_items[] = {
      {POSE_PROPAGATE_NEXT_KEY,
       "NEXT_KEY",
       0,
       "To Next Keyframe",
       "Propagate pose to first keyframe following the current frame only"},
      {POSE_PROPAGATE_LAST_KEY,
       "LAST_KEY",
       0,
       "To Last Keyframe",
       "Propagate pose to the last keyframe only (i.e. making action cyclic)"},
      {POSE_PROPAGATE_BEFORE_FRAME,
       "BEFORE_FRAME",
       0,
       "Before Frame",
       "Propagate pose to all keyframes between current frame and 'Frame' property"},
      {POSE_PROPAGATE_BEFORE_END,
       "BEFORE_END",
       0,
       "Before Last Keyframe",
       "Propagate pose to all keyframes from current frame until no more are found"},
      {POSE_PROPAGATE_SELECTED_KEYS,
       "SELECTED_KEYS",
       0,
       "On Selected Keyframes",
       "Propagate pose to all selected keyframes"},
      {POSE_PROPAGATE_SELECTED_MARKERS,
       "SELECTED_MARKERS",
       0,
       "On Selected Markers",
       "Propagate pose to all keyframes occurring on frames with Scene Markers after the current "
       "frame"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Propagate Pose";
  ot->idname = "POSE_OT_propagate";
  ot->description =
      "Copy selected aspects of the current pose to subsequent poses already keyframed";

  /* callbacks */
  ot->exec = pose_propagate_exec;
  ot->poll = ED_operator_posemode; /* XXX: needs selected bones! */

  /* flag */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  /* TODO: add "fade out" control for tapering off amount of propagation as time goes by? */
  ot->prop = RNA_def_enum(ot->srna,
                          "mode",
                          terminate_items,
                          POSE_PROPAGATE_NEXT_KEY,
                          "Terminate Mode",
                          "Method used to determine when to stop propagating pose to keyframes");
  RNA_def_float(ot->srna,
                "end_frame",
                250.0,
                FLT_MIN,
                FLT_MAX,
                "End Frame",
                "Frame to stop propagating frames to (for 'Before Frame' mode)",
                1.0,
                250.0);
}

/* **************************************************** */

}  // namespace blender
