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
 * The Original Code is Copyright (C) 2009, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 */

/** \file
 * \ingroup edarmature
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_dlrbTree.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.h"
#include "BKE_nla.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_unit.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "ED_armature.h"
#include "ED_keyframes_draw.h"
#include "ED_markers.h"
#include "ED_numinput.h"
#include "ED_screen.h"

#include "armature_intern.h"

/* **************************************************** */
/* == POSE 'SLIDING' TOOLS ==
 *
 * A) Push & Relax, Breakdowner
 * These tools provide the animator with various capabilities
 * for interactively controlling the spacing of poses, but also
 * for 'pushing' and/or 'relaxing' extremes as they see fit.
 *
 * B) Propagate
 * This tool copies elements of the selected pose to successive
 * keyframes, allowing the animator to go back and modify the poses
 * for some "static" pose controls, without having to repeatedly
 * doing a "next paste" dance.
 *
 * C) Pose Sculpting
 * This is yet to be implemented, but the idea here is to use
 * sculpting techniques to make it easier to pose rigs by allowing
 * rigs to be manipulated using a familiar paint-based interface.
 */
/* **************************************************** */
/* A) Push & Relax, Breakdowner */

/* Temporary data shared between these operators */
typedef struct tPoseSlideOp {
  /** current scene */
  Scene *scene;
  /** area that we're operating in (needed for modal()) */
  ScrArea *sa;
  /** region that we're operating in (needed for modal()) */
  ARegion *ar;
  /** len of the PoseSlideObject array. */
  uint objects_len;

  /** links between posechannels and f-curves for all the pose objects. */
  ListBase pfLinks;
  /** binary tree for quicker searching for keyframes (when applicable) */
  DLRBT_Tree keys;

  /** current frame number - global time */
  int cframe;

  /** frame before current frame (blend-from) - global time */
  int prevFrame;
  /** frame after current frame (blend-to)    - global time */
  int nextFrame;

  /** sliding mode (ePoseSlide_Modes) */
  short mode;
  /** unused for now, but can later get used for storing runtime settings.... */
  short flag;

  /** which transforms/channels are affected (ePoseSlide_Channels) */
  short channels;
  /** axis-limits for transforms (ePoseSlide_AxisLock) */
  short axislock;

  /** 0-1 value for determining the influence of whatever is relevant */
  float percentage;

  /** numeric input */
  NumInput num;

  struct tPoseSlideObject *ob_data_array;
} tPoseSlideOp;

typedef struct tPoseSlideObject {
  Object *ob;       /* active object that Pose Info comes from */
  float prevFrameF; /* prevFrame, but in local action time (for F-Curve lookups to work) */
  float nextFrameF; /* nextFrame, but in local action time (for F-Curve lookups to work) */
  bool valid;
} tPoseSlideObject;

/* Pose Sliding Modes */
typedef enum ePoseSlide_Modes {
  POSESLIDE_PUSH = 0,  /* exaggerate the pose... */
  POSESLIDE_RELAX,     /* soften the pose... */
  POSESLIDE_BREAKDOWN, /* slide between the endpoint poses, finding a 'soft' spot */
  POSESLIDE_PUSH_REST,
  POSESLIDE_RELAX_REST,
} ePoseSlide_Modes;

/* Transforms/Channels to Affect */
typedef enum ePoseSlide_Channels {
  PS_TFM_ALL = 0, /* All transforms and properties */

  PS_TFM_LOC, /* Loc/Rot/Scale */
  PS_TFM_ROT,
  PS_TFM_SIZE,

  PS_TFM_BBONE_SHAPE, /* Bendy Bones */

  PS_TFM_PROPS, /* Custom Properties */
} ePoseSlide_Channels;

/* Property enum for ePoseSlide_Channels */
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
    {0, NULL, 0, NULL, NULL},
};

/* Axis Locks */
typedef enum ePoseSlide_AxisLock {
  PS_LOCK_X = (1 << 0),
  PS_LOCK_Y = (1 << 1),
  PS_LOCK_Z = (1 << 2),
} ePoseSlide_AxisLock;

/* Property enum for ePoseSlide_AxisLock */
static const EnumPropertyItem prop_axis_lock_types[] = {
    {0, "FREE", 0, "Free", "All axes are affected"},
    {PS_LOCK_X, "X", 0, "X", "Only X-axis transforms are affected"},
    {PS_LOCK_Y, "Y", 0, "Y", "Only Y-axis transforms are affected"},
    {PS_LOCK_Z, "Z", 0, "Z", "Only Z-axis transforms are affected"},
    /* TODO: Combinations? */
    {0, NULL, 0, NULL, NULL},
};

/* ------------------------------------ */

/* operator init */
static int pose_slide_init(bContext *C, wmOperator *op, ePoseSlide_Modes mode)
{
  tPoseSlideOp *pso;

  /* init slide-op data */
  pso = op->customdata = MEM_callocN(sizeof(tPoseSlideOp), "tPoseSlideOp");

  /* get info from context */
  pso->scene = CTX_data_scene(C);
  pso->sa = CTX_wm_area(C);   /* only really needed when doing modal() */
  pso->ar = CTX_wm_region(C); /* only really needed when doing modal() */

  pso->cframe = pso->scene->r.cfra;
  pso->mode = mode;

  /* set range info from property values - these may get overridden for the invoke() */
  pso->percentage = RNA_float_get(op->ptr, "percentage");
  pso->prevFrame = RNA_int_get(op->ptr, "prev_frame");
  pso->nextFrame = RNA_int_get(op->ptr, "next_frame");

  /* get the set of properties/axes that can be operated on */
  pso->channels = RNA_enum_get(op->ptr, "channels");
  pso->axislock = RNA_enum_get(op->ptr, "axis_lock");

  /* for each Pose-Channel which gets affected, get the F-Curves for that channel
   * and set the relevant transform flags... */
  poseAnim_mapping_get(C, &pso->pfLinks);

  Object **objects = BKE_view_layer_array_from_objects_in_mode_unique_data(
      CTX_data_view_layer(C), CTX_wm_view3d(C), &pso->objects_len, OB_MODE_POSE);
  pso->ob_data_array = MEM_callocN(pso->objects_len * sizeof(tPoseSlideObject),
                                   "pose slide objects data");

  for (uint ob_index = 0; ob_index < pso->objects_len; ob_index++) {
    tPoseSlideObject *ob_data = &pso->ob_data_array[ob_index];
    Object *ob_iter = poseAnim_object_get(objects[ob_index]);

    /* Ensure validity of the settings from the context. */
    if (ob_iter == NULL) {
      continue;
    }

    ob_data->ob = ob_iter;
    ob_data->valid = true;

    /* apply NLA mapping corrections so the frame lookups work */
    ob_data->prevFrameF = BKE_nla_tweakedit_remap(
        ob_data->ob->adt, pso->prevFrame, NLATIME_CONVERT_UNMAP);
    ob_data->nextFrameF = BKE_nla_tweakedit_remap(
        ob_data->ob->adt, pso->nextFrame, NLATIME_CONVERT_UNMAP);

    /* set depsgraph flags */
    /* make sure the lock is set OK, unlock can be accidentally saved? */
    ob_data->ob->pose->flag |= POSE_LOCKED;
    ob_data->ob->pose->flag &= ~POSE_DO_UNLOCK;
  }
  MEM_freeN(objects);

  /* do basic initialize of RB-BST used for finding keyframes, but leave the filling of it up
   * to the caller of this (usually only invoke() will do it, to make things more efficient).
   */
  BLI_dlrbTree_init(&pso->keys);

  /* initialise numeric input */
  initNumInput(&pso->num);
  pso->num.idx_max = 0; /* one axis */
  pso->num.val_flag[0] |= NUM_NO_NEGATIVE;
  pso->num.unit_type[0] = B_UNIT_NONE; /* percentages don't have any units... */

  /* return status is whether we've got all the data we were requested to get */
  return 1;
}

