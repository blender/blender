/* SPDX-FileCopyrightText: 2023 Blender Developers
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Layered Action evaluation.
 */
#pragma once

#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "DNA_anim_types.h"

#include "RNA_access.hh"

#include "ANIM_action.hh"

namespace blender::animrig {

/* Identifies the property that an evaluated animation value is for.
 *
 * This could be replaced with either `FCurveIdentifier` or `RNAPath`.  However,
 * `FCurveIdentifier` is semantically meant to represent an fcurve itself rather
 * than the property an fcurve might be for, and moreover not all animation will
 * necessarily come from fcurves in the future anyway.  `RNAPath` would be more
 * semantically appropriate, but it stores a full copy of the string component
 * of the path, and here we want to be lighter than that and use a string
 * reference.
 */
class PropIdentifier {
 public:
  /**
   * Reference to the RNA path of the property.
   *
   * This string is typically owned by the FCurve that animates the property.
   */
  StringRefNull rna_path;
  int array_index;

  PropIdentifier() = default;

  PropIdentifier(const StringRefNull rna_path, const int array_index)
      : rna_path(rna_path), array_index(array_index)
  {
  }

  bool operator==(const PropIdentifier &other) const
  {
    return rna_path == other.rna_path && array_index == other.array_index;
  }
  bool operator!=(const PropIdentifier &other) const
  {
    return !(*this == other);
  }

  uint64_t hash() const
  {
    return get_default_hash(rna_path, array_index);
  }
};

/**
 * The evaluated value for an animated property, along with its RNA pointer.
 */
class AnimatedProperty {
 public:
  float value;
  PathResolvedRNA prop_rna;

  AnimatedProperty(const float value, const PathResolvedRNA &prop_rna)
      : value(value), prop_rna(prop_rna)
  {
  }
};

/* Result of FCurve evaluation for an action slot.
 * Mapping from property identifier to its float value.
 *
 * Can be fed to the evaluation of the next layer, mixed with another strip, or
 * used to modify actual RNA properties.
 *
 * TODO: see if this is efficient, and contains enough info, for mixing. For now
 * this just captures the FCurve evaluation result, but doesn't have any info
 * about how to do the mixing (LERP, quaternion SLERP, etc.).
 */
class EvaluationResult {
 protected:
  using EvaluationMap = Map<PropIdentifier, AnimatedProperty>;
  EvaluationMap result_;

 public:
  EvaluationResult() = default;
  EvaluationResult(const EvaluationResult &other) = default;
  ~EvaluationResult() = default;

 public:
  operator bool() const
  {
    return !this->is_empty();
  }
  bool is_empty() const
  {
    return result_.is_empty();
  }

  void store(const StringRefNull rna_path,
             const int array_index,
             const float value,
             const PathResolvedRNA &prop_rna)
  {
    PropIdentifier key(rna_path, array_index);
    AnimatedProperty anim_prop(value, prop_rna);
    result_.add_overwrite(key, anim_prop);
  }

  AnimatedProperty value(const StringRefNull rna_path, const int array_index) const
  {
    PropIdentifier key(rna_path, array_index);
    return result_.lookup(key);
  }

  const AnimatedProperty *lookup_ptr(const PropIdentifier &key) const
  {
    return result_.lookup_ptr(key);
  }
  AnimatedProperty *lookup_ptr(const PropIdentifier &key)
  {
    return result_.lookup_ptr(key);
  }

  EvaluationMap::ItemIterator items() const
  {
    return result_.items();
  }
};

/**
 * Evaluate the given action for the given slot and animated ID.
 *
 * This does *not* apply the resulting values to the ID.  Instead, it returns
 * the resulting values in an `EvaluationResult`.
 */
EvaluationResult evaluate_action(PointerRNA &animated_id_ptr,
                                 Action &action,
                                 slot_handle_t slot_handle,
                                 const AnimationEvalContext &anim_eval_context);

/**
 * Top level animation evaluation function.
 *
 * Animate the given ID, using the layered Action and the given slot.
 *
 * \param flush_to_original: when true, look up the original data-block (assuming
 * the given one is an evaluated copy) and update that too.
 */
void evaluate_and_apply_action(PointerRNA &animated_id_ptr,
                               Action &action,
                               slot_handle_t slot_handle,
                               const AnimationEvalContext &anim_eval_context,
                               bool flush_to_original);

}  // namespace blender::animrig
