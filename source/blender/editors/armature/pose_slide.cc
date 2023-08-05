/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung.
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

#include "BLI_blenlib.h"
#include "BLI_dlrbTree.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"

#include "BKE_fcurve.h"
#include "BKE_nla.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_unit.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_path.h"
#include "RNA_prototypes.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "ED_armature.hh"
#include "ED_keyframes_edit.hh"
#include "ED_keyframes_keylist.hh"
#include "ED_keyframing.hh"
#include "ED_markers.hh"
#include "ED_numinput.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_util.hh"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "armature_intern.h"

#include "BLF_api.h"

/* Pixel distance from 0% to 100%. */
#define SLIDE_PIXEL_DISTANCE (300 * U.pixelsize)
#define OVERSHOOT_RANGE_DELTA 0.2f

/* **************************************************** */
/* A) Push & Relax, Breakdowner */

/** Axis Locks. */
enum ePoseSlide_AxisLock {
  PS_LOCK_X = (1 << 0),
  PS_LOCK_Y = (1 << 1),
  PS_LOCK_Z = (1 << 2),
};

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
  PS_TFM_SIZE,

  PS_TFM_BBONE_SHAPE, /* Bendy Bones */

  PS_TFM_PROPS, /* Custom Properties */
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
  uint objects_len;

  /** links between posechannels and f-curves for all the pose objects. */
  ListBase pfLinks;
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
  /** unused for now, but can later get used for storing runtime settings.... */
  short flag;

  /* Store overlay settings when invoking the operator. Bones will be temporarily hidden. */
  int overlay_flag;

  /** Which transforms/channels are affected. */
  ePoseSlide_Channels channels;
  /** Axis-limits for transforms. */
  ePoseSlide_AxisLock axislock;

  tSlider *slider;

  /** Numeric input. */
  NumInput num;

  struct tPoseSlideObject *ob_data_array;
};

