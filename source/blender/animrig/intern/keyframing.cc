/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include <cfloat>
#include <cmath>
#include <string>

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"
#include "ANIM_fcurve.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_rna.hh"
#include "ANIM_visualkey.hh"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_nla.h"
#include "BKE_report.h"

#include "BLI_dynstr.h"
#include "BLI_utildefines.h"
#include "BLT_translation.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"
#include "DNA_anim_types.h"
#include "ED_keyframing.hh"
#include "MEM_guardedalloc.h"
#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.h"
#include "RNA_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender::animrig {

/** Used to make curves newly added to a cyclic Action cycle with the correct period. */
static void make_new_fcurve_cyclic(const bAction *act, FCurve *fcu)
{
  /* The curve must contain one (newly-added) keyframe. */
  if (fcu->totvert != 1 || !fcu->bezt) {
    return;
  }

  const float period = act->frame_end - act->frame_start;

  if (period < 0.1f) {
    return;
  }

  /* Move the keyframe into the range. */
  const float frame_offset = fcu->bezt[0].vec[1][0] - act->frame_start;
  const float fix = floorf(frame_offset / period) * period;

  fcu->bezt[0].vec[0][0] -= fix;
  fcu->bezt[0].vec[1][0] -= fix;
  fcu->bezt[0].vec[2][0] -= fix;

  /* Duplicate and offset the keyframe. */
  fcu->bezt = static_cast<BezTriple *>(MEM_reallocN(fcu->bezt, sizeof(BezTriple) * 2));
  fcu->totvert = 2;

  fcu->bezt[1] = fcu->bezt[0];
  fcu->bezt[1].vec[0][0] += period;
  fcu->bezt[1].vec[1][0] += period;
  fcu->bezt[1].vec[2][0] += period;

  if (!fcu->modifiers.first) {
    add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES, fcu);
  }
}

/* Check indices that were intended to be remapped and report any failed remaps. */
static void get_keyframe_values_create_reports(ReportList *reports,
                                               PointerRNA ptr,
                                               PropertyRNA *prop,
                                               const int index,
                                               const int count,
                                               const bool force_all,
                                               const BLI_bitmap *successful_remaps)
{

  DynStr *ds_failed_indices = BLI_dynstr_new();

  int total_failed = 0;
  for (int i = 0; i < count; i++) {
    const bool cur_index_evaluated = ELEM(index, i, -1) || force_all;
    if (!cur_index_evaluated) {
      /* `values[i]` was never intended to be remapped. */
      continue;
    }

    if (BLI_BITMAP_TEST_BOOL(successful_remaps, i)) {
      /* `values[i]` successfully remapped. */
      continue;
    }

    total_failed++;
    /* Report that `values[i]` were intended to be remapped but failed remapping process. */
    BLI_dynstr_appendf(ds_failed_indices, "%d, ", i);
  }

  if (total_failed == 0) {
    BLI_dynstr_free(ds_failed_indices);
    return;
  }

  char *str_failed_indices = BLI_dynstr_get_cstring(ds_failed_indices);
  BLI_dynstr_free(ds_failed_indices);

  BKE_reportf(reports,
              RPT_WARNING,
              "Could not insert %i keyframe(s) due to zero NLA influence, base value, or value "
              "remapping failed: %s.%s for indices [%s]",
              total_failed,
              ptr.owner_id->name,
              RNA_property_ui_name(prop),
              str_failed_indices);

  MEM_freeN(str_failed_indices);
}

/**
 * Retrieve current property values to keyframe,
 * possibly applying NLA correction when necessary.
 *
 * \param r_successful_remaps: Enables bits for indices which are both intended to be remapped and
 * were successfully remapped. Bitmap allocated so it must be freed afterward.
 */
