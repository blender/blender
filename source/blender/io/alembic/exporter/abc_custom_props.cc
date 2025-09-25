/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup Alembic
 */

#include "abc_custom_props.h"

#include "abc_writer_abstract.h"

#include <string>

#include <Alembic/Abc/OTypedArrayProperty.h>

#include "BLI_listbase.h"

#include "BKE_idprop.hh"
#include "DNA_ID.h"

using Alembic::Abc::ArraySample;
using Alembic::Abc::OArrayProperty;
using Alembic::Abc::OBoolArrayProperty;
using Alembic::Abc::OCompoundProperty;
using Alembic::Abc::ODoubleArrayProperty;
using Alembic::Abc::OFloatArrayProperty;
using Alembic::Abc::OInt32ArrayProperty;
using Alembic::Abc::OStringArrayProperty;

namespace blender::io::alembic {

CustomPropertiesExporter::CustomPropertiesExporter(ABCAbstractWriter *owner) : owner_(owner) {}

void CustomPropertiesExporter::write_all(const IDProperty *group)
{
  if (group == nullptr) {
    return;
  }
  BLI_assert(group->type == IDP_GROUP);

  /* Loop over the properties, just like IDP_foreach_property() does, but without the recursion. */
  LISTBASE_FOREACH (IDProperty *, id_property, &group->data.group) {
    write(id_property);
  }
}

void CustomPropertiesExporter::write(const IDProperty *id_property)
{
  BLI_assert(id_property->name[0] != '\0');

  switch (id_property->type) {
    case IDP_STRING: {
      /* The Alembic library doesn't accept null-terminated character arrays. */
      const std::string prop_value(IDP_string_get(id_property), id_property->len - 1);
      set_scalar_property<OStringArrayProperty, std::string>(id_property->name, prop_value);
      break;
    }
    case IDP_INT:
      static_assert(sizeof(int) == sizeof(int32_t), "Expecting 'int' to be 32-bit");
      set_scalar_property<OInt32ArrayProperty, int32_t>(id_property->name,
                                                        IDP_int_get(id_property));
      break;
    case IDP_FLOAT:
      set_scalar_property<OFloatArrayProperty, float>(id_property->name,
                                                      IDP_float_get(id_property));
      break;
    case IDP_DOUBLE:
      set_scalar_property<ODoubleArrayProperty, double>(id_property->name,
                                                        IDP_double_get(id_property));
      break;
    case IDP_BOOLEAN:
      set_scalar_property<OBoolArrayProperty, bool>(id_property->name, IDP_bool_get(id_property));
      break;
    case IDP_ARRAY:
      write_array(id_property);
      break;
    case IDP_IDPARRAY:
      write_idparray(id_property);
      break;
  }
}

void CustomPropertiesExporter::write_array(const IDProperty *id_property)
{
  BLI_assert(id_property->type == IDP_ARRAY);

  switch (id_property->subtype) {
    case IDP_INT: {
      const int *array = IDP_array_int_get(id_property);
      static_assert(sizeof(int) == sizeof(int32_t), "Expecting 'int' to be 32-bit");
      set_array_property<OInt32ArrayProperty, int32_t>(id_property->name, array, id_property->len);
      break;
    }
    case IDP_FLOAT: {
      const float *array = IDP_array_float_get(id_property);
      set_array_property<OFloatArrayProperty, float>(id_property->name, array, id_property->len);
      break;
    }
    case IDP_DOUBLE: {
      const double *array = IDP_array_double_get(id_property);
      set_array_property<ODoubleArrayProperty, double>(id_property->name, array, id_property->len);
      break;
    }
    case IDP_BOOLEAN: {
      const int8_t *array = IDP_array_bool_get(id_property);
      set_array_property<OBoolArrayProperty, int8_t>(id_property->name, array, id_property->len);
      break;
    }
  }
}

void CustomPropertiesExporter::write_idparray(const IDProperty *idp_array)
{
  BLI_assert(idp_array->type == IDP_IDPARRAY);

  if (idp_array->len == 0) {
    /* Don't bother writing dataless arrays. */
    return;
  }

  IDProperty *idp_elements = IDP_property_array_get(idp_array);

#ifndef NDEBUG
  /* Sanity check that all elements of the array have the same type.
   * Blender should already enforce this, hence it's only used in debug mode. */
  for (int i = 1; i < idp_array->len; i++) {
    if (idp_elements[i].type == idp_elements[0].type) {
      continue;
    }
    std::cerr << "Custom property " << idp_array->name << " has elements of varying type";
    BLI_assert_msg(0, "Mixed type IDP_ARRAY custom property found");
  }
#endif

  switch (idp_elements[0].type) {
    case IDP_STRING:
      write_idparray_of_strings(idp_array);
      break;
    case IDP_ARRAY:
      write_idparray_of_numbers(idp_array);
      break;
  }
}

void CustomPropertiesExporter::write_idparray_of_strings(const IDProperty *idp_array)
{
  BLI_assert(idp_array->type == IDP_IDPARRAY);
  BLI_assert(idp_array->len > 0);

  /* Convert to an array of std::strings, because Alembic doesn't like zero-delimited strings. */
  IDProperty *idp_elements = IDP_property_array_get(idp_array);
  std::vector<std::string> strings(idp_array->len);
  for (int i = 0; i < idp_array->len; i++) {
    BLI_assert(idp_elements[i].type == IDP_STRING);
    strings[i] = IDP_string_get(&idp_elements[i]);
  }

  /* Alembic needs a pointer to the first value of the array. */
  const std::string *array_of_strings = strings.data();
  set_array_property<OStringArrayProperty, std::string>(
      idp_array->name, array_of_strings, strings.size());
}

void CustomPropertiesExporter::write_idparray_of_numbers(const IDProperty *idp_array)
{
  BLI_assert(idp_array->type == IDP_IDPARRAY);
  BLI_assert(idp_array->len > 0);

  /* This must be an array of arrays. */
  IDProperty *idp_rows = IDP_property_array_get(idp_array);
  BLI_assert(idp_rows[0].type == IDP_ARRAY);

  const int subtype = idp_rows[0].subtype;
  if (!ELEM(subtype, IDP_INT, IDP_FLOAT, IDP_DOUBLE, IDP_BOOLEAN)) {
    /* Non-numerical types are not supported. */
    return;
  }

  switch (subtype) {
    case IDP_INT:
      static_assert(sizeof(int) == sizeof(int32_t), "Expecting 'int' to be 32-bit");
      write_idparray_flattened_typed<OInt32ArrayProperty, int32_t>(idp_array);
      break;
    case IDP_FLOAT:
      write_idparray_flattened_typed<OFloatArrayProperty, float>(idp_array);
      break;
    case IDP_DOUBLE:
      write_idparray_flattened_typed<ODoubleArrayProperty, double>(idp_array);
      break;
    case IDP_BOOLEAN:
      write_idparray_flattened_typed<OBoolArrayProperty, int8_t>(idp_array);
      break;
  }
}

template<typename ABCPropertyType, typename BlenderValueType>
void CustomPropertiesExporter::write_idparray_flattened_typed(const IDProperty *idp_array)
{
  BLI_assert(idp_array->type == IDP_IDPARRAY);
  BLI_assert(idp_array->len > 0);

  const IDProperty *idp_rows = IDP_property_array_get(idp_array);
  BLI_assert(idp_rows[0].type == IDP_ARRAY);
  BLI_assert(ELEM(idp_rows[0].subtype, IDP_INT, IDP_FLOAT, IDP_DOUBLE, IDP_BOOLEAN));

  const uint64_t num_rows = idp_array->len;
  std::vector<BlenderValueType> matrix_values;
  for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
    const BlenderValueType *row = (BlenderValueType *)IDP_array_voidp_get(&idp_rows[row_idx]);
    for (size_t col_idx = 0; col_idx < idp_rows[row_idx].len; col_idx++) {
      matrix_values.push_back(row[col_idx]);
    }
  }