struct tPoseSlideObject {
  /** Active object that Pose Info comes from. */
  Object *ob;
  /** `prev_frame`, but in local action time (for F-Curve look-ups to work). */
  float prev_frame;
  /** `next_frame`, but in local action time (for F-Curve look-ups to work). */
  float next_frame;
  bool valid;
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
    {PS_TFM_SIZE, "SIZE", 0, "Scale", "Scale only"},
    {PS_TFM_BBONE_SHAPE, "BBONE", 0, "Bendy Bone", "Bendy Bone shape properties"},
    {PS_TFM_PROPS, "CUSTOM", 0, "Custom Properties", "Custom properties"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Property enum for ePoseSlide_AxisLock */
static const EnumPropertyItem prop_axis_lock_types[] = {
    {0, "FREE", 0, "Free", "All axes are affected"},
    {PS_LOCK_X, "X", 0, "X", "Only X-axis transforms are affected"},
    {PS_LOCK_Y, "Y", 0, "Y", "Only Y-axis transforms are affected"},
    {PS_LOCK_Z, "Z", 0, "Z", "Only Z-axis transforms are affected"}, /* TODO: Combinations? */
    {0, nullptr, 0, nullptr, nullptr},
};

/* ------------------------------------ */

/** Operator custom-data initialization. */
static int pose_slide_init(bContext *C, wmOperator *op, ePoseSlide_Modes mode)
{
  tPoseSlideOp *pso;

  /* Init slide-op data. */
  pso = static_cast<tPoseSlideOp *>(
      op->customdata = MEM_callocN(sizeof(tPoseSlideOp), "tPoseSlideOp"));

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
  pso->axislock = ePoseSlide_AxisLock(RNA_enum_get(op->ptr, "axis_lock"));

  pso->slider = ED_slider_create(C);
  ED_slider_factor_set(pso->slider, RNA_float_get(op->ptr, "factor"));

  /* For each Pose-Channel which gets affected, get the F-Curves for that channel
   * and set the relevant transform flags. */
  poseAnim_mapping_get(C, &pso->pfLinks);

  Object **objects = BKE_view_layer_array_from_objects_in_mode_unique_data(CTX_data_scene(C),
                                                                           CTX_data_view_layer(C),
                                                                           CTX_wm_view3d(C),
                                                                           &pso->objects_len,
                                                                           OB_MODE_POSE);
  pso->ob_data_array = static_cast<tPoseSlideObject *>(
      MEM_callocN(pso->objects_len * sizeof(tPoseSlideObject), "pose slide objects data"));

  for (uint ob_index = 0; ob_index < pso->objects_len; ob_index++) {
    tPoseSlideObject *ob_data = &pso->ob_data_array[ob_index];
    Object *ob_iter = poseAnim_object_get(objects[ob_index]);

    /* Ensure validity of the settings from the context. */
    if (ob_iter == nullptr) {
      continue;
    }

    ob_data->ob = ob_iter;
    ob_data->valid = true;

    /* Apply NLA mapping corrections so the frame look-ups work. */
    ob_data->prev_frame = BKE_nla_tweakedit_remap(
        ob_data->ob->adt, pso->prev_frame, NLATIME_CONVERT_UNMAP);
    ob_data->next_frame = BKE_nla_tweakedit_remap(
        ob_data->ob->adt, pso->next_frame, NLATIME_CONVERT_UNMAP);

    /* Set depsgraph flags. */
    /* Make sure the lock is set OK, unlock can be accidentally saved? */
    ob_data->ob->pose->flag |= POSE_LOCKED;
    ob_data->ob->pose->flag &= ~POSE_DO_UNLOCK;
  }
  MEM_freeN(objects);

  /* Do basic initialize of RB-BST used for finding keyframes, but leave the filling of it up
   * to the caller of this (usually only invoke() will do it, to make things more efficient). */
  pso->keylist = ED_keylist_create();

  /* Initialize numeric input. */
  initNumInput(&pso->num);
  pso->num.idx_max = 0; /* One axis. */
  pso->num.val_flag[0] |= NUM_NO_NEGATIVE;
  pso->num.unit_type[0] = B_UNIT_NONE; /* Percentages don't have any units. */

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
  if (pso->area) {
    View3D *v3d = static_cast<View3D *>(pso->area->spacedata.first);
    v3d->overlay.flag = pso->overlay_flag;
  }

  /* Free the temp pchan links and their data. */
  poseAnim_mapping_free(&pso->pfLinks);

  /* Free RB-BST for keyframes (if it contained data). */
  ED_keylist_free(pso->keylist);

  if (pso->ob_data_array != nullptr) {
    MEM_freeN(pso->ob_data_array);
  }

  /* Free data itself. */
  MEM_freeN(pso);

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
  for (uint ob_index = 0; ob_index < pso->objects_len; ob_index++) {
    tPoseSlideObject *ob_data = &pso->ob_data_array[ob_index];
    if (ob_data->valid) {
      poseAnim_mapping_refresh(C, pso->scene, ob_data->ob);
    }
  }
}

/**
 * Although this lookup is not ideal, we won't be dealing with a lot of objects at a given time.
 * But if it comes to that we can instead store prev/next frame in the #tPChanFCurveLink.
 */
static bool pose_frame_range_from_object_get(tPoseSlideOp *pso,
                                             Object *ob,
                                             float *prev_frame,
                                             float *next_frame)
{
  for (uint ob_index = 0; ob_index < pso->objects_len; ob_index++) {
    tPoseSlideObject *ob_data = &pso->ob_data_array[ob_index];
    Object *ob_iter = ob_data->ob;

    if (ob_iter == ob) {
      *prev_frame = ob_data->prev_frame;
      *next_frame = ob_data->next_frame;
      return true;
    }
  }
  *prev_frame = *next_frame = 0.0f;
  return false;
}

/**
 * Helper for apply() - perform sliding for some value.
 */
static void pose_slide_apply_val(tPoseSlideOp *pso, FCurve *fcu, Object *ob, float *val)
{
  float prev_frame, next_frame;
  float prev_weight, next_weight;
  pose_frame_range_from_object_get(pso, ob, &prev_frame, &next_frame);

  const float factor = ED_slider_factor_get(pso->slider);
  const float current_frame = float(pso->current_frame);

  /* Calculate the relative weights of the endpoints. */
  if (pso->mode == POSESLIDE_BREAKDOWN) {
    /* Get weights from the factor control. */
    next_weight = factor;
    prev_weight = 1.0f - next_weight;
  }
  else {
    /* - these weights are derived from the relative distance of these
     *   poses from the current frame
     * - they then get normalized so that they only sum up to 1
     */

    next_weight = current_frame - float(pso->prev_frame);
    prev_weight = float(pso->next_frame) - current_frame;

    const float total_weight = next_weight + prev_weight;
    next_weight = (next_weight / total_weight);
    prev_weight = (prev_weight / total_weight);
  }

  /* Get keyframe values for endpoint poses to blend with. */
  /* Previous/start. */
  const float prev_frame_y = evaluate_fcurve(fcu, prev_frame);
  const float next_frame_y = evaluate_fcurve(fcu, next_frame);

  /* Depending on the mode, calculate the new value. */
  switch (pso->mode) {
    case POSESLIDE_PUSH: /* Make the current pose more pronounced. */
    {
      /* Slide the pose away from the breakdown pose in the timeline */
      (*val) -= ((prev_frame_y * prev_weight) + (next_frame_y * next_weight) - (*val)) * factor;
      break;
    }
    case POSESLIDE_RELAX: /* Make the current pose more like its surrounding ones. */
    {
      /* Slide the pose towards the breakdown pose in the timeline */
      (*val) += ((prev_frame_y * prev_weight) + (next_frame_y * next_weight) - (*val)) * factor;
      break;
    }
    case POSESLIDE_BREAKDOWN: /* Make the current pose slide around between the endpoints. */
    {
      /* Perform simple linear interpolation. */
      (*val) = interpf(next_frame_y, prev_frame_y, factor);
      break;
    }
    case POSESLIDE_BLEND: /* Blend the current pose with the previous (<50%) or next key (>50%). */
    {
      const float current_frame_y = evaluate_fcurve(fcu, current_frame);
      /* Convert factor to absolute 0-1 range which is needed for `interpf`. */
      const float blend_factor = fabs((factor - 0.5f) * 2);

      if (factor < 0.5) {
        /* Blend to previous key. */
        (*val) = interpf(prev_frame_y, current_frame_y, blend_factor);
      }
      else {
        /* Blend to next key. */
        (*val) = interpf(next_frame_y, current_frame_y, blend_factor);
      }

      break;
    }
    /* Those are handled in pose_slide_rest_pose_apply. */
    case POSESLIDE_BLEND_REST: {
      break;
    }
  }
}

/**
 * Helper for apply() - perform sliding for some 3-element vector.
 */
static void pose_slide_apply_vec3(tPoseSlideOp *pso,
                                  tPChanFCurveLink *pfl,
                                  float vec[3],
                                  const char propName[])
{
  LinkData *ld = nullptr;
  char *path = nullptr;

  /* Get the path to use. */
  path = BLI_sprintfN("%s.%s", pfl->pchan_path, propName);

  /* Using this path, find each matching F-Curve for the variables we're interested in. */
  while ((ld = poseAnim_mapping_getNextFCurve(&pfl->fcurves, ld, path))) {
    FCurve *fcu = (FCurve *)ld->data;
    const int idx = fcu->array_index;
    const int lock = pso->axislock;

    /* Check if this F-Curve is ok given the current axis locks. */
    BLI_assert(fcu->array_index < 3);

    if ((lock == 0) || ((lock & PS_LOCK_X) && (idx == 0)) || ((lock & PS_LOCK_Y) && (idx == 1)) ||
        ((lock & PS_LOCK_Z) && (idx == 2)))
    {
      /* Just work on these channels one by one... there's no interaction between values. */
      pose_slide_apply_val(pso, fcu, pfl->ob, &vec[fcu->array_index]);
    }
  }

  /* Free the temp path we got. */
  MEM_freeN(path);
}

/**
 * Helper for apply() - perform sliding for custom properties or bbone properties.
 */
static void pose_slide_apply_props(tPoseSlideOp *pso,
                                   tPChanFCurveLink *pfl,
                                   const char prop_prefix[])
{
  PointerRNA ptr = {nullptr};
  int len = strlen(pfl->pchan_path);

  /* Setup pointer RNA for resolving paths. */
  RNA_pointer_create(nullptr, &RNA_PoseBone, pfl->pchan, &ptr);

  /* - custom properties are just denoted using ["..."][etc.] after the end of the base path,
   *   so just check for opening pair after the end of the path
   * - bbone properties are similar, but they always start with a prefix "bbone_*",
   *   so a similar method should work here for those too
   */
  LISTBASE_FOREACH (LinkData *, ld, &pfl->fcurves) {
    FCurve *fcu = (FCurve *)ld->data;
    const char *bPtr, *pPtr;

    if (fcu->rna_path == nullptr) {
      continue;
    }

    /* Do we have a match?
     * - bPtr is the RNA Path with the standard part chopped off.
     * - pPtr is the chunk of the path which is left over.
     */
    bPtr = strstr(fcu->rna_path, pfl->pchan_path) + len;
    pPtr = strstr(bPtr, prop_prefix);

    if (pPtr) {
      /* Use RNA to try and get a handle on this property, then, assuming that it is just
       * numerical, try and grab the value as a float for temp editing before setting back. */
      PropertyRNA *prop = RNA_struct_find_property(&ptr, pPtr);

      if (prop) {
        switch (RNA_property_type(prop)) {
          /* Continuous values that can be smoothly interpolated. */
          case PROP_FLOAT: {
            const bool is_array = RNA_property_array_check(prop);
            float tval;
            if (is_array) {
              if (UNLIKELY(uint(fcu->array_index) >= RNA_property_array_length(&ptr, prop))) {
                break; /* Out of range, skip. */
              }
              tval = RNA_property_float_get_index(&ptr, prop, fcu->array_index);
            }
            else {
              tval = RNA_property_float_get(&ptr, prop);
            }

            pose_slide_apply_val(pso, fcu, pfl->ob, &tval);

            if (is_array) {
              RNA_property_float_set_index(&ptr, prop, fcu->array_index, tval);
            }
            else {
              RNA_property_float_set(&ptr, prop, tval);
            }
            break;
          }
          case PROP_INT: {
            const bool is_array = RNA_property_array_check(prop);
            float tval;
            if (is_array) {
              if (UNLIKELY(uint(fcu->array_index) >= RNA_property_array_length(&ptr, prop))) {
                break; /* Out of range, skip. */
              }
              tval = RNA_property_int_get_index(&ptr, prop, fcu->array_index);
            }
            else {
              tval = RNA_property_int_get(&ptr, prop);
            }

            pose_slide_apply_val(pso, fcu, pfl->ob, &tval);

            if (is_array) {
              RNA_property_int_set_index(&ptr, prop, fcu->array_index, tval);
            }
            else {
              RNA_property_int_set(&ptr, prop, tval);
            }
            break;
          }

          /* Values which can only take discrete values. */
          case PROP_BOOLEAN: {
            const bool is_array = RNA_property_array_check(prop);
            float tval;
            if (is_array) {
              if (UNLIKELY(uint(fcu->array_index) >= RNA_property_array_length(&ptr, prop))) {
                break; /* Out of range, skip. */
              }
              tval = RNA_property_boolean_get_index(&ptr, prop, fcu->array_index);
            }
            else {
              tval = RNA_property_boolean_get(&ptr, prop);
            }

            pose_slide_apply_val(pso, fcu, pfl->ob, &tval);

            /* XXX: do we need threshold clamping here? */
            if (is_array) {
              RNA_property_boolean_set_index(&ptr, prop, fcu->array_index, tval);
            }
            else {
              RNA_property_boolean_set(&ptr, prop, tval);
            }
            break;
          }
          case PROP_ENUM: {
            /* Don't handle this case - these don't usually represent interchangeable
             * set of values which should be interpolated between. */
            break;
          }

          default:
            /* Cannot handle. */
            // printf("Cannot Pose Slide non-numerical property\n");
            break;
        }
      }
    }
  }
}

/**
 * Helper for apply() - perform sliding for quaternion rotations (using quat blending).
 */
static void pose_slide_apply_quat(tPoseSlideOp *pso, tPChanFCurveLink *pfl)
{
  FCurve *fcu_w = nullptr, *fcu_x = nullptr, *fcu_y = nullptr, *fcu_z = nullptr;
  bPoseChannel *pchan = pfl->pchan;
  LinkData *ld = nullptr;
  char *path = nullptr;
  float prev_frame, next_frame;

  if (!pose_frame_range_from_object_get(pso, pfl->ob, &prev_frame, &next_frame)) {
    BLI_assert_msg(0, "Invalid pfl data");
    return;
  }

  /* Get the path to use - this should be quaternion rotations only (needs care). */
  path = BLI_sprintfN("%s.%s", pfl->pchan_path, "rotation_quaternion");

  /* Get the current frame number. */
  const float current_frame = float(pso->current_frame);
  const float factor = ED_slider_factor_get(pso->slider);

  /* Using this path, find each matching F-Curve for the variables we're interested in. */
  while ((ld = poseAnim_mapping_getNextFCurve(&pfl->fcurves, ld, path))) {
    FCurve *fcu = (FCurve *)ld->data;

    /* Assign this F-Curve to one of the relevant pointers. */
    switch (fcu->array_index) {
      case 3: /* z */
        fcu_z = fcu;
        break;
      case 2: /* y */
        fcu_y = fcu;
        break;
      case 1: /* x */
        fcu_x = fcu;
        break;
      case 0: /* w */
        fcu_w = fcu;
        break;
    }
  }

  /* Only if all channels exist, proceed. */
  if (fcu_w && fcu_x && fcu_y && fcu_z) {
    float quat_final[4];

    /* Perform blending. */
    if (ELEM(pso->mode, POSESLIDE_BREAKDOWN, POSESLIDE_PUSH, POSESLIDE_RELAX)) {
      float quat_prev[4], quat_next[4];

      quat_prev[0] = evaluate_fcurve(fcu_w, prev_frame);
      quat_prev[1] = evaluate_fcurve(fcu_x, prev_frame);
      quat_prev[2] = evaluate_fcurve(fcu_y, prev_frame);
      quat_prev[3] = evaluate_fcurve(fcu_z, prev_frame);

      quat_next[0] = evaluate_fcurve(fcu_w, next_frame);
      quat_next[1] = evaluate_fcurve(fcu_x, next_frame);
      quat_next[2] = evaluate_fcurve(fcu_y, next_frame);
      quat_next[3] = evaluate_fcurve(fcu_z, next_frame);

      normalize_qt(quat_prev);
      normalize_qt(quat_next);

      if (pso->mode == POSESLIDE_BREAKDOWN) {
        /* Just perform the interpolation between quat_prev and
         * quat_next using pso->factor as a guide. */
        interp_qt_qtqt(quat_final, quat_prev, quat_next, factor);
      }
      else {
        float quat_curr[4], quat_breakdown[4];

        normalize_qt_qt(quat_curr, pchan->quat);

        /* Compute breakdown based on actual frame range. */
        const float interp_factor = (current_frame - pso->prev_frame) /
                                    float(pso->next_frame - pso->prev_frame);

        interp_qt_qtqt(quat_breakdown, quat_prev, quat_next, interp_factor);

        if (pso->mode == POSESLIDE_PUSH) {
          interp_qt_qtqt(quat_final, quat_breakdown, quat_curr, 1.0f + factor);
        }
        else {
          BLI_assert(pso->mode == POSESLIDE_RELAX);
          interp_qt_qtqt(quat_final, quat_curr, quat_breakdown, factor);
        }
      }
    }
    else if (pso->mode == POSESLIDE_BLEND) {
      float quat_blend[4];
      float quat_curr[4];

      copy_qt_qt(quat_curr, pchan->quat);

      if (factor < 0.5) {
        quat_blend[0] = evaluate_fcurve(fcu_w, prev_frame);
        quat_blend[1] = evaluate_fcurve(fcu_x, prev_frame);
        quat_blend[2] = evaluate_fcurve(fcu_y, prev_frame);
        quat_blend[3] = evaluate_fcurve(fcu_z, prev_frame);
      }
      else {
        quat_blend[0] = evaluate_fcurve(fcu_w, next_frame);
        quat_blend[1] = evaluate_fcurve(fcu_x, next_frame);
        quat_blend[2] = evaluate_fcurve(fcu_y, next_frame);
        quat_blend[3] = evaluate_fcurve(fcu_z, next_frame);
      }

      normalize_qt(quat_blend);
      normalize_qt(quat_curr);

      const float blend_factor = fabs((factor - 0.5f) * 2);

      interp_qt_qtqt(quat_final, quat_curr, quat_blend, blend_factor);
    }

    /* Apply final to the pose bone, keeping compatible for similar keyframe positions. */
    quat_to_compatible_quat(pchan->quat, quat_final, pchan->quat);
  }

  /* Free the path now. */
  MEM_freeN(path);
}

static void pose_slide_rest_pose_apply_vec3(tPoseSlideOp *pso, float vec[3], float default_value)
{
  /* We only slide to the rest pose. So only use the default rest pose value. */
  const int lock = pso->axislock;
  const float factor = ED_slider_factor_get(pso->slider);
  for (int idx = 0; idx < 3; idx++) {
    if ((lock == 0) || ((lock & PS_LOCK_X) && (idx == 0)) || ((lock & PS_LOCK_Y) && (idx == 1)) ||
        ((lock & PS_LOCK_Z) && (idx == 2)))
    {
      float diff_val = default_value - vec[idx];
      vec[idx] += factor * diff_val;
    }
  }
}

static void pose_slide_rest_pose_apply_other_rot(tPoseSlideOp *pso, float vec[4], bool quat)
{
  /* We only slide to the rest pose. So only use the default rest pose value. */
  float default_values[] = {1.0f, 0.0f, 0.0f, 0.0f};
  if (!quat) {
    /* Axis Angle */
    default_values[0] = 0.0f;
    default_values[2] = 1.0f;
  }
  const float factor = ED_slider_factor_get(pso->slider);
  for (int idx = 0; idx < 4; idx++) {
    float diff_val = default_values[idx] - vec[idx];
    vec[idx] += factor * diff_val;
  }
}

/**
 * apply() - perform the pose sliding between the current pose and the rest pose.
 */
static void pose_slide_rest_pose_apply(bContext *C, tPoseSlideOp *pso)
{
  /* For each link, handle each set of transforms. */
  LISTBASE_FOREACH (tPChanFCurveLink *, pfl, &pso->pfLinks) {
    /* Valid transforms for each #bPoseChannel should have been noted already.
     * - Sliding the pose should be a straightforward exercise for location+rotation,
     *   but rotations get more complicated since we may want to use quaternion blending
     *   for quaternions instead.
     */
    bPoseChannel *pchan = pfl->pchan;

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_LOC) && (pchan->flag & POSE_LOC)) {
      /* Calculate these for the 'location' vector, and use location curves. */
      pose_slide_rest_pose_apply_vec3(pso, pchan->loc, 0.0f);
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_SIZE) && (pchan->flag & POSE_SIZE)) {
      /* Calculate these for the 'scale' vector, and use scale curves. */
      pose_slide_rest_pose_apply_vec3(pso, pchan->size, 1.0f);
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_ROT) && (pchan->flag & POSE_ROT)) {
      /* Everything depends on the rotation mode. */
      if (pchan->rotmode > 0) {
        /* Eulers - so calculate these for the 'eul' vector, and use euler_rotation curves. */
        pose_slide_rest_pose_apply_vec3(pso, pchan->eul, 0.0f);
      }
      else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
        pose_slide_rest_pose_apply_other_rot(pso, pchan->quat, false);
      }
      else {
        /* Quaternions - use quaternion blending. */
        pose_slide_rest_pose_apply_other_rot(pso, pchan->quat, true);
      }
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_BBONE_SHAPE) && (pchan->flag & POSE_BBONE_SHAPE)) {
      /* Bbone properties - they all start a "bbone_" prefix. */
      /* TODO: Not implemented. */
      // pose_slide_apply_props(pso, pfl, "bbone_");
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_PROPS) && (pfl->oldprops)) {
      /* Not strictly a transform, but custom properties contribute
       * to the pose produced in many rigs (e.g. the facial rigs used in Sintel). */
      /* TODO: Not implemented. */
      // pose_slide_apply_props(pso, pfl, "[\"");
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

    for (uint ob_index = 0; ob_index < pso->objects_len; ob_index++) {
      tPoseSlideObject *ob_data = &pso->ob_data_array[ob_index];

      if (!ob_data->valid) {
        continue;
      }

      /* Apply NLA mapping corrections so the frame look-ups work. */
      ob_data->prev_frame = BKE_nla_tweakedit_remap(
          ob_data->ob->adt, pso->prev_frame, NLATIME_CONVERT_UNMAP);
      ob_data->next_frame = BKE_nla_tweakedit_remap(
          ob_data->ob->adt, pso->next_frame, NLATIME_CONVERT_UNMAP);
    }
  }

  /* For each link, handle each set of transforms. */
  LISTBASE_FOREACH (tPChanFCurveLink *, pfl, &pso->pfLinks) {
    /* Valid transforms for each #bPoseChannel should have been noted already
     * - sliding the pose should be a straightforward exercise for location+rotation,
     *   but rotations get more complicated since we may want to use quaternion blending
     *   for quaternions instead...
     */
    bPoseChannel *pchan = pfl->pchan;

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_LOC) && (pchan->flag & POSE_LOC)) {
      /* Calculate these for the 'location' vector, and use location curves. */
      pose_slide_apply_vec3(pso, pfl, pchan->loc, "location");
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_SIZE) && (pchan->flag & POSE_SIZE)) {
      /* Calculate these for the 'scale' vector, and use scale curves. */
      pose_slide_apply_vec3(pso, pfl, pchan->size, "scale");
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_ROT) && (pchan->flag & POSE_ROT)) {
      /* Everything depends on the rotation mode. */
      if (pchan->rotmode > 0) {
        /* Eulers - so calculate these for the 'eul' vector, and use euler_rotation curves. */
        pose_slide_apply_vec3(pso, pfl, pchan->eul, "rotation_euler");
      }
      else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
        /* TODO: need to figure out how to do this! */
      }
      else {
        /* Quaternions - use quaternion blending. */
        pose_slide_apply_quat(pso, pfl);
      }
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_BBONE_SHAPE) && (pchan->flag & POSE_BBONE_SHAPE)) {
      /* Bbone properties - they all start a "bbone_" prefix. */
      pose_slide_apply_props(pso, pfl, "bbone_");
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_PROPS) && (pfl->oldprops)) {
      /* Not strictly a transform, but custom properties contribute
       * to the pose produced in many rigs (e.g. the facial rigs used in Sintel). */
      pose_slide_apply_props(pso, pfl, "[\"");
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
  poseAnim_mapping_autoKeyframe(C, pso->scene, &pso->pfLinks, float(pso->current_frame));
}