/* exiting the operator - free data */
static void pose_slide_exit(wmOperator *op)
{
  tPoseSlideOp *pso = op->customdata;

  /* if data exists, clear its data and exit */
  if (pso) {
    /* free the temp pchan links and their data */
    poseAnim_mapping_free(&pso->pfLinks);

    /* free RB-BST for keyframes (if it contained data) */
    BLI_dlrbTree_free(&pso->keys);

    if (pso->ob_data_array != NULL) {
      MEM_freeN(pso->ob_data_array);
    }

    /* free data itself */
    MEM_freeN(pso);
  }

  /* cleanup */
  op->customdata = NULL;
}

/* ------------------------------------ */

/* helper for apply() / reset() - refresh the data */
static void pose_slide_refresh(bContext *C, tPoseSlideOp *pso)
{
  /* wrapper around the generic version, allowing us to add some custom stuff later still */
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
                                             float *prevFrameF,
                                             float *nextFrameF)
{
  for (uint ob_index = 0; ob_index < pso->objects_len; ob_index++) {
    tPoseSlideObject *ob_data = &pso->ob_data_array[ob_index];
    Object *ob_iter = ob_data->ob;

    if (ob_iter == ob) {
      *prevFrameF = ob_data->prevFrameF;
      *nextFrameF = ob_data->nextFrameF;
      return true;
    }
  }
  *prevFrameF = *nextFrameF = 0.0f;
  return false;
}

/* helper for apply() - perform sliding for some value */
static void pose_slide_apply_val(tPoseSlideOp *pso, FCurve *fcu, Object *ob, float *val)
{
  float prevFrameF, nextFrameF;
  float cframe = (float)pso->cframe;
  float sVal, eVal;
  float w1, w2;

  pose_frame_range_from_object_get(pso, ob, &prevFrameF, &nextFrameF);

  /* get keyframe values for endpoint poses to blend with */
  /* previous/start */
  sVal = evaluate_fcurve(fcu, prevFrameF);
  /* next/end */
  eVal = evaluate_fcurve(fcu, nextFrameF);

  /* calculate the relative weights of the endpoints */
  if (pso->mode == POSESLIDE_BREAKDOWN) {
    /* get weights from the percentage control */
    w1 = pso->percentage; /* this must come second */
    w2 = 1.0f - w1;       /* this must come first */
  }
  else {
    /* - these weights are derived from the relative distance of these
     *   poses from the current frame
     * - they then get normalized so that they only sum up to 1
     */
    float wtot;

    w1 = cframe - (float)pso->prevFrame;
    w2 = (float)pso->nextFrame - cframe;

    wtot = w1 + w2;
    w1 = (w1 / wtot);
    w2 = (w2 / wtot);
  }

  /* Depending on the mode, calculate the new value:
   * - In all of these, the start+end values are multiplied by w2 and w1 (respectively),
   *   since multiplication in another order would decrease
   *   the value the current frame is closer to.
   */
  switch (pso->mode) {
    case POSESLIDE_PUSH: /* make the current pose more pronounced */
    {
      /* perform a weighted average here, favoring the middle pose
       * - numerator should be larger than denominator to 'expand' the result
       * - perform this weighting a number of times given by the percentage...
       */
      /* TODO: maybe a sensitivity ctrl on top of this is needed */
      int iters = (int)ceil(10.0f * pso->percentage);

      while (iters-- > 0) {
        (*val) = (-((sVal * w2) + (eVal * w1)) + ((*val) * 6.0f)) / 5.0f;
      }
      break;
    }
    case POSESLIDE_RELAX: /* make the current pose more like its surrounding ones */
    {
      /* perform a weighted average here, favoring the middle pose
       * - numerator should be smaller than denominator to 'relax' the result
       * - perform this weighting a number of times given by the percentage...
       */
      /* TODO: maybe a sensitivity ctrl on top of this is needed */
      int iters = (int)ceil(10.0f * pso->percentage);

      while (iters-- > 0) {
        (*val) = (((sVal * w2) + (eVal * w1)) + ((*val) * 5.0f)) / 6.0f;
      }
      break;
    }
    case POSESLIDE_BREAKDOWN: /* make the current pose slide around between the endpoints */
    {
      /* Perform simple linear interpolation -
       * coefficient for start must come from pso->percentage. */
      /* TODO: make this use some kind of spline interpolation instead? */
      (*val) = ((sVal * w2) + (eVal * w1));
      break;
    }
  }
}

/* helper for apply() - perform sliding for some 3-element vector */
static void pose_slide_apply_vec3(tPoseSlideOp *pso,
                                  tPChanFCurveLink *pfl,
                                  float vec[3],
                                  const char propName[])
{
  LinkData *ld = NULL;
  char *path = NULL;

  /* get the path to use... */
  path = BLI_sprintfN("%s.%s", pfl->pchan_path, propName);

  /* using this path, find each matching F-Curve for the variables we're interested in */
  while ((ld = poseAnim_mapping_getNextFCurve(&pfl->fcurves, ld, path))) {
    FCurve *fcu = (FCurve *)ld->data;
    const int idx = fcu->array_index;
    const int lock = pso->axislock;

    /* check if this F-Curve is ok given the current axis locks */
    BLI_assert(fcu->array_index < 3);

    if ((lock == 0) || ((lock & PS_LOCK_X) && (idx == 0)) || ((lock & PS_LOCK_Y) && (idx == 1)) ||
        ((lock & PS_LOCK_Z) && (idx == 2))) {
      /* just work on these channels one by one... there's no interaction between values */
      pose_slide_apply_val(pso, fcu, pfl->ob, &vec[fcu->array_index]);
    }
  }

  /* free the temp path we got */
  MEM_freeN(path);
}

/* helper for apply() - perform sliding for custom properties or bbone properties */
static void pose_slide_apply_props(tPoseSlideOp *pso,
                                   tPChanFCurveLink *pfl,
                                   const char prop_prefix[])
{
  PointerRNA ptr = {{NULL}};
  LinkData *ld;
  int len = strlen(pfl->pchan_path);

  /* setup pointer RNA for resolving paths */
  RNA_pointer_create(NULL, &RNA_PoseBone, pfl->pchan, &ptr);

  /* - custom properties are just denoted using ["..."][etc.] after the end of the base path,
   *   so just check for opening pair after the end of the path
   * - bbone properties are similar, but they always start with a prefix "bbone_*",
   *   so a similar method should work here for those too
   */
  for (ld = pfl->fcurves.first; ld; ld = ld->next) {
    FCurve *fcu = (FCurve *)ld->data;
    const char *bPtr, *pPtr;

    if (fcu->rna_path == NULL) {
      continue;
    }

    /* do we have a match?
     * - bPtr is the RNA Path with the standard part chopped off
     * - pPtr is the chunk of the path which is left over
     */
    bPtr = strstr(fcu->rna_path, pfl->pchan_path) + len;
    pPtr = strstr(bPtr, prop_prefix);

    if (pPtr) {
      /* use RNA to try and get a handle on this property, then, assuming that it is just
       * numerical, try and grab the value as a float for temp editing before setting back
       */
      PropertyRNA *prop = RNA_struct_find_property(&ptr, pPtr);

      if (prop) {
        switch (RNA_property_type(prop)) {
          /* continuous values that can be smoothly interpolated... */
          case PROP_FLOAT: {
            float tval = RNA_property_float_get(&ptr, prop);
            pose_slide_apply_val(pso, fcu, pfl->ob, &tval);
            RNA_property_float_set(&ptr, prop, tval);
            break;
          }
          case PROP_INT: {
            float tval = (float)RNA_property_int_get(&ptr, prop);
            pose_slide_apply_val(pso, fcu, pfl->ob, &tval);
            RNA_property_int_set(&ptr, prop, (int)tval);
            break;
          }

          /* values which can only take discrete values */
          case PROP_BOOLEAN: {
            float tval = (float)RNA_property_boolean_get(&ptr, prop);
            pose_slide_apply_val(pso, fcu, pfl->ob, &tval);
            RNA_property_boolean_set(
                &ptr, prop, (int)tval);  // XXX: do we need threshold clamping here?
            break;
          }
          case PROP_ENUM: {
            /* don't handle this case - these don't usually represent interchangeable
             * set of values which should be interpolated between
             */
            break;
          }

          default:
            /* cannot handle */
            // printf("Cannot Pose Slide non-numerical property\n");
            break;
        }
      }
    }
  }
}

