/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#include "DNA_particle_types.h"
#include "RNA_access.h"
#include "RNA_path.h"
#include "RNA_types.h"

#include "draw_handle.hh"
#include "draw_manager.hh"
#include "draw_shader_shared.h"

/* -------------------------------------------------------------------- */
/** \name ObjectAttributes
 * \{ */

/**
 * Extract object attribute from RNA property.
 * Returns true if the attribute was correctly extracted.
 * This function mirrors lookup_property in cycles/blender/blender_object.cpp
 */
bool ObjectAttribute::id_property_lookup(ID *id, const char *name)
{
  PointerRNA ptr, id_ptr;
  PropertyRNA *prop;

  if (id == nullptr) {
    return false;
  }

  RNA_id_pointer_create(id, &id_ptr);

  if (!RNA_path_resolve(&id_ptr, name, &ptr, &prop)) {
    return false;
  }

  if (prop == nullptr) {
    return false;
  }

  PropertyType type = RNA_property_type(prop);
  int array_len = RNA_property_array_length(&ptr, prop);

  if (array_len == 0) {
    float value;

    if (type == PROP_FLOAT) {
      value = RNA_property_float_get(&ptr, prop);
    }
    else if (type == PROP_INT) {
      value = RNA_property_int_get(&ptr, prop);
    }
    else {
      return false;
    }

    *reinterpret_cast<float4 *>(&data_x) = float4(value, value, value, 1.0f);
    return true;
  }

  if (type == PROP_FLOAT && array_len <= 4) {
    *reinterpret_cast<float4 *>(&data_x) = float4(0.0f, 0.0f, 0.0f, 1.0f);
    RNA_property_float_get_array(&ptr, prop, &data_x);
    return true;
  }
  return false;
}

/**
 * Go through all possible source of the given object uniform attribute.
 * Returns true if the attribute was correctly filled.
 * This function mirrors lookup_instance_property in cycles/blender/blender_object.cpp
 */
bool ObjectAttribute::sync(const blender::draw::ObjectRef &ref, const GPUUniformAttr &attr)
{
  hash_code = attr.hash_code;

  /* If requesting instance data, check the parent particle system and object. */
  if (attr.use_dupli) {
    if ((ref.dupli_object != nullptr) && (ref.dupli_object->particle_system != nullptr)) {
      ParticleSettings *settings = ref.dupli_object->particle_system->part;
      if (this->id_property_lookup((ID *)settings, attr.name_id_prop) ||
          this->id_property_lookup((ID *)settings, attr.name)) {
        return true;
      }
    }
    if (this->id_property_lookup((ID *)ref.dupli_parent, attr.name_id_prop) ||
        this->id_property_lookup((ID *)ref.dupli_parent, attr.name)) {
      return true;
    }
  }

  /* Check the object and mesh. */
  if (ref.object != nullptr) {
    if (this->id_property_lookup((ID *)ref.object, attr.name_id_prop) ||
        this->id_property_lookup((ID *)ref.object, attr.name) ||
        this->id_property_lookup((ID *)ref.object->data, attr.name_id_prop) ||
        this->id_property_lookup((ID *)ref.object->data, attr.name)) {
      return true;
    }
  }
  return false;
}

/** \} */