/**
 * Reset changes made to current pose.
 */
static void pose_slide_reset(tPoseSlideOp *pso)
{
  /* Wrapper around the generic call, so that custom stuff can be added later. */
  poseAnim_mapping_reset(&pso->pfLinks);
}

/* ------------------------------------ */

/**
 * Draw percentage indicator in status-bar.
 *
 * TODO: Include hints about locks here.
 */
static void pose_slide_draw_status(bContext *C, tPoseSlideOp *pso)
{
  char status_str[UI_MAX_DRAW_STR];
  char limits_str[UI_MAX_DRAW_STR];
  char axis_str[50];
  char mode_str[32];
  char slider_str[UI_MAX_DRAW_STR];
  char bone_vis_str[50];

  switch (pso->mode) {
    case POSESLIDE_PUSH:
      STRNCPY(mode_str, TIP_("Push Pose"));
      break;
    case POSESLIDE_RELAX:
      STRNCPY(mode_str, TIP_("Relax Pose"));
      break;
    case POSESLIDE_BREAKDOWN:
      STRNCPY(mode_str, TIP_("Breakdown"));
      break;
    case POSESLIDE_BLEND:
      STRNCPY(mode_str, TIP_("Blend to Neighbor"));
      break;

    default:
      /* Unknown. */
      STRNCPY(mode_str, TIP_("Sliding-Tool"));
      break;
  }

  switch (pso->axislock) {
    case PS_LOCK_X:
      STRNCPY(axis_str, TIP_("[X]/Y/Z axis only (X to clear)"));
      break;
    case PS_LOCK_Y:
      STRNCPY(axis_str, TIP_("X/[Y]/Z axis only (Y to clear)"));
      break;
    case PS_LOCK_Z:
      STRNCPY(axis_str, TIP_("X/Y/[Z] axis only (Z to clear)"));
      break;

    default:
      if (ELEM(pso->channels, PS_TFM_LOC, PS_TFM_ROT, PS_TFM_SIZE)) {
        STRNCPY(axis_str, TIP_("X/Y/Z = Axis Constraint"));
      }
      else {
        axis_str[0] = '\0';
      }
      break;
  }

  switch (pso->channels) {
    case PS_TFM_LOC:
      SNPRINTF(limits_str, TIP_("[G]/R/S/B/C - Location only (G to clear) | %s"), axis_str);
      break;
    case PS_TFM_ROT:
      SNPRINTF(limits_str, TIP_("G/[R]/S/B/C - Rotation only (R to clear) | %s"), axis_str);
      break;
    case PS_TFM_SIZE:
      SNPRINTF(limits_str, TIP_("G/R/[S]/B/C - Scale only (S to clear) | %s"), axis_str);
      break;
    case PS_TFM_BBONE_SHAPE:
      STRNCPY(limits_str, TIP_("G/R/S/[B]/C - Bendy Bone properties only (B to clear) | %s"));
      break;
    case PS_TFM_PROPS:
      STRNCPY(limits_str, TIP_("G/R/S/B/[C] - Custom Properties only (C to clear) | %s"));
      break;
    default:
      STRNCPY(limits_str, TIP_("G/R/S/B/C - Limit to Transform/Property Set"));
      break;
  }

  STRNCPY(bone_vis_str, TIP_("[H] - Toggle bone visibility"));

  ED_slider_status_string_get(pso->slider, slider_str, sizeof(slider_str));

  if (hasNumInput(&pso->num)) {
    Scene *scene = pso->scene;
    char str_offs[NUM_STR_REP_LEN];

    outputNumInput(&pso->num, str_offs, &scene->unit);

    SNPRINTF(status_str, "%s: %s | %s", mode_str, str_offs, limits_str);
  }
  else {
    SNPRINTF(status_str, "%s: %s | %s | %s", mode_str, limits_str, slider_str, bone_vis_str);
  }

  ED_workspace_status_text(C, status_str);
  ED_area_status_text(pso->area, "");
}

