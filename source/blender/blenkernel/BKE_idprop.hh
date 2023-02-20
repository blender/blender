/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_idprop.h"

#include "BLI_serialize.hh"
#include "BLI_span.hh"

namespace blender::bke::idprop {

/**
 * \brief Convert the given `properties` to `Value` objects for serialization.
 *
 * `IDP_ID` and `IDP_IDPARRAY` are not supported and will be ignored.
 *
 * UI data such as max/min will not be serialized.
 */
std::unique_ptr<io::serialize::ArrayValue> convert_to_serialize_values(
    const IDProperty *properties);

/**
 * \brief Convert the given `value` to an `IDProperty`.
 */
IDProperty *convert_from_serialize_value(const blender::io::serialize::Value &value);

class IDPropertyDeleter {
 public:
  void operator()(IDProperty *id_prop)
  {
    IDP_FreeProperty(id_prop);
  }
};

/** \brief Allocate a new IDProperty of type IDP_BOOLEAN, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create_bool(StringRefNull prop_name, bool value);

/** \brief Allocate a new IDProperty of type IDP_INT, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, int32_t value);

/** \brief Allocate a new IDProperty of type IDP_FLOAT, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, float value);

/** \brief Allocate a new IDProperty of type IDP_DOUBLE, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, double value);

/** \brief Allocate a new IDProperty of type IDP_STRING, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name,
                                                      const StringRefNull value);

/** \brief Allocate a new IDProperty of type IDP_ID, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, ID *value);

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and sub-type IDP_INT.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name,
                                                      Span<int32_t> values);

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and sub-type IDP_FLOAT.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, Span<float> values);

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and sub-type IDP_DOUBLE.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name,
                                                      Span<double> values);

/**
 * \brief Allocate a new IDProperty of type IDP_GROUP.
 *
 * \param prop_name: The name of the newly created property.
 */

std::unique_ptr<IDProperty, IDPropertyDeleter> create_group(StringRefNull prop_name);

}  // namespace blender::bke::idprop
