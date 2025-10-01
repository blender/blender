/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_action.hh"
#include "BKE_curve.hh"
#include "BKE_fcurve.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"

#include "ED_keyframes_edit.hh"

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"
#include "ANIM_fcurve.hh"

#include "keyframes_general_intern.hh"

/* This file contains code for various keyframe-editing tools which are 'destructive'
 * (i.e. they will modify the order of the keyframes, and change the size of the array).
 * While some of these tools may eventually be moved out into blenkernel, for now, it is
 * fine to have these calls here.
 *
 * There are also a few tools here which cannot be easily coded for in the other system (yet).
 * These may also be moved around at some point, but for now, they are best added here.
 *
 * - Joshua Leung, Dec 2008
 */

/* **************************************************** */

bool duplicate_fcurve_keys(FCurve *fcu)
{
  bool changed = false;

  /* this can only work when there is an F-Curve, and also when there are some BezTriples */
  if (ELEM(nullptr, fcu, fcu->bezt)) {
    return changed;
  }

  for (int i = 0; i < fcu->totvert; i++) {
    /* If a key is selected */
    if (fcu->bezt[i].f2 & SELECT) {
      /* Expand the list */
      BezTriple *newbezt = MEM_calloc_arrayN<BezTriple>((fcu->totvert + 1), "beztriple");

      memcpy(newbezt, fcu->bezt, sizeof(BezTriple) * (i + 1));
      memcpy(newbezt + i + 1, fcu->bezt + i, sizeof(BezTriple));
      memcpy(newbezt + i + 2, fcu->bezt + i + 1, sizeof(BezTriple) * (fcu->totvert - (i + 1)));
      fcu->totvert++;
      changed = true;
      /* reassign pointers... (free old, and add new) */
      MEM_freeN(fcu->bezt);
      fcu->bezt = newbezt;

      /* Unselect the current key */
      BEZT_DESEL_ALL(&fcu->bezt[i]);
      i++;

      /* Select the copied key */
      BEZT_SEL_ALL(&fcu->bezt[i]);
    }
  }
  return changed;
}

/* -------------------------------------------------------------------- */
/** \name Various Tools
 * \{ */

void clean_fcurve(bAnimListElem *ale,
                  float thresh,
                  bool cleardefault,
                  const bool only_selected_keys)
{
  FCurve *fcu = static_cast<FCurve *>(ale->key_data);
  BezTriple *old_bezts, *bezt, *beztn;
  BezTriple *lastb;
  int totCount, i;

  /* Check if any points. */
  if ((fcu == nullptr) || (fcu->bezt == nullptr) || (fcu->totvert == 0) ||
      (!cleardefault && fcu->totvert == 1))
  {
    return;
  }

  /* make a copy of the old BezTriples, and clear F-Curve */
  old_bezts = fcu->bezt;
  totCount = fcu->totvert;
  fcu->bezt = nullptr;
  fcu->totvert = 0;

  /* now insert first keyframe, as it should be ok */
  bezt = old_bezts;
  blender::animrig::insert_bezt_fcurve(fcu, bezt, eInsertKeyFlags(0));
  if (!(bezt->f2 & SELECT)) {
    lastb = fcu->bezt;
    lastb->f1 = lastb->f2 = lastb->f3 = 0;
  }

  /* Loop through BezTriples, comparing them. Skip any that do
   * not fit the criteria for "ok" points.
   */
  for (i = 1; i < totCount; i++) {
    float prev[2], cur[2], next[2];

    /* get BezTriples and their values */
    if (i < (totCount - 1)) {
      beztn = (old_bezts + (i + 1));
      next[0] = beztn->vec[1][0];
      next[1] = beztn->vec[1][1];
    }
    else {
      beztn = nullptr;
      next[0] = next[1] = 0.0f;
    }
    lastb = (fcu->bezt + (fcu->totvert - 1));
    bezt = (old_bezts + i);

    /* get references for quicker access */
    prev[0] = lastb->vec[1][0];
    prev[1] = lastb->vec[1][1];
    cur[0] = bezt->vec[1][0];
    cur[1] = bezt->vec[1][1];

    if (only_selected_keys && !(bezt->f2 & SELECT)) {
      blender::animrig::insert_bezt_fcurve(fcu, bezt, eInsertKeyFlags(0));
      lastb = (fcu->bezt + (fcu->totvert - 1));
      lastb->f1 = lastb->f2 = lastb->f3 = 0;
      continue;
    }

    /* check if current bezt occurs at same time as last ok */
    if (IS_EQT(cur[0], prev[0], thresh)) {
      /* If there is a next beztriple, and if occurs at the same time, only insert
       * if there is a considerable distance between the points, and also if the
       * current is further away than the next one is to the previous.
       */
      if (beztn && IS_EQT(cur[0], next[0], thresh) && (IS_EQT(next[1], prev[1], thresh) == 0)) {
        /* only add if current is further away from previous */
        if (cur[1] > next[1]) {
          if (IS_EQT(cur[1], prev[1], thresh) == 0) {
            /* add new keyframe */
            blender::animrig::insert_bezt_fcurve(fcu, bezt, eInsertKeyFlags(0));
          }
        }
      }
      else {
        /* only add if values are a considerable distance apart */
        if (IS_EQT(cur[1], prev[1], thresh) == 0) {
          /* add new keyframe */
          blender::animrig::insert_bezt_fcurve(fcu, bezt, eInsertKeyFlags(0));
        }
      }
    }
    else {
      /* checks required are dependent on whether this is last keyframe or not */
      if (beztn) {
        /* does current have same value as previous and next? */
        if (IS_EQT(cur[1], prev[1], thresh) == 0) {
          /* add new keyframe */
          blender::animrig::insert_bezt_fcurve(fcu, bezt, eInsertKeyFlags(0));
        }
        else if (IS_EQT(cur[1], next[1], thresh) == 0) {
          /* add new keyframe */
          blender::animrig::insert_bezt_fcurve(fcu, bezt, eInsertKeyFlags(0));
        }
      }
      else {
        /* add if value doesn't equal that of previous */
        if (IS_EQT(cur[1], prev[1], thresh) == 0) {
          /* add new keyframe */
          blender::animrig::insert_bezt_fcurve(fcu, bezt, eInsertKeyFlags(0));
        }
      }
    }
  }

  /* now free the memory used by the old BezTriples */
  if (old_bezts) {
    MEM_freeN(old_bezts);
  }

  /* final step, if there is just one key in fcurve, check if it's
   * the default value and if is, remove fcurve completely. */
  if (cleardefault && fcu->totvert == 1) {
    float default_value = 0.0f;
    PointerRNA ptr;
    PropertyRNA *prop;
    PointerRNA id_ptr = RNA_id_pointer_create(ale->id);

    /* get property to read from, and get value as appropriate */
    if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
      if (RNA_property_type(prop) == PROP_FLOAT) {
        default_value = RNA_property_float_get_default_index(&ptr, prop, fcu->array_index);
      }
    }

    if (fcu->bezt->vec[1][1] == default_value) {
      BKE_fcurve_delete_keys_all(fcu);

      /* check if curve is really unused and if it is, return signal for deletion */
      if (BKE_fcurve_is_empty(fcu)) {
        AnimData *adt = ale->adt;
        blender::animrig::animdata_fcurve_delete(adt, fcu);
        ale->key_data = nullptr;
      }
    }
  }
}

/**
 * Find the first segment of consecutive selected curve points, starting from \a start_index.
 * Keys that have BEZT_FLAG_IGNORE_TAG set are treated as unselected.
 * \param r_segment_start_idx: returns the start index of the segment.
 * \param r_segment_len: returns the number of curve points in the segment.
 * \return whether such a segment was found or not.
 */
static bool find_fcurve_segment(FCurve *fcu,
                                const int start_index,
                                int *r_segment_start_idx,
                                int *r_segment_len)
{
  *r_segment_start_idx = 0;
  *r_segment_len = 0;

  bool in_segment = false;

  for (int i = start_index; i < fcu->totvert; i++) {
    const bool point_is_selected = fcu->bezt[i].f2 & SELECT;
    const bool point_is_ignored = fcu->bezt[i].f2 & BEZT_FLAG_IGNORE_TAG;

    if (point_is_selected && !point_is_ignored) {
      if (!in_segment) {
        *r_segment_start_idx = i;
        in_segment = true;
      }
      (*r_segment_len)++;
    }
    else if (in_segment) {
      /* If the curve point is not selected then we have reached the end of the selected curve
       * segment. */
      return true; /* Segment found. */
    }
  }

  /* If the last curve point was in the segment, `r_segment_len` and `r_segment_start_idx`
   * are already updated and true is returned. */
  return in_segment;
}

ListBase find_fcurve_segments(FCurve *fcu)
{
  ListBase segments = {nullptr, nullptr};

  /* Ignore baked curves. */
  if (!fcu->bezt) {
    return segments;
  }

  int segment_start_idx = 0;
  int segment_len = 0;
  int current_index = 0;

  while (find_fcurve_segment(fcu, current_index, &segment_start_idx, &segment_len)) {
    FCurveSegment *segment;
    segment = MEM_callocN<FCurveSegment>("FCurveSegment");
    segment->start_index = segment_start_idx;
    segment->length = segment_len;
    BLI_addtail(&segments, segment);
    current_index = segment_start_idx + segment_len;
  }
  return segments;
}

static const BezTriple *fcurve_segment_start_get(FCurve *fcu, int index)
{
  const BezTriple *start_bezt = index - 1 >= 0 ? &fcu->bezt[index - 1] : &fcu->bezt[index];
  return start_bezt;
}

static const BezTriple *fcurve_segment_end_get(FCurve *fcu, int index)
{
  const BezTriple *end_bezt = index < fcu->totvert ? &fcu->bezt[index] : &fcu->bezt[index - 1];
  return end_bezt;
}

/* ---------------- */

