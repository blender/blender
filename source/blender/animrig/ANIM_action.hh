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

#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "RNA_types.hh"

#include <utility>

struct AnimationEvalContext;
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

/* Use an alias for the Slot handle type to help disambiguate function parameters. */
using slot_handle_t = decltype(::ActionSlot::handle);

/**
 * Container of animation data for one or more animated IDs.
 *
 * Broadly an Action consists of Layers, each Layer has Strips, and it's the
 * Strips that eventually contain the animation data.
 *
 * Temporary limitation: each Action can only contain one Layer.
 *
 * Which sub-set of that data drives the animation of which ID is determined by
 * which Slot is associated with that ID.
 *
 * \note This wrapper class for the `bAction` DNA struct only has functionality
 * for the layered animation data. The legacy F-Curves (in `bAction::curves`)
 * and their groups (in `bAction::groups`) are not managed here. To see whether
 * an Action uses this legacy data, or has been converted to the current layered
 * structure, use `Action::is_action_legacy()` and
 * `Action::is_action_layered()`. Note that an empty Action is considered valid
 * for both.
 *
 * \see AnimData::action
 * \see AnimData::slot_handle
 */
class Action : public ::bAction {
 public:
  Action() = default;
  /**
   * Copy constructor is deleted, as code should use regular ID library
   * management functions to duplicate this data-block.
   */
  Action(const Action &other) = delete;

  /* Discriminators for 'legacy' and 'layered' Actions. */
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
   */
  bool is_action_legacy() const;
  /**
   * Return whether this is a layered Action.
   *
   * - Animation data is stored in `bAction::layer_array`.
   * - Evaluated for data-blocks based on their slot handle.
   *
   * \note An empty Action is valid as both a legacy and layered Action.
   */
  bool is_action_layered() const;

  /* Action Layers access. */
  blender::Span<const Layer *> layers() const;
  blender::MutableSpan<Layer *> layers();
  const Layer *layer(int64_t index) const;
  Layer *layer(int64_t index);

  Layer &layer_add(StringRefNull name);

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
  blender::MutableSpan<Slot *> slots();
  const Slot *slot(int64_t index) const;
  Slot *slot(int64_t index);

  /**
   * Return the Slot with the given handle.
   *
   * \param handle can be `Slot::unassigned`, in which case `nullptr` is returned.
   *
   * \return `nullptr` when the slot cannot be found, so either the handle was
   * `Slot::unassigned` or some value that does not match any Slot in this Action.
   */
  Slot *slot_for_handle(slot_handle_t handle);
  const Slot *slot_for_handle(slot_handle_t handle) const;

  /**
   * Set the slot name, ensure it is unique, and propagate the new name to
   * all data-blocks that use it.
   *
   * This has to be done on the Action level to ensure each slot has a
   * unique name within the Action.
   *
   * \note This does NOT ensure the first two characters match the ID type of
   * this slot. This is the caller's responsibility.
   *
   * \see Action::slot_name_define
   * \see Action::slot_name_propagate
   */
  void slot_name_set(Main &bmain, Slot &slot, StringRefNull new_name);

  /**
   * Set the slot name, and ensure it is unique.
   *
   * \note This does NOT ensure the first two characters match the ID type of
   * this slot. This is the caller's responsibility.
   *
   * \see Action::slot_name_set
   * \see Action::slot_name_propagate
   */
  void slot_name_define(Slot &slot, StringRefNull new_name);

  /**
   * Update the `AnimData::action_slot_name` field of any ID that is animated by
   * this Slot.
   *
   * Should be called after `slot_name_define(slot)`. This is implemented as a separate
   * function due to the need to access `bmain`, which is available in the RNA on-property-update
   * handler, but not in the RNA property setter.
   */
  void slot_name_propagate(Main &bmain, const Slot &slot);

  Slot *slot_find_by_name(StringRefNull slot_name);

  /**
   * Create a new, unused Slot.
   *
   * The returned slot will be suitable for any ID type. After slot to an
   * ID, it be limited to that ID's type.
   */
  Slot &slot_add();

