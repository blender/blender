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

#include "RNA_path.hh"

#include "ANIM_action.hh"

namespace blender::animrig {

/**
 * Identifies the property that an evaluated animation value is for.
 */
class PropIdentifier {
 public:
  ParsedRNAPathRef rna_path;
  int array_index;

  PropIdentifier() = default;

  PropIdentifier(const ParsedRNAPathRef rna_path, const int array_index)
      : rna_path(rna_path), array_index(array_index)
  {
  }

  friend bool operator==(const PropIdentifier &a, const PropIdentifier &b)
  {
    return a.array_index == b.array_index && a.rna_path == b.rna_path;
  }

  uint64_t hash() const
  {
    return get_default_hash(this->array_index, this->rna_path);
  }
};

/**
 * The evaluated value for an animated property, along with its RNA pointer.
 */
class AnimatedProperty {
 public:
  float value;
  PathResolvedRNA prop_rna;

  AnimatedProperty(const float value, PathResolvedRNA &&prop_rna)
      : value(value), prop_rna(std::move(prop_rna))
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

  operator bool() const
  {
    return !this->is_empty();
  }
  bool is_empty() const
  {
    return result_.is_empty();
  }

  /**
   * When the expected count of entries is known, reserving with size instead of growing on demand
   * is more performant.
   */
  void reserve(const int64_t size)
  {
    result_.reserve(size);
  };

  void store(const ParsedRNAPathRef rna_path,
             const int array_index,
             const float value,
             PathResolvedRNA prop_rna)
  {
    result_.add_overwrite(PropIdentifier(rna_path, array_index),
                          AnimatedProperty(value, std::move(prop_rna)));
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
