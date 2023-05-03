/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. */

#include <type_traits>

#include "DNA_ID.h"

#include "BKE_idprop.hh"

namespace blender::bke::idprop {

/* -------------------------------------------------------------------- */
/** \name Create Functions
 * \{ */

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRefNull prop_name, int32_t value)
{
  IDPropertyTemplate prop_template{0};
  prop_template.i = value;
  IDProperty *property = IDP_New(IDP_INT, &prop_template, prop_name.c_str());
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create_bool(const StringRefNull prop_name,
                                                           bool value)
{
  IDPropertyTemplate prop_template{0};
  prop_template.i = value;
  IDProperty *property = IDP_New(IDP_BOOLEAN, &prop_template, prop_name.c_str());
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRefNull prop_name, float value)
{
  IDPropertyTemplate prop_template{0};
  prop_template.f = value;
  IDProperty *property = IDP_New(IDP_FLOAT, &prop_template, prop_name.c_str());
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRefNull prop_name, double value)
{
  IDPropertyTemplate prop_template{0};
  prop_template.d = value;
  IDProperty *property = IDP_New(IDP_DOUBLE, &prop_template, prop_name.c_str());
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRefNull prop_name,
                                                      const StringRefNull value)
{
  IDProperty *property = IDP_NewString(value.c_str(), prop_name.c_str());
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRefNull prop_name, ID *value)
{
  IDPropertyTemplate prop_template{0};
  prop_template.id = value;
  IDProperty *property = IDP_New(IDP_ID, &prop_template, prop_name.c_str());
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

static std::unique_ptr<IDProperty, IDPropertyDeleter> array_create(const StringRefNull prop_name,
                                                                   eIDPropertyType subtype,
                                                                   size_t array_len)
{
  IDPropertyTemplate prop_template{0};
  prop_template.array.len = array_len;
  prop_template.array.type = subtype;
  IDProperty *property = IDP_New(IDP_ARRAY, &prop_template, prop_name.c_str());
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

static void array_values_set(IDProperty *property,
                             const void *values,
                             size_t values_len,
                             size_t value_size)
{
  BLI_assert(values);
  BLI_assert(property->len == values_len);
  memcpy(IDP_Array(property), values, values_len * value_size);
}

/**
 * Create a IDProperty array of `id_property_subtype` and fill it with the given values.
 */
template<
    /** C-Primitive type of the array. Can be int32_t, float, double. */
    typename PrimitiveType,
    /** Sub-type of the #ID_ARRAY. Must match #PrimitiveType. */
    eIDPropertyType id_property_subtype>
std::unique_ptr<IDProperty, IDPropertyDeleter> create_array(StringRefNull prop_name,
                                                            Span<PrimitiveType> values)
{
  static_assert(std::is_same_v<PrimitiveType, int32_t> || std::is_same_v<PrimitiveType, float> ||
                    std::is_same_v<PrimitiveType, double>,
                "Allowed values for PrimitiveType are int32_t, float and double.");
  static_assert(!std::is_same_v<PrimitiveType, int32_t> || id_property_subtype == IDP_INT,
                "PrimitiveType and id_property_type do not match (int32_t).");
  static_assert(!std::is_same_v<PrimitiveType, float> || id_property_subtype == IDP_FLOAT,
                "PrimitiveType and id_property_type do not match (float).");
  static_assert(!std::is_same_v<PrimitiveType, double> || id_property_subtype == IDP_DOUBLE,
                "PrimitiveType and id_property_type do not match (double).");

  const int64_t values_len = values.size();
  BLI_assert(values_len > 0);
  std::unique_ptr<IDProperty, IDPropertyDeleter> property = array_create(
      prop_name.c_str(), id_property_subtype, values_len);
  array_values_set(
      property.get(), static_cast<const void *>(values.data()), values_len, sizeof(PrimitiveType));
  return property;
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRefNull prop_name,
                                                      Span<int32_t> values)
{
  return create_array<int32_t, IDP_INT>(prop_name, values);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRefNull prop_name,
                                                      Span<float> values)
{
  return create_array<float, IDP_FLOAT>(prop_name, values);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRefNull prop_name,
                                                      Span<double> values)
{
  return create_array<double, IDP_DOUBLE>(prop_name, values);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create_group(const StringRefNull prop_name)
{
  IDPropertyTemplate prop_template{0};
  IDProperty *property = IDP_New(IDP_GROUP, &prop_template, prop_name.c_str());
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

/* \} */

}  // namespace blender::bke::idprop
