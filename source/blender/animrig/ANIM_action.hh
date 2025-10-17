/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions and classes to work with Actions.
 */
#pragma once

#include "ANIM_fcurve.hh"
#include "ANIM_keyframing.hh"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"

#include "BLI_enum_flags.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "RNA_types.hh"

#include <utility>

struct FCurve;
struct FCurve;
struct ID;
struct Main;
struct PointerRNA;
struct Main;

namespace blender::animrig {

/* Forward declarations for the types defined later in this file. */
class Layer;
class Strip;
class Slot;

/**
 * Container of animation data for one or more animated IDs.
 *
 * An Action broadly consists of four things:
 *
 * 1. Layers, which contain Strips.
 * 2. Strips, which reference StripData.
 * 3. StripData: Strip{TYPE}Data contains animation data of the given type. For
 *    example, StripKeyframeData (currently the only StripData type) contains
 *    keyframes.
 * 4. Slots, which are used as identifiers for subsets of animation data within
 *    StripData items.
 *
 * StripData is not stored in the Strips themselves, but rather is stored
 * separately at the top level of the Action, and each Strip *references* a
 * StripData item. This allows Strip instancing by having more than one Strip
 * reference the same StripData item.
 *
 * Each Action has a set of Slots defined at its top level. The animation data
 * within a StripData item is organized into one or more subsets, each of which
 * is marked as being for a different Slot.
 *
 * For an ID to be animated by an Action, the ID must specify both an Action and
 * a Slot within that Action. The Slot that the ID uses determines which subset
 * of the animation data throughout the Action it is animated by. If an Action
 * but no Slot is specified, the ID is simply not animated.
 *
 * \note Temporary limitations: each Action can only contain one Layer, and each
 * Layer can only contain one infinite Strip with no time offset. These
 * limitations will be progressively lifted as we implement layered animation
 * and non-linear animation functionality for Actions in the future. (See:
 * `assert_baklava_phase_1_invariants()`.)
 *
 * \note This wrapper class for the `bAction` DNA struct only has functionality
 * for the layered animation data. The legacy F-Curves (in `bAction::curves`)
 * and their groups (in `bAction::groups`) are not managed here.
 *
 * To continue supporting legacy actions at runtime, there are
 * `Action::is_action_legacy()` and `Action::is_action_layered()` that report
 * whether an Action uses that legacy F-Curve data or is instead a layered
 * Action. These methods will eventually be removed when runtime support for
 * legacy actions is fully removed. For code in blend file loading and
 * versioning, which will stick around for the long-term, use
 * `animrig::versioning::action_is_layered()` instead. (Note that an empty
 * Action is considered both a valid legacy *and* layered action.)
 *
 * \see #AnimData::action
 * \see #AnimData::slot_handle
 * \see assert_baklava_phase_1_invariants()
 * \see #animrig::versioning::action_is_layered()
 */
class Action : public ::bAction {
 public:
  Action() = default;
  /**
   * Copy constructor is deleted, as code should use regular ID library
   * management functions to duplicate this data-block.
   */
  Action(const Action &other) = delete;

  /* Discriminators for 'legacy' and 'layered' Actions.
   *
   * Note: `is_action_legacy()` and `is_action_layered()` are transitional APIs,
   * and should eventually be removed. See their documentation below for
   * details.
   */
  /**
   * Return whether this Action has any data at all.
   *
   * \return true when `bAction::layer_array` and `bAction::slot_array`, as well as
   * the legacy `curves` list, are empty.
   */
  bool is_empty() const;
  /**
   * Return whether this is a legacy Action.
   *
   * - Animation data is stored in `bAction::curves`.
   * - Evaluated equally for all data-blocks that reference this Action.
   * - Slot handle is ignored.
   *
   * \note An empty Action is valid as both a legacy and layered Action. Code that only supports
   * layered Actions should assert on `is_action_layered()`.
   *
   * \note This method will be removed when runtime support for legacy Actions
   * is removed, so only use it in such runtime code. See
   * `animrig::versioning::action_is_layered()` for uses that should stick
   * around for the long term, such as blend file loading and versioning.
   *
   * \see #animrig::versioning::action_is_layered()
   */
  bool is_action_legacy() const;
  /**
   * Return whether this is a layered Action.
   *
   * - Animation data is stored in `bAction::layer_array`.
   * - Evaluated for data-blocks based on their slot handle.
   *
   * \note An empty Action is valid as both a legacy and layered Action.
   *
   * \note This method will be removed when runtime support for legacy Actions
   * is removed, so only use it in such runtime code. See
   * `animrig::versioning::action_is_layered()` for uses that should stick
   * around for the long term, such as blend file loading and versioning.
   *
   * \see #animrig::versioning::action_is_layered()
   */
  bool is_action_layered() const;

  /* Action Layers access. */
  blender::Span<const Layer *> layers() const;
  blender::Span<Layer *> layers();
  const Layer *layer(int64_t index) const;
  Layer *layer(int64_t index);

  /**
   * Create a new layer in this Action.
   *
   * The new layer is added to the end of the layer array, and will be empty (no
   * strips).
   *
   * \note At the time of writing this comment only a single layer per Action is
   * supported in Blender, but this function does NOT enforce that. Be careful!
   *
   * \param name: The name to give the new layer. If no name is given, a default
   * name is used. The name may be altered (e.g. appending ".001") to enforce
   * uniqueness within the Action.
   *
   * \return A reference to the newly created layer.
   *
   * \see assert_baklava_phase_1_invariants()
   */
  Layer &layer_add(std::optional<StringRefNull> name);

  /**
   * Remove the layer from this Action.
   *
   * After this call, the passed reference is no longer valid, as the memory
   * will have been freed. Any strips on the layer will be freed too.
   *
   * \return true when the layer was found & removed, false if it wasn't found.
   */
  bool layer_remove(Layer &layer_to_remove);

  /**
   * Ensure that there is at least one layer with the infinite keyframe strip.
   *
   * \note Within the limits of Project Baklava Phase 1, this means that there
   * will be exactly one layer with one keyframe strip on it.
   */
  void layer_keystrip_ensure();

  /* Action Slot access. */
  blender::Span<const Slot *> slots() const;
  blender::Span<Slot *> slots();
  const Slot *slot(int64_t index) const;
  Slot *slot(int64_t index);

  /**
   * Return the Slot with the given handle.
   *
   * \param handle: can be `Slot::unassigned`, in which case `nullptr` is returned.
   *
   * \return `nullptr` when the slot cannot be found, so either the handle was
   * `Slot::unassigned` or some value that does not match any Slot in this Action.
   */
  Slot *slot_for_handle(slot_handle_t handle);
  const Slot *slot_for_handle(slot_handle_t handle) const;

  /**
   * Set the slot display name (the part of the identifier after the two-letter
   * ID prefix), ensure the resulting identifier is unique, and propagate the
   * new identifier to all data-blocks that use it
   *
   * This has to be done on the Action level to ensure each slot has a unique
   * identifier within the Action.
   *
   * \see #Action::slot_identifier_set
   */
  void slot_display_name_set(Main &bmain, Slot &slot, StringRefNull new_display_name);

  /**
   * Set the slot display name (the part of the identifier after the two-letter
   * ID prefix), and ensure the resulting identifier is unique.
   *
   * This has to be done on the Action level to ensure each slot has a unique
   * identifier within the Action.
   *
   * \note This does NOT propagate the resulting slot identifier to the slot's
   * users.
   *
   * \see #Action::slot_display_name_set
   * \see #Action::slot_identifier_propagate
   */
  void slot_display_name_define(Slot &slot, StringRefNull new_display_name);

  /**
   * Set the slot's target ID type, updating the identifier prefix to match and
   * ensuring that the resulting identifier is unique.
   *
   * This has to be done on the Action level to ensure each slot has a unique
   * identifier within the Action.
   *
   * \note This does NOT propagate the identifier to the slot's users. That is
   * the caller's responsibility.
   *
   * \see #Action::slot_identifier_propagate
   */
  void slot_idtype_define(Slot &slot, ID_Type idtype);

  /**
   * Set the slot identifier, ensure it is unique, and propagate the new identifier to
   * all data-blocks that use it.
   *
   * This has to be done on the Action level to ensure each slot has a
   * unique identifier within the Action.
   *
   * \note This does NOT ensure the first two characters match the ID type of
   * this slot. This is the caller's responsibility.
   *
   * \see #Action::slot_identifier_define
   * \see #Action::slot_identifier_propagate
   */
  void slot_identifier_set(Main &bmain, Slot &slot, StringRefNull new_identifier);

  /**
   * Set the slot identifier, and ensure it is unique.
   *
   * \note This does NOT ensure the first two characters match the ID type of
   * this slot. This is the caller's responsibility.
   *
   * \see #Action::slot_identifier_set
   * \see #Action::slot_identifier_propagate
   */
  void slot_identifier_define(Slot &slot, StringRefNull new_identifier);

