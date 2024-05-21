/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include <cfloat>
#include <cmath>
#include <cstring>

#include "ANIM_animdata.hh"
#include "ANIM_fcurve.hh"
#include "BKE_fcurve.hh"
#include "BLI_math_base.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"
#include "DNA_anim_types.h"
#include "MEM_guardedalloc.h"

namespace blender::animrig {

KeyframeSettings get_keyframe_settings(const bool from_userprefs)
{
  KeyframeSettings settings = {};
  settings.keyframe_type = BEZT_KEYTYPE_KEYFRAME;
  settings.handle = HD_AUTO_ANIM;
  settings.interpolation = BEZT_IPO_BEZ;

  if (from_userprefs) {
    settings.interpolation = eBezTriple_Interpolation(U.ipo_new);
    settings.handle = eBezTriple_Handle(U.keyhandles_new);
  }
  return settings;
}

FCurve *create_fcurve_for_channel(const StringRef rna_path, const int array_index)
{
  FCurve *fcu = BKE_fcurve_create();
  fcu->rna_path = BLI_strdupn(rna_path.data(), rna_path.size());
  fcu->array_index = array_index;
  fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
  fcu->auto_smoothing = U.auto_smoothing_new;

  return fcu;
}

bool delete_keyframe_fcurve(AnimData *adt, FCurve *fcu, float cfra)
{
  bool found;

  const int index = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, cfra, fcu->totvert, &found);
  if (!found) {
    return false;
  }

  /* Delete the key at the index (will sanity check + do recalc afterwards). */
  BKE_fcurve_delete_key(fcu, index);
  BKE_fcurve_handles_recalc(fcu);

  /* Empty curves get automatically deleted. */
  if (BKE_fcurve_is_empty(fcu)) {
    animdata_fcurve_delete(nullptr, adt, fcu);
  }

  return true;
}

/* ************************************************** */
/* KEYFRAME INSERTION */

/* -------------- BezTriple Insertion -------------------- */

/* Change the Y position of a keyframe to match the input, adjusting handles. */
static void replace_bezt_keyframe_ypos(BezTriple *dst, const BezTriple *bezt)
{
  /* Just change the values when replacing, so as to not overwrite handles. */
  float dy = bezt->vec[1][1] - dst->vec[1][1];

  /* Just apply delta value change to the handle values. */
  dst->vec[0][1] += dy;
  dst->vec[1][1] += dy;
  dst->vec[2][1] += dy;

  dst->f1 = bezt->f1;
  dst->f2 = bezt->f2;
  dst->f3 = bezt->f3;

  /* TODO: perform some other operations? */
}

