/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_anim_types.h"
#include "DNA_screen_types.h"

#include "BLT_translation.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h" /* windows needs for M_PI */
#include "BLI_noise.h"
#include "BLI_utildefines.h"

#include "BKE_fcurve.h"
#include "BKE_idprop.h"

static CLG_LogRef LOG = {"bke.fmodifier"};

/* -------------------------------------------------------------------- */
/** \name F-Curve Modifier Types
 * \{ */

/* Info ------------------------------- */

/* F-Modifiers are modifiers which operate on F-Curves. However, they can also be defined
 * on NLA-Strips to affect all of the F-Curves referenced by the NLA-Strip.
 */

/* Template --------------------------- */

/* Each modifier defines a set of functions, which will be called at the appropriate
 * times. In addition to this, each modifier should have a type-info struct, where
 * its functions are attached for use.
 */

/* Template for type-info data:
 * - make a copy of this when creating new modifiers, and just change the functions
 *   pointed to as necessary
 * - although the naming of functions doesn't matter, it would help for code
 *   readability, to follow the same naming convention as is presented here
 * - any functions that a constraint doesn't need to define, don't define
 *   for such cases, just use nullptr
 * - these should be defined after all the functions have been defined, so that
 *   forward-definitions/prototypes don't need to be used!
 * - keep this copy #if-def'd so that future modifier can get based off this
 */
#if 0
static FModifierTypeInfo FMI_MODNAME = {
    /*type*/ FMODIFIER_TYPE_MODNAME,
    /*size*/ sizeof(FMod_ModName),
    /*acttype*/ FMI_TYPE_SOME_ACTION,
    /*requires_flag*/ FMI_REQUIRES_SOME_REQUIREMENT,
    /*name*/ "Modifier Name",
    /*structName*/ "FMod_ModName",
    /*storage_size*/ 0,
    /*free_data*/ fcm_modname_free,
    /*copy_data*/ fcm_modname_copy,
    /*new_data*/ fcm_modname_new_data,
    /*verify_data*/ fcm_modname_verify,
    /*evaluate_modifier_time*/ fcm_modname_time,
    /*evaluate_modifier*/ fcm_modname_evaluate,
};
#endif

/* Generator F-Curve Modifier --------------------------- */

/* Generators available:
 *  1) simple polynomial generator:
 *     - Expanded form:
 *       (y = C[0]*(x^(n)) + C[1]*(x^(n-1)) + ... + C[n])
 *     - Factorized form:
 *       (y = (C[0][0]*x + C[0][1]) * (C[1][0]*x + C[1][1]) * ... * (C[n][0]*x + C[n][1]))
 */

static void fcm_generator_free(FModifier *fcm)
{
  FMod_Generator *data = (FMod_Generator *)fcm->data;

  /* free polynomial coefficients array */
  if (data->coefficients) {
    MEM_freeN(data->coefficients);
  }
}

static void fcm_generator_copy(FModifier *fcm, const FModifier *src)
{
  FMod_Generator *gen = (FMod_Generator *)fcm->data;
  FMod_Generator *ogen = (FMod_Generator *)src->data;

  /* copy coefficients array? */
  if (ogen->coefficients) {
    gen->coefficients = static_cast<float *>(MEM_dupallocN(ogen->coefficients));
  }
}

static void fcm_generator_new_data(void *mdata)
{
  FMod_Generator *data = (FMod_Generator *)mdata;
  float *cp;

  /* set default generator to be linear 0-1 (gradient = 1, y-offset = 0) */
  data->poly_order = 1;
  data->arraysize = 2;
  cp = data->coefficients = static_cast<float *>(
      MEM_callocN(sizeof(float) * 2, "FMod_Generator_Coefs"));
  cp[0] = 0; /* y-offset */
  cp[1] = 1; /* gradient */
}

static void fcm_generator_verify(FModifier *fcm)
{
  FMod_Generator *data = (FMod_Generator *)fcm->data;

  /* requirements depend on mode */
  switch (data->mode) {
    case FCM_GENERATOR_POLYNOMIAL: /* expanded polynomial expression */
    {
      const int arraysize_new = data->poly_order + 1;
      /* arraysize needs to be order+1, so resize if not */
      if (data->arraysize != arraysize_new) {
        data->coefficients = static_cast<float *>(
            MEM_recallocN(data->coefficients, sizeof(float) * arraysize_new));
        data->arraysize = arraysize_new;
      }
      break;
    }
    case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* expanded polynomial expression */
    {
      const int arraysize_new = data->poly_order * 2;
      /* arraysize needs to be (2 * order), so resize if not */
      if (data->arraysize != arraysize_new) {
        data->coefficients = static_cast<float *>(
            MEM_recallocN(data->coefficients, sizeof(float) * arraysize_new));
        data->arraysize = arraysize_new;
      }
      break;
    }
  }
}

static void fcm_generator_evaluate(
    FCurve * /*fcu*/, FModifier *fcm, float *cvalue, float evaltime, void * /*storage*/)
{
  FMod_Generator *data = (FMod_Generator *)fcm->data;

  /* behavior depends on mode
   * NOTE: the data in its default state is fine too
   */
  switch (data->mode) {
    case FCM_GENERATOR_POLYNOMIAL: /* expanded polynomial expression */
    {
      /* we overwrite cvalue with the sum of the polynomial */
      float *powers = static_cast<float *>(
          MEM_callocN(sizeof(float) * data->arraysize, "Poly Powers"));
      float value = 0.0f;

      /* for each x^n, precalculate value based on previous one first... this should be
       * faster that calling pow() for each entry
       */
      for (uint i = 0; i < data->arraysize; i++) {
        /* first entry is x^0 = 1, otherwise, calculate based on previous */
        if (i) {
          powers[i] = powers[i - 1] * evaltime;
        }
        else {
          powers[0] = 1;
        }
      }

      /* for each coefficient, add to value, which we'll write to *cvalue in one go */
      for (uint i = 0; i < data->arraysize; i++) {
        value += data->coefficients[i] * powers[i];
      }

      /* only if something changed, write *cvalue in one go */
      if (data->poly_order) {
        if (data->flag & FCM_GENERATOR_ADDITIVE) {
          *cvalue += value;
        }
        else {
          *cvalue = value;
        }
      }

      /* cleanup */
      if (powers) {
        MEM_freeN(powers);
      }
      break;
    }
    case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* Factorized polynomial */
    {
      float value = 1.0f, *cp = nullptr;
      uint i;

      /* For each coefficient pair,
       * solve for that bracket before accumulating in value by multiplying. */
      for (cp = data->coefficients, i = 0; (cp) && (i < uint(data->poly_order)); cp += 2, i++) {
        value *= (cp[0] * evaltime + cp[1]);
      }

      /* only if something changed, write *cvalue in one go */
      if (data->poly_order) {
        if (data->flag & FCM_GENERATOR_ADDITIVE) {
          *cvalue += value;
        }
        else {
          *cvalue = value;
        }
      }
      break;
    }
  }
}

