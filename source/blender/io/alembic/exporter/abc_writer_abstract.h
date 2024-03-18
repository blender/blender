/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "IO_abstract_hierarchy_iterator.h"
#include "abc_custom_props.h"
#include "abc_hierarchy_iterator.h"

#include <Alembic/Abc/OObject.h>
#include <vector>

#include "DEG_depsgraph_query.hh"
#include "DNA_material_types.h"

struct IDProperty;
struct Object;

namespace blender::io::alembic {

class ABCAbstractWriter : public AbstractHierarchyWriter {
 protected:
  const ABCWriterConstructorArgs args_;

  bool frame_has_been_written_;
  bool is_animated_;
  uint32_t timesample_index_;
  Imath::Box3d bounding_box_;

  /* Visibility of this writer's data in Alembic. */
  Alembic::Abc::OCharProperty abc_visibility_;

  /* Optional writer for custom properties. */
  std::unique_ptr<CustomPropertiesExporter> custom_props_;

 public:
  explicit ABCAbstractWriter(const ABCWriterConstructorArgs &args);

  virtual void write(HierarchyContext &context) override;

  /* Returns true if the data to be written is actually supported. This would, for example, allow a
   * hypothetical camera writer accept a perspective camera but reject an orthogonal one.
   *
   * Returning false from a transform writer will prevent the object and all its descendants from
   * being exported. Returning false from a data writer (object data, hair, or particles) will
   * only prevent that data from being written (and thus cause the object to be exported as an
   * Empty). */
  virtual bool is_supported(const HierarchyContext *context) const;

  uint32_t timesample_index() const;
  const Imath::Box3d &bounding_box() const;

  /* Called by AlembicHierarchyCreator after checking that the data is supported via
   * is_supported(). */
  virtual void create_alembic_objects(const HierarchyContext *context) = 0;

  virtual Alembic::Abc::OObject get_alembic_object() const = 0;

  /* Return the Alembic object's CompoundProperty that'll contain the custom properties.
   *
   * This function is called whenever there are custom properties to be written to Alembic. It
   * should call abc_schema_prop_for_custom_props() with the writer's Alembic schema object.
   *
   * If custom properties are not supported by a specific subclass, it should return an empty
   * OCompoundProperty() and override ensure_custom_properties_exporter() to do nothing.
   */
  virtual Alembic::Abc::OCompoundProperty abc_prop_for_custom_props() = 0;

 protected:
  virtual void do_write(HierarchyContext &context) = 0;

  virtual void update_bounding_box(Object *object);

  /* Return ID properties of whatever ID datablock is written by this writer. Defaults to the
   * properties of the object data. Can return nullptr if no custom properties are to be written.
   */
  virtual const IDProperty *get_id_properties(const HierarchyContext &context) const;

  virtual void ensure_custom_properties_exporter(const HierarchyContext &context);

  void write_visibility(const HierarchyContext &context);

  /* Return the Alembic schema's compound property, which will be used for writing custom
   * properties.
   *
   * This can return either abc_schema.getUserProperties() or abc_schema.getArbGeomParams(). The
   * former only holds values similar to Blender's custom properties, whereas the latter can also
   * specify that certain custom properties vary per mesh component (so per face, vertex, etc.). As
   * such, .userProperties is more suitable for custom properties. However, Maya, Houdini use
   * .arbGeomParams for custom data.
   *
   * Because of this, the code uses this templated function so that there is one place that
   * determines where custom properties are exporter to.
   */
  template<typename T>
  Alembic::Abc::OCompoundProperty abc_schema_prop_for_custom_props(T abc_schema)
  {
    return abc_schema.getUserProperties();
  }
};

}  // namespace blender::io::alembic
