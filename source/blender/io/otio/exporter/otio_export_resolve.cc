/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#include <cmath>

#include <fmt/format.h>

#include "BKE_fcurve.hh"

#include "BLI_function_ref.hh"
#include "BLI_math_color_c.hh"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "SEQ_connect.hh"
#include "SEQ_iterator.hh"
#include "SEQ_transform.hh"

#include "opentimelineio/anyVector.h"
#include "opentimelineio/effect.h"

#include "otio_export_resolve.hh"
#include "otio_resolve.hh"

namespace blender::io::otio {

static AnyDictionary &resolve_metadata(SerializableObjectWithMetadata &object)
{
  return ensure_dictionary(object.metadata(), "Resolve_OTIO");
}

static AnyDictionary resolve_parameter(const std::string &id,
                                       const std::any &value,
                                       const std::string &variant_type)
{
  AnyDictionary parameter;
  parameter["Parameter ID"] = id;
  parameter["Parameter Value"] = value;
  parameter["Variant Type"] = variant_type;
  return parameter;
}

static AnyDictionary resolve_effect(const std::string &effect_name,
                                    const int64_t type,
                                    const AnyVector &parameters,
                                    const std::string &name = "")
{
  AnyDictionary effect;
  effect["Effect Name"] = effect_name;
  effect["Enabled"] = true;
  effect["Name"] = name.empty() ? effect_name : name;
  effect["Parameters"] = parameters;
  effect["Type"] = type;
  return effect;
}

static double rna_number(PointerRNA &ptr, PropertyRNA *prop)
{
  switch (RNA_property_type(prop)) {
    case PROP_INT:
      return RNA_property_int_get(&ptr, prop);
    case PROP_FLOAT:
      return RNA_property_float_get(&ptr, prop);
    default:
      return 0.0;
  }
}

static AnyDictionary number_parameter(PointerRNA &strip_ptr,
                                      const Strip &strip,
                                      const std::string &id,
                                      const char *rna_path,
                                      const FunctionRef<double(double)> convert)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  double value = 0.0;
  AnyDictionary keyframes;
  if (RNA_path_resolve_property(&strip_ptr, rna_path, &ptr, &prop)) {
    value = rna_number(ptr, prop);
    const FCurve *fcurve = BKE_fcurve_find_by_rna(
        &ptr, prop, 0, nullptr, nullptr, nullptr, nullptr);
    if (fcurve && fcurve->bezt) {
      for (const BezTriple &bezt : Span(fcurve->bezt, fcurve->totvert)) {
        AnyDictionary keyframe;
        keyframe["Value"] = convert(bezt.vec[1][1]);
        keyframe["Variant Type"] = std::string("Double");
        keyframes[std::to_string(int(std::round(bezt.vec[1][0] - strip.left_handle())))] =
            keyframe;
      }
    }
  }
  AnyDictionary parameter = resolve_parameter(id, convert(value), "Double");
  parameter["Key Frames"] = keyframes;
  return parameter;
}

static AnyVector number_parameters(PointerRNA &strip_ptr,
                                   const Strip &strip,
                                   const ResolveUnits &units,
                                   const StringRef effect)
{
  AnyVector parameters;
  for (const ResolveNumber &number : resolve_numbers()) {
    if (number.effect == effect) {
      const double scale = units.to_blender(number.unit);
      parameters.push_back(number_parameter(
          strip_ptr, strip, number.parameter, number.rna_path.c_str(), [&](const double value) {
            return value / scale;
          }));
    }
  }
  return parameters;
}

static PointerRNA strip_pointer(const Scene &scene, const Strip &strip)
{
  return RNA_pointer_create_discrete(
      const_cast<ID *>(&scene.id), RNA_Strip, const_cast<Strip *>(&strip));
}

static AnyVector strip_effects(const Scene &scene, const Strip &strip)
{
  PointerRNA ptr = strip_pointer(scene, strip);
  const ResolveUnits units(scene, strip);

  if (strip.type == STRIP_TYPE_SOUND) {
    const auto to_decibels = [](const double gain) {
      return std::max(20.0 * std::log10(gain), -100.0);
    };
    return {
        resolve_effect("Fairlight Clip Volume and Fades",
                       62,
                       {number_parameter(ptr, strip, "volume", "volume", to_decibels)},
                       "Volume"),
        resolve_effect("Fairlight Clip Pan",
                       72,
                       number_parameters(ptr, strip, units, "Fairlight Clip Pan"),
                       "Pan"),
    };
  }

  AnyVector effects;
  if (strip.type == STRIP_TYPE_ADJUSTMENT) {
    effects.push_back(resolve_effect("Effect", 74, {}));
  }
  if (strip.data && strip.data->transform) {
    AnyVector parameters = number_parameters(ptr, strip, units, "Transform");
    const float2 anchor = (seq::image_transform_origin_get(&scene, &strip) - 0.5f) /
                          units.anchor_to_origin();
    parameters.push_back(resolve_parameter(
        "transformationAnchorPoint", AnyVector{double(anchor.x), double(anchor.y)}, "POINTF"));
    parameters.push_back(
        resolve_parameter("transformationFlipX", bool(strip.flag & SEQ_FLIPX), "Bool"));
    parameters.push_back(
        resolve_parameter("transformationFlipY", bool(strip.flag & SEQ_FLIPY), "Bool"));
    effects.push_back(resolve_effect("Transform", 2, parameters));
  }
  if (strip.data && strip.data->crop) {
    effects.push_back(
        resolve_effect("Cropping", 3, number_parameters(ptr, strip, units, "Cropping")));
  }

  AnyVector composite = number_parameters(ptr, strip, units, "Composite");
  const char *blend_type = "";
  RNA_property_enum_identifier(
      nullptr, &ptr, RNA_struct_find_property(&ptr, "blend_type"), strip.blend_mode, &blend_type);
  if (const std::optional<int64_t> mode = resolve_composite_mode(blend_type)) {
    composite.push_back(resolve_parameter("composite mode", *mode, "UInt"));
  }
  effects.push_back(resolve_effect("Composite", 1, composite));

  if (ELEM(strip.type, STRIP_TYPE_MOVIE, STRIP_TYPE_IMAGE)) {
    effects.push_back(resolve_effect(
        "Retime and Scaling", 22, {resolve_parameter("scale", int64_t(1), "UInt")}));
  }
  return effects;
}

