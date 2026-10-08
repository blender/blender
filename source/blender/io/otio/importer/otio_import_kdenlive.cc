/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#include "opentimelineio/clip.h"

#include "otio_import_kdenlive.hh"
#include "otio_metadata.hh"

namespace blender::io::otio {

std::optional<StripType> kdenlive_generator_type(const GeneratorReference &generator)
{
  if (generator.generator_kind() == "kdenlive:SolidColor") {
    return STRIP_TYPE_COLOR;
  }
  return std::nullopt;
}

AnyDictionary kdenlive_strip_properties(SerializableObjectWithMetadata &object)
{
  AnyDictionary properties;
  auto *clip = dynamic_cast<Clip *>(&object);
  auto *generator = clip ? dynamic_cast<GeneratorReference *>(clip->media_reference()) : nullptr;
  const AnyDictionary *kdenlive = generator ?
                                      find<AnyDictionary>(generator->parameters(), "kdenlive") :
                                      nullptr;
  const std::string *color = kdenlive ? find<std::string>(*kdenlive, "color") : nullptr;
  if (color && color->starts_with("0x")) {
    properties["color"] = hex_to_color(color->substr(2));
  }
  return properties;
}

}  // namespace blender::io::otio