  /**
   * Update the `AnimData::last_slot_identifier` field of any ID that is animated by
   * this Slot.
   *
   * Should be called after `slot_identifier_define(slot)`. This is implemented as a separate
   * function due to the need to access `bmain`, which is available in the RNA on-property-update
   * handler, but not in the RNA property setter.
   */
  void slot_identifier_propagate(Main &bmain, const Slot &slot);

  /**
   * Return the slot in this action with the given identifier, if any.
   *
   * \return A pointer to the matching slot, or nullptr if no matching slot is
   * found.
   */
  Slot *slot_find_by_identifier(StringRefNull slot_identifier);

  /**
   * Create a new Slot.
   *
   * This method should generally not be used outside of low-level code and
   * legacy action versioning code, because it creates a Slot with an
   * unspecified intended ID type, which should be avoided. Prefer
   * `slot_add_for_id_type()` and `slot_add_for_id()` for adding new slots.
   *
   * TODO: we should probably rename this method to make it clear that it
   * shouldn't be used as the standard way to add a slot.
   *
   * The slot is given a default name and will be suitable for any ID type.
   * After assigning the slot to an ID, it will be changed to only be suitable
   * for that ID's type.
   *
   * \see slot_add_for_id_type()
   * \see slot_add_for_id()
   */
  Slot &slot_add();

  /**
   * Create a new, unused Slot for the given ID type.
   *
   * The returned slot will only be suitable for the specified ID type.
   */
  Slot &slot_add_for_id_type(ID_Type idtype);

  /**
   * Create a new, unused Slot suitable for the given ID.
   *
   * The slot will be named after `animated_id.adt.last_slot_identifier`, defaulting to the ID's
   * name when that is not set. This is done so that toggling Actions works transparently, when
   * toggling between `this` and the Action last assigned to the ID.
   *
   * The slot will only be suitable for the ID's type.
   *
   * Note that this assigns neither this Action nor the new Slot to the ID. This function
   * merely initializes the Slot itself to suitable values to start animating this ID.
   */
  Slot &slot_add_for_id(const ID &animated_id);

  /**
   * Remove a slot, and ALL animation data that belongs to it.
   *
   * After this call, the reference is no longer valid as the slot will have been freed.
   *
   * Note that this does NOT unassign this slot from all its users. When the Action is linked into
   * another file, that other file cannot be updated, and so missing slots are something that has
   * to be handled anyway. Also any new slot on this Action will NOT reuse this slot's handle.
   *
   * \return true when the layer was found & removed, false if it wasn't found.
   */
  bool slot_remove(Slot &slot_to_remove);

  /**
   * Move the given slot to position `to_slot_index` among the slots of the
   * action.
   *
   * `slot` must belong to this action, and `to_slot_index` must be a
   * valid index in the slot array.
   */
  void slot_move_to_index(Slot &slot, int to_slot_index);

  /**
   * Set the active Slot, ensuring only one Slot is flagged as the Active one.
   *
   * \param slot_handle: if #Slot::unassigned, there will not be any active slot.
   * Passing an unknown/invalid slot handle will result in no slot being active.
   */
  void slot_active_set(slot_handle_t slot_handle);

  /**
   * Get the active Slot.
   *
   * This requires a linear scan of the slots, to find the one with the 'Active' flag set. Storing
   * this on the Slot itself has the advantage that the 'active' status of a Slot can be determined
   * without requiring access to the owning Action.
   *
   * As this already does a linear scan for the active slot, the slot is returned as a pointer;
   * obtaining the pointer from a handle would require another linear scan to get the pointer,
   * whereas obtaining the handle from the pointer is a constant operation.
   */
  Slot *slot_active_get();

  /**
   * Strip data array access.
   */
  Span<const StripKeyframeData *> strip_keyframe_data() const;
  Span<StripKeyframeData *> strip_keyframe_data();

  /**
   * Return whether this Action actually has any animation data for the given slot.
   *
   * \see has_keyframes()
   */
  bool is_slot_animated(slot_handle_t slot_handle) const;

  /**
   * Check if the slot with this handle has any keyframes.
   *
   * If called on a legacy action, `action_slot_handle` is ignored and the
   * fcurves of the legacy action are checked for keyframes.
   *
   * \see is_slot_animated()
   */
  bool has_keyframes(slot_handle_t action_slot_handle) const ATTR_WARN_UNUSED_RESULT;

  /**
   * Return whether the action has one unique point in time keyed.
   *
   * This is mostly for the pose library, which will have different behavior depending on whether
   * an Action corresponds to a "pose" (one keyframe) or "animation snippet" (multiple keyframes).
   *
   * \return `false` when there is no keyframe at all or keys on different points in time, `true`
   * when exactly one point in time is keyed.
   */
  bool has_single_frame() const ATTR_WARN_UNUSED_RESULT;

  /**
   * Returns whether this Action is configured as cyclic.
   */
  bool is_cyclic() const ATTR_WARN_UNUSED_RESULT;

  /**
   * Get the layer that should be used for user-level keyframe insertion.
   *
   * \return The layer, or nullptr if no layer exists that can currently be used
   * for keyframing (e.g. all layers are locked, once we've implemented
   * locking).
   */
  Layer *get_layer_for_keyframing();

  /**
   * Retrieve the intended playback frame range of the entire Action.
   *
   * \return a tuple (start frame, end frame). This is either the manually set range (if enabled),
   * or the result of a scan of all F-Curves for their first & last frames.
   *
   * \see get_frame_range_of_keys()
   * \see get_frame_range_of_slot()
   */
  float2 get_frame_range() const ATTR_WARN_UNUSED_RESULT;

  /**
   * Retrieve the intended playback frame range of a slot.
   *
   * \return a tuple (start frame, end frame). This is either the manually set range (if enabled)
   * of the Action, or the result of a scan of all F-Curves of the slot for their first & last
   * frames.
   *
   * \see get_frame_range()
   */
  float2 get_frame_range_of_slot(slot_handle_t slot_handle) const ATTR_WARN_UNUSED_RESULT;

  /**
   * Calculate the extents of this Action.
   *
   * Performs a scan of all F-Curves for their first & last key frames.
   *
   * \return tuple (first key frame, last key frame).
   */
  float2 get_frame_range_of_keys(bool include_modifiers) const ATTR_WARN_UNUSED_RESULT;

  /**
   * Set the slot's ID type to that of the animated ID, ensure the identifier
   * prefix is set accordingly, and that the identifier is unique within the
   * Action.
   *
   * This is a low-level function, and shouldn't be called directly outside of
   * the generic slot-assignment functions.
   *
   * \note This assumes that the slot has no ID type set yet. If it does, it
   * is considered a bug to call this function.
   */
  void slot_setup_for_id(Slot &slot, const ID &animated_id);

 protected:
  /* Friends for the purpose of adding/removing strip data on the action's strip
   * data arrays. This is needed for the strip creation and removal code in
   * `Strip` and `Layer`'s methods. */
  friend Strip;
  friend Layer;

  /** Return the layer's index, or -1 if not found in this Action. */
  int64_t find_layer_index(const Layer &layer) const;

  /** Return the slot's index, or -1 if not found in this Action. */
  int64_t find_slot_index(const Slot &slot) const;

  /**
   * Append the given `StripKeyframeData` item to the action's keyframe data
   * array.
   *
   * Note: this takes ownership of `strip_data`.
   *
   * \return The index of the appended item in the array.
   */
  int strip_keyframe_data_append(StripKeyframeData *strip_data);

  /**
   * Remove the keyframe strip data at `index` if it is no longer used anywhere
   * in the action.
   *
   * If the strip data is unused, it is both removed from the array *and* freed.
   * Otherwise no changes are made and the action remains as-is.
   *
   * Note: this may alter the indices of some strip data items, due to items
   * shifting around to fill the gap left by the removed item. This method
   * ensures that all indices stored within the action (e.g. in the strips
   * themselves) are properly updated to the new values so that everything is
   * still referencing the same data. However, if any indices are stored
   * *outside* the action, they will no longer be valid.
   */
  void strip_keyframe_data_remove_if_unused(int index);

 private:
  /**
   * Create a new slot for this Action, but *don't* add it to the Action's list
   * of slots.
   *
   * This *does* give the slot a slot handle, and also correspondingly updates
   * the Action's `last_slot_handle` field, hence why this is a method on
   * Action.
   *
   * This is a low-level function. Prefer `slot_add()` and friends in most
   * cases.
   *
   * \see slot_add()
   * \see slot_add_for_id()
   */
  Slot &slot_allocate();

