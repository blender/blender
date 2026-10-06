/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#include "MEM_guardedalloc.h"

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"
#include "ANIM_fcurve.hh"

#include "BKE_colortools.hh"
#include "BKE_idprop.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_math_color_c.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

#include "DNA_ID.h"
#include "DNA_color_types.h"
#include "DNA_layer_types.h"
#include "DNA_sequence_types.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "SEQ_modifier.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_sound.hh"

#include "opentimelineio/anyVector.h"
#include "opentimelineio/deserialization.h"
#include "opentimelineio/serialization.h"

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

std::optional<double> to_number(const std::any &value)
{
  if (const double *number = std::any_cast<double>(&value)) {
    return *number;
  }
  if (const int64_t *number = std::any_cast<int64_t>(&value)) {
    return double(*number);
  }
  if (const bool *number = std::any_cast<bool>(&value)) {
    return double(*number);
  }
  return std::nullopt;
}

const std::any *find(const AnyDictionary &dictionary, const std::string &key)
{
  auto it = dictionary.find(key);
  return it != dictionary.end() ? &it->second : nullptr;
}

AnyVector hex_to_color(const std::string &hex)
{
  float3 color(0.0f);
  hex_to_rgb(hex.c_str(), &color.x, &color.y, &color.z);
  return AnyVector{double(color.x), double(color.y), double(color.z)};
}

static void set_number(PointerRNA &ptr, PropertyRNA *prop, const int index, const double value)
{
  const bool is_array = index >= 0;
  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN:
      is_array ? RNA_property_boolean_set_index(&ptr, prop, index, value != 0.0) :
                 RNA_property_boolean_set(&ptr, prop, value != 0.0);
      break;
    case PROP_INT:
      is_array ? RNA_property_int_set_index(&ptr, prop, index, int(value)) :
                 RNA_property_int_set(&ptr, prop, int(value));
      break;
    case PROP_FLOAT:
      is_array ? RNA_property_float_set_index(&ptr, prop, index, float(value)) :
                 RNA_property_float_set(&ptr, prop, float(value));
      break;
    default:
      break;
  }
}

static void insert_keyframes(Main *bmain,
                             PointerRNA &ptr,
                             PropertyRNA *prop,
                             const AnyVector &keyframes)
{
  const std::optional<std::string> path = RNA_path_from_ID_to_property(&ptr, prop);
  if (!path || RNA_property_array_check(prop)) {
    return;
  }
  bAction *action = animrig::id_action_ensure(bmain, ptr.owner_id);
  if (!action) {
    return;
  }
  FCurve &fcurve = animrig::action_fcurve_ensure(bmain, *action, *ptr.owner_id, {*path, 0});
  animrig::KeyframeSettings settings = animrig::get_keyframe_settings(false);
  settings.interpolation = BEZT_IPO_LIN;
  for (const std::any &item : keyframes) {
    const AnyVector *keyframe = std::any_cast<AnyVector>(&item);
    if (!keyframe || keyframe->size() != 2) {
      continue;
    }
    const std::optional<double> frame = to_number((*keyframe)[0]);
    const std::optional<double> value = to_number((*keyframe)[1]);
    if (frame && value) {
      animrig::insert_vert_fcurve(&fcurve, float2(*frame, *value), settings, INSERTKEY_NOFLAGS);
    }
  }
}

static PointerRNA find_by_name(
    Main *bmain, Editing *editing, PointerRNA &owner, StructRNA *type, const std::string &name)
{
  if (RNA_struct_is_ID(type)) {
    ID *id = BKE_libblock_find_name(bmain, RNA_type_to_ID_code(type), name.c_str());
    return RNA_id_pointer_create(id);
  }
  if (RNA_struct_is_a(type, RNA_Strip) && editing) {
    Strip *strip = seq::lookup_strip_by_name(editing, name.c_str());
    return RNA_pointer_create_discrete(owner.owner_id, RNA_Strip, strip);
  }
  if (RNA_struct_is_a(type, RNA_ViewLayer) && RNA_struct_is_a(owner.type, RNA_SceneStrip)) {
    Scene *scene = static_cast<Strip *>(owner.data)->scene;
    if (ViewLayer *view_layer = scene ? BKE_view_layer_find(scene, name.c_str()) : nullptr) {
      return RNA_pointer_create_id_subdata(scene->id, RNA_ViewLayer, view_layer);
    }
  }
  return {};
}