/* helper for apply() - perform sliding for quaternion rotations (using quat blending) */
static void pose_slide_apply_quat(tPoseSlideOp *pso, tPChanFCurveLink *pfl)
{
  FCurve *fcu_w = NULL, *fcu_x = NULL, *fcu_y = NULL, *fcu_z = NULL;
  bPoseChannel *pchan = pfl->pchan;
  LinkData *ld = NULL;
  char *path = NULL;
  float cframe;
  float prevFrameF, nextFrameF;

  if (!pose_frame_range_from_object_get(pso, pfl->ob, &prevFrameF, &nextFrameF)) {
    BLI_assert(!"Invalid pfl data");
    return;
  }

  /* get the path to use - this should be quaternion rotations only (needs care) */
  path = BLI_sprintfN("%s.%s", pfl->pchan_path, "rotation_quaternion");

  /* get the current frame number */
  cframe = (float)pso->cframe;

  /* using this path, find each matching F-Curve for the variables we're interested in */
  while ((ld = poseAnim_mapping_getNextFCurve(&pfl->fcurves, ld, path))) {
    FCurve *fcu = (FCurve *)ld->data;

    /* assign this F-Curve to one of the relevant pointers... */
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

  /* only if all channels exist, proceed */
  if (fcu_w && fcu_x && fcu_y && fcu_z) {
    float quat_prev[4], quat_prev_orig[4];
    float quat_next[4], quat_next_orig[4];
    float quat_curr[4], quat_curr_orig[4];
    float quat_final[4];

    copy_qt_qt(quat_curr_orig, pchan->quat);

    /* get 2 quats */
    quat_prev_orig[0] = evaluate_fcurve(fcu_w, prevFrameF);
    quat_prev_orig[1] = evaluate_fcurve(fcu_x, prevFrameF);
    quat_prev_orig[2] = evaluate_fcurve(fcu_y, prevFrameF);
    quat_prev_orig[3] = evaluate_fcurve(fcu_z, prevFrameF);

    quat_next_orig[0] = evaluate_fcurve(fcu_w, nextFrameF);
    quat_next_orig[1] = evaluate_fcurve(fcu_x, nextFrameF);
    quat_next_orig[2] = evaluate_fcurve(fcu_y, nextFrameF);
    quat_next_orig[3] = evaluate_fcurve(fcu_z, nextFrameF);

    normalize_qt_qt(quat_prev, quat_prev_orig);
    normalize_qt_qt(quat_next, quat_next_orig);
    normalize_qt_qt(quat_curr, quat_curr_orig);

    /* perform blending */
    if (pso->mode == POSESLIDE_BREAKDOWN) {
      /* Just perform the interpolation between quat_prev and
       * quat_next using pso->percentage as a guide. */
      interp_qt_qtqt(quat_final, quat_prev, quat_next, pso->percentage);
    }
    else if (pso->mode == POSESLIDE_PUSH) {
      float quat_diff[4];

      /* calculate the delta transform from the previous to the current */
      /* TODO: investigate ways to favour one transform more? */
      sub_qt_qtqt(quat_diff, quat_curr, quat_prev);

      /* increase the original by the delta transform, by an amount determined by percentage */
      add_qt_qtqt(quat_final, quat_curr, quat_diff, pso->percentage);

      normalize_qt(quat_final);
    }
    else {
      BLI_assert(pso->mode == POSESLIDE_RELAX);
      float quat_interp[4], quat_final_prev[4];
      /* TODO: maybe a sensitivity ctrl on top of this is needed */
      int iters = (int)ceil(10.0f * pso->percentage);

      copy_qt_qt(quat_final, quat_curr);

      /* perform this blending several times until a satisfactory result is reached */
      while (iters-- > 0) {
        /* calculate the interpolation between the endpoints */
        interp_qt_qtqt(quat_interp,
                       quat_prev,
                       quat_next,
                       (cframe - pso->prevFrame) / (pso->nextFrame - pso->prevFrame));

        normalize_qt_qt(quat_final_prev, quat_final);

        /* tricky interpolations - blending between original and new */
        interp_qt_qtqt(quat_final, quat_final_prev, quat_interp, 1.0f / 6.0f);
      }
    }

    /* Apply final to the pose bone, keeping compatible for similar keyframe positions. */
    quat_to_compatible_quat(pchan->quat, quat_final, quat_curr_orig);
  }

  /* free the path now */
  MEM_freeN(path);
}

static void pose_slide_rest_pose_apply_vec3(tPoseSlideOp *pso, float vec[3], float default_value)
{
  /* We only slide to the rest pose. So only use the default rest pose value */
  const int lock = pso->axislock;
  for (int idx = 0; idx < 3; idx++) {
    if ((lock == 0) || ((lock & PS_LOCK_X) && (idx == 0)) || ((lock & PS_LOCK_Y) && (idx == 1)) ||
        ((lock & PS_LOCK_Z) && (idx == 2))) {
      float diff_val = default_value - vec[idx];
      if (pso->mode == POSESLIDE_RELAX_REST) {
        vec[idx] += pso->percentage * diff_val;
      }
      else {
        /* Push */
        vec[idx] -= pso->percentage * diff_val;
      }
    }
  }
}

static void pose_slide_rest_pose_apply_other_rot(tPoseSlideOp *pso, float vec[4], bool quat)
{
  /* We only slide to the rest pose. So only use the default rest pose value */
  float default_values[] = {1.0f, 0.0f, 0.0f, 0.0f};
  if (!quat) {
    /* Axis Angle */
    default_values[0] = 0.0f;
    default_values[2] = 1.0f;
  }
  for (int idx = 0; idx < 4; idx++) {
    float diff_val = default_values[idx] - vec[idx];
    if (pso->mode == POSESLIDE_RELAX_REST) {
      vec[idx] += pso->percentage * diff_val;
    }
    else {
      /* Push */
      vec[idx] -= pso->percentage * diff_val;
    }
  }
}

/* apply() - perform the pose sliding between the current pose and the rest pose */
static void pose_slide_rest_pose_apply(bContext *C, tPoseSlideOp *pso)
{
  tPChanFCurveLink *pfl;

  /* for each link, handle each set of transforms */
  for (pfl = pso->pfLinks.first; pfl; pfl = pfl->next) {
    /* valid transforms for each PoseChannel should have been noted already
     * - sliding the pose should be a straightforward exercise for location+rotation,
     *   but rotations get more complicated since we may want to use quaternion blending
     *   for quaternions instead...
     */
    bPoseChannel *pchan = pfl->pchan;

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_LOC) && (pchan->flag & POSE_LOC)) {
      /* calculate these for the 'location' vector, and use location curves */
      pose_slide_rest_pose_apply_vec3(pso, pchan->loc, 0.0f);
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_SIZE) && (pchan->flag & POSE_SIZE)) {
      /* calculate these for the 'scale' vector, and use scale curves */
      pose_slide_rest_pose_apply_vec3(pso, pchan->size, 1.0f);
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_ROT) && (pchan->flag & POSE_ROT)) {
      /* everything depends on the rotation mode */
      if (pchan->rotmode > 0) {
        /* eulers - so calculate these for the 'eul' vector, and use euler_rotation curves */
        pose_slide_rest_pose_apply_vec3(pso, pchan->eul, 0.0f);
      }
      else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
        pose_slide_rest_pose_apply_other_rot(pso, pchan->quat, false);
      }
      else {
        /* quaternions - use quaternion blending */
        pose_slide_rest_pose_apply_other_rot(pso, pchan->quat, true);
      }
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_BBONE_SHAPE) && (pchan->flag & POSE_BBONE_SHAPE)) {
      /* bbone properties - they all start a "bbone_" prefix */
      // TODO Not implemented
      // pose_slide_apply_props(pso, pfl, "bbone_");
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_PROPS) && (pfl->oldprops)) {
      /* Not strictly a transform, but custom properties contribute
       * to the pose produced in many rigs (e.g. the facial rigs used in Sintel). */
      // TODO Not implemented
      // pose_slide_apply_props(pso, pfl, "[\""); /* dummy " for texteditor bugs */
    }
  }

  /* depsgraph updates + redraws */
  pose_slide_refresh(C, pso);
}