int insert_bezt_fcurve(FCurve *fcu, const BezTriple *bezt, eInsertKeyFlags flag)
{
  int i = 0;

  /* Are there already keyframes? */
  if (fcu->bezt) {
    bool replace;
    i = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, bezt->vec[1][0], fcu->totvert, &replace);

    /* Replace an existing keyframe? */
    if (replace) {
      /* 'i' may in rare cases exceed arraylen. */
      if ((i >= 0) && (i < fcu->totvert)) {
        if (flag & INSERTKEY_OVERWRITE_FULL) {
          fcu->bezt[i] = *bezt;
        }
        else {
          replace_bezt_keyframe_ypos(&fcu->bezt[i], bezt);
        }

        if (flag & INSERTKEY_CYCLE_AWARE) {
          /* If replacing an end point of a cyclic curve without offset,
           * modify the other end too. */
          if (ELEM(i, 0, fcu->totvert - 1) && BKE_fcurve_get_cycle_type(fcu) == FCU_CYCLE_PERFECT)
          {
            replace_bezt_keyframe_ypos(&fcu->bezt[i == 0 ? fcu->totvert - 1 : 0], bezt);
          }
        }
      }
    }
    /* Keyframing modes allow not replacing the keyframe. */
    else if ((flag & INSERTKEY_REPLACE) == 0) {
      /* Insert new - if we're not restricted to replacing keyframes only. */
      BezTriple *newb = static_cast<BezTriple *>(
          MEM_callocN((fcu->totvert + 1) * sizeof(BezTriple), "beztriple"));

      /* Add the beztriples that should occur before the beztriple to be pasted
       * (originally in fcu). */
      if (i > 0) {
        memcpy(newb, fcu->bezt, i * sizeof(BezTriple));
      }

      /* Add beztriple to paste at index i. */
      *(newb + i) = *bezt;

      /* Add the beztriples that occur after the beztriple to be pasted (originally in fcu). */
      if (i < fcu->totvert) {
        memcpy(newb + i + 1, fcu->bezt + i, (fcu->totvert - i) * sizeof(BezTriple));
      }

      /* Replace (+ free) old with new, only if necessary to do so. */
      MEM_freeN(fcu->bezt);
      fcu->bezt = newb;

      fcu->totvert++;
    }
    else {
      return -1;
    }
  }
  /* No keyframes yet, but can only add if...
   * 1) keyframing modes say that keyframes can only be replaced, so adding new ones won't know
   * 2) there are no samples on the curve
   *    NOTE: maybe we may want to allow this later when doing samples -> bezt conversions,
   *    but for now, having both is asking for trouble
   */
  else if ((flag & INSERTKEY_REPLACE) == 0 && (fcu->fpt == nullptr)) {
    /* Create new keyframes array. */
    fcu->bezt = static_cast<BezTriple *>(MEM_callocN(sizeof(BezTriple), "beztriple"));
    *(fcu->bezt) = *bezt;
    fcu->totvert = 1;
  }
  /* Cannot add anything. */
  else {
    /* Return error code -1 to prevent any misunderstandings. */
    return -1;
  }

  /* We need to return the index, so that some tools which do post-processing can
   * detect where we added the BezTriple in the array.
   */
  return i;
}

/**
 * Update the FCurve to allow insertion of `bezt` without modifying the curve shape.
 *
 * Checks whether it is necessary to apply Bezier subdivision due to involvement of non-auto
 * handles. If necessary, changes `bezt` handles from Auto to Aligned.
 *
 * \param bezt: key being inserted
 * \param prev: keyframe before that key
 * \param next: keyframe after that key
 */
static void subdivide_nonauto_handles(const FCurve *fcu,
                                      BezTriple *bezt,
                                      BezTriple *prev,
                                      BezTriple *next)
{
  if (prev->ipo != BEZT_IPO_BEZ || bezt->ipo != BEZT_IPO_BEZ) {
    return;
  }

  /* Don't change Vector handles, or completely auto regions. */
  const bool bezt_auto = BEZT_IS_AUTOH(bezt) || (bezt->h1 == HD_VECT && bezt->h2 == HD_VECT);
  const bool prev_auto = BEZT_IS_AUTOH(prev) || (prev->h2 == HD_VECT);
  const bool next_auto = BEZT_IS_AUTOH(next) || (next->h1 == HD_VECT);
  if (bezt_auto && prev_auto && next_auto) {
    return;
  }

  /* Subdivide the curve. */
  float delta;
  if (!BKE_fcurve_bezt_subdivide_handles(bezt, prev, next, &delta)) {
    return;
  }

  /* Decide when to force auto to manual. */
  if (!BEZT_IS_AUTOH(bezt)) {
    return;
  }
  if ((prev_auto || next_auto) && fcu->auto_smoothing == FCURVE_SMOOTH_CONT_ACCEL) {
    const float hx = bezt->vec[1][0] - bezt->vec[0][0];
    const float dx = bezt->vec[1][0] - prev->vec[1][0];

    /* This mode always uses 1/3 of key distance for handle x size. */
    const bool auto_works_well = fabsf(hx - dx / 3.0f) < 0.001f;
    if (auto_works_well) {
      return;
    }
  }

  /* Turn off auto mode. */
  bezt->h1 = bezt->h2 = HD_ALIGN;
}