  /**
   * Ensure the slot identifier prefix matches its ID type.
   *
   * This ensures that the first two characters match the ID type of
   * this slot.
   *
   * \see #Action::slot_identifier_propagate
   */
  void slot_identifier_ensure_prefix(Slot &slot);
};
static_assert(sizeof(Action) == sizeof(::bAction),
              "DNA struct and its C++ wrapper must have the same size");

/**
 * Strips define how the actual animation data is mapped onto the layers.
 *
 * Strips do not technically own their own data, but instead refer to data
 * that's stored in arrays directly on the action itself, and specify how that
 * data is mapped onto a layer.
 *
 * Different strips can refer to different types of data, although at the moment
 * only one type of strip data is implemented: keyframe animation data.
 */
class Strip : public ::ActionStrip {
 public:
  /**
   * The possible types of strip data.
   *
   * Each enum value here corresponds to one data type. It is used to record
   * which type of data a strip refers to in the strip's `data_type` field (also
   * returned by `Strip::type()`). Each data type also knows which enum value it
   * corresponds to, stored in the type's static `TYPE` field.
   */
  enum class Type : int8_t { Keyframe = 0 };

  /* Strips typically shouldn't be directly constructed or copied, because their
   * data is actually stored in arrays on the action, and that data also needs
   * to be created and managed along with the strips. */
  Strip() = delete;

  /**
   * Make a shallow copy, effectively creating an *instance* of a strip.
   *
   * Does *not* make a copy of the strip's data, which is stored in an array on
   * the owning action. */
  explicit Strip(const Strip &other) = default;

  /**
   * Creates a new strip of type `type` for `owning_action`, with the strip's
   * data created on the relevant data array on `owning_action`.
   *
   * NOTE: strongly prefer using `Layer::strip_add()`, which creates a strip
   * directly on a layer and sidesteps any ambiguities about ownership.
   *
   * This method does *not* add the strip to a layer. That is the responsibility
   * of the caller.
   *
   * The strip is heap-allocated, and the caller is responsible for ensuring
   * that it gets freed or is given an owner (such as a layer) that will later
   * free it.
   *
   * The new strip is initialized to have infinite extent and zero time offset.
   *
   * \see `Layer::strip_add()`
   */
  static Strip &create(Action &owning_action, const Strip::Type type);

  /**
   * Strip type.
   *
   * Convenience wrapper to avoid having to do the cast from `int` to
   * `Strip::Type` everywhere.
   */
  Type type() const
  {
    return Type(this->strip_type);
  }

  /**
   * Return whether the strip's frame range extends from -infinity to +infinity.
   */
  bool is_infinite() const;

  /**
   * Return whether the given frame is within the strip's frame range.
   *
   * \note Strip frame ranges are inclusive on both sides.
   */
  bool contains_frame(float frame_time) const;

  /**
   * Return whether the end of the strip's frame range matches the given frame
   * time.
   */
  bool is_last_frame(float frame_time) const;

  /**
   * Set the start and end frame.
   *
   * This directly sets the start/end frames to the values given. It is up to
   * the caller to ensure the invariants of the strip itself and of the layer it
   * belongs to.
   *
   * `frame_start` must be less than or equal to `frame_end`. Infinite values
   * (negative for `frame_start`, positive for `frame_end`) are supported.
   */
  void resize(float frame_start, float frame_end);

  /**
   * Fetch the strip's data from its owning action.
   *
   * `T` *must* correspond to the strip's data type. In other words, this must
   * hold true: `T::TYPE == strip.type()`.
   *
   * For example, to get a keyframe strip's data:
   *
   * \code{.cc}
   * StripKeyframeData &strip_data = strip.data<StripKeyframeData>(action);
   * \endcode
   */
  template<typename T> const T &data(const Action &owning_action) const;
  template<typename T> T &data(Action &owning_action);
};
static_assert(sizeof(Strip) == sizeof(::ActionStrip),
              "DNA struct and its C++ wrapper must have the same size");

/**
 * Layers can be stacked on top of each other to define the animation. Each
 * layer has a mix mode and an influence (0-1), which define how it is mixed
 * with the layers below it.
 *
 * Layers contain one or more Strips, which in turn contain the animation data
 * itself.
 *
 * Temporary limitation: at most one strip may exist on a layer, and it extends
 * from negative to positive infinity.
 *
 * Note: the invariants around multiple strips (such as strip overlap, ordering
 * within the strip array, etc.) have not yet been decided. These will be
 * decided and documented when support for multiple strips is added.
 */
class Layer : public ::ActionLayer {
 public:
  Layer() = default;
  Layer(const Layer &other) = delete;
  ~Layer();

  /**
   * Duplicate the layer and its strips, but only make shallow copies of the
   * strips.
   *
   * Specifically, this doesn't duplicate the strip data that's stored in the
   * layer's owning action, leaving the fields of the strips themselves
   * exactly as-is.
   *
   * WARNING: this method is primarily used in the code that makes full
   * duplicates of actions, where the arrays of strip data are copied separately
   * for efficiency. This method's applications are narrow and you probably
   * shouldn't use it unless you really know what you're doing.
   */
  Layer *duplicate_with_shallow_strip_copies(StringRefNull allocation_name) const;

  enum class Flags : uint8_t {
    /* Set by default, cleared to mute. */
    Enabled = (1 << 0),
  };

  Flags flags() const
  {
    return static_cast<Flags>(this->layer_flags);
  }

  enum class MixMode : int8_t {
    /** Channels in this layer override the same channels from underlying layers. */
    Replace = 0,
    /** Channels in this layer are added to underlying layers as sequential operations. */
    Offset = 1,
    /** Channels in this layer are added to underlying layers on a per-channel basis. */
    Add = 2,
    /** Channels in this layer are subtracted to underlying layers on a per-channel basis. */
    Subtract = 3,
    /** Channels in this layer are multiplied with underlying layers on a per-channel basis. */
    Multiply = 4,
  };

  MixMode mix_mode() const
  {
    return static_cast<MixMode>(this->layer_mix_mode);
  }

  /* Strip array access. */
  blender::Span<const Strip *> strips() const;
  blender::Span<Strip *> strips();
  const Strip *strip(int64_t index) const;
  Strip *strip(int64_t index);

  /**
   * Add a new Strip of the given type.
   *
   * This creates a new infinite strip and appends it to the end of the layer's
   * strip array. It does no validation of invariants, and it is up to the
   * caller to ensure that invariants hold.
   */
  Strip &strip_add(Action &owning_action, Strip::Type strip_type);

  /**
   * Remove the strip from this layer.
   *
   * After this call, the passed reference is no longer valid, as the memory
   * will have been freed.
   *
   * \return true when the strip was found & removed, false if it wasn't found.
   */
  bool strip_remove(Action &owning_action, Strip &strip);

 protected:
  /**
   * Return the index of `strip` in this layer's strip array, or -1 if not found
   * in this layer.
   */
  int64_t find_strip_index(const Strip &strip) const;
};
static_assert(sizeof(Layer) == sizeof(::ActionLayer),
              "DNA struct and its C++ wrapper must have the same size");

ENUM_OPERATORS(Layer::Flags);

/**
 * Identifier for a sub-set of the animation data inside an Action.
 *
 * An animatable ID specifies both an `Action*` and an `ActionSlot::handle`
 * to identify which F-Curves (and in the future other animation data) it will
 * be animated by.
 *
 * \see #AnimData::slot_handle
 */
class Slot : public ::ActionSlot {
 public:
  Slot();
  explicit Slot(const Slot &other);
  ~Slot();

  /**
   * Update the Slot after reading it from a blend file.
   *
   * This is a low-level function and should not typically be used. It's only here to let
   * blenkernel allocate the runtime struct when reading a Slot from disk, without having to
   * share the struct definition itself. */
  void blend_read_post();

  /**
   * Slot handle value indicating that there is no slot assigned.
   */
  constexpr static slot_handle_t unassigned = 0;

  /**
   * Slot identifiers consist of a two-character ID code, then the display name.
   * This means that the minimum length of a valid identifier is 3 characters.
   */
  constexpr static int identifier_length_min = 3;

  constexpr static int identifier_length_max = MAX_ID_NAME;
  static_assert(sizeof(AnimData::last_slot_identifier) == identifier_length_max);
  static_assert(sizeof(NlaStrip::last_slot_identifier) == identifier_length_max);

  /**
   * Return a string that represents the Slot's 'idtype'.
   *
   * E.g "OB" for object, "CA" for camera, etc.
   *
   * This is different from `identifier_prefix()`: this constructs a
   * string directly from the actual 'idtype' field of the Slot, whereas
   * `identifier_prefix()` returns the first two characters of the
   * identifier string.
   *
   * This distinction matters in some lower-level code where the two can
   * momentarily be out of sync, although this should always be corrected before
   * exiting such code so that it's never observable in higher-level code.
   *
   * \see identifier_prefix()
   * \see identifier_ensure_prefix()
   */
  std::string idtype_string() const;

