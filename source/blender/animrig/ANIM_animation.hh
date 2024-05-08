/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Animation data-block functionality.
 */
#pragma once

#include "ANIM_fcurve.hh"

#include "DNA_anim_types.h"

#include "BLI_math_vector.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

struct AnimationEvalContext;
struct FCurve;
struct ID;
struct Main;
struct PointerRNA;

namespace blender::animrig {

/* Forward declarations for the types defined later in this file. */
class Layer;
class Strip;
class Binding;

/* Use an alias for the Binding handle type to help disambiguate function parameters. */
using binding_handle_t = decltype(::AnimationBinding::handle);

/**
 * Container of animation data for one or more animated IDs.
 *
 * Broadly an Animation consists of Layers, each Layer has Strips, and it's the
 * Strips that eventually contain the animation data.
 *
 * Temporary limitation: each Animation can only contain one Layer.
 *
 * Which sub-set of that data drives the animation of which ID is determined by
 * which Binding is associated with that ID.
 *
 * \see AnimData::animation
 * \see AnimData::binding_handle
 */
class Animation : public ::Animation {
 public:
  Animation() = default;
  /**
   * Copy constructor is deleted, as code should use regular ID library
   * management functions to duplicate this data-block.
   */
  Animation(const Animation &other) = delete;

  /* Animation Layers access. */
  blender::Span<const Layer *> layers() const;
  blender::MutableSpan<Layer *> layers();
  const Layer *layer(int64_t index) const;
  Layer *layer(int64_t index);

  Layer &layer_add(StringRefNull name);

  /**
   * Remove the layer from this animation.
   *
   * After this call, the passed reference is no longer valid, as the memory
   * will have been freed. Any strips on the layer will be freed too.
   *
   * \return true when the layer was found & removed, false if it wasn't found.
   */
  bool layer_remove(Layer &layer_to_remove);

  /* Animation Binding access. */
  blender::Span<const Binding *> bindings() const;
  blender::MutableSpan<Binding *> bindings();
  const Binding *binding(int64_t index) const;
  Binding *binding(int64_t index);

  Binding *binding_for_handle(binding_handle_t handle);
  const Binding *binding_for_handle(binding_handle_t handle) const;

  /**
   * Set the binding name, ensure it is unique, and propagate the new name to
   * all data-blocks that use it.
   *
   * This has to be done on the Animation level to ensure each binding has a
   * unique name within the Animation.
   *
   * \note This does NOT ensure the first two characters match the ID type of
   * this binding. This is the caller's responsibility.
   *
   * \see Animation::binding_name_define
   * \see Animation::binding_name_propagate
   */
  void binding_name_set(Main &bmain, Binding &binding, StringRefNull new_name);

  /**
   * Set the binding name, and ensure it is unique.
   *
   * \note This does NOT ensure the first two characters match the ID type of
   * this binding. This is the caller's responsibility.
   *
   * \see Animation::binding_name_set
   * \see Animation::binding_name_propagate
   */
  void binding_name_define(Binding &binding, StringRefNull new_name);

  /**
   * Update the `AnimData::animation_binding_name` field of any ID that is animated by
   * this Binding.
   *
   * Should be called after `binding_name_define(binding)`. This is implemented as a separate
   * function due to the need to access `bmain`, which is available in the RNA on-property-update
   * handler, but not in the RNA property setter.
   */
  void binding_name_propagate(Main &bmain, const Binding &binding);

  Binding *binding_find_by_name(StringRefNull binding_name);

  Binding *binding_for_id(const ID &animated_id);
  const Binding *binding_for_id(const ID &animated_id) const;

  /**
   * Create a new, unused Binding.
   *
   * The returned binding will be suitable for any ID type. After binding to an
   * ID, it be limited to that ID's type.
   */
  Binding &binding_add();