/* apply() - perform the pose sliding based on weighting various poses */
static void pose_slide_apply(bContext *C, tPoseSlideOp *pso)
{
  tPChanFCurveLink *pfl;

  /* sanitise the frame ranges */
  if (pso->prevFrame == pso->nextFrame) {
    /* move out one step either side */
    pso->prevFrame--;
    pso->nextFrame++;

    for (uint ob_index = 0; ob_index < pso->objects_len; ob_index++) {
      tPoseSlideObject *ob_data = &pso->ob_data_array[ob_index];

      if (!ob_data->valid) {
        continue;
      }

      /* apply NLA mapping corrections so the frame lookups work */
      ob_data->prevFrameF = BKE_nla_tweakedit_remap(
          ob_data->ob->adt, pso->prevFrame, NLATIME_CONVERT_UNMAP);
      ob_data->nextFrameF = BKE_nla_tweakedit_remap(
          ob_data->ob->adt, pso->nextFrame, NLATIME_CONVERT_UNMAP);
    }
  }

  /* for each link, handle each set of transforms */
  for (pfl = pso->pfLinks.first; pfl; pfl = pfl->next) {
    /* valid transforms for each PoseChannel should have been noted already
     * - sliding the pose should be a straightforward exercise for location+rotation,
     *   but rotations get more complicated since we may want to use quaternion blending
     *   for quaternions instead...
     */
    bPoseChannel *pchan = pfl->pchan;

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_LOC) && (pchan->flag & POSE_LOC)) {
      /* calculate these for the 'location' vector, and use location curves */
      pose_slide_apply_vec3(pso, pfl, pchan->loc, "location");
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_SIZE) && (pchan->flag & POSE_SIZE)) {
      /* calculate these for the 'scale' vector, and use scale curves */
      pose_slide_apply_vec3(pso, pfl, pchan->size, "scale");
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_ROT) && (pchan->flag & POSE_ROT)) {
      /* everything depends on the rotation mode */
      if (pchan->rotmode > 0) {
        /* eulers - so calculate these for the 'eul' vector, and use euler_rotation curves */
        pose_slide_apply_vec3(pso, pfl, pchan->eul, "rotation_euler");
      }
      else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
        /* TODO: need to figure out how to do this! */
      }
      else {
        /* quaternions - use quaternion blending */
        pose_slide_apply_quat(pso, pfl);
      }
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_BBONE_SHAPE) && (pchan->flag & POSE_BBONE_SHAPE)) {
      /* bbone properties - they all start a "bbone_" prefix */
      pose_slide_apply_props(pso, pfl, "bbone_");
    }

    if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_PROPS) && (pfl->oldprops)) {
      /* Not strictly a transform, but custom properties contribute
       * to the pose produced in many rigs (e.g. the facial rigs used in Sintel). */
      pose_slide_apply_props(pso, pfl, "[\""); /* dummy " for texteditor bugs */
    }
  }

  /* depsgraph updates + redraws */
  pose_slide_refresh(C, pso);
}

/* perform auto-key-framing after changes were made + confirmed */
static void pose_slide_autoKeyframe(bContext *C, tPoseSlideOp *pso)
{
  /* wrapper around the generic call */
  poseAnim_mapping_autoKeyframe(C, pso->scene, &pso->pfLinks, (float)pso->cframe);
}

/* reset changes made to current pose */
static void pose_slide_reset(tPoseSlideOp *pso)
{
  /* wrapper around the generic call, so that custom stuff can be added later */
  poseAnim_mapping_reset(&pso->pfLinks);
}

/* ------------------------------------ */

/* draw percentage indicator in header */
// TODO: Include hints about locks here...
static void pose_slide_draw_status(tPoseSlideOp *pso)
{
  char status_str[UI_MAX_DRAW_STR];
  char limits_str[UI_MAX_DRAW_STR];
  char axis_str[50];
  char mode_str[32];

  switch (pso->mode) {
    case POSESLIDE_PUSH:
      strcpy(mode_str, TIP_("Push Pose"));
      break;
    case POSESLIDE_RELAX:
      strcpy(mode_str, TIP_("Relax Pose"));
      break;
    case POSESLIDE_BREAKDOWN:
      strcpy(mode_str, TIP_("Breakdown"));
      break;

    default:
      /* unknown */
      strcpy(mode_str, TIP_("Sliding-Tool"));
      break;
  }

  switch (pso->axislock) {
    case PS_LOCK_X:
      BLI_strncpy(axis_str, TIP_("[X]/Y/Z axis only (X to clear)"), sizeof(axis_str));
      break;
    case PS_LOCK_Y:
      BLI_strncpy(axis_str, TIP_("X/[Y]/Z axis only (Y to clear)"), sizeof(axis_str));
      break;
    case PS_LOCK_Z:
      BLI_strncpy(axis_str, TIP_("X/Y/[Z] axis only (Z to clear)"), sizeof(axis_str));
      break;

    default:
      if (ELEM(pso->channels, PS_TFM_LOC, PS_TFM_ROT, PS_TFM_SIZE)) {
        BLI_strncpy(axis_str, TIP_("X/Y/Z = Axis Constraint"), sizeof(axis_str));
      }
      else {
        axis_str[0] = '\0';
      }
      break;
  }

  switch (pso->channels) {
    case PS_TFM_LOC:
      BLI_snprintf(limits_str,
                   sizeof(limits_str),
                   TIP_("[G]/R/S/B/C - Location only (G to clear) | %s"),
                   axis_str);
      break;
    case PS_TFM_ROT:
      BLI_snprintf(limits_str,
                   sizeof(limits_str),
                   TIP_("G/[R]/S/B/C - Rotation only (R to clear) | %s"),
                   axis_str);
      break;
    case PS_TFM_SIZE:
      BLI_snprintf(limits_str,
                   sizeof(limits_str),
                   TIP_("G/R/[S]/B/C - Scale only (S to clear) | %s"),
                   axis_str);
      break;
    case PS_TFM_BBONE_SHAPE:
      BLI_strncpy(limits_str,
                  TIP_("G/R/S/[B]/C - Bendy Bone properties only (B to clear) | %s"),
                  sizeof(limits_str));
      break;
    case PS_TFM_PROPS:
      BLI_strncpy(limits_str,
                  TIP_("G/R/S/B/[C] - Custom Properties only (C to clear) | %s"),
                  sizeof(limits_str));
      break;
    default:
      BLI_strncpy(
          limits_str, TIP_("G/R/S/B/C - Limit to Transform/Property Set"), sizeof(limits_str));
      break;
  }

  if (hasNumInput(&pso->num)) {
    Scene *scene = pso->scene;
    char str_offs[NUM_STR_REP_LEN];

    outputNumInput(&pso->num, str_offs, &scene->unit);

    BLI_snprintf(
        status_str, sizeof(status_str), "%s: %s     |   %s", mode_str, str_offs, limits_str);
  }
  else {
    BLI_snprintf(status_str,
                 sizeof(status_str),
                 "%s: %d %%     |   %s",
                 mode_str,
                 (int)(pso->percentage * 100.0f),
                 limits_str);
  }

  ED_area_status_text(pso->sa, status_str);
}