static Vector<float> get_keyframe_values(ReportList *reports,
                                         PointerRNA ptr,
                                         PropertyRNA *prop,
                                         int index,
                                         NlaKeyframingContext *nla_context,
                                         eInsertKeyFlags flag,
                                         const AnimationEvalContext *anim_eval_context,
                                         bool *r_force_all,
                                         BLI_bitmap **r_successful_remaps)
{
  Vector<float> values;

  if ((flag & INSERTKEY_MATRIX) && visualkey_can_use(&ptr, prop)) {
    /* Visual-keying is only available for object and pchan datablocks, as
     * it works by keyframing using a value extracted from the final matrix
     * instead of using the kt system to extract a value.
     */
    values = visualkey_get_values(&ptr, prop);
  }
  else {
    values = get_rna_values(&ptr, prop);
  }

  *r_successful_remaps = BLI_BITMAP_NEW(values.size(), __func__);

  /* adjust the value for NLA factors */
  BKE_animsys_nla_remap_keyframe_values(nla_context,
                                        &ptr,
                                        prop,
                                        values.as_mutable_span(),
                                        index,
                                        anim_eval_context,
                                        r_force_all,
                                        *r_successful_remaps);
  get_keyframe_values_create_reports(reports,
                                     ptr,
                                     prop,
                                     index,
                                     values.size(),
                                     r_force_all ? *r_force_all : false,
                                     *r_successful_remaps);

  return values;
}

/**
 * Move the point where a key is about to be inserted to be inside the main cycle range.
 * Returns the type of the cycle if it is enabled and valid.
 */
static eFCU_Cycle_Type remap_cyclic_keyframe_location(FCurve *fcu, float *px, float *py)
{
  if (fcu->totvert < 2 || !fcu->bezt) {
    return FCU_CYCLE_NONE;
  }

  eFCU_Cycle_Type type = BKE_fcurve_get_cycle_type(fcu);

  if (type == FCU_CYCLE_NONE) {
    return FCU_CYCLE_NONE;
  }

  BezTriple *first = &fcu->bezt[0], *last = &fcu->bezt[fcu->totvert - 1];
  const float start = first->vec[1][0], end = last->vec[1][0];

  if (start >= end) {
    return FCU_CYCLE_NONE;
  }

  if (*px < start || *px > end) {
    float period = end - start;
    float step = floorf((*px - start) / period);
    *px -= step * period;

    if (type == FCU_CYCLE_OFFSET) {
      /* Nasty check to handle the case when the modes are different better. */
      FMod_Cycles *data = static_cast<FMod_Cycles *>(((FModifier *)fcu->modifiers.first)->data);
      short mode = (step >= 0) ? data->after_mode : data->before_mode;

      if (mode == FCM_EXTRAPOLATE_CYCLIC_OFFSET) {
        *py -= step * (last->vec[1][1] - first->vec[1][1]);
      }
    }
  }

  return type;
}

/* Return codes for new_key_needed. */
enum {
  KEYNEEDED_DONTADD = 0,
  KEYNEEDED_JUSTADD,
  KEYNEEDED_DELPREV,
  KEYNEEDED_DELNEXT,
} /*eKeyNeededStatus*/;

/**
 * This helper function determines whether a new keyframe is needed.
 *
 * Cases where keyframes should not be added:
 * 1. Keyframe to be added between two keyframes with similar values.
 * 2. Keyframe to be added on frame where two keyframes are already situated.
 * 3. Keyframe lies at point that intersects the linear line between two keyframes.
 */