  set_array_property<ABCPropertyType, BlenderValueType>(
      idp_array->name, matrix_values.data(), matrix_values.size());
}

template<typename ABCPropertyType, typename BlenderValueType>
void CustomPropertiesExporter::set_scalar_property(const StringRef property_name,
                                                   const BlenderValueType property_value)
{
  set_array_property<ABCPropertyType, BlenderValueType>(property_name, &property_value, 1);
}

template<typename ABCPropertyType, typename BlenderValueType>
void CustomPropertiesExporter::set_array_property(const StringRef property_name,
                                                  const BlenderValueType *array_values,
                                                  const size_t num_array_items)
{
  auto create_callback = [this, property_name]() -> OArrayProperty {
    return create_abc_property<ABCPropertyType>(property_name);
  };

  OArrayProperty array_prop = abc_properties_.lookup_or_add_cb(property_name, create_callback);
  Alembic::Util::Dimensions array_dimensions(num_array_items);
  ArraySample sample(array_values, array_prop.getDataType(), array_dimensions);
  array_prop.set(sample);
}

template<typename ABCPropertyType>
OArrayProperty CustomPropertiesExporter::create_abc_property(const StringRef property_name)
{
  /* Get the necessary info from our owner. */
  OCompoundProperty abc_prop_for_custom_props = owner_->abc_prop_for_custom_props();
  const uint32_t timesample_index = owner_->timesample_index();

  /* Construct the Alembic property. */
  ABCPropertyType abc_property(abc_prop_for_custom_props, property_name);
  abc_property.setTimeSampling(timesample_index);
  return abc_property;
}

}  // namespace blender::io::alembic
