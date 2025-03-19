/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include <fmt/format.h>

#include "ANIM_rna.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "DNA_object_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

namespace blender::animrig {

Vector<float> get_rna_values(PointerRNA *ptr, PropertyRNA *prop)
{
  Vector<float> values;
  if (RNA_property_array_check(prop)) {
    const int length = RNA_property_array_length(ptr, prop);

    switch (RNA_property_type(prop)) {
      case PROP_BOOLEAN: {
        bool *tmp_bool = static_cast<bool *>(MEM_malloc_arrayN(length, sizeof(bool), __func__));
        RNA_property_boolean_get_array(ptr, prop, tmp_bool);
        for (int i = 0; i < length; i++) {
          values.append(float(tmp_bool[i]));
        }
        MEM_freeN(tmp_bool);
        break;
      }
      case PROP_INT: {
        int *tmp_int = static_cast<int *>(MEM_malloc_arrayN(length, sizeof(int), __func__));
        RNA_property_int_get_array(ptr, prop, tmp_int);
        for (int i = 0; i < length; i++) {
          values.append(float(tmp_int[i]));
        }
        MEM_freeN(tmp_int);
        break;
      }
      case PROP_FLOAT: {
        values.reinitialize(length);
        RNA_property_float_get_array(ptr, prop, values.data());
        break;
      }
      default:
        values.reinitialize(length);
        break;
    }
  }
  else {
    switch (RNA_property_type(prop)) {
      case PROP_BOOLEAN:
        values.append(float(RNA_property_boolean_get(ptr, prop)));
        break;
      case PROP_INT:
        values.append(float(RNA_property_int_get(ptr, prop)));
        break;
      case PROP_FLOAT:
        values.append(RNA_property_float_get(ptr, prop));
        break;
      case PROP_ENUM:
        values.append(float(RNA_property_enum_get(ptr, prop)));
        break;
      default:
        values.append(0.0f);
    }
  }

  return values;
}

StringRef get_rotation_mode_path(const eRotationModes rotation_mode)
{
  switch (rotation_mode) {
    case ROT_MODE_QUAT:
      return "rotation_quaternion";
    case ROT_MODE_AXISANGLE:
      return "rotation_axis_angle";
    default:
      return "rotation_euler";
  }
}

static bool is_idproperty_keyable(const IDProperty *id_prop, PointerRNA *ptr, PropertyRNA *prop)
{
  /* While you can cast the IDProperty* to a PropertyRNA* and pass it to the RNA_* functions, this
   * does not work because it will not have the right flags set. Instead the resolved
   * PointerRNA and PropertyRNA need to be passed. */
  if (!RNA_property_anim_editable(ptr, prop)) {
    return false;
  }

  if (ELEM(id_prop->type,
           eIDPropertyType::IDP_BOOLEAN,
           eIDPropertyType::IDP_INT,
           eIDPropertyType::IDP_FLOAT,
           eIDPropertyType::IDP_DOUBLE))
  {
    return true;
  }

  if (id_prop->type == eIDPropertyType::IDP_ARRAY) {
    if (ELEM(id_prop->subtype,
             eIDPropertyType::IDP_BOOLEAN,
             eIDPropertyType::IDP_INT,
             eIDPropertyType::IDP_FLOAT,
             eIDPropertyType::IDP_DOUBLE))
    {
      return true;
    }
  }

  return false;
}

Vector<RNAPath> get_keyable_id_property_paths(const PointerRNA &ptr)
{
  IDProperty *properties;

  if (ptr.type == &RNA_PoseBone) {
    const bPoseChannel *pchan = static_cast<bPoseChannel *>(ptr.data);
    properties = pchan->prop;
  }
  else if (ptr.type == &RNA_Object) {
    const Object *ob = static_cast<Object *>(ptr.data);
    properties = ob->id.properties;
  }
  else {
    /* Pointer type not supported. */
    return {};
  }

  if (!properties) {
    return {};
  }

  blender::Vector<RNAPath> paths;
  LISTBASE_FOREACH (const IDProperty *, id_prop, &properties->data.group) {
    PointerRNA resolved_ptr;
    PropertyRNA *resolved_prop;
    std::string path = id_prop->name;
    /* Resolving the path twice, once as RNA property (without brackets, `"propname"`),
     * and once as ID property (with brackets, `["propname"]`).
     * This is required to support IDProperties that have been defined as part of an add-on.
     * Those need to be animated through an RNA path without the brackets. */
    bool is_resolved = RNA_path_resolve_property(
        &ptr, path.c_str(), &resolved_ptr, &resolved_prop);
    /* ID properties can be named the same as internal properties, for example `scale`. In that
     * case they would resolve, but it wouldn't be the correct property. `RNA_property_is_runtime`
     * catches that case. */
    if (!is_resolved || !RNA_property_is_runtime(resolved_prop)) {
      char name_escaped[MAX_IDPROP_NAME * 2];
      BLI_str_escape(name_escaped, id_prop->name, sizeof(name_escaped));
      path = fmt::format("[\"{}\"]", name_escaped);
      is_resolved = RNA_path_resolve_property(&ptr, path.c_str(), &resolved_ptr, &resolved_prop);
    }
    if (!is_resolved) {
      continue;
    }
    if (is_idproperty_keyable(id_prop, &resolved_ptr, resolved_prop)) {
      paths.append({path});
    }
  }
  return paths;
}

}  // namespace blender::animrig