/* common code for invoke() methods */
static int pose_slide_invoke_common(bContext *C, wmOperator *op, tPoseSlideOp *pso)
{
  tPChanFCurveLink *pfl;
  wmWindow *win = CTX_wm_window(C);

  /* for each link, add all its keyframes to the search tree */
  for (pfl = pso->pfLinks.first; pfl; pfl = pfl->next) {
    LinkData *ld;

    /* do this for each F-Curve */
    for (ld = pfl->fcurves.first; ld; ld = ld->next) {
      FCurve *fcu = (FCurve *)ld->data;
      fcurve_to_keylist(pfl->ob->adt, fcu, &pso->keys, 0);
    }
  }

  /* cancel if no keyframes found... */
  if (pso->keys.root) {
    ActKeyColumn *ak;
    float cframe = (float)pso->cframe;

    /* firstly, check if the current frame is a keyframe... */
    ak = (ActKeyColumn *)BLI_dlrbTree_search_exact(&pso->keys, compare_ak_cfraPtr, &cframe);

    if (ak == NULL) {
      /* current frame is not a keyframe, so search */
      ActKeyColumn *pk = (ActKeyColumn *)BLI_dlrbTree_search_prev(
          &pso->keys, compare_ak_cfraPtr, &cframe);
      ActKeyColumn *nk = (ActKeyColumn *)BLI_dlrbTree_search_next(
          &pso->keys, compare_ak_cfraPtr, &cframe);

      /* new set the frames */
      /* prev frame */
      pso->prevFrame = (pk) ? (pk->cfra) : (pso->cframe - 1);
      RNA_int_set(op->ptr, "prev_frame", pso->prevFrame);
      /* next frame */
      pso->nextFrame = (nk) ? (nk->cfra) : (pso->cframe + 1);
      RNA_int_set(op->ptr, "next_frame", pso->nextFrame);
    }
    else {
      /* current frame itself is a keyframe, so just take keyframes on either side */
      /* prev frame */
      pso->prevFrame = (ak->prev) ? (ak->prev->cfra) : (pso->cframe - 1);
      RNA_int_set(op->ptr, "prev_frame", pso->prevFrame);
      /* next frame */
      pso->nextFrame = (ak->next) ? (ak->next->cfra) : (pso->cframe + 1);
      RNA_int_set(op->ptr, "next_frame", pso->nextFrame);
    }

    /* apply NLA mapping corrections so the frame lookups work */
    for (uint ob_index = 0; ob_index < pso->objects_len; ob_index++) {
      tPoseSlideObject *ob_data = &pso->ob_data_array[ob_index];
      if (ob_data->valid) {
        ob_data->prevFrameF = BKE_nla_tweakedit_remap(
            ob_data->ob->adt, pso->prevFrame, NLATIME_CONVERT_UNMAP);
        ob_data->nextFrameF = BKE_nla_tweakedit_remap(
            ob_data->ob->adt, pso->nextFrame, NLATIME_CONVERT_UNMAP);
      }
    }
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "No keyframes to slide between");
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }

  /* initial apply for operator... */
  /* TODO: need to calculate percentage for initial round too... */
  if (pso->mode != POSESLIDE_PUSH_REST && pso->mode != POSESLIDE_RELAX_REST) {
    pose_slide_apply(C, pso);
  }
  else {
    pose_slide_rest_pose_apply(C, pso);
  }

  /* depsgraph updates + redraws */
  pose_slide_refresh(C, pso);

  /* set cursor to indicate modal */
  WM_cursor_modal_set(win, BC_EW_SCROLLCURSOR);

  /* header print */
  pose_slide_draw_status(pso);

  /* add a modal handler for this operator */
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

/* calculate percentage based on position of mouse (we only use x-axis for now.
 * since this is more convenient for users to do), and store new percentage value
 */
static void pose_slide_mouse_update_percentage(tPoseSlideOp *pso,
                                               wmOperator *op,
                                               const wmEvent *event)
{
  pso->percentage = (event->x - pso->ar->winrct.xmin) / ((float)pso->ar->winx);
  RNA_float_set(op->ptr, "percentage", pso->percentage);
}

/* handle an event to toggle channels mode */
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
  pso->axislock = 0;
  RNA_enum_set(op->ptr, "axis_lock", pso->axislock);
}

/* handle an event to toggle axis locks - returns whether any change in state is needed */
static bool pose_slide_toggle_axis_locks(wmOperator *op,
                                         tPoseSlideOp *pso,
                                         ePoseSlide_AxisLock axis)
{
  /* Axis can only be set when a transform is set - it doesn't make sense otherwise */
  if (ELEM(pso->channels, PS_TFM_ALL, PS_TFM_BBONE_SHAPE, PS_TFM_PROPS)) {
    pso->axislock = 0;
    RNA_enum_set(op->ptr, "axis_lock", pso->axislock);
    return false;
  }

  /* Turn on or off? */
  if (pso->axislock == axis) {
    /* Already limiting on this axis, so turn off */
    pso->axislock = 0;
  }
  else {
    /* Only this axis */
    pso->axislock = axis;
  }
  RNA_enum_set(op->ptr, "axis_lock", pso->axislock);

  /* Setting changed, so pose update is needed */
  return true;
}