  /**
   * Create a new slot, named after the given ID, and limited to the ID's type.
   *
   * Note that this assigns neither this Action nor the new Slot to the ID. This function
   * merely initializes the Slot itself to suitable values to start animating this ID.
   */
  Slot &slot_add_for_id(const ID &animated_id);

  /**
   * Ensure that an appropriate Slot exists for the given ID.
   *
   * If a suitable Slot can be found, that Slot is returned.  Otherwise,
   * one is created.
   *
   * This is essentially a wrapper for `find_suitable_slot_for()` and
   * `slot_add_for_id()`, and follows their semantics. Notably, like both of
   * those methods, this Action does not need to already be assigned to the ID.
   * And like `find_suitable_slot_for()`, if this Action *is* already
   * assigned to the ID with a valid Slot, that Slot is returned.
   *
   * Note that this assigns neither this Action nor the Slot to the ID. This
   * merely ensures that an appropriate Slot exists.
   *
   * \see `Action::find_suitable_slot_for()`
   * \see `Action::slot_add_for_id()`
   */
  Slot &slot_ensure_for_id(const ID &animated_id);

  /**
   * Set the active Slot, ensuring only one Slot is flagged as the Active one.
   *
   * \param slot_handle if Slot::unassigned, there will not be any active slot.
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

  /** Assign this Action to the ID.
   *
   * \param slot: The slot this ID should be animated by, may be nullptr if it is to be
   * assigned later. In that case, the ID will not actually receive any animation.
   * \param animated_id: The ID that should be animated by this Action.
   *
   * \return whether the assignment was successful.
   */
  bool assign_id(Slot *slot, ID &animated_id);

  /**
   * Unassign this Action from the animated ID.
   *
   * \param animated_id: ID that is animated by this Action. Calling this
   * function when this ID is _not_ animated by this Action is not allowed,
   * and considered a bug.
   */
  void unassign_id(ID &animated_id);

  /**
   * Find the slot that best matches the animated ID.
   *
   * If the ID is already animated by this Action, by matching this
   * Action's slots with (in order):
   *
   * - `animated_id.adt->slot_handle`,
   * - `animated_id.adt->slot_name`,
   * - `animated_id.name`.
   *
   * Note that this is different from #slot_for_id, which does not use the
   * slot name, and only works when this Action is already assigned. */
  Slot *find_suitable_slot_for(const ID &animated_id);

  /**
   * Return whether this Action actually has any animation data for the given slot.
   */
  bool is_slot_animated(slot_handle_t slot_handle) const;

  /**
   * Get the layer that should be used for user-level keyframe insertion.
   *
   * \return The layer, or nullptr if no layer exists that can currently be used
   * for keyframing (e.g. all layers are locked, once we've implemented
   * locking).
   */
  Layer *get_layer_for_keyframing();

 protected:
  /** Return the layer's index, or -1 if not found in this Action. */
  int64_t find_layer_index(const Layer &layer) const;

 private:
  Slot &slot_allocate();

  /**
   * Ensure the slot name prefix matches its ID type.
   *
   * This ensures that the first two characters match the ID type of
   * this slot.
   *
   * \see Action::slot_name_propagate
   */
  void slot_name_ensure_prefix(Slot &slot);

  /**
   * Set the slot's ID type to that of the animated ID, ensure the name
   * prefix is set accordingly, and that the name is unique within the
   * Action.
   *
   * \note This assumes that the slot has no ID type set yet. If it does, it
   * is considered a bug to call this function.
   */
  void slot_setup_for_id(Slot &slot, const ID &animated_id);
};
static_assert(sizeof(Action) == sizeof(::bAction),
              "DNA struct and its C++ wrapper must have the same size");

/**
 * Strips contain the actual animation data.
 *
 * Although the data model allows for different strip types, currently only a
 * single type is implemented: keyframe strips.
 */
