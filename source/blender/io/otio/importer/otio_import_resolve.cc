/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#include <algorithm>
#include <cmath>

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_scene_types.h"

#include "SEQ_connect.hh"

#include "opentimelineio/anyVector.h"
#include "opentimelineio/clip.h"
#include "opentimelineio/effect.h"

#include "otio_import_resolve.hh"
#include "otio_metadata.hh"
#include "otio_resolve.hh"

namespace blender::io::otio {

static const AnyDictionary *resolve_metadata(SerializableObjectWithMetadata &object)
{
  return find<AnyDictionary>(object.metadata(), "Resolve_OTIO");
}

static const AnyDictionary *metadata_effect(SerializableObjectWithMetadata &object)
{
  const AnyDictionary *resolve = resolve_metadata(object);
  return resolve ? find<AnyDictionary>(*resolve, "Effects") : nullptr;
}

static const AnyDictionary *find_parameter(const AnyDictionary &effect, const std::string &id)
{
  const AnyVector *parameters = find<AnyVector>(effect, "Parameters");
  if (!parameters) {
    return nullptr;
  }
  for (const std::any &item : *parameters) {
    const AnyDictionary *parameter = std::any_cast<AnyDictionary>(&item);
    const std::string *name = parameter ? find<std::string>(*parameter, "Parameter ID") : nullptr;
    if (name && *name == id) {
      return parameter;
    }
  }
  return nullptr;
}

static const std::any *parameter(const AnyDictionary &effect, const std::string &id)
{
  const AnyDictionary *parameter = find_parameter(effect, id);
  return parameter ? find(*parameter, "Parameter Value") : nullptr;
}

static std::optional<double> number(const std::any *value)
{
  return value ? to_number(*value) : std::nullopt;
}

static Vector<double2> parameter_keyframes(const AnyDictionary &effect,
                                           const std::string &id,
                                           const int start_frame,
                                           const FunctionRef<double(double)> convert)
{
  const AnyDictionary *parameter = find_parameter(effect, id);
  const AnyDictionary *keys = parameter ? find<AnyDictionary>(*parameter, "Key Frames") : nullptr;
  Vector<double2> keyframes;
  if (!keys) {
    return keyframes;
  }
  for (const auto &[frame, key] : *keys) {
    const AnyDictionary *keyframe = std::any_cast<AnyDictionary>(&key);
    if (const std::optional<double> value = number(keyframe ? find(*keyframe, "Value") : nullptr))
    {
      keyframes.append(
          double2(start_frame + std::strtod(frame.c_str(), nullptr), convert(*value)));
    }
  }
  std::sort(keyframes.begin(), keyframes.end(), [](const double2 &a, const double2 &b) {
    return a.x < b.x;
  });
  return keyframes;
}

static std::any keyframes_value(const Span<double2> keyframes)
{
  AnyVector values;
  for (const double2 &keyframe : keyframes) {
    values.push_back(AnyVector{keyframe.x, keyframe.y});
  }
  AnyDictionary animation;
  animation["keyframes"] = values;
  return animation;
}

static std::optional<std::any> animated_number(const AnyDictionary &effect,
                                               const std::string &id,
                                               const int start_frame,
                                               const FunctionRef<double(double)> convert)
{
  const Vector<double2> keyframes = parameter_keyframes(effect, id, start_frame, convert);
  if (!keyframes.is_empty()) {
    return keyframes_value(keyframes);
  }
  const std::optional<double> value = number(parameter(effect, id));
  return value ? std::make_optional<std::any>(convert(*value)) : std::nullopt;
}

static double value_at(const Span<double2> keyframes, const double frame)
{
  for (const int i : keyframes.index_range().drop_front(1)) {
    if (frame < keyframes[i].x) {
      const double2 &a = keyframes[i - 1];
      const double2 &b = keyframes[i];
      return math::interpolate(a.y, b.y, std::clamp((frame - a.x) / (b.x - a.x), 0.0, 1.0));
    }
  }
  return keyframes.last().y;
}

static Vector<double2> add_fades(const Span<double2> keyframes,
                                 const double start_frame,
                                 const double end_frame,
                                 const double fade_in,
                                 const double fade_out)
{
  const double fade_in_end = start_frame + fade_in;
  const double fade_out_start = end_frame - fade_out;
  Vector<double2> faded;
  if (fade_in > 0.0) {
    faded.append(double2(start_frame, 0.0));
  }
  faded.append(double2(fade_in_end, value_at(keyframes, fade_in_end)));
  for (const double2 &keyframe : keyframes) {
    if (keyframe.x > fade_in_end && keyframe.x < fade_out_start) {
      faded.append(keyframe);
    }
  }
  faded.append(double2(fade_out_start, value_at(keyframes, fade_out_start)));
  if (fade_out > 0.0) {
    faded.append(double2(end_frame, 0.0));
  }
  return faded;
}

static void copy_number(AnyDictionary &properties,
                        const StringRef rna_path,
                        const AnyDictionary &effect,
                        const std::string &id,
                        const int start_frame,
                        const double factor)
{
  std::optional<std::any> value = animated_number(
      effect, id, start_frame, [&](const double value) { return value * factor; });
  if (!value) {
    return;
  }
  const int64_t dot = rna_path.find('.');
  if (dot == StringRef::not_found) {
    properties[rna_path] = *value;
    return;
  }
  ensure_dictionary(properties, rna_path.substr(0, dot))[rna_path.substr(dot + 1)] = *value;
}

static void add_effect_properties(const AnyDictionary &effect,
                                  const Scene &scene,
                                  const Strip &strip,
                                  AnyDictionary &properties)
{
  const std::string *name = find<std::string>(effect, "Effect Name");
  const bool *enabled = find<bool>(effect, "Enabled");
  if (!name || (enabled && !*enabled)) {
    return;
  }

  ResolveUnits units(scene, strip);
  if (strip.data && strip.data->transform) {
    units.fit_scale = float2(strip.data->transform->scale_x, strip.data->transform->scale_y);
  }
  const int start_frame = strip.left_handle();

  for (const ResolveNumber &number : resolve_numbers()) {
    if (number.effect == *name) {
      copy_number(properties,
                  number.rna_path,
                  effect,
                  number.parameter,
                  start_frame,
                  units.to_blender(number.unit));
    }
  }

  if (*name == "Transform") {
    const AnyVector *anchor = std::any_cast<AnyVector>(
        parameter(effect, "transformationAnchorPoint"));
    if (anchor && anchor->size() == 2) {
      const float2 offset(to_number((*anchor)[0]).value_or(0.0),
                          to_number((*anchor)[1]).value_or(0.0));
      const float2 origin = 0.5f + offset * units.anchor_to_origin();
      ensure_dictionary(properties, "transform")["origin"] = AnyVector{double(origin.x),
                                                                       double(origin.y)};
    }
    if (const std::any *flip_x = parameter(effect, "transformationFlipX")) {
      properties["use_flip_x"] = *flip_x;
    }
    if (const std::any *flip_y = parameter(effect, "transformationFlipY")) {
      properties["use_flip_y"] = *flip_y;
    }
  }
  else if (*name == "Composite") {
    const std::any *mode = parameter(effect, "composite mode");
    const int64_t *mode_value = mode ? std::any_cast<int64_t>(mode) : nullptr;
    if (const char *blend_type = mode_value ? resolve_blend_type(*mode_value) : nullptr) {
      properties["blend_type"] = std::string(blend_type);
    }
  }
  else if (*name == "Fairlight Clip Volume and Fades") {
    const auto to_gain = [](const double decibels) { return std::pow(10.0, decibels / 20.0); };
    Vector<double2> volume = parameter_keyframes(effect, "volume", start_frame, to_gain);
    if (volume.is_empty()) {
      volume.append(
          double2(start_frame, to_gain(number(parameter(effect, "volume")).value_or(0.0))));
    }
    const double fade_in = number(parameter(effect, "faderIn")).value_or(0.0);
    const double fade_out = number(parameter(effect, "faderOut")).value_or(0.0);
    if (fade_in > 0.0 || fade_out > 0.0) {
      volume = add_fades(volume, start_frame, strip.right_handle(&scene), fade_in, fade_out);
    }
    properties["volume"] = volume.size() == 1 ? std::any(volume[0].y) : keyframes_value(volume);
  }
  else if (*name == "Solid Color") {
    const std::any *color = parameter(effect, "color");
    if (const std::string *hex = color ? std::any_cast<std::string>(color) : nullptr) {
      properties["color"] = hex_to_color(*hex);
    }
  }
  else if (*name == "Rich Text") {
    if (const std::any *text = parameter(effect, "rich text")) {
      properties["text"] = *text;
    }
    if (const std::any *position = parameter(effect, "position")) {
      properties["location"] = *position;
    }
  }
}

std::optional<StripType> resolve_generator_type(const GeneratorReference &generator)
{
  const std::string &kind = generator.generator_kind();
  if (kind == "Solid Color") {
    return STRIP_TYPE_COLOR;
  }
  if (kind == "Text" || StringRef(kind).startswith("Rich")) {
    return STRIP_TYPE_TEXT;
  }
  return std::nullopt;
}

static Vector<const AnyDictionary *> resolve_effects(SerializableObjectWithMetadata &object)
{
  Vector<const AnyDictionary *> effects;
  if (auto *item = dynamic_cast<Item *>(&object)) {
    for (const SerializableObject::Retainer<otio::Effect> &effect : item->effects()) {
      if (const AnyDictionary *resolve = resolve_metadata(*effect)) {
        effects.append(resolve);
      }
    }
  }
  if (auto *clip = dynamic_cast<Clip *>(&object)) {
    if (auto *generator = dynamic_cast<GeneratorReference *>(clip->media_reference())) {
      if (const AnyVector *list = find<AnyVector>(generator->parameters(), "Resolve_OTIO")) {
        for (const std::any &item : *list) {
          if (const AnyDictionary *effect = std::any_cast<AnyDictionary>(&item)) {
            effects.append(effect);
          }
        }
      }
    }
  }
  if (const AnyDictionary *effect = metadata_effect(object)) {
    effects.append(effect);
  }
  return effects;
}

bool resolve_is_adjustment_clip(Clip &clip)
{
  for (const AnyDictionary *effect : resolve_effects(clip)) {
    const int64_t *type = find<int64_t>(*effect, "Type");
    if (type && *type == 74) {
      return true;
    }
  }
  return false;
}

std::optional<StripType> resolve_transition_type(Transition &transition)
{
  const AnyDictionary *effect = metadata_effect(transition);
  const std::string *name = effect ? find<std::string>(*effect, "Effect Name") : nullptr;
  if (name && *name == "Edge Wipe") {
    return STRIP_TYPE_WIPE;
  }
  return std::nullopt;
}

std::optional<eSeqImageFitMethod> resolve_fit_method(Item &item)
{
  for (const AnyDictionary *effect : resolve_effects(item)) {
    const std::string *name = find<std::string>(*effect, "Effect Name");
    if (!name || *name != "Retime and Scaling") {
      continue;
    }
    switch (int(number(parameter(*effect, "scale")).value_or(0.0))) {
      case 1:
        return SEQ_USE_ORIGINAL_SIZE;
      case 2:
        return SEQ_SCALE_TO_FIT;
      case 3:
        return SEQ_SCALE_TO_FILL;
      case 4:
        return SEQ_STRETCH_TO_FILL;
    }
  }
  return std::nullopt;
}

AnyDictionary resolve_strip_properties(const Scene &scene,
                                       const Strip &strip,
                                       SerializableObjectWithMetadata &object)
{
  AnyDictionary properties;
  for (const AnyDictionary *effect : resolve_effects(object)) {
    add_effect_properties(*effect, scene, strip, properties);
  }
  return properties;
}

bool resolve_track_locked(Track &track)
{
  const AnyDictionary *resolve = resolve_metadata(track);
  const bool *locked = resolve ? find<bool>(*resolve, "Locked") : nullptr;
  return locked && *locked;
}

void resolve_connect_strips(
    const Span<std::pair<Strip *, SerializableObjectWithMetadata *>> strips)
{
  Map<int64_t, VectorSet<Strip *>> link_groups;
  for (const auto &[strip, object] : strips) {
    const AnyDictionary *resolve = resolve_metadata(*object);
    if (const int64_t *link_group = resolve ? find<int64_t>(*resolve, "Link Group ID") : nullptr) {
      link_groups.lookup_or_add_default(*link_group).add(strip);
    }
  }
  for (VectorSet<Strip *> &link_group : link_groups.values()) {
    seq::connect(link_group);
  }
}

}  // namespace blender::io::otio