void initialize_bezt(BezTriple *beztr,
                     const float2 position,
                     const KeyframeSettings &settings,
                     const eFCurve_Flags fcu_flags)
{
  /* Set all three points, for nicer start position.
   * NOTE: +/- 1 on vec.x for left and right handles is so that 'free' handles work ok...
   */
  beztr->vec[0][0] = position.x - 1.0f;
  beztr->vec[0][1] = position.y;
  beztr->vec[1][0] = position.x;
  beztr->vec[1][1] = position.y;
  beztr->vec[2][0] = position.x + 1.0f;
  beztr->vec[2][1] = position.y;
  beztr->f1 = beztr->f2 = beztr->f3 = SELECT;

  beztr->h1 = beztr->h2 = settings.handle;
  beztr->ipo = settings.interpolation;

  /* Interpolation type used is constrained by the type of values the curve can take. */
  if (fcu_flags & FCURVE_DISCRETE_VALUES) {
    beztr->ipo = BEZT_IPO_CONST;
  }
  else if ((beztr->ipo == BEZT_IPO_BEZ) && (fcu_flags & FCURVE_INT_VALUES)) {
    beztr->ipo = BEZT_IPO_LIN;
  }

  /* Set keyframe type value (supplied),
   * which should come from the scene settings in most cases. */
  BEZKEYTYPE_LVALUE(beztr) = settings.keyframe_type;

  /* Set default values for "easing" interpolation mode settings.
   * NOTE: Even if these modes aren't currently used, if users switch
   *       to these later, we want these to work in a sane way out of
   *       the box.
   */

  /* "back" easing - This value used to be used when overshoot=0, but that
   *                 introduced discontinuities in how the param worked. */
  beztr->back = 1.70158f;

  /* "elastic" easing - Values here were hand-optimized for a default duration of
   *                    ~10 frames (typical motion-graph motion length). */
  beztr->amplitude = 0.8f;
  beztr->period = 4.1f;
}

/**
 * Return whether the given fcurve already evaluates to the same value as the
 * proposed keyframe at the keyframe's time.
 *
 * This is a helper function for determining whether to insert a keyframe or not
 * when "only insert needed" is enabled.
 *
 * Note: this does *not* determine whether inserting the keyframe would change
 * the fcurve at points other than the keyframe itself. For example, even if
 * inserting the key wouldn't change the fcurve's value at the time of the
 * keyframe, the resulting changes to bezier interpolation could change the
 * fcurve on either side of it. This function intentionally does not account for
 * that, since that's not how the "only insert needed" feature is supposed to
 * work.
 */
static bool new_key_needed(const FCurve &fcu, const float frame, const float value)
{
  if (fcu.totvert == 0) {
    return true;
  }

  bool replace;
  const int bezt_index = BKE_fcurve_bezt_binarysearch_index(
      fcu.bezt, frame, fcu.totvert, &replace);

  if (replace) {
    /* If there is already a key, we only need to modify it if the proposed value is different. */
    return fcu.bezt[bezt_index].vec[1][1] != value;
  }

  const int diff_ulp = 32;
  const float fcu_eval = evaluate_fcurve(&fcu, frame);
  /* No need to insert a key if the same value is already the value of the FCurve at that point. */
  if (compare_ff_relative(fcu_eval, value, FLT_EPSILON, diff_ulp)) {
    return false;
  }

  return true;
}