static void add_modifiers(Main *bmain, Editing *editing, PointerRNA &ptr, const AnyVector &items)
{
  Strip *strip = static_cast<Strip *>(ptr.data);
  for (const std::any &item : items) {
    const AnyDictionary *dictionary = std::any_cast<AnyDictionary>(&item);
    const std::string *type_name = dictionary && dictionary->has_key("type") ?
                                       std::any_cast<std::string>(&dictionary->at("type")) :
                                       nullptr;
    int type;
    if (!type_name ||
        !RNA_enum_value_from_id(rna_enum_strip_modifier_type_items, type_name->c_str(), &type))
    {
      continue;
    }
    StripModifierData *smd = seq::modifier_new(strip, nullptr, eStripModifierType(type));
    seq::modifier_persistent_uid_init(*strip, *smd);
    PointerRNA modifier_ptr = RNA_pointer_create_discrete(ptr.owner_id, RNA_StripModifier, smd);
    rna_from_dictionary(bmain, editing, *dictionary, modifier_ptr);
  }
}

static void resize_collection(PointerRNA &ptr, const StringRef name, const int size)
{
  if (RNA_struct_is_a(ptr.type, RNA_CurveMap) && name == "points") {
    CurveMap *curve_map = static_cast<CurveMap *>(ptr.data);
    MEM_SAFE_DELETE(curve_map->curve);
    curve_map->curve = MEM_new_array<CurveMapPoint>(size, __func__);
    curve_map->totpoint = size;
  }
  else if (RNA_struct_is_a(ptr.type, RNA_SoundEqualizerModifier) && name == "graphics") {
    StripModifierData *smd = static_cast<StripModifierData *>(ptr.data);
    seq::sound_equalizermodifier_free(smd);
    for ([[maybe_unused]] const int i : IndexRange(size)) {
      seq::sound_equalizermodifier_add_graph(reinterpret_cast<SoundEqualizerModifierData *>(smd),
                                             SOUND_EQUALIZER_DEFAULT_MIN_FREQ,
                                             SOUND_EQUALIZER_DEFAULT_MAX_FREQ);
    }
  }
}

static float *curve_mapping_clip(PointerRNA &ptr, const StringRef name)
{
  if (!RNA_struct_is_a(ptr.type, RNA_CurveMapping)) {
    return nullptr;
  }
  rctf &clip = static_cast<CurveMapping *>(ptr.data)->clipr;
  if (name == "clip_min_x") {
    return &clip.xmin;
  }
  if (name == "clip_max_x") {
    return &clip.xmax;
  }
  if (name == "clip_min_y") {
    return &clip.ymin;
  }
  if (name == "clip_max_y") {
    return &clip.ymax;
  }
  return nullptr;
}

