/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <type_traits>

#include "DNA_ID.h"

#include "BKE_idprop.hh"

namespace blender::bke::idprop {

/* -------------------------------------------------------------------- */
/** \name Create Functions
 * \{ */

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRef prop_name,
                                                      int32_t value,
                                                      const eIDPropertyFlag flags)
{
  IDPropertyTemplate prop_template{0};
  prop_template.i = value;
  IDProperty *property = IDP_New(IDP_INT, &prop_template, prop_name, flags);
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create_bool(const StringRef prop_name,
                                                           bool value,
                                                           const eIDPropertyFlag flags)
{
  IDPropertyTemplate prop_template{0};
  prop_template.i = value;
  IDProperty *property = IDP_New(IDP_BOOLEAN, &prop_template, prop_name, flags);
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRef prop_name,
                                                      float value,
                                                      const eIDPropertyFlag flags)
{
  IDPropertyTemplate prop_template{0};
  prop_template.f = value;
  IDProperty *property = IDP_New(IDP_FLOAT, &prop_template, prop_name, flags);
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRef prop_name,
                                                      double value,
                                                      const eIDPropertyFlag flags)
{
  IDPropertyTemplate prop_template{0};
  prop_template.d = value;
  IDProperty *property = IDP_New(IDP_DOUBLE, &prop_template, prop_name, flags);
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRef prop_name,
                                                      const StringRefNull value,
                                                      const eIDPropertyFlag flags)
{
  IDProperty *property = IDP_NewString(value.c_str(), prop_name, flags);
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRef prop_name,
                                                      ID *value,
                                                      const eIDPropertyFlag flags)
{
  /* Do not assign embedded IDs to IDProperties. */
  BLI_assert(!value || (value->flag & ID_FLAG_EMBEDDED_DATA) == 0);

  IDPropertyTemplate prop_template{0};
  prop_template.id = value;
  IDProperty *property = IDP_New(IDP_ID, &prop_template, prop_name, flags);
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

static std::unique_ptr<IDProperty, IDPropertyDeleter> array_create(const StringRef prop_name,
                                                                   eIDPropertyType subtype,
                                                                   size_t array_len,
                                                                   const eIDPropertyFlag flags)
{
  IDPropertyTemplate prop_template{0};
  prop_template.array.len = array_len;
  prop_template.array.type = subtype;
  IDProperty *property = IDP_New(IDP_ARRAY, &prop_template, prop_name, flags);
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

static void array_values_set(IDProperty *property,
                             const void *values,
                             size_t values_len,
                             size_t value_size)
{
  BLI_assert(values);
  BLI_assert(property->len == values_len);
  memcpy(IDP_array_voidp_get(property), values, values_len * value_size);
}

/**
 * Create a IDProperty array of `id_property_subtype` and fill it with the given values.
 */
template<
    /** C-Primitive type of the array. Can be int32_t, float, double. */
    typename PrimitiveType,
    /** Sub-type of the #ID_ARRAY. Must match #PrimitiveType. */
    eIDPropertyType id_property_subtype>
std::unique_ptr<IDProperty, IDPropertyDeleter> create_array(StringRef prop_name,
                                                            Span<PrimitiveType> values,
                                                            const eIDPropertyFlag flags)
{
  static_assert(is_same_any_v<PrimitiveType, int32_t, float, double>,
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
      prop_name, id_property_subtype, values_len, flags);
  array_values_set(
      property.get(), static_cast<const void *>(values.data()), values_len, sizeof(PrimitiveType));
  return property;
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRef prop_name,
                                                      Span<int32_t> values,
                                                      const eIDPropertyFlag flags)
{
  return create_array<int32_t, IDP_INT>(prop_name, values, flags);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRef prop_name,
                                                      Span<float> values,
                                                      const eIDPropertyFlag flags)
{
  return create_array<float, IDP_FLOAT>(prop_name, values, flags);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create(const StringRef prop_name,
                                                      Span<double> values,
                                                      const eIDPropertyFlag flags)
{
  return create_array<double, IDP_DOUBLE>(prop_name, values, flags);
}

std::unique_ptr<IDProperty, IDPropertyDeleter> create_group(const StringRef prop_name,
                                                            const eIDPropertyFlag flags)
{
  IDPropertyTemplate prop_template{0};
  IDProperty *property = IDP_New(IDP_GROUP, &prop_template, prop_name, flags);
  return std::unique_ptr<IDProperty, IDPropertyDeleter>(property);
}

/** \} */

}  // namespace blender::bke::idprop
