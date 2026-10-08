/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#include "BLI_index_range.hh"
#include "BLI_math_constants.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "otio_resolve.hh"

namespace blender::io::otio {

Span<ResolveNumber> resolve_numbers()
{
  static const ResolveNumber numbers[] = {
      {"Transform", "transformationZoomX", "transform.scale_x", ResolveUnit::FitWidth},
      {"Transform", "transformationZoomY", "transform.scale_y", ResolveUnit::FitHeight},
      {"Transform", "transformationPan", "transform.offset_x", ResolveUnit::SceneWidth},
      {"Transform", "transformationTilt", "transform.offset_y", ResolveUnit::SceneHeight},
      {"Transform", "transformationRotationAngle", "transform.rotation", ResolveUnit::Degrees},
      {"Cropping", "cropLeft", "crop.min_x", ResolveUnit::MediaWidth},
      {"Cropping", "cropRight", "crop.max_x", ResolveUnit::MediaWidth},
      {"Cropping", "cropBottom", "crop.min_y", ResolveUnit::MediaHeight},
      {"Cropping", "cropTop", "crop.max_y", ResolveUnit::MediaHeight},
      {"Composite", "opacity", "blend_alpha", ResolveUnit::Percent},
      {"Fairlight Clip Pan", "pan", "pan", ResolveUnit::Percent},
      {"Edge Wipe", "angle", "angle", ResolveUnit::Degrees},
  };
  return Span(numbers, std::size(numbers));
}

ResolveUnits::ResolveUnits(const Scene &scene, const Strip &strip)
    : scene_size(scene.r.xsch, scene.r.ysch), media_size(scene_size)
{
  if (strip.data && strip.data->stripdata && strip.data->stripdata->orig_width > 0) {
    media_size = float2(strip.data->stripdata->orig_width, strip.data->stripdata->orig_height);
  }
}

double ResolveUnits::to_blender(const ResolveUnit unit) const
{
  switch (unit) {
    case ResolveUnit::None:
      return 1.0;
    case ResolveUnit::Percent:
      return 0.01;
    case ResolveUnit::Degrees:
      return DEG2RAD(1.0);
    case ResolveUnit::SceneWidth:
      return scene_size.x;
    case ResolveUnit::SceneHeight:
      return scene_size.y;
    case ResolveUnit::MediaWidth:
      return media_size.x;
    case ResolveUnit::MediaHeight:
      return media_size.y;
    case ResolveUnit::FitWidth:
      return fit_scale.x;
    case ResolveUnit::FitHeight:
      return fit_scale.y;
  }
  return 1.0;
}

float2 ResolveUnits::anchor_to_origin() const
{
  return scene_size.y / (media_size * fit_scale);
}

AnyDictionary &ensure_dictionary(AnyDictionary &parent, const std::string &key)
{
  std::any &value = parent[key];
  if (!std::any_cast<AnyDictionary>(&value)) {
    value = AnyDictionary();
  }
  return *std::any_cast<AnyDictionary>(&value);
}

static const char *composite_mode_blend_types[] = {
    "ALPHA_OVER", "ADD",        "SUBTRACT",    "DIFFERENCE",   "MULTIPLY",    "SCREEN",
    "OVERLAY",    "HARD_LIGHT", "SOFT_LIGHT",  "DARKEN",       "LIGHTEN",     "DODGE",
    "BURN",       "EXCLUSION",  "HUE",         "SATURATION",   "COLOR",       "VALUE",
    nullptr,      "ADD",        "LINEAR_BURN", "LINEAR_LIGHT", "VIVID_LIGHT", "PIN_LIGHT",
};

const char *resolve_blend_type(const int64_t composite_mode)
{
  return composite_mode >= 0 && composite_mode < int64_t(std::size(composite_mode_blend_types)) ?
             composite_mode_blend_types[composite_mode] :
             nullptr;
}

std::optional<int64_t> resolve_composite_mode(const StringRef blend_type)
{
  for (const int64_t mode : IndexRange(std::size(composite_mode_blend_types))) {
    if (composite_mode_blend_types[mode] && blend_type == composite_mode_blend_types[mode]) {
      return mode;
    }
  }
  return std::nullopt;
}

}  // namespace blender::io::otio