static short new_key_needed(FCurve *fcu, float cFrame, float nValue)
{
  if (fcu == nullptr) {
    return KEYNEEDED_JUSTADD;
  }
  int totCount = fcu->totvert;
  if (totCount == 0) {
    return KEYNEEDED_JUSTADD;
  }

  /* Loop through checking if any are the same. */
  BezTriple *bezt = fcu->bezt;
  BezTriple *prev = nullptr;
  for (int i = 0; i < totCount; i++) {
    float prevPosi = 0.0f, prevVal = 0.0f;
    float beztPosi = 0.0f, beztVal = 0.0f;

    beztPosi = bezt->vec[1][0];
    beztVal = bezt->vec[1][1];

    if (prev) {
      /* There is a keyframe before the one currently being examined. */
      prevPosi = prev->vec[1][0];
      prevVal = prev->vec[1][1];

      /* Keyframe to be added at point where there are already two similar points? */
      if (IS_EQF(prevPosi, cFrame) && IS_EQF(beztPosi, cFrame) && IS_EQF(beztPosi, prevPosi)) {
        return KEYNEEDED_DONTADD;
      }

      /* Keyframe between prev+current points? */
      if ((prevPosi <= cFrame) && (cFrame <= beztPosi)) {
        /* Is the value of keyframe to be added the same as keyframes on either side? */
        if (IS_EQF(prevVal, nValue) && IS_EQF(beztVal, nValue) && IS_EQF(prevVal, beztVal)) {
          return KEYNEEDED_DONTADD;
        }

        float realVal;

        /* Get real value of curve at that point. */
        realVal = evaluate_fcurve(fcu, cFrame);

        /* Compare whether it's the same as proposed. */
        if (IS_EQF(realVal, nValue)) {
          return KEYNEEDED_DONTADD;
        }
        return KEYNEEDED_JUSTADD;
      }

      /* New keyframe before prev beztriple? */
      if (cFrame < prevPosi) {
        /* A new keyframe will be added. However, whether the previous beztriple
         * stays around or not depends on whether the values of previous/current
         * beztriples and new keyframe are the same.
         */
        if (IS_EQF(prevVal, nValue) && IS_EQF(beztVal, nValue) && IS_EQF(prevVal, beztVal)) {
          return KEYNEEDED_DELNEXT;
        }

        return KEYNEEDED_JUSTADD;
      }
    }
    else {
      /* Just add a keyframe if there's only one keyframe
       * and the new one occurs before the existing one does.
       */
      if ((cFrame < beztPosi) && (totCount == 1)) {
        return KEYNEEDED_JUSTADD;
      }
    }

    /* Continue. Frame to do not yet passed (or other conditions not met) */
    if (i < (totCount - 1)) {
      prev = bezt;
      bezt++;
    }
    else {
      break;
    }
  }

  /* Frame in which to add a new-keyframe occurs after all other keys
   * -> If there are at least two existing keyframes, then if the values of the
   *    last two keyframes and the new-keyframe match, the last existing keyframe
   *    gets deleted as it is no longer required.
   * -> Otherwise, a keyframe is just added. 1.0 is added so that fake-2nd-to-last
   *    keyframe is not equal to last keyframe.
   */
  bezt = (fcu->bezt + (fcu->totvert - 1));
  const float valA = bezt->vec[1][1];
  float valB;
  if (prev) {
    valB = prev->vec[1][1];
  }
  else {
    valB = bezt->vec[1][1] + 1.0f;
  }

  if (IS_EQF(valA, nValue) && IS_EQF(valA, valB)) {
    return KEYNEEDED_DELPREV;
  }

  return KEYNEEDED_JUSTADD;
}

static AnimationEvalContext nla_time_remap(const AnimationEvalContext *anim_eval_context,
                                           PointerRNA *id_ptr,
                                           AnimData *adt,
                                           bAction *act,
                                           ListBase *nla_cache,
                                           NlaKeyframingContext **r_nla_context)
{
  if (adt && adt->action == act) {
    *r_nla_context = BKE_animsys_get_nla_keyframing_context(
        nla_cache, id_ptr, adt, anim_eval_context);

    const float remapped_frame = BKE_nla_tweakedit_remap(
        adt, anim_eval_context->eval_time, NLATIME_CONVERT_UNMAP);
    return BKE_animsys_eval_context_construct_at(anim_eval_context, remapped_frame);
  }

  *r_nla_context = nullptr;
  return *anim_eval_context;
}

/* Adjust frame on which to add keyframe, to make it easier to add corrective drivers. */
static float remap_driver_frame(const AnimationEvalContext *anim_eval_context,
                                PointerRNA *ptr,
                                PropertyRNA *prop,
                                const FCurve *fcu)
{
  float cfra = anim_eval_context->eval_time;
  PathResolvedRNA anim_rna;
  if (RNA_path_resolved_create(ptr, prop, fcu->array_index, &anim_rna)) {
    cfra = evaluate_driver(&anim_rna, fcu->driver, fcu->driver, anim_eval_context);
  }
  else {
    cfra = 0.0f;
  }
  return cfra;
}

/* Insert the specified keyframe value into a single F-Curve. */
static bool insert_keyframe_value(
    FCurve *fcu, float cfra, float curval, eBezTriple_KeyframeType keytype, eInsertKeyFlags flag)
{
  if (!BKE_fcurve_is_keyframable(fcu)) {
    return false;
  }

  /* Adjust coordinates for cycle aware insertion. */
  if (flag & INSERTKEY_CYCLE_AWARE) {
    if (remap_cyclic_keyframe_location(fcu, &cfra, &curval) != FCU_CYCLE_PERFECT) {
      /* Inhibit action from insert_vert_fcurve unless it's a perfect cycle. */
      flag &= ~INSERTKEY_CYCLE_AWARE;
    }
  }

  if (flag & INSERTKEY_NEEDED) {
    static short insert_mode = new_key_needed(fcu, cfra, curval);

    if (insert_mode == KEYNEEDED_DONTADD) {
      return false;
    }

    if (insert_vert_fcurve(fcu, cfra, curval, keytype, flag) < 0) {
      return false;
    }

    /* Based on the heuristics applied in new_key_needed(), the previous or next key needs to be
     * deleted. */
    switch (insert_mode) {
      case KEYNEEDED_DELPREV:
        BKE_fcurve_delete_key(fcu, fcu->totvert - 2);
        BKE_fcurve_handles_recalc(fcu);
        break;
      case KEYNEEDED_DELNEXT:
        BKE_fcurve_delete_key(fcu, 1);
        BKE_fcurve_handles_recalc(fcu);
        break;
    }

    return true;
  }

  return insert_vert_fcurve(fcu, cfra, curval, keytype, flag) >= 0;
}

