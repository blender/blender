/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Mirror/Symmetry functions applying to actions.
 */

#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_fcurve.hh"

#include "ANIM_action_legacy.hh"

#include "DEG_depsgraph.hh"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Flip the Action (Armature/Pose Objects)
 *
 * This flips the action using the rest pose (not the evaluated pose).
 *
 * Details:
 *
 * - Key-frames are modified in-place, creating new key-frames is not yet supported.
 *   That could be useful if a user for example only has 2x rotation channels set.
 *   In practice users typically keyframe all rotation channels or none.
 *
 * - F-Curve modifiers are disabled for evaluation,
 *   so the values written back to the keyframes don't include modifier offsets.
 *
 * - Sub-frame key-frames aren't supported,
 *   this could be added if needed without much trouble.
 *
 * - F-Curves must have a #FCurve.bezt array (sampled curves aren't supported).
 * \{ */

/**
 * This structure is created for each pose channels F-Curve,
 * an action be evaluated and stored in `fcurve_eval`,
 * with the mirrored values written into `bezt_array`.
 *
 * Store F-Curve evaluated values, constructed with a sorted array of rounded keyed-frames,
 * passed to #action_flip_pchan_cache_init.
 */
struct FCurve_KeyCache {
  /**
   * When nullptr, ignore this channel.
   */
  FCurve *fcurve;
  /**
   * Cached evaluated F-Curve values (without modifiers).
   */
  float *fcurve_eval;
  /**
   * Cached #FCurve.bezt values, nullptr when no key-frame exists on this frame.
   *
   * \note The case where two keyframes round to the same frame isn't supported.
   * In this case only the first will be used.
   */
  BezTriple **bezt_array;
};

/**
 * Assign `fkc` path, using a `path` lookup for a single value.
 */
static void action_flip_pchan_cache_fcurve_assign_value(FCurve_KeyCache *fkc,
                                                        int index,
                                                        const char *path,
                                                        FCurvePathCache *fcache)
{
  FCurve *fcu = BKE_fcurve_pathcache_find(fcache, path, index);
  if (fcu && fcu->bezt) {
    fkc->fcurve = fcu;
  }
}

/**
 * Assign #FCurve_KeyCache.fcurve path, using a `path` lookup for an array.
 */
static void action_flip_pchan_cache_fcurve_assign_array(FCurve_KeyCache *fkc,
                                                        int fkc_len,
                                                        const char *path,
                                                        FCurvePathCache *fcache)
{
  FCurve **fcurves = static_cast<FCurve **>(alloca(sizeof(*fcurves) * fkc_len));
  if (BKE_fcurve_pathcache_find_array(fcache, path, fcurves, fkc_len)) {
    for (int i = 0; i < fkc_len; i++) {
      if (fcurves[i] && fcurves[i]->bezt) {
        fkc[i].fcurve = fcurves[i];
      }
    }
  }
}

/**
 * Fill in pose channel cache for each frame in `keyed_frames`.
 *
 * \param keyed_frames: An array of keyed_frames to evaluate,
 * note that each frame is rounded to the nearest int.
 * \param keyed_frames_len: The length of the `keyed_frames` array.
 */
static void action_flip_pchan_cache_init(FCurve_KeyCache *fkc,
                                         const float *keyed_frames,
                                         int keyed_frames_len)
{
  BLI_assert(fkc->fcurve != nullptr);

  /* Cache the F-Curve values for `keyed_frames`. */
  const int fcurve_flag = fkc->fcurve->flag;
  fkc->fcurve->flag |= FCURVE_MOD_OFF;
  fkc->fcurve_eval = MEM_malloc_arrayN<float>(size_t(keyed_frames_len), __func__);
  for (int frame_index = 0; frame_index < keyed_frames_len; frame_index++) {
    const float evaltime = keyed_frames[frame_index];
    fkc->fcurve_eval[frame_index] = evaluate_fcurve_only_curve(fkc->fcurve, evaltime);
  }
  fkc->fcurve->flag = fcurve_flag;

  /* Cache the #BezTriple for `keyed_frames`, or leave as nullptr. */
  fkc->bezt_array = MEM_malloc_arrayN<BezTriple *>(size_t(keyed_frames_len), __func__);
  BezTriple *bezt = fkc->fcurve->bezt;
  BezTriple *bezt_end = fkc->fcurve->bezt + fkc->fcurve->totvert;

  int frame_index = 0;
  while (frame_index < keyed_frames_len) {
    const float evaltime = keyed_frames[frame_index];
    const float bezt_time = roundf(bezt->vec[1][0]);
    if (bezt_time > evaltime) {
      fkc->bezt_array[frame_index++] = nullptr;
    }
    else {
      if (bezt_time == evaltime) {
        fkc->bezt_array[frame_index++] = bezt;
      }
      bezt++;
      if (bezt == bezt_end) {
        break;
      }
    }
  }
  /* Clear remaining unset keyed_frames (if-any). */
  while (frame_index < keyed_frames_len) {
    fkc->bezt_array[frame_index++] = nullptr;
  }
}

/**
 */
static void action_flip_pchan(Object *ob_arm, const bPoseChannel *pchan, FCurvePathCache *fcache)
{
  /* Begin F-Curve pose channel value extraction. */
  /* Use a fixed buffer size as it's known this can only be at most:
   * `pose.bones["{MAXBONENAME}"].rotation_quaternion`. */
  char path_xform[256];
  char pchan_name_esc[sizeof(pchan->name) * 2];
  BLI_str_escape(pchan_name_esc, pchan->name, sizeof(pchan_name_esc));
  const int path_xform_prefix_len = SNPRINTF_UTF8(
      path_xform, "pose.bones[\"%s\"]", pchan_name_esc);
  char *path_xform_suffix = path_xform + path_xform_prefix_len;
  const int path_xform_suffix_maxncpy = sizeof(path_xform) - path_xform_prefix_len;

  /* Lookup and assign all available #FCurve channels,
   * unavailable channels are left nullptr. */

  /**
   * Structure to store transformation F-Curves corresponding to a pose bones transformation.
   * Match struct member names from #bPoseChannel so macros avoid repetition.
   *
   * \note There is no need to read values unless they influence the 4x4 transform matrix,
   * and no need to write values back unless they would be changed by a modified matrix.
   * So `rotmode` needs to be read, but doesn't need to be written back to.
   *
   * Most bendy-bone settings don't need to be included either, flipping their RNA paths is enough.
   * Although the X/Y settings could make sense to transform, in practice it would only
   * work well if the rotation happened to swap X/Y alignment, leave this for now.
   */
  struct {
    FCurve_KeyCache loc[3], eul[3], quat[4], rotAxis[3], rotAngle, scale[3], rotmode;
  } fkc_pchan = {{{nullptr}}};

#define FCURVE_ASSIGN_VALUE(id, path_test_suffix, index) \
  BLI_strncpy(path_xform_suffix, path_test_suffix, path_xform_suffix_maxncpy); \
  action_flip_pchan_cache_fcurve_assign_value(&fkc_pchan.id, index, path_xform, fcache)

#define FCURVE_ASSIGN_ARRAY(id, path_test_suffix) \
  BLI_strncpy(path_xform_suffix, path_test_suffix, path_xform_suffix_maxncpy); \
  action_flip_pchan_cache_fcurve_assign_array( \
      fkc_pchan.id, ARRAY_SIZE(fkc_pchan.id), path_xform, fcache)

  FCURVE_ASSIGN_ARRAY(loc, ".location");
  FCURVE_ASSIGN_ARRAY(eul, ".rotation_euler");
  FCURVE_ASSIGN_ARRAY(quat, ".rotation_quaternion");
  FCURVE_ASSIGN_ARRAY(rotAxis, ".rotation_axis_angle");
  FCURVE_ASSIGN_VALUE(rotAngle, ".rotation_axis_angle", 3);
  FCURVE_ASSIGN_ARRAY(scale, ".scale");
  FCURVE_ASSIGN_VALUE(rotmode, ".rotation_mode", 0);

#undef FCURVE_ASSIGN_VALUE
#undef FCURVE_ASSIGN_ARRAY

/* Array of F-Curves, for convenient access. */
#define FCURVE_CHANNEL_LEN (sizeof(fkc_pchan) / sizeof(FCurve_KeyCache))
  FCurve *fcurve_array[FCURVE_CHANNEL_LEN];
  int fcurve_array_len = 0;

  for (int chan = 0; chan < FCURVE_CHANNEL_LEN; chan++) {
    FCurve_KeyCache *fkc = (FCurve_KeyCache *)(&fkc_pchan) + chan;
    if (fkc->fcurve != nullptr) {
      fcurve_array[fcurve_array_len++] = fkc->fcurve;
    }
  }

  /* If this pose has no transform channels, there is nothing to do. */
  if (fcurve_array_len == 0) {
    return;
  }

  /* Calculate an array of frames used by any of the key-frames in `fcurve_array`. */
  int keyed_frames_len;
  const float *keyed_frames = BKE_fcurves_calc_keyed_frames(
      fcurve_array, fcurve_array_len, &keyed_frames_len);

  /* Initialize the pose channel curve cache from the F-Curve. */
  for (int chan = 0; chan < FCURVE_CHANNEL_LEN; chan++) {
    FCurve_KeyCache *fkc = (FCurve_KeyCache *)(&fkc_pchan) + chan;
    if (fkc->fcurve == nullptr) {
      continue;
    }
    action_flip_pchan_cache_init(fkc, keyed_frames, keyed_frames_len);
  }

  /* X-axis flipping matrix. */
  float flip_mtx[4][4];
  unit_m4(flip_mtx);
  flip_mtx[0][0] = -1;

  bPoseChannel *pchan_flip = nullptr;
  char pchan_name_flip[MAXBONENAME];
  BLI_string_flip_side_name(pchan_name_flip, pchan->name, false, sizeof(pchan_name_flip));
  if (!STREQ(pchan_name_flip, pchan->name)) {
    pchan_flip = BKE_pose_channel_find_name(ob_arm->pose, pchan_name_flip);
  }

  float arm_mat_inv[4][4];
  invert_m4_m4(arm_mat_inv, pchan_flip ? pchan_flip->bone->arm_mat : pchan->bone->arm_mat);

  /* Now flip the transformation & write it back to the F-Curves in `fkc_pchan`. */

  for (int frame_index = 0; frame_index < keyed_frames_len; frame_index++) {

    /* Temporary pose channel to write values into,
     * using the `fkc_pchan` values, falling back to the values in the pose channel. */
    bPoseChannel pchan_temp = blender::dna::shallow_copy(*pchan);

/* Load the values into the channel. */
#define READ_VALUE_FLT(id) \
  if (fkc_pchan.id.fcurve_eval != nullptr) { \
    pchan_temp.id = fkc_pchan.id.fcurve_eval[frame_index]; \
  } \
  ((void)0)

#define READ_VALUE_INT(id) \
  if (fkc_pchan.id.fcurve_eval != nullptr) { \
    pchan_temp.id = floorf(fkc_pchan.id.fcurve_eval[frame_index] + 0.5f); \
  } \
  ((void)0)

#define READ_ARRAY_FLT(id) \
  for (int i = 0; i < ARRAY_SIZE(pchan_temp.id); i++) { \
    READ_VALUE_FLT(id[i]); \
  } \
  ((void)0)

    READ_ARRAY_FLT(loc);
    READ_ARRAY_FLT(eul);
    READ_ARRAY_FLT(quat);
    READ_ARRAY_FLT(rotAxis);
    READ_VALUE_FLT(rotAngle);
    READ_ARRAY_FLT(scale);
    READ_VALUE_INT(rotmode);

#undef READ_ARRAY_FLT
#undef READ_VALUE_FLT
#undef READ_VALUE_INT

    float chan_mat[4][4];
    BKE_pchan_to_mat4(&pchan_temp, chan_mat);

    /* Move to the pose-space. */
    mul_m4_m4m4(chan_mat, pchan->bone->arm_mat, chan_mat);

    /* Flip the matrix. */
    mul_m4_m4m4(chan_mat, chan_mat, flip_mtx);
    mul_m4_m4m4(chan_mat, flip_mtx, chan_mat);

    /* Move back to bone-space space, using the flipped bone if it exists. */
    mul_m4_m4m4(chan_mat, arm_mat_inv, chan_mat);

    /* The rest pose having an X-axis that is not mapping to a left/right direction (so aligned
     * with the Y or Z axis) creates issues when flipping the pose. Instead of a negative scale on
     * the X-axis, it turns into a 180 degree rotation over the Y-axis.
     * This has only been observed with bones that can't be flipped,
     * hence the check for `pchan_flip`. */
    const float unit_x[3] = {1.0f, 0.0f, 0.0f};
    const bool is_x_axis_orthogonal = (pchan_flip == nullptr) &&
                                      (fabsf(dot_v3v3(pchan->bone->arm_mat[0], unit_x)) <= 1e-6f);
    if (is_x_axis_orthogonal) {
      /* Matrix needs to flip both the X and Z axes to come out right. */
      float extra_mat[4][4] = {
          {-1.0f, 0.0f, 0.0f, 0.0f},
          {0.0f, 1.0f, 0.0f, 0.0f},
          {0.0f, 0.0f, -1.0f, 0.0f},
          {0.0f, 0.0f, 0.0f, 1.0f},
      };
      mul_m4_m4m4(chan_mat, extra_mat, chan_mat);
    }

    BKE_pchan_apply_mat4(&pchan_temp, chan_mat, false);

/* Write the values back to the F-Curves. */
#define WRITE_VALUE_FLT(id) \
  if (fkc_pchan.id.fcurve_eval != nullptr) { \
    BezTriple *bezt = fkc_pchan.id.bezt_array[frame_index]; \
    if (bezt != nullptr) { \
      const float delta = pchan_temp.id - bezt->vec[1][1]; \
      bezt->vec[0][1] += delta; \
      bezt->vec[1][1] += delta; \
      bezt->vec[2][1] += delta; \
    } \
  } \
  ((void)0)

#define WRITE_ARRAY_FLT(id) \
  for (int i = 0; i < ARRAY_SIZE(pchan_temp.id); i++) { \
    WRITE_VALUE_FLT(id[i]); \
  } \
  ((void)0)

    /* Write the values back the F-Curves. */
    WRITE_ARRAY_FLT(loc);
    WRITE_ARRAY_FLT(eul);
    WRITE_ARRAY_FLT(quat);
    WRITE_ARRAY_FLT(rotAxis);
    WRITE_VALUE_FLT(rotAngle);
    WRITE_ARRAY_FLT(scale);
    /* No need to write back 'rotmode' as it can't be transformed. */

#undef WRITE_ARRAY_FLT
#undef WRITE_VALUE_FLT
  }

  /* Recalculate handles. */
  for (int i = 0; i < fcurve_array_len; i++) {
    BKE_fcurve_handles_recalc_ex(fcurve_array[i], eBezTriple_Flag(0));
  }

  MEM_freeN(keyed_frames);

  for (int chan = 0; chan < FCURVE_CHANNEL_LEN; chan++) {
    FCurve_KeyCache *fkc = (FCurve_KeyCache *)(&fkc_pchan) + chan;
    if (fkc->fcurve_eval) {
      MEM_freeN(fkc->fcurve_eval);
    }
    if (fkc->bezt_array) {
      MEM_freeN(fkc->bezt_array);
    }
  }
}

/**
 * Swap all RNA paths left/right.
 */
static void action_flip_pchan_rna_paths(bAction *act)
{
  const char *path_pose_prefix = "pose.bones[\"";
  const int path_pose_prefix_len = strlen(path_pose_prefix);

  /* Tag curves that have renamed f-curves. */
  for (bActionGroup *agrp : blender::animrig::legacy::channel_groups_all(act)) {
    agrp->flag &= ~AGRP_TEMP;
  }

  for (FCurve *fcu : blender::animrig::legacy::fcurves_all(act)) {
    if (!STRPREFIX(fcu->rna_path, path_pose_prefix)) {
      continue;
    }

    const char *name_esc = fcu->rna_path + path_pose_prefix_len;
    const char *name_esc_end = BLI_str_escape_find_quote(name_esc);

    /* While unlikely, an RNA path could be malformed. */
    if (UNLIKELY(name_esc_end == nullptr)) {
      continue;
    }

    char name[MAXBONENAME];
    const size_t name_esc_len = size_t(name_esc_end - name_esc);
    const size_t name_len = BLI_str_unescape(name, name_esc, name_esc_len);

    /* While unlikely, data paths could be constructed that have longer names than
     * are currently supported. */
    if (UNLIKELY(name_len >= sizeof(name))) {
      continue;
    }

    /* When the flipped name differs, perform the rename. */
    char name_flip[MAXBONENAME];
    BLI_string_flip_side_name(name_flip, name, false, sizeof(name_flip));
    if (!STREQ(name_flip, name)) {
      char name_flip_esc[MAXBONENAME * 2];
      BLI_str_escape(name_flip_esc, name_flip, sizeof(name_flip_esc));
      char *path_flip = BLI_sprintfN("pose.bones[\"%s%s", name_flip_esc, name_esc_end);
      MEM_freeN(fcu->rna_path);
      fcu->rna_path = path_flip;

      if (fcu->grp != nullptr) {
        fcu->grp->flag |= AGRP_TEMP;
      }
    }
  }

  /* Rename tagged groups. */
  for (bActionGroup *agrp : blender::animrig::legacy::channel_groups_all(act)) {
    if ((agrp->flag & AGRP_TEMP) == 0) {
      continue;
    }
    agrp->flag &= ~AGRP_TEMP;
    char name_flip[MAXBONENAME];
    BLI_string_flip_side_name(name_flip, agrp->name, false, sizeof(name_flip));
    if (!STREQ(name_flip, agrp->name)) {
      STRNCPY_UTF8(agrp->name, name_flip);
    }
  }
}

void BKE_action_flip_with_pose(bAction *act, blender::Span<Object *> objects)
{
  animrig::Action &action = act->wrap();
  if (action.slot_array_num == 0) {
    /* Cannot flip an empty action. */
    return;
  }
  blender::Set<animrig::Slot *> flipped_slots;
  for (Object *object : objects) {
    animrig::Slot *slot = animrig::generic_slot_for_autoassign(object->id, action, "");
    if (!slot) {
      slot = action.slot(0);
    }
    if (!flipped_slots.add(slot)) {
      continue;
    }
    Vector<FCurve *> fcurves = animrig::fcurves_for_action_slot(action, slot->handle);
    FCurvePathCache *fcache = BKE_fcurve_pathcache_create(fcurves);
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      action_flip_pchan(object, pchan, fcache);
    }
    BKE_fcurve_pathcache_destroy(fcache);
  }

  action_flip_pchan_rna_paths(act);

  DEG_id_tag_update(&act->id, ID_RECALC_SYNC_TO_EVAL);
}

/** \} */