  /**
   * Return the two-character type prefix of this Slot's identifier.
   *
   * This corresponds to the intended ID type of the slot, e.g "OB" for object,
   * "CA" for camera, etc.
   *
   * This is subtly different from `idtype_string()`. See its documentation for
   * details.
   *
   * \see idtype_string()
   * \see identifier_ensure_prefix()
   */
  StringRef identifier_prefix() const;

  /**
   * Return this Slot's identifier without the prefix, also known as the
   * "display name".
   *
   * E.g. if the identifier is "OBCube", then "Cube" is returned.
   *
   * \see identifier_prefix()
   */
  StringRefNull identifier_without_prefix() const;

  /**
   * Return whether this Slot is suitable to be used by the given ID.
   *
   * "Suitable" means that one of the following is true:
   *
   * - The Slot's intended ID type (`idtype`) matches the given ID's type.
   * - The Slot's intended ID type is unspecified (see `has_idtype()`).
   *
   * If either of those hold true, the Slot is considered suitable for the ID.
   * Otherwise it is considered unsuitable.
   *
   * Note that it is possible, but odd, for an ID to use a Slot that is not
   * suitable for it. This is discouraged, and a best effort is made to prevent
   * this in typical cases, but it is not possible to completely prevent due to
   * library linking (e.g. an Action linked from another file may be replaced in
   * that other file, causing its Slots to effectively change). Therefore this
   * method returning `false` should NOT be taken as a guarantee that this Slot
   * will never be used by the given ID or other IDs of the same type.
   *
   * \see idtype_string()
   * \see has_idtype()
   */
  bool is_suitable_for(const ID &animated_id) const;

  /**
   * Return whether this Slot has a specified intended ID type (`idtype`) set.
   *
   * \see idtype_string()
   * \see is_suitable_for()
   */
  bool has_idtype() const;

  /* Flags access. */
  enum class Flags : uint8_t {
    /** Expanded/collapsed in animation editors. */
    Expanded = (1 << 0),
    /** Selected in animation editors. */
    Selected = (1 << 1),
    /** The active Slot for this Action. Set via a method on the Action. */
    Active = (1 << 2),
  };
  Flags flags() const;
  bool is_expanded() const;
  void set_expanded(bool expanded);
  bool is_selected() const;
  void set_selected(bool selected);
  bool is_active() const;

  /** Return the set of IDs that are animated by this Slot. */
  Span<ID *> users(Main &bmain) const;

  /**
   * Directly return the runtime users vector.
   *
   * This function does not refresh the users cache, so it may be out of date.
   *
   * This is a low-level function, and should only be used when calling `users(bmain)` is not
   * appropriate.
   *
   * \see #Slot::users(Main &bmain)
   */
  Vector<ID *> runtime_users();

  /**
   * Register this ID as animated by this Slot.
   *
   * This is a low-level function and should not typically be used.
   * Use #Action::assign_id(slot, animated_id) instead.
   */
  void users_add(ID &animated_id);

  /**
   * Register this ID as no longer animated by this Slot.
   *
   * This is a low-level function and should not typically be used.
   * Use #Action::assign_id(nullptr, animated_id) instead.
   */
  void users_remove(ID &animated_id);

  /**
   * Mark the users cache as 'dirty', triggering a full rebuild next time it is accessed.
   *
   * This is typically not necessary, and only called from low-level code.
   *
   * \note This static method invalidates all user caches of all Action Slots.
   *
   * \see #blender::animrig::internal::rebuild_slot_user_cache()
   * \see #blender::bke::animdata::action_slots_user_cache_invalidate(), which is an alternative to
   *      calling this static method in case the caller only wants to depend on BKE headers.
   */
  static void users_invalidate(Main &bmain);

  /**
   * Ensure the first two characters of this Slot's identifier match its
   * intended ID type.
   *
   * This typically does not need to be called outside of some low-level
   * functions. Aside from versioning code that upgrades legacy actions, Slots
   * should always be created with a specific intended ID type and corresponding
   * identifier prefix that never changes after creation, making this method
   * unnecessary.
   *
   * In the rare cases that a Slot does not have a specified intended ID type,
   * this method *still* should typically not be called directly. In those cases
   * prefer assigning to an ID (e.g. via `Action::assign_action_slot()`), which
   * will set the Slot's intended ID type and identifier prefix to match the
   * given ID's type, as well as ensure identifier uniqueness within the Action.
   *
   * \note This does NOT ensure identifier uniqueness within the Action. That is the
   * responsibility of the caller.
   *
   * \see #assign_action_slot
   */
  void identifier_ensure_prefix();

 protected:
  friend Action;

  /**
   * Set the 'Active' flag. Only allowed to be called by Action.
   */
  void set_active(bool active);
};
static_assert(sizeof(Slot) == sizeof(::ActionSlot),
              "DNA struct and its C++ wrapper must have the same size");
ENUM_OPERATORS(Slot::Flags);

/**
 * Keyframe animation data for a keyframe strip.
 *
 * This contains a set of Channelbags, up to one for each slot in the owning
 * action. Each Channelbag contains the keyframe animation data for the slot it
 * corresponds to.
 *
 * \see ChannelBag
 */
class StripKeyframeData : public ::ActionStripKeyframeData {
 public:
  /* Value of `Strip::type()` that corresponds to this type. */
  static constexpr Strip::Type TYPE = Strip::Type::Keyframe;

  StripKeyframeData() = default;
  explicit StripKeyframeData(const StripKeyframeData &other);
  ~StripKeyframeData();

  /* Channelbag array access. */
  blender::Span<const Channelbag *> channelbags() const;
  blender::Span<Channelbag *> channelbags();
  const Channelbag *channelbag(int64_t index) const;
  Channelbag *channelbag(int64_t index);

  /**
   * Find the channelbag for the given slot.
   *
   * \return nullptr if there is none yet for the given slot.
   */
  const Channelbag *channelbag_for_slot(const Slot &slot) const;
  Channelbag *channelbag_for_slot(const Slot &slot);
  const Channelbag *channelbag_for_slot(slot_handle_t slot_handle) const;
  Channelbag *channelbag_for_slot(slot_handle_t slot_handle);

  /**
   * Add a channelbag for the given slot.
   *
   * Should only be called when there is no `Channelbag` for this slot yet.
   */
  Channelbag &channelbag_for_slot_add(const Slot &slot);
  Channelbag &channelbag_for_slot_add(slot_handle_t slot_handle);

  /**
   * Find the channelbag for the given slot, or if none exists, create it.
   */
  Channelbag &channelbag_for_slot_ensure(const Slot &slot);
  Channelbag &channelbag_for_slot_ensure(slot_handle_t slot_handle);

  /**
   * Remove the given channelbag from this strip data.
   *
   * After this call the reference is no longer valid, as the memory will have been freed.
   *
   * \return true when the channelbag was found & removed, false if it wasn't found.
   */
  bool channelbag_remove(Channelbag &channelbag_to_remove);

  /**
   * Remove all strip data for the given slot.
   */
  void slot_data_remove(slot_handle_t slot_handle);

  /**
   * Clone the channelbag belonging to the source slot, and assign it to the target slot.
   *
   * This is typically only called from #duplicate_slot().
   */
  void slot_data_duplicate(slot_handle_t source_slot_handle, slot_handle_t target_slot_handle);

  /**
   * Return the index of `channelbag` in this strip data's channelbag array, or
   * -1 if `channelbag` doesn't exist in this strip data.
   */
  int64_t find_channelbag_index(const Channelbag &channelbag) const;

  SingleKeyingResult keyframe_insert(Main *bmain,
                                     const Slot &slot,
                                     const FCurveDescriptor &fcurve_descriptor,
                                     float2 time_value,
                                     const KeyframeSettings &settings,
                                     eInsertKeyFlags insert_key_flags = INSERTKEY_NOFLAGS,
                                     std::optional<float2> cycle_range = std::nullopt);
};
static_assert(sizeof(StripKeyframeData) == sizeof(::ActionStripKeyframeData),
              "DNA struct and its C++ wrapper must have the same size");

/**
 * Collection of F-Curves, intended for a specific Slot handle.
 *
 * In addition to F-Curves, Channelbags can also contain ChannelGroups, which
 * are used to organize F-Curves within the Channelbag (e.g. all F-Curves for a
 * given bone can be put into a ChannelGroup with that bone's name).
 *
 * \see ChannelGroup
 */
class Channelbag : public ::ActionChannelbag {
 public:
  Channelbag() = default;
  explicit Channelbag(const Channelbag &other);
  ~Channelbag();

  /* FCurves access. */
  blender::Span<const FCurve *> fcurves() const;
  blender::Span<FCurve *> fcurves();
  const FCurve *fcurve(int64_t index) const;
  FCurve *fcurve(int64_t index);

  /**
   * Find an FCurve matching the fcurve descriptor.
   *
   * If it cannot be found, `nullptr` is returned.
   */
  const FCurve *fcurve_find(const FCurveDescriptor &fcurve_descriptor) const;
  FCurve *fcurve_find(const FCurveDescriptor &fcurve_descriptor);