static FModifierTypeInfo FMI_GENERATOR = {
    /*type*/ FMODIFIER_TYPE_GENERATOR,
    /*size*/ sizeof(FMod_Generator),
    /*acttype*/ FMI_TYPE_GENERATE_CURVE,
    /*requires_flag*/ FMI_REQUIRES_NOTHING,
    /*name*/ N_("Generator"),
    /*structName*/ "FMod_Generator",
    /*storage_size*/ 0,
    /*free_data*/ fcm_generator_free,
    /*copy_data*/ fcm_generator_copy,
    /*new_data*/ fcm_generator_new_data,
    /*verify_data*/ fcm_generator_verify,
    /*evaluate_modifier_time*/ nullptr,
    /*evaluate_modifier*/ fcm_generator_evaluate,
};

/* Built-In Function Generator F-Curve Modifier --------------------------- */

/* This uses the general equation for equations:
 *   y = amplitude * fn(phase_multiplier * x + phase_offset) + y_offset
 *
 * where amplitude, phase_multiplier/offset, y_offset are user-defined coefficients,
 * x is the evaluation 'time', and 'y' is the resultant value
 *
 * Functions available are
 * sin, cos, tan, sinc (normalized sin), natural log, square root
 */

static void fcm_fn_generator_new_data(void *mdata)
{
  FMod_FunctionGenerator *data = (FMod_FunctionGenerator *)mdata;

  /* set amplitude and phase multiplier to 1.0f so that something is generated */
  data->amplitude = 1.0f;
  data->phase_multiplier = 1.0f;
}

/* Unary 'normalized sine' function
 * y = sin(PI + x) / (PI * x),
 * except for x = 0 when y = 1.
 */
static double sinc(double x)
{
  if (fabs(x) < 0.0001) {
    return 1.0;
  }

  return sin(M_PI * x) / (M_PI * x);
}

static void fcm_fn_generator_evaluate(
    FCurve * /*fcu*/, FModifier *fcm, float *cvalue, float evaltime, void * /*storage*/)
{
  FMod_FunctionGenerator *data = (FMod_FunctionGenerator *)fcm->data;
  double arg = data->phase_multiplier * evaltime + data->phase_offset;
  double (*fn)(double v) = nullptr;

  /* get function pointer to the func to use:
   * WARNING: must perform special argument validation hereto guard against crashes
   */
  switch (data->type) {
    /* simple ones */
    case FCM_GENERATOR_FN_SIN: /* sine wave */
      fn = sin;
      break;
    case FCM_GENERATOR_FN_COS: /* cosine wave */
      fn = cos;
      break;
    case FCM_GENERATOR_FN_SINC: /* normalized sine wave */
      fn = sinc;
      break;

    /* validation required */
    case FCM_GENERATOR_FN_TAN: /* tangent wave */
    {
      /* check that argument is not on one of the discontinuities (i.e. 90deg, 270 deg, etc) */
      if (IS_EQ(fmod((arg - M_PI_2), M_PI), 0.0)) {
        if ((data->flag & FCM_GENERATOR_ADDITIVE) == 0) {
          *cvalue = 0.0f; /* no value possible here */
        }
      }
      else {
        fn = tan;
      }
      break;
    }
    case FCM_GENERATOR_FN_LN: /* natural log */
    {
      /* check that value is greater than 1? */
      if (arg > 1.0) {
        fn = log;
      }
      else {
        if ((data->flag & FCM_GENERATOR_ADDITIVE) == 0) {
          *cvalue = 0.0f; /* no value possible here */
        }
      }
      break;
    }
    case FCM_GENERATOR_FN_SQRT: /* square root */
    {
      /* no negative numbers */
      if (arg > 0.0) {
        fn = sqrt;
      }
      else {
        if ((data->flag & FCM_GENERATOR_ADDITIVE) == 0) {
          *cvalue = 0.0f; /* no value possible here */
        }
      }
      break;
    }
    default:
      CLOG_ERROR(&LOG, "Invalid Function-Generator for F-Modifier - %d", data->type);
      break;
  }

  /* execute function callback to set value if appropriate */
  if (fn) {
    float value = float(data->amplitude * float(fn(arg)) + data->value_offset);

    if (data->flag & FCM_GENERATOR_ADDITIVE) {
      *cvalue += value;
    }
    else {
      *cvalue = value;
    }
  }
}

static FModifierTypeInfo FMI_FN_GENERATOR = {
    /*type*/ FMODIFIER_TYPE_FN_GENERATOR,
    /*size*/ sizeof(FMod_FunctionGenerator),
    /*acttype*/ FMI_TYPE_GENERATE_CURVE,
    /*requires_flag*/ FMI_REQUIRES_NOTHING,
    /*name*/ N_("Built-In Function"),
    /*structName*/ "FMod_FunctionGenerator",
    /*storage_size*/ 0,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*new_data*/ fcm_fn_generator_new_data,
    /*verify_data*/ nullptr,
    /*evaluate_modifier_time*/ nullptr,
    /*evaluate_modifier*/ fcm_fn_generator_evaluate,
};

/* Envelope F-Curve Modifier --------------------------- */

static void fcm_envelope_free(FModifier *fcm)
{
  FMod_Envelope *env = (FMod_Envelope *)fcm->data;

  /* free envelope data array */
  if (env->data) {
    MEM_freeN(env->data);
  }
}

static void fcm_envelope_copy(FModifier *fcm, const FModifier *src)
{
  FMod_Envelope *env = (FMod_Envelope *)fcm->data;
  FMod_Envelope *oenv = (FMod_Envelope *)src->data;

  /* copy envelope data array */
  if (oenv->data) {
    env->data = static_cast<FCM_EnvelopeData *>(MEM_dupallocN(oenv->data));
  }
}