/**
 * Common code for invoke() methods.
 */
static int pose_slide_invoke_common(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);

  tPoseSlideOp *pso = static_cast<tPoseSlideOp *>(op->customdata);

  ED_slider_init(pso->slider, event);

  /* For each link, add all its keyframes to the search tree. */
  LISTBASE_FOREACH (tPChanFCurveLink *, pfl, &pso->pfLinks) {
    /* Do this for each F-Curve. */
    LISTBASE_FOREACH (LinkData *, ld, &pfl->fcurves) {
      FCurve *fcu = (FCurve *)ld->data;
      fcurve_to_keylist(pfl->ob->adt, fcu, pso->keylist, 0);
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
  for (uint ob_index = 0; ob_index < pso->objects_len; ob_index++) {
    tPoseSlideObject *ob_data = &pso->ob_data_array[ob_index];
    if (ob_data->valid) {
      ob_data->prev_frame = BKE_nla_tweakedit_remap(
          ob_data->ob->adt, pso->prev_frame, NLATIME_CONVERT_UNMAP);
      ob_data->next_frame = BKE_nla_tweakedit_remap(
          ob_data->ob->adt, pso->next_frame, NLATIME_CONVERT_UNMAP);
    }
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

  /* Save current bone visibility. */
  View3D *v3d = static_cast<View3D *>(pso->area->spacedata.first);
  pso->overlay_flag = v3d->overlay.flag;

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
  pso->axislock = ePoseSlide_AxisLock(0);
  RNA_enum_set(op->ptr, "axis_lock", pso->axislock);
}

/**
 * Handle an event to toggle axis locks - returns whether any change in state is needed.
 */
static bool pose_slide_toggle_axis_locks(wmOperator *op,
                                         tPoseSlideOp *pso,
                                         ePoseSlide_AxisLock axis)
{
  /* Axis can only be set when a transform is set - it doesn't make sense otherwise */
  if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_BBONE_SHAPE, PS_TFM_PROPS)) {
    pso->axislock = ePoseSlide_AxisLock(0);
    RNA_enum_set(op->ptr, "axis_lock", pso->axislock);
    return false;
  }

  /* Turn on or off? */
  if (pso->axislock == axis) {
    /* Already limiting on this axis, so turn off */
    pso->axislock = ePoseSlide_AxisLock(0);
  }
  else {
    /* Only this axis */
    pso->axislock = axis;
  }
  RNA_enum_set(op->ptr, "axis_lock", pso->axislock);

  /* Setting changed, so pose update is needed */
  return true;
}