bool insert_keyframe_direct(ReportList *reports,
                            PointerRNA ptr,
                            PropertyRNA *prop,
                            FCurve *fcu,
                            const AnimationEvalContext *anim_eval_context,
                            eBezTriple_KeyframeType keytype,
                            NlaKeyframingContext *nla_context,
                            eInsertKeyFlags flag)
{

  if (fcu == nullptr) {
    BKE_report(reports, RPT_ERROR, "No F-Curve to add keyframes to");
    return false;
  }

  if ((ptr.owner_id == nullptr) && (ptr.data == nullptr)) {
    BKE_report(
        reports, RPT_ERROR, "No RNA pointer available to retrieve values for keyframing from");
    return false;
  }

  if (prop == nullptr) {
    PointerRNA tmp_ptr;

    if (RNA_path_resolve_property(&ptr, fcu->rna_path, &tmp_ptr, &prop) == false) {
      const char *idname = (ptr.owner_id) ? ptr.owner_id->name : TIP_("<No ID pointer>");

      BKE_reportf(reports,
                  RPT_ERROR,
                  "Could not insert keyframe, as RNA path is invalid for the given ID (ID = %s, "
                  "path = %s)",
                  idname,
                  fcu->rna_path);
      return false;
    }

    /* Property found, so overwrite 'ptr' to make later code easier. */
    ptr = tmp_ptr;
  }

  /* Update F-Curve flags to ensure proper behavior for property type. */
  update_autoflags_fcurve_direct(fcu, prop);

  const int index = fcu->array_index;
  BLI_bitmap *successful_remaps = nullptr;
  Vector<float> values = get_keyframe_values(reports,
                                             ptr,
                                             prop,
                                             index,
                                             nla_context,
                                             flag,
                                             anim_eval_context,
                                             nullptr,
                                             &successful_remaps);

  float current_value = 0.0f;
  if (index >= 0 && index < values.size()) {
    current_value = values[index];
  }

  const bool curval_valid = BLI_BITMAP_TEST_BOOL(successful_remaps, index);
  MEM_freeN(successful_remaps);

  /* This happens if NLA rejects this insertion. */
  if (!curval_valid) {
    return false;
  }

  float cfra = anim_eval_context->eval_time;
  if ((flag & INSERTKEY_DRIVER) && (fcu->driver)) {
    cfra = remap_driver_frame(anim_eval_context, &ptr, prop, fcu);
  }

  const bool success = insert_keyframe_value(fcu, cfra, current_value, keytype, flag);

  if (!success) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Failed to insert keys on F-Curve with path '%s[%d]', ensure that it is not "
                "locked or sampled, and try removing F-Modifiers",
                fcu->rna_path,
                fcu->array_index);
  }
  return success;
}