static void fcm_envelope_new_data(void *mdata)
{
  FMod_Envelope *env = (FMod_Envelope *)mdata;

  /* set default min/max ranges */
  env->min = -1.0f;
  env->max = 1.0f;
}

static void fcm_envelope_verify(FModifier *fcm)
{
  FMod_Envelope *env = (FMod_Envelope *)fcm->data;

  /* if the are points, perform bubble-sort on them, as user may have changed the order */
  if (env->data) {
    /* XXX todo... */
  }
}

static void fcm_envelope_evaluate(
    FCurve * /*fcu*/, FModifier *fcm, float *cvalue, float evaltime, void * /*storage*/)
{
  FMod_Envelope *env = (FMod_Envelope *)fcm->data;
  FCM_EnvelopeData *fed, *prevfed, *lastfed;
  float min = 0.0f, max = 0.0f, fac = 0.0f;
  int a;

  /* get pointers */
  if (env->data == nullptr) {
    return;
  }
  prevfed = env->data;
  fed = prevfed + 1;
  lastfed = prevfed + (env->totvert - 1);

  /* get min/max values for envelope at evaluation time (relative to mid-value) */
  if (prevfed->time >= evaltime) {
    /* before or on first sample, so just extend value */
    min = prevfed->min;
    max = prevfed->max;
  }
  else if (lastfed->time <= evaltime) {
    /* after or on last sample, so just extend value */
    min = lastfed->min;
    max = lastfed->max;
  }
  else {
    /* evaltime occurs somewhere between segments */
    /* TODO: implement binary search for this to make it faster? */
    for (a = 0; prevfed && fed && (a < env->totvert - 1); a++, prevfed = fed, fed++) {
      /* evaltime occurs within the interval defined by these two envelope points */
      if ((prevfed->time <= evaltime) && (fed->time >= evaltime)) {
        float afac, bfac, diff;

        diff = fed->time - prevfed->time;
        afac = (evaltime - prevfed->time) / diff;
        bfac = (fed->time - evaltime) / diff;

        min = bfac * prevfed->min + afac * fed->min;
        max = bfac * prevfed->max + afac * fed->max;

        break;
      }
    }
  }

  /* adjust *cvalue
   * - fac is the ratio of how the current y-value corresponds to the reference range
   * - thus, the new value is found by mapping the old range to the new!
   */
  fac = (*cvalue - (env->midval + env->min)) / (env->max - env->min);
  *cvalue = min + fac * (max - min);
}

static FModifierTypeInfo FMI_ENVELOPE = {
    /*type*/ FMODIFIER_TYPE_ENVELOPE,
    /*size*/ sizeof(FMod_Envelope),
    /*acttype*/ FMI_TYPE_REPLACE_VALUES,
    /*requires_flag*/ 0,
    /*name*/ N_("Envelope"),
    /*structName*/ "FMod_Envelope",
    /*storage_size*/ 0,
    /*free_data*/ fcm_envelope_free,
    /*copy_data*/ fcm_envelope_copy,
    /*new_data*/ fcm_envelope_new_data,
    /*verify_data*/ fcm_envelope_verify,
    /*evaluate_modifier_time*/ nullptr,
    /*evaluate_modifier*/ fcm_envelope_evaluate,
};

/* exported function for finding points */

/* Binary search algorithm for finding where to insert Envelope Data Point.
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
#define BINARYSEARCH_FRAMEEQ_THRESH 0.0001f

int BKE_fcm_envelope_find_index(FCM_EnvelopeData array[],
                                float frame,
                                int arraylen,
                                bool *r_exists)
{
  int start = 0, end = arraylen;
  int loopbreaker = 0, maxloop = arraylen * 2;

  /* initialize exists-flag first */
  *r_exists = false;

  /* sneaky optimizations (don't go through searching process if...):
   * - keyframe to be added is to be added out of current bounds
   * - keyframe to be added would replace one of the existing ones on bounds
   */
  if ((arraylen <= 0) || (array == nullptr)) {
    CLOG_WARN(&LOG, "encountered invalid array");
    return 0;
  }

  /* check whether to add before/after/on */
  float framenum;

  /* 'First' Point (when only one point, this case is used) */
  framenum = array[0].time;
  if (IS_EQT(frame, framenum, BINARYSEARCH_FRAMEEQ_THRESH)) {
    *r_exists = true;
    return 0;
  }
  if (frame < framenum) {
    return 0;
  }

  /* 'Last' Point */
  framenum = array[(arraylen - 1)].time;
  if (IS_EQT(frame, framenum, BINARYSEARCH_FRAMEEQ_THRESH)) {
    *r_exists = true;
    return (arraylen - 1);
  }
  if (frame > framenum) {
    return arraylen;
  }

  /* most of the time, this loop is just to find where to put it
   * - 'loopbreaker' is just here to prevent infinite loops
   */
  for (loopbreaker = 0; (start <= end) && (loopbreaker < maxloop); loopbreaker++) {
    /* compute and get midpoint */

    /* we calculate the midpoint this way to avoid int overflows... */
    int mid = start + ((end - start) / 2);

    float midfra = array[mid].time;

    /* check if exactly equal to midpoint */
    if (IS_EQT(frame, midfra, BINARYSEARCH_FRAMEEQ_THRESH)) {
      *r_exists = true;
      return mid;
    }

    /* repeat in upper/lower half */
    if (frame > midfra) {
      start = mid + 1;
    }
    else if (frame < midfra) {
      end = mid - 1;
    }
  }

  /* print error if loop-limit exceeded */
  if (loopbreaker == (maxloop - 1)) {
    CLOG_ERROR(&LOG, "binary search was taking too long");

    /* Include debug info. */
    CLOG_ERROR(&LOG,
               "\tround = %d: start = %d, end = %d, arraylen = %d",
               loopbreaker,
               start,
               end,
               arraylen);
  }

  /* not found, so return where to place it */
  return start;
}
#undef BINARYSEARCH_FRAMEEQ_THRESH

/* Cycles F-Curve Modifier  --------------------------- */

