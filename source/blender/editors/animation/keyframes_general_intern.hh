/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#pragma once

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

#include "ANIM_action.hh"

#include <limits>
#include <optional>
#include <string>

struct FCurve;
struct ID;
struct Main;

namespace blender::ed::animation {

/**
 * Global copy/paste buffer for multi-slotted keyframe data.
 *
 * All the animation data managed by this struct is copied into it, and thus
 * owned by this struct.
 */
struct KeyframeCopyBuffer {
  /**
   * The copied keyframes, in a ChannelBag per slot.
   *
   * Note that the slot handles are arbitrary, and are likely different from the
   * handles of the original slots (i.e. the ones that the copied F-Curves were
   * for). This is to make it possible to copy from different Actions (like is
   * possible on the dope sheet) and still distinguish between their slots.
   */
  animrig::StripKeyframeData keyframe_data;

  /**
   * Slot identifier used for slotless keyframes.
   *
   * These are keyframes copied from F-Curves not owned by an Action, such as drivers and NLA
   * control curves.
   */
  static constexpr const char *SLOTLESS_SLOT_IDENTIFIER = "";

  /**
   * Just a more-or-less randomly chosen number to start at.
   *
   * Having this distinctly different from DNA_DEFAULT_ACTION_LAST_SLOT_HANDLE
   * makes it easier to spot bugs.
   */
  static constexpr animrig::slot_handle_t DEFAULT_LAST_USED_SLOT_HANDLE = 0x1acca;
  animrig::slot_handle_t last_used_slot_handle = DEFAULT_LAST_USED_SLOT_HANDLE;

  /**
   * Mapping from slot handles to their identifiers.
   *
   * Since the StripKeyframeData only stores slot handles, and not their
   * identifiers, this has to be stored here. An alternative would be to store
   * the copied data into an Action, but that would allow for multi-layer,
   * multi-strip data which is overkill for the functionality needed here.
   */
  Map<animrig::slot_handle_t, std::string> slot_identifiers;

  /**
   * Mapping from slot handles to the ID that they were copied from.
   *
   * Multiple IDs can be animated by a single slot, in which case an arbitrary
   * one is stored here. This pointer is only used to resolve RNA paths to find the
   * property name, and thus the exact ID doesn't matter much.
   *
   * TODO(@sybren): it would be better to track the ID name here, instead of the pointer.
   * That'll make it safer to work with when pasting into another file, or after
   * the copied-from ID has been deleted. For now I am trying to keep
   * things feature-par with the original code this is replacing.
   */
  Map<animrig::slot_handle_t, ID *> slot_animated_ids;

  /**
   * Pointers to F-Curves in this->keyframe_data that animate bones.
   *
   * This is mostly to indicate which F-Curves are flipped when pasting flipped.
   */
  Set<const FCurve *> bone_fcurves;

  /* The first and last frames that got copied. */
  float first_frame = std::numeric_limits<float>::infinity();
  float last_frame = -std::numeric_limits<float>::infinity();

  /** The current scene frame when copying. Used for the 'relative' paste method. */
  float current_frame = 0.0f;

  KeyframeCopyBuffer() = default;
  KeyframeCopyBuffer(const KeyframeCopyBuffer &other) = delete;
  ~KeyframeCopyBuffer() = default;

  bool is_empty() const;
  bool is_single_fcurve() const;
  bool is_bone(const FCurve &fcurve) const;
  int num_slots() const;

  animrig::Channelbag *channelbag_for_slot(StringRef slot_identifier);

  /**
   * Print the contents of the copy buffer to stdout.
   */
  void debug_print() const;
};

extern KeyframeCopyBuffer *keyframe_copy_buffer;

/**
 * Flip bone names in the RNA path, returning the flipped path.
 *
 * Returns empty optional if the `rna_path` is not animating a bone,
 * i.e. doesn't have the `pose.bones["` prefix.
 */
std::optional<std::string> flip_names(StringRefNull rna_path);

/**
 * Most strict paste buffer matching method: exact matches on RNA path and array index only.
 */
bool pastebuf_match_path_full(Main *bmain,
                              const FCurve &fcurve_to_match,
                              const FCurve &fcurve_in_copy_buffer,
                              animrig::slot_handle_t slot_handle_in_copy_buffer,
                              bool from_single,
                              bool to_single,
                              bool flip);

/**
 * Medium strict paste buffer matching method: match the property name (so not the entire RNA path)
 * and the array index.
 */
bool pastebuf_match_path_property(Main *bmain,
                                  const FCurve &fcurve_to_match,
                                  const FCurve &fcurve_in_copy_buffer,
                                  animrig::slot_handle_t slot_handle_in_copy_buffer,
                                  bool from_single,
                                  bool to_single,
                                  bool flip);

/**
 * Least strict paste buffer matching method: array indices only.
 */
bool pastebuf_match_index_only(Main *bmain,
                               const FCurve &fcurve_to_match,
                               const FCurve &fcurve_in_copy_buffer,
                               animrig::slot_handle_t slot_handle_in_copy_buffer,
                               bool from_single,
                               bool to_single,
                               bool flip);

}  // namespace blender::ed::animation