/* common code for modal() */
static int pose_slide_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tPoseSlideOp *pso = op->customdata;
  wmWindow *win = CTX_wm_window(C);
  bool do_pose_update = false;

  const bool has_numinput = hasNumInput(&pso->num);

  switch (event->type) {
    case LEFTMOUSE: /* confirm */
    case RETKEY:
    case PADENTER: {
      if (event->val == KM_PRESS) {
        /* return to normal cursor and header status */
        ED_area_status_text(pso->sa, NULL);
        WM_cursor_modal_restore(win);

        /* insert keyframes as required... */
        pose_slide_autoKeyframe(C, pso);
        pose_slide_exit(op);

        /* done! */
        return OPERATOR_FINISHED;
      }
      break;
    }

    case ESCKEY: /* cancel */
    case RIGHTMOUSE: {
      if (event->val == KM_PRESS) {
        /* return to normal cursor and header status */
        ED_area_status_text(pso->sa, NULL);
        WM_cursor_modal_restore(win);

        /* reset transforms back to original state */
        pose_slide_reset(pso);

        /* depsgraph updates + redraws */
        pose_slide_refresh(C, pso);

        /* clean up temp data */
        pose_slide_exit(op);

        /* canceled! */
        return OPERATOR_CANCELLED;
      }
      break;
    }

    /* Percentage Chane... */
    case MOUSEMOVE: /* calculate new position */
    {
      /* only handle mousemove if not doing numinput */
      if (has_numinput == false) {
        /* update percentage based on position of mouse */
        pose_slide_mouse_update_percentage(pso, op, event);

        /* update pose to reflect the new values (see below) */
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
        value = pso->percentage * 100.0f;
        applyNumInput(&pso->num, &value);

        pso->percentage = value / 100.0f;
        CLAMP(pso->percentage, 0.0f, 1.0f);
        RNA_float_set(op->ptr, "percentage", pso->percentage);

        /* Update pose to reflect the new values (see below) */
        do_pose_update = true;
        break;
      }
      else if (event->val == KM_PRESS) {
        switch (event->type) {
          /* Transform Channel Limits  */
          /* XXX: Replace these hardcoded hotkeys with a modalmap that can be customised */
          case GKEY: /* Location */
          {
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_LOC);
            do_pose_update = true;
            break;
          }
          case RKEY: /* Rotation */
          {
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_ROT);
            do_pose_update = true;
            break;
          }
          case SKEY: /* Scale */
          {
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_SIZE);
            do_pose_update = true;
            break;
          }
          case BKEY: /* Bendy Bones */
          {
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_BBONE_SHAPE);
            do_pose_update = true;
            break;
          }
          case CKEY: /* Custom Properties */
          {
            pose_slide_toggle_channels_mode(op, pso, PS_TFM_PROPS);
            do_pose_update = true;
            break;
          }

          /* Axis Locks */
          /* XXX: Hardcoded... */
          case XKEY: {
            if (pose_slide_toggle_axis_locks(op, pso, PS_LOCK_X)) {
              do_pose_update = true;
            }
            break;
          }
          case YKEY: {
            if (pose_slide_toggle_axis_locks(op, pso, PS_LOCK_Y)) {
              do_pose_update = true;
            }
            break;
          }
          case ZKEY: {
            if (pose_slide_toggle_axis_locks(op, pso, PS_LOCK_Z)) {
              do_pose_update = true;
            }
            break;
          }

          default: /* Some other unhandled key... */
            break;
        }
      }
      else {
        /* unhandled event - maybe it was some view manip? */
        /* allow to pass through */
        return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
      }
    }
  }

  /* Perform pose updates - in response to some user action
   * (e.g. pressing a key or moving the mouse). */
  if (do_pose_update) {
    /* update percentage indicator in header */
    pose_slide_draw_status(pso);

    /* reset transforms (to avoid accumulation errors) */
    pose_slide_reset(pso);

    /* apply... */
    if (pso->mode != POSESLIDE_PUSH_REST && pso->mode != POSESLIDE_RELAX_REST) {
      pose_slide_apply(C, pso);
    }
    else {
      pose_slide_rest_pose_apply(C, pso);
    }
  }

  /* still running... */
  return OPERATOR_RUNNING_MODAL;
}

/* common code for cancel() */
static void pose_slide_cancel(bContext *UNUSED(C), wmOperator *op)
{
  /* cleanup and done */
  pose_slide_exit(op);
}

/* common code for exec() methods */
static int pose_slide_exec_common(bContext *C, wmOperator *op, tPoseSlideOp *pso)
{
  /* settings should have been set up ok for applying, so just apply! */
  if (pso->mode != POSESLIDE_PUSH_REST && pso->mode != POSESLIDE_RELAX_REST) {
    pose_slide_apply(C, pso);
  }
  else {
    pose_slide_rest_pose_apply(C, pso);
  }

  /* insert keyframes if needed */
  pose_slide_autoKeyframe(C, pso);

  /* cleanup and done */
  pose_slide_exit(op);

  return OPERATOR_FINISHED;
}

/* common code for defining RNA properties */
/* TODO: Skip save on these? */
static void pose_slide_opdef_properties(wmOperatorType *ot)
{
  RNA_def_float_percentage(ot->srna,
                           "percentage",
                           0.5f,
                           0.0f,
                           1.0f,
                           "Percentage",
                           "Weighting factor for which keyframe is favored more",
                           0.0,
                           1.0);

  RNA_def_int(ot->srna,
              "prev_frame",
              0,
              MINAFRAME,
              MAXFRAME,
              "Previous Keyframe",
              "Frame number of keyframe immediately before the current frame",
              0,
              50);
  RNA_def_int(ot->srna,
              "next_frame",
              0,
              MINAFRAME,
              MAXFRAME,
              "Next Keyframe",
              "Frame number of keyframe immediately after the current frame",
              0,
              50);

  RNA_def_enum(ot->srna,
               "channels",
               prop_channels_types,
               PS_TFM_ALL,
               "Channels",
               "Set of properties that are affected");
  RNA_def_enum(ot->srna,
               "axis_lock",
               prop_axis_lock_types,
               0,
               "Axis Lock",
               "Transform axis to restrict effects to");
}

/* ------------------------------------ */

/* invoke() - for 'push from breakdown' mode */
static int pose_slide_push_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tPoseSlideOp *pso;

  /* initialize data  */
  if (pose_slide_init(C, op, POSESLIDE_PUSH) == 0) {
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }
  else {
    pso = op->customdata;
  }

  /* initialise percentage so that it won't pop on first mouse move */
  pose_slide_mouse_update_percentage(pso, op, event);

  /* do common setup work */
  return pose_slide_invoke_common(C, op, pso);
}

/* exec() - for push */
static int pose_slide_push_exec(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso;

  /* initialize data (from RNA-props) */
  if (pose_slide_init(C, op, POSESLIDE_PUSH) == 0) {
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }
  else {
    pso = op->customdata;
  }

  /* do common exec work */
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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_USE_EVAL_DATA;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */

/* invoke() - for 'relax to breakdown' mode */
static int pose_slide_relax_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tPoseSlideOp *pso;

  /* initialize data  */
  if (pose_slide_init(C, op, POSESLIDE_RELAX) == 0) {
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }
  else {
    pso = op->customdata;
  }

  /* initialise percentage so that it won't pop on first mouse move */
  pose_slide_mouse_update_percentage(pso, op, event);

  /* do common setup work */
  return pose_slide_invoke_common(C, op, pso);
}

/* exec() - for relax */
static int pose_slide_relax_exec(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso;

  /* initialize data (from RNA-props) */
  if (pose_slide_init(C, op, POSESLIDE_RELAX) == 0) {
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }
  else {
    pso = op->customdata;
  }

  /* do common exec work */
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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_USE_EVAL_DATA;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */
/* invoke() - for 'push from rest pose' mode */
static int pose_slide_push_rest_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tPoseSlideOp *pso;

  /* initialize data  */
  if (pose_slide_init(C, op, POSESLIDE_PUSH_REST) == 0) {
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }
  else {
    pso = op->customdata;
  }

  /* initialise percentage so that it won't pop on first mouse move */
  pose_slide_mouse_update_percentage(pso, op, event);

  /* do common setup work */
  return pose_slide_invoke_common(C, op, pso);
}

/* exec() - for push */
static int pose_slide_push_rest_exec(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso;

  /* initialize data (from RNA-props) */
  if (pose_slide_init(C, op, POSESLIDE_PUSH_REST) == 0) {
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }
  else {
    pso = op->customdata;
  }

  /* do common exec work */
  return pose_slide_exec_common(C, op, pso);
}