void blend_to_neighbor_fcurve_segment(FCurve *fcu, FCurveSegment *segment, const float factor)
{
  const BezTriple *target_bezt;
  /* Find which key to blend towards. */
  if (factor < 0) {
    target_bezt = fcurve_segment_start_get(fcu, segment->start_index);
  }
  else {
    target_bezt = fcurve_segment_end_get(fcu, segment->start_index + segment->length);
  }
  const float lerp_factor = fabs(factor);
  /* Blend each key individually. */
  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {
    const float key_y_value = interpf(target_bezt->vec[1][1], fcu->bezt[i].vec[1][1], lerp_factor);
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
}

/* ---------------- */

float get_default_rna_value(const FCurve *fcu, PropertyRNA *prop, PointerRNA *ptr)
{
  const int len = RNA_property_array_length(ptr, prop);

  float default_value = 0;
  /* Find the default value of that property. */
  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN:
      if (len) {
        default_value = float(RNA_property_boolean_get_default_index(ptr, prop, fcu->array_index));
      }
      else {
        default_value = float(RNA_property_boolean_get_default(ptr, prop));
      }
      break;
    case PROP_INT:
      if (len) {
        default_value = RNA_property_int_get_default_index(ptr, prop, fcu->array_index);
      }
      else {
        default_value = RNA_property_int_get_default(ptr, prop);
      }
      break;
    case PROP_FLOAT:
      if (len) {
        default_value = RNA_property_float_get_default_index(ptr, prop, fcu->array_index);
      }
      else {
        default_value = RNA_property_float_get_default(ptr, prop);
      }
      break;

    default:
      break;
  }
  return default_value;
}