/* This modifier changes evaltime to something that exists within the curve's frame-range,
 * then re-evaluates modifier stack up to this point using the new time. This re-entrant behavior
 * is very likely to be more time-consuming than the original approach...
 * (which was tightly integrated into the calculation code...).
 *
 * NOTE: this needs to be at the start of the stack to be of use,
 * as it needs to know the extents of the keyframes/sample-data.
 *
 * Possible TODO: store length of cycle information that can be initialized from the extents of
 * the keyframes/sample-data, and adjusted as appropriate.
 */

/* temp data used during evaluation */
struct tFCMED_Cycles {
  float cycyofs; /* y-offset to apply */
};

static void fcm_cycles_new_data(void *mdata)
{
  FMod_Cycles *data = (FMod_Cycles *)mdata;

  /* turn on cycles by default */
  data->before_mode = data->after_mode = FCM_EXTRAPOLATE_CYCLIC;
}

static float fcm_cycles_time(
    FCurve *fcu, FModifier *fcm, float /*cvalue*/, float evaltime, void *storage_)
{
  const FMod_Cycles *data = (FMod_Cycles *)fcm->data;
  tFCMED_Cycles *storage = static_cast<tFCMED_Cycles *>(storage_);
  float prevkey[2], lastkey[2], cycyofs = 0.0f;
  short side = 0, mode = 0;
  int cycles = 0;
  float ofs = 0;

  /* Initialize storage. */
  storage->cycyofs = 0;

  /* check if modifier is first in stack, otherwise disable ourself... */
  /* FIXME... */
  if (fcm->prev) {
    fcm->flag |= FMODIFIER_FLAG_DISABLED;
    return evaltime;
  }

  if (fcu == nullptr || (fcu->bezt == nullptr && fcu->fpt == nullptr)) {
    return evaltime;
  }

  /* calculate new evaltime due to cyclic interpolation */
  if (fcu->bezt) {
    const BezTriple *prevbezt = fcu->bezt;
    const BezTriple *lastbezt = prevbezt + fcu->totvert - 1;

    prevkey[0] = prevbezt->vec[1][0];
    prevkey[1] = prevbezt->vec[1][1];

    lastkey[0] = lastbezt->vec[1][0];
    lastkey[1] = lastbezt->vec[1][1];
  }
  else {
    BLI_assert(fcu->fpt != nullptr);
    const FPoint *prevfpt = fcu->fpt;
    const FPoint *lastfpt = prevfpt + fcu->totvert - 1;

    prevkey[0] = prevfpt->vec[0];
    prevkey[1] = prevfpt->vec[1];

    lastkey[0] = lastfpt->vec[0];
    lastkey[1] = lastfpt->vec[1];
  }

  /* check if modifier will do anything
   * 1) if in data range, definitely don't do anything
   * 2) if before first frame or after last frame, make sure some cycling is in use
   */
  if (evaltime < prevkey[0]) {
    if (data->before_mode) {
      side = -1;
      mode = data->before_mode;
      cycles = data->before_cycles;
      ofs = prevkey[0];
    }
  }
  else if (evaltime > lastkey[0]) {
    if (data->after_mode) {
      side = 1;
      mode = data->after_mode;
      cycles = data->after_cycles;
      ofs = lastkey[0];
    }
  }
  if (ELEM(0, side, mode)) {
    return evaltime;
  }

  /* find relative place within a cycle */
  {
    /* calculate period and amplitude (total height) of a cycle */
    const float cycdx = lastkey[0] - prevkey[0];
    const float cycdy = lastkey[1] - prevkey[1];

    /* check if cycle is infinitely small, to be point of being impossible to use */
    if (cycdx == 0) {
      return evaltime;
    }

    /* calculate the 'number' of the cycle */
    const float cycle = (float(side) * (evaltime - ofs) / cycdx);

    /* calculate the time inside the cycle */
    const float cyct = fmod(evaltime - ofs, cycdx);

    /* check that cyclic is still enabled for the specified time */
    if (cycles == 0) {
      /* catch this case so that we don't exit when we have (cycles = 0)
       * as this indicates infinite cycles...
       */
    }
    else if (cycle > cycles) {
      /* we are too far away from range to evaluate
       * TODO: but we should still hold last value...
       */
      return evaltime;
    }

    /* check if 'cyclic extrapolation', and thus calculate y-offset for this cycle */
    if (mode == FCM_EXTRAPOLATE_CYCLIC_OFFSET) {
      if (side < 0) {
        cycyofs = float(floor((evaltime - ofs) / cycdx));
      }
      else {
        cycyofs = float(ceil((evaltime - ofs) / cycdx));
      }
      cycyofs *= cycdy;
    }

    /* special case for cycle start/end */
    if (cyct == 0.0f) {
      evaltime = (side == 1 ? lastkey[0] : prevkey[0]);

      if ((mode == FCM_EXTRAPOLATE_MIRROR) && (int(cycle) % 2)) {
        evaltime = (side == 1 ? prevkey[0] : lastkey[0]);
      }
    }
    /* calculate where in the cycle we are (overwrite evaltime to reflect this) */
    else if ((mode == FCM_EXTRAPOLATE_MIRROR) && (int(cycle + 1) % 2)) {
      /* When 'mirror' option is used and cycle number is odd, this cycle is played in reverse
       * - for 'before' extrapolation, we need to flip in a different way, otherwise values past
       *   then end of the curve get referenced
       *   (result of fmod will be negative, and with different phase).
       */
      if (side < 0) {
        evaltime = prevkey[0] - cyct;
      }
      else {
        evaltime = lastkey[0] - cyct;
      }
    }
    else {
      /* the cycle is played normally... */
      evaltime = prevkey[0] + cyct;
    }
    if (evaltime < prevkey[0]) {
      evaltime += cycdx;
    }
  }

  /* store temp data if needed */
  if (mode == FCM_EXTRAPOLATE_CYCLIC_OFFSET) {
    storage->cycyofs = cycyofs;
  }

  /* return the new frame to evaluate */
  return evaltime;
}

static void fcm_cycles_evaluate(
    FCurve * /*fcu*/, FModifier * /*fcm*/, float *cvalue, float /*evaltime*/, void *storage_)
{
  tFCMED_Cycles *storage = static_cast<tFCMED_Cycles *>(storage_);
  *cvalue += storage->cycyofs;
}

