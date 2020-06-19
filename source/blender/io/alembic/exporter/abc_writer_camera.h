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

/** \file
 * \ingroup balembic
 */

#include "abc_writer_abstract.h"

#include <Alembic/AbcGeom/OCamera.h>

namespace blender {
namespace io {
namespace alembic {

class ABCCameraWriter : public ABCAbstractWriter {
 private:
  Alembic::AbcGeom::OCamera abc_camera_;
  Alembic::AbcGeom::OCameraSchema abc_camera_schema_;

  Alembic::AbcGeom::OCompoundProperty abc_custom_data_container_;
  Alembic::AbcGeom::OFloatProperty abc_stereo_distance_;
  Alembic::AbcGeom::OFloatProperty abc_eye_separation_;

 public:
  explicit ABCCameraWriter(const ABCWriterConstructorArgs &args);

  virtual void create_alembic_objects(const HierarchyContext *context) override;
  virtual const Alembic::Abc::OObject get_alembic_object() const override;

 protected:
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual void do_write(HierarchyContext &context) override;
};

}  // namespace alembic
}  // namespace io
}  // namespace blender
