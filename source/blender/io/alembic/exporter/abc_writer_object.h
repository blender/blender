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

#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>

#include "abc_exporter.h"

#include "DNA_ID.h"

struct Main;
struct Object;

namespace blender {
namespace io {
namespace alembic {

class AbcTransformWriter;

class AbcObjectWriter {
 protected:
  Object *m_object;
  ExportSettings &m_settings;

  uint32_t m_time_sampling;

  Imath::Box3d m_bounds;
  std::vector<AbcObjectWriter *> m_children;

  std::vector<std::pair<std::string, IDProperty *>> m_props;

  bool m_first_frame;
  std::string m_name;

 public:
  AbcObjectWriter(Object *ob,
                  uint32_t time_sampling,
                  ExportSettings &settings,
                  AbcObjectWriter *parent = NULL);

  virtual ~AbcObjectWriter();

  void addChild(AbcObjectWriter *child);

  virtual Imath::Box3d bounds();

  void write();

 private:
  virtual void do_write() = 0;
};

}  // namespace alembic
}  // namespace io
}  // namespace blender