  /**
   * Find an FCurve matching the fcurve descriptor, or create one if it doesn't
   * exist.
   *
   * \param bmain: Used to tag the dependency graph(s) for relationship
   * rebuilding. This is necessary when adding a new F-Curve, as a
   * previously-unanimated depsgraph component may become animated now. Can be
   * nullptr, in which case the tagging is skipped and is left as the
   * responsibility of the caller.
   */
  FCurve &fcurve_ensure(Main *bmain, const FCurveDescriptor &fcurve_descriptor);

  /**
   * Create an F-Curve, but only if it doesn't exist yet in this Channelbag.
   *
   * \return the F-Curve was created, or nullptr if it already existed.
   *
   * \param bmain: Used to tag the dependency graph(s) for relationship
   * rebuilding. This is necessary when adding a new F-Curve, as a
   * previously-unanimated depsgraph component may become animated now. Can be
   * nullptr, in which case the tagging is skipped and is left as the
   * responsibility of the caller.
   */
  FCurve *fcurve_create_unique(Main *bmain, const FCurveDescriptor &fcurve_descriptor);

  /**
   * Create many F-Curves at once.
   *
   * Conceptually the same as adding many curves in a loop:
   * \code{.cc}
   * Vector<FCurve*> res(fcurve_descriptors.size(), nullptr);
   * for (int64_t i = 0; i < fcurve_descriptors.size(); i++) {
   *  const FCurveDescriptor &desc = fcurve_descriptors[i];
   *  res[i] = this->fcurve_create_unique(bmain, desc);
   * }
   * return res;
   * \endcode
   *
   * However that is quadratic complexity due to each curve uniqueness check being
   * a linear scan, plus invariants rebuilding after each curve.
   *
   * \return Vector of created F-Curves. Vector size is the same as input span size.
   * A vector element can be nullptr if input descriptor has empty RNA path, or if
   * if such curve already exists.
   *
   * \param bmain: Used to tag the dependency graph(s) for relationship
   * rebuilding. This is necessary when adding a new F-Curve, as a
   * previously-unanimated depsgraph component may become animated now. Can be
   * nullptr, in which case the tagging is skipped and is left as the
   * responsibility of the caller.
   */
  Vector<FCurve *> fcurve_create_many(Main *bmain, Span<FCurveDescriptor> fcurve_descriptors);

  /**
   * Append an F-Curve to this Channelbag.
   *
   * This transfers ownership of the F-Curve to this Channelbag, and it is up to
   * the caller to ensure that this is valid (e.g. the F-Curve doesn't also
   * belong to something else).
   *
   * The F-Curve will not be member of any group after appending.
   *
   * This is considered a low-level function. Things like depsgraph relations
   * tagging is left to the caller.
   */
  void fcurve_append(FCurve &fcurve);

  /**
   * Remove an F-Curve from the Channelbag.
   *
   * Additionally, if the F-Curve was the last F-Curve in a channel group, that
   * channel group is also deleted.
   *
   * After this call, if the F-Curve was found, the reference will no longer be
   * valid, as the curve will have been freed.
   *
   * \return true when the F-Curve was found & removed, false if it wasn't found.
   *
   * \see fcurve_detach
   */
  bool fcurve_remove(FCurve &fcurve_to_remove);

  /**
   * Remove an F-Curve from the Channelbag, identified by its index in the array.
   *
   * Acts the same as fcurve_remove() except it's a bit more efficient as it
   * doesn't need to find the F-Curve in the array first.
   *
   * \see fcurve_remove
   */
  void fcurve_remove_by_index(int64_t fcurve_index);

  /**
   * Detach an F-Curve from the Channelbag.
   *
   * Additionally, if the fcurve was the last fcurve in a channel group, that
   * channel group is deleted.
   *
   * The F-Curve is not freed. After the call returns `true`, its ownership has
   * transferred to the caller.
   *
   * \return true when the F-Curve was found & detached, false if it wasn't found.
   *
   * \see fcurve_remove
   */
  bool fcurve_detach(FCurve &fcurve_to_detach);

  /**
   * Detach an F-Curve from the Channelbag, identified by its index in the array.
   *
   * Acts the same as fcurve_detach() except it's a bit more efficient as it
   * doesn't need to find the F-Curve in the array first.
   *
   * \see fcurve_detach
   */
  void fcurve_detach_by_index(int64_t fcurve_index);

  /**
   * Move the given fcurve to position `to_fcurve_index` in the fcurve array.
   *
   * Note: this can indirectly alter channel group memberships, because the
   * channel groups don't change what ranges in the fcurve array they cover.
   *
   * `fcurve` must belong to this channel bag, and `to_fcurve_index` must be a
   * valid index in the fcurve array.
   */
  void fcurve_move_to_index(FCurve &fcurve, int to_fcurve_index);

  /**
   * Remove all F-Curves from this Channelbag.
   *
   * Since all channel groups become empty, this also removes all channel
   * groups.
   */
  void fcurves_clear();

  /* Channel group access. */
  blender::Span<const bActionGroup *> channel_groups() const;
  blender::Span<bActionGroup *> channel_groups();
  const bActionGroup *channel_group(int64_t index) const;
  bActionGroup *channel_group(int64_t index);

  /**
   * Find the first bActionGroup (channel group) with the given name.
   *
   * Note that channel groups with the same name are allowed, and this simply
   * returns the first match.
   *
   * If no matching group is found, `nullptr` is returned.
   */
  const bActionGroup *channel_group_find(StringRef name) const;
  bActionGroup *channel_group_find(StringRef name);

  /**
   * Find the index of the channel group.
   *
   * \return The index of the channel group if found, or -1 if no such group is
   * found.
   */
  int channel_group_find_index(const bActionGroup *group) const;

  /**
   * Find the channel group that contains the fcurve at `fcurve_array_index` as
   * a member.
   *
   * \return The index of the channel group if found, or -1 if no such group is
   * found.
   */
  int channel_group_containing_index(int fcurve_array_index);

  /**
   * Create a new empty channel group with the given name.
   *
   * The new group is added to the end of the channel group array of the
   * Channelbag.
   *
   * This function ensures the group has a unique name, and thus the name of the
   * created group may differ from the `name` parameter.
   *
   * \return A reference to the new channel group.
   */
  bActionGroup &channel_group_create(StringRefNull name);

  /**
   * Find a channel group with the given name, or if none exists create one.
   *
   * If a new group is created, it's added to the end of the channel group array
   * of the Channelbag.
   *
   * \return A reference to the channel group.
   */
  bActionGroup &channel_group_ensure(StringRefNull name);

  /**
   * Remove the given channel group from the channel bag.
   *
   * Any fcurves that were part of this group will me moved to just after all
   * grouped fcurves.
   *
   * \return true when the channel group was found & removed, false if it wasn't
   * found.
   */
  bool channel_group_remove(bActionGroup &group);

  /**
   * Move the given channel group's to position `to_group_index` among the
   * channel groups.
   *
   * The fcurves in the channel group are moved with it, so that membership
   * doesn't change.
   *
   * `group` must belong to this channel bag, and `to_group_index` must be a
   * valid index in the channel group array.
   */
  void channel_group_move_to_index(bActionGroup &group, int to_group_index);

  /**
   * Assigns the given FCurve to the given channel group.
   *
   * Fails if either doesn't belong to this channel bag, but otherwise always
   * succeeds.
   *
   * \return True on success, false on failure.
   */
  bool fcurve_assign_to_channel_group(FCurve &fcurve, bActionGroup &to_group);

  /**
   * Removes the given FCurve from the channel group it's in, if any.
   *
   * As part of removing `fcurve` from its group, `fcurve` is moved to the end
   * of the fcurve array. However, if `fcurve` is already ungrouped then this
   * method is a no-op.
   *
   * Fails if the fcurve doesn't belong to this channel bag, but otherwise
   * always succeeds.
   *
   * \return True on success, false on failure.
   */
  bool fcurve_ungroup(FCurve &fcurve);

 protected:
  /**
   * Create an F-Curve.
   *
   * Assumes that there is no such F-Curve yet on this Channelbag. If it is
   * uncertain whether this is the case, use `fcurve_create_unique()` instead.
   *
   * \param bmain: Used to tag the dependency graph(s) for relationship
   * rebuilding. This is necessary when adding a new F-Curve, as a
   * previously-unanimated depsgraph component may become animated now. Can be
   * nullptr, in which case the tagging is skipped and is left as the
   * responsibility of the caller.
   */
  FCurve &fcurve_create(Main *bmain, const FCurveDescriptor &fcurve_descriptor);

