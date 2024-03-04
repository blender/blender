/* SPDX-FileCopyrightText: 2024 Blender Developers
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "RNA_access.hh"

namespace blender::animrig::internal {

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

class AnimatedProperty {
 public:
  float value;
  PathResolvedRNA prop_rna;

  AnimatedProperty(const float value, const PathResolvedRNA &prop_rna)
      : value(value), prop_rna(prop_rna)
  {
  }
};

/* Evaluated FCurves for some animation binding.
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
 * Evaluate the animation data on the given layer, for the given binding. This
 * just returns the evaluation result, without taking any other layers,
 * blending, influence, etc. into account.
 */
EvaluationResult evaluate_layer(PointerRNA &animated_id_ptr,
                                Layer &layer,
                                binding_handle_t binding_handle,
                                const AnimationEvalContext &anim_eval_context);

}  // namespace blender::animrig::internal