class Strip : public ::ActionStrip {
 public:
  /**
   * Strip instances should not be created via this constructor. Create a sub-class like
   * #KeyframeStrip instead.
   *
   * The reason is that various functions will assume that the `Strip` is actually a down-cast
   * instance of another strip class, and that `Strip::type()` will say which type. To avoid having
   * to explicitly deal with an 'invalid' type everywhere, creating a `Strip` directly is simply
   * not allowed.
   */
  Strip() = delete;

  /**
   * Strip cannot be duplicated via the copy constructor. Either use a concrete
   * strip type's copy constructor, or use Strip::duplicate().
   *
   * The reason why the copy constructor won't work is due to the double nature
   * of the inheritance at play here:
   *
   * C-style inheritance: `KeyframeActionStrip` "inherits" `ActionStrip"
   *   by embedding the latter. This means that any `KeyframeActionStrip *`
   *   can be reinterpreted as `ActionStrip *`.
   *
   * C++-style inheritance: the C++ wrappers inherit the DNA structs, so
   *   `animrig::Strip` inherits `::ActionStrip`, and
   *   `animrig::KeyframeStrip` inherits `::KeyframeActionStrip`.
   */
  Strip(const Strip &other) = delete;
  ~Strip();

  Strip *duplicate(StringRefNull allocation_name) const;

  enum class Type : int8_t { Keyframe = 0 };

  /**
   * Strip type, so it's known which subclass this can be wrapped in without
   * having to rely on C++ RTTI.
   */
  Type type() const
  {
    return Type(this->strip_type);
  }

  template<typename T> bool is() const;
  template<typename T> T &as();
  template<typename T> const T &as() const;

  bool is_infinite() const;
  bool contains_frame(float frame_time) const;
  bool is_last_frame(float frame_time) const;

  /**
   * Set the start and end frame.
   *
   * Note that this does not do anything else. There is no check whether the
   * frame numbers are valid (i.e. frame_start <= frame_end). Infinite values
   * (negative for frame_start, positive for frame_end) are supported.
   */
  void resize(float frame_start, float frame_end);
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
 */
class Layer : public ::ActionLayer {
 public:
  Layer() = default;
  Layer(const Layer &other);
  ~Layer();

  enum class Flags : uint8_t {
    /* Set by default, cleared to mute. */
    Enabled = (1 << 0),
    /* When adding/removing a flag, also update the ENUM_OPERATORS() invocation below. */
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

  /* Strip access. */
  blender::Span<const Strip *> strips() const;
  blender::MutableSpan<Strip *> strips();
  const Strip *strip(int64_t index) const;
  Strip *strip(int64_t index);

  /**
   * Add a new Strip of the given type.
   *
   * \see strip_add<T>() for a templated version that returns the strip as its
   * concrete C++ type.
   */
  Strip &strip_add(Strip::Type strip_type);

  /**
   * Add a new strip of the type of T.
   *
   * T must be a concrete subclass of animrig::Strip.
   *
   * \see KeyframeStrip
   */
  template<typename T> T &strip_add()
  {
    Strip &strip = this->strip_add(T::TYPE);
    return strip.as<T>();
  }

  /**
   * Remove the strip from this layer.
   *
   * After this call, the passed reference is no longer valid, as the memory
   * will have been freed.
   *
   * \return true when the strip was found & removed, false if it wasn't found.
   */
  bool strip_remove(Strip &strip);

 protected:
  /** Return the strip's index, or -1 if not found in this layer. */
  int64_t find_strip_index(const Strip &strip) const;
};
static_assert(sizeof(Layer) == sizeof(::ActionLayer),
              "DNA struct and its C++ wrapper must have the same size");

ENUM_OPERATORS(Layer::Flags, Layer::Flags::Enabled);

/**
 * Identifier for a sub-set of the animation data inside an Action.
 *
 * An animatable ID specifies both an `Action*` and an `ActionSlot::handle`
 * to identify which F-Curves (and in the future other animation data) it will
 * be animated by.
 *
 * This is called a 'slot' because it binds the animatable ID to the sub-set
 * of animation data that should animate it.
 *
 * \see AnimData::slot_handle
 */
class Slot : public ::ActionSlot {
 public:
  Slot();
  Slot(const Slot &other);
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
   * Slot names consist of a two-character ID code, then the display name.
   * This means that the minimum length of a valid name is 3 characters.
   */
  constexpr static int name_length_min = 3;

