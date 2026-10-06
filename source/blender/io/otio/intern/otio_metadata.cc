/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#include "BKE_idprop.hh"

#include "BLI_array.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_sequence_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "opentimelineio/anyVector.h"
#include "opentimelineio/deserialization.h"

#include "otio_metadata.hh"

namespace blender::io::otio {

static bool skip_property(PointerRNA &ptr, PropertyRNA *prop)
{
  static const Set<StringRef> properties_stored_by_otio = {
      "name",
      "channel",
      "mute",
      "select",
      "select_left_handle",
      "select_right_handle",
      "left_handle",
      "right_handle",
      "left_handle_offset",
      "right_handle_offset",
      "content_start",
      "content_trim_start",
      "content_trim_end",
      "sound_offset",
      "duration",
      "filepath",
      "directory",
      "sound",
      "elements",
      "strips",
      "channels",
      "connections",
  };

  const StringRef identifier = RNA_property_identifier(prop);
  if (RNA_property_deprecated(prop)) {
    return true;
  }
  if (RNA_struct_is_a(ptr.type, RNA_Strip) && properties_stored_by_otio.contains(identifier)) {
    return true;
  }
  if (ELEM(RNA_property_type(prop), PROP_POINTER, PROP_COLLECTION)) {
    return false;
  }
  return !RNA_property_editable_flag(&ptr, prop) && identifier != "type";
}

static std::any pointer_value(PointerRNA &ptr)
{
  if (!ptr.data) {
    return {};
  }
  if (RNA_struct_is_ID(ptr.type)) {
    return std::string(static_cast<ID *>(ptr.data)->name + 2);
  }
  if (RNA_struct_is_a(ptr.type, RNA_Strip)) {
    return std::string(static_cast<Strip *>(ptr.data)->name + 2);
  }
  if (RNA_struct_is_a(ptr.type, RNA_ViewLayer)) {
    return std::string(static_cast<ViewLayer *>(ptr.data)->name);
  }
  return rna_to_dictionary(ptr);
}

static std::any property_value(PointerRNA &ptr, PropertyRNA *prop)
{
  const int length = RNA_property_array_length(&ptr, prop);

  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN: {
      if (length == 0) {
        return RNA_property_boolean_get(&ptr, prop);
      }
      Array<bool> values(length);
      RNA_property_boolean_get_array(&ptr, prop, values.data());
      return AnyVector(values.begin(), values.end());
    }
    case PROP_INT: {
      if (length == 0) {
        return int64_t(RNA_property_int_get(&ptr, prop));
      }
      Array<int> values(length);
      RNA_property_int_get_array(&ptr, prop, values.data());
      AnyVector vector;
      for (const int value : values) {
        vector.push_back(int64_t(value));
      }
      return vector;
    }
    case PROP_FLOAT: {
      if (length == 0) {
        return double(RNA_property_float_get(&ptr, prop));
      }
      Array<float> values(length);
      RNA_property_float_get_array(&ptr, prop, values.data());
      AnyVector vector;
      for (const float value : values) {
        vector.push_back(double(value));
      }
      return vector;
    }
    case PROP_STRING:
      return RNA_property_string_get(&ptr, prop);
    case PROP_ENUM: {
      const char *identifier;
      if (!RNA_property_enum_identifier(
              nullptr, &ptr, prop, RNA_property_enum_get(&ptr, prop), &identifier))
      {
        return {};
      }
      return std::string(identifier);
    }
    case PROP_POINTER: {
      PointerRNA pointer = RNA_property_pointer_get(&ptr, prop);
      return pointer_value(pointer);
    }
    case PROP_COLLECTION: {
      AnyVector vector;
      RNA_PROP_BEGIN (&ptr, item, prop) {
        std::any value = pointer_value(item);
        if (value.has_value()) {
          vector.push_back(value);
        }
      }
      RNA_PROP_END;
      return vector;
    }
  }
  return {};
}

AnyDictionary rna_to_dictionary(PointerRNA &ptr)
{
  AnyDictionary dictionary;
  RNA_STRUCT_BEGIN_SKIP_RNA_TYPE (&ptr, prop) {
    if (skip_property(ptr, prop)) {
      continue;
    }
    std::any value = property_value(ptr, prop);
    if (value.has_value()) {
      dictionary[RNA_property_identifier(prop)] = value;
    }
  }
  RNA_STRUCT_END;
  return dictionary;
}

void add_foreign_metadata(IDProperty *properties, SerializableObjectWithMetadata &object)
{
  IDProperty *otio_metadata = properties ? IDP_GetPropertyFromGroup(properties, "otio_metadata") :
                                           nullptr;
  if (!otio_metadata) {
    return;
  }

  /* Any data we imported from other video editors was converted to custom properties stored under
   * #otio_metadata. Here, on export, we convert those properties back to metadata. */
  IDP_foreach_property(otio_metadata, IDP_TYPE_FILTER_STRING, [&](IDProperty *prop) {
    std::any value;
    if (!STREQ(prop->name, "blender") &&
        deserialize_json_from_string(IDP_string_get(prop), &value, nullptr))
    {
      object.metadata()[prop->name] = value;
    }
  });
}

}  // namespace blender::io::otio