void blend_to_default_fcurve(PointerRNA *id_ptr, FCurve *fcu, const float factor)
{
  PointerRNA ptr;
  PropertyRNA *prop;

  /* Check if path is valid. */
  if (!RNA_path_resolve_property(id_ptr, fcu->rna_path, &ptr, &prop)) {
    return;
  }

  const float default_value = get_default_rna_value(fcu, prop, &ptr);

  /* Blend selected keys to default. */
  for (int i = 0; i < fcu->totvert; i++) {
    if (!(fcu->bezt[i].f2 & SELECT)) {
      continue;
    }
    const float key_y_value = interpf(default_value, fcu->bezt[i].vec[1][1], factor);
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
}

/* ---------------- */

void scale_average_fcurve_segment(FCurve *fcu, FCurveSegment *segment, const float factor)
{
  float y = 0;

  /* Find first the average of the y values to then use it in the final calculation. */
  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {
    y += fcu->bezt[i].vec[1][1];
  }

  const float y_average = y / segment->length;

  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {
    const float key_y_value = interpf(y_average, fcu->bezt[i].vec[1][1], 1 - factor);
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
}

/* ---------------- */

struct ButterworthCoefficients {
  double *A, *d1, *d2;
  int filter_order;
};

ButterworthCoefficients *ED_anim_allocate_butterworth_coefficients(const int filter_order)
{
  ButterworthCoefficients *bw_coeff = MEM_callocN<ButterworthCoefficients>(
      "Butterworth Coefficients");
  bw_coeff->filter_order = filter_order;
  bw_coeff->d1 = MEM_calloc_arrayN<double>(filter_order, "coeff filtered");
  bw_coeff->d2 = MEM_calloc_arrayN<double>(filter_order, "coeff samples");
  bw_coeff->A = MEM_calloc_arrayN<double>(filter_order, "Butterworth A");
  return bw_coeff;
}

void ED_anim_free_butterworth_coefficients(ButterworthCoefficients *bw_coeff)
{
  MEM_freeN(bw_coeff->d1);
  MEM_freeN(bw_coeff->d2);
  MEM_freeN(bw_coeff->A);
  MEM_freeN(bw_coeff);
}

void ED_anim_calculate_butterworth_coefficients(const float cutoff_frequency,
                                                const float sampling_frequency,
                                                ButterworthCoefficients *bw_coeff)
{
  double s = double(sampling_frequency);
  const double a = tan(M_PI * cutoff_frequency / s);
  const double a2 = a * a;
  double r;
  for (int i = 0; i < bw_coeff->filter_order; ++i) {
    r = sin(M_PI * (2.0 * i + 1.0) / (4.0 * bw_coeff->filter_order));
    s = a2 + 2.0 * a * r + 1.0;
    bw_coeff->A[i] = a2 / s;
    bw_coeff->d1[i] = 2.0 * (1 - a2) / s;
    bw_coeff->d2[i] = -(a2 - 2.0 * a * r + 1.0) / s;
  }
}

static double butterworth_filter_value(
    double x, double *w0, double *w1, double *w2, ButterworthCoefficients *bw_coeff)
{
  for (int i = 0; i < bw_coeff->filter_order; i++) {
    w0[i] = bw_coeff->d1[i] * w1[i] + bw_coeff->d2[i] * w2[i] + x;
    x = bw_coeff->A[i] * (w0[i] + 2.0 * w1[i] + w2[i]);
    w2[i] = w1[i];
    w1[i] = w0[i];
  }
  return x;
}

static float butterworth_calculate_blend_value(float *samples,
                                               const float *filtered_values,
                                               const int start_index,
                                               const int end_index,
                                               const int sample_index,
                                               const int blend_in_out)
{
  if (start_index == end_index || blend_in_out == 0) {
    return samples[start_index];
  }

  const float blend_in_y_samples = samples[start_index];
  const float blend_out_y_samples = samples[end_index];

  const float blend_in_y_filtered = filtered_values[start_index + blend_in_out];
  const float blend_out_y_filtered = filtered_values[end_index - blend_in_out];

  const float slope_in_samples = samples[start_index] - samples[start_index - 1];
  const float slope_out_samples = samples[end_index] - samples[end_index + 1];
  const float slope_in_filtered = filtered_values[start_index + blend_in_out - 1] -
                                  filtered_values[start_index + blend_in_out];
  const float slope_out_filtered = filtered_values[end_index - blend_in_out] -
                                   filtered_values[end_index - blend_in_out - 1];

  if (sample_index - start_index <= blend_in_out) {
    const int blend_index = sample_index - start_index;
    const float blend_in_out_factor = clamp_f(float(blend_index) / blend_in_out, 0.0f, 1.0f);
    const float blend_value = interpf(blend_in_y_filtered +
                                          slope_in_filtered * (blend_in_out - blend_index),
                                      blend_in_y_samples + slope_in_samples * blend_index,
                                      blend_in_out_factor);
    return blend_value;
  }
  if (end_index - sample_index <= blend_in_out) {
    const int blend_index = end_index - sample_index;
    const float blend_in_out_factor = clamp_f(float(blend_index) / blend_in_out, 0.0f, 1.0f);
    const float blend_value = interpf(blend_out_y_filtered +
                                          slope_out_filtered * (blend_in_out - blend_index),
                                      blend_out_y_samples + slope_out_samples * blend_index,
                                      blend_in_out_factor);
    return blend_value;
  }
  return 0;
}

void butterworth_smooth_fcurve_segment(FCurve *fcu,
                                       FCurveSegment *segment,
                                       float *samples,
                                       const int sample_count,
                                       const float factor,
                                       const int blend_in_out,
                                       const int sample_rate,
                                       ButterworthCoefficients *bw_coeff)
{
  const int filter_order = bw_coeff->filter_order;

  float *filtered_values = MEM_calloc_arrayN<float>(sample_count,
                                                    "Butterworth Filtered FCurve Values");

  double *w0 = MEM_calloc_arrayN<double>(filter_order, "w0");
  double *w1 = MEM_calloc_arrayN<double>(filter_order, "w1");
  double *w2 = MEM_calloc_arrayN<double>(filter_order, "w2");

  /* The values need to be offset so the first sample starts at 0. This avoids oscillations at the
   * start and end of the curve. */
  const float fwd_offset = samples[0];

  for (int i = 0; i < sample_count; i++) {
    const double x = double(samples[i] - fwd_offset);
    const double filtered_value = butterworth_filter_value(x, w0, w1, w2, bw_coeff);
    filtered_values[i] = float(filtered_value) + fwd_offset;
  }

  for (int i = 0; i < filter_order; i++) {
    w0[i] = 0.0;
    w1[i] = 0.0;
    w2[i] = 0.0;
  }

  const float bwd_offset = filtered_values[sample_count - 1];

  /* Run the filter backwards as well to remove phase offset. */
  for (int i = sample_count - 1; i >= 0; i--) {
    const double x = double(filtered_values[i] - bwd_offset);
    const double filtered_value = butterworth_filter_value(x, w0, w1, w2, bw_coeff);
    filtered_values[i] = float(filtered_value) + bwd_offset;
  }

  const int segment_end_index = segment->start_index + segment->length;
  BezTriple left_bezt = fcu->bezt[segment->start_index];
  BezTriple right_bezt = fcu->bezt[segment_end_index - 1];

  const int samples_start_index = filter_order * sample_rate;
  const int samples_end_index = int(right_bezt.vec[1][0] - left_bezt.vec[1][0] + filter_order) *
                                sample_rate;

  const int blend_in_out_clamped = min_ii(blend_in_out,
                                          (samples_end_index - samples_start_index) / 2);

  for (int i = segment->start_index; i < segment_end_index; i++) {
    float blend_in_out_factor;
    if (blend_in_out_clamped == 0) {
      blend_in_out_factor = 1;
    }
    else if (i < segment->start_index + segment->length / 2) {
      blend_in_out_factor = min_ff(float(i - segment->start_index) / blend_in_out_clamped, 1.0f);
    }
    else {
      blend_in_out_factor = min_ff(float(segment_end_index - i - 1) / blend_in_out_clamped, 1.0f);
    }

    const float x_delta = fcu->bezt[i].vec[1][0] - left_bezt.vec[1][0] + filter_order;
    /* Using round() instead of casting to int. Casting would introduce a stepping issue when the
     * x-value is just below a full frame. */
    const int filter_index = round(x_delta * sample_rate);
    const float blend_value = butterworth_calculate_blend_value(samples,
                                                                filtered_values,
                                                                samples_start_index,
                                                                samples_end_index,
                                                                filter_index,
                                                                blend_in_out_clamped);

    const float blended_value = interpf(
        filtered_values[filter_index], blend_value, blend_in_out_factor);
    const float key_y_value = interpf(blended_value, samples[filter_index], factor);

    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }

  MEM_freeN(filtered_values);
  MEM_freeN(w0);
  MEM_freeN(w1);
  MEM_freeN(w2);
}

/* ---------------- */

void ED_ANIM_get_1d_gauss_kernel(const float sigma, const int kernel_size, double *r_kernel)
{
  BLI_assert(sigma > 0.0f);
  BLI_assert(kernel_size > 0);
  const double sigma_sq = 2.0 * sigma * sigma;
  double sum = 0.0;

  for (int i = 0; i < kernel_size; i++) {
    const double normalized_index = double(i) / (kernel_size - 1);
    r_kernel[i] = exp(-normalized_index * normalized_index / sigma_sq);
    if (i == 0) {
      sum += r_kernel[i];
    }
    else {
      /* We only calculate half the kernel,
       * the normalization needs to take that into account. */
      sum += r_kernel[i] * 2;
    }
  }

  /* Normalize kernel values. */
  for (int i = 0; i < kernel_size; i++) {
    r_kernel[i] /= sum;
  }
}

void smooth_fcurve_segment(FCurve *fcu,
                           FCurveSegment *segment,
                           const float *original_values,
                           float *samples,
                           const int sample_count,
                           const float factor,
                           const int kernel_size,
                           const double *kernel)
{
  const int segment_end_index = segment->start_index + segment->length;
  const float segment_start_x = fcu->bezt[segment->start_index].vec[1][0];
  float *filtered_samples = static_cast<float *>(MEM_dupallocN(samples));
  for (int i = kernel_size; i < sample_count - kernel_size; i++) {
    /* Apply the kernel. */
    double filter_result = samples[i] * kernel[0];
    for (int j = 1; j <= kernel_size; j++) {
      const double kernel_value = kernel[j];
      filter_result += samples[i + j] * kernel_value;
      filter_result += samples[i - j] * kernel_value;
    }
    filtered_samples[i] = filter_result;
  }

  for (int i = segment->start_index; i < segment_end_index; i++) {
    const float sample_index_f = (fcu->bezt[i].vec[1][0] - segment_start_x) + kernel_size;
    /* Using round() instead of (int). The latter would create stepping on x-values that are just
     * below a full frame. */
    const int sample_index = round(sample_index_f);
    /* Sampling the two closest indices to support subframe keys. This can end up being the same
     * index as sample_index, in which case the interpolation will happen between two identical
     * values. */
    const int secondary_index = clamp_i(
        sample_index + signum_i(sample_index_f - sample_index), 0, sample_count - 1);

    const float filter_result = interpf(filtered_samples[secondary_index],
                                        filtered_samples[sample_index],
                                        std::abs(sample_index_f - sample_index));
    const float key_y_value = interpf(
        filter_result, original_values[i - segment->start_index], factor);
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
  MEM_freeN(filtered_samples);
}
/* ---------------- */

static float ease_sigmoid_function(const float x, const float width, const float shift)
{
  const float x_shift = (x - shift) * width;
  const float y = x_shift / sqrt(1 + pow2f(x_shift));
  /* Normalize result to 0-1. */
  return (y + 1) * 0.5f;
}

void ease_fcurve_segment(FCurve *fcu,
                         FCurveSegment *segment,
                         const float factor,
                         const float width)
{
  const BezTriple *left_key = fcurve_segment_start_get(fcu, segment->start_index);
  const BezTriple *right_key = fcurve_segment_end_get(fcu, segment->start_index + segment->length);

  const float key_x_range = right_key->vec[1][0] - left_key->vec[1][0];
  const float key_y_range = right_key->vec[1][1] - left_key->vec[1][1];

  /* Happens if there is only 1 key on the FCurve. Needs to be skipped because it
   * would be a divide by 0. */
  if (IS_EQF(key_x_range, 0.0f)) {
    return;
  }

  /* Using the factor on the X-shift we are basically moving the curve horizontally. */
  const float shift = -factor;
  const float y_min = ease_sigmoid_function(-1, width, shift);
  const float y_max = ease_sigmoid_function(1, width, shift);

  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {
    /* Mapping the x-location of the key within the segment to a -1/1 range. */
    const float x = ((fcu->bezt[i].vec[1][0] - left_key->vec[1][0]) / key_x_range) * 2 - 1;
    const float y = ease_sigmoid_function(x, width, shift);
    /* Normalizing the y value to the min and max to ensure that the keys at the end are not
     * detached from the rest of the animation. */
    const float blend = (y - y_min) * (1 / (y_max - y_min));

    const float key_y_value = left_key->vec[1][1] + key_y_range * blend;
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
}

/* ---------------- */

void blend_offset_fcurve_segment(FCurve *fcu, FCurveSegment *segment, const float factor)
{
  const BezTriple *left_key = fcurve_segment_start_get(fcu, segment->start_index);
  const BezTriple *right_key = fcurve_segment_end_get(fcu, segment->start_index + segment->length);

  float y_delta;

  if (factor > 0) {
    const BezTriple segment_last_key = fcu->bezt[segment->start_index + segment->length - 1];
    y_delta = right_key->vec[1][1] - segment_last_key.vec[1][1];
  }
  else {
    const BezTriple segment_first_key = fcu->bezt[segment->start_index];
    y_delta = left_key->vec[1][1] - segment_first_key.vec[1][1];
  }

  const float offset_value = y_delta * fabs(factor);
  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {
    const float key_y_value = fcu->bezt[i].vec[1][1] + offset_value;
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
}

/* ---------------- */

static float s_curve(float x, float slope, float width, float height, float xshift, float yshift)
{
  /* Formula for 'S' curve we use for the "ease" sliders.
   * The shift values move the curve vertically or horizontally.
   * The range of the curve used is from 0 to 1 on "x" and "y"
   * so we can scale it (width and height) and move it (`xshift` and y `yshift`)
   * to crop the part of the curve we need. Slope determines how curvy the shape is. */
  float y = height * pow((x - xshift), slope) /
                (pow((x - xshift), slope) + pow((width - (x - xshift)), slope)) +
            yshift;

  /* The curve doesn't do what we want beyond our margins so we clamp the values. */
  if (x > xshift + width) {
    y = height + yshift;
  }
  else if (x < xshift) {
    y = yshift;
  }
  return y;
}

void blend_to_ease_fcurve_segment(FCurve *fcu, FCurveSegment *segment, const float factor)
{
  const BezTriple *left_key = fcurve_segment_start_get(fcu, segment->start_index);
  const BezTriple *right_key = fcurve_segment_end_get(fcu, segment->start_index + segment->length);

  const float key_x_range = right_key->vec[1][0] - left_key->vec[1][0];
  const float key_y_range = right_key->vec[1][1] - left_key->vec[1][1];

  /* Happens if there is only 1 key on the FCurve. Needs to be skipped because it
   * would be a divide by 0. */
  if (IS_EQF(key_x_range, 0.0f)) {
    return;
  }

  const float slope = 3.0;
  /* By doubling the size of the "S" curve we just one side of it, a "C" shape. */
  const float width = 2.0;
  const float height = 2.0;
  float xy_shift;

  /* Shifting the x and y values we can decide what side of the "S" shape to use. */
  if (factor > 0) {
    xy_shift = -1.0;
  }
  else {
    xy_shift = 0.0;
  }

  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {

    const float x = (fcu->bezt[i].vec[1][0] - left_key->vec[1][0]) / key_x_range;
    const float ease = s_curve(x, slope, width, height, xy_shift, xy_shift);
    const float base = left_key->vec[1][1] + key_y_range * ease;

    float y_delta;
    if (factor > 0) {
      y_delta = base - fcu->bezt[i].vec[1][1];
    }
    else {
      y_delta = fcu->bezt[i].vec[1][1] - base;
    }

    const float key_y_value = fcu->bezt[i].vec[1][1] + y_delta * factor;
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
}

/* ---------------- */

bool match_slope_fcurve_segment(FCurve *fcu, FCurveSegment *segment, const float factor)
{
  const BezTriple *left_key = fcurve_segment_start_get(fcu, segment->start_index);
  const BezTriple *right_key = fcurve_segment_end_get(fcu, segment->start_index + segment->length);

  BezTriple beyond_key;
  const BezTriple *reference_key;

  if (factor >= 0) {
    /* Stop the function if there is no key beyond the right neighboring one. */
    if (segment->start_index + segment->length >= fcu->totvert - 1) {
      return false;
    }
    reference_key = right_key;
    beyond_key = fcu->bezt[segment->start_index + segment->length + 1];
  }
  else {
    /* Stop the function if there is no key beyond the left neighboring one. */
    if (segment->start_index <= 1) {
      return false;
    }
    reference_key = left_key;
    beyond_key = fcu->bezt[segment->start_index - 2];
  }

  /* This delta values are used to get the relationship between the bookend keys and the
   * reference keys beyond those. */
  const float y_delta = beyond_key.vec[1][1] - reference_key->vec[1][1];
  const float x_delta = beyond_key.vec[1][0] - reference_key->vec[1][0];

  /* Avoids dividing by 0. */
  if (x_delta == 0) {
    return false;
  }

  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {

    /* These new deltas are used to determine the relationship between the current key and the
     * bookend ones. */
    const float new_x_delta = fcu->bezt[i].vec[1][0] - reference_key->vec[1][0];
    const float new_y_delta = new_x_delta * y_delta / x_delta;

    const float delta = reference_key->vec[1][1] + new_y_delta - fcu->bezt[i].vec[1][1];

    const float key_y_value = fcu->bezt[i].vec[1][1] + delta * fabs(factor);
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
  return true;
}

/* ---------------- */

void shear_fcurve_segment(FCurve *fcu,
                          FCurveSegment *segment,
                          const float factor,
                          tShearDirection direction)
{
  const BezTriple *left_key = fcurve_segment_start_get(fcu, segment->start_index);
  const BezTriple *right_key = fcurve_segment_end_get(fcu, segment->start_index + segment->length);

  const float key_x_range = right_key->vec[1][0] - left_key->vec[1][0];
  const float key_y_range = right_key->vec[1][1] - left_key->vec[1][1];

  /* Happens if there is only 1 key on the FCurve. Needs to be skipped because it
   * would be a divide by 0. */
  if (IS_EQF(key_x_range, 0.0f)) {
    return;
  }

  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {
    /* For easy calculation of the curve, the values are normalized. */
    float normalized_x;
    if (direction == SHEAR_FROM_LEFT) {
      normalized_x = (fcu->bezt[i].vec[1][0] - left_key->vec[1][0]) / key_x_range;
    }
    else {
      normalized_x = (right_key->vec[1][0] - fcu->bezt[i].vec[1][0]) / key_x_range;
    }

    const float y_delta = key_y_range * normalized_x;

    const float key_y_value = fcu->bezt[i].vec[1][1] + y_delta * factor;
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
}

/* ---------------- */

void push_pull_fcurve_segment(FCurve *fcu, FCurveSegment *segment, const float factor)
{
  const BezTriple *left_key = fcurve_segment_start_get(fcu, segment->start_index);
  const BezTriple *right_key = fcurve_segment_end_get(fcu, segment->start_index + segment->length);

  const float key_x_range = right_key->vec[1][0] - left_key->vec[1][0];
  const float key_y_range = right_key->vec[1][1] - left_key->vec[1][1];

  /* Happens if there is only 1 key on the FCurve. Needs to be skipped because it
   * would be a divide by 0. */
  if (IS_EQF(key_x_range, 0.0f)) {
    return;
  }

  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {
    /* For easy calculation of the curve, the values are normalized. */
    const float normalized_x = (fcu->bezt[i].vec[1][0] - left_key->vec[1][0]) / key_x_range;

    const float linear = left_key->vec[1][1] + key_y_range * normalized_x;

    const float delta = fcu->bezt[i].vec[1][1] - linear;

    const float key_y_value = linear + delta * factor;
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
}

/* ---------------- */

void time_offset_fcurve_segment(FCurve *fcu, FCurveSegment *segment, const float frame_offset)
{
  /* Two bookend keys of the fcurve are needed to be able to cycle the values. */
  const BezTriple *last_key = &fcu->bezt[fcu->totvert - 1];
  const BezTriple *first_key = &fcu->bezt[0];

  const float fcu_x_range = last_key->vec[1][0] - first_key->vec[1][0];
  const float fcu_y_range = last_key->vec[1][1] - first_key->vec[1][1];

  const float first_key_x = first_key->vec[1][0];

  /* If we operate directly on the fcurve there will be a feedback loop
   * so we need to capture the "y" values on an array to then apply them on a second loop. */
  float *y_values = MEM_calloc_arrayN<float>(segment->length, "Time Offset Samples");

  for (int i = 0; i < segment->length; i++) {
    /* This simulates the fcu curve moving in time. */
    const float time = fcu->bezt[segment->start_index + i].vec[1][0] + frame_offset;
    /* Need to normalize time to first_key to specify that as the wrapping point. */
    const float wrapped_time = floored_fmod(time - first_key_x, fcu_x_range) + first_key_x;
    const float delta_y = fcu_y_range * floorf((time - first_key_x) / fcu_x_range);

    const float key_y_value = evaluate_fcurve(fcu, wrapped_time) + delta_y;
    y_values[i] = key_y_value;
  }

  for (int i = 0; i < segment->length; i++) {
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[segment->start_index + i], y_values[i]);
  }
  MEM_freeN(y_values);
}

/* ---------------- */

void scale_from_fcurve_segment_neighbor(FCurve *fcu,
                                        FCurveSegment *segment,
                                        const float factor,
                                        const FCurveSegmentAnchor anchor)
{
  const BezTriple *reference_key;
  switch (anchor) {
    case FCurveSegmentAnchor::LEFT:
      reference_key = fcurve_segment_start_get(fcu, segment->start_index);
      break;
    case FCurveSegmentAnchor::RIGHT:
      reference_key = fcurve_segment_end_get(fcu, segment->start_index + segment->length);
      break;
  }

  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {
    const float key_y_value = interpf(fcu->bezt[i].vec[1][1], reference_key->vec[1][1], factor);
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
}

/* ---------------- */

void breakdown_fcurve_segment(FCurve *fcu, FCurveSegment *segment, const float factor)
{
  const BezTriple *left_bezt = fcurve_segment_start_get(fcu, segment->start_index);
  const BezTriple *right_bezt = fcurve_segment_end_get(fcu,
                                                       segment->start_index + segment->length);

  const float lerp_factor = (factor + 1) / 2;
  for (int i = segment->start_index; i < segment->start_index + segment->length; i++) {
    const float key_y_value = interpf(right_bezt->vec[1][1], left_bezt->vec[1][1], lerp_factor);
    BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[i], key_y_value);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FCurve Decimate
 * \{ */

/* Check if the keyframe interpolation type is supported */
static bool prepare_for_decimate(FCurve *fcu, int i)
{
  switch (fcu->bezt[i].ipo) {
    case BEZT_IPO_BEZ:
      /* We do not need to do anything here as the keyframe already has the required setting.
       */
      return true;
    case BEZT_IPO_LIN:
      /* Convert to a linear bezt curve to be able to use the decimation algorithm. */
      fcu->bezt[i].ipo = BEZT_IPO_BEZ;
      fcu->bezt[i].h1 = HD_FREE;
      fcu->bezt[i].h2 = HD_FREE;

      if (i != 0) {
        float h1[3];
        sub_v3_v3v3(h1, fcu->bezt[i - 1].vec[1], fcu->bezt[i].vec[1]);
        mul_v3_fl(h1, 1.0f / 3.0f);
        add_v3_v3(h1, fcu->bezt[i].vec[1]);
        copy_v3_v3(fcu->bezt[i].vec[0], h1);
      }

      if (i + 1 != fcu->totvert) {
        float h2[3];
        sub_v3_v3v3(h2, fcu->bezt[i + 1].vec[1], fcu->bezt[i].vec[1]);
        mul_v3_fl(h2, 1.0f / 3.0f);
        add_v3_v3(h2, fcu->bezt[i].vec[1]);
        copy_v3_v3(fcu->bezt[i].vec[2], h2);
      }
      return true;
    default:
      /* These are unsupported. */
      return false;
  }
}

/* Decimate the given curve segment. */
static void decimate_fcurve_segment(FCurve *fcu,
                                    int bezt_segment_start_idx,
                                    int bezt_segment_len,
                                    float remove_ratio,
                                    float error_sq_max)
{
  int selected_len = bezt_segment_len;

  /* Make sure that we can remove the start/end point of the segment if they
   * are not the start/end point of the curve. BKE_curve_decimate_bezt_array
   * has a check that prevents removal of the first and last index in the
   * passed array. */
  if (bezt_segment_len + bezt_segment_start_idx != fcu->totvert &&
      prepare_for_decimate(fcu, bezt_segment_len + bezt_segment_start_idx))
  {
    bezt_segment_len++;
  }
  if (bezt_segment_start_idx != 0 && prepare_for_decimate(fcu, bezt_segment_start_idx - 1)) {
    bezt_segment_start_idx--;
    bezt_segment_len++;
  }

  const int target_fcurve_verts = ceil(bezt_segment_len - selected_len * remove_ratio);

  BKE_curve_decimate_bezt_array(&fcu->bezt[bezt_segment_start_idx],
                                bezt_segment_len,
                                12, /* The actual resolution displayed in the viewport is dynamic
                                     * so we just pick a value that preserves the curve shape. */
                                false,
                                SELECT,
                                BEZT_FLAG_TEMP_TAG,
                                error_sq_max,
                                target_fcurve_verts);
}

bool decimate_fcurve(bAnimListElem *ale, float remove_ratio, float error_sq_max)
{
  FCurve *fcu = static_cast<FCurve *>(ale->key_data);
  /* Check if the curve actually has any points. */
  if (fcu == nullptr || fcu->bezt == nullptr || fcu->totvert == 0) {
    return true;
  }

  BezTriple *old_bezts = fcu->bezt;

  bool can_decimate_all_selected = true;

  for (int i = 0; i < fcu->totvert; i++) {
    /* Ignore keyframes that are not supported. */
    if (!prepare_for_decimate(fcu, i)) {
      can_decimate_all_selected = false;
      fcu->bezt[i].f2 |= BEZT_FLAG_IGNORE_TAG;
    }
    /* Make sure that the temp flag is unset as we use it to determine what to remove. */
    fcu->bezt[i].f2 &= ~BEZT_FLAG_TEMP_TAG;
  }

  ListBase segments = find_fcurve_segments(fcu);
  LISTBASE_FOREACH (FCurveSegment *, segment, &segments) {
    decimate_fcurve_segment(
        fcu, segment->start_index, segment->length, remove_ratio, error_sq_max);
  }
  BLI_freelistN(&segments);

  uint old_totvert = fcu->totvert;
  fcu->bezt = nullptr;
  fcu->totvert = 0;

  for (int i = 0; i < old_totvert; i++) {
    BezTriple *bezt = (old_bezts + i);
    bezt->f2 &= ~BEZT_FLAG_IGNORE_TAG;
    if ((bezt->f2 & BEZT_FLAG_TEMP_TAG) == 0) {
      blender::animrig::insert_bezt_fcurve(fcu, bezt, eInsertKeyFlags(0));
    }
  }
  /* now free the memory used by the old BezTriples */
  if (old_bezts) {
    MEM_freeN(old_bezts);
  }

  return can_decimate_all_selected;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FCurve Smooth
 * \{ */

/* temp struct used for smooth_fcurve */
struct tSmooth_Bezt {
  float *h1, *h2, *h3; /* bezt->vec[0,1,2][1] */
  float y1, y2, y3;    /* averaged before/new/after y-values */
};

void smooth_fcurve(FCurve *fcu)
{
  int totSel = 0;

  if (fcu->bezt == nullptr) {
    return;
  }

  /* first loop through - count how many verts are selected */
  BezTriple *bezt = fcu->bezt;
  for (int i = 0; i < fcu->totvert; i++, bezt++) {
    if (BEZT_ISSEL_ANY(bezt)) {
      totSel++;
    }
  }

  /* if any points were selected, allocate tSmooth_Bezt points to work on */
  if (totSel >= 3) {
    tSmooth_Bezt *tarray, *tsb;

    /* allocate memory in one go */
    tsb = tarray = MEM_calloc_arrayN<tSmooth_Bezt>(totSel, "tSmooth_Bezt Array");

    /* populate tarray with data of selected points */
    bezt = fcu->bezt;
    for (int i = 0, x = 0; (i < fcu->totvert) && (x < totSel); i++, bezt++) {
      if (BEZT_ISSEL_ANY(bezt)) {
        /* tsb simply needs pointer to vec, and index */
        tsb->h1 = &bezt->vec[0][1];
        tsb->h2 = &bezt->vec[1][1];
        tsb->h3 = &bezt->vec[2][1];

        /* advance to the next tsb to populate */
        if (x < totSel - 1) {
          tsb++;
        }
        else {
          break;
        }
      }
    }

    /* calculate the new smoothed F-Curve's with weighted averages:
     * - this is done with two passes to avoid progressive corruption errors
     * - uses 5 points for each operation (which stores in the relevant handles)
     * -   previous: w/a ratio = 3:5:2:1:1
     * -   next: w/a ratio = 1:1:2:5:3
     */

    /* round 1: calculate smoothing deltas and new values */
    tsb = tarray;
    for (int i = 0; i < totSel; i++, tsb++) {
      /* Don't touch end points (otherwise, curves slowly explode,
       * as we don't have enough data there). */
      if (ELEM(i, 0, (totSel - 1)) == 0) {
        const tSmooth_Bezt *tP1 = tsb - 1;
        const tSmooth_Bezt *tP2 = (i - 2 > 0) ? (tsb - 2) : (nullptr);
        const tSmooth_Bezt *tN1 = tsb + 1;
        const tSmooth_Bezt *tN2 = (i + 2 < totSel) ? (tsb + 2) : (nullptr);

        const float p1 = *tP1->h2;
        const float p2 = (tP2) ? (*tP2->h2) : (*tP1->h2);
        const float c1 = *tsb->h2;
        const float n1 = *tN1->h2;
        const float n2 = (tN2) ? (*tN2->h2) : (*tN1->h2);

        /* calculate previous and next, then new position by averaging these */
        tsb->y1 = (3 * p2 + 5 * p1 + 2 * c1 + n1 + n2) / 12;
        tsb->y3 = (p2 + p1 + 2 * c1 + 5 * n1 + 3 * n2) / 12;

        tsb->y2 = (tsb->y1 + tsb->y3) / 2;
      }
    }

    /* round 2: apply new values */
    tsb = tarray;
    for (int i = 0; i < totSel; i++, tsb++) {
      /* don't touch end points, as their values weren't touched above */
      if (ELEM(i, 0, (totSel - 1)) == 0) {
        /* y2 takes the average of the 2 points */
        *tsb->h2 = tsb->y2;

        /* handles are weighted between their original values and the averaged values */
        *tsb->h1 = ((*tsb->h1) * 0.7f) + (tsb->y1 * 0.3f);
        *tsb->h3 = ((*tsb->h3) * 0.7f) + (tsb->y3 * 0.3f);
      }
    }

    /* free memory required for tarray */
    MEM_freeN(tarray);
  }

  /* recalculate handles */
  BKE_fcurve_handles_recalc(fcu);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy/Paste Tools
 *
 * - The copy/paste buffer currently stores a set of temporary F-Curves containing only the
 *   keyframes that were selected in each of the original F-Curves.
 * - All pasted frames are offset by the same amount.
 *   This is calculated as the difference in the times of the current frame and the
 *   `first keyframe` (i.e. the earliest one in all channels).
 * - The earliest frame is calculated per copy operation.
 * \{ */

namespace blender::ed::animation {

KeyframeCopyBuffer *keyframe_copy_buffer = nullptr;

bool KeyframeCopyBuffer::is_empty() const
{
  /* No need to check the channelbags for having F-Curves, as they are only
   * added when an F-Curve needs to be stored. */
  return this->keyframe_data.channelbags().is_empty();
}

bool KeyframeCopyBuffer::is_single_fcurve() const
{
  if (this->keyframe_data.channelbags().size() != 1) {
    return false;
  }

  const animrig::Channelbag *channelbag = this->keyframe_data.channelbag(0);
  return channelbag->fcurves().size() == 1;
}

bool KeyframeCopyBuffer::is_bone(const FCurve &fcurve) const
{
  return this->bone_fcurves.contains(&fcurve);
}

int KeyframeCopyBuffer::num_slots() const
{
  /* The number of slots can be taken from any of these properties, so just assert that they are
   * consistent with each other. */
  BLI_assert(this->keyframe_data.channelbags().size() == this->slot_identifiers.size());

  /* Return the number of channelbags, as this is actually storing data; the `slot_identifiers`
   * field is more cache-like. Even though they should produce the same value, I (Sybren) feel more
   * comfortable using the channelbag count, as channelbags are defined as per-slot F-Curve
   * storage. */
  return this->keyframe_data.channelbags().size();
}

animrig::Channelbag *KeyframeCopyBuffer::channelbag_for_slot(const StringRef slot_identifier)
{
  /* TODO: use a nicer data structure so this loop isn't necessary any more. Or use a vector, in
   * which case we always need to loop, but it'll be small and maybe the lower overhead of Vector
   * (vs Map) will be worth it anyway. */
  for (const auto [handle, identifier] : this->slot_identifiers.items()) {
    if (identifier == slot_identifier) {
      return this->keyframe_data.channelbag_for_slot(handle);
    }
  }
  return nullptr;
}

void KeyframeCopyBuffer::debug_print() const
{
  using namespace blender::animrig;

  std::cout << "KeyframeCopyBuffer contents:" << std::endl;
  std::cout << "  frame range: " << this->first_frame << "-" << this->last_frame << std::endl;
  std::cout << "  scene frame: " << this->current_frame << std::endl;

  if (is_empty()) {
    std::cout << "  buffer is empty" << std::endl;
  }

  if (is_single_fcurve()) {
    std::cout << "  buffer has single F-Curve" << std::endl;
  }

  const StripKeyframeData &keyframe_data = this->keyframe_data;
  std::cout << "  channelbags: " << keyframe_data.channelbags().size() << std::endl;
  for (const Channelbag *channelbag : keyframe_data.channelbags()) {

    const std::string &slot_identifier = this->slot_identifiers.lookup(channelbag->slot_handle);
    if (slot_identifier == KeyframeCopyBuffer::SLOTLESS_SLOT_IDENTIFIER) {
      std::cout << "  - Channelbag for slotless F-Curves:" << std::endl;
    }
    else {
      std::cout << "  - Channelbag for slot \"" << slot_identifier << "\":" << std::endl;
    }
    for (const FCurve *fcurve : channelbag->fcurves()) {
      std::cout << "      " << fcurve->rna_path << "[" << fcurve->array_index << "]";
      if (this->is_bone(*fcurve)) {
        std::cout << " (bone)";
      }
      std::cout << std::endl;
    }
  }
}

}  // namespace blender::ed::animation

void ANIM_fcurves_copybuf_reset()
{
  using namespace blender::ed::animation;
  ANIM_fcurves_copybuf_free();
  keyframe_copy_buffer = MEM_new<KeyframeCopyBuffer>(__func__);
}

void ANIM_fcurves_copybuf_free()
{
  using namespace blender::ed::animation;
  if (keyframe_copy_buffer) {
    MEM_delete(keyframe_copy_buffer);
  }
}

/* ------------------- */

static bool is_animating_bone(const bAnimListElem *ale)
{
  BLI_assert(ale->datatype == ALE_FCURVE);

  if (!ale->id || GS(ale->id->name) != ID_OB) {
    return false;
  }

  Object *ob = reinterpret_cast<Object *>(ale->id);
  if (ob->type != OB_ARMATURE) {
    return false;
  }

  FCurve *fcurve = static_cast<FCurve *>(ale->key_data);
  if (!fcurve->rna_path) {
    return false;
  }

  char bone_name[sizeof(bPoseChannel::name)];
  if (!BLI_str_quoted_substr(fcurve->rna_path, "pose.bones[", bone_name, sizeof(bone_name))) {
    return false;
  }

  bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, bone_name);
  return pchan != nullptr;
};

namespace {

using namespace blender;
using namespace blender::animrig;
using namespace blender::ed::animation;

/**
 * Utility class to help map slots from the Actions data was copied from, to the slots used by the
 * copy-paste buffer.
 *
 * To the caller, this mapping is implicit, and is just reflected in the returned `Channelbag` for
 * some `bAnimListElem`. There is a 1:1 mapping in the copy-paste buffer between slots and their
 * channelbags,
 *
 * Technically this code could be part of KeyframeCopyBuffer. The nice thing about the current
 * structure is that the properties of this class are only accessible when copying, and once the
 * work is done, this gets destructed. The KeyframeCopyBuffer class only tracks data that is
 * relevant for pasting.
 */
class SlotMapper {
 public:
  KeyframeCopyBuffer &buffer;

  /** Mapping from action + slot handle to the buffer's internal slot handle. */
  Map<std::pair<const Action *, slot_handle_t>, slot_handle_t> orig_to_buffer_slots;

  /**
   * Slot handle used for slotless keyframes.
   *
   * NOTE: this is only used in this class, and NOT used by the KeyframeCopyBuffer data structure.
   * There, the Channelbag for slotless data can be found via
   * `keyframe_copy_buffer->channelbag_for_slot(SLOTLESS_SLOT_IDENTIFIER)`.
   *
   * These are for keyframes copied from F-Curves not owned by an Action, such as drivers and NLA
   * control curves.
   */
  static constexpr animrig::slot_handle_t SLOTLESS_SLOT_HANDLE = 0;

  /**
   * Ensure a Channelbag exists in the keyframe copy buffer for the F-Curve this 'ale' points to.
   */
  Channelbag &channelbag_for_ale(const bAnimListElem *ale)
  {
    /* Slots really only exist with F-Curves from Actions. */
    const Action *map_key_action;
    slot_handle_t map_key_handle;
    if (GS(ale->fcurve_owner_id->name) == ID_AC) {
      map_key_action = &reinterpret_cast<bAction *>(ale->fcurve_owner_id)->wrap();
      map_key_handle = ale->slot_handle;
    }
    else {
      map_key_action = nullptr;
      map_key_handle = SLOTLESS_SLOT_HANDLE;
    }

    const auto map_key = std::make_pair(map_key_action, map_key_handle);

    if (const std::optional<slot_handle_t> opt_internal_slot_handle =
            this->orig_to_buffer_slots.lookup_try(map_key))
    {
      /* There already is a slot for this, and that means there is a channelbag too. */
      const slot_handle_t internal_slot_handle = *opt_internal_slot_handle;
      BLI_assert(this->buffer.slot_identifiers.contains(internal_slot_handle));

      Channelbag *channelbag = this->buffer.keyframe_data.channelbag_for_slot(
          internal_slot_handle);
      BLI_assert_msg(channelbag, "If the slot exists, so should the channelbag");

      return *channelbag;
    }

    /* Create a new Channelbag for this F-Curve. */
    const slot_handle_t internal_slot_handle = ++this->buffer.last_used_slot_handle;
    Channelbag &channelbag = this->buffer.keyframe_data.channelbag_for_slot_add(
        internal_slot_handle);
    this->orig_to_buffer_slots.add_new(map_key, internal_slot_handle);

    /* Determine the slot identifier. */
    StringRef slot_identifier;
    if (map_key_action) {
      const Slot *ale_slot = map_key_action->slot_for_handle(ale->slot_handle);
      BLI_assert_msg(ale_slot, "Slot for copied keyframes is expected to exist.");
      slot_identifier = ale_slot->identifier;
    }
    else {
      slot_identifier = KeyframeCopyBuffer::SLOTLESS_SLOT_IDENTIFIER;
    }
    this->buffer.slot_identifiers.add(internal_slot_handle, slot_identifier);

    /* ale->id might be nullptr on unassigned slots. */
    this->buffer.slot_animated_ids.add_new(internal_slot_handle, ale->id);

    return channelbag;
  }
};

}  // namespace

bool copy_animedit_keys(bAnimContext *ac, ListBase *anim_data)
{
  using namespace blender::ed::animation;
  using namespace blender::animrig;

  ANIM_fcurves_copybuf_reset();

  SlotMapper slot_mapper{*keyframe_copy_buffer};

  LISTBASE_FOREACH (const bAnimListElem *, ale, anim_data) {
    BLI_assert(ale->datatype == ALE_FCURVE);
    const FCurve *fcu = static_cast<const FCurve *>(ale->key_data);

    /* Firstly, check if F-Curve has any selected keyframes. Skip if no selected
     * keyframes found (so no need to create unnecessary copy-buffer data). This
     * check should also eliminate any problems associated with using
     * sample-data. */
    if (ANIM_fcurve_keyframes_loop(
            nullptr,
            /* The const-cast is because I (Sybren) want to have `fcu` as `const` in as much of
             * this LISTBASE_FOREACH as possible. The code is alternating between the to-be-copied
             * F-Curve and the copy, and I want the compiler to help distinguish those. */
            const_cast<FCurve *>(fcu),
            nullptr,
            ANIM_editkeyframes_ok(BEZT_OK_SELECTED_KEY),
            nullptr) == 0)
    {
      continue;
    }

    Channelbag &channelbag = slot_mapper.channelbag_for_ale(ale);

    /* Create an F-Curve on this ChannelBag. */
    FCurve &fcurve_copy = *BKE_fcurve_create();
    fcurve_copy.rna_path = BLI_strdup(fcu->rna_path);
    fcurve_copy.array_index = fcu->array_index;
    channelbag.fcurve_append(fcurve_copy);

    if (fcu->grp) {
      bActionGroup &group = channelbag.channel_group_ensure(fcu->grp->name);
      channelbag.fcurve_assign_to_channel_group(fcurve_copy, group);
    }

    /* Detect if this is a bone. We do that here rather than during pasting
     * because ID pointers will get invalidated on undo / loading another file. */
    if (is_animating_bone(ale)) {
      keyframe_copy_buffer->bone_fcurves.add(&fcurve_copy);
    }

    /* Add selected keyframes to the buffer F-Curve. */
    int bezt_index = 0;
    BezTriple *bezt = fcu->bezt;
    for (; bezt_index < fcu->totvert; bezt_index++, bezt++) {
      /* Don't copy if only a handle is selected. */
      if (!BEZT_ISSEL_IDX(bezt, 1)) {
        continue;
      }

      /* Use INSERTKEY_FAST as that avoids recalculating handles. They should
       * remain as-is in the buffer. */
      animrig::insert_bezt_fcurve(&fcurve_copy, bezt, INSERTKEY_OVERWRITE_FULL | INSERTKEY_FAST);

      /* Keep track of the extremities. */
      const float bezt_frame = bezt->vec[1][0];
      keyframe_copy_buffer->first_frame = std::min(keyframe_copy_buffer->first_frame, bezt_frame);
      keyframe_copy_buffer->last_frame = std::max(keyframe_copy_buffer->last_frame, bezt_frame);
    }
  }

  keyframe_copy_buffer->current_frame = ac->scene->r.cfra;

#ifndef NDEBUG
  /* TODO: remove this call completely when slot-aware copy-pasting has been implemented. */
  keyframe_copy_buffer->debug_print();
#endif

  return !keyframe_copy_buffer->is_empty();
}

namespace blender::ed::animation {

std::optional<std::string> flip_names(const blender::StringRefNull rna_path)
{
  int ofs_start, ofs_end;
  if (!BLI_str_quoted_substr_range(rna_path.c_str(), "pose.bones[", &ofs_start, &ofs_end)) {
    return {};
  }

  /* NOTE: there is no need to un-escape the string to flip it.
   * However the buffer does need to be twice the size. */
  char bname_new[MAX_VGROUP_NAME * 2];

  /* Take a copy so it's 0-terminated. */
  const std::string bone_name = rna_path.substr(ofs_start, ofs_end - ofs_start);

  BLI_string_flip_side_name(bname_new, bone_name.c_str(), false, sizeof(bname_new));

  return rna_path.substr(0, ofs_start) + bname_new + rna_path.substr(ofs_end);
}

using pastebuf_match_func = bool (*)(Main *bmain,
                                     const FCurve &fcurve_to_match,
                                     const FCurve &fcurve_in_copy_buffer,
                                     blender::animrig::slot_handle_t slot_handle_in_copy_buffer,
                                     bool from_single,
                                     bool to_single,
                                     bool flip);

namespace {

using namespace blender::animrig;

enum class SlotMatchMethod {
  /** No matching, just ignore slots altogether. */
  NONE = 0,
  /** Source and target F-Curve must be from a slot with the same identifier. */
  IDENTIFIER = 1,
  /** Target F-Curve must be from a selected slot, name does not matter. */
  SELECTION = 2,
  /** NAME + target F-Curve must be from a selected slot. */
  SELECTION_AND_IDENTIFIER = 3,
};

}  // namespace

/**
 * Determine whether slot names matter when matching copied data to selected-to-paste-into
 * channels.
 */
static SlotMatchMethod get_slot_match_method(const bool from_single,
                                             const bool to_single,
                                             const KeyframePasteContext &paste_context)
{
  BLI_assert_msg(!(from_single && to_single),
                 "The from-single-to-single case is expected to be implemented as a special case "
                 "in `paste_animedit_keys()`");
  UNUSED_VARS_NDEBUG(from_single);

  if (paste_context.num_fcurves_selected == 0) {
    /* No F-Curves selected to explicitly paste into. The names of the selected slots determine the
     * source Channelbag. */

    if (paste_context.num_slots_selected == 0) {
      /* Since none of the slots were selected to paste into, just do a match by name and ignore
       * their selection state. */
      return SlotMatchMethod::IDENTIFIER;
    }

    if (keyframe_copy_buffer->num_slots() == 1) {
      /* Copied from one slot, pasting into one or more selected slots. This should only look at
       * selection of the target slot, and ignore slot names. */
      return SlotMatchMethod::SELECTION;
    }

    /* Copied from multiple slots, in which case slot names do matter, and the targets should be
     * limited by their slot selection (i.e. F-Curves from unselected slots should not be pasted
     * into). */
    return SlotMatchMethod::SELECTION_AND_IDENTIFIER;
  }

  if (to_single) {
    /* Copying into a single F-Curve. Because the single-to-single case is handled somewhere else,
     * we know multiple F-Curves were copied. Slot names do not matter, matching is done purely on
     * RNA path + array index.
     *
     * TODO: slot names may matter here after all, when the copy buffer has multiple slots. In that
     * case the single F-Curve to paste into can still match multiple copied F-Curves. That's a
     * corner case to implement at some other time, though. */
    return SlotMatchMethod::NONE;
  }

  /* Pasting into multiple F-Curves. Whether slot names matter depends on how many slots the
   * key were copied from:
   * - 0 slots: impossible, then there would not be any keys at all.
   * - 1 slot: slot names do not matter. This makes it possible to copy-paste between slots.
   * - 2+ slots: slot names matter, only paste within the same slot as the copied data.
   */

  const int num_slots_copied = keyframe_copy_buffer->num_slots();
  BLI_assert_msg(num_slots_copied > 0,
                 "If any keyframes were copied, they MUST have come from some slot.");
  if (num_slots_copied > 1) {
    /* Copied from multiple slots, so do name matching. */
    return SlotMatchMethod::IDENTIFIER;
  }

  return SlotMatchMethod::NONE;
}

/**
 * Return the first item in the copy buffer that matches the given bAnimListElem.
 *
 * \param ale_to_paste_into: must be an ALE that represents an F-Curve. The entire ALE is passed
 * (instead of just the F-Curve) as it provides information about the Action & Slot it came from.
 */
static const FCurve *pastebuf_find_matching_copybuf_item(const pastebuf_match_func strategy,
                                                         Main *bmain,
                                                         const bAnimListElem &ale_to_paste_into,
                                                         const bool from_single,
                                                         const bool to_single,
                                                         const KeyframePasteContext &paste_context)
{
  using namespace blender::animrig;

  BLI_assert(ale_to_paste_into.datatype == ALE_FCURVE);
  const FCurve &fcurve_to_match = *static_cast<FCurve *>(ale_to_paste_into.data);

  BLI_assert_msg(!(from_single && to_single),
                 "The from-single-to-single case is expected to be implemented as a special case "
                 "in `paste_animedit_keys()`");

  /* Because `channelbags_to_paste_from` can reference `single_copy_buffer_channelbag`, the latter
   * has to live longer, hence it is declared first. */
  const Channelbag *single_copy_buffer_channelbag;
  Span<const Channelbag *> channelbags_to_paste_from;

  /* Get the slot of this ALE, as some of the cases below need to query it. It might be slotless,
   * for example for NLA control curves. */
  const Slot *ale_slot = nullptr;
  const Action *ale_action = nullptr;
  if (GS(ale_to_paste_into.fcurve_owner_id->name) == ID_AC) {
    ale_action = &reinterpret_cast<bAction *>(ale_to_paste_into.fcurve_owner_id)->wrap();
    ale_slot = ale_action->slot_for_handle(ale_to_paste_into.slot_handle);
    BLI_assert(ale_slot);
  }

  /* NASTINESS: this code shouldn't have to care about which slots are currently visible in
   * the channel list. But since selection state is only relevant when they CAN actually be
   * selected, it does matter. This code assumes:
   *   1. because SELECTION or SELECTION_AND_IDENTIFIER was returned, slot selection is a
   *      thing in this mode,
   *   2. because slot selection is a thing, and this F-Curve is potentially getting pasted
   *      into, its slot is visible too,
   *   3. and because of that, the selection state of this slot is enough to check here. */

  const SlotMatchMethod slot_match = get_slot_match_method(from_single, to_single, paste_context);
  switch (slot_match) {
    case SlotMatchMethod::SELECTION:
      if (!ale_slot->is_selected()) {
        return nullptr;
      }
      ATTR_FALLTHROUGH;

    case SlotMatchMethod::NONE:
      /* Just search through all channelbags in the copy buffer. */
      channelbags_to_paste_from = keyframe_copy_buffer->keyframe_data.channelbags();
      break;

    case SlotMatchMethod::SELECTION_AND_IDENTIFIER:
      if (!ale_slot->is_selected()) {
        return nullptr;
      }
      ATTR_FALLTHROUGH;

    case SlotMatchMethod::IDENTIFIER: {
      /* See if we copied from a slot whose identifier matches this ALE. */
      const std::string target_slot_identifier = ale_slot ?
                                                     ale_slot->identifier :
                                                     KeyframeCopyBuffer::SLOTLESS_SLOT_IDENTIFIER;

      single_copy_buffer_channelbag = keyframe_copy_buffer->channelbag_for_slot(
          target_slot_identifier);
      if (!single_copy_buffer_channelbag) {
        /* No data copied from a slot with this name. */
        return nullptr;
      }

      /* Only consider the F-Curves from this channelbag. Because of channelbags_to_paste_from
       * referencing single_copy_buffer_channelbag, the latter has to live longer, hence it was
       * declared first. */
      channelbags_to_paste_from = Span<const Channelbag *>(&single_copy_buffer_channelbag, 1);
      break;
    }
  }

  for (const Channelbag *channelbag : channelbags_to_paste_from) {
    for (const FCurve *fcurve : channelbag->fcurves()) {
      if (strategy(bmain,
                   fcurve_to_match,
                   *fcurve,
                   channelbag->slot_handle,
                   from_single,
                   to_single,
                   paste_context.flip))
      {
        return fcurve;
      }
    }
  }

  return nullptr;
}

bool pastebuf_match_path_full(Main * /*bmain*/,
                              const FCurve &fcurve_to_match,
                              const FCurve &fcurve_in_copy_buffer,
                              blender::animrig::slot_handle_t /*slot_handle_in_copy_buffer*/,
                              const bool from_single,
                              const bool to_single,
                              const bool flip)
{
  if (!fcurve_to_match.rna_path || !fcurve_in_copy_buffer.rna_path) {
    /* No paths to compare to, so only ok if pasting to a single F-Curve. */
    return to_single;
  }

  /* The 'source' of the copy data is considered 'ok' if either we're copying a single F-Curve,
   * or the array index matches (so [location X,Y,Z] can be copied to a single 'scale' property,
   * and it'll pick up the right X/Y/Z component). */
  const bool is_source_ok = from_single ||
                            fcurve_in_copy_buffer.array_index == fcurve_to_match.array_index;
  if (!is_source_ok) {
    return false;
  }

  if (!to_single && flip && keyframe_copy_buffer->is_bone(fcurve_in_copy_buffer)) {
    const std::optional<std::string> with_flipped_name = blender::ed::animation::flip_names(
        fcurve_in_copy_buffer.rna_path);
    return with_flipped_name && with_flipped_name == fcurve_to_match.rna_path;
  }

  return to_single || STREQ(fcurve_in_copy_buffer.rna_path, fcurve_to_match.rna_path);
}

bool pastebuf_match_path_property(Main *bmain,
                                  const FCurve &fcurve_to_match,
                                  const FCurve &fcurve_in_copy_buffer,
                                  blender::animrig::slot_handle_t slot_handle_in_copy_buffer,
                                  const bool from_single,
                                  const bool /*to_single*/,
                                  const bool /*flip*/)
{
  if (!fcurve_in_copy_buffer.rna_path || !fcurve_to_match.rna_path) {
    return false;
  }

  /* The 'source' of the copy data is considered 'ok' if either we're copying a single F-Curve,
   * or the array index matches (so [location X,Y,Z] can be copied to a single 'scale' property,
   * and it'll pick up the right X/Y/Z component). */
  const bool is_source_ok = from_single ||
                            fcurve_in_copy_buffer.array_index == fcurve_to_match.array_index;
  if (!is_source_ok) {
    return false;
  }

  /* find the property of the fcurve and compare against the end of the tAnimCopybufItem
   * more involved since it needs to do path lookups.
   * This is not 100% reliable since the user could be editing the curves on a path that won't
   * resolve, or a bone could be renamed after copying for eg. but in normal copy & paste
   * this should work out ok.
   */
  const std::optional<ID *> optional_id = keyframe_copy_buffer->slot_animated_ids.lookup_try(
      slot_handle_in_copy_buffer);
  if (!optional_id) {
    printf(
        "paste_animedit_keys: no idea which ID was animated by \"%s\" in slot \"%s\", so cannot "
        "match by property name\n",
        fcurve_in_copy_buffer.rna_path,
        keyframe_copy_buffer->slot_identifiers.lookup(slot_handle_in_copy_buffer).c_str());
    return false;
  }
  ID *animated_id = optional_id.value();
  if (BLI_findindex(which_libbase(bmain, GS(animated_id->name)), animated_id) == -1) {
    /* The ID could have been removed after copying the keys. This function
     * needs it to resolve the property & get the name. */
    printf("paste_animedit_keys: error ID has been removed!\n");
    return false;
  }

  PointerRNA rptr;
  PropertyRNA *prop;
  PointerRNA id_ptr = RNA_id_pointer_create(animated_id);

  if (!RNA_path_resolve_property(&id_ptr, fcurve_in_copy_buffer.rna_path, &rptr, &prop)) {
    printf("paste_animedit_keys: failed to resolve path id:%s, '%s'!\n",
           animated_id->name,
           fcurve_in_copy_buffer.rna_path);
    return false;
  }

  const char *identifier = RNA_property_identifier(prop);
  /* NOTE: paths which end with "] will fail with this test - Animated ID Props. */
  return blender::StringRef(fcurve_to_match.rna_path).endswith(identifier);
}

bool pastebuf_match_index_only(Main * /*bmain*/,
                               const FCurve &fcurve_to_match,
                               const FCurve &fcurve_in_copy_buffer,
                               blender::animrig::slot_handle_t /* slot_handle_in_copy_buffer */,
                               const bool from_single,
                               const bool /*to_single*/,
                               const bool /*flip*/)
{
  return from_single || fcurve_in_copy_buffer.array_index == fcurve_to_match.array_index;
}

/* ................ */

static void do_curve_mirror_flippping(const FCurve &fcurve, BezTriple &bezt)
{
  if (!keyframe_copy_buffer->is_bone(fcurve)) {
    return;
  }

  /* TODO: pull the investigation of the RNA path out of this function, and out of the loop over
   * all keys of the F-Curve. It only has to be done once per F-Curve, and not for every single
   * key. */
  const size_t slength = strlen(fcurve.rna_path);
  bool flip = false;
  if (BLI_strn_endswith(fcurve.rna_path, "location", slength) && fcurve.array_index == 0) {
    flip = true;
  }
  else if (BLI_strn_endswith(fcurve.rna_path, "rotation_quaternion", slength) &&
           ELEM(fcurve.array_index, 2, 3))
  {
    flip = true;
  }
  else if (BLI_strn_endswith(fcurve.rna_path, "rotation_euler", slength) &&
           ELEM(fcurve.array_index, 1, 2))
  {
    flip = true;
  }
  else if (BLI_strn_endswith(fcurve.rna_path, "rotation_axis_angle", slength) &&
           ELEM(fcurve.array_index, 2, 3))
  {
    flip = true;
  }

  if (flip) {
    bezt.vec[0][1] = -bezt.vec[0][1];
    bezt.vec[1][1] = -bezt.vec[1][1];
    bezt.vec[2][1] = -bezt.vec[2][1];
  }
}

/* helper for paste_animedit_keys() - performs the actual pasting */
static void paste_animedit_keys_fcurve(FCurve *fcu,
                                       const FCurve &fcurve_in_copy_buffer,
                                       float offset[2],
                                       const eKeyMergeMode merge_mode,
                                       bool flip)
{
  BezTriple *bezt;
  int i;

  /* First de-select existing FCurve's keyframes */
  for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    BEZT_DESEL_ALL(bezt);
  }

  /* mix mode with existing data */
  switch (merge_mode) {
    case KEYFRAME_PASTE_MERGE_MIX:
      /* do-nothing */
      break;

    case KEYFRAME_PASTE_MERGE_OVER:
      /* remove all keys */
      BKE_fcurve_delete_keys_all(fcu);
      break;

    case KEYFRAME_PASTE_MERGE_OVER_RANGE:
    case KEYFRAME_PASTE_MERGE_OVER_RANGE_ALL: {
      float f_min;
      float f_max;

      if (merge_mode == KEYFRAME_PASTE_MERGE_OVER_RANGE) {
        f_min = fcurve_in_copy_buffer.bezt[0].vec[1][0] + offset[0];
        f_max = fcurve_in_copy_buffer.bezt[fcurve_in_copy_buffer.totvert - 1].vec[1][0] +
                offset[0];
      }
      else { /* Entire Range */
        f_min = keyframe_copy_buffer->first_frame + offset[0];
        f_max = keyframe_copy_buffer->last_frame + offset[0];
      }

      /* remove keys in range */
      if (f_min < f_max) {
        /* select verts in range for removal */
        for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
          if ((f_min < bezt[0].vec[1][0]) && (bezt[0].vec[1][0] < f_max)) {
            bezt->f2 |= SELECT;
          }
        }

        /* remove frames in the range */
        BKE_fcurve_delete_keys_selected(fcu);
      }
      break;
    }
  }

  /* just start pasting, with the first keyframe on the current frame, and so on */
  for (i = 0, bezt = fcurve_in_copy_buffer.bezt; i < fcurve_in_copy_buffer.totvert; i++, bezt++) {
    /* Create a copy to modify, before inserting it into the F-Curve. The
     * applied offset also determines the frame number of the pasted BezTriple.
     * If the insertion is done before the offset is applied, it will replace
     * the original key and _then_ move it to the new position. */
    BezTriple bezt_copy = *bezt;

    if (flip) {
      do_curve_mirror_flippping(fcurve_in_copy_buffer, bezt_copy);
    }

    add_v2_v2(bezt_copy.vec[0], offset);
    add_v2_v2(bezt_copy.vec[1], offset);
    add_v2_v2(bezt_copy.vec[2], offset);

    /* Ensure that all pasted data is selected. */
    BEZT_SEL_ALL(&bezt_copy);

    /* Only now that it has the right values, do the pasting into the F-Curve. */
    blender::animrig::insert_bezt_fcurve(fcu, &bezt_copy, INSERTKEY_OVERWRITE_FULL);
  }

  /* recalculate F-Curve's handles? */
  BKE_fcurve_handles_recalc(fcu);
}
}  // namespace blender::ed::animation

const EnumPropertyItem rna_enum_keyframe_paste_offset_items[] = {
    {KEYFRAME_PASTE_OFFSET_CFRA_START,
     "START",
     0,
     "Frame Start",
     "Paste keys starting at current frame"},
    {KEYFRAME_PASTE_OFFSET_CFRA_END, "END", 0, "Frame End", "Paste keys ending at current frame"},
    {KEYFRAME_PASTE_OFFSET_CFRA_RELATIVE,
     "RELATIVE",
     0,
     "Frame Relative",
     "Paste keys relative to the current frame when copying"},
    {KEYFRAME_PASTE_OFFSET_NONE, "NONE", 0, "No Offset", "Paste keys from original time"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_keyframe_paste_offset_value_items[] = {
    {KEYFRAME_PASTE_VALUE_OFFSET_LEFT_KEY,
     "LEFT_KEY",
     0,
     "Left Key",
     "Paste keys with the first key matching the key left of the cursor"},
    {KEYFRAME_PASTE_VALUE_OFFSET_RIGHT_KEY,
     "RIGHT_KEY",
     0,
     "Right Key",
     "Paste keys with the last key matching the key right of the cursor"},
    {KEYFRAME_PASTE_VALUE_OFFSET_CFRA,
     "CURRENT_FRAME",
     0,
     "Current Frame Value",
     "Paste keys relative to the value of the curve under the cursor"},
    {KEYFRAME_PASTE_VALUE_OFFSET_CURSOR,
     "CURSOR_VALUE",
     0,
     "Cursor Value",
     "Paste keys relative to the Y-Position of the cursor"},
    {KEYFRAME_PASTE_VALUE_OFFSET_NONE,
     "NONE",
     0,
     "No Offset",
     "Paste keys with the same value as they were copied"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_keyframe_paste_merge_items[] = {
    {KEYFRAME_PASTE_MERGE_MIX, "MIX", 0, "Mix", "Overlay existing with new keys"},
    {KEYFRAME_PASTE_MERGE_OVER, "OVER_ALL", 0, "Overwrite All", "Replace all keys"},
    {KEYFRAME_PASTE_MERGE_OVER_RANGE,
     "OVER_RANGE",
     0,
     "Overwrite Range",
     "Overwrite keys in pasted range"},
    {KEYFRAME_PASTE_MERGE_OVER_RANGE_ALL,
     "OVER_RANGE_ALL",
     0,
     "Overwrite Entire Range",
     "Overwrite keys in pasted range, using the range of all copied keys"},
    {0, nullptr, 0, nullptr, nullptr},
};

static float paste_get_y_offset(const bAnimContext *ac,
                                const FCurve &fcurve_in_copy_buffer,
                                const bAnimListElem *ale,
                                const eKeyPasteValueOffset value_offset_mode)
{
  BLI_assert(ale->datatype == ALE_FCURVE);
  const FCurve *fcu = static_cast<const FCurve *>(ale->data);
  const float cfra = BKE_scene_frame_get(ac->scene);

  switch (value_offset_mode) {
    case KEYFRAME_PASTE_VALUE_OFFSET_CURSOR: {
      const SpaceGraph *sipo = reinterpret_cast<SpaceGraph *>(ac->sl);
      const float offset = sipo->cursorVal - fcurve_in_copy_buffer.bezt[0].vec[1][1];
      return offset;
    }

    case KEYFRAME_PASTE_VALUE_OFFSET_CFRA: {
      const float cfra_y = evaluate_fcurve(fcu, cfra);
      const float offset = cfra_y - fcurve_in_copy_buffer.bezt[0].vec[1][1];
      return offset;
    }

    case KEYFRAME_PASTE_VALUE_OFFSET_LEFT_KEY: {
      bool replace;
      const int fcu_index = BKE_fcurve_bezt_binarysearch_index(
          fcu->bezt, cfra, fcu->totvert, &replace);
      const BezTriple left_key = fcu->bezt[max_ii(fcu_index - 1, 0)];
      const float offset = left_key.vec[1][1] - fcurve_in_copy_buffer.bezt[0].vec[1][1];
      return offset;
    }

    case KEYFRAME_PASTE_VALUE_OFFSET_RIGHT_KEY: {
      bool replace;
      const int fcu_index = BKE_fcurve_bezt_binarysearch_index(
          fcu->bezt, cfra, fcu->totvert, &replace);
      const BezTriple right_key = fcu->bezt[min_ii(fcu_index, fcu->totvert - 1)];
      const float offset = right_key.vec[1][1] -
                           fcurve_in_copy_buffer.bezt[fcurve_in_copy_buffer.totvert - 1].vec[1][1];
      return offset;
    }

    case KEYFRAME_PASTE_VALUE_OFFSET_NONE:
      break;
  }

  return 0.0f;
}

eKeyPasteError paste_animedit_keys(bAnimContext *ac,
                                   ListBase *anim_data,
                                   const KeyframePasteContext &paste_context)
{
  using namespace blender::ed::animation;

  if (!keyframe_copy_buffer || keyframe_copy_buffer->is_empty()) {
    return KEYFRAME_PASTE_NOTHING_TO_PASTE;
  }
  if (BLI_listbase_is_empty(anim_data)) {
    return KEYFRAME_PASTE_NOWHERE_TO_PASTE;
  }

  const Scene *scene = (ac->scene);
  const bool from_single = keyframe_copy_buffer->is_single_fcurve();
  const bool to_single = BLI_listbase_is_single(anim_data);
  float offset[2] = {0, 0};

  /* methods of offset */
  switch (paste_context.offset_mode) {
    case KEYFRAME_PASTE_OFFSET_CFRA_START:
      offset[0] = float(scene->r.cfra - keyframe_copy_buffer->first_frame);
      break;
    case KEYFRAME_PASTE_OFFSET_CFRA_END:
      offset[0] = float(scene->r.cfra - keyframe_copy_buffer->last_frame);
      break;
    case KEYFRAME_PASTE_OFFSET_CFRA_RELATIVE:
      offset[0] = float(scene->r.cfra - keyframe_copy_buffer->current_frame);
      break;
    case KEYFRAME_PASTE_OFFSET_NONE:
      offset[0] = 0.0f;
      break;
  }

  if (from_single && to_single) {
    /* 1:1 match, no tricky checking, just paste. */
    bAnimListElem *ale = static_cast<bAnimListElem *>(anim_data->first);
    FCurve *fcu = static_cast<FCurve *>(ale->data); /* destination F-Curve */
    const FCurve &fcurve_in_copy_buffer =
        *keyframe_copy_buffer->keyframe_data.channelbag(0)->fcurve(0);

    offset[1] = paste_get_y_offset(
        ac, fcurve_in_copy_buffer, ale, paste_context.value_offset_mode);

    ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcu, false, false);
    paste_animedit_keys_fcurve(
        fcu, fcurve_in_copy_buffer, offset, paste_context.merge_mode, false);
    ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcu, true, false);

    ale->update |= ANIM_UPDATE_DEFAULT;

    ANIM_animdata_update(ac, anim_data);

    return KEYFRAME_PASTE_OK;
  }

  /* Try to find "matching" channels to paste keyframes into with increasingly
   * loose matching heuristics. The process finishes when at least one F-Curve
   * has been pasted into. */
  Vector<pastebuf_match_func> matchers = {
      pastebuf_match_path_full, pastebuf_match_path_property, pastebuf_match_index_only};

  for (const pastebuf_match_func matcher : matchers) {
    bool found_match = false;

    LISTBASE_FOREACH (bAnimListElem *, ale, anim_data) {
      /* See if there is an F-Curve in the copy buffer that matches this ALE. */
      const FCurve *fcurve_in_copy_buffer = pastebuf_find_matching_copybuf_item(
          matcher, ac->bmain, *ale, from_single, to_single, paste_context);
      if (!fcurve_in_copy_buffer) {
        continue;
      }

      /* Copy the relevant data from the matching buffer curve. */
      offset[1] = paste_get_y_offset(
          ac, *fcurve_in_copy_buffer, ale, paste_context.value_offset_mode);

      /* Do the actual pasting. */
      FCurve *fcurve_to_paste_into = static_cast<FCurve *>(ale->data);
      ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve_to_paste_into, false, false);
      paste_animedit_keys_fcurve(fcurve_to_paste_into,
                                 *fcurve_in_copy_buffer,
                                 offset,
                                 paste_context.merge_mode,
                                 paste_context.flip);
      ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve_to_paste_into, true, false);

      found_match = true;
      ale->update |= ANIM_UPDATE_DEFAULT;
    }

    /* Don't continue if some fcurves were pasted. */
    if (found_match) {
      break;
    }
  }

  ANIM_animdata_update(ac, anim_data);

  return KEYFRAME_PASTE_OK;
}

/** \} */
