/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#pragma once

#include "opentimelineio/serializableObjectWithMetadata.h"

namespace blender {
struct Editing;
struct IDProperty;
struct Main;
struct PointerRNA;
}  // namespace blender

namespace blender::io::otio {
using namespace opentimelineio::OPENTIMELINEIO_VERSION_NS;

std::optional<double> to_number(const std::any &value);
const std::any *find(const AnyDictionary &dictionary, const std::string &key);
template<typename T> const T *find(const AnyDictionary &dictionary, const std::string &key)
{
  const std::any *value = find(dictionary, key);
  return value ? std::any_cast<T>(value) : nullptr;
}
AnyVector hex_to_color(const std::string &hex);

AnyDictionary rna_to_dictionary(PointerRNA &ptr);
void rna_from_dictionary(Main *bmain,
                         Editing *editing,
                         const AnyDictionary &dictionary,
                         PointerRNA &ptr);

void add_foreign_metadata(IDProperty *properties, SerializableObjectWithMetadata &object);
void store_foreign_metadata(const SerializableObjectWithMetadata &object, IDProperty *&properties);

}  // namespace blender::io::otio
