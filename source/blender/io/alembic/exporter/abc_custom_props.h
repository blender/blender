/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup Alembic
 */

#pragma once

#include <Alembic/Abc/OArrayProperty.h>
#include <Alembic/Abc/OCompoundProperty.h>

#include "BLI_map.hh"

#include <memory>

struct IDProperty;

namespace blender::io::alembic {

class ABCAbstractWriter;

/* Write values of Custom Properties (a.k.a. ID Properties) to Alembic.
 *
 * Each Alembic Writer instance optionally has one CustomPropertiesExporter (CPE). This CPE not
 * only writes the custom properties to Alembic, but also keeps references in memory so that the
 * Alembic library doesn't prematurely finalize the data. */
class CustomPropertiesExporter {
 private:
  /* Owner is used to get the OCompoundProperty and time sample index. The former should only be
   * requested from the Alembic library when it's actually going to be used to add custom
   * properties (otherwise an invalid Alembic file is written). */
  ABCAbstractWriter *owner_;

  /* The Compound Property that will contain the exported custom properties.
   *
   * Typically this the return value of abc_schema.getArbGeomParams() or
   * abc_schema.getUserProperties(). */
  Alembic::Abc::OCompoundProperty abc_compound_prop_;

  /* Mapping from property name in Blender to property in Alembic.
   * Here Blender does the same as other software (Maya, Houdini), and writes
   * scalar properties as single-element arrays. */
  Map<std::string, Alembic::Abc::OArrayProperty> abc_properties_;

 public:
  CustomPropertiesExporter(ABCAbstractWriter *owner);
  virtual ~CustomPropertiesExporter() = default;

  void write_all(const IDProperty *group);

 private:
  void write(const IDProperty *id_property);
  void write_array(const IDProperty *id_property);

  /* IDProperty arrays are used to store arrays-of-arrays or arrays-of-strings. */
  void write_idparray(const IDProperty *idp_array);
  void write_idparray_of_strings(const IDProperty *idp_array);
  void write_idparray_of_numbers(const IDProperty *idp_array);

  /* Flatten an array-of-arrays into one long array, then write that.
   * It is tempting to write an array of NxM numbers as a matrix, but there is
   * no guarantee that the data actually represents a matrix. */
  template<typename ABCPropertyType, typename BlenderValueType>
  void write_idparray_flattened_typed(const IDProperty *idp_array);

  /* Write a single scalar (i.e. non-array) property as single-value array. */
  template<typename ABCPropertyType, typename BlenderValueType>
  void set_scalar_property(StringRef property_name, const BlenderValueType property_value);

  template<typename ABCPropertyType, typename BlenderValueType>
  void set_array_property(StringRef property_name,
                          const BlenderValueType *array_values,
                          size_t num_array_items);

  template<typename ABCPropertyType>
  Alembic::Abc::OArrayProperty create_abc_property(StringRef property_name);
};

}  // namespace blender::io::alembic