/** Find or create the FCurve based on the given path, and insert the specified value into it. */
static bool insert_keyframe_fcurve_value(Main *bmain,
                                         ReportList *reports,
                                         PointerRNA *ptr,
                                         PropertyRNA *prop,
                                         bAction *act,
                                         const char group[],
                                         const char rna_path[],
                                         int array_index,
                                         const AnimationEvalContext *anim_eval_context,
                                         float curval,
                                         eBezTriple_KeyframeType keytype,
                                         eInsertKeyFlags flag)
{
  /* Make sure the F-Curve exists.
   * - if we're replacing keyframes only, DO NOT create new F-Curves if they do not exist yet
   *   but still try to get the F-Curve if it exists...
   */
  const bool can_create_curve = (flag & (INSERTKEY_REPLACE | INSERTKEY_AVAILABLE)) == 0;
  FCurve *fcu = can_create_curve ?
                    action_fcurve_ensure(bmain, act, group, ptr, rna_path, array_index) :
                    action_fcurve_find(act, rna_path, array_index);

  /* We may not have a F-Curve when we're replacing only. */
  if (!fcu) {
    return false;
  }

  const bool is_new_curve = (fcu->totvert == 0);

  /* Set color mode if the F-Curve is new (i.e. without any keyframes). */
  if (is_new_curve && (flag & INSERTKEY_XYZ2RGB)) {
    /* For Loc/Rot/Scale and also Color F-Curves, the color of the F-Curve in the Graph Editor,
     * is determined by the array index for the F-Curve
     */
    PropertySubType prop_subtype = RNA_property_subtype(prop);
    if (ELEM(prop_subtype, PROP_TRANSLATION, PROP_XYZ, PROP_EULER, PROP_COLOR, PROP_COORDS)) {
      fcu->color_mode = FCURVE_COLOR_AUTO_RGB;
    }
    else if (ELEM(prop_subtype, PROP_QUATERNION)) {
      fcu->color_mode = FCURVE_COLOR_AUTO_YRGB;
    }
  }

  /* If the curve has only one key, make it cyclic if appropriate. */
  const bool is_cyclic_action = (flag & INSERTKEY_CYCLE_AWARE) && BKE_action_is_cyclic(act);

  if (is_cyclic_action && fcu->totvert == 1) {
    make_new_fcurve_cyclic(act, fcu);
  }

  /* Update F-Curve flags to ensure proper behavior for property type. */
  update_autoflags_fcurve_direct(fcu, prop);

  float cfra = anim_eval_context->eval_time;
  if ((flag & INSERTKEY_DRIVER) && (fcu->driver)) {
    cfra = remap_driver_frame(anim_eval_context, ptr, prop, fcu);
  }

  const bool success = insert_keyframe_value(fcu, cfra, curval, keytype, flag);

  if (!success) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Failed to insert keys on F-Curve with path '%s[%d]', ensure that it is not "
                "locked or sampled, and try removing F-Modifiers",
                fcu->rna_path,
                fcu->array_index);
  }

  /* If the curve is new, make it cyclic if appropriate. */
  if (is_cyclic_action && is_new_curve) {
    make_new_fcurve_cyclic(act, fcu);
  }

  return success;
}

