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
 * The Original Code is Copyright (C) 2016 Kévin Dietrich.
 * All rights reserved.
 */

/** \file
 * \ingroup balembic
 */

#ifndef __ABC_WRITER_CURVES_H__
#define __ABC_WRITER_CURVES_H__

#include "abc_writer_mesh.h"
#include "abc_writer_object.h"

namespace blender {
namespace io {
namespace alembic {

class AbcCurveWriter : public AbcObjectWriter {
  Alembic::AbcGeom::OCurvesSchema m_schema;
  Alembic::AbcGeom::OCurvesSchema::Sample m_sample;

 public:
  AbcCurveWriter(Object *ob,
                 AbcTransformWriter *parent,
                 uint32_t time_sampling,
                 ExportSettings &settings);

 protected:
  void do_write();
};

class AbcCurveMeshWriter : public AbcGenericMeshWriter {
 public:
  AbcCurveMeshWriter(Object *ob,
                     AbcTransformWriter *parent,
                     uint32_t time_sampling,
                     ExportSettings &settings);

 protected:
  Mesh *getEvaluatedMesh(Scene *scene_eval, Object *ob_eval, bool &r_needsfree);
};

}  // namespace alembic
}  // namespace io
}  // namespace blender

#endif /* __ABC_WRITER_CURVES_H__ */