 private:
  /**
   * Remove the channel group at `channel_group_index` from the channel group
   * array.
   *
   * This is a low-level function that *only* manipulates the channel group
   * array in the most basic way. It literally just removes the given item from
   * the array and frees it, just like `erase()` on `std::vector`.
   *
   * It specifically does *not* maintain any of the semantic invariants of the
   * group array or its relationship to the fcurves.
   *
   * `restore_channel_group_invariants()` should be called at some point after
   * this to restore the semantic invariants.
   *
   * \see `restore_channel_group_invariants()`
   */
  void channel_group_remove_raw(int group_index);

  /**
   * Restore invariants related to channel groups.
   *
   * This restores critical invariants and should be called (at some point) any
   * time that groups are explicitly modified or that group membership of
   * fcurves might change implicitly (e.g. due to moving/adding/removing
   * fcurves).
   *
   * The specific invariants restored by this method are:
   * 1. All grouped fcurves should come before all non-grouped fcurves.
   * 2. All fcurves should point back to the group they belong to (if any) via
   *    their `grp` pointer.
   *
   * This function assumes that the fcurves are already in the correct group
   * order (so the first N belong to the first group, which is also of length N,
   * etc.). The groups are then updated so their starting index matches this.
   * Then the fcurves' `grp` pointer is updated, so that any changes in group
   * membership is correctly reflected.
   *
   * For example, if the mapping of groups to fcurves looks like this (g* are
   * the groups, dots indicate ungrouped areas, and f* are the fcurves, so e.g.
   * group g0 currently contains f1 and f2, but ought to contain f0 and f1):
   *
   * \code{.unparsed}
   * |..| g0  |..|g1|.....| g2  |..|
   * |f0|f1|f2|f3|f4|f5|f6|f7|f8|f9|
   * \endcode
   *
   * Then after calling this function they will look like this:
   *
   * \code{.unparsed}
   * | g0  |g1| g2  |..............|
   * |f0|f1|f2|f3|f4|f5|f6|f7|f8|f9|
   * \endcode
   *
   * Note that this specifically does *not* move the fcurves, but rather moves
   * the groups *over* the fcurves, changing membership.
   *
   * The `grp` pointers in the fcurves are then updated to reflect their new
   * group membership, using the groups as the source of truth.
   */
  void restore_channel_group_invariants();
};

static_assert(sizeof(Channelbag) == sizeof(::ActionChannelbag),
              "DNA struct and its C++ wrapper must have the same size");

/**
 * A group of channels within a Channelbag.
 *
 * This does *not* own the fcurves--the Channelbag does. This just groups
 * fcurves for organizational purposes, e.g. for use in the channel list in the
 * animation editors.
 */
class ChannelGroup : public ::bActionGroup {
 public:
  /**
   * Determine whether this channel group is from a legacy action or a layered action.
   *
   * TODO: this should be removed, as it's currently only used by code that is
   * no longer relevant and should also be removed due to legacy actions no
   * longer being supported at runtime.
   *
   * \return True if it's from a legacy action, false if it's from a layered action.
   */
  bool is_legacy() const;