int insert_keyframe(Main *bmain,
                    ReportList *reports,
                    ID *id,
                    bAction *act,
                    const char group[],
                    const char rna_path[],
                    int array_index,
                    const AnimationEvalContext *anim_eval_context,
                    eBezTriple_KeyframeType keytype,
                    eInsertKeyFlags flag)
{
  if (id == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "No ID block to insert keyframe in (path = %s)", rna_path);
    return 0;
  }

  if (!BKE_id_is_editable(bmain, id)) {
    BKE_reportf(reports, RPT_ERROR, "'%s' on %s is not editable", rna_path, id->name + 2);
    return 0;
  }

  PointerRNA ptr;
  PropertyRNA *prop = nullptr;
  PointerRNA id_ptr = RNA_id_pointer_create(id);
  if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not insert keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        (id) ? id->name : TIP_("<Missing ID block>"),
        rna_path);
    return 0;
  }

  /* If no action is provided, keyframe to the default one attached to this ID-block. */
  if (act == nullptr) {
    act = ED_id_action_ensure(bmain, id);
    if (act == nullptr) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Could not insert keyframe, as this type does not support animation data (ID = "
                  "%s, path = %s)",
                  id->name,
                  rna_path);
      return 0;
    }
  }

  /* Apply NLA-mapping to frame to use (if applicable). */
  NlaKeyframingContext *nla_context = nullptr;
  ListBase nla_cache = {nullptr, nullptr};
  AnimData *adt = BKE_animdata_from_id(id);
  const AnimationEvalContext remapped_context = nla_time_remap(
      anim_eval_context, &id_ptr, adt, act, &nla_cache, &nla_context);

  bool force_all;
  BLI_bitmap *successful_remaps = nullptr;
  Vector<float> values = get_keyframe_values(reports,
                                             ptr,
                                             prop,
                                             array_index,
                                             nla_context,
                                             flag,
                                             anim_eval_context,
                                             &force_all,
                                             &successful_remaps);

  /* Key the entire array. */
  int key_count = 0;
  if (array_index == -1 || force_all) {
    /* In force mode, if any of the curves succeeds, drop the replace mode and restart. */
    if (force_all && (flag & (INSERTKEY_REPLACE | INSERTKEY_AVAILABLE)) != 0) {
      int exclude = -1;

      for (array_index = 0; array_index < values.size(); array_index++) {
        if (!BLI_BITMAP_TEST_BOOL(successful_remaps, array_index)) {
          continue;
        }

        if (insert_keyframe_fcurve_value(bmain,
                                         reports,
                                         &ptr,
                                         prop,
                                         act,
                                         group,
                                         rna_path,
                                         array_index,
                                         &remapped_context,
                                         values[array_index],
                                         keytype,
                                         flag))
        {
          key_count++;
          exclude = array_index;
          break;
        }
      }

      if (exclude != -1) {
        flag &= ~(INSERTKEY_REPLACE | INSERTKEY_AVAILABLE);

        for (array_index = 0; array_index < values.size(); array_index++) {
          if (!BLI_BITMAP_TEST_BOOL(successful_remaps, array_index)) {
            continue;
          }

          if (array_index != exclude) {
            key_count += insert_keyframe_fcurve_value(bmain,
                                                      reports,
                                                      &ptr,
                                                      prop,
                                                      act,
                                                      group,
                                                      rna_path,
                                                      array_index,
                                                      &remapped_context,
                                                      values[array_index],
                                                      keytype,
                                                      flag);
          }
        }
      }
    }
    /* Simply insert all channels. */
    else {
      for (array_index = 0; array_index < values.size(); array_index++) {
        if (!BLI_BITMAP_TEST_BOOL(successful_remaps, array_index)) {
          continue;
        }

        key_count += insert_keyframe_fcurve_value(bmain,
                                                  reports,
                                                  &ptr,
                                                  prop,
                                                  act,
                                                  group,
                                                  rna_path,
                                                  array_index,
                                                  &remapped_context,
                                                  values[array_index],
                                                  keytype,
                                                  flag);
      }
    }
  }
  /* Key a single index. */
  else {
    if (array_index >= 0 && array_index < values.size() &&
        BLI_BITMAP_TEST_BOOL(successful_remaps, array_index))
    {
      key_count += insert_keyframe_fcurve_value(bmain,
                                                reports,
                                                &ptr,
                                                prop,
                                                act,
                                                group,
                                                rna_path,
                                                array_index,
                                                &remapped_context,
                                                values[array_index],
                                                keytype,
                                                flag);
    }
  }

  MEM_freeN(successful_remaps);
  BKE_animsys_free_nla_keyframing_context_cache(&nla_cache);

  if (key_count > 0) {
    if (act != nullptr) {
      DEG_id_tag_update(&act->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }
    if (adt != nullptr && adt->action != nullptr && adt->action != act) {
      DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }
  }

  return key_count;
}

/* ************************************************** */
/* KEYFRAME DELETION */

/* Main Keyframing API call:
 * Use this when validation of necessary animation data isn't necessary as it
 * already exists. It will delete a keyframe at the current frame.
 *
 * The flag argument is used for special settings that alter the behavior of
 * the keyframe deletion. These include the quick refresh options.
 */

static void deg_tag_after_keyframe_delete(Main *bmain, ID *id, AnimData *adt)
{
  if (adt->action == nullptr) {
    /* In the case last f-curve was removed need to inform dependency graph
     * about relations update, since it needs to get rid of animation operation
     * for this data-block. */
    DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION_NO_FLUSH);
    DEG_relations_tag_update(bmain);
  }
  else {
    DEG_id_tag_update_ex(bmain, &adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
  }
}

