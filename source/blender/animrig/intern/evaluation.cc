/* SPDX-FileCopyrightText: 2023 Blender Developers
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_evaluation.hh"

#include "RNA_access.hh"

#include "BKE_animsys.h"
#include "BKE_fcurve.hh"

#include "BLI_map.hh"

#include "evaluation_internal.hh"

namespace blender::animrig {

using namespace internal;

/**
 * Blend the 'current layer' with the 'last evaluation result', returning the
 * blended result.
 */
EvaluationResult blend_layer_results(const EvaluationResult &last_result,
                                     const EvaluationResult &current_result,
                                     const Layer &current_layer);

/**
 * Apply the result of the animation evaluation to the given data-block.
 *
 * \param flush_to_original: when true, look up the original data-block (assuming the given one is
 * an evaluated copy) and update that too.
 */
void apply_evaluation_result(const EvaluationResult &evaluation_result,
                             PointerRNA &animated_id_ptr,
                             bool flush_to_original);

static EvaluationResult evaluate_animation(PointerRNA &animated_id_ptr,
                                           Action &animation,
                                           const binding_handle_t binding_handle,
                                           const AnimationEvalContext &anim_eval_context)
{
  EvaluationResult last_result;

  /* Evaluate each layer in order. */
  for (Layer *layer : animation.layers()) {
    if (layer->influence <= 0.0f) {
      /* Don't bother evaluating layers without influence. */
      continue;
    }

    auto layer_result = evaluate_layer(animated_id_ptr, *layer, binding_handle, anim_eval_context);
    if (!layer_result) {
      continue;
    }

    if (!last_result) {
      /* Simple case: no results so far, so just use this layer as-is. There is
       * nothing to blend/combine with, so ignore the influence and combination
       * options. */
      last_result = layer_result;
      continue;
    }

    /* Complex case: blend this layer's result into the previous layer's result. */
    last_result = blend_layer_results(last_result, layer_result, *layer);
  }

  return last_result;
}

void evaluate_and_apply_animation(PointerRNA &animated_id_ptr,
                                  Action &animation,
                                  const binding_handle_t binding_handle,
                                  const AnimationEvalContext &anim_eval_context,
                                  const bool flush_to_original)
{
  EvaluationResult evaluation_result = evaluate_animation(
      animated_id_ptr, animation, binding_handle, anim_eval_context);
  if (!evaluation_result) {
    return;
  }

  apply_evaluation_result(evaluation_result, animated_id_ptr, flush_to_original);
}

/* Copy of the same-named function in anim_sys.cc, with the check on action groups removed. */
static bool is_fcurve_evaluatable(const FCurve *fcu)
{
  if (fcu->flag & (FCURVE_MUTED | FCURVE_DISABLED)) {
    return false;
  }
  if (BKE_fcurve_is_empty(fcu)) {
    return false;
  }
  return true;
}

/* Copy of the same-named function in anim_sys.cc, but with the special handling for NLA strips
 * removed. */
static void animsys_construct_orig_pointer_rna(const PointerRNA *ptr, PointerRNA *ptr_orig)
{
  *ptr_orig = *ptr;
  /* Original note from anim_sys.cc:
   * -----------
   * NOTE: nlastrip_evaluate_controls() creates PointerRNA with ID of nullptr. Technically, this is
   * not a valid pointer, but there are exceptions in various places of this file which handles
   * such pointers.
   * We do special trickery here as well, to quickly go from evaluated to original NlaStrip.
   * -----------
   * And this is all not ported to the new layered animation system. */
  BLI_assert_msg(ptr->owner_id, "NLA support was not ported to the layered animation system");
  ptr_orig->owner_id = ptr_orig->owner_id->orig_id;
  ptr_orig->data = ptr_orig->owner_id;
}

/* Copy of the same-named function in anim_sys.cc. */
static void animsys_write_orig_anim_rna(PointerRNA *ptr,
                                        const char *rna_path,
                                        const int array_index,
                                        const float value)
{
  PointerRNA ptr_orig;
  animsys_construct_orig_pointer_rna(ptr, &ptr_orig);

  PathResolvedRNA orig_anim_rna;
  /* TODO(sergey): Should be possible to cache resolved path in dependency graph somehow. */
  if (BKE_animsys_rna_path_resolve(&ptr_orig, rna_path, array_index, &orig_anim_rna)) {
    BKE_animsys_write_to_rna_path(&orig_anim_rna, value);
  }
}

static EvaluationResult evaluate_keyframe_strip(PointerRNA &animated_id_ptr,
                                                KeyframeStrip &key_strip,
                                                const binding_handle_t binding_handle,
                                                const AnimationEvalContext &offset_eval_context)
{
  ChannelBag *channelbag_for_binding = key_strip.channelbag_for_binding(binding_handle);
  if (!channelbag_for_binding) {
    return {};
  }

  EvaluationResult evaluation_result;
  for (FCurve *fcu : channelbag_for_binding->fcurves()) {
    /* Blatant copy of animsys_evaluate_fcurves(). */

    if (!is_fcurve_evaluatable(fcu)) {
      continue;
    }

    PathResolvedRNA anim_rna;
    if (!BKE_animsys_rna_path_resolve(
            &animated_id_ptr, fcu->rna_path, fcu->array_index, &anim_rna))
    {
      printf("Cannot resolve RNA path %s[%d] on ID %s\n",
             fcu->rna_path,
             fcu->array_index,
             animated_id_ptr.owner_id->name);
      continue;
    }

    const float curval = calculate_fcurve(&anim_rna, fcu, &offset_eval_context);
    evaluation_result.store(fcu->rna_path, fcu->array_index, curval, anim_rna);
  }

  return evaluation_result;
}