  /**
   * Get the fcurves in this channel group.
   */
  Span<FCurve *> fcurves();
  Span<const FCurve *> fcurves() const;
};

static_assert(sizeof(ChannelGroup) == sizeof(::bActionGroup),
              "DNA struct and its C++ wrapper must have the same size");

/**
 * Create a new Action with zero users.
 *
 * This is basically the same as `BKE_action_add`, except that the Action has
 * zero users and it's already wrapped with its C++ wrapper.
 *
 * \see #BKE_action_add
 */
Action &action_add(Main &bmain, StringRefNull name);

/* ---------- Action & Slot Assignment --------------- */

enum class ActionSlotAssignmentResult : int8_t {
  OK = 0,
  SlotNotFromAction = 1, /* Slot does not belong to the assigned Action. */
  SlotNotSuitable = 2,   /* Slot is not suitable for the given ID type. */
  MissingAction = 3,     /* No Action assigned yet, so cannot assign slot. */
};

/**
 * Return whether the given Action can be assigned to the ID.
 *
 * This always returns `true` for layered Actions. For legacy Actions it
 * returns `true` if the Action's `idroot` matches the ID.
 */
[[nodiscard]] bool is_action_assignable_to(const bAction *dna_action, ID_Type id_code);

/**
 * Assign the Action to the ID.
 *
 * This will make a best-effort guess as to which slot to use, in this
 * order;
 *
 * - By slot handle.
 * - By fallback string.
 * - By the ID's name (matching against the slot name).
 * - If the above do not find a suitable slot, the animated ID will not
 *   receive any animation and the caller is responsible for creating a slot
 *   and assigning it.
 *
 * \return `false` if the assignment was not possible (for example the ID is of a type that cannot
 * be animated). If the above fall-through case of "no slot found" is reached, this function
 * will still return `true` as the Action was successfully assigned.
 */
[[nodiscard]] bool assign_action(bAction *action, ID &animated_id);

/**
 * Same as assign_action(action, id) above.
 *
 * Use this function when you already have the AnimData struct of this ID.
 *
 * \return true when successful, false otherwise. This can fail when the NLA is in tweak mode (no
 * action changes allowed) or when a legacy Action is assigned and it doesn't match the animated
 * ID's type.
 */
[[nodiscard]] bool assign_action(bAction *action, OwnedAnimData owned_adt);

ActionSlotAssignmentResult assign_action_slot(Slot *slot_to_assign, ID &animated_id);

/**
 * Utility function that assigns both an Action and a slot of that Action.
 *
 * Returns the result of the underlying assign_action_slot() call.
 *
 * \see assign_action
 * \see assign_action_slot
 */
ActionSlotAssignmentResult assign_action_and_slot(Action *action,
                                                  Slot *slot_to_assign,
                                                  ID &animated_id);

/**
 * Assign the Action, ensuring that a Slot is also assigned.
 *
 * If this Action happens to already be assigned, and a Slot is assigned too, that Slot is
 * returned. Otherwise a new Slot is created + assigned.
 *
 * \returns the assigned slot if the assignment was successful, or `nullptr` otherwise. Reasons the
 * assignment can fail is when the given ID is of an animatable type, when the ID is in NLA Tweak
 * mode (in which case no Action assignments can happen), or when the legacy Action ID type doesn't
 * match the animated ID.
 *
 * \note Contrary to `assign_action()` this skips the search by slot identifier when the Action is
 * already assigned. It should be possible for an animator to un-assign a slot, then create a new
 * slot by inserting a new key. This shouldn't auto-assign the old slot (by identifier) and _then_
 * insert the key.
 *
 * \see assign_action()
 */
[[nodiscard]] Slot *assign_action_ensure_slot_for_keying(Action &action, ID &animated_id);

/**
 * Same as assign_action, except it assigns to #AnimData::tmpact and #AnimData::tmp_slot_handle.
 */
[[nodiscard]] bool assign_tmpaction(bAction *action, OwnedAnimData owned_adt);

[[nodiscard]] ActionSlotAssignmentResult assign_tmpaction_and_slot_handle(
    bAction *action, slot_handle_t slot_handle, OwnedAnimData owned_adt);

/**
 * Un-assign the Action assigned to this ID.
 *
 * Same as calling `assign_action(nullptr, animated_id)`.
 *
 * \see #blender::animrig::assign_action(ID &animated_id)
 */
[[nodiscard]] bool unassign_action(ID &animated_id);

/**
 * Un-assign the Action assigned to this ID.
 *
 * Same as calling `assign_action(nullptr, owned_adt)`.
 *
 * \see #blender::animrig::assign_action(OwnedAnimData owned_adt)
 */
[[nodiscard]] bool unassign_action(OwnedAnimData owned_adt);

/**
 * Generic function to build Action-assignment logic.
 *
 * This is a low-level function, intended as a building block for higher-level Action assignment
 * functions.
 *
 * The function is named "generic" as it is independent of whether this is for
 * direct assignment to the ID, or to an NLA strip, or an Action Constraint.
 */
[[nodiscard]] bool generic_assign_action(ID &animated_id,
                                         bAction *action_to_assign,
                                         bAction *&action_ptr_ref,
                                         slot_handle_t &slot_handle_ref,
                                         char *slot_identifier);

/**
 * Generic function to build Slot-assignment logic.
 *
 * This is a low-level function, intended as a building block for higher-level slot assignment
 * functions.
 *
 * The function is named "generic" as it is independent of whether this is for
 * direct assignment to the ID, or to an NLA strip, or an Action Constraint.
 */
[[nodiscard]] ActionSlotAssignmentResult generic_assign_action_slot(Slot *slot_to_assign,
                                                                    ID &animated_id,
                                                                    bAction *&action_ptr_ref,
                                                                    slot_handle_t &slot_handle_ref,
                                                                    char *slot_identifier);

/**
 * Generic function to build Slot Handle-assignment logic.
 *
 * This is a low-level function, intended as a building block for higher-level slot handle
 * assignment functions.
 *
 * The function is named "generic" as it is independent of whether this is for
 * direct assignment to the ID, or to an NLA strip, or an Action Constraint.
 */
[[nodiscard]] ActionSlotAssignmentResult generic_assign_action_slot_handle(
    slot_handle_t slot_handle_to_assign,
    ID &animated_id,
    bAction *&action_ptr_ref,
    slot_handle_t &slot_handle_ref,
    char *slot_identifier);

/**
 * Generic function for finding the slot to auto-assign when the Action is assigned.
 *
 * This is a low-level function, used by generic_assign_action() to pick a slot.
 * It's declared here so that unit tests can reach it.
 *
 * The function is named "generic" as it is independent of whether this is for
 * direct assignment to the ID, or to an NLA strip, or an Action Constraint.
 *
 * \see #generic_assign_action()
 * \see #generic_assign_action_slot()
 * \see #generic_assign_action_slot_handle()
 */
[[nodiscard]] Slot *generic_slot_for_autoassign(const ID &animated_id,
                                                Action &action,
                                                StringRefNull last_slot_identifier);

/* --------------- Accessors --------------------- */

/**
 * Return the Action of this ID, or nullptr if it has none.
 */
Action *get_action(ID &animated_id);

/**
 * Get the Action and the Slot that animate this ID.
 *
 * \return One of two options:
 *  - `pair<Action, Slot>` when an Action and a Slot are assigned. In other
 *    words, when this ID is actually animated by this Action+Slot pair.
 *  - `nullopt`: when this ID is not animated. This can have several causes: not
 *    an animatable type, no Action assigned, or no Slot assigned.
 */
std::optional<std::pair<Action *, Slot *>> get_action_slot_pair(ID &animated_id);

const animrig::Channelbag *channelbag_for_action_slot(const Action &action,
                                                      slot_handle_t slot_handle);
animrig::Channelbag *channelbag_for_action_slot(Action &action, slot_handle_t slot_handle);

/**
 * Return the F-Curves for this specific slot handle.
 *
 * This is just a utility function, that's intended to become obsolete when multi-layer Actions
 * are introduced. However, since Blender currently only supports a single layer with a single
 * strip, of a single type, this function can be used.
 *
 * The use of this function is also an indicator for code that will have to be altered when
 * multi-layered Actions are getting implemented.
 *
 * \note This function requires a layered Action. To transparently handle legacy Actions, see the
 * `animrig::legacy` namespace.
 *
 * \see #blender::animrig::legacy::fcurves_for_action_slot
 */
Span<FCurve *> fcurves_for_action_slot(Action &action, slot_handle_t slot_handle);
Span<const FCurve *> fcurves_for_action_slot(const Action &action, slot_handle_t slot_handle);

/**
 * Find or create a Channelbag on the given action, for the given ID.
 *
 * This function also ensures that there is a layer and a keyframe strip for the
 * channelbag to exist on.
 *
 * \param action: MUST already be assigned to the animated ID.
 *
 * \param animated_id: The ID that is animated by this Action. It is used to
 * create and assign an appropriate slot if needed when creating the fcurve, and
 * set the fcurve color properly
 */
Channelbag &action_channelbag_ensure(bAction &dna_action, ID &animated_id);

/**
 * Find or create an F-Curve on the given action that matches the given fcurve
 * descriptor.
 *
 * \param bmain:  If not nullptr, this function also ensures that dependency
 * graph relationships are rebuilt. This is necessary when adding a new F-Curve,
 * as a previously-unanimated depsgraph component may become animated now.
 *
 * \param action: MUST already be assigned to the animated ID.
 *
 * \param animated_id: The ID that is animated by this Action. It is used to
 * create and assign an appropriate slot if needed when creating the fcurve, and
 * set the fcurve color properly
 *
 * \param fcurve_descriptor: description of the fcurve to lookup/create. Note
 * that this is *not* relative to `ptr` (e.g. if `ptr` is not an ID). It should
 * contain the exact data path of the fcurve to be looked up/created.
 */
FCurve &action_fcurve_ensure(Main *bmain,
                             bAction &action,
                             ID &animated_id,
                             const FCurveDescriptor &fcurve_descriptor);

/**
 * Find or create an F-Curve on the given action that matches the given fcurve
 * descriptor.
 *
 * This function is primarily intended for use with legacy actions, but for
 * reasons of expedience it now also works with layered actions under the
 * following limited circumstances: `ptr` must be non-null and must have an
 * `owner_id` that already uses `act`. See the comments in the implementation
 * for more details.
 *
 * \note This function also ensures that dependency graph relationships are
 * rebuilt. This is necessary when adding a new F-Curve, as a
 * previously-unanimated depsgraph component may become animated now.
 *
 * \param ptr: RNA pointer for the struct the fcurve is being looked up/created
 * for. It is used to create and assign an appropriate slot if needed when
 * creating the fcurve, and set the fcurve color properly
 *
 * \param fcurve_descriptor: description of the fcurve to lookup/create. Note
 * that this is *not* relative to `ptr` (e.g. if `ptr` is not an ID). It should
 * contain the exact data path of the fcurve to be looked up/created.
 *
 * \see action_fcurve_ensure for a function that is specific to layered actions,
 * and is easier to use because it does not depend on an RNA pointer.
 */
FCurve *action_fcurve_ensure_ex(Main *bmain,
                                bAction *act,
                                const char group[],
                                PointerRNA *ptr,
                                const FCurveDescriptor &fcurve_descriptor);

/**
 * Same as above, but creates a legacy Action.
 *
 * \note this function should ONLY be used in unit tests, in order to create
 * legacy Actions for testing. Or in the very rare cases where handling of
 * legacy Actions is still necessary AND you have no PointerRNA. In all other
 * cases, just call #action_fcurve_ensure, it'll do the right thing
 * transparently on whatever Action you give it.
 */
FCurve *action_fcurve_ensure_legacy(Main *bmain,
                                    bAction *act,
                                    const char group[],
                                    PointerRNA *ptr,
                                    const FCurveDescriptor &fcurve_descriptor);

/**
 * Find the F-Curve in the given Action.
 *
 * All the Action slots are searched for this F-Curve. To limit to a single
 * slot, use fcurve_find_in_action_slot().
 *
 * \see #blender::animrig::fcurve_find_in_action_slot
 */
FCurve *fcurve_find_in_action(bAction *act, const FCurveDescriptor &fcurve_descriptor);

/**
 * Find the F-Curve in the given Action Slot.
 *
 * \see #blender::animrig::fcurve_find_in_action
 */
FCurve *fcurve_find_in_action_slot(bAction *act,
                                   slot_handle_t slot_handle,
                                   const FCurveDescriptor &fcurve_descriptor);

/**
 * Find the F-Curve in the Action Slot assigned to this ADT.
 *
 * \see #blender::animrig::fcurve_find_in_action
 */
FCurve *fcurve_find_in_assigned_slot(AnimData &adt, const FCurveDescriptor &fcurve_descriptor);

/**
 * Return whether `fcurve` targets the given collection path + data name.
 *
 * For example, to match F-Curves for the pose bone named `"botje"`, you'd pass
 * `collection_rna_path = "pose.bones["` and `data_name="botje"`.
 *
 * \return True if `fcurve` matches, false if it doesn't.
 */
bool fcurve_matches_collection_path(const FCurve &fcurve,
                                    StringRefNull collection_rna_path,
                                    StringRefNull data_name);

/**
 * Return the F-Curves in the given action+slot for which `predicate` returns
 * true.
 *
 * This works for both layered and legacy actions. For legacy actions the slot
 * handle is ignored.
 */
Vector<FCurve *> fcurves_in_action_slot_filtered(
    bAction *act, slot_handle_t slot_handle, FunctionRef<bool(const FCurve &fcurve)> predicate);

/**
 * Return the F-Curves in the given span for which `predicate` returns true.
 */
Vector<FCurve *> fcurves_in_span_filtered(Span<FCurve *> fcurves,
                                          FunctionRef<bool(const FCurve &fcurve)> predicate);

/**
 * Return the F-Curves in the given listbase for which `predicate` returns
 * true.
 */
Vector<FCurve *> fcurves_in_listbase_filtered(ListBase /* FCurve * */ fcurves,
                                              FunctionRef<bool(const FCurve &fcurve)> predicate);

/**
 * Remove the given FCurve from the action by searching for it in all channelbags.
 * This assumes that an FCurve can only exist in an action once.
 *
 * Compatible with both legacy and layered Actions.
 *
 *  \returns true if the given FCurve was removed.
 *
 * \see action_fcurve_detach
 */
bool action_fcurve_remove(Action &action, FCurve &fcu);

/**
 * Detach the F-Curve from the Action, searching for it in all channelbags.
 *
 * Compatible with both legacy and layered Actions. The slot handles are ignored
 * for legacy Actions.
 *
 * The F-Curve is not freed, and ownership is transferred to the caller.
 *
 * \see action_fcurve_remove
 * \see action_fcurve_attach
 * \see action_fcurve_move
 *
 * \return true when the F-Curve was found and detached, false if not found.
 */
bool action_fcurve_detach(Action &action, FCurve &fcurve_to_detach);

/**
 * Attach the F-Curve to the Action Slot.
 *
 * Compatible with both legacy and layered Actions. The slot handle is ignored
 * for legacy Actions.
 *
 * On layered Actions, this assumes the 'Baklava Phase 1' invariants (one layer,
 * one keyframe strip).
 *
 * \see action_fcurve_detach
 * \see action_fcurve_move
 */
void action_fcurve_attach(Action &action,
                          slot_handle_t action_slot,
                          FCurve &fcurve_to_attach,
                          std::optional<StringRefNull> group_name);

/**
 * Move an F-Curve from one Action to the other.
 *
 * If the F-Curve was part of a channel group, the group membership also carries
 * over to the destination Action. If no group with the same name exists, it is
 * created. This only happens for layered Actions, though.
 *
 * Compatible with both legacy and layered Actions. The slot handle and group
 * membership are ignored for legacy Actions.
 *
 * The F-Curve must exist on the source Action. All channelbags for all slots
 * are searched for the F-Curve.
 *
 * \param action_slot_dst: may not be #Slot::unassigned on layered Actions.
 *
 * \see #blender::animrig::action_fcurve_detach
 */
void action_fcurve_move(Action &action_dst,
                        slot_handle_t action_slot_dst,
                        Action &action_src,
                        FCurve &fcurve);

/**
 * Moves all F-Curves from one Channelbag to the other.
 *
 * The Channelbags do not need to be part of the same action, or even belong to
 * an action at all.
 *
 * If the F-Curves belonged to channel groups, the group membership also carries
 * over to the destination Channelbag. If groups with the same names don't
 * exist, they are created. \see #blender::animrig::action_fcurve_detach
 *
 * The order of existing channel groups in the destination Channelbag are not
 * changed, and any new groups are placed after those in the order they appeared
 * in the src group.
 */
void channelbag_fcurves_move(Channelbag &channelbag_dst, Channelbag &channelbag_src);

/**
 * Find an appropriate user of the given Action + Slot for keyframing purposes.
 *
 * (NOTE: although this function exists for handling situations caused by the
 * expanded capabilities of layered actions, for convenience it also works with
 * legacy actions. For legacy actions this simply returns `primary_id` as long
 * as it's a user of `action`.)
 *
 * Usually this function shouldn't be necessary, because you'll already have an
 * obvious ID that you're keying. But in some cases (such as the action editor
 * where multiple slots are accessible) the active ID that would normally get
 * keyed might have nothing to do with the slot that's actually getting keyed.
 *
 * This function handles such cases by attempting to find an actual user of the
 * slot that's appropriate for keying. More specifically:
 *
 * - If `primary_id` is a user of the slot, `primary_id` is always returned.
 * - If the slot has precisely one user, that user is returned.
 * - Otherwise, nullptr is returned.
 *
 * In other words, the cases where a user of the slot is *not* returned are:
 *
 * - The slot has no users at all.
 * - The slot has multiple users, none of which are `primary_id`, and therefore
 *   there is no single, clear user that can be appropriately used for keying.
 *
 * \param primary_id: whenever this is among the users of the action + slot, it
 * is given priority and is returned. May be null.
 */
ID *action_slot_get_id_for_keying(Main &bmain,
                                  Action &action,
                                  slot_handle_t slot_handle,
                                  ID *primary_id);

/**
 * Make a best-effort guess as to which ID* is animated by the given slot.
 *
 * This is only used in rare cases; usually the ID* for which operations are
 * performed is known.
 *
 * \note This function was specifically written because the 'display name' of an
 * F-Curve can only be determined by resolving its RNA path, and for that an ID*
 * is necessary. It would be better to cache that name on the F-Curve itself, so
 * that this constant resolving (for drawing, filtering by name, etc.) isn't
 * necessary any more.
 */
ID *action_slot_get_id_best_guess(Main &bmain, Slot &slot, ID *primary_id);

/**
 * Return the handle of the first slot of this Action.
 *
 * This is for code that needs to treat Actions as somewhat-legacy Actions, i.e. as holders of
 * F-Curves for which the specific slot is not interesting.
 *
 * TODO: Maybe at some point this function should get extended with an ID type parameter, to return
 * the first slot that is suitable for that ID type.
 *
 * \return The handle of the first slot, or #Slot::unassigned if there is no slot (which includes
 * legacy Actions).
 */
slot_handle_t first_slot_handle(const ::bAction &dna_action);

/**
 * Assert the invariants of Project Baklava phase 1.
 *
 * For an action the invariants are that it:
 * - Is a legacy action.
 * - OR has zero layers.
 * - OR has a single layer that adheres to the phase 1 invariants for layers.
 *
 * For a layer the invariants are that it:
 * - Has zero strips.
 * - OR has a single strip that adheres to the phase 1 invariants for strips.
 *
 * For a strip the invariants are that it:
 * - Is a keyframe strip.
 * - AND is infinite.
 * - AND has no time offset (i.e. aligns with scene time).
 *
 * This simultaneously serves as a todo marker for later phases of Project
 * Baklava and ensures that the phase-1 invariants hold at runtime.
 *
 * TODO: these functions should be changed to assert fewer and fewer assumptions
 * as we progress through the phases of Project Baklava and more and more of the
 * new animation system is implemented. Finally, they should be removed entirely
 * when the full system is completely implemented.
 */
void assert_baklava_phase_1_invariants(const Action &action);
/** \copydoc assert_baklava_phase_1_invariants(const Action &) */
void assert_baklava_phase_1_invariants(const Layer &layer);
/** \copydoc assert_baklava_phase_1_invariants(const Action &) */
void assert_baklava_phase_1_invariants(const Strip &strip);

/**
 * Creates a new `Action` that matches the old action but is converted to have layers.
 * Returns a nullptr if the action is empty or already layered.
 */
Action *convert_to_layered_action(Main &bmain, const Action &legacy_action);

/**
 * Move the given slot from `from_action` to `to_action`.
 * The slot identifier might not be exactly the same if the identifier already exists in the slots
 * of `to_action`. Also the slot handle is likely going to be different on `to_action`. All users
 * of the slot will be reassigned to the moved slot on `to_action`.
 *
 * \note The `from_action` will not be deleted by this function. But it might leave it without
 * users which means it will not be saved (unless it has a fake user).
 */
void move_slot(Main &bmain, Slot &slot, Action &from_action, Action &to_action);

/**
 * Duplicate a slot, and all its animation data.
 *
 * Data-blocks using the slot are not updated, so the returned slot will be unused.
 *
 * The `action` MUST own `slot`.
 */
Slot &duplicate_slot(Action &action, const Slot &slot);

/**
 * Deselect the keys of all actions in the Span. Duplicate entries are only visited once.
 */
void deselect_keys_actions(blender::Span<bAction *> actions);

/**
 * Deselect all keys within the action.
 */
void action_deselect_keys(Action &action);

}  // namespace blender::animrig