void POSE_OT_push_rest(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Push Pose from Rest Pose";
  ot->idname = "POSE_OT_push_rest";
  ot->description = "Push the current pose further away from the rest pose";

  /* callbacks */
  ot->exec = pose_slide_push_rest_exec;
  ot->invoke = pose_slide_push_rest_invoke;
  ot->modal = pose_slide_modal;
  ot->cancel = pose_slide_cancel;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_USE_EVAL_DATA;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */

/* invoke() - for 'relax' mode */
static int pose_slide_relax_rest_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tPoseSlideOp *pso;

  /* initialize data  */
  if (pose_slide_init(C, op, POSESLIDE_RELAX_REST) == 0) {
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }
  else {
    pso = op->customdata;
  }

  /* initialise percentage so that it won't pop on first mouse move */
  pose_slide_mouse_update_percentage(pso, op, event);

  /* do common setup work */
  return pose_slide_invoke_common(C, op, pso);
}

/* exec() - for relax */
static int pose_slide_relax_rest_exec(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso;

  /* initialize data (from RNA-props) */
  if (pose_slide_init(C, op, POSESLIDE_RELAX_REST) == 0) {
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }
  else {
    pso = op->customdata;
  }

  /* do common exec work */
  return pose_slide_exec_common(C, op, pso);
}

void POSE_OT_relax_rest(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Relax Pose to Rest Pose";
  ot->idname = "POSE_OT_relax_rest";
  ot->description = "Make the current pose more similar to the rest pose";

  /* callbacks */
  ot->exec = pose_slide_relax_rest_exec;
  ot->invoke = pose_slide_relax_rest_invoke;
  ot->modal = pose_slide_modal;
  ot->cancel = pose_slide_cancel;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_USE_EVAL_DATA;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* ........................ */

/* invoke() - for 'breakdown' mode */
static int pose_slide_breakdown_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tPoseSlideOp *pso;

  /* initialize data  */
  if (pose_slide_init(C, op, POSESLIDE_BREAKDOWN) == 0) {
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }
  else {
    pso = op->customdata;
  }

  /* initialise percentage so that it won't pop on first mouse move */
  pose_slide_mouse_update_percentage(pso, op, event);

  /* do common setup work */
  return pose_slide_invoke_common(C, op, pso);
}

/* exec() - for breakdown */
static int pose_slide_breakdown_exec(bContext *C, wmOperator *op)
{
  tPoseSlideOp *pso;

  /* initialize data (from RNA-props) */
  if (pose_slide_init(C, op, POSESLIDE_BREAKDOWN) == 0) {
    pose_slide_exit(op);
    return OPERATOR_CANCELLED;
  }
  else {
    pso = op->customdata;
  }

  /* do common exec work */
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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_USE_EVAL_DATA;

  /* Properties */
  pose_slide_opdef_properties(ot);
}

/* **************************************************** */
/* B) Pose Propagate */

/* "termination conditions" - i.e. when we stop */
typedef enum ePosePropagate_Termination {
  /* stop after the current hold ends */
  POSE_PROPAGATE_SMART_HOLDS = 0,
  /* only do on the last keyframe */
  POSE_PROPAGATE_LAST_KEY,
  /* stop after the next keyframe */
  POSE_PROPAGATE_NEXT_KEY,
  /* stop after the specified frame */
  POSE_PROPAGATE_BEFORE_FRAME,
  /* stop when we run out of keyframes */
  POSE_PROPAGATE_BEFORE_END,

  /* only do on keyframes that are selected */
  POSE_PROPAGATE_SELECTED_KEYS,
  /* only do on the frames where markers are selected */
  POSE_PROPAGATE_SELECTED_MARKERS,
} ePosePropagate_Termination;

/* Termination data needed for some modes -
 * assumes only one of these entries will be needed at a time. */
typedef union tPosePropagate_ModeData {
  /* smart holds + before frame: frame number to stop on */
  float end_frame;

  /* selected markers: listbase for CfraElem's marking these frames */
  ListBase sel_markers;
} tPosePropagate_ModeData;

/* --------------------------------- */

/* get frame on which the "hold" for the bone ends
 * XXX: this may not really work that well if a bone moves on some channels and not others
 *      if this happens to be a major issue, scrap this, and just make this happen
 *      independently per F-Curve
 */
static float pose_propagate_get_boneHoldEndFrame(tPChanFCurveLink *pfl, float startFrame)
{
  DLRBT_Tree keys;

  Object *ob = pfl->ob;
  AnimData *adt = ob->adt;
  LinkData *ld;
  float endFrame = startFrame;

  /* set up optimized data-structures for searching for relevant keyframes + holds */
  BLI_dlrbTree_init(&keys);

  for (ld = pfl->fcurves.first; ld; ld = ld->next) {
    FCurve *fcu = (FCurve *)ld->data;
    fcurve_to_keylist(adt, fcu, &keys, 0);
  }

  /* find the long keyframe (i.e. hold), and hence obtain the endFrame value
   * - the best case would be one that starts on the frame itself
   */
  ActKeyColumn *ab = (ActKeyColumn *)BLI_dlrbTree_search_exact(
      &keys, compare_ak_cfraPtr, &startFrame);

  /* There are only two cases for no-exact match:
   *  1) the current frame is just before another key but not on a key itself
   *  2) the current frame is on a key, but that key doesn't link to the next
   *
   * If we've got the first case, then we can search for another block,
   * otherwise forget it, as we'd be overwriting some valid data.
   */
  if (ab == NULL) {
    /* we've got case 1, so try the one after */
    ab = (ActKeyColumn *)BLI_dlrbTree_search_next(&keys, compare_ak_cfraPtr, &startFrame);

    if ((actkeyblock_get_valid_hold(ab) & ACTKEYBLOCK_FLAG_STATIC_HOLD) == 0) {
      /* try the block before this frame then as last resort */
      ab = (ActKeyColumn *)BLI_dlrbTree_search_prev(&keys, compare_ak_cfraPtr, &startFrame);
    }
  }

  /* whatever happens, stop searching now... */
  if ((actkeyblock_get_valid_hold(ab) & ACTKEYBLOCK_FLAG_STATIC_HOLD) == 0) {
    /* restrict range to just the frame itself
     * i.e. everything is in motion, so no holds to safely overwrite
     */
    ab = NULL;
  }

  /* check if we can go any further than we've already gone */
  if (ab) {
    /* go to next if it is also valid and meets "extension" criteria */
    while (ab->next) {
      ActKeyColumn *abn = ab->next;

      /* must be valid */
      if ((actkeyblock_get_valid_hold(abn) & ACTKEYBLOCK_FLAG_STATIC_HOLD) == 0) {
        break;
      }
      /* should have the same number of curves */
      if (ab->totblock != abn->totblock) {
        break;
      }

      /* we can extend the bounds to the end of this "next" block now */
      ab = abn;
    }

    /* end frame can now take the value of the end of the block */
    endFrame = ab->next->cfra;
  }

  /* free temp memory */
  BLI_dlrbTree_free(&keys);

  /* return the end frame we've found */
  return endFrame;
}