  /**
   * Return the name prefix for the Slot's type.
   *
   * This is the ID name prefix, so "OB" for objects, "CA" for cameras, etc.
   */
  std::string name_prefix_for_idtype() const;

  /**
   * Return the name without the prefix, also known as the "display name".
   *
   * \see name_prefix_for_idtype
   */
  StringRefNull name_without_prefix() const;

  /** Return whether this Slot is usable by this ID type. */
  bool is_suitable_for(const ID &animated_id) const;

  /** Return whether this Slot has an `idtype` set. */
  bool has_idtype() const;

  /* Flags access. */
  enum class Flags : uint8_t {
    /** Expanded/collapsed in animation editors. */
    Expanded = (1 << 0),
    /** Selected in animation editors. */
    Selected = (1 << 1),
    /** The active Slot for this Action. Set via a method on the Action. */
    Active = (1 << 2),
    /* When adding/removing a flag, also update the ENUM_OPERATORS() invocation,
     * all the way below the Slot class. */
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
   * \see Slot::users(Main &bmain)
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
   * \see blender::animrig::internal::rebuild_slot_user_cache()
   */
  static void users_invalidate(Main &bmain);

 protected:
  friend Action;

  /**
   * Ensure the first two characters of the name match the ID type.
   *
   * \note This does NOT ensure name uniqueness within the Action. That is
   * the responsibility of the caller.
   */
  void name_ensure_prefix();

  /**
   * Set the 'Active' flag. Only allowed to be called by Action.
   */
  void set_active(bool active);
};
static_assert(sizeof(Slot) == sizeof(::ActionSlot),
              "DNA struct and its C++ wrapper must have the same size");
ENUM_OPERATORS(Slot::Flags, Slot::Flags::Active);

/**
 * KeyframeStrips effectively contain a bag of F-Curves for each Slot.
 */
class KeyframeStrip : public ::KeyframeActionStrip {
 public:
  /**
   * Low-level strip type.
   *
   * Do not use this in comparisons directly, use Strip::as<KeyframeStrip>() or
   * Strip::is<KeyframeStrip>() instead. This value is here only to make
   * functions like those easier to write.
   */
  static constexpr Strip::Type TYPE = Strip::Type::Keyframe;

  KeyframeStrip() = default;
  KeyframeStrip(const KeyframeStrip &other);
  ~KeyframeStrip();

  /** Implicitly convert a KeyframeStrip& to a Strip&. */
  operator Strip &();

  /* ChannelBag array access. */
  blender::Span<const ChannelBag *> channelbags() const;
  blender::MutableSpan<ChannelBag *> channelbags();
  const ChannelBag *channelbag(int64_t index) const;
  ChannelBag *channelbag(int64_t index);

  /**
   * Find the animation channels for this slot.
   *
   * \return nullptr if there is none yet for this slot.
   */
  const ChannelBag *channelbag_for_slot(const Slot &slot) const;
  ChannelBag *channelbag_for_slot(const Slot &slot);
  const ChannelBag *channelbag_for_slot(slot_handle_t slot_handle) const;
  ChannelBag *channelbag_for_slot(slot_handle_t slot_handle);

  /**
   * Add the animation channels for this slot.
   *
   * Should only be called when there is no `ChannelBag` for this slot yet.
   */
  ChannelBag &channelbag_for_slot_add(const Slot &slot);

  /**
   * Find the ChannelBag for `slot`, or if none exists, create it.
   */
  ChannelBag &channelbag_for_slot_ensure(const Slot &slot);

  /**
   * Remove the ChannelBag from this slot.
   *
   * After this call the reference is no longer valid, as the memory will have been freed.
   *
   * \return true when the ChannelBag was found & removed, false if it wasn't found.
   */
  bool channelbag_remove(ChannelBag &channelbag_to_remove);