  /**
   * Create a new binding, named after the given ID, and limited to the ID's type.
   *
   * Note that this assigns neither this Animation nor the new Binding to the ID. This function
   * merely initializes the Binding itself to suitable values to start animating this ID.
   */
  Binding &binding_add_for_id(const ID &animated_id);

  /** Assign this animation to the ID.
   *
   * \param binding: The binding this ID should be animated by, may be nullptr if it is to be
   * assigned later. In that case, the ID will not actually receive any animation.
   * \param animated_id: The ID that should be animated by this Animation data-block.
   *
   * \return whether the assignment was successful.
   */
  bool assign_id(Binding *binding, ID &animated_id);

  /**
   * Unassign this Animation from the animated ID.
   *
   * \param animated_id: ID that is animated by this Animation. Calling this
   * function when this ID is _not_ animated by this Animation is not allowed,
   * and considered a bug.
   */
  void unassign_id(ID &animated_id);

  /**
   * Find the binding that best matches the animated ID.
   *
   * If the ID is already animated by this Animation, by matching this
   * Animation's bindings with (in order):
   *
   * - `animated_id.adt->binding_handle`,
   * - `animated_id.adt->binding_name`,
   * - `animated_id.name`.
   *
   * Note that this is different from #binding_for_id, which does not use the
   * binding name, and only works when this Animation is already assigned. */
  Binding *find_suitable_binding_for(const ID &animated_id);

  /**
   * Return whether this Animation actually has any animation data for the given binding.
   */
  bool is_binding_animated(binding_handle_t binding_handle) const;

  /** Free all data in the `Animation`. Doesn't delete the `Animation` itself. */
  void free_data();

 protected:
  /** Return the layer's index, or -1 if not found in this animation. */
  int64_t find_layer_index(const Layer &layer) const;

 private:
  Binding &binding_allocate();

  /**
   * Ensure the binding name prefix matches its ID type.
   *
   * This ensures that the first two characters match the ID type of
   * this binding.
   *
   * \see Animation::binding_name_propagate
   */
  void binding_name_ensure_prefix(Binding &binding);