/* get reference value from F-Curve using RNA */
static bool pose_propagate_get_refVal(Object *ob, FCurve *fcu, float *value)
{
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop;
  bool found = false;

  /* base pointer is always the object -> id_ptr */
  RNA_id_pointer_create(&ob->id, &id_ptr);

  /* resolve the property... */
  if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
    if (RNA_property_array_check(prop)) {
      /* array */
      if (fcu->array_index < RNA_property_array_length(&ptr, prop)) {
        found = true;
        switch (RNA_property_type(prop)) {
          case PROP_BOOLEAN:
            *value = (float)RNA_property_boolean_get_index(&ptr, prop, fcu->array_index);
            break;
          case PROP_INT:
            *value = (float)RNA_property_int_get_index(&ptr, prop, fcu->array_index);
            break;
          case PROP_FLOAT:
            *value = RNA_property_float_get_index(&ptr, prop, fcu->array_index);
            break;
          default:
            found = false;
            break;
        }
      }
    }
    else {
      /* not an array */
      found = true;
      switch (RNA_property_type(prop)) {
        case PROP_BOOLEAN:
          *value = (float)RNA_property_boolean_get(&ptr, prop);
          break;
        case PROP_INT:
          *value = (float)RNA_property_int_get(&ptr, prop);
          break;
        case PROP_ENUM:
          *value = (float)RNA_property_enum_get(&ptr, prop);
          break;
        case PROP_FLOAT:
          *value = RNA_property_float_get(&ptr, prop);
          break;
        default:
          found = false;
          break;
      }
    }
  }

  return found;
}

/* propagate just works along each F-Curve in turn */
static void pose_propagate_fcurve(
    wmOperator *op, Object *ob, FCurve *fcu, float startFrame, tPosePropagate_ModeData modeData)
{
  const int mode = RNA_enum_get(op->ptr, "mode");

  BezTriple *bezt;
  float refVal = 0.0f;
  bool keyExists;
  int i, match;
  short first = 1;

  /* skip if no keyframes to edit */
  if ((fcu->bezt == NULL) || (fcu->totvert < 2)) {
    return;
  }

  /* find the reference value from bones directly, which means that the user
   * doesn't need to firstly keyframe the pose (though this doesn't mean that
   * they can't either)
   */
  if (!pose_propagate_get_refVal(ob, fcu, &refVal)) {
    return;
  }

  /* find the first keyframe to start propagating from
   * - if there's a keyframe on the current frame, we probably want to save this value there too
   *   since it may be as of yet unkeyed
   * - if starting before the starting frame, don't touch the key, as it may have had some valid
   *   values
   * - if only doing selected keyframes, start from the first one
   */
  if (mode != POSE_PROPAGATE_SELECTED_KEYS) {
    match = binarysearch_bezt_index(fcu->bezt, startFrame, fcu->totvert, &keyExists);

    if (fcu->bezt[match].vec[1][0] < startFrame) {
      i = match + 1;
    }
    else {
      i = match;
    }
  }
  else {
    /* selected - start from first keyframe */
    i = 0;
  }

  for (bezt = &fcu->bezt[i]; i < fcu->totvert; i++, bezt++) {
    /* additional termination conditions based on the operator 'mode' property go here... */
    if (ELEM(mode, POSE_PROPAGATE_BEFORE_FRAME, POSE_PROPAGATE_SMART_HOLDS)) {
      /* stop if keyframe is outside the accepted range */
      if (bezt->vec[1][0] > modeData.end_frame) {
        break;
      }
    }
    else if (mode == POSE_PROPAGATE_NEXT_KEY) {
      /* stop after the first keyframe has been processed */
      if (first == 0) {
        break;
      }
    }
    else if (mode == POSE_PROPAGATE_LAST_KEY) {
      /* only affect this frame if it will be the last one */
      if (i != (fcu->totvert - 1)) {
        continue;
      }
    }
    else if (mode == POSE_PROPAGATE_SELECTED_MARKERS) {
      /* only allow if there's a marker on this frame */
      CfraElem *ce = NULL;

      /* stop on matching marker if there is one */
      for (ce = modeData.sel_markers.first; ce; ce = ce->next) {
        if (ce->cfra == round_fl_to_int(bezt->vec[1][0])) {
          break;
        }
      }

      /* skip this keyframe if no marker */
      if (ce == NULL) {
        continue;
      }
    }
    else if (mode == POSE_PROPAGATE_SELECTED_KEYS) {
      /* only allow if this keyframe is already selected - skip otherwise */
      if (BEZT_ISSEL_ANY(bezt) == 0) {
        continue;
      }
    }

    /* just flatten handles, since values will now be the same either side... */
    /* TODO: perhaps a fade-out modulation of the value is required here (optional once again)? */
    bezt->vec[0][1] = bezt->vec[1][1] = bezt->vec[2][1] = refVal;

    /* select keyframe to indicate that it's been changed */
    bezt->f2 |= SELECT;
    first = 0;
  }
}

/* --------------------------------- */

static int pose_propagate_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  ListBase pflinks = {NULL, NULL};
  tPChanFCurveLink *pfl;

  tPosePropagate_ModeData modeData;
  const int mode = RNA_enum_get(op->ptr, "mode");

  /* isolate F-Curves related to the selected bones */
  poseAnim_mapping_get(C, &pflinks);

  if (BLI_listbase_is_empty(&pflinks)) {
    /* There is a change the reason the list is empty is
     * that there is no valid object to propagate poses for.
     * This is very unlikely though, so we focus on the most likely issue. */
    BKE_report(op->reports, RPT_ERROR, "No keyframed poses to propagate to");
    return OPERATOR_CANCELLED;
  }

  /* mode-specific data preprocessing (requiring no access to curves) */
  if (mode == POSE_PROPAGATE_SELECTED_MARKERS) {
    /* get a list of selected markers */
    ED_markers_make_cfra_list(&scene->markers, &modeData.sel_markers, SELECT);
  }
  else {
    /* assume everything else wants endFrame */
    modeData.end_frame = RNA_float_get(op->ptr, "end_frame");
  }

  /* for each bone, perform the copying required */
  for (pfl = pflinks.first; pfl; pfl = pfl->next) {
    LinkData *ld;

    /* mode-specific data preprocessing (requiring access to all curves) */
    if (mode == POSE_PROPAGATE_SMART_HOLDS) {
      /* we store in endFrame the end frame of the "long keyframe" (i.e. a held value) starting
       * from the keyframe that occurs after the current frame
       */
      modeData.end_frame = pose_propagate_get_boneHoldEndFrame(pfl, (float)CFRA);
    }

    /* go through propagating pose to keyframes, curve by curve */
    for (ld = pfl->fcurves.first; ld; ld = ld->next) {
      pose_propagate_fcurve(op, pfl->ob, (FCurve *)ld->data, (float)CFRA, modeData);
    }
  }

  /* free temp data */
  poseAnim_mapping_free(&pflinks);

  if (mode == POSE_PROPAGATE_SELECTED_MARKERS) {
    BLI_freelistN(&modeData.sel_markers);
  }

  /* updates + notifiers */
  FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob) {
    poseAnim_mapping_refresh(C, scene, ob);
  }
  FOREACH_OBJECT_IN_MODE_END;

  return OPERATOR_FINISHED;
}

/* --------------------------------- */

void POSE_OT_propagate(wmOperatorType *ot)
{
  static const EnumPropertyItem terminate_items[] = {
      {POSE_PROPAGATE_SMART_HOLDS,
       "WHILE_HELD",
       0,
       "While Held",
       "Propagate pose to all keyframes after current frame that don't change (Default behavior)"},
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
      {0, NULL, 0, NULL, NULL},
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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_USE_EVAL_DATA;

  /* properties */
  /* TODO: add "fade out" control for tapering off amount of propagation as time goes by? */
  ot->prop = RNA_def_enum(ot->srna,
                          "mode",
                          terminate_items,
                          POSE_PROPAGATE_SMART_HOLDS,
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
