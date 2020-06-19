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

/** \file
 * \ingroup balembic
 */

#ifndef __ABC_READER_CAMERA_H__
#define __ABC_READER_CAMERA_H__

#include "abc_reader_object.h"

namespace blender {
namespace io {
namespace alembic {

class AbcCameraReader : public AbcObjectReader {
  Alembic::AbcGeom::ICameraSchema m_schema;

 public:
  AbcCameraReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

  bool valid() const;
  bool accepts_object_type(const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
                           const Object *const ob,
                           const char **err_str) const;

  void readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel);
};

}  // namespace alembic
}  // namespace io
}  // namespace blender

#endif /* __ABC_READER_CAMERA_H__ */