static FModifierTypeInfo FMI_CYCLES = {
    /*type*/ FMODIFIER_TYPE_CYCLES,
    /*size*/ sizeof(FMod_Cycles),
    /*acttype*/ FMI_TYPE_EXTRAPOLATION,
    /*requires_flag*/ FMI_REQUIRES_ORIGINAL_DATA,
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_ACTION, "Cycles"),
    /*structName*/ "FMod_Cycles",
    /*storage_size*/ sizeof(tFCMED_Cycles),
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*new_data*/ fcm_cycles_new_data,
    /*verify_data*/ nullptr /*fcm_cycles_verify*/,
    /*evaluate_modifier_time*/ fcm_cycles_time,
    /*evaluate_modifier*/ fcm_cycles_evaluate,
};

/* Noise F-Curve Modifier  --------------------------- */

static void fcm_noise_new_data(void *mdata)
{
  FMod_Noise *data = (FMod_Noise *)mdata;

  /* defaults */
  data->size = 1.0f;
  data->strength = 1.0f;
  data->phase = 1.0f;
  data->offset = 0.0f;
  data->depth = 0;
  data->modification = FCM_NOISE_MODIF_REPLACE;
}

static void fcm_noise_evaluate(
    FCurve * /*fcu*/, FModifier *fcm, float *cvalue, float evaltime, void * /*storage*/)
{
  FMod_Noise *data = (FMod_Noise *)fcm->data;
  float noise;

  /* generate noise using good old Blender Noise
   * - 0.1 is passed as the 'z' value, otherwise evaluation fails for size = phase = 1
   *   with evaltime being an integer (which happens when evaluating on frame by frame basis)
   */
  noise = BLI_noise_turbulence(
      data->size, evaltime - data->offset, data->phase, 0.1f, data->depth);

  /* combine the noise with existing motion data */
  switch (data->modification) {
    case FCM_NOISE_MODIF_ADD:
      *cvalue = *cvalue + noise * data->strength;
      break;
    case FCM_NOISE_MODIF_SUBTRACT:
      *cvalue = *cvalue - noise * data->strength;
      break;
    case FCM_NOISE_MODIF_MULTIPLY:
      *cvalue = *cvalue * noise * data->strength;
      break;
    case FCM_NOISE_MODIF_REPLACE:
    default:
      *cvalue = *cvalue + (noise - 0.5f) * data->strength;
      break;
  }
}

static FModifierTypeInfo FMI_NOISE = {
    /*type*/ FMODIFIER_TYPE_NOISE,
    /*size*/ sizeof(FMod_Noise),
    /*acttype*/ FMI_TYPE_REPLACE_VALUES,
    /*requires_flag*/ 0,
    /*name*/ N_("Noise"),
    /*structName*/ "FMod_Noise",
    /*storage_size*/ 0,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*new_data*/ fcm_noise_new_data,
    /*verify_data*/ nullptr /*fcm_noise_verify*/,
    /*evaluate_modifier_time*/ nullptr,
    /*evaluate_modifier*/ fcm_noise_evaluate,
};

/* Python F-Curve Modifier --------------------------- */

static void fcm_python_free(FModifier *fcm)
{
  FMod_Python *data = (FMod_Python *)fcm->data;

  /* id-properties */
  IDP_FreeProperty(data->prop);
}

static void fcm_python_new_data(void *mdata)
{
  FMod_Python *data = (FMod_Python *)mdata;

  /* Everything should be set correctly by calloc, except for the prop->type constant. */
  data->prop = static_cast<IDProperty *>(MEM_callocN(sizeof(IDProperty), "PyFModifierProps"));
  data->prop->type = IDP_GROUP;
}

static void fcm_python_copy(FModifier *fcm, const FModifier *src)
{
  FMod_Python *pymod = (FMod_Python *)fcm->data;
  FMod_Python *opymod = (FMod_Python *)src->data;

  pymod->prop = IDP_CopyProperty(opymod->prop);
}

static void fcm_python_evaluate(FCurve * /*fcu*/,
                                FModifier * /*fcm*/,
                                float * /*cvalue*/,
                                float /*evaltime*/,
                                void * /*storage*/)
{
#ifdef WITH_PYTHON
// FMod_Python *data = (FMod_Python *)fcm->data;

/* FIXME... need to implement this modifier...
 * It will need it execute a script using the custom properties
 */
#endif /* WITH_PYTHON */
}

static FModifierTypeInfo FMI_PYTHON = {
    /*type*/ FMODIFIER_TYPE_PYTHON,
    /*size*/ sizeof(FMod_Python),
    /*acttype*/ FMI_TYPE_GENERATE_CURVE,
    /*requires_flag*/ FMI_REQUIRES_RUNTIME_CHECK,
    /*name*/ N_("Python"),
    /*structName*/ "FMod_Python",
    /*storage_size*/ 0,
    /*free_data*/ fcm_python_free,
    /*copy_data*/ fcm_python_copy,
    /*new_data*/ fcm_python_new_data,
    /*verify_data*/ nullptr /*fcm_python_verify*/,
    /*evaluate_modifier_time*/ nullptr /*fcm_python_time*/,
    /*evaluate_modifier*/ fcm_python_evaluate,
};

/* Limits F-Curve Modifier --------------------------- */

static float fcm_limits_time(
    FCurve * /*fcu*/, FModifier *fcm, float /*cvalue*/, float evaltime, void * /*storage*/)
{
  FMod_Limits *data = (FMod_Limits *)fcm->data;

  /* check for the time limits */
  if ((data->flag & FCM_LIMIT_XMIN) && (evaltime < data->rect.xmin)) {
    return data->rect.xmin;
  }
  if ((data->flag & FCM_LIMIT_XMAX) && (evaltime > data->rect.xmax)) {
    return data->rect.xmax;
  }

  /* modifier doesn't change time */
  return evaltime;
}

static void fcm_limits_evaluate(
    FCurve * /*fcu*/, FModifier *fcm, float *cvalue, float /*evaltime*/, void * /*storage*/)
{
  FMod_Limits *data = (FMod_Limits *)fcm->data;

  /* value limits now */
  if ((data->flag & FCM_LIMIT_YMIN) && (*cvalue < data->rect.ymin)) {
    *cvalue = data->rect.ymin;
  }
  if ((data->flag & FCM_LIMIT_YMAX) && (*cvalue > data->rect.ymax)) {
    *cvalue = data->rect.ymax;
  }
}