  /** Return the channelbag's index, or -1 if there is none for this slot handle. */
  int64_t find_channelbag_index(const ChannelBag &channelbag) const;

  SingleKeyingResult keyframe_insert(Main *bmain,
                                     const Slot &slot,
                                     FCurveDescriptor fcurve_descriptor,
                                     float2 time_value,
                                     const KeyframeSettings &settings,
                                     eInsertKeyFlags insert_key_flags = INSERTKEY_NOFLAGS);
};
static_assert(sizeof(KeyframeStrip) == sizeof(::KeyframeActionStrip),
              "DNA struct and its C++ wrapper must have the same size");

template<> KeyframeStrip &Strip::as<KeyframeStrip>();
template<> const KeyframeStrip &Strip::as<KeyframeStrip>() const;

/**
 * Collection of F-Curves, intended for a specific Slot handle.
 */
class ChannelBag : public ::ActionChannelBag {
 public:
  ChannelBag() = default;
  ChannelBag(const ChannelBag &other);
  ~ChannelBag();

  /* FCurves access. */
  blender::Span<const FCurve *> fcurves() const;
  blender::MutableSpan<FCurve *> fcurves();
  const FCurve *fcurve(int64_t index) const;
  FCurve *fcurve(int64_t index);

  /**
   * Find an FCurve matching the fcurve descriptor.
   *
   * If it cannot be found, `nullptr` is returned.
   */
  const FCurve *fcurve_find(FCurveDescriptor fcurve_descriptor) const;
  FCurve *fcurve_find(FCurveDescriptor fcurve_descriptor);

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
  FCurve &fcurve_ensure(Main *bmain, FCurveDescriptor fcurve_descriptor);

  /**
   * Create an F-Curve, but only if it doesn't exist yet in this ChannelBag.
   *
   * \return the F-Curve it it was created, or nullptr if it already existed.
   *
   * \param bmain: Used to tag the dependency graph(s) for relationship
   * rebuilding. This is necessary when adding a new F-Curve, as a
   * previously-unanimated depsgraph component may become animated now. Can be
   * nullptr, in which case the tagging is skipped and is left as the
   * responsibility of the caller.
   */
  FCurve *fcurve_create_unique(Main *bmain, FCurveDescriptor fcurve_descriptor);

  /**
   * Remove an F-Curve from the ChannelBag.
   *
   * After this call, if the F-Curve was found, the reference will no longer be
   * valid, as the curve will have been freed.
   *
   * \return true when the F-Curve was found & removed, false if it wasn't found.
   */
  bool fcurve_remove(FCurve &fcurve_to_remove);

  /**
   * Remove all F-Curves from this ChannelBag.
   */
  void fcurves_clear();