/* Wrap functions for the DNA structs. */

inline blender::animrig::ChannelGroup &bActionGroup::wrap()
{
  return *reinterpret_cast<blender::animrig::ChannelGroup *>(this);
}
inline const blender::animrig::ChannelGroup &bActionGroup::wrap() const
{
  return *reinterpret_cast<const blender::animrig::ChannelGroup *>(this);
}

inline blender::animrig::Action &bAction::wrap()
{
  return *reinterpret_cast<blender::animrig::Action *>(this);
}
inline const blender::animrig::Action &bAction::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Action *>(this);
}

inline blender::animrig::Layer &ActionLayer::wrap()
{
  return *reinterpret_cast<blender::animrig::Layer *>(this);
}
inline const blender::animrig::Layer &ActionLayer::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Layer *>(this);
}

inline blender::animrig::Slot &ActionSlot::wrap()
{
  return *reinterpret_cast<blender::animrig::Slot *>(this);
}
inline const blender::animrig::Slot &ActionSlot::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Slot *>(this);
}

inline blender::animrig::Strip &ActionStrip::wrap()
{
  return *reinterpret_cast<blender::animrig::Strip *>(this);
}
inline const blender::animrig::Strip &ActionStrip::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Strip *>(this);
}

inline blender::animrig::StripKeyframeData &ActionStripKeyframeData::wrap()
{
  return *reinterpret_cast<blender::animrig::StripKeyframeData *>(this);
}
inline const blender::animrig::StripKeyframeData &ActionStripKeyframeData::wrap() const
{
  return *reinterpret_cast<const blender::animrig::StripKeyframeData *>(this);
}

inline blender::animrig::Channelbag &ActionChannelbag::wrap()
{
  return *reinterpret_cast<blender::animrig::Channelbag *>(this);
}
inline const blender::animrig::Channelbag &ActionChannelbag::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Channelbag *>(this);
}