static FModifierTypeInfo FMI_LIMITS = {
    /*type*/ FMODIFIER_TYPE_LIMITS,
    /*size*/ sizeof(FMod_Limits),
    /*acttype*/ FMI_TYPE_GENERATE_CURVE,
    /*requires_flag*/ FMI_REQUIRES_RUNTIME_CHECK, /* XXX... err... */
    /*name*/ N_("Limits"),
    /*structName*/ "FMod_Limits",
    /*storage_size*/ 0,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*new_data*/ nullptr,
    /*verify_data*/ nullptr,
    /*evaluate_modifier_time*/ fcm_limits_time,
    /*evaluate_modifier*/ fcm_limits_evaluate,
};

/* Stepped F-Curve Modifier --------------------------- */

static void fcm_stepped_new_data(void *mdata)
{
  FMod_Stepped *data = (FMod_Stepped *)mdata;

  /* just need to set the step-size to 2-frames by default */
  /* XXX: or would 5 be more normal? */
  data->step_size = 2.0f;
}

static float fcm_stepped_time(
    FCurve * /*fcu*/, FModifier *fcm, float /*cvalue*/, float evaltime, void * /*storage*/)
{
  FMod_Stepped *data = (FMod_Stepped *)fcm->data;
  int snapblock;

  /* check range clamping to see if we should alter the timing to achieve the desired results */
  if (data->flag & FCM_STEPPED_NO_BEFORE) {
    if (evaltime < data->start_frame) {
      return evaltime;
    }
  }
  if (data->flag & FCM_STEPPED_NO_AFTER) {
    if (evaltime > data->end_frame) {
      return evaltime;
    }
  }

  /* we snap to the start of the previous closest block of 'step_size' frames
   * after the start offset has been discarded
   * - i.e. round down
   */
  snapblock = int((evaltime - data->offset) / data->step_size);

  /* reapply the offset, and multiple the snapblock by the size of the steps to get
   * the new time to evaluate at
   */
  return (float(snapblock) * data->step_size) + data->offset;
}

