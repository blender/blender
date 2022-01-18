/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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

/** \brief Allocate a new IDProperty of type IDP_INT, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, int32_t value);

/** \brief Allocate a new IDProperty of type IDP_FLOAT, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, float value);

/** \brief Allocate a new IDProperty of type IDP_DOUBLE, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, double value);

/** \brief Allocate a new IDProperty of type IDP_STRING, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name,
                                                      const StringRefNull value);

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and subtype IDP_INT.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name,
                                                      Span<int32_t> values);

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and subtype IDP_FLOAT.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, Span<float> values);

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and subtype IDP_DOUBLE.
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