int delete_keyframe(Main *bmain,
                    ReportList *reports,
                    ID *id,
                    bAction *act,
                    const char rna_path[],
                    int array_index,
                    float cfra)
{
  AnimData *adt = BKE_animdata_from_id(id);

  if (ELEM(nullptr, id, adt)) {
    BKE_report(reports, RPT_ERROR, "No ID block and/or AnimData to delete keyframe from");
    return 0;
  }

  PointerRNA ptr;
  PropertyRNA *prop;
  PointerRNA id_ptr = RNA_id_pointer_create(id);
  if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not delete keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        id->name,
        rna_path);
    return 0;
  }

  if (act == nullptr) {
    if (adt->action) {
      act = adt->action;
      cfra = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);
    }
    else {
      BKE_reportf(reports, RPT_ERROR, "No action to delete keyframes from for ID = %s", id->name);
      return 0;
    }
  }

  int array_index_max = array_index + 1;

  if (array_index == -1) {
    array_index = 0;
    array_index_max = RNA_property_array_length(&ptr, prop);

    /* For single properties, increase max_index so that the property itself gets included,
     * but don't do this for standard arrays since that can cause corruption issues
     * (extra unused curves).
     */
    if (array_index_max == array_index) {
      array_index_max++;
    }
  }

  /* Will only loop once unless the array index was -1. */
  int key_count = 0;
  for (; array_index < array_index_max; array_index++) {
    FCurve *fcu = action_fcurve_find(act, rna_path, array_index);

    if (fcu == nullptr) {
      continue;
    }

    if (BKE_fcurve_is_protected(fcu)) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Not deleting keyframe for locked F-Curve '%s' for %s '%s'",
                  fcu->rna_path,
                  BKE_idtype_idcode_to_name(GS(id->name)),
                  id->name + 2);
      continue;
    }

    key_count += delete_keyframe_fcurve(adt, fcu, cfra);
  }
  if (key_count) {
    deg_tag_after_keyframe_delete(bmain, id, adt);
  }

  return key_count;
}

/* ************************************************** */
/* KEYFRAME CLEAR */

int clear_keyframe(Main *bmain,
                   ReportList *reports,
                   ID *id,
                   bAction *act,
                   const char rna_path[],
                   int array_index,
                   eInsertKeyFlags /*flag*/)
{
  AnimData *adt = BKE_animdata_from_id(id);

  if (ELEM(nullptr, id, adt)) {
    BKE_report(reports, RPT_ERROR, "No ID block and/or AnimData to delete keyframe from");
    return 0;
  }

  PointerRNA ptr;
  PropertyRNA *prop;
  PointerRNA id_ptr = RNA_id_pointer_create(id);
  if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not clear keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        id->name,
        rna_path);
    return 0;
  }

  if (act == nullptr) {
    if (adt->action) {
      act = adt->action;
    }
    else {
      BKE_reportf(reports, RPT_ERROR, "No action to delete keyframes from for ID = %s", id->name);
      return 0;
    }
  }

  int array_index_max = array_index + 1;
  if (array_index == -1) {
    array_index = 0;
    array_index_max = RNA_property_array_length(&ptr, prop);

    /* For single properties, increase max_index so that the property itself gets included,
     * but don't do this for standard arrays since that can cause corruption issues
     * (extra unused curves).
     */
    if (array_index_max == array_index) {
      array_index_max++;
    }
  }

  int key_count = 0;
  /* Will only loop once unless the array index was -1. */
  for (; array_index < array_index_max; array_index++) {
    FCurve *fcu = action_fcurve_find(act, rna_path, array_index);

    if (fcu == nullptr) {
      continue;
    }

    if (BKE_fcurve_is_protected(fcu)) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Not clearing all keyframes from locked F-Curve '%s' for %s '%s'",
                  fcu->rna_path,
                  BKE_idtype_idcode_to_name(GS(id->name)),
                  id->name + 2);
      continue;
    }

    animdata_fcurve_delete(nullptr, adt, fcu);

    key_count++;
  }
  if (key_count) {
    deg_tag_after_keyframe_delete(bmain, id, adt);
  }

  return key_count;
}

int insert_key_action(Main *bmain,
                      bAction *action,
                      PointerRNA *ptr,
                      const std::string &rna_path,
                      const float frame,
                      const Span<float> values,
                      eInsertKeyFlags insert_key_flag,
                      eBezTriple_KeyframeType key_type)
{
  BLI_assert(bmain != nullptr);
  BLI_assert(action != nullptr);

  std::string group;
  if (ptr->type == &RNA_PoseBone) {
    bPoseChannel *pose_channel = static_cast<bPoseChannel *>(ptr->data);
    group = pose_channel->name;
  }
  else {
    group = "Object Transforms";
  }

  int property_array_index = 0;
  int inserted_keys = 0;
  for (float value : values) {
    FCurve *fcurve = action_fcurve_ensure(
        bmain, action, group.c_str(), ptr, rna_path.c_str(), property_array_index);
    const bool inserted_key = insert_keyframe_value(
        fcurve, frame, value, key_type, insert_key_flag);
    if (inserted_key) {
      inserted_keys++;
    }
    property_array_index++;
  }
  return inserted_keys;
}

}  // namespace blender::animrig