/**
 * Operator `modal()` callback.
 */
static int pose_slide_modal(bContext *C, wmOperator *op, const wmEvent *event)
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
        ED_area_status_text(pso->area, nullptr);
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
        ED_area_status_text(pso->area, nullptr);
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
        CLAMP(factor, 0.0f, 1.0f);
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
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_SIZE);
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
            if (pose_slide_toggle_axis_locks(op, pso, PS_LOCK_X)) {
              do_pose_update = true;
            }
            break;
          }
          case EVT_YKEY: {
            if (pose_slide_toggle_axis_locks(op, pso, PS_LOCK_Y)) {
              do_pose_update = true;
            }
            break;
          }
          case EVT_ZKEY: {
            if (pose_slide_toggle_axis_locks(op, pso, PS_LOCK_Z)) {
              do_pose_update = true;
            }
            break;
          }

          /* Toggle Bone visibility. */
          case EVT_HKEY: {
            View3D *v3d = static_cast<View3D *>(pso->area->spacedata.first);
            v3d->overlay.flag ^= V3D_OVERLAY_HIDE_BONES;
            ED_region_tag_redraw(pso->region);
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
static int pose_slide_exec_common(bContext *C, wmOperator *op, tPoseSlideOp *pso)
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
                      0,
                      "Axis Lock",
                      "Transform axis to restrict effects to");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ------------------------------------ */

