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
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */
#pragma once

#include "IO_abstract_hierarchy_iterator.h"
#include "abc_hierarchy_iterator.h"

#include <Alembic/Abc/OObject.h>
#include <vector>

#include "DEG_depsgraph_query.h"
#include "DNA_material_types.h"

struct Material;
struct Object;

namespace blender {
namespace io {
namespace alembic {

class ABCAbstractWriter : public AbstractHierarchyWriter {
 protected:
  const ABCWriterConstructorArgs args_;

  bool frame_has_been_written_;
  bool is_animated_;
  uint32_t timesample_index_;
  Imath::Box3d bounding_box_;

 public:
  explicit ABCAbstractWriter(const ABCWriterConstructorArgs &args);
  virtual ~ABCAbstractWriter();

  virtual void write(HierarchyContext &context) override;

  /* Returns true if the data to be written is actually supported. This would, for example, allow a
   * hypothetical camera writer accept a perspective camera but reject an orthogonal one.
   *
   * Returning false from a transform writer will prevent the object and all its descendants from
   * being exported. Returning false from a data writer (object data, hair, or particles) will
   * only prevent that data from being written (and thus cause the object to be exported as an
   * Empty). */
  virtual bool is_supported(const HierarchyContext *context) const;

  const Imath::Box3d &bounding_box() const;

  /* Called by AlembicHierarchyCreator after checking that the data is supported via
   * is_supported(). */
  virtual void create_alembic_objects(const HierarchyContext *context) = 0;

  virtual const Alembic::Abc::OObject get_alembic_object() const = 0;

 protected:
  virtual void do_write(HierarchyContext &context) = 0;

  virtual void update_bounding_box(Object *object);
};

}  // namespace alembic
}  // namespace io
}  // namespace blender
