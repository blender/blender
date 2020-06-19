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

#ifndef __ABC_WRITER_NURBS_H__
#define __ABC_WRITER_NURBS_H__

#include "abc_writer_object.h"

namespace blender {
namespace io {
namespace alembic {

class AbcNurbsWriter : public AbcObjectWriter {
  std::vector<Alembic::AbcGeom::ONuPatchSchema> m_nurbs_schema;
  bool m_is_animated;

 public:
  AbcNurbsWriter(Object *ob,
                 AbcTransformWriter *parent,
                 uint32_t time_sampling,
                 ExportSettings &settings);

 private:
  virtual void do_write();

  bool isAnimated() const;
};

}  // namespace alembic
}  // namespace io
}  // namespace blender

#endif /* __ABC_WRITER_NURBS_H__ */