  /**
   * Set the binding's ID type to that of the animated ID, ensure the name
   * prefix is set accordingly, and that the name is unique within the
   * Animation.
   *
   * \note This assumes that the binding has no ID type set yet. If it does, it
   * is considered a bug to call this function.
   */
  void binding_setup_for_id(Binding &binding, const ID &animated_id);
};
static_assert(sizeof(Animation) == sizeof(::Animation),
              "DNA struct and its C++ wrapper must have the same size");

/**
 * Strips contain the actual animation data.
 *
 * Although the data model allows for different strip types, currently only a
 * single type is implemented: keyframe strips.
 */
class Strip : public ::AnimationStrip {
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
   * C-style inheritance: `KeyframeAnimationStrip` "inherits" `AnimationStrip"
   *   by embedding the latter. This means that any `KeyframeAnimationStrip *`
   *   can be reinterpreted as `AnimationStrip *`.
   *
   * C++-style inheritance: the C++ wrappers inherit the DNA structs, so
   *   `animrig::Strip` inherits `::AnimationStrip`, and
   *   `animrig::KeyframeStrip` inherits `::KeyframeAnimationStrip`.
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
static_assert(sizeof(Strip) == sizeof(::AnimationStrip),
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
class Layer : public ::AnimationLayer {
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
  Strip &strip_add(Strip::Type strip_type);

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
static_assert(sizeof(Layer) == sizeof(::AnimationLayer),
              "DNA struct and its C++ wrapper must have the same size");

ENUM_OPERATORS(Layer::Flags, Layer::Flags::Enabled);

/**
 * Identifier for a sub-set of the animation data inside an Animation data-block.
 *
 * An animatable ID specifies both an `Animation*` and an `AnimationBinding::handle`
 * to identify which F-Curves (and in the future other animation data) it will
 * be animated by.
 *
 * This is called a 'binding' because it binds the animatable ID to the sub-set
 * of animation data that should animate it.
 *
 * \see AnimData::binding_handle
 */
class Binding : public ::AnimationBinding {
 public:
  Binding() = default;
  Binding(const Binding &other) = default;
  ~Binding() = default;

  /**
   * Binding handle value indicating that there is no binding assigned.
   */
  constexpr static binding_handle_t unassigned = 0;

  /**
   * Binding names consist of a two-character ID code, then the display name.
   * This means that the minimum length of a valid name is 3 characters.
   */
  constexpr static int name_length_min = 3;

  /**
   * Return the name prefix for the Binding's type.
   *
   * This is the ID name prefix, so "OB" for objects, "CA" for cameras, etc.
   */
  std::string name_prefix_for_idtype() const;

  /**
   * Return the name without the prefix.
   *
   * \see name_prefix_for_idtype
   */
  StringRefNull name_without_prefix() const;

  /** Return whether this Binding is usable by this ID type. */
  bool is_suitable_for(const ID &animated_id) const;

  /** Return whether this Binding has an `idtype` set. */
  bool has_idtype() const;

 protected:
  friend Animation;

  /**
   * Ensure the first two characters of the name match the ID type.
   *
   * \note This does NOT ensure name uniqueness within the Animation. That is
   * the responsibility of the caller.
   */
  void name_ensure_prefix();
};
static_assert(sizeof(Binding) == sizeof(::AnimationBinding),
              "DNA struct and its C++ wrapper must have the same size");

/**
 * KeyframeStrips effectively contain a bag of F-Curves for each Binding.
 */
class KeyframeStrip : public ::KeyframeAnimationStrip {
 public:
  KeyframeStrip() = default;
  KeyframeStrip(const KeyframeStrip &other);
  ~KeyframeStrip();

  /* ChannelBag array access. */
  blender::Span<const ChannelBag *> channelbags() const;
  blender::MutableSpan<ChannelBag *> channelbags();
  const ChannelBag *channelbag(int64_t index) const;
  ChannelBag *channelbag(int64_t index);

  /**
   * Find the animation channels for this binding.
   *
   * \return nullptr if there is none yet for this binding.
   */
  const ChannelBag *channelbag_for_binding(const Binding &binding) const;
  ChannelBag *channelbag_for_binding(const Binding &binding);
  const ChannelBag *channelbag_for_binding(binding_handle_t binding_handle) const;
  ChannelBag *channelbag_for_binding(binding_handle_t binding_handle);

  /**
   * Add the animation channels for this binding.
   *
   * Should only be called when there is no `ChannelBag` for this binding yet.
   */
  ChannelBag &channelbag_for_binding_add(const Binding &binding);
  /**
   * Find an FCurve for this binding + RNA path + array index combination.
   *
   * If it cannot be found, `nullptr` is returned.
   */
  FCurve *fcurve_find(const Binding &binding, StringRefNull rna_path, int array_index);

  /**
   * Find an FCurve for this binding + RNA path + array index combination.
   *
   * If it cannot be found, a new one is created.
   */
  FCurve &fcurve_find_or_create(const Binding &binding, StringRefNull rna_path, int array_index);

  SingleKeyingResult keyframe_insert(const Binding &binding,
                                     StringRefNull rna_path,
                                     int array_index,
                                     float2 time_value,
                                     const KeyframeSettings &settings);
};
static_assert(sizeof(KeyframeStrip) == sizeof(::KeyframeAnimationStrip),
              "DNA struct and its C++ wrapper must have the same size");

template<> KeyframeStrip &Strip::as<KeyframeStrip>();
template<> const KeyframeStrip &Strip::as<KeyframeStrip>() const;

/**
 * Collection of F-Curves, intended for a specific Binding handle.
 */
class ChannelBag : public ::AnimationChannelBag {
 public:
  ChannelBag() = default;
  ChannelBag(const ChannelBag &other);
  ~ChannelBag();

  /* FCurves access. */
  blender::Span<const FCurve *> fcurves() const;
  blender::MutableSpan<FCurve *> fcurves();
  const FCurve *fcurve(int64_t index) const;
  FCurve *fcurve(int64_t index);

  const FCurve *fcurve_find(StringRefNull rna_path, int array_index) const;
};
static_assert(sizeof(ChannelBag) == sizeof(::AnimationChannelBag),
              "DNA struct and its C++ wrapper must have the same size");

/**
 * Assign the animation to the ID.
 *
 * This will will make a best-effort guess as to which binding to use, in this
 * order;
 *
 * - By binding handle.
 * - By fallback string.
 * - By the ID's name (matching against the binding name).
 * - If the above do not find a suitable binding, the animated ID will not
 *   receive any animation and the caller is responsible for creating a binding
 *   and assigning it.
 *
 * \return `false` if the assignment was not possible (for example the ID is of a type that cannot
 * be animated). If the above fall-through case of "no binding found" is reached, this function
 * will still return `true` as the Animation was successfully assigned.
 */
bool assign_animation(Animation &anim, ID &animated_id);

/**
 * Ensure that this ID is no longer animated.
 */
void unassign_animation(ID &animated_id);

/**
 * Clear the animation binding of this ID.
 *
 * `adt.binding_handle_name` is updated to reflect the current name of the
 * binding, before un-assigning. This is to ensure that the stored name reflects
 * the actual binding that was used, making re-binding trivial.
 *
 * \param adt the AnimData of the animated ID.
 *
 * \note this does not clear the Animation pointer, just the binding handle.
 */
void unassign_binding(AnimData &adt);

/**
 * Return the Animation of this ID, or nullptr if it has none.
 */
Animation *get_animation(ID &animated_id);

/**
 * Return the F-Curves for this specific binding handle.
 *
 * This is just a utility function, that's intended to become obsolete when multi-layer animation
 * is introduced. However, since Blender currently only supports a single layer with a single
 * strip, of a single type, this function can be used.
 *
 * The use of this function is also an indicator for code that will have to be altered when
 * multi-layered animation is getting implemented.
 */
Span<FCurve *> fcurves_for_animation(Animation &anim, binding_handle_t binding_handle);
Span<const FCurve *> fcurves_for_animation(const Animation &anim, binding_handle_t binding_handle);

}  // namespace blender::animrig

/* Wrap functions for the DNA structs. */

inline blender::animrig::Animation &Animation::wrap()
{
  return *reinterpret_cast<blender::animrig::Animation *>(this);
}
inline const blender::animrig::Animation &Animation::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Animation *>(this);
}

inline blender::animrig::Layer &AnimationLayer::wrap()
{
  return *reinterpret_cast<blender::animrig::Layer *>(this);
}
inline const blender::animrig::Layer &AnimationLayer::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Layer *>(this);
}

inline blender::animrig::Binding &AnimationBinding::wrap()
{
  return *reinterpret_cast<blender::animrig::Binding *>(this);
}
inline const blender::animrig::Binding &AnimationBinding::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Binding *>(this);
}

inline blender::animrig::Strip &AnimationStrip::wrap()
{
  return *reinterpret_cast<blender::animrig::Strip *>(this);
}
inline const blender::animrig::Strip &AnimationStrip::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Strip *>(this);
}

inline blender::animrig::KeyframeStrip &KeyframeAnimationStrip::wrap()
{
  return *reinterpret_cast<blender::animrig::KeyframeStrip *>(this);
}
inline const blender::animrig::KeyframeStrip &KeyframeAnimationStrip::wrap() const
{
  return *reinterpret_cast<const blender::animrig::KeyframeStrip *>(this);
}

inline blender::animrig::ChannelBag &AnimationChannelBag::wrap()
{
  return *reinterpret_cast<blender::animrig::ChannelBag *>(this);
}
inline const blender::animrig::ChannelBag &AnimationChannelBag::wrap() const
{
  return *reinterpret_cast<const blender::animrig::ChannelBag *>(this);
}