static FModifierTypeInfo FMI_STEPPED = {
    /*type*/ FMODIFIER_TYPE_STEPPED,
    /*size*/ sizeof(FMod_Limits),
    /*acttype*/ FMI_TYPE_GENERATE_CURVE,
    /*requires_flag*/ FMI_REQUIRES_RUNTIME_CHECK, /* XXX... err... */
    /*name*/ N_("Stepped"),
    /*structName*/ "FMod_Stepped",
    /*storage_size*/ 0,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*new_data*/ fcm_stepped_new_data,
    /*verify_data*/ nullptr,
    /*evaluate_modifier_time*/ fcm_stepped_time,
    /*evaluate_modifier*/ nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name F-Curve Modifier Type API
 *
 * all of the f-curve modifier api functions use #fmodifiertypeinfo structs to carry out
 * and operations that involve f-curve modifier specific code.
 * \{ */

/* These globals only ever get directly accessed in this file */
static FModifierTypeInfo *fmodifiersTypeInfo[FMODIFIER_NUM_TYPES];
static short FMI_INIT = 1; /* when non-zero, the list needs to be updated */

/** This function only gets called when #FMI_INIT is non-zero. */
static void fmods_init_typeinfo()
{
  fmodifiersTypeInfo[0] = nullptr;           /* 'Null' F-Curve Modifier */
  fmodifiersTypeInfo[1] = &FMI_GENERATOR;    /* Generator F-Curve Modifier */
  fmodifiersTypeInfo[2] = &FMI_FN_GENERATOR; /* Built-In Function Generator F-Curve Modifier */
  fmodifiersTypeInfo[3] = &FMI_ENVELOPE;     /* Envelope F-Curve Modifier */
  fmodifiersTypeInfo[4] = &FMI_CYCLES;       /* Cycles F-Curve Modifier */
  fmodifiersTypeInfo[5] = &FMI_NOISE;        /* Apply-Noise F-Curve Modifier */
  fmodifiersTypeInfo[6] = nullptr /*&FMI_FILTER*/;
  /* Filter F-Curve Modifier */         /* XXX unimplemented. */
  fmodifiersTypeInfo[7] = &FMI_PYTHON;  /* Custom Python F-Curve Modifier */
  fmodifiersTypeInfo[8] = &FMI_LIMITS;  /* Limits F-Curve Modifier */
  fmodifiersTypeInfo[9] = &FMI_STEPPED; /* Stepped F-Curve Modifier */
}

const FModifierTypeInfo *get_fmodifier_typeinfo(const int type)
{
  /* initialize the type-info list? */
  if (FMI_INIT) {
    fmods_init_typeinfo();
    FMI_INIT = 0;
  }

  /* only return for valid types */
  if ((type >= FMODIFIER_TYPE_NULL) && (type < FMODIFIER_NUM_TYPES)) {
    /* there shouldn't be any segfaults here... */
    return fmodifiersTypeInfo[type];
  }

  CLOG_ERROR(&LOG, "No valid F-Curve Modifier type-info data available. Type = %i", type);

  return nullptr;
}

const FModifierTypeInfo *fmodifier_get_typeinfo(const FModifier *fcm)
{
  /* only return typeinfo for valid modifiers */
  if (fcm) {
    return get_fmodifier_typeinfo(fcm->type);
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name F-Curve Modifier Public API
 * \{ */

FModifier *add_fmodifier(ListBase *modifiers, int type, FCurve *owner_fcu)
{
  const FModifierTypeInfo *fmi = get_fmodifier_typeinfo(type);
  FModifier *fcm;

  /* sanity checks */
  if (ELEM(nullptr, modifiers, fmi)) {
    return nullptr;
  }

  /* special checks for whether modifier can be added */
  if ((modifiers->first) && (type == FMODIFIER_TYPE_CYCLES)) {
    /* cycles modifier must be first in stack, so for now, don't add if it can't be */
    /* TODO: perhaps there is some better way, but for now, */
    CLOG_STR_ERROR(&LOG,
                   "Cannot add 'Cycles' modifier to F-Curve, as 'Cycles' modifier can only be "
                   "first in stack.");
    return nullptr;
  }

  /* add modifier itself */
  fcm = static_cast<FModifier *>(MEM_callocN(sizeof(FModifier), "F-Curve Modifier"));
  fcm->type = type;
  fcm->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT; /* Expand the main panel, not the sub-panels. */
  fcm->curve = owner_fcu;
  fcm->influence = 1.0f;
  BLI_addtail(modifiers, fcm);

  /* Set modifier name and make sure it is unique. */
  BKE_fmodifier_name_set(fcm, "");

  /* tag modifier as "active" if no other modifiers exist in the stack yet */
  if (BLI_listbase_is_single(modifiers)) {
    fcm->flag |= FMODIFIER_FLAG_ACTIVE;
  }

  /* add modifier's data */
  fcm->data = MEM_callocN(fmi->size, fmi->structName);

  /* init custom settings if necessary */
  if (fmi->new_data) {
    fmi->new_data(fcm->data);
  }

  /* update the fcurve if the Cycles modifier is added */
  if ((owner_fcu) && (type == FMODIFIER_TYPE_CYCLES)) {
    BKE_fcurve_handles_recalc(owner_fcu);
  }

  /* return modifier for further editing */
  return fcm;
}

FModifier *copy_fmodifier(const FModifier *src)
{
  const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(src);
  FModifier *dst;

  /* sanity check */
  if (src == nullptr) {
    return nullptr;
  }

  /* copy the base data, clearing the links */
  dst = static_cast<FModifier *>(MEM_dupallocN(src));
  dst->next = dst->prev = nullptr;
  dst->curve = nullptr;

  /* make a new copy of the F-Modifier's data */
  dst->data = MEM_dupallocN(src->data);

  /* only do specific constraints if required */
  if (fmi && fmi->copy_data) {
    fmi->copy_data(dst, src);
  }

  /* return the new modifier */
  return dst;
}

void copy_fmodifiers(ListBase *dst, const ListBase *src)
{
  FModifier *fcm, *srcfcm;

  if (ELEM(nullptr, dst, src)) {
    return;
  }

  BLI_listbase_clear(dst);
  BLI_duplicatelist(dst, src);

  for (fcm = static_cast<FModifier *>(dst->first), srcfcm = static_cast<FModifier *>(src->first);
       fcm && srcfcm;
       srcfcm = srcfcm->next, fcm = fcm->next)
  {
    const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(fcm);

    /* make a new copy of the F-Modifier's data */
    fcm->data = MEM_dupallocN(fcm->data);
    fcm->curve = nullptr;

    /* only do specific constraints if required */
    if (fmi && fmi->copy_data) {
      fmi->copy_data(fcm, srcfcm);
    }
  }
}

bool remove_fmodifier(ListBase *modifiers, FModifier *fcm)
{
  const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(fcm);

  /* sanity check */
  if (fcm == nullptr) {
    return false;
  }

  /* removing the cycles modifier requires a handle update */
  FCurve *update_fcu = (fcm->type == FMODIFIER_TYPE_CYCLES) ? fcm->curve : nullptr;

  /* free modifier's special data (stored inside fcm->data) */
  if (fcm->data) {
    if (fmi && fmi->free_data) {
      fmi->free_data(fcm);
    }

    /* free modifier's data (fcm->data) */
    MEM_freeN(fcm->data);
  }

  /* remove modifier from stack */
  if (modifiers) {
    BLI_freelinkN(modifiers, fcm);

    /* update the fcurve if the Cycles modifier is removed */
    if (update_fcu) {
      BKE_fcurve_handles_recalc(update_fcu);
    }

    return true;
  }

  /* XXX this case can probably be removed some day, as it shouldn't happen... */
  CLOG_STR_ERROR(&LOG, "no modifier stack given");
  MEM_freeN(fcm);
  return false;
}

void free_fmodifiers(ListBase *modifiers)
{
  FModifier *fcm, *fmn;

  /* sanity check */
  if (modifiers == nullptr) {
    return;
  }

  /* free each modifier in order - modifier is unlinked from list and freed */
  for (fcm = static_cast<FModifier *>(modifiers->first); fcm; fcm = fmn) {
    fmn = fcm->next;
    remove_fmodifier(modifiers, fcm);
  }
}

FModifier *find_active_fmodifier(ListBase *modifiers)
{
  FModifier *fcm;

  /* sanity checks */
  if (ELEM(nullptr, modifiers, modifiers->first)) {
    return nullptr;
  }

  /* loop over modifiers until 'active' one is found */
  for (fcm = static_cast<FModifier *>(modifiers->first); fcm; fcm = fcm->next) {
    if (fcm->flag & FMODIFIER_FLAG_ACTIVE) {
      return fcm;
    }
  }

  /* no modifier is active */
  return nullptr;
}

void set_active_fmodifier(ListBase *modifiers, FModifier *fcm)
{
  FModifier *fm;

  /* sanity checks */
  if (ELEM(nullptr, modifiers, modifiers->first)) {
    return;
  }

  /* deactivate all, and set current one active */
  for (fm = static_cast<FModifier *>(modifiers->first); fm; fm = fm->next) {
    fm->flag &= ~FMODIFIER_FLAG_ACTIVE;
  }

  /* make given modifier active */
  if (fcm) {
    fcm->flag |= FMODIFIER_FLAG_ACTIVE;
  }
}

bool list_has_suitable_fmodifier(const ListBase *modifiers, int mtype, short acttype)
{
  FModifier *fcm;

  /* if there are no specific filtering criteria, just skip */
  if ((mtype == 0) && (acttype == 0)) {
    return (modifiers && modifiers->first);
  }

  /* sanity checks */
  if (ELEM(nullptr, modifiers, modifiers->first)) {
    return false;
  }

  /* Find the first modifier fitting these criteria. */
  for (fcm = static_cast<FModifier *>(modifiers->first); fcm; fcm = fcm->next) {
    const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(fcm);
    short mOk = 1, aOk = 1; /* by default 1, so that when only one test, won't fail */

    /* check if applicable ones are fulfilled */
    if (mtype) {
      mOk = (fcm->type == mtype);
    }
    if (acttype > -1) {
      aOk = (fmi->acttype == acttype);
    }

    /* if both are ok, we've found a hit */
    if (mOk && aOk) {
      return true;
    }
  }

  /* no matches */
  return false;
}

/* Evaluation API --------------------------- */

uint evaluate_fmodifiers_storage_size_per_modifier(ListBase *modifiers)
{
  /* Sanity checks. */
  if (ELEM(nullptr, modifiers, modifiers->first)) {
    return 0;
  }

  uint max_size = 0;

  LISTBASE_FOREACH (FModifier *, fcm, modifiers) {
    const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(fcm);

    if (fmi == nullptr) {
      continue;
    }

    max_size = MAX2(max_size, fmi->storage_size);
  }

  return max_size;
}

/**
 * Helper function - calculate influence of #FModifier.
 */
static float eval_fmodifier_influence(FModifier *fcm, float evaltime)
{
  float influence;

  /* sanity check */
  if (fcm == nullptr) {
    return 0.0f;
  }

  /* should we use influence stored in modifier or not
   * NOTE: this is really just a hack so that we don't need to version patch old files ;)
   */
  if (fcm->flag & FMODIFIER_FLAG_USEINFLUENCE) {
    influence = fcm->influence;
  }
  else {
    influence = 1.0f;
  }

  /* restricted range or full range? */
  if (fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) {
    if ((evaltime < fcm->sfra) || (evaltime > fcm->efra)) {
      /* out of range */
      return 0.0f;
    }
    if ((fcm->blendin != 0.0f) && (evaltime >= fcm->sfra) &&
        (evaltime <= fcm->sfra + fcm->blendin)) {
      /* blend in range */
      float a = fcm->sfra;
      float b = fcm->sfra + fcm->blendin;
      return influence * (evaltime - a) / (b - a);
    }
    if ((fcm->blendout != 0.0f) && (evaltime <= fcm->efra) &&
        (evaltime >= fcm->efra - fcm->blendout)) {
      /* blend out range */
      float a = fcm->efra;
      float b = fcm->efra - fcm->blendout;
      return influence * (evaltime - a) / (b - a);
    }
  }

  /* just return the influence of the modifier */
  return influence;
}

float evaluate_time_fmodifiers(FModifiersStackStorage *storage,
                               ListBase *modifiers,
                               FCurve *fcu,
                               float cvalue,
                               float evaltime)
{
  /* sanity checks */
  if (ELEM(nullptr, modifiers, modifiers->last)) {
    return evaltime;
  }

  if (fcu && fcu->flag & FCURVE_MOD_OFF) {
    return evaltime;
  }

  /* Starting from the end of the stack, calculate the time effects of various stacked modifiers
   * on the time the F-Curve should be evaluated at.
   *
   * This is done in reverse order to standard evaluation, as when this is done in standard
   * order, each modifier would cause jumps to other points in the curve, forcing all
   * previous ones to be evaluated again for them to be correct. However, if we did in the
   * reverse order as we have here, we can consider them a macro to micro type of waterfall
   * effect, which should get us the desired effects when using layered time manipulations
   * (such as multiple 'stepped' modifiers in sequence, causing different stepping rates)
   */
  uint fcm_index = storage->modifier_count - 1;
  for (FModifier *fcm = static_cast<FModifier *>(modifiers->last); fcm;
       fcm = fcm->prev, fcm_index--) {
    const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(fcm);

    if (fmi == nullptr) {
      continue;
    }

    /* If modifier cannot be applied on this frame
     * (whatever scale it is on, it won't affect the results)
     * hence we shouldn't bother seeing what it would do given the chance. */
    if ((fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) == 0 ||
        ((fcm->sfra <= evaltime) && (fcm->efra >= evaltime)))
    {
      /* only evaluate if there's a callback for this */
      if (fmi->evaluate_modifier_time) {
        if ((fcm->flag & (FMODIFIER_FLAG_DISABLED | FMODIFIER_FLAG_MUTED)) == 0) {
          void *storage_ptr = POINTER_OFFSET(storage->buffer,
                                             fcm_index * storage->size_per_modifier);

          float nval = fmi->evaluate_modifier_time(fcu, fcm, cvalue, evaltime, storage_ptr);

          float influence = eval_fmodifier_influence(fcm, evaltime);
          evaltime = interpf(nval, evaltime, influence);
        }
      }
    }
  }

  /* return the modified evaltime */
  return evaltime;
}

void evaluate_value_fmodifiers(FModifiersStackStorage *storage,
                               ListBase *modifiers,
                               FCurve *fcu,
                               float *cvalue,
                               float evaltime)
{
  FModifier *fcm;

  /* sanity checks */
  if (ELEM(nullptr, modifiers, modifiers->first)) {
    return;
  }

  if (fcu->flag & FCURVE_MOD_OFF) {
    return;
  }

  /* evaluate modifiers */
  uint fcm_index = 0;
  for (fcm = static_cast<FModifier *>(modifiers->first); fcm; fcm = fcm->next, fcm_index++) {
    const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(fcm);

    if (fmi == nullptr) {
      continue;
    }

    /* Only evaluate if there's a callback for this,
     * and if F-Modifier can be evaluated on this frame. */
    if ((fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) == 0 ||
        ((fcm->sfra <= evaltime) && (fcm->efra >= evaltime)))
    {
      if (fmi->evaluate_modifier) {
        if ((fcm->flag & (FMODIFIER_FLAG_DISABLED | FMODIFIER_FLAG_MUTED)) == 0) {
          void *storage_ptr = POINTER_OFFSET(storage->buffer,
                                             fcm_index * storage->size_per_modifier);

          float nval = *cvalue;
          fmi->evaluate_modifier(fcu, fcm, &nval, evaltime, storage_ptr);

          float influence = eval_fmodifier_influence(fcm, evaltime);
          *cvalue = interpf(nval, *cvalue, influence);
        }
      }
    }
  }
}

/* ---------- */

void fcurve_bake_modifiers(FCurve *fcu, int start, int end)
{
  ChannelDriver *driver;

  /* sanity checks */
  /* TODO: make these tests report errors using reports not CLOG's */
  if (ELEM(nullptr, fcu, fcu->modifiers.first)) {
    CLOG_ERROR(&LOG, "No F-Curve with F-Curve Modifiers to Bake");
    return;
  }

  /* temporarily, disable driver while we sample, so that they don't influence the outcome */
  driver = fcu->driver;
  fcu->driver = nullptr;

  /* bake the modifiers, by sampling the curve at each frame */
  fcurve_store_samples(fcu, nullptr, start, end, fcurve_samplingcb_evalcurve);

  /* free the modifiers now */
  free_fmodifiers(&fcu->modifiers);

  /* restore driver */
  fcu->driver = driver;
}

/** \} */