/**
 * Operator `invoke()` callback for 'push from breakdown' mode.
 */
static int pose_slide_push_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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
static int pose_slide_push_exec(bContext *C, wmOperator *op)
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
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */

/**
 * Invoke callback - for 'relax to breakdown' mode.
 */
static int pose_slide_relax_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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
static int pose_slide_relax_exec(bContext *C, wmOperator *op)
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
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */
/**
 * Operator `invoke()` - for 'blend with rest pose' mode.
 */
static int pose_slide_blend_rest_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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
static int pose_slide_blend_rest_exec(bContext *C, wmOperator *op)
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
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */

/**
 * Operator `invoke()` - for 'breakdown' mode.
 */
static int pose_slide_breakdown_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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
static int pose_slide_breakdown_exec(bContext *C, wmOperator *op)
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
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */
static int pose_slide_blend_to_neighbors_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* Initialize data. */
  if (pose_slide_init(C, op, POSESLIDE_BLEND) == 0) {
    pose_slide_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  /* Do common setup work. */
  return pose_slide_invoke_common(C, op, event);
}

static int pose_slide_blend_to_neighbors_exec(bContext *C, wmOperator *op)
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
  ot->poll = ED_operator_posemode;

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

static void propagate_curve_values(ListBase /*tPChanFCurveLink*/ *pflinks,
                                   const float source_frame,
                                   ListBase /*FrameLink*/ *target_frames)
{
  LISTBASE_FOREACH (tPChanFCurveLink *, pfl, pflinks) {
    LISTBASE_FOREACH (LinkData *, ld, &pfl->fcurves) {
      FCurve *fcu = (FCurve *)ld->data;
      const float current_fcu_value = evaluate_fcurve(fcu, source_frame);
      LISTBASE_FOREACH (FrameLink *, target_frame, target_frames) {
        insert_vert_fcurve(
            fcu, target_frame->frame, current_fcu_value, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NEEDED);
      }
    }
  }
}