 protected:
  /**
   * Create an F-Curve.
   *
   * Assumes that there is no such F-Curve yet on this ChannelBag. If it is
   * uncertain whether this is the case, use `fcurve_create_unique()` instead.
   *
   * \param bmain: Used to tag the dependency graph(s) for relationship
   * rebuilding. This is necessary when adding a new F-Curve, as a
   * previously-unanimated depsgraph component may become animated now. Can be
   * nullptr, in which case the tagging is skipped and is left as the
   * responsibility of the caller.
   */
  FCurve &fcurve_create(Main *bmain, FCurveDescriptor fcurve_descriptor);
};
static_assert(sizeof(ChannelBag) == sizeof(::ActionChannelBag),
              "DNA struct and its C++ wrapper must have the same size");

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
bool assign_action(Action &action, ID &animated_id);

/**
 * Return whether the given Action can be assigned to the ID.
 *
 * This always returns `true` for layered Actions. For legacy Actions it
 * returns `true` if the Action's `idroot` matches the ID.
 */
bool is_action_assignable_to(const bAction *dna_action, ID_Type id_code);

/**
 * Ensure that this ID is no longer animated.
 */
void unassign_action(ID &animated_id);

/**
 * Clear the Action slot of this ID.
 *
 * `adt.slot_handle_name` is updated to reflect the current name of the
 * slot, before un-assigning. This is to ensure that the stored name reflects
 * the actual slot that was used, making re-slot trivial.
 *
 * \param animated_id: the animated ID.
 *
 * \note this does not clear the Action pointer, just the slot handle.
 */
void unassign_slot(ID &animated_id);

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

/**
 * Return the F-Curves for this specific slot handle.
 *
 * This is just a utility function, that's intended to become obsolete when multi-layer Actions
 * are introduced. However, since Blender currently only supports a single layer with a single
 * strip, of a single type, this function can be used.
 *
 * The use of this function is also an indicator for code that will have to be altered when
 * multi-layered Actions are getting implemented.
 */
Span<FCurve *> fcurves_for_action_slot(Action &action, slot_handle_t slot_handle);
Span<const FCurve *> fcurves_for_action_slot(const Action &action, slot_handle_t slot_handle);

/**
 * Return all F-Curves in the Action.
 *
 * This works for both legacy and layered Actions.
 *
 * This is a utility function whose purpose is unclear after multi-layer Actions are introduced.
 * It might still be useful, it might not be.
 *
 * The use of this function is an indicator for code that might have to be altered when
 * multi-layered Actions are getting implemented.
 */
Vector<const FCurve *> fcurves_all(const Action &action);
Vector<FCurve *> fcurves_all(Action &action);

/**
 * Get (or add relevant data to be able to do so) an F-Curve from the given
 * Action. This assumes that all the destinations are valid.
 *
 * NOTE: this function is primarily intended for use with legacy actions, but
 * for reasons of expedience it now also works with layered actions under the
 * following limited circumstances: `ptr` must be non-null and must have an
 * `owner_id` that already uses `act`. Otherwise this function will return
 * nullptr for layered actions. See the comments in the implementation for more
 * details.
 *
 * \note This function also ensures that dependency graph relationships are
 * rebuilt. This is necessary when adding a new F-Curve, as a
 * previously-unanimated depsgraph component may become animated now.
 *
 * \param ptr: RNA pointer for the struct the fcurve is being looked up/created
 * for. For legacy actions this is optional and may be null.
 *
 * \param fcurve_descriptor: description of the fcurve to lookup/create. Note
 * that this is *not* relative to `ptr` (e.g. if `ptr` is not an ID). It should
 * contain the exact data path of the fcurve to be looked up/created.
 */
FCurve *action_fcurve_ensure(Main *bmain,
                             bAction *act,
                             const char group[],
                             PointerRNA *ptr,
                             FCurveDescriptor fcurve_descriptor);

/**
 * Find the F-Curve from the given Action. This assumes that all the destinations are valid.
 */
FCurve *action_fcurve_find(bAction *act, FCurveDescriptor fcurve_descriptor);

/**
 * Remove the given FCurve from the action by searching for it in all channelbags.
 * This assumes that an FCurve can only exist in an action once.
 *
 *  \returns true if the given FCurve was removed.
 */
bool action_fcurve_remove(Action &action, FCurve &fcu);

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
 * Deselect the keys of all actions in the Span. Duplicate entries are only visited once.
 */
void deselect_keys_actions(blender::Span<bAction *> actions);

/**
 * Deselect all keys within the action.
 */
void action_deselect_keys(Action &action);

}  // namespace blender::animrig

/* Wrap functions for the DNA structs. */

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

inline blender::animrig::KeyframeStrip &KeyframeActionStrip::wrap()
{
  return *reinterpret_cast<blender::animrig::KeyframeStrip *>(this);
}
inline const blender::animrig::KeyframeStrip &KeyframeActionStrip::wrap() const
{
  return *reinterpret_cast<const blender::animrig::KeyframeStrip *>(this);
}

inline blender::animrig::ChannelBag &ActionChannelBag::wrap()
{
  return *reinterpret_cast<blender::animrig::ChannelBag *>(this);
}
inline const blender::animrig::ChannelBag &ActionChannelBag::wrap() const
{
  return *reinterpret_cast<const blender::animrig::ChannelBag *>(this);
}