static void set_property(
    Main *bmain, Editing *editing, PointerRNA &ptr, PropertyRNA *prop, const std::any &value)
{
  const StringRef name = RNA_property_identifier(prop);

  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN:
    case PROP_INT:
    case PROP_FLOAT: {
      if (float *clip = curve_mapping_clip(ptr, name)) {
        if (const std::optional<double> number = to_number(value)) {
          *clip = float(*number);
        }
      }
      else if (const AnyDictionary *animation = std::any_cast<AnyDictionary>(&value)) {
        if (const AnyVector *keyframes = find<AnyVector>(*animation, "keyframes")) {
          insert_keyframes(bmain, ptr, prop, *keyframes);
        }
      }
      else if (const AnyVector *values = std::any_cast<AnyVector>(&value)) {
        const int length = std::min<int>(RNA_property_array_length(&ptr, prop), values->size());
        for (const int i : IndexRange(length)) {
          if (const std::optional<double> number = to_number((*values)[i])) {
            set_number(ptr, prop, i, *number);
          }
        }
      }
      else if (const std::optional<double> number = to_number(value)) {
        set_number(ptr, prop, -1, *number);
      }
      break;
    }
    case PROP_STRING:
      if (const std::string *string = std::any_cast<std::string>(&value)) {
        RNA_property_string_set(&ptr, prop, string->c_str());
      }
      break;
    case PROP_ENUM: {
      const std::string *identifier = std::any_cast<std::string>(&value);
      int enum_value;
      if (identifier &&
          RNA_property_enum_value(nullptr, &ptr, prop, identifier->c_str(), &enum_value))
      {
        RNA_property_enum_set(&ptr, prop, enum_value);
      }
      break;
    }
    case PROP_POINTER: {
      if (const std::string *target_name = std::any_cast<std::string>(&value)) {
        PointerRNA target = find_by_name(
            bmain, editing, ptr, RNA_property_pointer_type(&ptr, prop), *target_name);
        if (target.data && RNA_property_editable_flag(&ptr, prop)) {
          RNA_property_pointer_set(&ptr, prop, target, nullptr);
        }
      }
      else if (const AnyDictionary *dictionary = std::any_cast<AnyDictionary>(&value)) {
        PointerRNA child = RNA_property_pointer_get(&ptr, prop);
        if (child.data) {
          rna_from_dictionary(bmain, editing, *dictionary, child);
          if (RNA_struct_is_a(child.type, RNA_CurveMapping)) {
            BKE_curvemapping_changed_all(static_cast<CurveMapping *>(child.data));
          }
        }
      }
      break;
    }
    case PROP_COLLECTION: {
      const AnyVector *items = std::any_cast<AnyVector>(&value);
      if (!items) {
        break;
      }
      if (RNA_struct_is_a(ptr.type, RNA_Strip) && name == "modifiers") {
        add_modifiers(bmain, editing, ptr, *items);
        break;
      }
      resize_collection(ptr, name, items->size());
      int i = 0;
      RNA_PROP_BEGIN (&ptr, item, prop) {
        if (i < items->size()) {
          if (const AnyDictionary *dictionary = std::any_cast<AnyDictionary>(&(*items)[i])) {
            rna_from_dictionary(bmain, editing, *dictionary, item);
          }
        }
        i++;
      }
      RNA_PROP_END;
      break;
    }
  }
}

void rna_from_dictionary(Main *bmain,
                         Editing *editing,
                         const AnyDictionary &dictionary,
                         PointerRNA &ptr)
{
  for (const auto &[key, value] : dictionary) {
    PropertyRNA *prop = RNA_struct_find_property(&ptr, key.c_str());
    if (!prop || skip_property(ptr, prop)) {
      continue;
    }
    if (ELEM(RNA_property_type(prop), PROP_POINTER, PROP_COLLECTION) ||
        RNA_property_editable_flag(&ptr, prop))
    {
      set_property(bmain, editing, ptr, prop, value);
    }
  }
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

void store_foreign_metadata(const SerializableObjectWithMetadata &object, IDProperty *&properties)
{
  for (const auto &[key, value] : object.metadata()) {
    if (key == "blender") {
      continue;
    }
    if (!properties) {
      properties = bke::idprop::create_group("").release();
    }
    IDProperty *otio_metadata = IDP_GetPropertyFromGroup(properties, "otio_metadata");
    if (!otio_metadata) {
      otio_metadata = bke::idprop::create_group("otio_metadata").release();
      IDP_AddToGroup(properties, otio_metadata);
    }
    IDP_ReplaceInGroup(
        otio_metadata,
        bke::idprop::create(key, serialize_json_to_string(value, nullptr, nullptr, 0)).release());
  }
}

}  // namespace blender::io::otio