static float find_next_key(ListBase *pflinks, const float start_frame)
{
  float target_frame = FLT_MAX;
  LISTBASE_FOREACH (tPChanFCurveLink *, pfl, pflinks) {
    LISTBASE_FOREACH (LinkData *, ld, &pfl->fcurves) {
      FCurve *fcu = (FCurve *)ld->data;
      bool replace;
      int current_frame_index = BKE_fcurve_bezt_binarysearch_index(
          fcu->bezt, start_frame, fcu->totvert, &replace);
      if (replace) {
        const int bezt_index = min_ii(current_frame_index + 1, fcu->totvert - 1);
        target_frame = min_ff(target_frame, fcu->bezt[bezt_index].vec[1][0]);
      }
      else {
        target_frame = min_ff(target_frame, fcu->bezt[current_frame_index].vec[1][0]);
      }
    }
  }

  return target_frame;
}

static float find_last_key(ListBase *pflinks)
{
  float target_frame = FLT_MIN;
  LISTBASE_FOREACH (tPChanFCurveLink *, pfl, pflinks) {
    LISTBASE_FOREACH (LinkData *, ld, &pfl->fcurves) {
      FCurve *fcu = (FCurve *)ld->data;
      target_frame = max_ff(target_frame, fcu->bezt[fcu->totvert - 1].vec[1][0]);
    }
  }

  return target_frame;
}

