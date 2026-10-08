/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#pragma once

#include <optional>

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"

#include "opentimelineio/anyDictionary.h"

namespace blender {
struct Scene;
struct Strip;
}  // namespace blender

namespace blender::io::otio {
using namespace opentimelineio::OPENTIMELINEIO_VERSION_NS;

enum class ResolveUnit {
  None,
  Percent,
  Degrees,
  SceneWidth,
  SceneHeight,
  MediaWidth,
  MediaHeight,
  FitWidth,
  FitHeight,
};

struct ResolveNumber {
  StringRefNull effect;
  StringRefNull parameter;
  StringRefNull rna_path;
  ResolveUnit unit;
};

Span<ResolveNumber> resolve_numbers();

struct ResolveUnits {
  float2 scene_size;
  float2 media_size;
  float2 fit_scale = float2(1.0f);

  ResolveUnits(const Scene &scene, const Strip &strip);

  double to_blender(ResolveUnit unit) const;
  float2 anchor_to_origin() const;
};

AnyDictionary &ensure_dictionary(AnyDictionary &parent, const std::string &key);

const char *resolve_blend_type(int64_t composite_mode);
std::optional<int64_t> resolve_composite_mode(StringRef blend_type);

}  // namespace blender::io::otio
