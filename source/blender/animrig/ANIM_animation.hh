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
   * Set the binding name.
   *
   * This has to be done on the Animation level to ensure each binding has a
   * unique name within the Animation.
   *
   * \see Animation::binding_name_define
   * \see Animation::binding_name_propagate
   */
  void binding_name_set(Main &bmain, Binding &binding, StringRefNull new_name);

  /**
   * Set the binding name, and ensure it is unique.
   *
   * This function usually isn't necessary, call #binding_name_set instead.
   *
   * \see Animation::binding_name_set
   * \see Animation::binding_name_propagate
   */
  void binding_name_define(Binding &binding, StringRefNull new_name);

  /**
   * Update the `AnimData::animation_binding_name` field of any ID that is animated by
   * this.Binding.
   *
   * Should be called after `binding_name_define(binding)`. This is implemented as a separate
   * function due to the need to access bmain, which is available in the RNA on-property-update
   * handler, but not in the RNA property setter.
   */
  void binding_name_propagate(Main &bmain, const Binding &binding);

  Binding *binding_find_by_name(StringRefNull binding_name);

  Binding *binding_for_id(const ID &animated_id);
  const Binding *binding_for_id(const ID &animated_id) const;

  Binding &binding_add();

  /** Assign this animation to the ID.
   *
   * \param binding The binding this ID should be animated by, may be nullptr if it is to be
   * assigned later. In that case, the ID will not actually receive any animation.
   * \param animated_id The ID that should be animated by this Animation data-block.
   */
  bool assign_id(Binding *binding, ID &animated_id);

  /**
   * Unassign this Animation from the animated ID.
   *
   * \param animated_id ID that is animated by this Animation. Calling this
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
  Strip() = default;
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
   * Let the given ID receive animation from this binding.
   *
   * This is a low-level function; for most purposes you want
   * #Animation::assign_id instead.
   *
   * \note This does _not_ set animated_id->adt->animation to the owner of this
   * Binding. It's the caller's responsibility to do that.
   *
   * \return Whether this was possible. If the Binding was already bound to a
   * specific ID type, and `animated_id` is of a different type, it will be
   * refused. If the ID type cannot be animated at all, false is also returned.
   *
   * \see assign_animation
   * \see Animation::assign_id
   */
  bool connect_id(ID &animated_id);

  /** Return whether this Binding is usable by this ID type. */
  bool is_suitable_for(const ID &animated_id) const;
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
 * - By the ID's name (matching agains the binding name).
 * - If the above do not find a suitable binding, the animated ID will not
 *   receive any animation and the calller is responsible for creating a binding
 *   and assigning it.
 *
 * \return `false` if the assignment was not possible (for example the ID is of a type that cannot
 * be animated). If the above fall-through case of "no binding found" is reached, this function
 * will still return `true` as the Animation was succesfully assigned.
 */
bool assign_animation(Animation &anim, ID &animated_id);

/**
 * Ensure that this ID is no longer animated.
 */
void unassign_animation(ID &animated_id);

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