static void get_selected_marker_positions(Scene *scene, ListBase /*FrameLink*/ *target_frames)
{
  ListBase selected_markers = {nullptr, nullptr};
  ED_markers_make_cfra_list(&scene->markers, &selected_markers, SELECT);
  LISTBASE_FOREACH (CfraElem *, marker, &selected_markers) {
    FrameLink *link = static_cast<FrameLink *>(MEM_callocN(sizeof(FrameLink), "Marker Key Link"));
    link->frame = marker->cfra;
    BLI_addtail(target_frames, link);
  }
  BLI_freelistN(&selected_markers);
}

static void get_keyed_frames_in_range(ListBase *pflinks,
                                      const float start_frame,
                                      const float end_frame,
                                      ListBase /*FrameLink*/ *target_frames)
{
  AnimKeylist *keylist = ED_keylist_create();
  LISTBASE_FOREACH (tPChanFCurveLink *, pfl, pflinks) {
    LISTBASE_FOREACH (LinkData *, ld, &pfl->fcurves) {
      FCurve *fcu = (FCurve *)ld->data;
      fcurve_to_keylist(nullptr, fcu, keylist, 0);
    }
  }
  LISTBASE_FOREACH (ActKeyColumn *, column, ED_keylist_listbase(keylist)) {
    if (column->cfra <= start_frame) {
      continue;
    }
    if (column->cfra > end_frame) {
      break;
    }
    FrameLink *link = static_cast<FrameLink *>(MEM_callocN(sizeof(FrameLink), "Marker Key Link"));
    link->frame = column->cfra;
    BLI_addtail(target_frames, link);
  }
  ED_keylist_free(keylist);
}

static void get_selected_frames(ListBase *pflinks, ListBase /*FrameLink*/ *target_frames)
{
  AnimKeylist *keylist = ED_keylist_create();
  LISTBASE_FOREACH (tPChanFCurveLink *, pfl, pflinks) {
    LISTBASE_FOREACH (LinkData *, ld, &pfl->fcurves) {
      FCurve *fcu = (FCurve *)ld->data;
      fcurve_to_keylist(nullptr, fcu, keylist, 0);
    }
  }
  LISTBASE_FOREACH (ActKeyColumn *, column, ED_keylist_listbase(keylist)) {
    if (!column->sel) {
      continue;
    }
    FrameLink *link = static_cast<FrameLink *>(MEM_callocN(sizeof(FrameLink), "Marker Key Link"));
    link->frame = column->cfra;
    BLI_addtail(target_frames, link);
  }
  ED_keylist_free(keylist);
}

/* --------------------------------- */

static int pose_propagate_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  ListBase pflinks = {nullptr, nullptr};

  const int mode = RNA_enum_get(op->ptr, "mode");

  /* Isolate F-Curves related to the selected bones. */
  poseAnim_mapping_get(C, &pflinks);

  if (BLI_listbase_is_empty(&pflinks)) {
    /* There is a change the reason the list is empty is
     * that there is no valid object to propagate poses for.
     * This is very unlikely though, so we focus on the most likely issue. */
    BKE_report(op->reports, RPT_ERROR, "No keyframed poses to propagate to");
    return OPERATOR_CANCELLED;
  }

  const float end_frame = RNA_float_get(op->ptr, "end_frame");
  const float current_frame = BKE_scene_frame_get(scene);

  ListBase target_frames = {nullptr, nullptr};

  switch (mode) {
    case POSE_PROPAGATE_NEXT_KEY: {
      float target_frame = find_next_key(&pflinks, current_frame);
      FrameLink *link = static_cast<FrameLink *>(MEM_callocN(sizeof(FrameLink), "Next Key Link"));
      link->frame = target_frame;
      BLI_addtail(&target_frames, link);
      propagate_curve_values(&pflinks, current_frame, &target_frames);
      break;
    }

    case POSE_PROPAGATE_LAST_KEY: {
      float target_frame = find_last_key(&pflinks);
      FrameLink *link = static_cast<FrameLink *>(MEM_callocN(sizeof(FrameLink), "Last Key Link"));
      link->frame = target_frame;
      BLI_addtail(&target_frames, link);
      propagate_curve_values(&pflinks, current_frame, &target_frames);
      break;
    }

    case POSE_PROPAGATE_SELECTED_MARKERS: {
      get_selected_marker_positions(scene, &target_frames);
      propagate_curve_values(&pflinks, current_frame, &target_frames);
      break;
    }

    case POSE_PROPAGATE_BEFORE_END: {
      get_keyed_frames_in_range(&pflinks, current_frame, FLT_MAX, &target_frames);
      propagate_curve_values(&pflinks, current_frame, &target_frames);
      break;
    }
    case POSE_PROPAGATE_BEFORE_FRAME: {
      get_keyed_frames_in_range(&pflinks, current_frame, end_frame, &target_frames);
      propagate_curve_values(&pflinks, current_frame, &target_frames);
      break;
    }
    case POSE_PROPAGATE_SELECTED_KEYS: {
      get_selected_frames(&pflinks, &target_frames);
      propagate_curve_values(&pflinks, current_frame, &target_frames);
      break;
    }
  }

  BLI_freelistN(&target_frames);

  /* Free temp data. */
  poseAnim_mapping_free(&pflinks);

  /* Updates + notifiers. */
  FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob) {
    poseAnim_mapping_refresh(C, scene, ob);
  }
  FOREACH_OBJECT_IN_MODE_END;

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