SingleKeyingResult insert_vert_fcurve(FCurve *fcu,
                                      const float2 position,
                                      const KeyframeSettings &settings,
                                      eInsertKeyFlags flag)
{
  BLI_assert(fcu != nullptr);

  if ((flag & INSERTKEY_NEEDED) && !new_key_needed(*fcu, position[0], position[1])) {
    return SingleKeyingResult::NO_KEY_NEEDED;
  }

  BezTriple beztr = {{{0}}};
  initialize_bezt(&beztr, position, settings, eFCurve_Flags(fcu->flag));

  uint oldTot = fcu->totvert;
  int a;

  /* Add temp beztriple to keyframes. */
  a = insert_bezt_fcurve(fcu, &beztr, flag);
  BKE_fcurve_active_keyframe_set(fcu, &fcu->bezt[a]);

  /* Key insertion failed. */
  if (a < 0) {
    /* TODO: we need more info from `insert_bezt_fcurve()` called above to
     * return a more specific failure. */
    return SingleKeyingResult::UNKNOWN_FAILURE;
  }

  /* Set handle-type and interpolation. */
  if ((fcu->totvert > 2) && (flag & INSERTKEY_REPLACE) == 0) {
    BezTriple *bezt = (fcu->bezt + a);

    /* Set interpolation from previous (if available),
     * but only if we didn't just replace some keyframe:
     * - Replacement is indicated by no-change in number of verts.
     * - When replacing, the user may have specified some interpolation that should be kept.
     */
    if (fcu->totvert > oldTot) {
      if (a > 0) {
        bezt->ipo = (bezt - 1)->ipo;
      }
      else if (a < fcu->totvert - 1) {
        bezt->ipo = (bezt + 1)->ipo;
      }

      if (0 < a && a < (fcu->totvert - 1) && (flag & INSERTKEY_OVERWRITE_FULL) == 0) {
        subdivide_nonauto_handles(fcu, bezt, bezt - 1, bezt + 1);
      }
    }
  }

  /* Don't recalculate handles if fast is set.
   * - this is a hack to make importers faster
   * - we may calculate twice (due to auto-handle needing to be calculated twice)
   */
  if ((flag & INSERTKEY_FAST) == 0) {
    BKE_fcurve_handles_recalc(fcu);
  }

  /* Return the index at which the keyframe was added. */
  return SingleKeyingResult::SUCCESS;
}

void sample_fcurve_segment(const FCurve *fcu,
                           const float start_frame,
                           const float sample_rate,
                           float *samples,
                           const int sample_count)
{
  for (int i = 0; i < sample_count; i++) {
    const float evaluation_time = start_frame + (float(i) / sample_rate);
    samples[i] = evaluate_fcurve(fcu, evaluation_time);
  }
}

static void remove_fcurve_key_range(FCurve *fcu,
                                    const int2 range,
                                    const BakeCurveRemove removal_mode)
{
  switch (removal_mode) {

    case BakeCurveRemove::ALL: {
      BKE_fcurve_delete_keys_all(fcu);
      break;
    }

    case BakeCurveRemove::OUT_RANGE: {
      bool replace;

      int before_index = BKE_fcurve_bezt_binarysearch_index(
          fcu->bezt, range[0], fcu->totvert, &replace);

      if (before_index > 0) {
        BKE_fcurve_delete_keys(fcu, {0, uint(before_index)});
      }

      int after_index = BKE_fcurve_bezt_binarysearch_index(
          fcu->bezt, range[1], fcu->totvert, &replace);
      /* #OUT_RANGE is treated as exclusive on both ends. */
      if (replace) {
        after_index++;
      }
      if (after_index < fcu->totvert) {
        BKE_fcurve_delete_keys(fcu, {uint(after_index), fcu->totvert});
      }
      break;
    }

    case BakeCurveRemove::IN_RANGE: {
      bool replace;
      const int range_start_index = BKE_fcurve_bezt_binarysearch_index(
          fcu->bezt, range[0], fcu->totvert, &replace);
      int range_end_index = BKE_fcurve_bezt_binarysearch_index(
          fcu->bezt, range[1], fcu->totvert, &replace);
      if (replace) {
        range_end_index++;
      }

      if (range_end_index > range_start_index) {
        BKE_fcurve_delete_keys(fcu, {uint(range_start_index), uint(range_end_index)});
      }
      break;
    }

    default:
      break;
  }
}