static std::string color_hex(const float rgb[3])
{
  uchar color[3];
  rgb_float_to_uchar(color, rgb);
  return fmt::format("#{:02x}{:02x}{:02x}", color[0], color[1], color[2]);
}

static AnyDictionary title_parameter(const TextVars &data)
{
  std::string html = fmt::format(
      "<span style=\"font-size:{}pt; color:{};\">", data.text_size, color_hex(data.color));
  for (const char c : StringRef(data.text_ptr ? data.text_ptr : "")) {
    switch (c) {
      case '&':
        html += "&amp;";
        break;
      case '<':
        html += "&lt;";
        break;
      case '>':
        html += "&gt;";
        break;
      case '\n':
        html += "<br>";
        break;
      default:
        html += c;
    }
  }
  html += "</span>";

  AnyDictionary parameter;
  parameter["Parameter ID"] = std::string("title blob");
  parameter["Title HTML"] = html;
  return parameter;
}

GeneratorReference *resolve_generator_reference(const Strip &strip,
                                                const TimeRange &available_range)
{
  std::string kind;
  AnyDictionary effect;
  if (strip.type == STRIP_TYPE_COLOR) {
    const SolidColorVars *data = static_cast<const SolidColorVars *>(strip.effectdata);
    kind = "Solid Color";
    effect = resolve_effect("Solid Color",
                            5,
                            {resolve_parameter("color", color_hex(data->col), "Color")},
                            "Generator");
  }
  else if (strip.type == STRIP_TYPE_TEXT) {
    const TextVars *data = static_cast<const TextVars *>(strip.effectdata);
    kind = "Rich";
    effect = resolve_effect(
        "Rich Text",
        24,
        {title_parameter(*data),
         resolve_parameter(
             "position", AnyVector{double(data->loc[0]), double(data->loc[1])}, "POINTF")});
  }
  else {
    return nullptr;
  }

  AnyDictionary metadata;
  AnyDictionary generator_metadata;
  generator_metadata["Generator Type"] = kind;
  metadata["Resolve_OTIO"] = generator_metadata;
  AnyDictionary parameters;
  parameters["Resolve_OTIO"] = AnyVector{effect};
  return new GeneratorReference(strip.name + 2, kind, available_range, parameters, metadata);
}

/* Resolve only preserves links between video and audio of the same clip. Other linked
 * clips (e.g. several color generators) come back unlinked on import, even if Resolve exported
 * them itself. But still write every connection in case Resolve learns to read them later. */
Map<const Strip *, int64_t> resolve_link_groups(const ListBaseT<Strip> &seqbase)
{
  Map<const Strip *, int64_t> link_groups;
  int64_t link_group = 0;
  for (Strip *strip : seq::query_all_strips_recursive(&seqbase)) {
    if (!seq::is_strip_connected(strip) || link_groups.contains(strip)) {
      continue;
    }
    link_group++;
    link_groups.add(strip, link_group);
    for (Strip *connected : seq::connected_strips_get(strip)) {
      link_groups.add(connected, link_group);
    }
  }
  return link_groups;
}

void resolve_add_clip_data(const Scene &scene,
                           const Strip &strip,
                           const Map<const Strip *, int64_t> &link_groups,
                           Clip &clip)
{
  for (const std::any &effect : strip_effects(scene, strip)) {
    auto *otio_effect = new otio::Effect("", "Resolve Effect");
    otio_effect->metadata()["Resolve_OTIO"] = effect;
    clip.effects().push_back(otio_effect);
  }
  if (const int64_t *link_group = link_groups.lookup_ptr(&strip)) {
    resolve_metadata(clip)["Link Group ID"] = *link_group;
  }
}

void resolve_add_transition_data(const Scene &scene,
                                 const Strip &transition,
                                 Transition &otio_transition)
{
  if (transition.type != STRIP_TYPE_WIPE) {
    return;
  }
  PointerRNA ptr = strip_pointer(scene, transition);
  AnyDictionary &resolve = resolve_metadata(otio_transition);
  resolve["Effects"] = resolve_effect(
      "Edge Wipe",
      13,
      number_parameters(ptr, transition, ResolveUnits(scene, transition), "Edge Wipe"));
  resolve["Transition Type"] = std::string("Edge Wipe");
}

void resolve_add_track_data(const SeqTimelineChannel *channel, Track &track)
{
  resolve_metadata(track)["Locked"] = bool(channel && channel->is_locked());
}

void resolve_add_timeline_metadata(Timeline &timeline)
{
  resolve_metadata(timeline)["Resolve OTIO Meta Version"] = std::string("1.0");
}

}  // namespace blender::io::otio