void apply_evaluation_result(const EvaluationResult &evaluation_result,
                             PointerRNA &animated_id_ptr,
                             const bool flush_to_original)
{
  for (auto channel_result : evaluation_result.items()) {
    const PropIdentifier &prop_ident = channel_result.key;
    const AnimatedProperty &anim_prop = channel_result.value;
    const float animated_value = anim_prop.value;
    PathResolvedRNA anim_rna = anim_prop.prop_rna;

    BKE_animsys_write_to_rna_path(&anim_rna, animated_value);

    if (flush_to_original) {
      /* Convert the StringRef to a `const char *`, as the rest of the RNA path handling code in
       * BKE still uses `char *` instead of `StringRef`. */
      animsys_write_orig_anim_rna(&animated_id_ptr,
                                  StringRefNull(prop_ident.rna_path).c_str(),
                                  prop_ident.array_index,
                                  animated_value);
    }
  }
}

static EvaluationResult evaluate_strip(PointerRNA &animated_id_ptr,
                                       Strip &strip,
                                       const binding_handle_t binding_handle,
                                       const AnimationEvalContext &anim_eval_context)
{
  AnimationEvalContext offset_eval_context = anim_eval_context;
  /* Positive offset means the entire strip is pushed "to the right", so
   * evaluation needs to happen further "to the left". */
  offset_eval_context.eval_time -= strip.frame_offset;

  switch (strip.type()) {
    case Strip::Type::Keyframe: {
      KeyframeStrip &key_strip = strip.as<KeyframeStrip>();
      return evaluate_keyframe_strip(
          animated_id_ptr, key_strip, binding_handle, offset_eval_context);
    }
  }

  return {};
}

EvaluationResult blend_layer_results(const EvaluationResult &last_result,
                                     const EvaluationResult &current_result,
                                     const Layer &current_layer)
{
  /* TODO?: store the layer results sequentially, so that we can step through
   * them in parallel, instead of iterating over one and doing map lookups on
   * the other. */

  EvaluationResult blend = last_result;

  for (auto channel_result : current_result.items()) {
    const PropIdentifier &prop_ident = channel_result.key;
    AnimatedProperty *last_prop = blend.lookup_ptr(prop_ident);
    const AnimatedProperty &anim_prop = channel_result.value;

    if (!last_prop) {
      /* Nothing to blend with, so just take (influence * value). */
      blend.store(prop_ident.rna_path,
                  prop_ident.array_index,
                  anim_prop.value * current_layer.influence,
                  anim_prop.prop_rna);
      continue;
    }

    /* TODO: move this to a separate function. And write more smartness for rotations. */
    switch (current_layer.mix_mode()) {
      case Layer::MixMode::Replace:
        last_prop->value = anim_prop.value * current_layer.influence;
        break;
      case Layer::MixMode::Offset:
        last_prop->value = math::interpolate(
            current_layer.influence, last_prop->value, anim_prop.value);
        break;
      case Layer::MixMode::Add:
        last_prop->value += anim_prop.value * current_layer.influence;
        break;
      case Layer::MixMode::Subtract:
        last_prop->value -= anim_prop.value * current_layer.influence;
        break;
      case Layer::MixMode::Multiply:
        last_prop->value *= anim_prop.value * current_layer.influence;
        break;
    };
  }

  return blend;
}

namespace internal {

EvaluationResult evaluate_layer(PointerRNA &animated_id_ptr,
                                Layer &layer,
                                const binding_handle_t binding_handle,
                                const AnimationEvalContext &anim_eval_context)
{
  /* TODO: implement cross-blending between overlapping strips. For now, this is not supported.
   * Instead, the first strong result is taken (see below), and if that is not available, the last
   * weak result will be used.
   *
   * Weak result: obtained from evaluating the final frame of the strip.
   * Strong result: any result that is not a weak result. */
  EvaluationResult last_weak_result;

  for (Strip *strip : layer.strips()) {
    if (!strip->contains_frame(anim_eval_context.eval_time)) {
      continue;
    }

    const EvaluationResult strip_result = evaluate_strip(
        animated_id_ptr, *strip, binding_handle, anim_eval_context);
    if (!strip_result) {
      continue;
    }

    const bool is_weak_result = strip->is_last_frame(anim_eval_context.eval_time);
    if (is_weak_result) {
      /* Keep going until a strong result is found. */
      last_weak_result = strip_result;
      continue;
    }

    /* Found a strong result, just return it. */
    return strip_result;
  }

  return last_weak_result;
}

}  // namespace internal

}  // namespace blender::animrig