void bake_fcurve(FCurve *fcu,
                 const int2 range,
                 const float step,
                 const BakeCurveRemove remove_existing)
{
  BLI_assert(step > 0);
  const int sample_count = (range[1] - range[0]) / step + 1;
  float *samples = static_cast<float *>(
      MEM_callocN(sample_count * sizeof(float), "Channel Bake Samples"));
  const float sample_rate = 1.0f / step;
  sample_fcurve_segment(fcu, range[0], sample_rate, samples, sample_count);

  if (remove_existing != BakeCurveRemove::NONE) {
    remove_fcurve_key_range(fcu, range, remove_existing);
  }

  BezTriple *baked_keys = static_cast<BezTriple *>(
      MEM_callocN(sample_count * sizeof(BezTriple), "beztriple"));

  const KeyframeSettings settings = get_keyframe_settings(true);

  for (int i = 0; i < sample_count; i++) {
    BezTriple *key = &baked_keys[i];
    float2 key_position = {range[0] + i * step, samples[i]};
    initialize_bezt(key, key_position, settings, eFCurve_Flags(fcu->flag));
  }

  int merged_size;
  BezTriple *merged_bezt = BKE_bezier_array_merge(
      baked_keys, sample_count, fcu->bezt, fcu->totvert, &merged_size);

  if (fcu->bezt != nullptr) {
    /* Can happen if we removed all keys beforehand. */
    MEM_freeN(fcu->bezt);
  }
  MEM_freeN(baked_keys);
  fcu->bezt = merged_bezt;
  fcu->totvert = merged_size;

  MEM_freeN(samples);
  BKE_fcurve_handles_recalc(fcu);
}

struct TempFrameValCache {
  float frame, val;
};

void bake_fcurve_segments(FCurve *fcu)
{
  const BezTriple *bezt, *start = nullptr, *end = nullptr;
  TempFrameValCache *value_cache, *fp;
  int sfra, range;
  int i, n;

  if (fcu->bezt == nullptr) {
    return;
  }

  KeyframeSettings settings = get_keyframe_settings(true);
  settings.keyframe_type = BEZT_KEYTYPE_BREAKDOWN;

  /* Find selected keyframes... once pair has been found, add keyframes. */
  for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    /* check if selected, and which end this is */
    if (BEZT_ISSEL_ANY(bezt)) {
      if (start) {
        /* If next bezt is also selected, don't start sampling yet,
         * but instead wait for that one to reconsider, to avoid
         * changing the curve when sampling consecutive segments
         * (#53229)
         */
        if (i < fcu->totvert - 1) {
          BezTriple *next = &fcu->bezt[i + 1];
          if (BEZT_ISSEL_ANY(next)) {
            continue;
          }
        }

        end = bezt;

        /* Cache values then add keyframes using these values, as adding
         * keyframes while sampling will affect the outcome...
         * - Only start sampling+adding from index=1, so that we don't overwrite original keyframe.
         */
        range = int(ceil(end->vec[1][0] - start->vec[1][0]));
        sfra = int(floor(start->vec[1][0]));

        if (range) {
          value_cache = static_cast<TempFrameValCache *>(
              MEM_callocN(sizeof(TempFrameValCache) * range, "IcuFrameValCache"));

          /* Sample values. */
          for (n = 1, fp = value_cache; n < range && fp; n++, fp++) {
            fp->frame = float(sfra + n);
            fp->val = evaluate_fcurve(fcu, fp->frame);
          }

          /* Add keyframes with these, tagging as 'breakdowns'. */
          for (n = 1, fp = value_cache; n < range && fp; n++, fp++) {
            blender::animrig::insert_vert_fcurve(
                fcu, {fp->frame, fp->val}, settings, INSERTKEY_NOFLAGS);
          }

          MEM_freeN(value_cache);

          /* As we added keyframes, we need to compensate so that bezt is at the right place. */
          bezt = fcu->bezt + i + range - 1;
          i += (range - 1);
        }

        /* The current selection island has ended, so start again from scratch. */
        start = nullptr;
        end = nullptr;
      }
      else {
        /* Just set start keyframe. */
        start = bezt;
        end = nullptr;
      }
    }
  }

  BKE_fcurve_handles_recalc(fcu);
}

}  // namespace blender::animrig
